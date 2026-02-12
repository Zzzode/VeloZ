/**
 * @file memory_pool.h
 * @brief Advanced memory management utilities for VeloZ
 *
 * This file provides advanced memory management utilities including:
 * - MemoryPool<T>: Generic memory pool interface
 * - FixedSizeMemoryPool<T, BlockSize>: Fixed-size block pool for common objects
 * - PoolAllocator<T, Pool>: STL-compatible allocator for using pools with containers
 * - MemoryMonitor: Memory usage tracking and statistics
 *
 * These utilities help reduce allocation overhead, improve cache locality,
 * and provide visibility into memory usage patterns.
 */

#pragma once

#include <cstddef>
#include <memory>
#include <vector>
#include <deque>
#include <mutex>
#include <functional>
#include <atomic>
#include <stdexcept>
#include <new>
#include <thread>
#include <unordered_map>
#include <string>
#include <algorithm>
#include <format>
#include <sstream>
#include <iomanip>

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
   * @brief Reset the pool, releasing all blocks
   */
  virtual void reset() = 0;

  /**
   * @brief Shrink the pool to fit current usage
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

  ~FixedSizeMemoryPool() override {
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
    std::scoped_lock lock(mu_);

    // Try to get from free list
    if (!free_list_.empty()) {
      void* ptr = free_list_.back();
      free_list_.pop_back();
      return ptr;
    }

    // Allocate a new block if possible
    if (max_blocks_ == 0 || blocks_.size() < max_blocks_) {
      allocate_block();
      void* ptr = free_list_.back();
      free_list_.pop_back();
      return ptr;
    }

    throw std::bad_alloc();
  }

  /**
   * @brief Deallocate previously allocated memory
   * @param ptr Pointer to deallocate
   */
  void deallocate(void* ptr) override {
    if (ptr == nullptr) {
      return;
    }

    std::scoped_lock lock(mu_);
    free_list_.push_back(ptr);
    ++deallocation_count_;
  }

  /**
   * @brief Create an object using the pool
   * @tparam Args Constructor argument types
   * @param args Constructor arguments
   * @return Pointer to new object with custom deleter
   */
  template <typename... Args>
  [[nodiscard]] std::unique_ptr<T, std::function<void(T*)>> create(Args&&... args) {
    void* ptr = allocate();
    T* obj = new (ptr) T(std::forward<Args>(args)...);
    ++allocation_count_;
    total_allocated_bytes_ += sizeof(T);
    size_t current_total = total_allocated_bytes_.load();
    size_t current_peak = peak_allocated_bytes_.load();
    while (current_total > current_peak && !peak_allocated_bytes_.compare_exchange_weak(current_peak, current_total)) {
      // Retry if another thread updated it
      current_total = total_allocated_bytes_.load();
      current_peak = peak_allocated_bytes_.load();
    }
    return std::unique_ptr<T, std::function<void(T*)>>(
        obj, [this](T* p) { destroy(p); });
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
    total_allocated_bytes_ -= sizeof(T);
  }

  // Statistics methods
  [[nodiscard]] size_t available_blocks() const override {
    std::scoped_lock lock(mu_);
    return free_list_.size();
  }

  [[nodiscard]] size_t total_blocks() const override {
    std::scoped_lock lock(mu_);
    return blocks_.size() * BlockSize;
  }

  [[nodiscard]] size_t block_size() const override {
    return sizeof(T);
  }

  [[nodiscard]] size_t total_allocated_bytes() const override {
    return total_allocated_bytes_;
  }

  [[nodiscard]] size_t peak_allocated_bytes() const override {
    return peak_allocated_bytes_;
  }

  [[nodiscard]] uint64_t allocation_count() const override {
    return allocation_count_;
  }

  [[nodiscard]] uint64_t deallocation_count() const override {
    return deallocation_count_;
  }

  void preallocate(size_t count) override {
    preallocate_blocks((count + BlockSize - 1) / BlockSize);
  }

  void reset() override {
    std::scoped_lock lock(mu_);
    for (auto* block : blocks_) {
      ::operator delete(block);
    }
    blocks_.clear();
    free_list_.clear();
    total_allocated_bytes_ = 0;
    peak_allocated_bytes_ = 0;
  }

  void shrink_to_fit() override {
    std::scoped_lock lock(mu_);
    // Remove empty blocks from the end
    size_t total_blocks = blocks_.size() * BlockSize;
    size_t used_blocks = total_blocks - free_list_.size();
    size_t needed_blocks = (used_blocks + BlockSize - 1) / BlockSize;

    while (blocks_.size() > needed_blocks) {
      ::operator delete(blocks_.back());
      blocks_.pop_back();
    }
  }

