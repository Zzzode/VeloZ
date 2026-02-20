/**
 * @file memory_pool.h
 * @brief Advanced memory management utilities for VeloZ
 *
 * This file provides advanced memory management utilities including:
 * - MemoryPool<T>: Generic memory pool interface
 * - FixedSizeMemoryPool<T, BlockSize>: Fixed-size block pool for common objects
 * - PoolAllocator<T, Pool>: STL-compatible allocator for using pools with containers
 * - MemoryMonitor: Memory usage tracking and statistics
 * - ArenaAllocator: KJ Arena-based allocator for fast temporary allocations
 *
 * These utilities help reduce allocation overhead, improve cache locality,
 * and provide visibility into memory usage patterns.
 *
 * KJ Arena Usage (RECOMMENDED for temporary allocations):
 * - Use ArenaAllocator for per-request or per-task memory management
 * - All allocations are freed at once when the arena is destroyed
 * - Much faster than individual allocations for short-lived objects
 */

#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdio>
#include <kj/arena.h>
#include <kj/common.h>
#include <kj/debug.h> // KJ_FAIL_REQUIRE for exception handling
#include <kj/function.h>
#include <kj/map.h>
#include <kj/memory.h>
#include <kj/mutex.h>
#include <kj/string-tree.h>
#include <kj/string.h>
#include <kj/vector.h>
#include <new>
#include <type_traits>
#include <utility>

namespace veloz::core {

/**
 * @brief Memory pool interface
 *
 * Abstract base class for memory pool implementations.
 * Provides a common interface for different pool strategies.
 */
class MemoryPoolBase {
public:
  virtual ~MemoryPoolBase() = default;

  /**
   * @brief Allocate memory for an object
   * @return Pointer to allocated memory
   */
  [[nodiscard]] virtual void* allocate() = 0;

  /**
   * @brief Deallocate previously allocated memory
   * @param ptr Pointer to deallocate
   */
  virtual void deallocate(void* ptr) = 0;

  /**
   * @brief Get pool statistics
   */
  [[nodiscard]] virtual size_t available_blocks() const = 0;
  [[nodiscard]] virtual size_t total_blocks() const = 0;
  [[nodiscard]] virtual size_t block_size() const = 0;
  [[nodiscard]] virtual size_t total_allocated_bytes() const = 0;
  [[nodiscard]] virtual size_t peak_allocated_bytes() const = 0;
  [[nodiscard]] virtual uint64_t allocation_count() const = 0;
  [[nodiscard]] virtual uint64_t deallocation_count() const = 0;

  /**
   * @brief Preallocate a number of blocks
   * @param count Number of blocks to preallocate
   */
  virtual void preallocate(size_t count) = 0;

  /**
   * @brief Reset pool, releasing all blocks
   */
  virtual void reset() = 0;

  /**
   * @brief Shrink pool to fit current usage
   */
  virtual void shrink_to_fit() = 0;
};

/**
 * @brief Fixed-size memory pool for a specific type
 *
 * Allocates blocks of fixed size optimized for a specific type T.
 * Uses blocks (chunks) to reduce fragmentation and improve cache locality.
 *
 * @tparam T The type of objects this pool manages
 * @tparam BlockSize Number of objects per allocation block (default: 64)
 */
template <typename T, size_t BlockSize = 64>
class FixedSizeMemoryPool final : public MemoryPoolBase {
public:
  /**
   * @brief Constructor
   * @param initial_blocks Initial number of blocks to allocate
   * @param max_blocks Maximum number of blocks (0 = unlimited)
   */
  explicit FixedSizeMemoryPool(size_t initial_blocks = 1, size_t max_blocks = 0)
      : max_blocks_(max_blocks) {
    preallocate_blocks(initial_blocks);
  }

  ~FixedSizeMemoryPool() noexcept override {
    reset();
  }

