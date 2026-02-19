#include "veloz/core/metrics.h"

#include <iomanip>
#include <kj/common.h>
#include <kj/memory.h>
#include <kj/mutex.h>
#include <sstream>

namespace veloz::core {

kj::String MetricsRegistry::to_prometheus() const {
  auto lock = guarded_.lockExclusive();

  // std::ostringstream used for Prometheus text format generation
  std::ostringstream oss;

  // Export counters
  for (const auto& entry : lock->counters) {
    const auto& name = entry.key;
    const auto& counter = entry.value;
    if (counter->description().size() > 0) {
      oss << "# HELP " << name.cStr() << " " << counter->description().cStr() << "\n";
    }
    oss << "# TYPE " << name.cStr() << " counter\n";
    oss << name.cStr() << " " << counter->value() << "\n";
  }

  // Export gauges
  for (const auto& entry : lock->gauges) {
    const auto& name = entry.key;
    const auto& gauge = entry.value;
    if (gauge->description().size() > 0) {
      oss << "# HELP " << name.cStr() << " " << gauge->description().cStr() << "\n";
    }
    oss << "# TYPE " << name.cStr() << " gauge\n";
    oss << name.cStr() << " " << gauge->value() << "\n";
  }

  // Export histograms
  for (const auto& entry : lock->histograms) {
    const auto& name = entry.key;
    const auto& histogram = entry.value;
    if (histogram->description().size() > 0) {
      oss << "# HELP " << name.cStr() << " " << histogram->description().cStr() << "\n";
    }
    oss << "# TYPE " << name.cStr() << " histogram\n";
    auto bucket_counts = histogram->bucket_counts();
    for (size_t i = 0; i < histogram->buckets().size(); ++i) {
      oss << name.cStr() << "_bucket{le=\"" << histogram->buckets()[i] << "\"} "
          << bucket_counts[i] << "\n";
    }
    oss << name.cStr() << "_bucket{le=\"+Inf\"} " << histogram->count() << "\n";
    oss << name.cStr() << "_sum " << histogram->sum() << "\n";
    oss << name.cStr() << "_count " << histogram->count() << "\n";
  }

  return kj::str(oss.str().c_str());
}

// Thread-safe global metrics registry using KJ MutexGuarded (no bare pointers)
struct GlobalMetricsState {
  kj::Maybe<kj::Own<MetricsRegistry>> registry{kj::none};
};
static kj::MutexGuarded<GlobalMetricsState> g_metrics_registry;

MetricsRegistry& global_metrics() {
  auto lock = g_metrics_registry.lockExclusive();
  KJ_IF_SOME(registry, lock->registry) {
    return *registry; // Dereference kj::Own<MetricsRegistry> to get MetricsRegistry reference
  }
  // Create new registry and store it
  auto newRegistry = kj::heap<MetricsRegistry>();

  // Register some system metrics using kj::StringPtr literals
  newRegistry->register_counter("veloz_system_start_time"_kj, "System start time"_kj);
  newRegistry->register_gauge("veloz_system_uptime"_kj, "System uptime in seconds"_kj);
  newRegistry->register_gauge("veloz_event_loop_pending_tasks"_kj,
                              "Number of pending tasks in event loop"_kj);
  newRegistry->register_histogram("veloz_event_loop_task_latency"_kj,
                                  "Event loop task execution latency in seconds"_kj);

  MetricsRegistry& ref = *newRegistry;
  lock->registry = kj::mv(newRegistry);
  return ref;
}

} // namespace veloz::core
