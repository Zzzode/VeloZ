#include "veloz/core/metrics.h"

#include <kj/common.h>
#include <kj/memory.h>
#include <kj/mutex.h>
#include <kj/string-tree.h>

namespace veloz::core {

kj::String MetricsRegistry::to_prometheus() const {
  auto lock = guarded_.lockExclusive();

  kj::Vector<kj::StringTree> lines;

  // Export counters
  for (const auto& entry : lock->counters) {
    const auto& name = entry.key;
    const auto& counter = entry.value;
    if (counter->description().size() > 0) {
      lines.add(kj::strTree("# HELP "_kj, name, " "_kj, counter->description(), "\n"_kj));
    }
    lines.add(kj::strTree("# TYPE "_kj, name, " counter\n"_kj));
    lines.add(kj::strTree(name, " "_kj, counter->value(), "\n"_kj));
  }

  // Export gauges
  for (const auto& entry : lock->gauges) {
    const auto& name = entry.key;
    const auto& gauge = entry.value;
    if (gauge->description().size() > 0) {
      lines.add(kj::strTree("# HELP "_kj, name, " "_kj, gauge->description(), "\n"_kj));
    }
    lines.add(kj::strTree("# TYPE "_kj, name, " gauge\n"_kj));
    lines.add(kj::strTree(name, " "_kj, gauge->value(), "\n"_kj));
  }

  // Export histograms
  for (const auto& entry : lock->histograms) {
    const auto& name = entry.key;
    const auto& histogram = entry.value;
    if (histogram->description().size() > 0) {
      lines.add(kj::strTree("# HELP "_kj, name, " "_kj, histogram->description(), "\n"_kj));
    }
    lines.add(kj::strTree("# TYPE "_kj, name, " histogram\n"_kj));
    auto bucket_counts = histogram->bucket_counts();
    for (size_t i = 0; i < histogram->buckets().size(); ++i) {
      lines.add(kj::strTree(name, "_bucket{le=\""_kj, histogram->buckets()[i], "\"} "_kj,
                            bucket_counts[i], "\n"_kj));
    }
    lines.add(kj::strTree(name, "_bucket{le=\"+Inf\"} "_kj, histogram->count(), "\n"_kj));
    lines.add(kj::strTree(name, "_sum "_kj, histogram->sum(), "\n"_kj));
    lines.add(kj::strTree(name, "_count "_kj, histogram->count(), "\n"_kj));
  }

  return kj::StringTree(lines.releaseAsArray(), ""_kj).flatten();
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
  newRegistry->register_counter("veloz_reconnect_count"_kj,
                                "Total number of reconnect attempts"_kj);

  MetricsRegistry& ref = *newRegistry;
  lock->registry = kj::mv(newRegistry);
  return ref;
}

} // namespace veloz::core
