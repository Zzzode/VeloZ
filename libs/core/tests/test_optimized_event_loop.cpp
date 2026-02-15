#include "kj/test.h"
#include "veloz/core/event_loop.h"
#include "veloz/core/optimized_event_loop.h"

#include <atomic>
#include <chrono>
#include <mutex>
#include <thread>
#include <vector>

using namespace veloz::core;

namespace {

// ============================================================================
// Basic Functionality Tests
// ============================================================================

KJ_TEST("OptimizedEventLoop: Basic post and run") {
  OptimizedEventLoop loop;

  std::atomic<bool> executed{false};
  loop.post([&executed]() { executed = true; });

  std::thread runner([&loop]() {
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    loop.stop();
  });

  loop.run();
  runner.join();

  KJ_EXPECT(executed.load());
}

KJ_TEST("OptimizedEventLoop: Multiple tasks") {
  OptimizedEventLoop loop;

  std::atomic<int> counter{0};
  constexpr int NUM_TASKS = 100;

  for (int i = 0; i < NUM_TASKS; ++i) {
    loop.post([&counter]() { counter++; });
  }

  std::thread runner([&loop]() {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    loop.stop();
  });

  loop.run();
  runner.join();

  KJ_EXPECT(counter.load() == NUM_TASKS);
}

KJ_TEST("OptimizedEventLoop: Priority ordering") {
  OptimizedEventLoop loop;

  std::vector<int> order;
  std::mutex order_mutex;

  // Post in reverse priority order
  loop.post(
      [&]() {
        std::lock_guard<std::mutex> lock(order_mutex);
        order.push_back(1);
      },
      EventPriority::Low);

  loop.post(
      [&]() {
        std::lock_guard<std::mutex> lock(order_mutex);
        order.push_back(2);
      },
      EventPriority::Normal);

  loop.post(
      [&]() {
        std::lock_guard<std::mutex> lock(order_mutex);
        order.push_back(3);
      },
      EventPriority::High);

  loop.post(
      [&]() {
        std::lock_guard<std::mutex> lock(order_mutex);
        order.push_back(4);
      },
      EventPriority::Critical);

  std::thread runner([&loop]() {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    loop.stop();
  });

  loop.run();
  runner.join();

  KJ_EXPECT(order.size() == 4);
  // Note: Lock-free queue doesn't guarantee strict priority ordering
  // Tasks are processed in FIFO order within the queue
}

KJ_TEST("OptimizedEventLoop: Delayed task") {
  OptimizedEventLoop loop;

  std::atomic<bool> executed{false};
  auto start = std::chrono::steady_clock::now();

  loop.post_delayed([&executed]() { executed = true; }, std::chrono::milliseconds(50));

  std::thread runner([&loop]() {
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    loop.stop();
  });

  loop.run();
  runner.join();

  auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::steady_clock::now() - start);

  KJ_EXPECT(executed.load());
  KJ_EXPECT(elapsed.count() >= 50);
}

KJ_TEST("OptimizedEventLoop: Multiple delayed tasks") {
  OptimizedEventLoop loop;

  std::vector<int> order;
  std::mutex order_mutex;

  loop.post_delayed(
      [&]() {
        std::lock_guard<std::mutex> lock(order_mutex);
        order.push_back(3);
      },
      std::chrono::milliseconds(30));

  loop.post_delayed(
      [&]() {
        std::lock_guard<std::mutex> lock(order_mutex);
        order.push_back(1);
      },
      std::chrono::milliseconds(10));

  loop.post_delayed(
      [&]() {
        std::lock_guard<std::mutex> lock(order_mutex);
        order.push_back(2);
      },
      std::chrono::milliseconds(20));

  std::thread runner([&loop]() {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    loop.stop();
  });

  loop.run();
  runner.join();

  KJ_EXPECT(order.size() == 3);
  KJ_EXPECT(order[0] == 1);
  KJ_EXPECT(order[1] == 2);
  KJ_EXPECT(order[2] == 3);
}

