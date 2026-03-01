#include "handlers/health_handler.h"

#include "bridge/engine_bridge.h"
#include "veloz/core/json.h"

#include <chrono>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <kj/async-io.h>
#include <kj/debug.h>
#include <kj/time.h>
#include <sstream>
#include <unistd.h>

#if defined(__APPLE__)
#include <mach/mach.h>
#include <mach/task_info.h>
#endif

namespace veloz::gateway {

// ============================================================================
// Constructor
// ============================================================================

HealthHandler::HealthHandler(bridge::EngineBridge& bridge)
    : bridge_(bridge), start_time_(kj::systemPreciseCalendarClock().now()) {}

// ============================================================================
// HealthHandler Implementation
// ============================================================================

kj::Promise<void> HealthHandler::handleSimpleHealth(RequestContext& ctx) {
  try {
    // Get current timestamp
    auto now = std::chrono::system_clock::now();
    auto now_time_t = std::chrono::system_clock::to_time_t(now);

    auto builder = core::JsonBuilder::object();
    builder.put("status", "ok");
    builder.put("timestamp", format_timestamp(static_cast<std::int64_t>(now_time_t)));

    kj::String json_body = builder.build();
    return ctx.sendJson(200, json_body);
  } catch (const kj::Exception& e) {
    KJ_LOG(ERROR, "Error in simple health handler", e);
    return ctx.sendError(500, "Internal Server Error"_kj);
  }
}

kj::Promise<void> HealthHandler::handleDetailedHealth(RequestContext& ctx) {
  try {
    // Get current timestamp
    auto now = std::chrono::system_clock::now();
    auto now_time_t = std::chrono::system_clock::to_time_t(now);

    auto builder = core::JsonBuilder::object();

    builder.put("status", "ok");
    builder.put("timestamp", format_timestamp(static_cast<std::int64_t>(now_time_t)));

    // Calculate uptime in seconds
    kj::Date current_time = kj::systemPreciseCalendarClock().now();
    kj::Duration uptime = current_time - start_time_;
    auto uptime_seconds = uptime / kj::SECONDS;

    // Get engine status
    bool engine_running = bridge_.is_running();
    const auto& metrics = bridge_.metrics();

    builder.put_object("engine", [&](core::JsonBuilder& engine) {
      engine.put("running", engine_running);
      engine.put("uptime_seconds", static_cast<std::int64_t>(uptime_seconds));
      engine.put("orders_processed", static_cast<std::int64_t>(
                                         metrics.orders_submitted.load(std::memory_order_relaxed)));
    });

    // Memory usage
    double memory_mb = get_memory_usage_mb();
    builder.put("memory_mb", memory_mb);

    // Version
    builder.put("version", "1.0.0");

    kj::String json_body = builder.build();
    return ctx.sendJson(200, json_body);
  } catch (const kj::Exception& e) {
    KJ_LOG(ERROR, "Error in detailed health handler", e);
    return ctx.sendError(500, "Internal Server Error"_kj);
  }
}

// ============================================================================
// Private Methods
// ============================================================================

kj::String HealthHandler::format_timestamp(std::int64_t unix_ts) {
  std::time_t time = static_cast<std::time_t>(unix_ts);
  std::tm tm;
  gmtime_r(&time, &tm);

  char buf[32];
  std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &tm);

  return kj::str(buf);
}

double HealthHandler::get_memory_usage_mb() {
  // Read memory usage from /proc/self/status (Linux) or task_info (macOS)
#if defined(__linux__)
  try {
    std::ifstream status("/proc/self/status");
    std::string line;
    while (std::getline(status, line)) {
      if (line.find("VmRSS:") == 0) {
        // VmRSS is in kB
        std::istringstream iss(line);
        std::string label;
        std::int64_t kb;
        iss >> label >> kb;
        return static_cast<double>(kb) / 1024.0; // Convert to MB
      }
    }
  } catch (...) {
    // Ignore errors
  }
  return 0.0;
#elif defined(__APPLE__)
  // On macOS, use task_info to get resident size
  task_vm_info_data_t vm_info;
  mach_msg_type_number_t count = TASK_VM_INFO_COUNT;
  if (task_info(mach_task_self(), TASK_VM_INFO, reinterpret_cast<task_info_t>(&vm_info), &count) ==
      KERN_SUCCESS) {
    return static_cast<double>(vm_info.resident_size) / (1024.0 * 1024.0);
  }
  return 0.0;
#else
  return 0.0;
#endif
}

} // namespace veloz::gateway
