#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <kj/common.h>
#include <kj/memory.h>
#include <utility> // std::move, std::forward

namespace veloz::core {

// Cache line size for alignment to prevent false sharing
inline constexpr size_t CACHE_LINE_SIZE = 64;

/**
 * @brief Tagged pointer to solve ABA problem in lock-free algorithms
 *
 * Uses the upper 16 bits for a generation tag and lower 48 bits for the pointer.
 * This is sufficient for x86-64 where only 48 bits are used for virtual addresses.
 */
template <typename T> class TaggedPtr {
public:
  TaggedPtr() noexcept : value_(0) {}

  explicit TaggedPtr(T* ptr, uint16_t tag = 0) noexcept
      : value_((static_cast<uint64_t>(tag) << 48) | (reinterpret_cast<uint64_t>(ptr) & PTR_MASK)) {}

  [[nodiscard]] T* ptr() const noexcept {
    return reinterpret_cast<T*>(value_ & PTR_MASK);
  }

  [[nodiscard]] uint16_t tag() const noexcept {
    return static_cast<uint16_t>(value_ >> 48);
  }

  [[nodiscard]] TaggedPtr with_next_tag(T* new_ptr) const noexcept {
    return TaggedPtr(new_ptr, static_cast<uint16_t>(tag() + 1));
  }

  [[nodiscard]] uint64_t raw() const noexcept {
    return value_;
  }

  bool operator==(const TaggedPtr& other) const noexcept {
    return value_ == other.value_;
  }

  bool operator!=(const TaggedPtr& other) const noexcept {
    return value_ != other.value_;
  }

private:
  static constexpr uint64_t PTR_MASK = 0x0000FFFFFFFFFFFF;
  uint64_t value_;
};

static_assert(sizeof(TaggedPtr<void>) == sizeof(uint64_t), "TaggedPtr must be 64 bits");

/**
 * @brief Lock-free node pool for efficient node allocation/deallocation
 *
 * Uses a lock-free freelist (Treiber stack) for node recycling.
 * Thread-safe for concurrent allocation and deallocation.
 */
template <typename T> class LockFreeNodePool {
public:
  struct Node {
    std::atomic<Node*> next{nullptr};
    alignas(T) unsigned char storage[sizeof(T)];

    template <typename... Args> T* construct(Args&&... args) {
      return new (storage) T(std::forward<Args>(args)...);
    }

    T* get() noexcept {
      return reinterpret_cast<T*>(storage);
    }

    const T* get() const noexcept {
      return reinterpret_cast<const T*>(storage);
    }

    void destroy() {
      get()->~T();
    }
  };

  LockFreeNodePool() = default;

  ~LockFreeNodePool() {
    // Free all nodes in the freelist
    Node* node = free_list_.load(std::memory_order_relaxed);
    while (node != nullptr) {
      Node* next = node->next.load(std::memory_order_relaxed);
      delete node;
      node = next;
    }
  }

  // Non-copyable, non-movable
  LockFreeNodePool(const LockFreeNodePool&) = delete;
  LockFreeNodePool& operator=(const LockFreeNodePool&) = delete;
  LockFreeNodePool(LockFreeNodePool&&) = delete;
  LockFreeNodePool& operator=(LockFreeNodePool&&) = delete;

  /**
   * @brief Allocate a node from the pool
   *
   * First tries to pop from the freelist, falls back to new allocation.
   * Thread-safe.
   */
  [[nodiscard]] Node* allocate() {
    Node* node = free_list_.load(std::memory_order_acquire);
    while (node != nullptr) {
      Node* next = node->next.load(std::memory_order_relaxed);
      if (free_list_.compare_exchange_weak(node, next, std::memory_order_acq_rel,
                                           std::memory_order_acquire)) {
        node->next.store(nullptr, std::memory_order_relaxed);
        allocated_count_.fetch_add(1, std::memory_order_relaxed);
        return node;
      }
      // CAS failed, node was updated, retry
    }

    // Freelist empty, allocate new node
    allocated_count_.fetch_add(1, std::memory_order_relaxed);
    total_allocations_.fetch_add(1, std::memory_order_relaxed);
    return new Node();
  }

  /**
   * @brief Return a node to the pool
   *
   * Pushes the node onto the freelist for reuse.
   * Thread-safe.
   */
  void deallocate(Node* node) {
    if (node == nullptr)
      return;

    Node* old_head = free_list_.load(std::memory_order_relaxed);
    do {
      node->next.store(old_head, std::memory_order_relaxed);
    } while (!free_list_.compare_exchange_weak(old_head, node, std::memory_order_release,
                                               std::memory_order_relaxed));

    allocated_count_.fetch_sub(1, std::memory_order_relaxed);
  }

  /**
   * @brief Get the number of currently allocated nodes
   */
  [[nodiscard]] size_t allocated_count() const noexcept {
    return allocated_count_.load(std::memory_order_relaxed);
  }

  /**
   * @brief Get the total number of allocations (including reused nodes)
   */
  [[nodiscard]] size_t total_allocations() const noexcept {
    return total_allocations_.load(std::memory_order_relaxed);
  }

private:
  std::atomic<Node*> free_list_{nullptr};
  std::atomic<size_t> allocated_count_{0};
  std::atomic<size_t> total_allocations_{0};
};

/**
 * @brief Lock-free MPMC (Multi-Producer Multi-Consumer) queue
 *
 * Based on the Michael-Scott queue algorithm with tagged pointers
 * to solve the ABA problem. Cache-line aligned head/tail to prevent
 * false sharing.
 *
 * Thread-safe for concurrent push and pop operations.
 */
template <typename T> class LockFreeQueue {
public:
  using NodePool = LockFreeNodePool<T>;
  using Node = typename NodePool::Node;

  LockFreeQueue() {
    // Create sentinel node
    Node* sentinel = node_pool_.allocate();
    sentinel->next.store(nullptr, std::memory_order_relaxed);

    TaggedPtr<Node> initial(sentinel, 0);
    head_.store(initial.raw(), std::memory_order_relaxed);
    tail_.store(initial.raw(), std::memory_order_relaxed);
  }

  ~LockFreeQueue() {
    // Drain the queue
    while (pop() != kj::none) {
    }

    // Free the sentinel
    TaggedPtr<Node> head = load_tagged(head_);
    if (head.ptr() != nullptr) {
      node_pool_.deallocate(head.ptr());
    }
  }

  // Non-copyable, non-movable
  LockFreeQueue(const LockFreeQueue&) = delete;
  LockFreeQueue& operator=(const LockFreeQueue&) = delete;
  LockFreeQueue(LockFreeQueue&&) = delete;
  LockFreeQueue& operator=(LockFreeQueue&&) = delete;

  /**
   * @brief Push a value onto the queue
   *
   * Thread-safe. Multiple producers can push concurrently.
   */
  template <typename U> void push(U&& value) {
    Node* node = node_pool_.allocate();
    node->construct(std::forward<U>(value));
    node->next.store(nullptr, std::memory_order_relaxed);

    while (true) {
      TaggedPtr<Node> tail = load_tagged(tail_);
      Node* tail_ptr = tail.ptr();
      Node* next = tail_ptr->next.load(std::memory_order_acquire);

      // Check if tail is still consistent
      uint64_t tail_raw = tail.raw();
      if (tail_raw == tail_.load(std::memory_order_acquire)) {
        if (next == nullptr) {
          // Try to link node at the end
          if (tail_ptr->next.compare_exchange_weak(next, node, std::memory_order_release,
                                                   std::memory_order_relaxed)) {
            // Successfully linked, try to swing tail
            TaggedPtr<Node> new_tail = tail.with_next_tag(node);
            uint64_t new_tail_raw = new_tail.raw();
            tail_.compare_exchange_strong(tail_raw, new_tail_raw, std::memory_order_release,
                                          std::memory_order_relaxed);
            size_.fetch_add(1, std::memory_order_relaxed);
            return;
          }
        } else {
          // Tail is lagging, try to advance it
          TaggedPtr<Node> new_tail = tail.with_next_tag(next);
          uint64_t new_tail_raw = new_tail.raw();
          tail_.compare_exchange_weak(tail_raw, new_tail_raw, std::memory_order_release,
                                      std::memory_order_relaxed);
        }
      }
    }
  }

  /**
   * @brief Pop a value from the queue
   *
   * Thread-safe. Multiple consumers can pop concurrently.
   * @return The popped value, or kj::none if queue is empty
   */
  [[nodiscard]] kj::Maybe<T> pop() {
    while (true) {
      TaggedPtr<Node> head = load_tagged(head_);
      TaggedPtr<Node> tail = load_tagged(tail_);
      Node* head_ptr = head.ptr();
      Node* next = head_ptr->next.load(std::memory_order_acquire);

      // Check if head is still consistent
      uint64_t head_raw = head.raw();
      if (head_raw == head_.load(std::memory_order_acquire)) {
        if (head_ptr == tail.ptr()) {
          if (next == nullptr) {
            // Queue is empty
            return kj::none;
          }
          // Tail is lagging, try to advance it
          TaggedPtr<Node> new_tail = tail.with_next_tag(next);
          uint64_t tail_raw = tail.raw();
          uint64_t new_tail_raw = new_tail.raw();
          tail_.compare_exchange_weak(tail_raw, new_tail_raw, std::memory_order_release,
                                      std::memory_order_relaxed);
        } else {
          // Read value before CAS, otherwise another dequeue might free the node
          T value = std::move(*next->get());

          // Try to swing head to the next node
          TaggedPtr<Node> new_head = head.with_next_tag(next);
          uint64_t new_head_raw = new_head.raw();
          if (head_.compare_exchange_weak(head_raw, new_head_raw, std::memory_order_release,
                                          std::memory_order_relaxed)) {
            // Successfully dequeued, return old head to pool
            node_pool_.deallocate(head_ptr);
            size_.fetch_sub(1, std::memory_order_relaxed);
            return value;
          }
        }
      }
    }
  }

  /**
   * @brief Check if the queue is empty
   *
   * Note: This is a snapshot and may be stale by the time it returns.
   */
  [[nodiscard]] bool empty() const noexcept {
    return size_.load(std::memory_order_relaxed) == 0;
  }

  /**
   * @brief Get the approximate size of the queue
   *
   * Note: This is a snapshot and may be stale by the time it returns.
   */
  [[nodiscard]] size_t size() const noexcept {
    return size_.load(std::memory_order_relaxed);
  }

  /**
   * @brief Get statistics about the node pool
   */
  [[nodiscard]] size_t pool_allocated_count() const noexcept {
    return node_pool_.allocated_count();
  }

  [[nodiscard]] size_t pool_total_allocations() const noexcept {
    return node_pool_.total_allocations();
  }

private:
  static TaggedPtr<Node> load_tagged(const std::atomic<uint64_t>& atomic) {
    uint64_t raw = atomic.load(std::memory_order_acquire);
    TaggedPtr<Node> result;
    std::memcpy(&result, &raw, sizeof(raw));
    return result;
  }

  // Cache-line aligned to prevent false sharing
  alignas(CACHE_LINE_SIZE) std::atomic<uint64_t> head_;
  alignas(CACHE_LINE_SIZE) std::atomic<uint64_t> tail_;
  alignas(CACHE_LINE_SIZE) std::atomic<size_t> size_{0};

  NodePool node_pool_;
};

} // namespace veloz::core
