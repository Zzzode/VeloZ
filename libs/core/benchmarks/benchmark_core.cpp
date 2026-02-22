/**
 * @file benchmark_core.cpp
 * @brief Performance benchmarks for VeloZ core components
 *
 * This file benchmarks:
 * 1. Event loop throughput (events/sec)
 * 2. JSON parsing performance
 * 3. JSON building performance
 * 4. Lock-free queue operations
 * 5. Memory pool allocations
 * 6. Arena allocator performance
 */

#include "benchmark_framework.h"
#include "veloz/core/event_loop.h"
#include "veloz/core/json.h"
#include "veloz/core/lockfree_queue.h"
#include "veloz/core/memory_pool.h"

#include <atomic>
#include <chrono>
#include <cstdio>
#include <kj/debug.h>
#include <kj/string.h>
#include <kj/thread.h>
#include <kj/vector.h>
#include <thread>

using namespace veloz::core;
using namespace veloz::core::benchmark;

namespace {

// ============================================================================
// Test Data
// ============================================================================

// Sample JSON for parsing benchmarks (typical market data message)
constexpr const char* SAMPLE_JSON_SMALL = R"({
  "symbol": "BTCUSDT",
  "price": "50000.50",
  "quantity": "1.5",
  "timestamp": 1640000000000
})";

constexpr const char* SAMPLE_JSON_MEDIUM = R"({
  "stream": "btcusdt@trade",
  "data": {
    "e": "trade",
    "E": 1640000000000,
    "s": "BTCUSDT",
    "t": 12345678,
    "p": "50000.50",
    "q": "1.5",
    "b": 88888888,
    "a": 99999999,
    "T": 1640000000000,
    "m": true,
    "M": true
  }
})";

constexpr const char* SAMPLE_JSON_LARGE = R"({
  "lastUpdateId": 1234567890,
  "bids": [
    ["50000.00", "1.0"],
    ["49999.00", "2.0"],
    ["49998.00", "3.0"],
    ["49997.00", "4.0"],
    ["49996.00", "5.0"],
    ["49995.00", "6.0"],
    ["49994.00", "7.0"],
    ["49993.00", "8.0"],
    ["49992.00", "9.0"],
    ["49991.00", "10.0"]
  ],
  "asks": [
    ["50001.00", "1.0"],
    ["50002.00", "2.0"],
    ["50003.00", "3.0"],
    ["50004.00", "4.0"],
    ["50005.00", "5.0"],
    ["50006.00", "6.0"],
    ["50007.00", "7.0"],
    ["50008.00", "8.0"],
    ["50009.00", "9.0"],
    ["50010.00", "10.0"]
  ]
})";

// ============================================================================
// JSON Parsing Benchmarks
// ============================================================================

BenchmarkResult benchmark_json_parse_small() {
  Benchmark bench("JSON Parse (small, 4 fields)");
  return bench.run(100000, [](uint64_t) {
    auto doc = JsonDocument::parse(SAMPLE_JSON_SMALL);
    auto root = doc.root();
    [[maybe_unused]] auto symbol = root["symbol"].get_string();
    [[maybe_unused]] auto price = root["price"].get_string();
  });
}

BenchmarkResult benchmark_json_parse_medium() {
  Benchmark bench("JSON Parse (medium, nested)");
  return bench.run(100000, [](uint64_t) {
    auto doc = JsonDocument::parse(SAMPLE_JSON_MEDIUM);
    auto root = doc.root();
    auto data = root["data"];
    [[maybe_unused]] auto symbol = data["s"].get_string();
    [[maybe_unused]] auto price = data["p"].get_string();
  });
}

BenchmarkResult benchmark_json_parse_large() {
  Benchmark bench("JSON Parse (large, arrays)");
  return bench.run(50000, [](uint64_t) {
    auto doc = JsonDocument::parse(SAMPLE_JSON_LARGE);
    auto root = doc.root();
    auto bids = root["bids"];
    [[maybe_unused]] auto size = bids.size();
    [[maybe_unused]] auto first_bid = bids[0][0].get_string();
  });
}

// ============================================================================
// JSON Building Benchmarks
// ============================================================================

BenchmarkResult benchmark_json_build_small() {
  Benchmark bench("JSON Build (small, 4 fields)");
  return bench.run(100000, [](uint64_t i) {
    auto builder = JsonBuilder::object();
    builder.put("symbol", "BTCUSDT")
        .put("price", 50000.50)
        .put("quantity", 1.5)
        .put("timestamp", static_cast<int64_t>(i));
    [[maybe_unused]] auto json = builder.build();
  });
}