private:
  void allocate_block() {
    void* block = ::operator new(BlockSize * sizeof(T), std::align_val_t{alignof(T)});
    blocks_.push_back(block);

    // Add all slots in block to free list
    char* bytes = static_cast<char*>(block);
    for (size_t i = 0; i < BlockSize; ++i) {
      free_list_.push_back(bytes + i * sizeof(T));
    }
  }

  void preallocate_blocks(size_t count) {
    std::scoped_lock lock(mu_);
    size_t current_blocks = blocks_.size();
    if (max_blocks_ > 0 && current_blocks >= max_blocks_) {
      return;
    }

    size_t to_allocate = std::min(count, max_blocks_ > 0 ? max_blocks_ - current_blocks : count);
    for (size_t i = 0; i < to_allocate; ++i) {
      allocate_block();
    }
  }

  mutable std::mutex mu_;
  std::vector<void*> blocks_;          // Allocated memory blocks
  std::vector<void*> free_list_;        // Free slots
  const size_t max_blocks_;
  std::atomic<size_t> total_allocated_bytes_{0};
  std::atomic<size_t> peak_allocated_bytes_{0};
  std::atomic<uint64_t> allocation_count_{0};
  std::atomic<uint64_t> deallocation_count_{0};
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
template <typename T, typename Pool = FixedSizeMemoryPool<T, 64>>
class PoolAllocator {
public:
  using value_type = T;
  using pointer = T*;
  using const_pointer = const T*;
  using reference = T&;
  using const_reference = const T&;
  using size_type = size_t;
  using difference_type = ptrdiff_t;

  template <typename U>
  struct rebind {
    using other = PoolAllocator<U, Pool>;
  };

  PoolAllocator() noexcept = default;

  template <typename U>
  explicit PoolAllocator(const PoolAllocator<U, Pool>& other) noexcept
      : pool_(other.pool_) {}

