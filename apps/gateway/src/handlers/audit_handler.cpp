#include "handlers/audit_handler.h"

#include "audit/audit_logger.h"
#include "audit/audit_store.h"
#include "veloz/core/json.h"

#include <chrono>
#include <ctime>
#include <kj/debug.h>

namespace veloz::gateway {

// ============================================================================
// Helper Functions
// ============================================================================

namespace {

// Parse ISO8601 timestamp string to time_point
kj::Maybe<std::chrono::system_clock::time_point> parse_iso8601(kj::StringPtr str) {
  // Expected format: YYYY-MM-DDTHH:MM:SSZ or YYYY-MM-DDTHH:MM:SS.sssZ
  std::tm tm = {};
  char* end = strptime(str.cStr(), "%Y-%m-%dT%H:%M:%S", &tm);
  if (!end) {
    return kj::none;
  }

  auto time = std::chrono::system_clock::from_time_t(std::mktime(&tm));

  // Handle optional milliseconds
  if (*end == '.') {
    ++end;
    int ms = 0;
    while (*end >= '0' && *end <= '9') {
      ms = ms * 10 + (*end - '0');
      ++end;
    }
    time += std::chrono::milliseconds(ms);
  }

  // Skip 'Z' or timezone
  if (*end == 'Z') {
    ++end;
  }

  return time;
}

// Format time_point to ISO8601 string
kj::String format_iso8601(const std::chrono::system_clock::time_point& tp) {
  auto time = std::chrono::system_clock::to_time_t(tp);
  std::tm tm = *std::gmtime(&time);

  char buffer[32];
  std::strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%SZ", &tm);
  return kj::heapString(buffer);
}

// Extract query parameter from query string
kj::Maybe<kj::String> getQueryParam(kj::StringPtr queryString, kj::StringPtr name) {
  // queryString format: "param1=value1&param2=value2"
  kj::String searchKey = kj::str(name, "=");

  size_t pos = 0;
  while (pos < queryString.size()) {
    auto slice = queryString.slice(pos);
    KJ_IF_SOME(foundPos, slice.findFirst('&')) {
      auto param = slice.slice(0, foundPos);
      if (param.startsWith(searchKey)) {
        // URL decode value
        auto value = param.slice(searchKey.size());
        return kj::heapString(value);
      }
      pos += foundPos + 1;
    }
    else {
      // Last parameter
      if (slice.startsWith(searchKey)) {
        auto value = slice.slice(searchKey.size());
        return kj::heapString(value);
      }
      break;
    }
  }

  return kj::none;
}

// Parse integer from string
kj::Maybe<uint32_t> parseUint32(kj::StringPtr str) {
  if (str.size() == 0)
    return kj::none;

  uint32_t result = 0;
  for (char c : str) {
    if (c < '0' || c > '9')
      return kj::none;
    result = result * 10 + static_cast<uint32_t>(c - '0');
  }
  return result;
}

// Escape JSON string (currently unused but kept for future use)
[[maybe_unused]] kj::String escapeJson(kj::StringPtr str) {
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
      result.add(c);
      break;
    }
  }
  result.add('\0');
  return kj::String(result.releaseAsArray());
}

} // namespace

// ============================================================================
// AuditHandler Implementation
// ============================================================================

AuditHandler::AuditHandler(audit::AuditStore& store) : store_(store) {}