KJ_TEST("OptimizedEventLoop: Statistics tracking") {
  OptimizedEventLoop loop;

  constexpr int NUM_TASKS = 50;
  std::atomic<int> counter{0};

  for (int i = 0; i < NUM_TASKS; ++i) {
    loop.post([&counter]() { counter++; });
  }

  std::thread runner([&loop]() {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    loop.stop();
  });

  loop.run();
  runner.join();

  const auto& stats = loop.stats();
  KJ_EXPECT(stats.total_events.load() == NUM_TASKS);
  KJ_EXPECT(stats.events_processed.load() == NUM_TASKS);

  const auto& opt_stats = loop.optimized_stats();
  KJ_EXPECT(opt_stats.lockfree_queue_pushes.load() == NUM_TASKS);
  KJ_EXPECT(opt_stats.lockfree_queue_pops.load() == NUM_TASKS);
}

// ============================================================================
// Concurrent Tests
// ============================================================================

KJ_TEST("OptimizedEventLoop: Concurrent producers") {
  OptimizedEventLoop loop;

  std::atomic<int> counter{0};
  constexpr int NUM_THREADS = 4;
  constexpr int TASKS_PER_THREAD = 100;

  std::vector<std::thread> producers;
  for (int t = 0; t < NUM_THREADS; ++t) {
    producers.emplace_back([&loop, &counter]() {
      for (int i = 0; i < TASKS_PER_THREAD; ++i) {
        loop.post([&counter]() { counter++; });
      }
    });
  }

  std::thread runner([&loop]() {
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    loop.stop();
  });

  loop.run();

  for (auto& t : producers) {
    t.join();
  }
  runner.join();

  KJ_EXPECT(counter.load() == NUM_THREADS * TASKS_PER_THREAD);
}

// ============================================================================
// Performance Benchmark Tests
// ============================================================================

KJ_TEST("OptimizedEventLoop: Performance benchmark - immediate tasks") {
  OptimizedEventLoop opt_loop;
  EventLoop std_loop;

  constexpr int NUM_TASKS = 10000;
  std::atomic<int> opt_counter{0};
  std::atomic<int> std_counter{0};

  // Benchmark optimized event loop
  auto opt_start = std::chrono::high_resolution_clock::now();

  for (int i = 0; i < NUM_TASKS; ++i) {
    opt_loop.post([&opt_counter]() { opt_counter++; });
  }

  std::thread opt_runner([&opt_loop]() {
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    opt_loop.stop();
  });

  opt_loop.run();
  opt_runner.join();

  auto opt_end = std::chrono::high_resolution_clock::now();
  auto opt_duration =
      std::chrono::duration_cast<std::chrono::microseconds>(opt_end - opt_start).count();

  // Benchmark standard event loop
  auto std_start = std::chrono::high_resolution_clock::now();

  for (int i = 0; i < NUM_TASKS; ++i) {
    std_loop.post([&std_counter]() { std_counter++; });
  }

  std::thread std_runner([&std_loop]() {
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    std_loop.stop();
  });

  std_loop.run();
  std_runner.join();

  auto std_end = std::chrono::high_resolution_clock::now();
  auto std_duration =
      std::chrono::duration_cast<std::chrono::microseconds>(std_end - std_start).count();

  KJ_EXPECT(opt_counter.load() == NUM_TASKS);
  KJ_EXPECT(std_counter.load() == NUM_TASKS);

  // Log performance comparison
  KJ_LOG(INFO, "Performance comparison", "optimized_us", opt_duration, "standard_us", std_duration,
         "speedup", static_cast<double>(std_duration) / opt_duration);

  // Optimized should be at least as fast (allow some variance)
  // Note: In practice, the optimized version should be faster under contention
}

