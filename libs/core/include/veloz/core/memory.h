#pragma once

#include <atomic>
#include <cstddef>
#include <deque>
#include <functional> // std::function used for custom deleter in std::unique_ptr
#include <kj/arena.h>
#include <kj/common.h>
#include <kj/debug.h> // KJ_FAIL_REQUIRE for exception handling
#include <kj/function.h>
#include <kj/memory.h>
#include <kj/mutex.h>
#include <memory> // std::unique_ptr used for custom deleter support in acquire()
#include <vector> // std::vector used for internal pool storage

namespace veloz::core {

// =======================================================================================
// KJ Memory Utilities (RECOMMENDED - use these by default)
// =======================================================================================

/**
 * @brief Create a heap-allocated object using KJ's memory management
 * @tparam T Type to allocate
 * @tparam Args Constructor argument types
 * @param args Constructor arguments
 * @return kj::Own<T> owning pointer to the allocated object
 *
 * This is the RECOMMENDED way to allocate heap objects in VeloZ.
 * Use this instead of std::make_unique or new/delete.
 *
 * Example:
 * @code
 * auto obj = makeOwn<MyClass>(arg1, arg2);
 * // obj is automatically freed when it goes out of scope
 * @endcode
 */
template <typename T, typename... Args> [[nodiscard]] kj::Own<T> makeOwn(Args&&... args) {
  return kj::heap<T>(kj::fwd<Args>(args)...);
}

/**
 * @brief Create a heap-allocated array using KJ's memory management
 * @tparam T Element type
 * @param size Number of elements
 * @return kj::Array<T> owning array
 *
 * This is the RECOMMENDED way to allocate arrays in VeloZ.
 * Use this instead of std::vector for fixed-size arrays or new[]/delete[].
 *
 * Example:
 * @code
 * auto arr = makeArray<int>(100);
 * // arr is automatically freed when it goes out of scope
 * @endcode
 */
template <typename T> [[nodiscard]] kj::Array<T> makeArray(size_t size) {
  return kj::heapArray<T>(size);
}

/**
 * @brief Create a heap-allocated array with initial values
 * @tparam T Element type
 * @param values Initializer list of values
 * @return kj::Array<T> owning array
 */
template <typename T> [[nodiscard]] kj::Array<T> makeArray(std::initializer_list<T> values) {
  auto arr = kj::heapArray<T>(values.size());
  size_t i = 0;
  for (const auto& v : values) {
    arr[i++] = v;
  }
  return arr;
}

/**
 * @brief Wrap a raw pointer in a kj::Own without taking ownership
 * @tparam T Type of the pointed-to object
 * @param ptr Raw pointer (must outlive the returned Own)
 * @return kj::Own<T> that does NOT free the pointer
 *
 * Use this when you need to pass a non-owned pointer to an API that expects Own<T>.
 * WARNING: The pointed-to object must outlive the returned Own.
 */
template <typename T> [[nodiscard]] kj::Own<T> wrapNonOwning(T* ptr) {
  return kj::Own<T>(ptr, kj::NullDisposer::instance);
}

// =======================================================================================
// VeloZArena - Arena-based memory allocator wrapper around kj::Arena
// =======================================================================================

/**
 * @brief Arena-based memory allocator for fast temporary allocations
 *
 * VeloZArena wraps kj::Arena to provide fast allocation with bulk deallocation.
 * All objects allocated from an arena are freed together when the arena is
 * destroyed or reset. This is ideal for:
 * - Per-request/per-event allocations
 * - Temporary objects with similar lifetimes
 * - Avoiding memory fragmentation in hot paths
 *
 * Example:
 * @code
 * VeloZArena arena;
 * auto* event = arena.allocate<MarketEvent>(symbol, price, qty);
 * auto* order = arena.allocate<Order>(orderId, side, price);
 * // All allocations freed when arena goes out of scope or reset() is called
 * @endcode
 */
class VeloZArena final {
public:
  /**
   * @brief Construct an arena with default chunk size
   */
  VeloZArena() : arena_() {}

