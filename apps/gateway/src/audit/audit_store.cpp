#include "audit_store.h"

#include <algorithm>
#include <chrono>
#include <cstring>
#include <dirent.h>
#include <fcntl.h>
#include <format>
#include <kj/array.h>
#include <kj/async.h>
#include <kj/debug.h>
#include <kj/string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#ifdef __APPLE__
#include <sys/time.h> // For timezone on macOS
#endif

namespace veloz::gateway::audit {

// ============================================================================
// Helper Functions
// ============================================================================

namespace {

// Helper to extract a quoted string value after a key
kj::Maybe<kj::String> extract_string_value(kj::StringPtr line, kj::StringPtr key) {
  auto key_start = line.find(key);
  if (key_start == kj::none)
    return kj::none;

  kj::Maybe<kj::String> result = kj::none;
  KJ_IF_SOME(start, key_start) {
    auto val_start = start + key.size();
    auto val_end = line.slice(val_start).find("\"");
    if (val_end == kj::none)
      return kj::none;
    KJ_IF_SOME(end, val_end) {
      result = kj::str(line.slice(val_start, val_start + end));
    }
  }
  return result;
}

// Parse NDJSON line to AuditLogEntry
kj::Maybe<AuditLogEntry> parse_ndjson_line(kj::StringPtr line) {
  // Simple JSON parsing for our NDJSON format
  // Format: {"timestamp":"ISO8601","type":"string","action":"string",...}

  AuditLogEntry entry;

  // Find timestamp
  auto ts_result = extract_string_value(line, "\"timestamp\":\"");
  if (ts_result == kj::none)
    return kj::none;

  KJ_IF_SOME(ts_str, ts_result) {
    // Parse ISO8601 timestamp (simplified parsing)
    // Format: YYYY-MM-DDTHH:MM:SS.sssZ
    std::tm tm_val = {};
    int year, month, day, hour, min, sec, ms;
    if (sscanf(ts_str.cStr(), "%d-%d-%dT%d:%d:%d.%dZ", &year, &month, &day, &hour, &min, &sec,
               &ms) != 7) {
      return kj::none;
    }
    tm_val.tm_year = year - 1900;
    tm_val.tm_mon = month - 1;
    tm_val.tm_mday = day;
    tm_val.tm_hour = hour;
    tm_val.tm_min = min;
    tm_val.tm_sec = sec;
#ifdef __APPLE__
    // macOS doesn't have timegm, use portable workaround
    // Parse timestamp manually to avoid timezone issues
    // ISO8601 timestamp: YYYY-MM-DDTHH:MM:SS.sssZ (UTC)
    time_t local_time = mktime(&tm_val);
    // Get timezone offset by comparing gmtime and localtime
    struct tm* gmt = gmtime(&local_time);
    struct tm* lcl = localtime(&local_time);
    if (gmt != nullptr && lcl != nullptr) {
      // Calculate timezone offset in seconds
      int timezone_offset = (lcl->tm_hour - gmt->tm_hour) * 3600 + (lcl->tm_min - gmt->tm_min) * 60;
      time_t time_t_val = local_time + timezone_offset;
      entry.timestamp =
          std::chrono::system_clock::from_time_t(time_t_val) + std::chrono::milliseconds(ms);
    } else {
      entry.timestamp =
          std::chrono::system_clock::from_time_t(local_time) + std::chrono::milliseconds(ms);
    }
#else
    time_t time_t_val = timegm(&tm_val);
    entry.timestamp =
        std::chrono::system_clock::from_time_t(time_t_val) + std::chrono::milliseconds(ms);
#endif
  }
  else {
    return kj::none;
  }

  // Find type
  auto type_result = extract_string_value(line, "\"type\":\"");
  if (type_result == kj::none)
    return kj::none;
  KJ_IF_SOME(type_str, type_result) {
    auto maybe_type = stringToAuditLogType(type_str);
    KJ_IF_SOME(t, maybe_type) {
      entry.type = t;
    }
    else {
      return kj::none;
    }
  }
  else {
    return kj::none;
  }

  // Find action
  auto action_result = extract_string_value(line, "\"action\":\"");
  if (action_result == kj::none)
    return kj::none;
  KJ_IF_SOME(action_str, action_result) {
    entry.action = kj::mv(action_str);
  }
  else {
    return kj::none;
  }

  // Find user_id
  auto user_result = extract_string_value(line, "\"user_id\":\"");
  if (user_result == kj::none)
    return kj::none;
  KJ_IF_SOME(user_str, user_result) {
    entry.user_id = kj::mv(user_str);
  }
  else {
    return kj::none;
  }

  // Find ip_address
  auto ip_result = extract_string_value(line, "\"ip_address\":\"");
  if (ip_result == kj::none)
    return kj::none;
  KJ_IF_SOME(ip_str, ip_result) {
    entry.ip_address = kj::mv(ip_str);
  }
  else {
    return kj::none;
  }

  // Find request_id (optional)
  auto req_result = extract_string_value(line, "\"request_id\":\"");
  KJ_IF_SOME(req_str, req_result) {
    entry.request_id = kj::mv(req_str);
  }

  // TODO: Parse details map if needed

  return entry;
}

// Get date string from time_point
kj::String date_string(const std::chrono::system_clock::time_point& tp) {
  auto time_t_val = std::chrono::system_clock::to_time_t(tp);
  std::tm tm_val;
  gmtime_r(&time_t_val, &tm_val);
  // Format date as YYYY-MM-DD using KJ string concatenation
  // This avoids std::format which isn't portable
  char year_buf[5], month_buf[3], day_buf[3];
  snprintf(year_buf, sizeof(year_buf), "%04d", tm_val.tm_year + 1900);
  snprintf(month_buf, sizeof(month_buf), "%02d", tm_val.tm_mon + 1);
  snprintf(day_buf, sizeof(day_buf), "%02d", tm_val.tm_mday);
  return kj::str(year_buf, "-", month_buf, "-", day_buf);
}

// Check if string contains substring (case-insensitive)
bool contains_case_insensitive(kj::StringPtr haystack, kj::StringPtr needle) {
  if (needle.size() == 0)
    return true;
  if (haystack.size() < needle.size())
    return false;

  auto it = std::search(haystack.begin(), haystack.end(), needle.begin(), needle.end(),
                        [](char a, char b) { return std::tolower(a) == std::tolower(b); });

  return it != haystack.end();
}

} // namespace

// ============================================================================
// AuditStore Implementation
// ============================================================================

AuditStore::AuditStore(kj::StringPtr log_dir) : log_dir_(kj::str(log_dir)) {
  KJ_REQUIRE(mkdir(log_dir_.cStr(), 0755) == 0 || errno == EEXIST, "Failed to create log directory",
             log_dir_);
}

kj::Promise<AuditQueryResult> AuditStore::query(const AuditQueryOptions& options) {
  AuditQueryResult result;
  result.total_count = 0;
  result.has_more = false;

  // Get log files for the time range
  auto start_time = options.start_time.orDefault(
      [] { return std::chrono::system_clock::now() - std::chrono::hours(24 * 7); });
  auto end_time = options.end_time.orDefault([] { return std::chrono::system_clock::now(); });

  auto files = get_log_files_for_range(start_time, end_time);

  // Parse entries from each file
  kj::Vector<AuditLogEntry> all_entries;
  for (auto& file : files) {
    auto entries = parse_log_file(file);
    for (auto& entry : entries) {
      if (matches_query(entry, options)) {
        all_entries.add(kj::mv(entry));
      }
    }
  }

  result.total_count = all_entries.size();

  // Sort using index array (kj::Vector doesn't support std::sort directly)
  // Create index array for sorting
  kj::Vector<size_t> indices;
  indices.reserve(all_entries.size());
  for (size_t i = 0; i < all_entries.size(); ++i) {
    indices.add(i);
  }

  // Sort indices by timestamp
  if (options.descending) {
    std::sort(indices.begin(), indices.end(), [&all_entries](size_t a, size_t b) {
      return all_entries[a].timestamp > all_entries[b].timestamp;
    });
  } else {
    std::sort(indices.begin(), indices.end(), [&all_entries](size_t a, size_t b) {
      return all_entries[a].timestamp < all_entries[b].timestamp;
    });
  }

  // Apply pagination using sorted indices
  size_t start = options.offset;
  size_t end = std::min(start + static_cast<size_t>(options.limit), indices.size());

  for (size_t i = start; i < end; ++i) {
    result.entries.add(kj::mv(all_entries[indices[i]]));
  }

  result.has_more = end < indices.size();

  return result;
}

kj::Promise<kj::Maybe<AuditLogEntry>> AuditStore::get_by_request_id(kj::StringPtr request_id) {
  auto request_id_copy = kj::str(request_id);
  // Get all log files
  DIR* dir = opendir(log_dir_.cStr());
  if (dir == nullptr) {
    return kj::Maybe<AuditLogEntry>(kj::none);
  }

  KJ_DEFER(closedir(dir));

  // Scan all log files
  struct dirent* entry;
  while ((entry = readdir(dir)) != nullptr) {
    kj::StringPtr name = entry->d_name;
    if (!name.startsWith("audit-") || !name.endsWith(".log")) {
      continue;
    }

    auto full_path = kj::str(log_dir_, "/", name);
    auto entries = parse_log_file(full_path);

    for (const auto& log_entry : entries) {
      KJ_IF_SOME(rid, log_entry.request_id) {
        if (rid == request_id_copy) {
          return kj::Maybe<AuditLogEntry>(log_entry.clone());
        }
      }
    }
  }

  return kj::Maybe<AuditLogEntry>(kj::none);
}

kj::Promise<uint64_t> AuditStore::count(const AuditQueryOptions& options) {
  auto start_time = options.start_time.orDefault(
      [] { return std::chrono::system_clock::now() - std::chrono::hours(24 * 7); });
  auto end_time = options.end_time.orDefault([] { return std::chrono::system_clock::now(); });

  auto files = get_log_files_for_range(start_time, end_time);

  // Count entries matching query
  uint64_t count = 0;
  for (auto& file : files) {
    auto entries = parse_log_file(file);
    for (auto& entry : entries) {
      if (matches_query(entry, options)) {
        count++;
      }
    }
  }

  return count;
}

kj::Vector<kj::String> AuditStore::list_log_files() {
  kj::Vector<kj::String> files;

  DIR* dir = opendir(log_dir_.cStr());
  if (dir == nullptr) {
    return files;
  }

  KJ_DEFER(closedir(dir));

  struct dirent* entry;
  while ((entry = readdir(dir)) != nullptr) {
    kj::StringPtr name = entry->d_name;
    if (name.startsWith("audit-") && name.endsWith(".log")) {
      files.add(kj::str(name));
    }
  }

  // Sort by name (which includes date)
  // Use KJ approach: sort indices and build result
  kj::Vector<size_t> indices;
  indices.reserve(files.size());
  for (size_t i = 0; i < files.size(); ++i) {
    indices.add(i);
  }

  // Sort indices based on string comparison
  std::sort(indices.begin(), indices.end(),
            [&files](size_t a, size_t b) { return files[a] < files[b]; });

  // Build sorted result
  kj::Vector<kj::String> sorted_files;
  sorted_files.reserve(indices.size());
  for (size_t idx : indices) {
    sorted_files.add(kj::mv(files[idx]));
  }
  return sorted_files;
}

kj::Promise<kj::Own<AuditStore::TimeRangeStats>>
AuditStore::get_stats(std::chrono::system_clock::time_point start,
                      std::chrono::system_clock::time_point end) {
  auto stats = kj::heap<TimeRangeStats>();
  stats->total_entries = 0;
  stats->auth_count = 0;
  stats->order_count = 0;
  stats->apikey_count = 0;
  stats->error_count = 0;
  stats->access_count = 0;

  auto files = get_log_files_for_range(start, end);

  for (auto& file : files) {
    auto entries = parse_log_file(file);

    for (const auto& entry : entries) {
      if (entry.timestamp >= start && entry.timestamp <= end) {
        stats->total_entries++;

        switch (entry.type) {
        case AuditLogType::Auth:
          stats->auth_count++;
          break;
        case AuditLogType::Order:
          stats->order_count++;
          break;
        case AuditLogType::ApiKey:
          stats->apikey_count++;
          break;
        case AuditLogType::Error:
          stats->error_count++;
          break;
        case AuditLogType::Access:
          stats->access_count++;
          break;
        }

        // Count by action
        {
          auto maybeVal = stats->action_counts.find(entry.action);
          KJ_IF_SOME(val, maybeVal) {
            val += 1;
          }
          else {
            stats->action_counts.insert(kj::str(entry.action), 1);
          }
        }

        // Count by user
        {
          auto maybeVal = stats->user_counts.find(entry.user_id);
          KJ_IF_SOME(val, maybeVal) {
            val += 1;
          }
          else {
            stats->user_counts.insert(kj::str(entry.user_id), 1);
          }
        }
      }
    }
  }

  return stats;
}

kj::Promise<uint32_t> AuditStore::compress_old_logs(uint32_t older_than_days) {
  // Calculate cutoff date
  auto now = std::chrono::system_clock::now();
  auto cutoff = now - std::chrono::hours(24 * older_than_days);
  auto cutoff_time = std::chrono::system_clock::to_time_t(cutoff);

  uint32_t compressed_count = 0;

  DIR* dir = opendir(log_dir_.cStr());
  if (dir == nullptr) {
    return compressed_count;
  }

  KJ_DEFER(closedir(dir));

  struct dirent* entry;
  while ((entry = readdir(dir)) != nullptr) {
    kj::StringPtr name = entry->d_name;
    if (!name.startsWith("audit-") || !name.endsWith(".log")) {
      continue;
    }

    // Skip already compressed files
    if (name.endsWith(".gz")) {
      continue;
    }

    auto full_path = kj::str(log_dir_, "/", name);

    // Get file modification time
    struct stat st;
    if (stat(full_path.cStr(), &st) != 0) {
      continue;
    }

    // Compress if old enough
    if (st.st_mtime < cutoff_time) {
      // Use gzip to compress
      auto gz_path = kj::str(full_path, ".gz");
      auto cmd = kj::str("gzip -c '", full_path, "' > '", gz_path, "'");

      if (system(cmd.cStr()) == 0) {
        // Remove original file
        unlink(full_path.cStr());
        compressed_count++;
        KJ_LOG(INFO, "Compressed audit log", name);
      }
    }
  }

  return compressed_count;
}

kj::Promise<uint32_t> AuditStore::delete_old_logs(uint32_t retention_days) {
  auto now = std::chrono::system_clock::now();
  auto cutoff = now - std::chrono::hours(24 * retention_days);
  auto cutoff_time = std::chrono::system_clock::to_time_t(cutoff);

  uint32_t deleted_count = 0;

  DIR* dir = opendir(log_dir_.cStr());
  if (dir == nullptr) {
    return deleted_count;
  }

  KJ_DEFER(closedir(dir));

  struct dirent* entry;
  while ((entry = readdir(dir)) != nullptr) {
    kj::StringPtr name = entry->d_name;
    if (!name.startsWith("audit-")) {
      continue;
    }
    if (!name.endsWith(".log") && !name.endsWith(".log.gz")) {
      continue;
    }

    auto full_path = kj::str(log_dir_, "/", name);

    struct stat st;
    if (stat(full_path.cStr(), &st) != 0) {
      continue;
    }

    if (st.st_mtime < cutoff_time) {
      if (unlink(full_path.cStr()) == 0) {
        deleted_count++;
        KJ_LOG(INFO, "Deleted old audit log", name);
      }
    }
  }

  return deleted_count;
}

kj::Vector<AuditLogEntry> AuditStore::parse_log_file(kj::StringPtr path) {
  kj::Vector<AuditLogEntry> entries;

  int fd = ::open(path.cStr(), O_RDONLY);
  if (fd < 0) {
    return entries;
  }

  KJ_DEFER(close(fd));

  // Read file into buffer using KJ Vector
  kj::Vector<char> content;
  char buffer[4096];
  ssize_t bytes_read;
  while ((bytes_read = ::read(fd, buffer, sizeof(buffer))) > 0) {
    content.addAll(kj::ArrayPtr<const char>(buffer, bytes_read));
  }

  size_t pos = 0;
  while (pos < content.size()) {
    size_t nlPos = content.size();
    for (size_t i = pos; i < content.size(); ++i) {
      if (content[i] == '\n') {
        nlPos = i;
        break;
      }
    }

    auto line = kj::str(kj::ArrayPtr<const char>(content.begin() + pos, nlPos - pos));
    auto maybe_entry = parse_ndjson_line(line);

    KJ_IF_SOME(entry, maybe_entry) {
      entries.add(kj::mv(entry));
    }

    if (nlPos == content.size()) {
      break;
    }
    pos = nlPos + 1;
  }

  return entries;
}

bool AuditStore::matches_query(const AuditLogEntry& entry, const AuditQueryOptions& options) const {
  // Filter by type
  KJ_IF_SOME(type, options.type) {
    if (entry.type != type)
      return false;
  }

  // Filter by user_id
  KJ_IF_SOME(user_id, options.user_id) {
    if (entry.user_id != user_id)
      return false;
  }

  // Filter by ip_address
  KJ_IF_SOME(ip, options.ip_address) {
    if (entry.ip_address != ip)
      return false;
  }

  // Filter by time range
  KJ_IF_SOME(start, options.start_time) {
    if (entry.timestamp < start)
      return false;
  }

  KJ_IF_SOME(end, options.end_time) {
    if (entry.timestamp > end)
      return false;
  }

  // Filter by action substring
  KJ_IF_SOME(action_filter, options.action_contains) {
    if (!contains_case_insensitive(entry.action, action_filter)) {
      return false;
    }
  }

  return true;
}

kj::Vector<kj::String>
AuditStore::get_log_files_for_range(std::chrono::system_clock::time_point start,
                                    std::chrono::system_clock::time_point end) {

  kj::Vector<kj::String> files;

  // Generate date strings for the range
  auto start_date = date_string(start);
  auto end_date = date_string(end);

  DIR* dir = opendir(log_dir_.cStr());
  if (dir == nullptr) {
    return files;
  }

  KJ_DEFER(closedir(dir));

  struct dirent* entry;
  while ((entry = readdir(dir)) != nullptr) {
    kj::StringPtr name = entry->d_name;
    if (!name.startsWith("audit-") || !name.endsWith(".log")) {
      continue;
    }

    // Extract date from filename: audit-YYYY-MM-DD.log
    auto date_part = name.slice(6, 16); // YYYY-MM-DD

    // Check if within range
    if (date_part >= start_date.slice(0, 10) && date_part <= end_date.slice(0, 10)) {
      files.add(kj::str(log_dir_, "/", name));
    }
  }

  return files;
}

} // namespace veloz::gateway::audit
