#pragma once

#include <atomic>
#include <cstddef>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <vector>

namespace veloz::core {

// Memory alignment utilities
[[nodiscard]] void* aligned_alloc(size_t size, size_t alignment);
void aligned_free(void* ptr);

template <typename T> [[nodiscard]] T* aligned_new(size_t alignment = alignof(T)) {
  void* ptr = aligned_alloc(sizeof(T), alignment);
  if (ptr == nullptr) {
    throw std::bad_alloc();
  }
  return new (ptr) T();
}

template <typename T, typename... Args>
[[nodiscard]] T* aligned_new(size_t alignment, Args&&... args) {
  void* ptr = aligned_alloc(sizeof(T), alignment);
  if (ptr == nullptr) {
    throw std::bad_alloc();
  }
  return new (ptr) T(std::forward<Args>(args)...);
}

template <typename T> void aligned_delete(T* ptr) {
  if (ptr == nullptr) {
    return;
  }
  ptr->~T();
  aligned_free(ptr);
}

// Simple object pool implementation
template <typename T> class ObjectPool final {
public:
  explicit ObjectPool(size_t initial_size = 0, size_t max_size = 0)
      : max_size_(max_size), size_(0) {
    if (initial_size > 0) {
      for (size_t i = 0; i < initial_size; ++i) {
        pool_.push_back(std::make_unique<T>());
      }
      size_ = initial_size;
    }
  }

  ~ObjectPool() = default;

  // Acquire object from pool
  template <typename... Args> std::unique_ptr<T, std::function<void(T*)>> acquire(Args&&... args) {
    std::scoped_lock lock(mu_);

    if (!pool_.empty()) {
      auto ptr = pool_.back().release();
      pool_.pop_back();
      // Reset object state
      ptr->~T();
      new (ptr) T(std::forward<Args>(args)...);
      return std::unique_ptr<T, std::function<void(T*)>>(ptr, [this](T* p) { release(p); });
    }

    if (max_size_ == 0 || size_ < max_size_) {
      auto ptr = new T(std::forward<Args>(args)...);
      ++size_;
      return std::unique_ptr<T, std::function<void(T*)>>(ptr, [this](T* p) { release(p); });
    }

    throw std::runtime_error("Object pool exhausted");
  }

  // Release object back to pool
  void release(T* ptr) {
    if (ptr == nullptr) {
      return;
    }

    std::scoped_lock lock(mu_);

    if (max_size_ > 0 && pool_.size() >= max_size_) {
      delete ptr;
      --size_;
    } else {
      pool_.push_back(std::unique_ptr<T>(ptr));
    }
  }

  // Get pool status information
  [[nodiscard]] size_t available() const {
    std::scoped_lock lock(mu_);
    return pool_.size();
  }

  [[nodiscard]] size_t size() const {
    std::scoped_lock lock(mu_);
    return size_;
  }

  [[nodiscard]] size_t max_size() const noexcept {
    return max_size_;
  }

  // Preallocate objects
  void preallocate(size_t count) {
    std::scoped_lock lock(mu_);
    const size_t needed = count > pool_.size() ? count - pool_.size() : 0;
    for (size_t i = 0; i < needed; ++i) {
      if (max_size_ > 0 && size_ >= max_size_) {
        break;
      }
      pool_.push_back(std::make_unique<T>());
      ++size_;
    }
  }

  // Clear pool
  void clear() {
    std::scoped_lock lock(mu_);
    pool_.clear();
    size_ = 0;
  }

private:
  std::vector<std::unique_ptr<T>> pool_;
  mutable std::mutex mu_;
  const size_t max_size_;
  size_t size_;
};

// Thread-local object pool
template <typename T> class ThreadLocalObjectPool final {
public:
  explicit ThreadLocalObjectPool(size_t initial_size = 0, size_t max_size = 0)
      : initial_size_(initial_size), max_size_(max_size) {}

  ~ThreadLocalObjectPool() = default;

  // Acquire object from pool
  template <typename... Args> std::unique_ptr<T, std::function<void(T*)>> acquire(Args&&... args) {
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

    throw std::runtime_error("Thread local object pool exhausted");
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
