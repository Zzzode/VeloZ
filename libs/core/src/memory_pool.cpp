#include "veloz/core/memory_pool.h"

#include <kj/common.h>
#include <kj/memory.h>

namespace veloz::core {

// Global memory monitor instance
static MemoryMonitor* g_memory_monitor = nullptr;

MemoryMonitor& global_memory_monitor() {
  if (g_memory_monitor == nullptr) {
    g_memory_monitor = new MemoryMonitor();
  }
  return *g_memory_monitor;
}

} // namespace veloz::core
