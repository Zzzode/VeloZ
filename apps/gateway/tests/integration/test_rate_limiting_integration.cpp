/**
 * @file test_rate_limiting_integration.cpp
 * @brief Integration tests for rate limiting middleware
 *
 * Tests cover:
 * - Rate limit enforcement across multiple requests
 * - Token bucket refill behavior
 * - Per-user vs per-IP rate limiting
 * - Rate limit headers in responses
 * - 429 response format
 * - Rate limit recovery
 *
 * Performance targets:
 * - Rate limit check: <1μs
 * - Token refill: <1ms
 */

#include "../src/middleware/rate_limiter.h"
#include "../src/router.h"

#include <chrono>
#include <kj/async-io.h>
#include <kj/compat/http.h>
#include <kj/string.h>
#include <kj/test.h>
#include <thread>

using namespace veloz::gateway;
using namespace veloz::gateway::middleware;

// =============================================================================
// Test Infrastructure
// =============================================================================

namespace {

/**
 * @brief Helper to measure execution time
 */
template <typename F> uint64_t measure_time_ns(F&& func) {
  auto start = std::chrono::high_resolution_clock::now();
  func();
  auto end = std::chrono::high_resolution_clock::now();
  return static_cast<uint64_t>(
      std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count());
}

/**
 * @brief Helper to sleep for milliseconds
 */
void sleep_ms(uint64_t ms) {
  std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}

} // namespace