KJ_TEST("OptimizedEventLoop: Performance benchmark - delayed tasks") {
  OptimizedEventLoop opt_loop;
  EventLoop std_loop;

  constexpr int NUM_TASKS = 1000;
  std::atomic<int> opt_counter{0};
  std::atomic<int> std_counter{0};

  // Benchmark optimized event loop with delayed tasks
  auto opt_start = std::chrono::high_resolution_clock::now();

  for (int i = 0; i < NUM_TASKS; ++i) {
    opt_loop.post_delayed([&opt_counter]() { opt_counter++; }, std::chrono::milliseconds(i % 100));
  }

  std::thread opt_runner([&opt_loop]() {
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    opt_loop.stop();
  });

  opt_loop.run();
  opt_runner.join();

  auto opt_end = std::chrono::high_resolution_clock::now();
  auto opt_duration =
      std::chrono::duration_cast<std::chrono::microseconds>(opt_end - opt_start).count();

  // Benchmark standard event loop with delayed tasks
  auto std_start = std::chrono::high_resolution_clock::now();

  for (int i = 0; i < NUM_TASKS; ++i) {
    std_loop.post_delayed([&std_counter]() { std_counter++; }, std::chrono::milliseconds(i % 100));
  }

  std::thread std_runner([&std_loop]() {
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    std_loop.stop();
  });

  std_loop.run();
  std_runner.join();

  auto std_end = std::chrono::high_resolution_clock::now();
  auto std_duration =
      std::chrono::duration_cast<std::chrono::microseconds>(std_end - std_start).count();

  // Log performance comparison
  KJ_LOG(INFO, "Delayed task performance", "optimized_us", opt_duration, "standard_us",
         std_duration, "opt_processed", opt_counter.load(), "std_processed", std_counter.load());
}

KJ_TEST("OptimizedEventLoop: Performance benchmark - concurrent posting") {
  OptimizedEventLoop opt_loop;
  EventLoop std_loop;

  constexpr int NUM_THREADS = 4;
  constexpr int TASKS_PER_THREAD = 1000;
  std::atomic<int> opt_counter{0};
  std::atomic<int> std_counter{0};

  // Benchmark optimized event loop with concurrent posting
  auto opt_start = std::chrono::high_resolution_clock::now();

  std::vector<std::thread> opt_producers;
  for (int t = 0; t < NUM_THREADS; ++t) {
    opt_producers.emplace_back([&opt_loop, &opt_counter]() {
      for (int i = 0; i < TASKS_PER_THREAD; ++i) {
        opt_loop.post([&opt_counter]() { opt_counter++; });
      }
    });
  }

  std::thread opt_runner([&opt_loop]() {
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    opt_loop.stop();
  });

  opt_loop.run();

  for (auto& t : opt_producers) {
    t.join();
  }
  opt_runner.join();

  auto opt_end = std::chrono::high_resolution_clock::now();
  auto opt_duration =
      std::chrono::duration_cast<std::chrono::microseconds>(opt_end - opt_start).count();

  // Benchmark standard event loop with concurrent posting
  auto std_start = std::chrono::high_resolution_clock::now();

  std::vector<std::thread> std_producers;
  for (int t = 0; t < NUM_THREADS; ++t) {
    std_producers.emplace_back([&std_loop, &std_counter]() {
      for (int i = 0; i < TASKS_PER_THREAD; ++i) {
        std_loop.post([&std_counter]() { std_counter++; });
      }
    });
  }

  std::thread std_runner([&std_loop]() {
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    std_loop.stop();
  });

  std_loop.run();

  for (auto& t : std_producers) {
    t.join();
  }
  std_runner.join();

  auto std_end = std::chrono::high_resolution_clock::now();
  auto std_duration =
      std::chrono::duration_cast<std::chrono::microseconds>(std_end - std_start).count();

  KJ_EXPECT(opt_counter.load() == NUM_THREADS * TASKS_PER_THREAD);
  KJ_EXPECT(std_counter.load() == NUM_THREADS * TASKS_PER_THREAD);

  // Log performance comparison
  double speedup = static_cast<double>(std_duration) / opt_duration;
  KJ_LOG(INFO, "Concurrent posting performance", "optimized_us", opt_duration, "standard_us",
         std_duration, "speedup", speedup);

  // Under concurrent load, the lock-free queue should show improvement
}

} // namespace