kj::Promise<void> AuditHandler::handleQueryLogs(RequestContext& ctx) {
  try {
    // Parse query parameters
    audit::AuditQueryOptions options;

    // log_type parameter
    KJ_IF_SOME(typeStr, getQueryParam(ctx.queryString, "type")) {
      auto maybeType = audit::stringToAuditLogType(typeStr);
      KJ_IF_SOME(type, maybeType) {
        options.type = type;
      }
    }

    // user_id parameter
    KJ_IF_SOME(userId, getQueryParam(ctx.queryString, "user_id")) {
      options.user_id = kj::mv(userId);
    }

    // ip_address parameter
    KJ_IF_SOME(ipAddress, getQueryParam(ctx.queryString, "ip_address")) {
      options.ip_address = kj::mv(ipAddress);
    }

    // start_time parameter
    KJ_IF_SOME(startTimeStr, getQueryParam(ctx.queryString, "start_time")) {
      KJ_IF_SOME(startTime, parse_iso8601(startTimeStr)) {
        options.start_time = startTime;
      }
    }

    // end_time parameter
    KJ_IF_SOME(endTimeStr, getQueryParam(ctx.queryString, "end_time")) {
      KJ_IF_SOME(endTime, parse_iso8601(endTimeStr)) {
        options.end_time = endTime;
      }
    }

    // limit parameter (default: 100)
    KJ_IF_SOME(limitStr, getQueryParam(ctx.queryString, "limit")) {
      KJ_IF_SOME(limit, parseUint32(limitStr)) {
        options.limit = limit;
      }
    }

    // offset parameter (default: 0)
    KJ_IF_SOME(offsetStr, getQueryParam(ctx.queryString, "offset")) {
      KJ_IF_SOME(offset, parseUint32(offsetStr)) {
        options.offset = offset;
      }
    }

    // Query audit logs
    return store_.query(options).then([&ctx](audit::AuditQueryResult result) -> kj::Promise<void> {
      // Build JSON response
      // Performance target: <100μs

      auto builder = core::JsonBuilder::object();
      builder.put("status", "success");

      builder.put_array("data", [&](core::JsonBuilder& arr) {
        for (const auto& entry : result.entries) {
          arr.add_object([&](core::JsonBuilder& obj) {
            obj.put("timestamp", format_iso8601(entry.timestamp));
            obj.put("type", audit::auditLogTypeToString(entry.type));
            obj.put("action", entry.action);
            obj.put("user_id", entry.user_id);
            obj.put("ip_address", entry.ip_address);

            KJ_IF_SOME(reqId, entry.request_id) {
              obj.put("request_id", reqId);
            }

            obj.put_object("details", [&](core::JsonBuilder& details) {
              for (const auto& [key, value] : entry.details) {
                details.put(key, value);
              }
            });
          });
        }
      });

      builder.put_object("pagination", [&](core::JsonBuilder& pagination) {
        pagination.put("total", static_cast<int64_t>(result.total_count));
        pagination.put("has_more", result.has_more);
      });

      kj::String json_body = builder.build();
      return ctx.sendJson(200, json_body);
    });

  } catch (const kj::Exception& e) {
    KJ_LOG(ERROR, "Error in audit query handler", e);
    return ctx.sendError(500, "Internal Server Error"_kj);
  }
}

kj::Promise<void> AuditHandler::handleGetStats(RequestContext& ctx) {
  try {
    // Get time range from query parameters (default: last 24 hours)
    auto now = std::chrono::system_clock::now();
    auto default_start = now - std::chrono::hours(24);

    std::chrono::system_clock::time_point start_time = default_start;
    std::chrono::system_clock::time_point end_time = now;

    KJ_IF_SOME(startTimeStr, getQueryParam(ctx.queryString, "start_time")) {
      KJ_IF_SOME(parsed, parse_iso8601(startTimeStr)) {
        start_time = parsed;
      }
    }

    KJ_IF_SOME(endTimeStr, getQueryParam(ctx.queryString, "end_time")) {
      KJ_IF_SOME(parsed, parse_iso8601(endTimeStr)) {
        end_time = parsed;
      }
    }

    // Get statistics
    return store_.get_stats(start_time, end_time)
        .then([&ctx](kj::Own<audit::AuditStore::TimeRangeStats> stats) -> kj::Promise<void> {
          // Build JSON response
          // Performance target: <100μs

          auto builder = core::JsonBuilder::object();
          builder.put("status", "success");

          builder.put_object("data", [&](core::JsonBuilder& data) {
            data.put("total_entries", static_cast<int64_t>(stats->total_entries));
            data.put("auth_count", static_cast<int64_t>(stats->auth_count));
            data.put("order_count", static_cast<int64_t>(stats->order_count));
            data.put("apikey_count", static_cast<int64_t>(stats->apikey_count));
            data.put("error_count", static_cast<int64_t>(stats->error_count));
            data.put("access_count", static_cast<int64_t>(stats->access_count));

            data.put_object("action_counts", [&](core::JsonBuilder& actions) {
              for (const auto& [action, count] : stats->action_counts) {
                actions.put(action, static_cast<int64_t>(count));
              }
            });

            data.put_object("user_counts", [&](core::JsonBuilder& users) {
              for (const auto& [user_id, count] : stats->user_counts) {
                users.put(user_id, static_cast<int64_t>(count));
              }
            });
          });

          kj::String json_body = builder.build();
          return ctx.sendJson(200, json_body);
        });

  } catch (const kj::Exception& e) {
    KJ_LOG(ERROR, "Error in audit stats handler", e);
    return ctx.sendError(500, "Internal Server Error"_kj);
  }
}

kj::Promise<void> AuditHandler::handleTriggerArchive(RequestContext& ctx) {
  try {
    // Trigger compression of old logs (default: older than 7 days)
    return store_.compress_old_logs(7).then([&ctx](uint32_t compressed_count) -> kj::Promise<void> {
      auto builder = core::JsonBuilder::object();
      builder.put("status", "success");
      builder.put_object("data", [&](core::JsonBuilder& data) {
        data.put("compressed_files", static_cast<int64_t>(compressed_count));
      });

      kj::String json_body = builder.build();
      return ctx.sendJson(200, json_body);
    });

  } catch (const kj::Exception& e) {
    KJ_LOG(ERROR, "Error in audit archive handler", e);
    return ctx.sendError(500, "Internal Server Error"_kj);
  }
}

} // namespace veloz::gateway