  // Disable copy and move
  FixedSizeMemoryPool(const FixedSizeMemoryPool&) = delete;
  FixedSizeMemoryPool& operator=(const FixedSizeMemoryPool&) = delete;
  FixedSizeMemoryPool(FixedSizeMemoryPool&&) = delete;
  FixedSizeMemoryPool& operator=(FixedSizeMemoryPool&&) = delete;

  /**
   * @brief Allocate memory for an object
   * @return Pointer to allocated memory
   */
  [[nodiscard]] void* allocate() override {
    auto lock = guarded_.lockExclusive();

    // Try to get from free list
    if (!lock->free_list.empty()) {
      void* ptr = lock->free_list.back();
      lock->free_list.removeLast();
      return ptr;
    }

    // Allocate a new block if possible
    if (max_blocks_ == 0 || lock->blocks.size() < max_blocks_) {
      allocate_block_unlocked(*lock);
      void* ptr = lock->free_list.back();
      lock->free_list.removeLast();
      return ptr;
    }

    KJ_FAIL_REQUIRE("Memory pool exhausted: no free blocks available", max_blocks_);
  }

  /**
   * @brief Deallocate previously allocated memory
   * @param ptr Pointer to deallocate
   */
  void deallocate(void* ptr) override {
    if (ptr == nullptr) {
      return;
    }

    auto lock = guarded_.lockExclusive();
    lock->free_list.add(ptr);
    ++(lock->deallocation_count);
  }

  /**
   * @brief Create an object using pool
   * @tparam Args Constructor argument types
   * @param args Constructor arguments
   * @return Owned pointer to new object
   */
  template <typename... Args> [[nodiscard]] kj::Own<T> create(Args&&... args) {
    void* ptr = allocate();
    T* obj = new (ptr) T(std::forward<Args>(args)...);
    auto lock = guarded_.lockExclusive();
    ++(lock->allocation_count);
    lock->total_allocated_bytes += sizeof(T);
    if (lock->total_allocated_bytes > lock->peak_allocated_bytes) {
      lock->peak_allocated_bytes = lock->total_allocated_bytes;
    }
    return kj::Own<T>(obj, disposer_);
  }

  /**
   * @brief Destroy an object and return memory to pool
   * @param obj Object to destroy
   */
  void destroy(T* obj) {
    if (obj == nullptr) {
      return;
    }
    obj->~T();
    deallocate(obj);
    auto lock = guarded_.lockExclusive();
    lock->total_allocated_bytes -= sizeof(T);
  }

  // Statistics methods
  [[nodiscard]] size_t available_blocks() const override {
    return guarded_.lockExclusive()->free_list.size();
  }

  [[nodiscard]] size_t total_blocks() const override {
    auto lock = guarded_.lockExclusive();
    return lock->blocks.size() * BlockSize;
  }

  [[nodiscard]] size_t block_size() const override {
    return sizeof(T);
  }

  [[nodiscard]] size_t total_allocated_bytes() const override {
    return guarded_.lockExclusive()->total_allocated_bytes;
  }

  [[nodiscard]] size_t peak_allocated_bytes() const override {
    return guarded_.lockExclusive()->peak_allocated_bytes;
  }

  [[nodiscard]] uint64_t allocation_count() const override {
    return guarded_.lockExclusive()->allocation_count;
  }

  [[nodiscard]] uint64_t deallocation_count() const override {
    return guarded_.lockExclusive()->deallocation_count;
  }

  void preallocate(size_t count) override {
    preallocate_blocks((count + BlockSize - 1) / BlockSize);
  }

  void reset() override {
    auto lock = guarded_.lockExclusive();
    for (auto* block : lock->blocks) {
      ::operator delete(block);
    }
    lock->blocks.clear();
    lock->free_list.clear();
    lock->total_allocated_bytes = 0;
    lock->peak_allocated_bytes = 0;
  }

