#include "veloz/core/metrics.h"

#include <iomanip>
#include <kj/common.h>
#include <kj/memory.h>
#include <sstream>

namespace veloz::core {

std::string MetricsRegistry::to_prometheus() const {
  auto lock = guarded_.lockExclusive();

  std::ostringstream oss;

  // Export counters
  for (const auto& [name, counter] : lock->counters) {
    if (!counter->description().empty()) {
      oss << "# HELP " << name << " " << counter->description() << "\n";
    }
    oss << "# TYPE " << name << " counter\n";
    oss << name << " " << counter->value() << "\n";
  }

  // Export gauges
  for (const auto& [name, gauge] : lock->gauges) {
    if (!gauge->description().empty()) {
      oss << "# HELP " << name << " " << gauge->description() << "\n";
    }
    oss << "# TYPE " << name << " gauge\n";
    oss << name << " " << gauge->value() << "\n";
  }

  // Export histograms
  for (const auto& [name, histogram] : lock->histograms) {
    if (!histogram->description().empty()) {
      oss << "# HELP " << name << " " << histogram->description() << "\n";
    }
    oss << "# TYPE " << name << " histogram\n";
    auto bucket_counts = histogram->bucket_counts();
    for (size_t i = 0; i < histogram->buckets().size(); ++i) {
      oss << name << "_bucket{le=\"" << histogram->buckets()[i] << "\"} " << bucket_counts[i]
          << "\n";
    }
    oss << name << "_bucket{le=\"+Inf\"} " << histogram->count() << "\n";
    oss << name << "_sum " << histogram->sum() << "\n";
    oss << name << "_count " << histogram->count() << "\n";
  }

  return oss.str();
}

// Global metrics registry instance
static MetricsRegistry* g_metrics_registry = nullptr;

MetricsRegistry& global_metrics() {
  if (g_metrics_registry == nullptr) {
    g_metrics_registry = new MetricsRegistry();

    // Register some system metrics
    g_metrics_registry->register_counter("veloz_system_start_time", "System start time");
    g_metrics_registry->register_gauge("veloz_system_uptime", "System uptime in seconds");
    g_metrics_registry->register_gauge("veloz_event_loop_pending_tasks",
                                       "Number of pending tasks in event loop");
    g_metrics_registry->register_histogram("veloz_event_loop_task_latency",
                                           "Event loop task execution latency in seconds");
  }
  return *g_metrics_registry;
}

} // namespace veloz::core
