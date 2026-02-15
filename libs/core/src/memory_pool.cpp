#include "veloz/core/memory_pool.h"

#include <kj/common.h>
#include <kj/memory.h>
#include <kj/mutex.h>

namespace veloz::core {

// Thread-safe global memory monitor using KJ MutexGuarded (no bare pointers)
struct GlobalMemoryMonitorState {
  kj::Maybe<kj::Own<MemoryMonitor>> monitor{kj::none};
};
static kj::MutexGuarded<GlobalMemoryMonitorState> g_memory_monitor;

MemoryMonitor& global_memory_monitor() {
  auto lock = g_memory_monitor.lockExclusive();
  KJ_IF_SOME(monitor, lock->monitor) {
    return *monitor;  // Dereference kj::Own<MemoryMonitor> to get MemoryMonitor reference
  }
  // Create new monitor and store it
  auto newMonitor = kj::heap<MemoryMonitor>();
  MemoryMonitor& ref = *newMonitor;
  lock->monitor = kj::mv(newMonitor);
  return ref;
}

} // namespace veloz::core