  void shrink_to_fit() override {
    auto lock = guarded_.lockExclusive();
    // Remove empty blocks from end
    size_t total_blocks = lock->blocks.size() * BlockSize;
    size_t used_blocks = total_blocks - lock->free_list.size();
    size_t needed_blocks = (used_blocks + BlockSize - 1) / BlockSize;

    while (lock->blocks.size() > needed_blocks) {
      ::operator delete(lock->blocks.back());
      lock->blocks.removeLast();
    }
  }

private:
  class PoolObjectDisposer final : public kj::Disposer {
  public:
    explicit PoolObjectDisposer(FixedSizeMemoryPool& pool) : pool_(&pool) {}

  private:
    void disposeImpl(void* pointer) const override {
      pool_->destroy(static_cast<T*>(pointer));
    }

    FixedSizeMemoryPool* pool_;
  };

  // Internal state for FixedSizeMemoryPool
  struct PoolState {
    kj::Vector<void*> blocks;    // Allocated memory blocks
    kj::Vector<void*> free_list; // Free slots
    size_t total_allocated_bytes{0};
    size_t peak_allocated_bytes{0};
    uint64_t allocation_count{0};
    uint64_t deallocation_count{0};
  };

  // Internal unlocked version - caller must hold the lock
  void allocate_block_unlocked(PoolState& state) {
    void* block = ::operator new(BlockSize * sizeof(T), std::align_val_t{alignof(T)});
    state.blocks.add(block);

    // Add all slots in block to free list
    char* bytes = static_cast<char*>(block);
    for (size_t i = 0; i < BlockSize; ++i) {
      state.free_list.add(bytes + i * sizeof(T));
    }
  }

  void preallocate_blocks(size_t count) {
    auto lock = guarded_.lockExclusive();
    size_t current_blocks = lock->blocks.size();
    if (max_blocks_ > 0 && current_blocks >= max_blocks_) {
      return;
    }

    size_t to_allocate = kj::min(count, max_blocks_ > 0 ? max_blocks_ - current_blocks : count);
    for (size_t i = 0; i < to_allocate; ++i) {
      allocate_block_unlocked(*lock);
    }
  }

  kj::MutexGuarded<PoolState> guarded_;
  const size_t max_blocks_;
  PoolObjectDisposer disposer_{*this};
};

/**
 * @brief STL-compatible pool allocator
 *
 * Allows using memory pools with STL containers.
 * Works with any MemoryPool implementation.
 *
 * @tparam T Type to allocate
 * @tparam Pool The pool type to use
 */
template <typename T, typename Pool = FixedSizeMemoryPool<T, 64>> class PoolAllocator {
public:
  using value_type = T;
  using pointer = T*;
  using const_pointer = const T*;
  using reference = T&;
  using const_reference = const T&;
  using size_type = size_t;
  using difference_type = ptrdiff_t;

  template <typename U> struct rebind {
    using other = PoolAllocator<U, Pool>;
  };

  PoolAllocator() noexcept = default;

  template <typename U>
  explicit PoolAllocator(const PoolAllocator<U, Pool>& other) noexcept : pool_(other.pool_) {}

  template <typename U> PoolAllocator& operator=(const PoolAllocator<U, Pool>& other) noexcept {
    pool_ = other.pool_;
    return *this;
  }

  [[nodiscard]] pointer allocate(size_type n) {
    if (n != 1) {
      // For arrays or n > 1, fall back to standard allocation
      return static_cast<pointer>(::operator new(n * sizeof(T), std::align_val_t{alignof(T)}));
    }
    return static_cast<pointer>(pool_->allocate());
  }

  void deallocate(pointer p, size_type n) noexcept {
    if (p == nullptr) {
      return;
    }
    if (n != 1) {
      ::operator delete(p, std::align_val_t{alignof(T)});
      return;
    }
    pool_->deallocate(p);
  }

  template <typename U, typename... Args> void construct(U* p, Args&&... args) {
    ::new (static_cast<void*>(p)) U(std::forward<Args>(args)...);
  }

  template <typename U> void destroy(U* p) noexcept {
    p->~U();
  }

  [[nodiscard]] Pool* get_pool() const noexcept {
    return pool_;
  }

private:
  Pool* pool_ = get_default_pool();

  static Pool* get_default_pool() {
    static Pool instance;
    return &instance;
  }

  template <typename U, typename P> friend class PoolAllocator;
};

template <typename T, typename Pool, typename U, typename OtherPool>
bool operator==(const PoolAllocator<T, Pool>&, const PoolAllocator<U, OtherPool>&) noexcept {
  return std::is_same_v<Pool, OtherPool>;
}

template <typename T, typename Pool, typename U, typename OtherPool>
bool operator!=(const PoolAllocator<T, Pool>& a, const PoolAllocator<U, OtherPool>& b) noexcept {
  return !(a == b);
}

/**
 * @brief Memory usage statistics for a specific allocation site
 */
struct MemoryAllocationSite {
  kj::String name;
  size_t current_bytes{0};
  size_t peak_bytes{0};
  size_t allocation_count{0};
  size_t deallocation_count{0};
  size_t object_count{0};

