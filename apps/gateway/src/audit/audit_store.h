#pragma once

#include "audit_logger.h"

#include <chrono>
#include <kj/common.h>
#include <kj/filesystem.h>
#include <kj/memory.h>
#include <kj/string.h>
#include <kj/vector.h>

namespace veloz::gateway::audit {

// ============================================================================
// Audit Query Options
// ============================================================================

/**
 * @brief Options for querying audit logs
 */
struct AuditQueryOptions {
  kj::Maybe<AuditLogType> type;
  kj::Maybe<kj::String> user_id;
  kj::Maybe<kj::String> ip_address;
  kj::Maybe<std::chrono::system_clock::time_point> start_time;
  kj::Maybe<std::chrono::system_clock::time_point> end_time;
  kj::Maybe<kj::String> action_contains;
  uint32_t limit = 100;
  uint32_t offset = 0;
  bool descending = true; // Newest first
};

/**
 * @brief Result of an audit query
 */
struct AuditQueryResult {
  kj::Vector<AuditLogEntry> entries;
  uint64_t total_count;
  bool has_more;
};

// ============================================================================
// Audit Store
// ============================================================================

/**
 * @brief Audit log storage and query interface
 *
 * Provides:
 * - Log file management
 * - Historical log querying
 * - Log file compression for old logs
 * - Statistics aggregation
 */
class AuditStore {
public:
  /**
   * @brief Construct an audit store
   */
  explicit AuditStore(kj::StringPtr log_dir);

  /**
   * @brief Query audit logs with filters
   *
   * @param options Query options
   * @return Query result with matching entries
   */
  kj::Promise<AuditQueryResult> query(const AuditQueryOptions& options);

  /**
   * @brief Get a single log entry by ID (request_id)
   */
  kj::Promise<kj::Maybe<AuditLogEntry>> get_by_request_id(kj::StringPtr request_id);

  /**
   * @brief Get count of log entries matching a filter
   */
  kj::Promise<uint64_t> count(const AuditQueryOptions& options);

  /**
   * @brief Get list of available log files
   */
  kj::Vector<kj::String> list_log_files();

  /**
   * @brief Get statistics for a time range
   */
  struct TimeRangeStats {
    uint64_t total_entries;
    uint64_t auth_count;
    uint64_t order_count;
    uint64_t apikey_count;
    uint64_t error_count;
    uint64_t access_count;
    kj::HashMap<kj::String, uint64_t> action_counts;
    kj::HashMap<kj::String, uint64_t> user_counts;
  };

  kj::Promise<kj::Own<TimeRangeStats>> get_stats(std::chrono::system_clock::time_point start,
                                                 std::chrono::system_clock::time_point end);

  /**
   * @brief Compress old log files
   *
   * Compresses log files older than specified days.
   *
   * @param older_than_days Compress files older than this many days
   * @return Number of files compressed
   */
  kj::Promise<uint32_t> compress_old_logs(uint32_t older_than_days = 7);

  /**
   * @brief Delete log files older than retention period
   *
   * @param retention_days Delete files older than this many days
   * @return Number of files deleted
   */
  kj::Promise<uint32_t> delete_old_logs(uint32_t retention_days = 30);

private:
  // Parse a log file line by line
  kj::Vector<AuditLogEntry> parse_log_file(kj::StringPtr path);

  // Check if entry matches query options
  bool matches_query(const AuditLogEntry& entry, const AuditQueryOptions& options) const;

  // Get log file paths for a date range
  kj::Vector<kj::String> get_log_files_for_range(std::chrono::system_clock::time_point start,
                                                 std::chrono::system_clock::time_point end);

  kj::String log_dir_;
};

} // namespace veloz::gateway::audit