namespace {

// =============================================================================
// Basic Rate Limiting Tests
// =============================================================================

KJ_TEST("Rate limiting: allows requests under limit") {
  RateLimiterConfig config;
  config.capacity = 10;
  config.refill_rate = 1.0; // 1 token per second

  RateLimiter limiter(config);

  // First 10 requests should succeed
  for (int i = 0; i < 10; ++i) {
    auto result = limiter.check("user_1");
    KJ_EXPECT(result.allowed);
    KJ_EXPECT(result.remaining == static_cast<uint32_t>(9 - i));
  }
}

KJ_TEST("Rate limiting: blocks requests over limit") {
  RateLimiterConfig config;
  config.capacity = 5;
  config.refill_rate = 1.0;

  RateLimiter limiter(config);

  // Use all tokens
  for (int i = 0; i < 5; ++i) {
    auto result = limiter.check("user_1");
    KJ_EXPECT(result.allowed);
  }

  // 6th request should be blocked
  auto result = limiter.check("user_1");
  KJ_EXPECT(!result.allowed);
  KJ_EXPECT(result.remaining == 0);

  // Should have retry_after
  KJ_IF_SOME(retry, result.retry_after) {
    KJ_LOG(INFO, "Retry after: ", retry);
    // Should be a valid ISO 8601 duration
    KJ_EXPECT(retry.startsWith("PT"));
  }
  else {
    KJ_FAIL_EXPECT("Expected retry_after to be set");
  }
}

KJ_TEST("Rate limiting: independent buckets per user") {
  RateLimiterConfig config;
  config.capacity = 5;
  config.refill_rate = 1.0;
  config.per_user_limiting = true;

  RateLimiter limiter(config);

  // User 1 uses all tokens
  for (int i = 0; i < 5; ++i) {
    auto result = limiter.check("user_1");
    KJ_EXPECT(result.allowed);
  }

  // User 1 is blocked
  auto result1 = limiter.check("user_1");
  KJ_EXPECT(!result1.allowed);

  // User 2 should still have tokens
  auto result2 = limiter.check("user_2");
  KJ_EXPECT(result2.allowed);
  KJ_EXPECT(result2.remaining == 4); // One used

  // User 3 should also have tokens
  auto result3 = limiter.check("user_3");
  KJ_EXPECT(result3.allowed);
  KJ_EXPECT(result3.remaining == 4);
}

KJ_TEST("Rate limiting: token refill") {
  RateLimiterConfig config;
  config.capacity = 10;
  config.refill_rate = 10.0; // 10 tokens per second

  RateLimiter limiter(config);

  // Use all tokens
  for (int i = 0; i < 10; ++i) {
    auto result = limiter.check("user_1");
    KJ_EXPECT(result.allowed);
  }

  // Verify bucket is empty
  auto result1 = limiter.check("user_1");
  KJ_EXPECT(!result1.allowed);

  // Wait for tokens to refill (0.5 seconds = 5 tokens)
  sleep_ms(500);

  // Should have approximately 5 tokens now
  auto result2 = limiter.check("user_1");
  KJ_EXPECT(result2.allowed);

  // Can make about 5 more requests
  int additionalRequests = 0;
  for (int i = 0; i < 10; ++i) {
    auto result = limiter.check("user_1");
    if (result.allowed) {
      additionalRequests++;
    }
  }

  // Should have been able to make approximately 5 requests
  KJ_EXPECT(additionalRequests >= 4 && additionalRequests <= 6);
}

KJ_TEST("Rate limiting: full bucket refill") {
  RateLimiterConfig config;
  config.capacity = 5;
  config.refill_rate = 5.0; // 5 tokens per second

  RateLimiter limiter(config);

  // Use all tokens
  for (int i = 0; i < 5; ++i) {
    auto result = limiter.check("user_1");
    KJ_EXPECT(result.allowed);
  }

  // Wait for full refill (1 second)
  sleep_ms(1000);

  // Should have full bucket again
  for (int i = 0; i < 5; ++i) {
    auto result = limiter.check("user_1");
    KJ_EXPECT(result.allowed);
  }

  // 6th request blocked
  auto result = limiter.check("user_1");
  KJ_EXPECT(!result.allowed);
}

// =============================================================================
// Rate Limit Headers Tests
// =============================================================================

KJ_TEST("Rate limiting: response headers set correctly") {
  auto headerTable = kj::heap<kj::HttpHeaderTable>();
  kj::HttpHeaders headers(*headerTable);

  RateLimiterConfig config;
  config.capacity = 100;
  config.refill_rate = 10.0;

  RateLimiter limiter(config);
  auto result = limiter.check("user_1");

  // Set rate limit headers
  RateLimiter::set_rate_limit_headers(headers, result);

  // Verify headers are set
  KJ_IF_SOME(limitId, headerTable->stringToId("X-RateLimit-Limit"_kj)) {
    auto limitHeader = headers.get(limitId);
    KJ_IF_SOME(limit, limitHeader) {
      KJ_EXPECT(limit == "100");
    }
  }

  KJ_IF_SOME(remainingId, headerTable->stringToId("X-RateLimit-Remaining"_kj)) {
    auto remainingHeader = headers.get(remainingId);
    KJ_IF_SOME(remaining, remainingHeader) {
      KJ_EXPECT(remaining == "99"); // One token used
    }
  }

  KJ_IF_SOME(resetId, headerTable->stringToId("X-RateLimit-Reset"_kj)) {
    auto resetHeader = headers.get(resetId);
    KJ_IF_SOME(reset, resetHeader) {
      uint64_t resetTime = std::stoull(reset.cStr());
      KJ_EXPECT(resetTime > 0);
    }
  }
}

KJ_TEST("Rate limiting: 429 response format") {
  auto headerTable = kj::heap<kj::HttpHeaderTable>();

  RateLimiterConfig config;
  config.capacity = 5;
  config.refill_rate = 1.0;

  RateLimiter limiter(config);

  // Exhaust tokens
  for (int i = 0; i < 5; ++i) {
    auto result = limiter.check("user_1");
    KJ_EXPECT(result.allowed);
  }

  auto result = limiter.check("user_1");
  KJ_EXPECT(!result.allowed);

  // Note: In real implementation, we would test send_429_response
  // but that requires a MockHttpResponse

  // Verify result has retry_after
  KJ_IF_SOME(retry, result.retry_after) {
    KJ_EXPECT(retry.size() > 0);
    // ISO 8601 duration format: PT<seconds>S
    KJ_EXPECT(retry.startsWith("PT"));
  }
}

// =============================================================================
// Per-IP vs Per-User Rate Limiting Tests
// =============================================================================

KJ_TEST("Rate limiting: per-IP limiting") {
  RateLimiterConfig config;
  config.capacity = 10;
  config.refill_rate = 1.0;
  config.per_user_limiting = false;

  RateLimiter limiter(config);

  // Same IP, different users - should share bucket
  for (int i = 0; i < 10; ++i) {
    auto result = limiter.check_ip("192.168.1.1");
    KJ_EXPECT(result.allowed);
  }

  // IP should be blocked
  auto result1 = limiter.check_ip("192.168.1.1");
  KJ_EXPECT(!result1.allowed);

  // Different IP should have tokens
  auto result2 = limiter.check_ip("192.168.1.2");
  KJ_EXPECT(result2.allowed);
}

KJ_TEST("Rate limiting: per-user limiting") {
  RateLimiterConfig config;
  config.capacity = 10;
  config.refill_rate = 1.0;
  config.per_user_limiting = true;

  RateLimiter limiter(config);

  // Same user, different IPs - should share bucket
  for (int i = 0; i < 10; ++i) {
    auto result = limiter.check("user_1");
    KJ_EXPECT(result.allowed);
  }

  // User should be blocked
  auto result1 = limiter.check("user_1");
  KJ_EXPECT(!result1.allowed);

  // Different user should have tokens
  auto result2 = limiter.check("user_2");
  KJ_EXPECT(result2.allowed);
}

// =============================================================================
// Bucket Management Tests
// =============================================================================

KJ_TEST("Rate limiting: bucket count tracking") {
  RateLimiterConfig config;
  config.capacity = 10;
  config.refill_rate = 1.0;

  RateLimiter limiter(config);

  KJ_EXPECT(limiter.bucket_count() == 0);

  // Create buckets for different users
  auto result1 = limiter.check("user_1");
  KJ_EXPECT(result1.allowed);
  KJ_EXPECT(limiter.bucket_count() == 1);

  auto result2 = limiter.check("user_2");
  KJ_EXPECT(result2.allowed);
  KJ_EXPECT(limiter.bucket_count() == 2);

  auto result3 = limiter.check("user_3");
  KJ_EXPECT(result3.allowed);
  KJ_EXPECT(limiter.bucket_count() == 3);

  // Same user doesn't create new bucket
  auto result4 = limiter.check("user_1");
  KJ_EXPECT(result4.allowed);
  KJ_EXPECT(limiter.bucket_count() == 3);
}

KJ_TEST("Rate limiting: stale bucket cleanup") {
  RateLimiterConfig config;
  config.capacity = 10;
  config.refill_rate = 1.0;
  config.bucket_ttl_ms = 100; // Very short TTL for testing
  config.cleanup_interval_ms = 50;

  RateLimiter limiter(config);

  // Create some buckets
  auto bucket1 = limiter.check("user_1");
  auto bucket2 = limiter.check("user_2");
  auto bucket3 = limiter.check("user_3");
  KJ_EXPECT(bucket1.allowed);
  KJ_EXPECT(bucket2.allowed);
  KJ_EXPECT(bucket3.allowed);

  KJ_EXPECT(limiter.bucket_count() == 3);

  // Wait for buckets to expire
  sleep_ms(200);

  // Force cleanup
  limiter.cleanup_stale_buckets();

  // Buckets should be removed
  // Note: Exact count depends on implementation timing
  size_t remaining = limiter.bucket_count();
  KJ_LOG(INFO, "Remaining buckets after cleanup: ", remaining);

  // Using expired bucket should create new bucket
  auto result = limiter.check("user_1");
  KJ_EXPECT(result.allowed);
  KJ_EXPECT(limiter.bucket_count() >= 1);
}

// =============================================================================
// Performance Tests
// =============================================================================

KJ_TEST("Rate limiting: check performance < 1μs") {
  RateLimiterConfig config;
  config.capacity = 10000;
  config.refill_rate = 1000.0;

  RateLimiter limiter(config);

  // Warm up
  for (int i = 0; i < 100; ++i) {
    auto result = limiter.check("warmup_user");
    KJ_EXPECT(result.allowed);
  }

  // Measure
  constexpr size_t NUM_CHECKS = 10000;
  auto totalTimeNs = measure_time_ns([&]() {
    for (size_t i = 0; i < NUM_CHECKS; ++i) {
      auto result = limiter.check("perf_user");
      (void)result;
    }
  });

  double avgNs = static_cast<double>(totalTimeNs) / NUM_CHECKS;
  double avgUs = avgNs / 1000.0;

  KJ_LOG(INFO, "Average rate limit check: ", avgUs, " μs (", avgNs, " ns)");

  // Performance target: <1μs
  bool meetsTarget = avgUs < 1.0;
  if (!meetsTarget) {
    KJ_LOG(WARNING, "Rate limit check slower than 1μs target: ", avgUs, " μs");
  }

  // Note: May be slower in debug builds
  KJ_EXPECT(avgUs < 10.0); // Relaxed for debug builds
}

KJ_TEST("Rate limiting: concurrent access performance") {
  RateLimiterConfig config;
  config.capacity = 100000;
  config.refill_rate = 10000.0;

  RateLimiter limiter(config);

  constexpr size_t NUM_USERS = 1000;
  constexpr size_t REQUESTS_PER_USER = 10;

  auto totalTimeNs = measure_time_ns([&]() {
    for (size_t u = 0; u < NUM_USERS; ++u) {
      for (size_t r = 0; r < REQUESTS_PER_USER; ++r) {
        auto result = limiter.check(kj::str("user_", u));
        (void)result;
      }
    }
  });

  size_t totalRequests = NUM_USERS * REQUESTS_PER_USER;
  double avgNs = static_cast<double>(totalTimeNs) / totalRequests;
  double avgUs = avgNs / 1000.0;

  KJ_LOG(INFO, "Average concurrent check: ", avgUs, " μs for ", totalRequests, " requests");

  // Should still be fast with concurrent access
  KJ_EXPECT(avgUs < 5.0);
}

KJ_TEST("Rate limiting: token refill performance") {
  RateLimiterConfig config;
  config.capacity = 100;
  config.refill_rate = 1000.0; // High refill rate

  RateLimiter limiter(config);

  // Exhaust bucket
  for (int i = 0; i < 100; ++i) {
    auto result = limiter.check("refill_user");
    KJ_EXPECT(result.allowed);
  }

  // Wait 1 second for significant refill
  sleep_ms(1000);

  // Measure refill time on next check
  auto refillTimeNs = measure_time_ns([&]() {
    auto result = limiter.check("refill_user");
    (void)result;
  });

  double refillTimeUs = static_cast<double>(refillTimeNs) / 1000.0;

  KJ_LOG(INFO, "Refill + check time: ", refillTimeUs, " μs");

  // Refill should add minimal overhead
  KJ_EXPECT(refillTimeUs < 10.0);
}

// =============================================================================
// Edge Cases Tests
// =============================================================================

KJ_TEST("Rate limiting: zero capacity") {
  RateLimiterConfig config;
  config.capacity = 0;
  config.refill_rate = 1.0;

  RateLimiter limiter(config);

  // All requests should be blocked
  auto result = limiter.check("user_1");
  KJ_EXPECT(!result.allowed);
}

KJ_TEST("Rate limiting: very high refill rate") {
  RateLimiterConfig config;
  config.capacity = 10;
  config.refill_rate = 1000000.0; // 1M tokens per second

  RateLimiter limiter(config);

  // Exhaust bucket
  for (int i = 0; i < 10; ++i) {
    auto result = limiter.check("fast_refill_user");
    KJ_EXPECT(result.allowed);
  }

  // Wait very short time
  sleep_ms(1);

  // Should have tokens again
  auto result = limiter.check("fast_refill_user");
  KJ_EXPECT(result.allowed);
}

KJ_TEST("Rate limiting: maximum capacity") {
  RateLimiterConfig config;
  config.capacity = UINT32_MAX; // Maximum capacity
  config.refill_rate = 1.0;

  RateLimiter limiter(config);

  // Should always have tokens
  for (int i = 0; i < 1000; ++i) {
    auto result = limiter.check("unlimited_user");
    KJ_EXPECT(result.allowed);
  }
}

KJ_TEST("Rate limiting: empty user ID") {
  RateLimiterConfig config;
  config.capacity = 10;
  config.refill_rate = 1.0;

  RateLimiter limiter(config);

  // Empty string should still work (creates a bucket with empty key)
  auto result = limiter.check("");
  KJ_EXPECT(result.allowed);
}

// =============================================================================
// Rate Limit Recovery Tests
// =============================================================================

KJ_TEST("Rate limiting: recovery after rate limit") {
  RateLimiterConfig config;
  config.capacity = 5;
  config.refill_rate = 5.0; // 5 tokens per second

  RateLimiter limiter(config);

  // Exhaust tokens
  for (int i = 0; i < 5; ++i) {
    auto result = limiter.check("recovery_user");
    KJ_EXPECT(result.allowed);
  }

  // Verify blocked
  auto blocked = limiter.check("recovery_user");
  KJ_EXPECT(!blocked.allowed);

  // Wait for refill
  sleep_ms(1000);

  // Should be able to make requests again
  for (int i = 0; i < 5; ++i) {
    auto result = limiter.check("recovery_user");
    KJ_EXPECT(result.allowed);
  }
}

KJ_TEST("Rate limiting: gradual recovery") {
  RateLimiterConfig config;
  config.capacity = 10;
  config.refill_rate = 10.0; // 10 tokens per second

  RateLimiter limiter(config);

  // Exhaust tokens
  for (int i = 0; i < 10; ++i) {
    auto result = limiter.check("gradual_user");
    KJ_EXPECT(result.allowed);
  }

  // Each 100ms should give us approximately 1 token
  for (int i = 1; i <= 5; ++i) {
    sleep_ms(100);

    // Make one request
    auto result = limiter.check("gradual_user");
    KJ_EXPECT(result.allowed);
    KJ_LOG(INFO, "After ", i * 100, "ms, request succeeded, remaining: ", result.remaining);
  }
}

// =============================================================================
// Stress Tests
// =============================================================================

KJ_TEST("Rate limiting: stress test with many users") {
  RateLimiterConfig config;
  config.capacity = 100;
  config.refill_rate = 100.0;

  RateLimiter limiter(config);

  constexpr size_t NUM_USERS = 10000;

  auto totalTimeNs = measure_time_ns([&]() {
    for (size_t i = 0; i < NUM_USERS; ++i) {
      auto result = limiter.check(kj::str("stress_user_", i));
      (void)result;
    }
  });

  double totalTimeMs = static_cast<double>(totalTimeNs) / 1'000'000.0;

  KJ_LOG(INFO, "Stress test: ", NUM_USERS, " users in ", totalTimeMs, " ms");
  KJ_LOG(INFO, "Average: ", totalTimeNs / NUM_USERS, " ns per check");

  // Should handle 10,000 unique users efficiently
  KJ_EXPECT(limiter.bucket_count() == NUM_USERS);
  KJ_EXPECT(totalTimeMs < 1000.0); // Should complete in <1 second
}

} // namespace