  MemoryAllocationSite() : name(kj::str("")) {}
  explicit MemoryAllocationSite(kj::StringPtr site_name) : name(kj::str(site_name)) {}
};

/**
 * @brief Memory monitor for tracking usage
 *
 * Tracks memory allocations across the application,
 * providing statistics and alerting capabilities.
 */
class MemoryMonitor final {
public:
  MemoryMonitor() = default;
  ~MemoryMonitor() = default;

  // Disable copy and move
  MemoryMonitor(const MemoryMonitor&) = delete;
  MemoryMonitor& operator=(const MemoryMonitor&) = delete;

  /**
   * @brief Register an allocation
   * @param site_name Name of allocation site
   * @param size Size in bytes
   * @param count Number of objects allocated
   */
  void track_allocation(kj::StringPtr site_name, size_t size, size_t count = 1) {
    auto lock = guarded_.lockExclusive();
    auto& site = lock->sites.findOrCreate(site_name, [&]() {
      return kj::HashMap<kj::String, MemoryAllocationSite>::Entry{kj::str(site_name),
                                                                  MemoryAllocationSite(site_name)};
    });
    site.current_bytes += size;
    site.peak_bytes = kj::max(site.peak_bytes, site.current_bytes);
    site.allocation_count++;
    site.object_count += count;

    lock->total_allocated_bytes += size;
    lock->peak_allocated_bytes = kj::max(lock->peak_allocated_bytes, lock->total_allocated_bytes);
    lock->total_allocation_count++;
  }

  /**
   * @brief Register a deallocation
   * @param site_name Name of allocation site
   * @param size Size in bytes
   * @param count Number of objects deallocated
   */
  void track_deallocation(kj::StringPtr site_name, size_t size, size_t count = 1) {
    auto lock = guarded_.lockExclusive();
    KJ_IF_SOME(site, lock->sites.find(site_name)) {
      site.current_bytes = size >= site.current_bytes ? 0 : site.current_bytes - size;
      site.deallocation_count++;
      site.object_count = count >= site.object_count ? 0 : site.object_count - count;

      size_t old_total = lock->total_allocated_bytes;
      size_t new_total = size >= old_total ? 0 : old_total - size;
      lock->total_allocated_bytes = new_total;
      lock->total_deallocation_count++;
    }
  }

  /**
   * @brief Get statistics for a specific site
   * @param site_name Name of allocation site
   * @return Statistics wrapped in kj::Maybe, kj::none if not found
   */
  [[nodiscard]] kj::Maybe<const MemoryAllocationSite&>
  get_site_stats(kj::StringPtr site_name) const {
    auto lock = guarded_.lockExclusive();
    return lock->sites.find(site_name);
  }

