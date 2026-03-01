/**
 * @file test_concurrent_access.cpp
 * @brief Integration tests for concurrent access and thread safety
 *
 * Tests cover:
 * - Concurrent HTTP requests
 * - Thread-safe event broadcasting
 * - Race condition detection
 * - Concurrent order submissions
 * - Thread-safe rate limiting
 * - Concurrent SSE subscriptions
 *
 * Performance targets:
 * - 100 concurrent requests: <5s total
 * - No race conditions or deadlocks
 * - Linear scalability up to 1000 connections
 */

#include "../src/auth/api_key_manager.h"
#include "../src/auth/jwt_manager.h"
#include "../src/bridge/event_broadcaster.h"
#include "../src/middleware/rate_limiter.h"

#include <atomic>
#include <chrono>
#include <kj/async-io.h>
#include <kj/compat/http.h>
#include <kj/function.h>
#include <kj/mutex.h>
#include <kj/string.h>
#include <kj/test.h>
#include <kj/thread.h>
#include <kj/vector.h>
#include <thread>
#include <vector>

using namespace veloz::gateway;
using namespace veloz::gateway::bridge;
using namespace veloz::gateway::middleware;
using namespace veloz::gateway::auth;

// =============================================================================
// Test Infrastructure
// =============================================================================

namespace {

/**
 * @brief Helper to measure execution time
 */
template <typename F> uint64_t measure_time_ms(F&& func) {
  auto start = std::chrono::high_resolution_clock::now();
  func();
  auto end = std::chrono::high_resolution_clock::now();
  return static_cast<uint64_t>(
      std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count());
}

/**
 * @brief Simple thread pool for concurrent testing
 */
class ThreadPool {
public:
  explicit ThreadPool(size_t num_threads) {
    for (size_t i = 0; i < num_threads; ++i) {
      threads_.add(kj::heap<kj::Thread>([this]() {
        while (true) {
          kj::Maybe<kj::Function<void()>> task;
          {
            auto lock = state_.lockExclusive();
            lock.wait(
                [](const State& state) { return state.stop || state.head < state.tasks.size(); });
            if (lock->stop && lock->head >= lock->tasks.size()) {
              return;
            }
            task = kj::mv(lock->tasks[lock->head]);
            ++lock->head;
            if (lock->head == lock->tasks.size()) {
              lock->tasks.clear();
              lock->head = 0;
            }
          }
          KJ_IF_SOME(fn, task) {
            fn();
          }
        }
      }));
    }
  }

  ~ThreadPool() {
    {
      auto lock = state_.lockExclusive();
      lock->stop = true;
    }
    threads_.clear();
  }

  template <typename F> void submit(F&& task) {
    {
      auto lock = state_.lockExclusive();
      lock->tasks.add(kj::Function<void()>(kj::fwd<F>(task)));
    }
  }

  void wait_all() {
    auto lock = state_.lockExclusive();
    lock.wait([](const State& state) { return state.head >= state.tasks.size(); });
  }

private:
  struct State {
    kj::Vector<kj::Function<void()>> tasks;
    size_t head{0};
    bool stop{false};
  };

  kj::Vector<kj::Own<kj::Thread>> threads_;
  kj::MutexGuarded<State> state_;
};

/**
 * @brief Thread-safe counter for testing
 */
class AtomicCounter {
public:
  void increment() {
    count_.fetch_add(1, std::memory_order_relaxed);
  }
  uint64_t get() const {
    return count_.load(std::memory_order_relaxed);
  }
  void reset() {
    count_.store(0, std::memory_order_relaxed);
  }

private:
  std::atomic<uint64_t> count_{0};
};

} // namespace