  template <typename U>
  PoolAllocator& operator=(const PoolAllocator<U, Pool>& other) noexcept {
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

  template <typename U, typename... Args>
  void construct(U* p, Args&&... args) {
    ::new (static_cast<void*>(p)) U(std::forward<Args>(args)...);
  }

  template <typename U>
  void destroy(U* p) noexcept {
    p->~U();
  }

  [[nodiscard]] Pool* get_pool() const noexcept { return pool_; }

private:
  Pool* pool_ = get_default_pool();

  static Pool* get_default_pool() {
    static Pool instance;
    return &instance;
  }

  template <typename U, typename P>
  friend class PoolAllocator;
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
  std::string name;
  size_t current_bytes{0};
  size_t peak_bytes{0};
  size_t allocation_count{0};
  size_t deallocation_count{0};
  size_t object_count{0};
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
   * @param site_name Name of the allocation site
   * @param size Size in bytes
   * @param count Number of objects allocated
   */
  void track_allocation(const std::string& site_name, size_t size, size_t count = 1) {
    std::scoped_lock lock(mu_);
    auto& site = sites_[site_name];
    site.name = site_name;
    site.current_bytes += size;
    site.peak_bytes = std::max(site.peak_bytes, site.current_bytes);
    site.allocation_count++;
    site.object_count += count;

    total_allocated_bytes_.store(total_allocated_bytes_.load() + size);
    size_t current_peak = peak_allocated_bytes_.load();
    size_t new_total = total_allocated_bytes_.load();
    while (new_total > current_peak && !peak_allocated_bytes_.compare_exchange_weak(current_peak, new_total)) {
      // Retry if another thread updated it
    }
    total_allocation_count_.store(total_allocation_count_.load() + 1);
  }

  /**
   * @brief Register a deallocation
   * @param site_name Name of the allocation site
   * @param size Size in bytes
   * @param count Number of objects deallocated
   */
  void track_deallocation(const std::string& site_name, size_t size, size_t count = 1) {
    std::scoped_lock lock(mu_);
    auto it = sites_.find(site_name);
    if (it != sites_.end()) {
      auto& site = it->second;
      site.current_bytes = size >= site.current_bytes ? 0 : site.current_bytes - size;
      site.deallocation_count++;
      site.object_count = count >= site.object_count ? 0 : site.object_count - count;

      size_t old_total = total_allocated_bytes_.load();
      size_t new_total = size >= old_total ? 0 : old_total - size;
      total_allocated_bytes_.store(new_total);
      total_deallocation_count_.store(total_deallocation_count_.load() + 1);
    }
  }

  /**
   * @brief Get statistics for a specific site
   * @param site_name Name of the allocation site
   * @return Statistics or nullptr if not found
   */
  [[nodiscard]] const MemoryAllocationSite* get_site_stats(const std::string& site_name) const {
    std::scoped_lock lock(mu_);
    auto it = sites_.find(site_name);
    return it != sites_.end() ? &it->second : nullptr;
  }

  /**
   * @brief Get all allocation site statistics
   * @return Map of site names to statistics
   */
  [[nodiscard]] std::unordered_map<std::string, MemoryAllocationSite> get_all_sites() const {
    std::scoped_lock lock(mu_);
    return sites_;
  }

  /**
   * @brief Get global statistics
   */
  [[nodiscard]] size_t total_allocated_bytes() const { return total_allocated_bytes_; }
  [[nodiscard]] size_t peak_allocated_bytes() const { return peak_allocated_bytes_; }
  [[nodiscard]] uint64_t total_allocation_count() const { return total_allocation_count_; }
  [[nodiscard]] uint64_t total_deallocation_count() const { return total_deallocation_count_; }
  [[nodiscard]] size_t active_sites() const {
    std::scoped_lock lock(mu_);
    return sites_.size();
  }

  /**
   * @brief Set memory alert threshold
   * @param threshold_bytes Alert when allocated bytes exceed this
   */
  void set_alert_threshold(size_t threshold_bytes) {
    std::scoped_lock lock(mu_);
    alert_threshold_ = threshold_bytes;
  }

  /**
   * @brief Check if memory usage exceeds threshold
   * @return true if alert threshold is exceeded
   */
  [[nodiscard]] bool check_alert() const {
    return total_allocated_bytes_ > alert_threshold_;
  }

  /**
   * @brief Generate a memory usage report
   * @return Formatted report string
   */
  [[nodiscard]] std::string generate_report() const {
    std::scoped_lock lock(mu_);

    // Load atomic values
    size_t total_bytes = total_allocated_bytes_.load();
    size_t peak_bytes = peak_allocated_bytes_.load();
    uint64_t alloc_count = total_allocation_count_.load();
    uint64_t dealloc_count = total_deallocation_count_.load();

    std::ostringstream oss;
    oss << "Memory Usage Report\n";
    oss << "==================\n";
    oss << std::format("Total Allocated: {} bytes ({:.2f} MB)\n",
                        total_bytes, total_bytes / 1024.0 / 1024.0);
    oss << std::format("Peak Allocated: {} bytes ({:.2f} MB)\n",
                        peak_bytes, peak_bytes / 1024.0 / 1024.0);
    oss << std::format("Total Allocations: {}\n", alloc_count);
    oss << std::format("Total Deallocations: {}\n", dealloc_count);
    oss << std::format("Active Sites: {}\n\n", sites_.size());

    oss << "Top Sites by Peak Usage:\n";
    std::vector<std::pair<std::string, size_t>> sorted_sites;
    for (const auto& [name, stats] : sites_) {
      sorted_sites.emplace_back(name, stats.peak_bytes);
    }
    std::sort(sorted_sites.begin(), sorted_sites.end(),
              [](const auto& a, const auto& b) { return a.second > b.second; });

    for (size_t i = 0; i < std::min(sorted_sites.size(), size_t(10)); ++i) {
      const auto& [name, bytes] = sorted_sites[i];
      const auto& stats = sites_.at(name);
      oss << std::format("  {:<30} {:>12} bytes ({:>6} allocs, {:>6} objects)\n",
                            name, bytes, stats.allocation_count, stats.object_count);
    }

    return oss.str();
  }

  /**
   * @brief Reset all statistics
   */
  void reset() {
    std::scoped_lock lock(mu_);
    sites_.clear();
    total_allocated_bytes_.store(0);
    peak_allocated_bytes_.store(0);
    total_allocation_count_.store(0);
    total_deallocation_count_.store(0);
  }

private:
  mutable std::mutex mu_;
  std::unordered_map<std::string, MemoryAllocationSite> sites_;
  std::atomic<size_t> total_allocated_bytes_{0};
  std::atomic<size_t> peak_allocated_bytes_{0};
  std::atomic<uint64_t> total_allocation_count_{0};
  std::atomic<uint64_t> total_deallocation_count_{0};
  std::atomic<size_t> alert_threshold_{1024 * 1024 * 1024}; // Default: 1GB
};

/**
 * @brief Global memory monitor accessor
 */
[[nodiscard]] MemoryMonitor& global_memory_monitor();

/**
 * @brief Helper class to track allocations using RAII
 */
template <typename T>
class MemoryTracker {
public:
  explicit MemoryTracker(const std::string& site_name)
      : site_name_(site_name), monitor_(&global_memory_monitor()) {}

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
  std::string site_name_;
  MemoryMonitor* monitor_;
};

} // namespace veloz::core