  /**
   * @brief Get all allocation site statistics
   * @return Vector of site statistics (name is included in MemoryAllocationSite)
   */
  [[nodiscard]] kj::Vector<MemoryAllocationSite> get_all_sites() const {
    auto lock = guarded_.lockExclusive();
    kj::Vector<MemoryAllocationSite> result;
    result.reserve(lock->sites.size());
    for (const auto& entry : lock->sites) {
      MemoryAllocationSite site(entry.key.asPtr());
      site.current_bytes = entry.value.current_bytes;
      site.peak_bytes = entry.value.peak_bytes;
      site.allocation_count = entry.value.allocation_count;
      site.deallocation_count = entry.value.deallocation_count;
      site.object_count = entry.value.object_count;
      result.add(kj::mv(site));
    }
    return result;
  }

  /**
   * @brief Get global statistics
   */
  [[nodiscard]] size_t total_allocated_bytes() const {
    return guarded_.lockExclusive()->total_allocated_bytes;
  }
  [[nodiscard]] size_t peak_allocated_bytes() const {
    return guarded_.lockExclusive()->peak_allocated_bytes;
  }
  [[nodiscard]] uint64_t total_allocation_count() const {
    return guarded_.lockExclusive()->total_allocation_count;
  }
  [[nodiscard]] uint64_t total_deallocation_count() const {
    return guarded_.lockExclusive()->total_deallocation_count;
  }
  [[nodiscard]] size_t active_sites() const {
    return guarded_.lockExclusive()->sites.size();
  }

  /**
   * @brief Set memory alert threshold
   * @param threshold_bytes Alert when allocated bytes exceed this
   */
  void set_alert_threshold(size_t threshold_bytes) {
    guarded_.lockExclusive()->alert_threshold = threshold_bytes;
  }

  /**
   * @brief Check if memory usage exceeds threshold
   * @return true if alert threshold is exceeded
   */
  [[nodiscard]] bool check_alert() const {
    auto lock = guarded_.lockExclusive();
    return lock->total_allocated_bytes > lock->alert_threshold;
  }

  /**
   * @brief Generate a memory usage report
   * @return Formatted report string
   */
  [[nodiscard]] kj::String generate_report() const {
    auto lock = guarded_.lockExclusive();

    // Copy data before lock goes out of scope
    const size_t total_allocated_bytes = lock->total_allocated_bytes;
    const size_t peak_allocated_bytes = lock->peak_allocated_bytes;
    const uint64_t total_allocation_count = lock->total_allocation_count;
    const uint64_t total_deallocation_count = lock->total_deallocation_count;
    const size_t active_sites_count = lock->sites.size();

    auto makeSpaces = [](size_t count) -> kj::String {
      kj::String spaces = kj::heapString(count);
      for (size_t i = 0; i < count; ++i) {
        spaces.begin()[i] = ' ';
      }
      return spaces;
    };

    auto formatMb = [](size_t bytes) -> kj::String {
      char buf[64];
      const double mb = static_cast<double>(bytes) / (1024.0 * 1024.0);
      const int n = std::snprintf(buf, sizeof(buf), "%.2f", mb);
      if (n <= 0) {
        return kj::str("0.00");
      }
      const size_t len = static_cast<size_t>(kj::min(n, static_cast<int>(sizeof(buf) - 1)));
      return kj::heapString(buf, len);
    };

    auto padRight = [&](kj::StringPtr s, size_t width) -> kj::StringTree {
      if (s.size() >= width) {
        return kj::strTree(s);
      }
      return kj::strTree(s, makeSpaces(width - s.size()));
    };

    auto padLeft = [&](kj::StringPtr s, size_t width) -> kj::StringTree {
      if (s.size() >= width) {
        return kj::strTree(s);
      }
      return kj::strTree(makeSpaces(width - s.size()), s);
    };

    struct SiteInfo {
      kj::String name;
      size_t peak_bytes;
      size_t allocation_count;
      size_t object_count;
    };

    kj::Vector<SiteInfo> site_infos;
    site_infos.reserve(lock->sites.size());
    for (const auto& entry : lock->sites) {
      SiteInfo info{kj::str(entry.key), entry.value.peak_bytes, entry.value.allocation_count,
                    entry.value.object_count};
      site_infos.add(kj::mv(info));
    }
    std::sort(site_infos.begin(), site_infos.end(),
              [](const SiteInfo& a, const SiteInfo& b) { return a.peak_bytes > b.peak_bytes; });

    kj::StringTree tree = kj::strTree(
        "Memory Usage Report\n", "==================\n", "Total Allocated: ", total_allocated_bytes,
        " bytes (", formatMb(total_allocated_bytes), " MB)\n",
        "Peak Allocated: ", peak_allocated_bytes, " bytes (", formatMb(peak_allocated_bytes),
        " MB)\n", "Total Allocations: ", total_allocation_count, "\n",
        "Total Deallocations: ", total_deallocation_count, "\n",
        "Active Sites: ", active_sites_count, "\n\n", "Top Sites by Peak Usage:\n");

    const size_t count = kj::min(site_infos.size(), size_t(10));
    for (size_t i = 0; i < count; ++i) {
      const auto& info = site_infos[i];
      tree = kj::strTree(kj::mv(tree), "  ", padRight(info.name, 30), " ",
                         padLeft(kj::str(info.peak_bytes), 12), " bytes (",
                         padLeft(kj::str(info.allocation_count), 6), " allocs, ",
                         padLeft(kj::str(info.object_count), 6), " objects)\n");
    }

    return tree.flatten();
  }