BenchmarkResult benchmark_json_build_nested() {
  Benchmark bench("JSON Build (nested object)");
  return bench.run(50000, [](uint64_t i) {
    auto builder = JsonBuilder::object();
    builder.put("stream", "btcusdt@trade")
        .put_object("data", [i](JsonBuilder& data) {
          data.put("e", "trade")
              .put("s", "BTCUSDT")
              .put("p", "50000.50")
              .put("t", static_cast<int64_t>(i));
        });
    [[maybe_unused]] auto json = builder.build();
  });
}

// ============================================================================
// Lock-Free Queue Benchmarks
// ============================================================================

BenchmarkResult benchmark_lockfree_queue_push() {
  Benchmark bench("LockFreeQueue Push");
  LockFreeQueue<int> queue;

  return bench.run(1000000, [&queue](uint64_t i) { queue.push(static_cast<int>(i)); });
}

BenchmarkResult benchmark_lockfree_queue_pop() {
  Benchmark bench("LockFreeQueue Pop");
  LockFreeQueue<int> queue;

  // Pre-fill queue
  for (int i = 0; i < 1000000; ++i) {
    queue.push(i);
  }

  return bench.run(1000000, [&queue](uint64_t) { [[maybe_unused]] auto val = queue.pop(); });
}

BenchmarkResult benchmark_lockfree_queue_push_pop() {
  Benchmark bench("LockFreeQueue Push+Pop");
  LockFreeQueue<int> queue;

  return bench.run(500000, [&queue](uint64_t i) {
    queue.push(static_cast<int>(i));
    [[maybe_unused]] auto val = queue.pop();
  });
}

BenchmarkResult benchmark_lockfree_queue_concurrent() {
  Benchmark bench("LockFreeQueue Concurrent (4 threads)");
  LockFreeQueue<int> queue;

  constexpr int ITEMS_PER_ITER = 100;

  return bench.run(10000, [&queue](uint64_t) {
    std::atomic<int> produced{0};
    std::atomic<int> consumed{0};
    std::atomic<bool> done{false};

    // 2 producers
    kj::Vector<kj::Own<kj::Thread>> producers;
    for (int t = 0; t < 2; ++t) {
      producers.add(kj::heap<kj::Thread>([&queue, &produced]() {
        for (int i = 0; i < ITEMS_PER_ITER / 2; ++i) {
          queue.push(i);
          produced.fetch_add(1, std::memory_order_relaxed);
        }
      }));
    }

    // 2 consumers
    kj::Vector<kj::Own<kj::Thread>> consumers;
    for (int t = 0; t < 2; ++t) {
      consumers.add(kj::heap<kj::Thread>([&queue, &consumed, &done]() {
        while (!done.load(std::memory_order_acquire) || !queue.empty()) {
          auto val = queue.pop();
          if (val != kj::none) {
            consumed.fetch_add(1, std::memory_order_relaxed);
          } else {
            std::this_thread::yield();
          }
        }
      }));
    }

    producers.clear(); // Join producers
    done.store(true, std::memory_order_release);
    consumers.clear(); // Join consumers
  });
}

// ============================================================================
// Memory Pool Benchmarks
// ============================================================================

struct TestObject {
  int64_t id;
  double value;
  char data[64];
};

BenchmarkResult benchmark_memory_pool_allocate() {
  Benchmark bench("MemoryPool Allocate");
  FixedSizeMemoryPool<TestObject, 256> pool(4); // 4 blocks of 256 objects

  return bench.run(100000, [&pool](uint64_t) {
    auto obj = pool.create();
    obj->id = 42;
    // Object is automatically returned to pool when kj::Own goes out of scope
  });
}

BenchmarkResult benchmark_memory_pool_vs_heap() {
  Benchmark bench("Heap Allocate (comparison)");

  return bench.run(100000, [](uint64_t) {
    auto obj = kj::heap<TestObject>();
    obj->id = 42;
  });
}

// ============================================================================
// Arena Allocator Benchmarks
// ============================================================================

BenchmarkResult benchmark_arena_allocate() {
  Benchmark bench("ArenaAllocator Allocate");

  return bench.run(100000, [](uint64_t) {
    ArenaAllocator arena(4096);
    for (int i = 0; i < 10; ++i) {
      [[maybe_unused]] auto& obj = arena.allocate<TestObject>();
    }
    // All allocations freed when arena goes out of scope
  });
}

