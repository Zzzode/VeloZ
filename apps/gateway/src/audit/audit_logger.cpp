#include "audit_logger.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstring>
#include <dirent.h>
#include <fcntl.h>
#include <format>
#include <kj/debug.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <thread>
#include <time.h>
#include <unistd.h>

namespace veloz::gateway::audit {

// ============================================================================
// Audit Log Type Utilities
// ============================================================================

kj::StringPtr auditLogTypeToString(AuditLogType type) noexcept {
  switch (type) {
  case AuditLogType::Auth:
    return "auth"_kj;
  case AuditLogType::Order:
    return "order"_kj;
  case AuditLogType::ApiKey:
    return "apikey"_kj;
  case AuditLogType::Error:
    return "error"_kj;
  case AuditLogType::Access:
    return "access"_kj;
  }
  return "unknown"_kj;
}

kj::Maybe<AuditLogType> stringToAuditLogType(kj::StringPtr str) noexcept {
  if (str == "auth")
    return AuditLogType::Auth;
  if (str == "order")
    return AuditLogType::Order;
  if (str == "apikey")
    return AuditLogType::ApiKey;
  if (str == "error")
    return AuditLogType::Error;
  if (str == "access")
    return AuditLogType::Access;
  return kj::none;
}

// ============================================================================
// AuditLogEntry Implementation
// ============================================================================

namespace {

// Format time point to ISO8601 string
kj::String format_iso8601(const std::chrono::system_clock::time_point& tp) {
  auto time_t_val = std::chrono::system_clock::to_time_t(tp);
  auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(tp.time_since_epoch()) % 1000;

  std::tm tm_val;
  gmtime_r(&time_t_val, &tm_val);

  // std::format returns std::string which doesn't work with kj::str directly
  // Use .c_str() to convert to const char* which kj::str can accept
  auto formatted = std::format("{:04d}-{:02d}-{:02d}T{:02d}:{:02d}:{:02d}.{:03d}Z",
                               tm_val.tm_year + 1900, tm_val.tm_mon + 1, tm_val.tm_mday,
                               tm_val.tm_hour, tm_val.tm_min, tm_val.tm_sec, ms.count());
  return kj::str(formatted.c_str());
}

// Escape string for JSON
kj::String escape_json(kj::StringPtr str) {
  kj::Vector<char> result;
  for (char c : str) {
    switch (c) {
    case '"':
      result.add('\\');
      result.add('"');
      break;
    case '\\':
      result.add('\\');
      result.add('\\');
      break;
    case '\n':
      result.add('\\');
      result.add('n');
      break;
    case '\r':
      result.add('\\');
      result.add('r');
      break;
    case '\t':
      result.add('\\');
      result.add('t');
      break;
    default:
      if (static_cast<unsigned char>(c) < 0x20) {
        // Control character, escape as \uXXXX
        auto escaped = std::format("\\u{:04x}", static_cast<unsigned char>(c));
        for (char ec : escaped) {
          result.add(ec);
        }
      } else {
        result.add(c);
      }
    }
  }
  return kj::str(result);
}

} // namespace

kj::String AuditLogEntry::to_ndjson() const {
  kj::Vector<kj::String> parts;

  // Timestamp
  parts.add(kj::str("\"timestamp\":\"", format_iso8601(timestamp), "\""));

  // Type
  parts.add(kj::str("\"type\":\"", auditLogTypeToString(type), "\""));

  // Action
  parts.add(kj::str("\"action\":\"", escape_json(action), "\""));

  // User ID
  parts.add(kj::str("\"user_id\":\"", escape_json(user_id), "\""));

  // IP Address
  parts.add(kj::str("\"ip_address\":\"", escape_json(ip_address), "\""));

  // Request ID (optional)
  KJ_IF_SOME(rid, request_id) {
    parts.add(kj::str("\"request_id\":\"", escape_json(rid), "\""));
  }

  // Details (optional map) - kj::HashMap doesn't have empty(), use size() == 0
  if (details.size() > 0) {
    kj::Vector<kj::String> detail_parts;
    for (auto& [key, value] : details) {
      detail_parts.add(kj::str("\"", escape_json(key), "\":\"", escape_json(value), "\""));
    }
    parts.add(kj::str("\"details\":{", kj::strArray(detail_parts, ","), "}"));
  }

  return kj::str("{", kj::strArray(parts, ","), "}\n");
}

AuditLogEntry AuditLogEntry::clone() const {
  AuditLogEntry result;
  result.timestamp = timestamp;
  result.type = type;
  result.action = kj::str(action);
  result.user_id = kj::str(user_id);
  result.ip_address = kj::str(ip_address);
  KJ_IF_SOME(rid, request_id) {
    result.request_id = kj::str(rid);
  }
  for (auto& [key, value] : details) {
    result.details.insert(kj::str(key), kj::str(value));
  }
  return result;
}

// ============================================================================
// AuditLoggerConfig Implementation
// ============================================================================

AuditLoggerConfig AuditLoggerConfig::withDefaults(kj::StringPtr logDir) {
  AuditLoggerConfig config;
  config.log_dir = logDir;
  return config;
}

// ============================================================================
// AuditLogger Implementation
// ============================================================================

AuditLogger::AuditLogger(kj::StringPtr log_dir)
    : AuditLogger(AuditLoggerConfig::withDefaults(log_dir)) {}

AuditLogger::AuditLogger(AuditLoggerConfig config)
    : config_(kj::mv(config)), current_log_file_(kj::str("")), current_date_{},
      current_file_size_(0) {
  // Initialize stats
  {
    auto lock = stats_.lockExclusive();
    lock->total_logged = 0;
    lock->total_flushed = 0;
    lock->total_rotations = 0;
    lock->total_errors = 0;
    lock->current_queue_size = 0;
  }

  // Ensure log directory exists
  KJ_REQUIRE(mkdir(config_.log_dir.cStr(), 0755) == 0 || errno == EEXIST,
             "Failed to create log directory", config_.log_dir);

  // Initialize current date
  auto now = std::chrono::system_clock::now();
  auto time_t_val = std::chrono::system_clock::to_time_t(now);
  gmtime_r(&time_t_val, &current_date_);

  // Open initial log file
  open_log_file();

  // Apply retention policy
  cleanup_old_logs();

  // Start writer thread
  writer_thread_ = kj::heap<kj::Thread>([this]() { writer_thread_func(); });

  KJ_LOG(INFO, "Audit logger initialized", config_.log_dir);
}

AuditLogger::~AuditLogger() {
  // Signal shutdown
  shutdown_.store(true, std::memory_order_release);

  // Wait for writer thread to finish
  writer_thread_ = kj::none;

  // Final flush
  KJ_IF_SOME(fd, log_file_) {
    // Sync to disk
    fsync(fd.get());
  }

  KJ_LOG(INFO, "Audit logger shutdown complete", stats_.lockExclusive()->total_logged);
}

kj::Promise<void> AuditLogger::log(AuditLogEntry entry) {
  // Push to lock-free queue (non-blocking)
  queue_.push(kj::mv(entry));

  // Update stats
  {
    auto lock = stats_.lockExclusive();
    lock->total_logged++;
    lock->current_queue_size = queue_.size();
  }

  // Return immediately - actual write happens in background
  return kj::READY_NOW;
}

kj::Promise<void> AuditLogger::log(AuditLogType type, kj::String action, kj::String user_id,
                                   kj::String ip_address, kj::Maybe<kj::String> request_id) {
  AuditLogEntry entry;
  entry.timestamp = std::chrono::system_clock::now();
  entry.type = type;
  entry.action = kj::mv(action);
  entry.user_id = kj::mv(user_id);
  entry.ip_address = kj::mv(ip_address);
  entry.request_id = kj::mv(request_id);

  return log(kj::mv(entry));
}

kj::Promise<void> AuditLogger::flush() {
  // Wait until queue is empty
  while (!queue_.empty()) {
    std::this_thread::sleep_for(std::chrono::microseconds(100));
  }

  // Sync file to disk
  KJ_IF_SOME(fd, log_file_) {
    fsync(fd.get());
  }

  return kj::READY_NOW;
}

void AuditLogger::writer_thread_func() {
  kj::Vector<AuditLogEntry> batch;
  batch.reserve(100);

  while (!shutdown_.load(std::memory_order_acquire) || !queue_.empty()) {
    // Drain queue into batch
    while (batch.size() < 100) {
      auto maybe_entry = queue_.pop();
      KJ_IF_SOME(entry, maybe_entry) {
        batch.add(kj::mv(entry));
      }
      else {
        break;
      }
    }

    if (batch.empty()) {
      // No entries, sleep briefly
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
      continue;
    }

    // Write batch
    for (const auto& entry : batch) {
      write_entry(entry);
    }

    // Sync periodically
    static int sync_counter = 0;
    if (++sync_counter >= 100) {
      KJ_IF_SOME(fd, log_file_) {
        fsync(fd.get());
      }
      sync_counter = 0;
    }

    // Update stats
    {
      auto lock = stats_.lockExclusive();
      lock->total_flushed += batch.size();
      lock->current_queue_size = queue_.size();
    }

    batch.clear();
  }
}

void AuditLogger::write_entry(const AuditLogEntry& entry) {
  // Check for date change and rotation
  maybe_rotate_log();

  // Format entry as NDJSON
  auto json = entry.to_ndjson();

  // Write to file
  KJ_IF_SOME(fd, log_file_) {
    auto bytes = json.asBytes();
    ssize_t written = ::write(fd.get(), bytes.begin(), bytes.size());
    if (written < 0) {
      // Write error
      auto lock = stats_.lockExclusive();
      lock->total_errors++;
      KJ_LOG(ERROR, "Failed to write audit log entry", strerror(errno));
    } else {
      current_file_size_ += written;
    }
  }

  // Console output if enabled
  if (config_.enable_console_output) {
    KJ_LOG(INFO, "AUDIT", json);
  }
}

void AuditLogger::maybe_rotate_log() {
  // Check for size rotation
  if (current_file_size_ >= config_.max_file_size) {
    // Close current file
    log_file_ = kj::none;

    // Open new file (with incremented counter)
    open_log_file();

    {
      auto lock = stats_.lockExclusive();
      lock->total_rotations++;
    }

    KJ_LOG(INFO, "Rotated audit log due to size", current_file_size_);
    return;
  }

  // Check for date change
  auto now = std::chrono::system_clock::now();
  auto time_t_val = std::chrono::system_clock::to_time_t(now);
  std::tm now_tm;
  gmtime_r(&time_t_val, &now_tm);

  if (now_tm.tm_year != current_date_.tm_year || now_tm.tm_mon != current_date_.tm_mon ||
      now_tm.tm_mday != current_date_.tm_mday) {
    // Date changed, rotate
    current_date_ = now_tm;
    log_file_ = kj::none;
    open_log_file();

    {
      auto lock = stats_.lockExclusive();
      lock->total_rotations++;
    }

    // Cleanup old logs
    cleanup_old_logs();

    KJ_LOG(INFO, "Rotated audit log due to date change");
  }
}

kj::String AuditLogger::generate_log_path(const std::tm& date) {
  // std::format returns std::string; convert via .c_str() for kj::str
  auto formatted =
      std::format("{:04d}-{:02d}-{:02d}", date.tm_year + 1900, date.tm_mon + 1, date.tm_mday);
  return kj::str(config_.log_dir, "/audit-", formatted.c_str(), ".log");
}

void AuditLogger::open_log_file() {
  auto path = generate_log_path(current_date_);

  // Check if file exists to determine if we need to add counter
  int counter = 0;
  auto final_path = kj::mv(path);
  while (access(final_path.cStr(), F_OK) == 0) {
    counter++;
    // std::format returns std::string; convert via .c_str() for kj::str
    auto date_formatted = std::format("{:04d}-{:02d}-{:02d}", current_date_.tm_year + 1900,
                                      current_date_.tm_mon + 1, current_date_.tm_mday);
    final_path = kj::str(config_.log_dir, "/audit-", date_formatted.c_str(), "-", counter, ".log");
  }

  // Open file for append
  int fd = ::open(final_path.cStr(), O_WRONLY | O_CREAT | O_APPEND, 0644);
  KJ_REQUIRE(fd >= 0, "Failed to open audit log file", final_path, strerror(errno));

  log_file_ = kj::AutoCloseFd(fd);
  current_log_file_ = kj::mv(final_path);
  current_file_size_ = 0;

  // Get current file size if appending to existing file
  struct stat st;
  if (fstat(fd, &st) == 0) {
    current_file_size_ = st.st_size;
  }
}

void AuditLogger::cleanup_old_logs() {
  // Calculate cutoff date
  auto now = std::chrono::system_clock::now();
  auto cutoff = now - std::chrono::hours(24 * config_.retention_days);
  auto cutoff_time = std::chrono::system_clock::to_time_t(cutoff);

  // Open log directory
  DIR* dir = opendir(config_.log_dir.cStr());
  if (dir == nullptr) {
    KJ_LOG(ERROR, "Failed to open log directory for cleanup", config_.log_dir);
    return;
  }

  KJ_DEFER(closedir(dir));

  // Scan for old log files
  struct dirent* entry;
  while ((entry = readdir(dir)) != nullptr) {
    // Check if it's a log file
    kj::StringPtr name = entry->d_name;
    if (!name.startsWith("audit-") || !name.endsWith(".log")) {
      continue;
    }

    // Build full path
    auto full_path = kj::str(config_.log_dir, "/", name);

    // Get file modification time
    struct stat st;
    if (stat(full_path.cStr(), &st) != 0) {
      continue;
    }

    // Delete if too old
    if (st.st_mtime < cutoff_time) {
      if (unlink(full_path.cStr()) == 0) {
        KJ_LOG(INFO, "Deleted old audit log", name);
      }
    }
  }
}

void AuditLogger::apply_retention_policy() {
  cleanup_old_logs();
}

AuditLogger::Stats AuditLogger::get_stats() const {
  auto lock = stats_.lockExclusive();
  return {lock->total_logged, lock->total_flushed, lock->total_rotations, lock->total_errors,
          lock->current_queue_size};
}

} // namespace veloz::gateway::audit