  /**
   * @brief Construct an arena with specified initial chunk size
   * @param chunk_size_hint Hint for initial chunk allocation size in bytes
   */
  explicit VeloZArena(size_t chunk_size_hint) : arena_(chunk_size_hint) {}

  ~VeloZArena() = default;

  // Non-copyable, non-movable (arena memory is not relocatable)
  VeloZArena(const VeloZArena&) = delete;
  VeloZArena& operator=(const VeloZArena&) = delete;
  VeloZArena(VeloZArena&&) = delete;
  VeloZArena& operator=(VeloZArena&&) = delete;

  /**
   * @brief Allocate and construct an object in the arena
   * @tparam T Type to allocate
   * @tparam Args Constructor argument types
   * @param args Constructor arguments
   * @return Pointer to the constructed object (valid until arena reset/destruction)
   *
   * The returned pointer is NOT owned - the arena manages the memory.
   * Do NOT delete the returned pointer.
   */
  template <typename T, typename... Args> [[nodiscard]] T* allocate(Args&&... args) {
    return &arena_.allocate<T>(kj::fwd<Args>(args)...);
  }

  /**
   * @brief Allocate an array of objects in the arena
   * @tparam T Element type
   * @param count Number of elements
   * @return ArrayPtr to the allocated array (valid until arena reset/destruction)
   */
  template <typename T> [[nodiscard]] kj::ArrayPtr<T> allocateArray(size_t count) {
    return arena_.allocateArray<T>(count);
  }

  /**
   * @brief Allocate raw bytes in the arena
   * @param size Number of bytes to allocate
   * @param alignment Alignment requirement (default: max alignment)
   * @return Pointer to allocated memory
   */
  [[nodiscard]] void* allocateBytes(size_t size, size_t alignment = alignof(std::max_align_t)) {
    // kj::Arena doesn't have a direct allocateBytes, use allocateArray<byte>
    auto arr = arena_.allocateArray<kj::byte>(size);
    return arr.begin();
  }

  /**
   * @brief Copy a string into the arena
   * @param str String to copy
   * @return StringPtr to the arena-allocated copy
   */
  [[nodiscard]] kj::StringPtr copyString(kj::StringPtr str) {
    return arena_.copyString(str);
  }

  /**
   * @brief Get the underlying kj::Arena for advanced usage
   * @return Reference to the underlying arena
   */
  [[nodiscard]] kj::Arena& underlying() noexcept {
    return arena_;
  }

  [[nodiscard]] const kj::Arena& underlying() const noexcept {
    return arena_;
  }

private:
  kj::Arena arena_;
};

/**
 * @brief Thread-safe arena wrapper for concurrent allocations
 *
 * Wraps VeloZArena with mutex protection for thread-safe access.
 * Use this when multiple threads need to allocate from the same arena.
 * For single-threaded hot paths, prefer plain VeloZArena.
 */
class ThreadSafeArena final {
public:
  ThreadSafeArena() : guarded_() {}
  explicit ThreadSafeArena(size_t chunk_size_hint) : guarded_(chunk_size_hint) {}

  ~ThreadSafeArena() = default;

  ThreadSafeArena(const ThreadSafeArena&) = delete;
  ThreadSafeArena& operator=(const ThreadSafeArena&) = delete;

  /**
   * @brief Allocate and construct an object in the arena (thread-safe)
   */
  template <typename T, typename... Args> [[nodiscard]] T* allocate(Args&&... args) {
    auto lock = guarded_.lockExclusive();
    return lock->allocate<T>(kj::fwd<Args>(args)...);
  }

  /**
   * @brief Allocate an array in the arena (thread-safe)
   */
  template <typename T> [[nodiscard]] kj::ArrayPtr<T> allocateArray(size_t count) {
    auto lock = guarded_.lockExclusive();
    return lock->allocateArray<T>(count);
  }

