#include "veloz/core/metrics.h"

#include <sstream>

namespace veloz::core {

std::string MetricsRegistry::to_prometheus() const {
  std::ostringstream oss;

  // 导出计数器
  for (const auto& [name, counter] : counters_) {
    if (!counter->description().empty()) {
      oss << "# HELP " << name << " " << counter->description() << "\n";
    }
    oss << "# TYPE " << name << " counter\n";
    oss << name << " " << counter->value() << "\n";
  }

  // 导出仪表盘
  for (const auto& [name, gauge] : gauges_) {
    if (!gauge->description().empty()) {
      oss << "# HELP " << name << " " << gauge->description() << "\n";
    }
    oss << "# TYPE " << name << " gauge\n";
    oss << name << " " << gauge->value() << "\n";
  }

  // 导出直方图
  for (const auto& [name, histogram] : histograms_) {
    if (!histogram->description().empty()) {
      oss << "# HELP " << name << " " << histogram->description() << "\n";
    }
    oss << "# TYPE " << name << " histogram\n";

    auto bucket_counts = histogram->bucket_counts();
    for (size_t i = 0; i < histogram->buckets().size(); ++i) {
      oss << name << "_bucket{le=\"" << histogram->buckets()[i] << "\"} " << bucket_counts[i] << "\n";
    }
    oss << name << "_bucket{le=\"+Inf\"} " << histogram->count() << "\n";
    oss << name << "_sum " << histogram->sum() << "\n";
    oss << name << "_count " << histogram->count() << "\n";
  }

  return oss.str();
}

// 全局指标注册表实例
static MetricsRegistry* g_metrics_registry = nullptr;

MetricsRegistry& global_metrics() {
  if (g_metrics_registry == nullptr) {
    g_metrics_registry = new MetricsRegistry();

    // 注册一些系统指标
    g_metrics_registry->register_counter("veloz_system_start_time", "System start time");
    g_metrics_registry->register_gauge("veloz_system_uptime", "System uptime in seconds");
    g_metrics_registry->register_gauge("veloz_event_loop_pending_tasks", "Number of pending tasks in event loop");
    g_metrics_registry->register_histogram("veloz_event_loop_task_latency", "Event loop task execution latency in seconds");
  }
  return *g_metrics_registry;
}

} // namespace veloz::core