BenchmarkResult benchmark_arena_string_copy() {
  Benchmark bench("ArenaAllocator String Copy");

  return bench.run(100000, [](uint64_t) {
    ArenaAllocator arena(4096);
    for (int i = 0; i < 10; ++i) {
      [[maybe_unused]] auto str = arena.copyString("BTCUSDT@trade"_kj);
    }
  });
}

// ============================================================================
// Event Loop Benchmarks
// ============================================================================

BenchmarkResult benchmark_event_loop_post() {
  Benchmark bench("EventLoop Post (single thread)");

  return bench.run(10000, [](uint64_t) {
    auto loop = kj::heap<EventLoop>();
    std::atomic<int> counter{0};

    for (int i = 0; i < 100; ++i) {
      loop->post([&counter] { counter.fetch_add(1, std::memory_order_relaxed); });
    }

    EventLoop* loop_ptr = loop.get();
    {
      kj::Thread worker([loop_ptr] { loop_ptr->run(); });

      while (!loop->is_running()) {
        std::this_thread::sleep_for(std::chrono::microseconds(10));
      }
      // Wait for tasks to complete
      while (counter.load(std::memory_order_relaxed) < 100) {
        std::this_thread::sleep_for(std::chrono::microseconds(10));
      }
      loop->stop();
    }
  });
}

BenchmarkResult benchmark_event_loop_priority() {
  Benchmark bench("EventLoop Post with Priority");

  return bench.run(10000, [](uint64_t) {
    auto loop = kj::heap<EventLoop>();
    std::atomic<int> counter{0};

    // Mix of priorities
    for (int i = 0; i < 25; ++i) {
      loop->post([&counter] { counter.fetch_add(1, std::memory_order_relaxed); },
                 EventPriority::Low);
      loop->post([&counter] { counter.fetch_add(1, std::memory_order_relaxed); },
                 EventPriority::Normal);
      loop->post([&counter] { counter.fetch_add(1, std::memory_order_relaxed); },
                 EventPriority::High);
      loop->post([&counter] { counter.fetch_add(1, std::memory_order_relaxed); },
                 EventPriority::Critical);
    }

    EventLoop* loop_ptr = loop.get();
    {
      kj::Thread worker([loop_ptr] { loop_ptr->run(); });

      while (!loop->is_running()) {
        std::this_thread::sleep_for(std::chrono::microseconds(10));
      }
      while (counter.load(std::memory_order_relaxed) < 100) {
        std::this_thread::sleep_for(std::chrono::microseconds(10));
      }
      loop->stop();
    }
  });
}

// ============================================================================
// Main Benchmark Runner
// ============================================================================

void run_all_benchmarks() {
  BenchmarkSuite suite("VeloZ Core Components");

  KJ_LOG(INFO, "Starting performance benchmarks...\n");

  // JSON Parsing
  KJ_LOG(INFO, "Running JSON parsing benchmarks...");
  suite.add_result(benchmark_json_parse_small());
  suite.add_result(benchmark_json_parse_medium());
  suite.add_result(benchmark_json_parse_large());

  // JSON Building
  KJ_LOG(INFO, "Running JSON building benchmarks...");
  suite.add_result(benchmark_json_build_small());
  suite.add_result(benchmark_json_build_nested());

  // Lock-Free Queue
  KJ_LOG(INFO, "Running lock-free queue benchmarks...");
  suite.add_result(benchmark_lockfree_queue_push());
  suite.add_result(benchmark_lockfree_queue_pop());
  suite.add_result(benchmark_lockfree_queue_push_pop());
  suite.add_result(benchmark_lockfree_queue_concurrent());

  // Memory Pool
  KJ_LOG(INFO, "Running memory pool benchmarks...");
  suite.add_result(benchmark_memory_pool_allocate());
  suite.add_result(benchmark_memory_pool_vs_heap());

  // Arena Allocator
  KJ_LOG(INFO, "Running arena allocator benchmarks...");
  suite.add_result(benchmark_arena_allocate());
  suite.add_result(benchmark_arena_string_copy());

  // Event Loop
  KJ_LOG(INFO, "Running event loop benchmarks...");
  suite.add_result(benchmark_event_loop_post());
  suite.add_result(benchmark_event_loop_priority());

  // Generate and print report
  kj::String report = suite.generate_report();
  KJ_LOG(INFO, report.cStr());

  // Also print to stdout for easier capture
  std::printf("%s\n", report.cStr());
}

} // namespace

int main(int argc, char* argv[]) {
  (void)argc;
  (void)argv;

  try {
    run_all_benchmarks();
    return 0;
  } catch (const kj::Exception& e) {
    KJ_LOG(ERROR, "Benchmark failed with exception", e.getDescription());
    return 1;
  }
}