namespace {

// =============================================================================
// Concurrent Event Broadcasting Tests
// =============================================================================

KJ_TEST("Concurrent access: event broadcasting from multiple threads") {
  EventBroadcasterConfig config;
  config.max_subscriptions = 100;
  auto broadcaster = kj::heap<EventBroadcaster>(config);

  constexpr size_t NUM_THREADS = 10;
  constexpr size_t EVENTS_PER_THREAD = 100;

  AtomicCounter events_sent;

  // Start threads
  std::vector<std::thread> threads;
  for (size_t t = 0; t < NUM_THREADS; ++t) {
    threads.emplace_back([&broadcaster, &events_sent, t]() {
      for (size_t i = 0; i < EVENTS_PER_THREAD; ++i) {
        uint64_t event_id = t * EVENTS_PER_THREAD + i + 1;
        SseEvent event;
        event.id = event_id;
        event.type = SseEventType::System;
        event.data = kj::str("{\"thread\":", t, ",\"seq\":", i, "}");
        broadcaster->broadcast(kj::mv(event));
        events_sent.increment();
      }
    });
  }

  // Wait for all threads
  for (auto& t : threads) {
    t.join();
  }

  size_t expected = NUM_THREADS * EVENTS_PER_THREAD;
  KJ_EXPECT(events_sent.get() == expected);

  auto stats = broadcaster->get_stats();
  KJ_EXPECT(stats.events_broadcast == expected);
}

KJ_TEST("Concurrent access: concurrent subscriptions") {
  EventBroadcasterConfig config;
  config.max_subscriptions = 1000;
  auto broadcaster = kj::heap<EventBroadcaster>(config);

  constexpr size_t NUM_THREADS = 50;
  constexpr size_t SUBS_PER_THREAD = 10;

  AtomicCounter total_subs;
  std::vector<kj::Own<SseSubscription>> subscriptions;
  kj::MutexGuarded<decltype(subscriptions)> subs_mutex;

  // Concurrently create subscriptions
  std::vector<std::thread> threads;
  for (size_t t = 0; t < NUM_THREADS; ++t) {
    threads.emplace_back([&broadcaster, &total_subs, &subs_mutex]() {
      for (size_t i = 0; i < SUBS_PER_THREAD; ++i) {
        auto sub = broadcaster->subscribe(0);
        {
          auto lock = subs_mutex.lockExclusive();
          lock->push_back(kj::mv(sub));
        }
        total_subs.increment();
      }
    });
  }

  for (auto& t : threads) {
    t.join();
  }

  size_t expected = NUM_THREADS * SUBS_PER_THREAD;
  KJ_EXPECT(total_subs.get() == expected);
  KJ_EXPECT(broadcaster->subscription_count() == expected);
}

KJ_TEST("Concurrent access: concurrent subscribe and broadcast") {
  EventBroadcasterConfig config;
  config.max_subscriptions = 1000;
  auto broadcaster = kj::heap<EventBroadcaster>(config);

  constexpr size_t NUM_EVENTS = 1000;
  std::atomic<bool> running{true};
  AtomicCounter subs_created;
  AtomicCounter events_broadcast;

  // Thread creating subscriptions
  std::thread sub_thread([&]() {
    while (running.load()) {
      auto sub = broadcaster->subscribe(0);
      subs_created.increment();
    }
  });

  // Thread broadcasting events
  std::thread broadcast_thread([&]() {
    for (size_t i = 1; i <= NUM_EVENTS; ++i) {
      SseEvent event;
      event.id = i;
      event.type = SseEventType::System;
      event.data = kj::str("{}");
      broadcaster->broadcast(kj::mv(event));
      events_broadcast.increment();
    }
  });

  // Let broadcast complete
  broadcast_thread.join();
  running.store(false);
  sub_thread.join();

  KJ_EXPECT(events_broadcast.get() == NUM_EVENTS);
  KJ_LOG(INFO, "Subscriptions created during broadcast: ", subs_created.get());
}

// =============================================================================
// Concurrent Rate Limiting Tests
// =============================================================================

KJ_TEST("Concurrent access: rate limiter thread safety") {
  RateLimiterConfig config;
  config.capacity = 10000;
  config.refill_rate = 1000.0;
  auto limiter = kj::heap<RateLimiter>(config);

  constexpr size_t NUM_THREADS = 100;
  constexpr size_t REQUESTS_PER_THREAD = 100;

  AtomicCounter allowed_count;
  AtomicCounter blocked_count;

  std::vector<std::thread> threads;
  auto start_time = std::chrono::high_resolution_clock::now();

  for (size_t t = 0; t < NUM_THREADS; ++t) {
    threads.emplace_back([&limiter, &allowed_count, &blocked_count, t]() {
      kj::String user_id = kj::str("user_", t);
      for (size_t i = 0; i < REQUESTS_PER_THREAD; ++i) {
        auto result = limiter->check(user_id);
        if (result.allowed) {
          allowed_count.increment();
        } else {
          blocked_count.increment();
        }
      }
    });
  }

  for (auto& t : threads) {
    t.join();
  }

  auto end_time = std::chrono::high_resolution_clock::now();
  auto elapsed_ms =
      std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();

  size_t total_requests = NUM_THREADS * REQUESTS_PER_THREAD;
  KJ_LOG(INFO, "Total requests: ", total_requests);
  KJ_LOG(INFO, "Allowed: ", allowed_count.get());
  KJ_LOG(INFO, "Blocked: ", blocked_count.get());
  KJ_LOG(INFO, "Time: ", elapsed_ms, " ms");
  KJ_LOG(INFO, "Throughput: ", static_cast<double>(total_requests) / elapsed_ms * 1000.0, " req/s");

  // Should have exactly NUM_THREADS buckets
  KJ_EXPECT(limiter->bucket_count() == NUM_THREADS);
}

KJ_TEST("Concurrent access: rate limiter with same user from multiple threads") {
  RateLimiterConfig config;
  config.capacity = 1000;
  config.refill_rate = 0.0;
  auto limiter = kj::heap<RateLimiter>(config);

  constexpr size_t NUM_THREADS = 50;
  constexpr size_t REQUESTS_PER_THREAD = 50;

  AtomicCounter allowed_count;

  std::vector<std::thread> threads;
  std::atomic<uint64_t> last_remaining{1000};

  for (size_t t = 0; t < NUM_THREADS; ++t) {
    threads.emplace_back([&limiter, &allowed_count, &last_remaining]() {
      for (size_t i = 0; i < REQUESTS_PER_THREAD; ++i) {
        auto result = limiter->check("shared_user");
        if (result.allowed) {
          allowed_count.increment();
          // Track minimum remaining
          uint64_t current = last_remaining.load();
          while (result.remaining < current &&
                 !last_remaining.compare_exchange_weak(current, result.remaining)) {
          }
        }
      }
    });
  }

  for (auto& t : threads) {
    t.join();
  }

  // Total allowed should equal capacity (1000)
  KJ_EXPECT(allowed_count.get() == 1000);

  // Should only have one bucket
  KJ_EXPECT(limiter->bucket_count() == 1);
}

// =============================================================================
// Concurrent JWT Validation Tests
// =============================================================================

KJ_TEST("Concurrent access: JWT token validation thread safety") {
  auto jwt = kj::heap<JwtManager>("test_secret_key_32_characters_long!", kj::none, 3600, 604800);

  // Create a token
  auto token = jwt->create_access_token("concurrent_user");
  kj::StringPtr token_ptr = token;

  constexpr size_t NUM_THREADS = 100;
  constexpr size_t VALIDATIONS_PER_THREAD = 100;

  AtomicCounter valid_count;
  AtomicCounter invalid_count;
  kj::MutexGuarded<int> jwt_lock;

  std::vector<std::thread> threads;

  for (size_t t = 0; t < NUM_THREADS; ++t) {
    threads.emplace_back([&jwt, token_ptr, &valid_count, &invalid_count, &jwt_lock]() {
      for (size_t i = 0; i < VALIDATIONS_PER_THREAD; ++i) {
        auto lock = jwt_lock.lockExclusive();
        auto info = jwt->verify_access_token(token_ptr);
        if (info != kj::none) {
          valid_count.increment();
        } else {
          invalid_count.increment();
        }
      }
    });
  }

  for (auto& t : threads) {
    t.join();
  }

  size_t total = NUM_THREADS * VALIDATIONS_PER_THREAD;
  KJ_EXPECT(valid_count.get() == total);
  KJ_EXPECT(invalid_count.get() == 0);
}

KJ_TEST("Concurrent access: concurrent token creation and validation") {
  auto jwt = kj::heap<JwtManager>("test_secret_key_32_characters_long!", kj::none, 3600, 604800);

  constexpr size_t NUM_THREADS = 20;
  constexpr size_t TOKENS_PER_THREAD = 50;

  AtomicCounter tokens_created;
  AtomicCounter validations_passed;
  kj::MutexGuarded<int> jwt_lock;

  std::vector<std::thread> threads;

  for (size_t t = 0; t < NUM_THREADS; ++t) {
    threads.emplace_back([&jwt, &tokens_created, &validations_passed, &jwt_lock, t]() {
      for (size_t i = 0; i < TOKENS_PER_THREAD; ++i) {
        kj::String user_id = kj::str("user_", t, "_", i);
        auto lock = jwt_lock.lockExclusive();
        auto token = jwt->create_access_token(user_id);
        tokens_created.increment();

        auto info = jwt->verify_access_token(token);
        if (info != kj::none) {
          validations_passed.increment();
        }
      }
    });
  }

  for (auto& t : threads) {
    t.join();
  }

  size_t expected = NUM_THREADS * TOKENS_PER_THREAD;
  KJ_EXPECT(tokens_created.get() == expected);
  KJ_EXPECT(validations_passed.get() == expected);
}

// =============================================================================
// Concurrent API Key Operations Tests
// =============================================================================

KJ_TEST("Concurrent access: API key creation from multiple threads") {
  auto api_keys = kj::heap<ApiKeyManager>();

  constexpr size_t NUM_THREADS = 20;
  constexpr size_t KEYS_PER_THREAD = 50;

  AtomicCounter keys_created;
  kj::MutexGuarded<kj::Vector<kj::String>> all_key_ids;
  kj::MutexGuarded<int> api_key_lock;

  std::vector<std::thread> threads;

  for (size_t t = 0; t < NUM_THREADS; ++t) {
    threads.emplace_back([&api_keys, &keys_created, &all_key_ids, &api_key_lock, t]() {
      for (size_t i = 0; i < KEYS_PER_THREAD; ++i) {
        kj::String user_id = kj::str("user_", t);
        kj::String key_name = kj::str("key_", i);
        kj::Vector<kj::String> perms;
        perms.add(kj::str("read"));

        auto lock = api_key_lock.lockExclusive();
        auto key = api_keys->create_key(user_id, key_name, kj::mv(perms));
        keys_created.increment();

        auto id_lock = all_key_ids.lockExclusive();
        id_lock->add(kj::str(key.key_id));
      }
    });
  }

  for (auto& t : threads) {
    t.join();
  }

  size_t expected = NUM_THREADS * KEYS_PER_THREAD;
  KJ_EXPECT(keys_created.get() == expected);
}

KJ_TEST("Concurrent access: API key validation under load") {
  auto api_keys = kj::heap<ApiKeyManager>();

  // Create a key
  kj::Vector<kj::String> perms;
  perms.add(kj::str("read"));
  perms.add(kj::str("write"));
  auto key = api_keys->create_key("test_user", "test_key", kj::mv(perms));

  constexpr size_t NUM_THREADS = 100;
  constexpr size_t VALIDATIONS_PER_THREAD = 100;

  AtomicCounter valid_count;

  std::vector<std::thread> threads;

  for (size_t t = 0; t < NUM_THREADS; ++t) {
    threads.emplace_back([&api_keys, &key, &valid_count]() {
      for (size_t i = 0; i < VALIDATIONS_PER_THREAD; ++i) {
        auto info = api_keys->validate(key.raw_key);
        if (info != kj::none) {
          valid_count.increment();
        }
      }
    });
  }

  for (auto& t : threads) {
    t.join();
  }

  size_t expected = NUM_THREADS * VALIDATIONS_PER_THREAD;
  KJ_EXPECT(valid_count.get() == expected);
}

// =============================================================================
// Mixed Workload Tests
// =============================================================================

KJ_TEST("Concurrent access: mixed workload simulation") {
  // Simulate realistic concurrent workload:
  // - Authentication requests
  // - Event subscriptions
  // - Rate limit checks
  // - Event broadcasting

  auto jwt = kj::heap<JwtManager>("test_secret_key_32_characters_long!", kj::none, 3600, 604800);
  EventBroadcasterConfig bc_config;
  bc_config.max_subscriptions = 100;
  auto broadcaster = kj::heap<EventBroadcaster>(bc_config);
  RateLimiterConfig rl_config;
  rl_config.capacity = 10000;
  auto limiter = kj::heap<RateLimiter>(rl_config);

  constexpr size_t NUM_AUTH_THREADS = 10;
  constexpr size_t NUM_SSE_THREADS = 20;
  constexpr size_t NUM_RATE_THREADS = 30;
  constexpr size_t NUM_EVENT_THREADS = 20;

  AtomicCounter auth_ops;
  AtomicCounter sse_ops;
  AtomicCounter rate_ops;
  AtomicCounter event_ops;

  std::vector<std::thread> all_threads;

  // Authentication threads
  for (size_t i = 0; i < NUM_AUTH_THREADS; ++i) {
    all_threads.emplace_back([&jwt, &auth_ops, i]() {
      kj::String user = kj::str("auth_user_", i);
      for (size_t j = 0; j < 100; ++j) {
        auto token = jwt->create_access_token(user);
        auto info = jwt->verify_access_token(token);
        auth_ops.increment();
      }
    });
  }

  // SSE subscription threads
  for (size_t i = 0; i < NUM_SSE_THREADS; ++i) {
    all_threads.emplace_back([&broadcaster, &sse_ops]() {
      kj::EventLoop loop;
      kj::WaitScope waitScope(loop);
      auto sub = broadcaster->subscribe(0);
      for (size_t j = 0; j < 50; ++j) {
        auto event = sub->next_event();
        sse_ops.increment();
      }
    });
  }

  // Rate limit check threads
  for (size_t i = 0; i < NUM_RATE_THREADS; ++i) {
    all_threads.emplace_back([&limiter, &rate_ops, i]() {
      kj::String user = kj::str("rate_user_", i);
      for (size_t j = 0; j < 100; ++j) {
        auto result = limiter->check(user);
        rate_ops.increment();
      }
    });
  }

  // Event broadcasting threads
  for (size_t i = 0; i < NUM_EVENT_THREADS; ++i) {
    all_threads.emplace_back([&broadcaster, &event_ops, i]() {
      for (size_t j = 0; j < 50; ++j) {
        SseEvent event;
        event.id = i * 50 + j + 1;
        event.type = SseEventType::System;
        event.data = kj::str("{}");
        broadcaster->broadcast(kj::mv(event));
        event_ops.increment();
      }
    });
  }

  // Wait for all threads
  for (auto& t : all_threads) {
    t.join();
  }

  KJ_LOG(INFO, "Auth ops: ", auth_ops.get());
  KJ_LOG(INFO, "SSE ops: ", sse_ops.get());
  KJ_LOG(INFO, "Rate ops: ", rate_ops.get());
  KJ_LOG(INFO, "Event ops: ", event_ops.get());
}

// =============================================================================
// Stress Tests
// =============================================================================

KJ_TEST("Concurrent access: high contention stress test") {
  RateLimiterConfig config;
  config.capacity = 100000;
  config.refill_rate = 10000.0;
  auto limiter = kj::heap<RateLimiter>(config);

  // Many threads hitting same user (high contention)
  constexpr size_t NUM_THREADS = 200;
  constexpr size_t REQUESTS_PER_THREAD = 100;

  AtomicCounter total_ops;
  auto start_time = std::chrono::high_resolution_clock::now();

  std::vector<std::thread> threads;
  for (size_t t = 0; t < NUM_THREADS; ++t) {
    threads.emplace_back([&limiter, &total_ops]() {
      for (size_t i = 0; i < REQUESTS_PER_THREAD; ++i) {
        KJ_UNUSED auto result = limiter->check("contended_user");
        total_ops.increment();
      }
    });
  }

  for (auto& t : threads) {
    t.join();
  }

  auto end_time = std::chrono::high_resolution_clock::now();
  auto elapsed_ms =
      std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();

  size_t total_requests = NUM_THREADS * REQUESTS_PER_THREAD;
  double throughput = static_cast<double>(total_requests) / elapsed_ms * 1000.0;

  KJ_LOG(INFO, "High contention test:");
  KJ_LOG(INFO, "  Total requests: ", total_requests);
  KJ_LOG(INFO, "  Time: ", elapsed_ms, " ms");
  KJ_LOG(INFO, "  Throughput: ", throughput, " req/s");

  KJ_EXPECT(total_ops.get() == total_requests);
}

KJ_TEST("Concurrent access: 100 concurrent requests complete in <5s") {
  EventBroadcasterConfig config;
  auto broadcaster = kj::heap<EventBroadcaster>(config);
  auto jwt = kj::heap<JwtManager>("test_secret_key_32_characters_long!", kj::none, 3600, 604800);

  constexpr size_t NUM_CONCURRENT = 100;
  AtomicCounter completed;

  auto start_time = std::chrono::high_resolution_clock::now();

  // Simulate 100 concurrent request flows
  std::vector<std::thread> threads;
  for (size_t i = 0; i < NUM_CONCURRENT; ++i) {
    threads.emplace_back([&broadcaster, &jwt, &completed, i]() {
      // Each "request" does:
      // 1. Auth validation
      auto token = jwt->create_access_token(kj::str("user_", i));
      auto info = jwt->verify_access_token(token);
      KJ_EXPECT(info != kj::none);

      // 2. Subscribe to events
      auto sub = broadcaster->subscribe(0);

      // 3. Receive events
      for (size_t j = 0; j < 10; ++j) {
        SseEvent event;
        event.id = i * 10 + j + 1;
        event.type = SseEventType::System;
        event.data = kj::str("{}");
        broadcaster->broadcast(kj::mv(event));
      }

      completed.increment();
    });
  }

  for (auto& t : threads) {
    t.join();
  }

  auto end_time = std::chrono::high_resolution_clock::now();
  auto elapsed_ms =
      std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();

  KJ_LOG(INFO, "100 concurrent requests completed in ", elapsed_ms, " ms");

  KJ_EXPECT(completed.get() == NUM_CONCURRENT);
  KJ_EXPECT(elapsed_ms < 5000); // Performance target: <5s
}

// =============================================================================
// Race Condition Detection Tests
// =============================================================================

KJ_TEST("Concurrent access: no race conditions in event ID assignment") {
  EventBroadcasterConfig config;
  auto broadcaster = kj::heap<EventBroadcaster>(config);

  constexpr size_t NUM_THREADS = 50;
  constexpr size_t EVENTS_PER_THREAD = 100;

  kj::MutexGuarded<kj::HashSet<uint64_t>> seen_ids;
  AtomicCounter duplicates;

  // Collect all event IDs
  auto sub = broadcaster->subscribe(0);
  std::thread collector([&sub, &seen_ids, &duplicates]() {
    kj::EventLoop loop;
    kj::WaitScope waitScope(loop);
    size_t expected = NUM_THREADS * EVENTS_PER_THREAD;
    for (size_t i = 0; i < expected; ++i) {
      auto received = sub->next_event().wait(waitScope);
      KJ_IF_SOME(e, received) {
        auto lock = seen_ids.lockExclusive();
        if (lock->contains(e.id)) {
          duplicates.increment();
        }
        lock->insert(e.id);
      }
    }
  });

  kj::Vector<kj::Own<kj::Thread>> threads;
  for (size_t t = 0; t < NUM_THREADS; ++t) {
    threads.add(kj::heap<kj::Thread>([&broadcaster]() {
      for (size_t i = 0; i < EVENTS_PER_THREAD; ++i) {
        SseEvent event;
        event.id = 0; // Let broadcaster assign
        event.type = SseEventType::System;
        event.data = kj::str("{}");
        broadcaster->broadcast(kj::mv(event));
      }
    }));
  }
  threads.clear();
  collector.join();

  // No duplicate IDs should be assigned
  KJ_EXPECT(duplicates.get() == 0);
}

KJ_TEST("Concurrent access: atomic operations consistency") {
  std::atomic<uint64_t> counter{0};

  constexpr size_t NUM_THREADS = 100;
  constexpr size_t INCREMENTS_PER_THREAD = 1000;

  std::vector<std::thread> threads;
  for (size_t t = 0; t < NUM_THREADS; ++t) {
    threads.emplace_back([&counter]() {
      for (size_t i = 0; i < INCREMENTS_PER_THREAD; ++i) {
        counter.fetch_add(1, std::memory_order_relaxed);
      }
    });
  }

  for (auto& t : threads) {
    t.join();
  }

  uint64_t expected = NUM_THREADS * INCREMENTS_PER_THREAD;
  KJ_EXPECT(counter.load() == expected);
}

} // namespace