  /**
   * @brief Copy a string into the arena (thread-safe)
   */
  [[nodiscard]] kj::StringPtr copyString(kj::StringPtr str) {
    auto lock = guarded_.lockExclusive();
    return lock->copyString(str);
  }

private:
  kj::MutexGuarded<VeloZArena> guarded_;
};

// Memory alignment utilities
[[nodiscard]] void* aligned_alloc(size_t size, size_t alignment);
void aligned_free(void* ptr);

template <typename T> [[nodiscard]] T* aligned_new(size_t alignment = alignof(T)) {
  void* ptr = aligned_alloc(sizeof(T), alignment);
  KJ_REQUIRE(ptr != nullptr, "aligned_alloc failed: out of memory", sizeof(T), alignment);
  return new (ptr) T();
}

template <typename T, typename... Args>
[[nodiscard]] T* aligned_new(size_t alignment, Args&&... args) {
  void* ptr = aligned_alloc(sizeof(T), alignment);
  KJ_REQUIRE(ptr != nullptr, "aligned_alloc failed: out of memory", sizeof(T), alignment);
  return new (ptr) T(std::forward<Args>(args)...);
}

template <typename T> void aligned_delete(T* ptr) {
  if (ptr == nullptr) {
    return;
  }
  ptr->~T();
  aligned_free(ptr);
}

// Simple object pool implementation using KJ synchronization
template <typename T> class ObjectPool final {
public:
  explicit ObjectPool(size_t initial_size = 0, size_t max_size = 0)
      : guarded_(PoolState(initial_size)), max_size_(max_size) {}

  ~ObjectPool() = default;

  // Acquire object from pool
  // Uses std::unique_ptr with custom deleter since kj::Own doesn't support release() or custom deleters
  template <typename... Args>
  std::unique_ptr<T, std::function<void(T*)>> acquire(Args&&... args) {
    auto lock = guarded_.lockExclusive();
    if (!lock->pool.empty()) {
      // Release ownership from the pool's unique_ptr and take the raw pointer
      T* ptr = lock->pool.back().release();
      lock->pool.pop_back();
      // Reset object state by destroying and reconstructing in place
      ptr->~T();
      new (ptr) T(std::forward<Args>(args)...);
      lock.release(); // Release lock before returning
      return std::unique_ptr<T, std::function<void(T*)>>(ptr, [this](T* p) { release(p); });
    }

    if (max_size_ == 0 || lock->size < max_size_) {
      auto ptr = new T(std::forward<Args>(args)...);
      ++(lock->size);
      lock.release(); // Release lock before returning
      return std::unique_ptr<T, std::function<void(T*)>>(ptr, [this](T* p) { release(p); });
    }

    KJ_FAIL_REQUIRE("Object pool exhausted", max_size_);
  }

  // Release object back to pool
  void release(T* ptr) {
    if (ptr == nullptr) {
      return;
    }

    auto lock = guarded_.lockExclusive();
    if (max_size_ > 0 && lock->pool.size() >= max_size_) {
      delete ptr;
      --(lock->size);
    } else {
      lock->pool.push_back(std::unique_ptr<T>(ptr));
    }
  }

  // Get pool status information
  [[nodiscard]] size_t available() const {
    return guarded_.lockExclusive()->pool.size();
  }

  [[nodiscard]] size_t size() const {
    return guarded_.lockExclusive()->size;
  }

  [[nodiscard]] size_t max_size() const noexcept {
    return max_size_;
  }

  // Preallocate objects
  void preallocate(size_t count) {
    auto lock = guarded_.lockExclusive();
    const size_t needed = count > lock->pool.size() ? count - lock->pool.size() : 0;
    for (size_t i = 0; i < needed; ++i) {
      if (max_size_ > 0 && lock->size >= max_size_) {
        break;
      }
      lock->pool.push_back(std::make_unique<T>());
      ++(lock->size);
    }
  }

  // Clear pool
  void clear() {
    auto lock = guarded_.lockExclusive();
    lock->pool.clear();
    lock->size = 0;
  }

private:
  // Internal state for the pool
  // Uses std::unique_ptr because kj::Own doesn't support release() method
  struct PoolState {
    std::vector<std::unique_ptr<T>> pool;
    size_t size;

    explicit PoolState(size_t initial_size) : size(initial_size) {
      pool.reserve(initial_size);
      for (size_t i = 0; i < initial_size; ++i) {
        pool.push_back(std::make_unique<T>());
      }
    }
  };

  kj::MutexGuarded<PoolState> guarded_;
  const size_t max_size_;
};

// Thread-local object pool (no synchronization needed)
// Note: Uses std::unique_ptr because kj::Own doesn't support custom deleters or release()
template <typename T> class ThreadLocalObjectPool final {
public:
  explicit ThreadLocalObjectPool(size_t initial_size = 0, size_t max_size = 0)
      : initial_size_(initial_size), max_size_(max_size) {}

  ~ThreadLocalObjectPool() = default;

  // Acquire object from pool
  // Returns std::unique_ptr with custom deleter (kj::Own doesn't support custom deleters)
  template <typename... Args>
  std::unique_ptr<T, std::function<void(T*)>> acquire(Args&&... args) {
    auto& pool = pool_;

    if (!pool.empty()) {
      auto ptr = pool.back().release();
      pool.pop_back();
      ptr->~T();
      new (ptr) T(std::forward<Args>(args)...);
      return std::unique_ptr<T, std::function<void(T*)>>(ptr, [this](T* p) { release(p); });
    }

    if (max_size_ == 0 || pool.size() < max_size_) {
      auto ptr = new T(std::forward<Args>(args)...);
      return std::unique_ptr<T, std::function<void(T*)>>(ptr, [this](T* p) { release(p); });
    }

    KJ_FAIL_REQUIRE("Thread local object pool exhausted", max_size_);
  }

  // Release object back to pool
  void release(T* ptr) {
    if (ptr == nullptr) {
      return;
    }

    auto& pool = pool_;
    if (max_size_ > 0 && pool.size() >= max_size_) {
      delete ptr;
    } else {
      pool.push_back(std::unique_ptr<T>(ptr));
    }
  }

private:
  // std::unique_ptr used because kj::Own doesn't support release() method
  static thread_local std::vector<std::unique_ptr<T>> pool_;
  const size_t initial_size_;
  const size_t max_size_;
};

template <typename T> thread_local std::vector<std::unique_ptr<T>> ThreadLocalObjectPool<T>::pool_;

// Memory usage statistics
class MemoryStats final {
public:
  MemoryStats() = default;