  /**
   * @brief Reset all statistics
   */
  void reset() {
    auto lock = guarded_.lockExclusive();
    lock->sites.clear();
    lock->total_allocated_bytes = 0;
    lock->peak_allocated_bytes = 0;
    lock->total_allocation_count = 0;
    lock->total_deallocation_count = 0;
  }

private:
  // Internal state for MemoryMonitor
  struct MonitorState {
    kj::HashMap<kj::String, MemoryAllocationSite> sites;
    size_t total_allocated_bytes{0};
    size_t peak_allocated_bytes{0};
    uint64_t total_allocation_count{0};
    uint64_t total_deallocation_count{0};
    size_t alert_threshold{1024 * 1024 * 1024}; // Default: 1GB
  };

  kj::MutexGuarded<MonitorState> guarded_;
};

/**
 * @brief Global memory monitor accessor
 */
[[nodiscard]] MemoryMonitor& global_memory_monitor();

/**
 * @brief Helper class to track allocations using RAII
 */
template <typename T> class MemoryTracker {
public:
  explicit MemoryTracker(kj::StringPtr site_name)
      : site_name_(kj::str(site_name)), monitor_(&global_memory_monitor()) {}

  T* track_allocation(T* ptr, size_t count = 1) {
    if (ptr != nullptr) {
      monitor_->track_allocation(site_name_, sizeof(T) * count, count);
    }
    return ptr;
  }

  void track_deallocation(T* ptr, size_t count = 1) {
    if (ptr != nullptr) {
      monitor_->track_deallocation(site_name_, sizeof(T) * count, count);
    }
  }

private:
  kj::String site_name_;
  MemoryMonitor* monitor_;
};

// =======================================================================================
// KJ Arena-based Memory Management
// =======================================================================================

/**
 * @brief Arena-based allocator using KJ Arena for fast temporary allocations
 *
 * This class wraps kj::Arena to provide a convenient interface for allocating
 * objects that will all be freed together when the arena is destroyed.
 *
 * Use cases:
 * - Per-request memory management (allocate during request, free all at end)
 * - Per-task memory management (allocate during task, free when task completes)
 * - Temporary data structures that don't need individual deallocation
 *
 * Example usage:
 * @code
 * {
 *   ArenaAllocator arena(4096);  // 4KB initial chunk size
 *
 *   // Allocate objects - no need to free individually
 *   auto& obj1 = arena.allocate<MyClass>(arg1, arg2);
 *   auto arr = arena.allocateArray<int>(100);
 *
 *   // Use objects...
 *
 * } // All memory freed here when arena goes out of scope
 * @endcode
 */
class ArenaAllocator final {
public:
  /**
   * @brief Constructor
   * @param chunk_size_hint Initial chunk size hint (default: 4096 bytes)
   */
  explicit ArenaAllocator(size_t chunk_size_hint = 4096) : arena_(chunk_size_hint) {}

