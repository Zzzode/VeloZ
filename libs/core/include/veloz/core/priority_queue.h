#pragma once

#include <kj/vector.h>

KJ_BEGIN_HEADER

namespace veloz::core {

/**
 * @brief KJ-native priority queue implementation
 *
 * A max-heap based priority queue using kj::Vector as underlying storage.
 * Provides O(1) access to top element, O(log n) insertion and extraction.
 *
 * Works with move-only types (like kj::Vector) by using move semantics
 * for heap operations.
 *
 * @tparam T The element type
 * @tparam Compare The comparison function type
 *
 * @note This implementation uses kj::Vector instead of std::vector to maintain
 *       consistency with KJ-first policy in VeloZ.
 * @note Uses a max-heap where larger elements are at the top, matching
 *       Task::operator> semantics where higher-priority tasks execute first.
 */
template <typename T> class PriorityQueue {
public:
  using value_type = T;
  using size_type = size_t;

  /**
   * @brief Comparator function type
   * Returns true if 'a' should come before 'b' in the queue
   */
  using Comparator = bool (*)(const T& a, const T& b);

  explicit PriorityQueue(Comparator comp = [](const T& a, const T& b) { return a > b; })
      : comp_(comp) {}

  // Copy operations - not supported since kj::Vector is not copyable
  PriorityQueue(const PriorityQueue&) = delete;
  PriorityQueue& operator=(const PriorityQueue&) = delete;

  // Move operations
  PriorityQueue(PriorityQueue&&) = default;
  PriorityQueue& operator=(PriorityQueue&&) = default;

  /**
   * @brief Returns true if queue is empty
   */
  [[nodiscard]] bool empty() const {
    return data_.empty();
  }

  /**
   * @brief Returns number of elements in the queue
   */
  [[nodiscard]] size_type size() const {
    return data_.size();
  }

  /**
   * @brief Returns a reference to the underlying vector (read-only)
   * Allows iteration for counting operations without copying.
   */
  [[nodiscard]] const kj::Vector<T>& data() const {
    return data_;
  }

  /**
   * @brief Returns the top element (maximum in max-heap)
   * @pre !empty()
   */
  [[nodiscard]] const T& top() const {
    KJ_IREQUIRE(!empty(), "PriorityQueue is empty");
    return data_[0];
  }

  /**
   * @brief Insert a new element
   * Complexity: O(log n)
   */
  void push(T value) {
    size_t index = data_.size();
    data_.add(kj::mv(value));
    sift_up(index);
  }

  /**
   * @brief Remove the top element
   * Complexity: O(log n)
   * @pre !empty()
   */
  void pop() {
    popImpl();
  }

  /**
   * @brief Remove and return the top element
   * Complexity: O(log n)
   * @pre !empty()
   * @return The moved top element
   */
  T popValue() {
    return popImpl();
  }

  /**
   * @brief Remove all elements
   */
  void clear() {
    data_.clear();
  }

private:
  kj::Vector<T> data_;
  Comparator comp_;

  // Forward declarations of private methods
  T popImpl();
  void sift_up(size_t index);
  void sift_down(size_t index);
};

// ================================================================================
// Private method implementations
// ================================================================================

template <typename T> T PriorityQueue<T>::popImpl() {
  KJ_IREQUIRE(!empty(), "PriorityQueue is empty");

  size_t last = data_.size() - 1;

  // Move top element out before rearranging
  T result = kj::mv(data_[0]);

  // Move last element to root
  if (last != 0) {
    data_[0] = kj::mv(data_[last]);
  }

  // Remove last element
  data_.removeLast();

  // Restore heap property
  if (!empty()) {
    sift_down(0);
  }

  return result;
}

template <typename T> void PriorityQueue<T>::sift_up(size_t index) {
  while (index > 0) {
    size_t parent = (index - 1) / 2;

    if (comp_(data_[index], data_[parent])) {
      // Three-way swap: temp <- parent <- child <- temp
      // Since we can't copy, we use move + move assignment
      T temp = kj::mv(data_[parent]);
      data_[parent] = kj::mv(data_[index]);
      data_[index] = kj::mv(temp);

      index = parent;
    } else {
      break;
    }
  }
}

template <typename T> void PriorityQueue<T>::sift_down(size_t index) {
  size_t size = data_.size();

  while (true) {
    size_t left_child = 2 * index + 1;
    size_t right_child = 2 * index + 2;
    size_t largest = index;

    if (left_child < size && comp_(data_[left_child], data_[largest])) {
      largest = left_child;
    }
    if (right_child < size && comp_(data_[right_child], data_[largest])) {
      largest = right_child;
    }

    if (largest != index) {
      // Three-way swap: temp <- index <- largest <- temp
      T temp = kj::mv(data_[index]);
      data_[index] = kj::mv(data_[largest]);
      data_[largest] = kj::mv(temp);

      index = largest;
    } else {
      break;
    }
  }
}

} // namespace veloz::core

KJ_END_HEADER
