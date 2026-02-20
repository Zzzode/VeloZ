#include "kj/test.h"
#include "veloz/core/lockfree_queue.h"

#include <atomic>
#include <chrono>
#include <kj/string.h>
#include <kj/thread.h>
#include <kj/vector.h>
#include <thread>

using namespace veloz::core;

namespace {

// ============================================================================
// TaggedPtr Tests
// ============================================================================

KJ_TEST("TaggedPtr: Basic construction and access") {
  int value = 42;
  TaggedPtr<int> ptr(&value, 123);

  KJ_EXPECT(ptr.ptr() == &value);
  KJ_EXPECT(ptr.tag() == 123);
}

KJ_TEST("TaggedPtr: Default construction") {
  TaggedPtr<int> ptr;

  KJ_EXPECT(ptr.ptr() == nullptr);
  KJ_EXPECT(ptr.tag() == 0);
}

KJ_TEST("TaggedPtr: with_next_tag increments tag") {
  int value1 = 1;
  int value2 = 2;
  TaggedPtr<int> ptr(&value1, 100);

  auto next = ptr.with_next_tag(&value2);

  KJ_EXPECT(next.ptr() == &value2);
  KJ_EXPECT(next.tag() == 101);
}

KJ_TEST("TaggedPtr: Equality comparison") {
  int value = 42;
  TaggedPtr<int> ptr1(&value, 10);
  TaggedPtr<int> ptr2(&value, 10);
  TaggedPtr<int> ptr3(&value, 11);

  KJ_EXPECT(ptr1 == ptr2);
  KJ_EXPECT(ptr1 != ptr3);
}

// ============================================================================
// LockFreeNodePool Tests
// ============================================================================

KJ_TEST("LockFreeNodePool: Allocate and deallocate") {
  LockFreeNodePool<int> pool;

  auto* node = pool.allocate();
  KJ_EXPECT(node != nullptr);
  KJ_EXPECT(pool.allocated_count() == 1);

  pool.deallocate(node);
  KJ_EXPECT(pool.allocated_count() == 0);
}

KJ_TEST("LockFreeNodePool: Node reuse") {
  LockFreeNodePool<int> pool;

  auto* node1 = pool.allocate();
  pool.deallocate(node1);

  auto* node2 = pool.allocate();
  // Should reuse node1
  KJ_EXPECT(node2 == node1);
  KJ_EXPECT(pool.total_allocations() == 1);
}

KJ_TEST("LockFreeNodePool: Multiple allocations") {
  LockFreeNodePool<int> pool;

  kj::Vector<LockFreeNodePool<int>::Node*> nodes;
  for (int i = 0; i < 10; ++i) {
    nodes.add(pool.allocate());
  }

  KJ_EXPECT(pool.allocated_count() == 10);
  KJ_EXPECT(pool.total_allocations() == 10);

  for (auto* node : nodes) {
    pool.deallocate(node);
  }

  KJ_EXPECT(pool.allocated_count() == 0);
}

KJ_TEST("LockFreeNodePool: Construct and destroy value") {
  LockFreeNodePool<kj::String> pool;

  auto* node = pool.allocate();
  node->construct(kj::str("Hello, World!"));

  KJ_EXPECT(*node->get() == "Hello, World!"_kj);

  node->destroy();
  pool.deallocate(node);
}

// ============================================================================
// LockFreeQueue Tests
// ============================================================================

KJ_TEST("LockFreeQueue: Empty queue") {
  LockFreeQueue<int> queue;

  KJ_EXPECT(queue.empty());
  KJ_EXPECT(queue.size() == 0);

  auto empty_result = queue.pop();
  KJ_EXPECT(empty_result == kj::none);
}

KJ_TEST("LockFreeQueue: Push and pop single element") {
  LockFreeQueue<int> queue;

  queue.push(42);
  KJ_EXPECT(!queue.empty());
  KJ_EXPECT(queue.size() == 1);

  auto maybe_value = queue.pop();
  KJ_IF_SOME(value, maybe_value) {
    KJ_EXPECT(value == 42);
  }
  else {
    KJ_FAIL_EXPECT("value not found");
  }
  KJ_EXPECT(queue.empty());
}

KJ_TEST("LockFreeQueue: FIFO order") {
  LockFreeQueue<int> queue;

  queue.push(1);
  queue.push(2);
  queue.push(3);

  KJ_EXPECT(queue.size() == 3);

  auto maybe_v1 = queue.pop();
  KJ_IF_SOME(v1, maybe_v1) {
    KJ_EXPECT(v1 == 1);
  }
  else {
    KJ_FAIL_EXPECT("v1 not found");
  }

  auto maybe_v2 = queue.pop();
  KJ_IF_SOME(v2, maybe_v2) {
    KJ_EXPECT(v2 == 2);
  }
  else {
    KJ_FAIL_EXPECT("v2 not found");
  }

  auto maybe_v3 = queue.pop();
  KJ_IF_SOME(v3, maybe_v3) {
    KJ_EXPECT(v3 == 3);
  }
  else {
    KJ_FAIL_EXPECT("v3 not found");
  }

  KJ_EXPECT(queue.pop() == kj::none);
}

KJ_TEST("LockFreeQueue: Move semantics") {
  LockFreeQueue<kj::String> queue;

  kj::String str = kj::str("Hello");
  queue.push(kj::mv(str));

  auto maybe_result = queue.pop();
  KJ_IF_SOME(result, maybe_result) {
    KJ_EXPECT(result == "Hello"_kj);
  }
  else {
    KJ_FAIL_EXPECT("result not found");
  }
}

KJ_TEST("LockFreeQueue: Many elements") {
  LockFreeQueue<int> queue;

  constexpr int N = 1000;
  for (int i = 0; i < N; ++i) {
    queue.push(i);
  }

  KJ_EXPECT(queue.size() == N);

  for (int i = 0; i < N; ++i) {
    auto maybe_value = queue.pop();
    KJ_IF_SOME(value, maybe_value) {
      KJ_EXPECT(value == i);
    }
    else {
      KJ_FAIL_EXPECT("value not found");
    }
  }

  KJ_EXPECT(queue.empty());
}

// ============================================================================
// Concurrent Tests
// ============================================================================

KJ_TEST("LockFreeQueue: Concurrent producers") {
  LockFreeQueue<int> queue;

  std::atomic<int> counter{0};

  constexpr int NUM_THREADS = 4;
  constexpr int ITEMS_PER_THREAD = 1000;

  kj::Vector<kj::Own<kj::Thread>> threads;
  for (int t = 0; t < NUM_THREADS; ++t) {
    threads.add(kj::heap<kj::Thread>([&queue, &counter, t]() {
      for (int i = 0; i < ITEMS_PER_THREAD; ++i) {
        queue.push(t * ITEMS_PER_THREAD + i);
        counter.fetch_add(1, std::memory_order_relaxed);
      }
    }));
  }

  threads.clear(); // Joins all threads

  KJ_EXPECT(counter.load() == NUM_THREADS * ITEMS_PER_THREAD);
  KJ_EXPECT(queue.size() == NUM_THREADS * ITEMS_PER_THREAD);
}

KJ_TEST("LockFreeQueue: Concurrent consumers") {
  LockFreeQueue<int> queue;

  constexpr int TOTAL_ITEMS = 4000;
  for (int i = 0; i < TOTAL_ITEMS; ++i) {
    queue.push(i);
  }

  std::atomic<int> consumed{0};
  constexpr int NUM_THREADS = 4;

  kj::Vector<kj::Own<kj::Thread>> threads;
  for (int t = 0; t < NUM_THREADS; ++t) {
    threads.add(kj::heap<kj::Thread>([&queue, &consumed]() {
      while (true) {
        auto maybe_value = queue.pop();
        if (maybe_value == kj::none) {
          break;
        }
        consumed.fetch_add(1, std::memory_order_relaxed);
      }
    }));
  }

  threads.clear(); // Joins all threads

  KJ_EXPECT(consumed.load() == TOTAL_ITEMS);
  KJ_EXPECT(queue.empty());
}

KJ_TEST("LockFreeQueue: Concurrent producers and consumers") {
  LockFreeQueue<int> queue;

  std::atomic<int> produced{0};
  std::atomic<int> consumed{0};
  std::atomic<bool> done{false};

  constexpr int NUM_PRODUCERS = 2;
  constexpr int NUM_CONSUMERS = 2;
  constexpr int ITEMS_PER_PRODUCER = 1000;

  kj::Vector<kj::Own<kj::Thread>> producers;
  for (int t = 0; t < NUM_PRODUCERS; ++t) {
    producers.add(kj::heap<kj::Thread>([&queue, &produced]() {
      for (int i = 0; i < ITEMS_PER_PRODUCER; ++i) {
        queue.push(i);
        produced.fetch_add(1, std::memory_order_relaxed);
      }
    }));
  }

  kj::Vector<kj::Own<kj::Thread>> consumers;
  for (int t = 0; t < NUM_CONSUMERS; ++t) {
    consumers.add(kj::heap<kj::Thread>([&queue, &consumed, &done]() {
      while (!done.load(std::memory_order_acquire) || !queue.empty()) {
        auto maybe_value = queue.pop();
        if (maybe_value != kj::none) {
          consumed.fetch_add(1, std::memory_order_relaxed);
        } else {
          std::this_thread::yield();
        }
      }
    }));
  }

  producers.clear(); // Joins producers

  done.store(true, std::memory_order_release);

  consumers.clear(); // Joins consumers

  KJ_EXPECT(produced.load() == NUM_PRODUCERS * ITEMS_PER_PRODUCER);
  KJ_EXPECT(consumed.load() == NUM_PRODUCERS * ITEMS_PER_PRODUCER);
}

// ============================================================================
// Performance Benchmark (informational)
// ============================================================================

KJ_TEST("LockFreeQueue: Performance benchmark") {
  LockFreeQueue<int> queue;

  constexpr int ITERATIONS = 100000;

  // Measure push performance
  auto push_start = std::chrono::high_resolution_clock::now();
  for (int i = 0; i < ITERATIONS; ++i) {
    queue.push(i);
  }
  auto push_end = std::chrono::high_resolution_clock::now();

  auto push_duration =
      std::chrono::duration_cast<std::chrono::nanoseconds>(push_end - push_start).count();
  auto push_per_op = push_duration / ITERATIONS;

  // Measure pop performance
  auto pop_start = std::chrono::high_resolution_clock::now();
  for (int i = 0; i < ITERATIONS; ++i) {
    [[maybe_unused]] auto value = queue.pop();
  }
  auto pop_end = std::chrono::high_resolution_clock::now();

  auto pop_duration =
      std::chrono::duration_cast<std::chrono::nanoseconds>(pop_end - pop_start).count();
  auto pop_per_op = pop_duration / ITERATIONS;

  // Log performance (informational, not a pass/fail criterion)
  KJ_LOG(INFO, "Lock-free queue performance", push_per_op, "ns/push", pop_per_op, "ns/pop");

  // Soft check - should be reasonably fast (< 1000ns per operation)
  KJ_EXPECT(push_per_op < 1000);
  KJ_EXPECT(pop_per_op < 1000);
}

} // namespace