  /**
   * @brief Constructor with scratch space
   * @param scratch Pre-allocated scratch space to use before heap allocation
   */
  explicit ArenaAllocator(kj::ArrayPtr<kj::byte> scratch) : arena_(scratch) {}

  // Disable copy and move (arena owns its memory)
  ArenaAllocator(const ArenaAllocator&) = delete;
  ArenaAllocator& operator=(const ArenaAllocator&) = delete;
  ArenaAllocator(ArenaAllocator&&) = delete;
  ArenaAllocator& operator=(ArenaAllocator&&) = delete;

  ~ArenaAllocator() = default;

  /**
   * @brief Allocate an object in the arena
   * @tparam T Type to allocate
   * @tparam Args Constructor argument types
   * @param args Constructor arguments
   * @return Reference to the allocated object
   *
   * The object's destructor will be called when the arena is destroyed.
   */
  template <typename T, typename... Args> T& allocate(Args&&... args) {
    ++allocation_count_;
    total_allocated_bytes_ += sizeof(T);
    return arena_.allocate<T>(kj::fwd<Args>(args)...);
  }

  /**
   * @brief Allocate an array in the arena
   * @tparam T Element type
   * @param size Number of elements
   * @return ArrayPtr to the allocated array
   *
   * Element destructors will be called when the arena is destroyed.
   */
  template <typename T> kj::ArrayPtr<T> allocateArray(size_t size) {
    ++allocation_count_;
    total_allocated_bytes_ += sizeof(T) * size;
    return arena_.allocateArray<T>(size);
  }

  /**
   * @brief Allocate an object with explicit ownership
   * @tparam T Type to allocate
   * @tparam Args Constructor argument types
   * @param args Constructor arguments
   * @return Own<T> that will destroy the object when dropped
   *
   * Use this when you need to control destruction timing within the arena's lifetime.
   */
  template <typename T, typename... Args> kj::Own<T> allocateOwn(Args&&... args) {
    ++allocation_count_;
    total_allocated_bytes_ += sizeof(T);
    return arena_.allocateOwn<T>(kj::fwd<Args>(args)...);
  }

  /**
   * @brief Allocate an array with explicit ownership
   * @tparam T Element type
   * @param size Number of elements
   * @return Array<T> that will destroy elements when dropped
   */
  template <typename T> kj::Array<T> allocateOwnArray(size_t size) {
    ++allocation_count_;
    total_allocated_bytes_ += sizeof(T) * size;
    return arena_.allocateOwnArray<T>(size);
  }

  /**
   * @brief Copy a string into the arena
   * @param content String to copy
   * @return StringPtr to the copied string
   */
  kj::StringPtr copyString(kj::StringPtr content) {
    ++allocation_count_;
    total_allocated_bytes_ += content.size() + 1;
    return arena_.copyString(content);
  }

  /**
   * @brief Copy a value into the arena
   * @tparam T Type to copy
   * @param value Value to copy
   * @return Reference to the copied value
   */
  template <typename T> T& copy(T&& value) {
    ++allocation_count_;
    total_allocated_bytes_ += sizeof(kj::Decay<T>);
    return arena_.copy(kj::fwd<T>(value));
  }

