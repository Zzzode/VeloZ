#pragma once

#include <atomic>
#include <cstddef>
#include <deque>
#include <kj/arena.h>
#include <kj/common.h>
#include <kj/debug.h> // KJ_FAIL_REQUIRE for exception handling
#include <kj/function.h>
#include <kj/memory.h>
#include <kj/mutex.h>
#include <kj/vector.h>

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
 * auto& event = arena.allocate<MarketEvent>(symbol, price, qty);
 * auto& order = arena.allocate<Order>(orderId, side, price);
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
  template <typename T, typename... Args> [[nodiscard]] T& allocate(Args&&... args) {
    return arena_.allocate<T>(kj::fwd<Args>(args)...);
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
   * @return ArrayPtr to allocated memory
   */
  [[nodiscard]] kj::ArrayPtr<kj::byte> allocateBytes(size_t size, size_t alignment = alignof(std::max_align_t)) {
    // kj::Arena doesn't have a direct allocateBytes, use allocateArray<byte>
    return arena_.allocateArray<kj::byte>(size);
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
  template <typename T, typename... Args> [[nodiscard]] T& allocate(Args&&... args) {
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

// =======================================================================================
// ResettableArenaPool - Arena pool with reset/reuse for batch processing
// =======================================================================================

/**
 * @brief Arena pool that supports reset/reuse for batch processing
 *
 * Since kj::Arena doesn't support reset, this class manages a pool of arenas
 * that can be recycled. When an arena is "reset", it's destroyed and a new
 * one is created. This is efficient for batch processing patterns where
 * all allocations in a batch have the same lifetime.
 *
 * Usage pattern:
 * @code
 * ResettableArenaPool pool(4096);  // 4KB chunk size
 *
 * // Batch 1
 * auto& event1 = pool.allocate<MarketEvent>(...);
 * auto& event2 = pool.allocate<MarketEvent>(...);
 * // Process batch...
 * pool.reset();  // Free all allocations
 *
 * // Batch 2
 * auto& event3 = pool.allocate<MarketEvent>(...);
 * // ...
 * @endcode
 */
class ResettableArenaPool final {
public:
  /**
   * @brief Construct a resettable arena pool
   * @param chunk_size_hint Hint for arena chunk allocation size in bytes
   */
  explicit ResettableArenaPool(size_t chunk_size_hint = 4096)
      : chunk_size_hint_(chunk_size_hint), arena_(kj::heap<VeloZArena>(chunk_size_hint)),
        allocation_count_(0), total_allocated_bytes_(0) {}

  ~ResettableArenaPool() = default;

  // Non-copyable, non-movable
  ResettableArenaPool(const ResettableArenaPool&) = delete;
  ResettableArenaPool& operator=(const ResettableArenaPool&) = delete;
  ResettableArenaPool(ResettableArenaPool&&) = delete;
  ResettableArenaPool& operator=(ResettableArenaPool&&) = delete;

  /**
   * @brief Allocate and construct an object in the arena
   * @tparam T Type to allocate
   * @tparam Args Constructor argument types
   * @param args Constructor arguments
   * @return Reference to the constructed object (valid until reset() is called)
   */
  template <typename T, typename... Args> [[nodiscard]] T& allocate(Args&&... args) {
    ++allocation_count_;
    total_allocated_bytes_ += sizeof(T);
    return arena_->allocate<T>(kj::fwd<Args>(args)...);
  }

  /**
   * @brief Allocate an array of objects in the arena
   * @tparam T Element type
   * @param count Number of elements
   * @return ArrayPtr to the allocated array (valid until reset() is called)
   */
  template <typename T> [[nodiscard]] kj::ArrayPtr<T> allocateArray(size_t count) {
    ++allocation_count_;
    total_allocated_bytes_ += sizeof(T) * count;
    return arena_->allocateArray<T>(count);
  }

  /**
   * @brief Copy a string into the arena
   * @param str String to copy
   * @return StringPtr to the arena-allocated copy (valid until reset() is called)
   */
  [[nodiscard]] kj::StringPtr copyString(kj::StringPtr str) {
    ++allocation_count_;
    total_allocated_bytes_ += str.size() + 1;
    return arena_->copyString(str);
  }

  /**
   * @brief Reset the arena, freeing all allocations
   *
   * All pointers returned by allocate() become invalid after this call.
   * A new arena is created for subsequent allocations.
   */
  void reset() {
    arena_ = kj::heap<VeloZArena>(chunk_size_hint_);
    allocation_count_ = 0;
    total_allocated_bytes_ = 0;
  }

  /**
   * @brief Get the number of allocations since last reset
   */
  [[nodiscard]] size_t allocationCount() const noexcept {
    return allocation_count_;
  }

  /**
   * @brief Get the total bytes allocated since last reset (approximate)
   */
  [[nodiscard]] size_t totalAllocatedBytes() const noexcept {
    return total_allocated_bytes_;
  }

  /**
   * @brief Get the underlying arena for advanced usage
   */
  [[nodiscard]] VeloZArena& underlying() noexcept {
    return *arena_;
  }

private:
  size_t chunk_size_hint_;
  kj::Own<VeloZArena> arena_;
  size_t allocation_count_;
  size_t total_allocated_bytes_;
};

/**
 * @brief Thread-safe resettable arena pool
 *
 * Wraps ResettableArenaPool with mutex protection for thread-safe access.
 */
class ThreadSafeResettableArenaPool final {
public:
  explicit ThreadSafeResettableArenaPool(size_t chunk_size_hint = 4096)
      : guarded_(chunk_size_hint) {}

  ~ThreadSafeResettableArenaPool() = default;

  ThreadSafeResettableArenaPool(const ThreadSafeResettableArenaPool&) = delete;
  ThreadSafeResettableArenaPool& operator=(const ThreadSafeResettableArenaPool&) = delete;

  /**
   * @brief Allocate and construct an object (thread-safe)
   */
  template <typename T, typename... Args> [[nodiscard]] T& allocate(Args&&... args) {
    auto lock = guarded_.lockExclusive();
    return lock->allocate<T>(kj::fwd<Args>(args)...);
  }

  /**
   * @brief Allocate an array (thread-safe)
   */
  template <typename T> [[nodiscard]] kj::ArrayPtr<T> allocateArray(size_t count) {
    auto lock = guarded_.lockExclusive();
    return lock->allocateArray<T>(count);
  }

  /**
   * @brief Copy a string (thread-safe)
   */
  [[nodiscard]] kj::StringPtr copyString(kj::StringPtr str) {
    auto lock = guarded_.lockExclusive();
    return lock->copyString(str);
  }

  /**
   * @brief Reset the arena (thread-safe)
   *
   * All references returned by allocate() become invalid after this call.
   * This clears all allocated memory but keeps the arenas for reuse.
   */
  void reset() {
    auto lock = guarded_.lockExclusive();
    lock->reset();
  }

  /**
   * @brief Get allocation count (thread-safe)
   */
  [[nodiscard]] size_t allocationCount() const {
    auto lock = guarded_.lockExclusive();
    return lock->allocationCount();
  }

  /**
   * @brief Get total allocated bytes (thread-safe)
   */
  [[nodiscard]] size_t totalAllocatedBytes() const {
    auto lock = guarded_.lockExclusive();
    return lock->totalAllocatedBytes();
  }

private:
  kj::MutexGuarded<ResettableArenaPool> guarded_;
};

// Memory alignment utilities

/**
 * @brief RAII wrapper for raw aligned memory allocation.
 *
 * This struct ensures that memory allocated by allocateAligned() is properly freed
 * if not transferred to another owner. It replaces raw void* returns.
 */
struct AlignedMemory {
  void* ptr = nullptr;
  size_t size = 0;
  size_t alignment = 0;

  AlignedMemory() = default;
  AlignedMemory(void* p, size_t s, size_t a) : ptr(p), size(s), alignment(a) {}
  
  ~AlignedMemory(); // Defined in .cpp to avoid inline dependency on freeAligned
  
  AlignedMemory(AlignedMemory&& other) noexcept 
      : ptr(other.ptr), size(other.size), alignment(other.alignment) {
    other.ptr = nullptr;
    other.size = 0;
    other.alignment = 0;
  }
  
  AlignedMemory& operator=(AlignedMemory&& other) noexcept {
    if (this != &other) {
      reset();
      ptr = other.ptr;
      size = other.size;
      alignment = other.alignment;
      other.ptr = nullptr;
      other.size = 0;
      other.alignment = 0;
    }
    return *this;
  }

  // No copy
  AlignedMemory(const AlignedMemory&) = delete;
  AlignedMemory& operator=(const AlignedMemory&) = delete;

  void* release() {
    void* p = ptr;
    ptr = nullptr;
    size = 0;
    alignment = 0;
    return p;
  }
  
  void reset(); // Defined in .cpp
  
  bool isValid() const { return ptr != nullptr; }
};

[[nodiscard]] AlignedMemory allocateAligned(size_t size, size_t alignment);
void freeAligned(void* ptr);

class AlignedDisposer : public kj::Disposer {
public:
  void disposeImpl(void* pointer) const override;
  static const AlignedDisposer instance;
};

template <typename T> [[nodiscard]] kj::Own<T> aligned_new(size_t alignment = alignof(T)) {
  AlignedMemory mem = allocateAligned(sizeof(T), alignment);
  KJ_REQUIRE(mem.isValid(), "allocateAligned failed: out of memory", sizeof(T), alignment);
  
  void* ptr = mem.release(); // Transfer ownership to kj::Own via AlignedDisposer
  auto obj = new (ptr) T();
  return kj::Own<T>(obj, AlignedDisposer::instance);
}

template <typename T, typename... Args>
[[nodiscard]] kj::Own<T> aligned_new(size_t alignment, Args&&... args) {
  AlignedMemory mem = allocateAligned(sizeof(T), alignment);
  KJ_REQUIRE(mem.isValid(), "allocateAligned failed: out of memory", sizeof(T), alignment);
  
  void* ptr = mem.release(); // Transfer ownership to kj::Own via AlignedDisposer
  auto obj = new (ptr) T(kj::fwd<Args>(args)...);
  return kj::Own<T>(obj, AlignedDisposer::instance);
}

// Forward declaration for PooledObject
template <typename T> class ObjectPool;

template <typename T> class ThreadLocalObjectPool;

// =======================================================================================
// PooledObject - RAII wrapper for pooled objects (KJ-based alternative to std::unique_ptr
// with custom deleter)
// =======================================================================================

/**
 * @brief RAII wrapper for objects acquired from ObjectPool or ThreadLocalObjectPool
 * @tparam T Type of the pooled object
 *
 * This class provides unique_ptr-like semantics for pooled objects. When the PooledObject
 * is destroyed, the underlying object is automatically returned to the pool instead of
 * being deleted.
 *
 * This wrapper exists because kj::Own does not support:
 * 1. Custom deleters (needed to return objects to pool)
 * 2. The release() method (needed for pool management)
 *
 * Example:
 * @code
 * ObjectPool<MyClass> pool(10, 100);
 * {
 *   auto obj = pool.acquire(arg1, arg2);
 *   obj->doSomething();
 * } // Object automatically returned to pool here
 * @endcode
 */
template <typename T> class PooledObject final {
public:
  /**
   * @brief Construct a PooledObject with ownership of a raw pointer
   * @param ptr Raw pointer to the pooled object
   * @param releaseFn Function to call when releasing the object back to pool
   */
  PooledObject(T* ptr, kj::Function<void(T*)> releaseFn)
      : ptr_(ptr), releaseFn_(kj::mv(releaseFn)) {}

  /**
   * @brief Destructor - returns object to pool via release function
   */
  ~PooledObject() noexcept {
    if (ptr_ != nullptr) {
      releaseFn_(ptr_);
    }
  }

  // Move-only semantics (like kj::Own)
  PooledObject(PooledObject&& other) noexcept
      : ptr_(other.ptr_), releaseFn_(kj::mv(other.releaseFn_)) {
    other.ptr_ = nullptr;
  }

  PooledObject& operator=(PooledObject&& other) noexcept {
    if (this != &other) {
      if (ptr_ != nullptr) {
        releaseFn_(ptr_);
      }
      ptr_ = other.ptr_;
      releaseFn_ = kj::mv(other.releaseFn_);
      other.ptr_ = nullptr;
    }
    return *this;
  }

  // Non-copyable
  PooledObject(const PooledObject&) = delete;
  PooledObject& operator=(const PooledObject&) = delete;

  // Pointer-like interface
  [[nodiscard]] T* operator->() noexcept {
    return ptr_;
  }
  [[nodiscard]] const T* operator->() const noexcept {
    return ptr_;
  }
  [[nodiscard]] T& operator*() noexcept {
    return *ptr_;
  }
  [[nodiscard]] const T& operator*() const noexcept {
    return *ptr_;
  }
  [[nodiscard]] T* get() noexcept {
    return ptr_;
  }
  [[nodiscard]] const T* get() const noexcept {
    return ptr_;
  }

  // Boolean conversion for null checks
  explicit operator bool() const noexcept {
    return ptr_ != nullptr;
  }

  // Comparison with nullptr
  bool operator==(std::nullptr_t) const noexcept {
    return ptr_ == nullptr;
  }
  bool operator!=(std::nullptr_t) const noexcept {
    return ptr_ != nullptr;
  }

private:
  T* ptr_;
  kj::Function<void(T*)> releaseFn_;
};

// =======================================================================================
// ObjectPool - Thread-safe object pool using KJ synchronization
// =======================================================================================

/**
 * @brief Thread-safe object pool for reusing heap-allocated objects
 * @tparam T Type of objects to pool
 *
 * ObjectPool maintains a pool of pre-allocated objects that can be acquired and released.
 * This reduces allocation {}overhead for frequently created/destroyed objects.
 *
 * Thread safety is provided via kj::MutexGuarded.
 *
 * Example:
 * @code
 * ObjectPool<Connection> pool(10, 100);  // Initial 10, max 100
 * {
 *   auto conn = pool.acquire(host, port);
 *   conn->send(data);
 * } // Connection returned to pool
 * @endcode
 */
template <typename T> class ObjectPool final {
public:
  explicit ObjectPool(size_t initial_size = 0, size_t max_size = 0)
      : guarded_(PoolState(initial_size)), max_size_(max_size) {}

  ~ObjectPool() = default;

  // Non-copyable, non-movable
  ObjectPool(const ObjectPool&) = delete;
  ObjectPool& operator=(const ObjectPool&) = delete;
  ObjectPool(ObjectPool&&) = delete;
  ObjectPool& operator=(ObjectPool&&) = delete;

  /**
   * @brief Acquire an object from the pool
   * @tparam Args Constructor argument types
   * @param args Constructor arguments (used to reinitialize recycled objects)
   * @return PooledObject<T> RAII wrapper that returns object to pool on destruction
   * @throws kj::Exception if pool is exhausted and at max_size
   */
  template <typename... Args> [[nodiscard]] PooledObject<T> acquire(Args&&... args) {
    auto lock = guarded_.lockExclusive();
    if (lock->pool.size() > 0) {
      // Take object from pool
      T* ptr = lock->pool.back();
      lock->pool.removeLast();
      // Reset object state by destroying and reconstructing in place
      ptr->~T();
      new (ptr) T(kj::fwd<Args>(args)...);
      lock.release(); // Release lock before returning
      return PooledObject<T>(ptr, [this](T* p) { release(p); });
    }

    if (max_size_ == 0 || lock->size < max_size_) {
      auto ptr = new T(kj::fwd<Args>(args)...);
      ++(lock->size);
      lock.release(); // Release lock before returning
      return PooledObject<T>(ptr, [this](T* p) { release(p); });
    }

    KJ_FAIL_REQUIRE("Object pool exhausted", max_size_);
  }

  /**
   * @brief Release an object back to the pool
   * @param ptr Raw pointer to the object to release
   *
   * This is called automatically by PooledObject destructor.
   * Can also be called manually if needed.
   */
  void release(T* ptr) {
    if (ptr == nullptr) {
      return;
    }

    auto lock = guarded_.lockExclusive();
    if (max_size_ > 0 && lock->pool.size() >= max_size_) {
      delete ptr;
      --(lock->size);
    } else {
      lock->pool.add(ptr);
    }
  }

  /**
   * @brief Get number of available objects in the pool
   */
  [[nodiscard]] size_t available() const {
    return guarded_.lockExclusive()->pool.size();
  }

  /**
   * @brief Get total number of objects managed by the pool
   */
  [[nodiscard]] size_t size() const {
    return guarded_.lockExclusive()->size;
  }

  /**
   * @brief Get maximum pool size (0 = unlimited)
   */
  [[nodiscard]] size_t max_size() const noexcept {
    return max_size_;
  }

  /**
   * @brief Preallocate objects to the pool
   * @param count Number of objects to preallocate
   */
  void preallocate(size_t count) {
    auto lock = guarded_.lockExclusive();
    const size_t current_size = lock->pool.size();
    const size_t needed = count > current_size ? count - current_size : 0;
    for (size_t i = 0; i < needed; ++i) {
      if (max_size_ > 0 && lock->size >= max_size_) {
        break;
      }
      lock->pool.add(new T());
      ++(lock->size);
    }
  }

  /**
   * @brief Clear all objects from the pool
   */
  void clear() {
    auto lock = guarded_.lockExclusive();
    for (size_t i = 0; i < lock->pool.size(); ++i) {
      delete lock->pool[i];
    }
    lock->pool.clear();
    lock->size = 0;
  }

private:
  // Internal state for the pool
  // Uses kj::Vector<T*> for raw pointer storage (pool owns the objects)
  struct PoolState {
    kj::Vector<T*> pool;
    size_t size;

    explicit PoolState(size_t initial_size) : size(initial_size) {
      pool.reserve(initial_size);
      for (size_t i = 0; i < initial_size; ++i) {
        pool.add(new T());
      }
    }

    ~PoolState() {
      for (size_t i = 0; i < pool.size(); ++i) {
        delete pool[i];
      }
    }

    // Non-copyable
    PoolState(const PoolState&) = delete;
    PoolState& operator=(const PoolState&) = delete;

    // Movable for MutexGuarded initialization
    PoolState(PoolState&& other) noexcept : pool(kj::mv(other.pool)), size(other.size) {
      other.size = 0;
    }
    PoolState& operator=(PoolState&& other) noexcept {
      if (this != &other) {
        for (size_t i = 0; i < pool.size(); ++i) {
          delete pool[i];
        }
        pool = kj::mv(other.pool);
        size = other.size;
        other.size = 0;
      }
      return *this;
    }
  };

  kj::MutexGuarded<PoolState> guarded_;
  const size_t max_size_;
};

// =======================================================================================
// ThreadLocalObjectPool - Thread-local object pool (no synchronization needed)
// =======================================================================================

/**
 * @brief Thread-local object pool for single-threaded contexts
 * @tparam T Type of objects to pool
 *
 * ThreadLocalObjectPool provides object pooling without synchronization overhead.
 * Each thread has its own pool, making it ideal for per-thread allocation patterns.
 *
 * Uses PooledObject<T> as return type with kj::Function for the release callback.
 *
 * Example:
 * @code
 * ThreadLocalObjectPool<Buffer> pool(10, 100);
 * {
 *   auto buf = pool.acquire(1024);
 *   buf->write(data);
 * } // Buffer returned to thread-local pool
 * @endcode
 */
template <typename T> class ThreadLocalObjectPool final {
public:
  explicit ThreadLocalObjectPool(size_t initial_size = 0, size_t max_size = 0)
      : initial_size_(initial_size), max_size_(max_size) {}

  ~ThreadLocalObjectPool() = default;

  // Non-copyable, non-movable
  ThreadLocalObjectPool(const ThreadLocalObjectPool&) = delete;
  ThreadLocalObjectPool& operator=(const ThreadLocalObjectPool&) = delete;
  ThreadLocalObjectPool(ThreadLocalObjectPool&&) = delete;
  ThreadLocalObjectPool& operator=(ThreadLocalObjectPool&&) = delete;

  /**
   * @brief Acquire an object from the thread-local pool
   * @tparam Args Constructor argument types
   * @param args Constructor arguments (used to reinitialize recycled objects)
   * @return PooledObject<T> RAII wrapper that returns object to pool on destruction
   * @throws kj::Exception if pool is exhausted and at max_size
   */
  template <typename... Args> [[nodiscard]] PooledObject<T> acquire(Args&&... args) {
    auto& pool = pool_;

    if (pool.size() > 0) {
      T* ptr = pool.back();
      pool.removeLast();
      ptr->~T();
      new (ptr) T(kj::fwd<Args>(args)...);
      return PooledObject<T>(ptr, [this](T* p) { release(p); });
    }

    if (max_size_ == 0 || pool.size() < max_size_) {
      auto ptr = new T(kj::fwd<Args>(args)...);
      return PooledObject<T>(ptr, [this](T* p) { release(p); });
    }

    KJ_FAIL_REQUIRE("Thread local object pool exhausted", max_size_);
  }

  /**
   * @brief Release an object back to the thread-local pool
   * @param ptr Raw pointer to the object to release
   *
   * This is called automatically by PooledObject destructor.
   */
  void release(T* ptr) {
    if (ptr == nullptr) {
      return;
    }

    auto& pool = pool_;
    if (max_size_ > 0 && pool.size() >= max_size_) {
      delete ptr;
    } else {
      pool.add(ptr);
    }
  }

private:
  // Thread-local storage for raw pointers (pool owns the objects)
  // Using kj::Vector<T*> instead of std::vector<std::unique_ptr<T>>
  static thread_local kj::Vector<T*> pool_;
  const size_t initial_size_;
  const size_t max_size_;
};

template <typename T> thread_local kj::Vector<T*> ThreadLocalObjectPool<T>::pool_;

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
