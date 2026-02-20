#include "veloz/core/memory.h"

#include "veloz/core/memory_pool.h"

#include <cstdlib>
#include <kj/common.h>
#include <kj/memory.h>

namespace veloz::core {

AlignedMemory allocateAligned(size_t size, size_t alignment) {
  void* ptr;
#if defined(__POSIX__) || defined(__linux__)
  int result = posix_memalign(&ptr, alignment, size);
  if (result != 0) {
    return AlignedMemory();
  }
#elif defined(_WIN32)
  ptr = _aligned_malloc(size, alignment);
#else
  // Generic implementation
  ptr = malloc(size + alignment + sizeof(void*));
  if (ptr == nullptr) {
    return AlignedMemory();
  }
  void** aligned_ptr =
      reinterpret_cast<void**>((reinterpret_cast<uintptr_t>(ptr) + alignment + sizeof(void*)) &
                               ~(static_cast<uintptr_t>(alignment) - 1));
  aligned_ptr[-1] = ptr;
  ptr = aligned_ptr;
#endif
  return AlignedMemory(ptr, size, alignment);
}

void freeAligned(void* ptr) {
  if (ptr == nullptr) {
    return;
  }
#if defined(__POSIX__) || defined(__linux__)
  free(ptr);
#elif defined(_WIN32)
  _aligned_free(ptr);
#else
  // Generic implementation
  void* real_ptr = reinterpret_cast<void**>(ptr)[-1];
  free(real_ptr);
#endif
}

AlignedMemory::~AlignedMemory() {
  if (ptr) {
    freeAligned(ptr);
  }
}

void AlignedMemory::reset() {
  if (ptr) {
    freeAligned(ptr);
    ptr = nullptr;
    size = 0;
    alignment = 0;
  }
}

const AlignedDisposer AlignedDisposer::instance;

void AlignedDisposer::disposeImpl(void* pointer) const {
  freeAligned(pointer);
}

// Global memory monitor instance using KJ Lazy for thread-safe lazy initialization
static kj::Lazy<MemoryMonitor> g_memory_monitor;

MemoryMonitor& global_memory_monitor() {
  return g_memory_monitor.get(
      [](kj::SpaceFor<MemoryMonitor>& space) { return kj::heap<MemoryMonitor>(); });
}

// Global memory statistics instance using KJ Lazy for thread-safe lazy initialization
static kj::Lazy<MemoryStats> g_memory_stats;

MemoryStats& global_memory_stats() {
  return g_memory_stats.get(
      [](kj::SpaceFor<MemoryStats>& space) { return kj::heap<MemoryStats>(); });
}

} // namespace veloz::core