  void allocate(size_t size) {
    total_allocated_ += size;
    size_t current = total_allocated_.load();
    size_t peak = peak_allocated_.load();
    while (current > peak) {
      if (peak_allocated_.compare_exchange_weak(peak, current)) {
        break;
      }
    }
    ++allocation_count_;
  }

  void deallocate(size_t size) {
    total_allocated_ -= size;
    ++deallocation_count_;
  }

  [[nodiscard]] size_t total_allocated() const noexcept {
    return total_allocated_;
  }
  [[nodiscard]] size_t peak_allocated() const noexcept {
    return peak_allocated_;
  }
  [[nodiscard]] uint64_t allocation_count() const noexcept {
    return allocation_count_;
  }
  [[nodiscard]] uint64_t deallocation_count() const noexcept {
    return deallocation_count_;
  }

  void reset() {
    total_allocated_ = 0;
    peak_allocated_ = 0;
    allocation_count_ = 0;
    deallocation_count_ = 0;
  }

private:
  std::atomic<size_t> total_allocated_{0};
  std::atomic<size_t> peak_allocated_{0};
  std::atomic<uint64_t> allocation_count_{0};
  std::atomic<uint64_t> deallocation_count_{0};
};

// Global memory statistics
[[nodiscard]] MemoryStats& global_memory_stats();

} // namespace veloz::core