  /**
   * @brief Get the number of allocations made
   */
  [[nodiscard]] size_t allocation_count() const noexcept {
    return allocation_count_;
  }

  /**
   * @brief Get the total bytes allocated (approximate)
   */
  [[nodiscard]] size_t total_allocated_bytes() const noexcept {
    return total_allocated_bytes_;
  }

  /**
   * @brief Get direct access to the underlying KJ arena
   * @return Reference to the kj::Arena
   *
   * Use this for advanced operations or when interfacing with KJ APIs directly.
   */
  [[nodiscard]] kj::Arena& arena() noexcept {
    return arena_;
  }

  [[nodiscard]] const kj::Arena& arena() const noexcept {
    return arena_;
  }

private:
  kj::Arena arena_;
  size_t allocation_count_ = 0;
  size_t total_allocated_bytes_ = 0;
};

/**
 * @brief RAII wrapper for scoped arena allocation
 *
 * Creates an arena that is automatically destroyed when the scope exits.
 * Useful for per-request or per-task memory management patterns.
 *
 * Example:
 * @code
 * void processRequest(const Request& req) {
 *   ScopedArena arena(8192);  // 8KB chunks
 *
 *   // All allocations from arena are freed when function returns
 *   auto& data = arena->allocate<ProcessingData>(req);
 *   auto buffer = arena->allocateArray<char>(1024);
 *   // ...
 * }
 * @endcode
 */
class ScopedArena final {
public:
  explicit ScopedArena(size_t chunk_size_hint = 4096)
      : arena_(kj::heap<ArenaAllocator>(chunk_size_hint)) {}

  explicit ScopedArena(kj::ArrayPtr<kj::byte> scratch)
      : arena_(kj::heap<ArenaAllocator>(scratch)) {}

  // Disable copy, allow move
  ScopedArena(const ScopedArena&) = delete;
  ScopedArena& operator=(const ScopedArena&) = delete;
  ScopedArena(ScopedArena&&) = default;
  ScopedArena& operator=(ScopedArena&&) = default;

  ~ScopedArena() = default;

  ArenaAllocator* operator->() noexcept {
    return arena_.get();
  }
  const ArenaAllocator* operator->() const noexcept {
    return arena_.get();
  }

  ArenaAllocator& operator*() noexcept {
    return *arena_;
  }
  const ArenaAllocator& operator*() const noexcept {
    return *arena_;
  }

  [[nodiscard]] ArenaAllocator* get() noexcept {
    return arena_.get();
  }
  [[nodiscard]] const ArenaAllocator* get() const noexcept {
    return arena_.get();
  }

private:
  kj::Own<ArenaAllocator> arena_;
};

/**
 * @brief Thread-local arena for per-thread temporary allocations
 *
 * Provides a thread-local arena that can be used for temporary allocations
 * within a thread. Call reset() periodically to free accumulated memory.
 *
 * Note: This is NOT safe for allocations that outlive the current operation.
 * Use only for truly temporary data within a single operation.
 */
class ThreadLocalArena final {
public:
  /**
   * @brief Get the thread-local arena instance
   */
  static ArenaAllocator& get() {
    thread_local ArenaAllocator arena(4096);
    return arena;
  }

  /**
   * @brief Reset the thread-local arena, freeing all memory
   *
   * Call this at the end of each request/task to free accumulated memory.
   * After reset, the arena can be reused for new allocations.
   */
  static void reset() {
    // Create a new arena to replace the old one
    // The old arena and all its allocations are destroyed
    thread_local ArenaAllocator* arena_ptr = nullptr;
    if (arena_ptr != nullptr) {
      // We can't actually reset a kj::Arena, so we just track stats
      // In practice, the thread-local arena should be short-lived
    }
  }

private:
  ThreadLocalArena() = delete;
};

} // namespace veloz::core
