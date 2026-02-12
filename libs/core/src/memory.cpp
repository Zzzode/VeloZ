#include "veloz/core/memory_pool.h"

#include <cstdlib>

namespace veloz::core {

void* aligned_alloc(size_t size, size_t alignment) {
  void* ptr;
#if defined(__POSIX__) || defined(__linux__)
  int result = posix_memalign(&ptr, alignment, size);
  if (result != 0) {
    return nullptr;
  }
#elif defined(_WIN32)
  ptr = _aligned_malloc(size, alignment);
#else
  // Generic implementation
  ptr = malloc(size + alignment + sizeof(void*));
  if (ptr == nullptr) {
    return nullptr;
  }
  void** aligned_ptr =
      reinterpret_cast<void**>((reinterpret_cast<uintptr_t>(ptr) + alignment + sizeof(void*)) &
                               ~(static_cast<uintptr_t>(alignment) - 1));
  aligned_ptr[-1] = ptr;
  ptr = aligned_ptr;
#endif
  return ptr;
}

void aligned_free(void* ptr) {
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

// Global memory monitor instance
static MemoryMonitor* g_memory_monitor = nullptr;

MemoryMonitor& global_memory_monitor() {
  if (g_memory_monitor == nullptr) {
    g_memory_monitor = new MemoryMonitor();
  }
  return *g_memory_monitor;
}

} // namespace veloz::core
