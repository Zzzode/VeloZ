#pragma once

#include "veloz/core/lockfree_queue.h"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <kj/async.h>
#include <kj/common.h>
#include <kj/filesystem.h>
#include <kj/map.h>
#include <kj/memory.h>
#include <kj/mutex.h>
#include <kj/string.h>
#include <kj/thread.h>
#include <kj/time.h>
#include <kj/vector.h>

namespace veloz::gateway::audit {

// ============================================================================
// Audit Log Types
// ============================================================================

/**
 * @brief Type of audit log entry
 */
enum class AuditLogType : uint8_t {
  Auth = 0,   // Authentication events (login, logout, token refresh)
  Order = 1,  // Order-related events (create, cancel, modify)
  ApiKey = 2, // API key management events (create, delete, rotate)
  Error = 3,  // Error events (rate limit, validation, system errors)
  Access = 4  // API access events (endpoint access, permission checks)
};

/**
 * @brief Convert audit log type to string
 */
kj::StringPtr auditLogTypeToString(AuditLogType type) noexcept;

/**
 * @brief Parse string to audit log type
 */
kj::Maybe<AuditLogType> stringToAuditLogType(kj::StringPtr str) noexcept;

// ============================================================================
// Audit Log Entry
// ============================================================================

/**
 * @brief Single audit log entry
 *
 * Contains timestamp, type, and contextual information for an audit event.
 * Designed to be serialized to NDJSON format.
 */
struct AuditLogEntry {
  std::chrono::system_clock::time_point timestamp;
  AuditLogType type;
  kj::String action;
  kj::String user_id;
  kj::String ip_address;
  kj::Maybe<kj::String> request_id;
  kj::HashMap<kj::String, kj::String> details;

  /**
   * @brief Convert entry to NDJSON format (newline-delimited JSON)
   *
   * Returns a JSON string with the following structure:
   * {"timestamp":"ISO8601","type":"string","action":"string","user_id":"string",...}
   */
  kj::String to_ndjson() const;

  /**
   * @brief Create a copy of the entry
   *
   * Required for HashMap compatibility since AuditLogEntry is move-only.
   */
  AuditLogEntry clone() const;
};

// ============================================================================
// Configuration
// ============================================================================

/**
 * @brief Configuration for the audit logger
 */
struct AuditLoggerConfig {
  kj::StringPtr log_dir = "/var/log/veloz/audit"_kj;
  size_t max_file_size = 100 * 1024 * 1024; // 100MB
  uint32_t retention_days = 30;
  size_t queue_capacity = 10000;
  bool enable_console_output = false;

  static AuditLoggerConfig withDefaults(kj::StringPtr logDir);
};

// ============================================================================
// Audit Logger
// ============================================================================

/**
 * @brief High-throughput async audit logger
 *
 * Features:
 * - Lock-free queue for non-blocking log entry submission
 * - Background writer thread for file I/O
 * - Automatic log rotation by size and date
 * - Retention policy enforcement
 * - NDJSON format for easy parsing
 *
 * Performance target: <5μs per log entry (non-blocking)
 */
class AuditLogger {
public:
  /**
   * @brief Construct an audit logger with default config
   */
  explicit AuditLogger(kj::StringPtr log_dir);

  /**
   * @brief Construct an audit logger with custom config
   */
  explicit AuditLogger(AuditLoggerConfig config);

  /**
   * @brief Destructor - flushes remaining entries and stops writer thread
   */
  ~AuditLogger();

  // Non-copyable, non-movable
  AuditLogger(const AuditLogger&) = delete;
  AuditLogger& operator=(const AuditLogger&) = delete;
  AuditLogger(AuditLogger&&) = delete;
  AuditLogger& operator=(AuditLogger&&) = delete;

  /**
   * @brief Log an audit entry asynchronously
   *
   * Non-blocking operation - pushes to lock-free queue.
   * Returns immediately, actual writing happens in background thread.
   *
   * @param entry The audit log entry to record
   * @return Promise that completes when entry is queued
   */
  kj::Promise<void> log(AuditLogEntry entry);

  /**
   * @brief Convenience method to log an audit entry with minimal fields
   */
  kj::Promise<void> log(AuditLogType type, kj::String action, kj::String user_id,
                        kj::String ip_address, kj::Maybe<kj::String> request_id = kj::none);

  /**
   * @brief Flush all pending entries to disk
   *
   * Blocks until all queued entries are written.
   *
   * @return Promise that completes when flush is done
   */
  kj::Promise<void> flush();

  /**
   * @brief Get the number of entries waiting to be written
   */
  [[nodiscard]] size_t pending_count() const noexcept {
    return queue_.size();
  }

  /**
   * @brief Get the current log file path
   */
  [[nodiscard]] kj::StringPtr current_log_file() const noexcept {
    return current_log_file_;
  }

  /**
   * @brief Apply retention policy and delete old log files
   */
  void apply_retention_policy();

  /**
   * @brief Get statistics about the logger
   */
  struct Stats {
    uint64_t total_logged;
    uint64_t total_flushed;
    uint64_t total_rotations;
    uint64_t total_errors;
    size_t current_queue_size;
  };

  [[nodiscard]] Stats get_stats() const;

private:
  // Writer thread entry point
  void writer_thread_func();

  // Write a single entry to the log file
  void write_entry(const AuditLogEntry& entry);

  // Check if log rotation is needed and perform it
  void maybe_rotate_log();

  // Generate log file path based on current date
  kj::String generate_log_path(const std::tm& date);

  // Open or reopen the current log file
  void open_log_file();

  // Check retention and delete old files
  void cleanup_old_logs();

  // Configuration
  AuditLoggerConfig config_;

  // Log state
  veloz::core::LockFreeQueue<AuditLogEntry> queue_;
  kj::Maybe<kj::Own<kj::Thread>> writer_thread_;
  kj::Maybe<kj::AutoCloseFd> log_file_;
  kj::String current_log_file_;
  std::tm current_date_;
  uint64_t current_file_size_;

  // Statistics (thread-safe using MutexGuarded)
  struct StatsData {
    uint64_t total_logged;
    uint64_t total_flushed;
    uint64_t total_rotations;
    uint64_t total_errors;
    size_t current_queue_size;
  };

  kj::MutexGuarded<StatsData> stats_;

  // Shutdown flag
  std::atomic<bool> shutdown_{false};
};

} // namespace veloz::gateway::audit
