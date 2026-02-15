#include "veloz/core/metrics.h"

#include <iomanip>
#include <kj/common.h>
#include <kj/memory.h>
#include <kj/mutex.h>
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

// Thread-safe global metrics registry using KJ MutexGuarded (no bare pointers)
struct GlobalMetricsState {
  kj::Maybe<kj::Own<MetricsRegistry>> registry{kj::none};
};
static kj::MutexGuarded<GlobalMetricsState> g_metrics_registry;

MetricsRegistry& global_metrics() {
  auto lock = g_metrics_registry.lockExclusive();
  KJ_IF_SOME(registry, lock->registry) {
    return *registry;  // Dereference kj::Own<MetricsRegistry> to get MetricsRegistry reference
  }
  // Create new registry and store it
  auto newRegistry = kj::heap<MetricsRegistry>();

  // Register some system metrics
  newRegistry->register_counter("veloz_system_start_time", "System start time");
  newRegistry->register_gauge("veloz_system_uptime", "System uptime in seconds");
  newRegistry->register_gauge("veloz_event_loop_pending_tasks",
                              "Number of pending tasks in event loop");
  newRegistry->register_histogram("veloz_event_loop_task_latency",
                                  "Event loop task execution latency in seconds");

  MetricsRegistry& ref = *newRegistry;
  lock->registry = kj::mv(newRegistry);
  return ref;
}

} // namespace veloz::core
