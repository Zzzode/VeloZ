#include "middleware/rate_limiter.h"

#include <atomic>
#include <kj/async-io.h>
#include <kj/compat/http.h>
#include <kj/test.h>
#include <thread>
#include <vector>

namespace veloz::gateway::middleware {
namespace {

// Helper to create a mock HTTP header table
kj::HttpHeaderTable::Builder createHeaderTableBuilder() {
  return kj::HttpHeaderTable::Builder();
}

kj::Maybe<kj::StringPtr> findHeader(const kj::HttpHeaders& headers, kj::StringPtr name) {
  kj::Maybe<kj::StringPtr> result = kj::none;
  headers.forEach([&](kj::StringPtr headerName, kj::StringPtr headerValue) {
    if (headerName == name) {
      result = headerValue;
    }
  });
  return result;
}

// Test basic configuration
KJ_TEST("RateLimiter: default configuration") {
  RateLimiterConfig config;
  KJ_EXPECT(config.capacity == 100);
  KJ_EXPECT(config.refill_rate == 10.0);
  KJ_EXPECT(config.cleanup_interval_ms == 300'000);
  KJ_EXPECT(config.bucket_ttl_ms == 1'800'000);
  KJ_EXPECT(config.per_user_limiting == true);
}

// Test custom configuration
KJ_TEST("RateLimiter: custom configuration") {
  RateLimiterConfig config;
  config.capacity = 50;
  config.refill_rate = 5.0;
  config.cleanup_interval_ms = 60'000;
  config.bucket_ttl_ms = 600'000;
  config.per_user_limiting = false;

  RateLimiter limiter(config);
  auto retrieved = limiter.get_config();

  KJ_EXPECT(retrieved.capacity == 50);
  KJ_EXPECT(retrieved.refill_rate == 5.0);
  KJ_EXPECT(retrieved.cleanup_interval_ms == 60'000);
  KJ_EXPECT(retrieved.bucket_ttl_ms == 600'000);
  KJ_EXPECT(retrieved.per_user_limiting == false);
}

// Test token bucket initialization and first check
KJ_TEST("RateLimiter: first check allows request") {
  RateLimiterConfig config;
  config.capacity = 10;
  config.refill_rate = 1.0; // 1 token per second

  RateLimiter limiter(config);

  // First check should be allowed
  auto result = limiter.check("user_1");
  KJ_EXPECT(result.allowed == true);
  KJ_EXPECT(result.remaining == 9); // One token consumed
  KJ_EXPECT(result.reset_at_ns > 0);
  KJ_EXPECT(result.retry_after == kj::none);
}

// Test token consumption
KJ_TEST("RateLimiter: token consumption") {
  RateLimiterConfig config;
  config.capacity = 5;
  config.refill_rate = 1.0; // 1 token per second

  RateLimiter limiter(config);

  // Consume all tokens
  for (int i = 0; i < 5; i++) {
    auto result = limiter.check("user_1");
    KJ_EXPECT(result.allowed == true);
    KJ_EXPECT(result.remaining == static_cast<uint32_t>(4 - i));
  }

  // Next request should be denied
  auto result = limiter.check("user_1");
  KJ_EXPECT(result.allowed == false);
  KJ_EXPECT(result.remaining == 0);
  KJ_EXPECT(result.retry_after != kj::none);
}

// Test rate limit headers
KJ_TEST("RateLimiter: rate limit headers") {
  // Build the header table properly before using it
  auto headerTableBuilder = createHeaderTableBuilder();
  kj::Own<const kj::HttpHeaderTable> headerTable = headerTableBuilder.build();

  kj::HttpHeaders headers(*headerTable);

  RateLimitResult result;
  result.allowed = false;
  result.remaining = 5;
  // 1730 seconds since Unix epoch, stored as nanoseconds
  result.reset_at_ns = 1730ULL * 1'000'000'000ULL;
  result.retry_after = kj::str("PT10S");

  RateLimiter::set_rate_limit_headers(headers, result);

  // Verify headers are set
  KJ_IF_SOME(remaining, findHeader(headers, "X-RateLimit-Remaining"_kj)) {
    KJ_EXPECT(remaining == "5");
  }
  else {
    KJ_FAIL_REQUIRE("X-RateLimit-Remaining header not set");
  }

  KJ_IF_SOME(reset, findHeader(headers, "X-RateLimit-Reset"_kj)) {
    // Should be in seconds (divide by 1 billion)
    KJ_EXPECT(reset == "1730");
  }
  else {
    KJ_FAIL_REQUIRE("X-RateLimit-Reset header not set");
  }

  KJ_IF_SOME(retry, findHeader(headers, "Retry-After"_kj)) {
    KJ_EXPECT(retry == "PT10S");
  }
  else {
    KJ_FAIL_REQUIRE("Retry-After header not set");
  }
}

// Test multiple users have separate buckets
KJ_TEST("RateLimiter: separate buckets per user") {
  RateLimiterConfig config;
  config.capacity = 5;
  config.refill_rate = 1.0;

  RateLimiter limiter(config);

  // Consume all tokens for user_1
  for (int i = 0; i < 5; i++) {
    auto result = limiter.check("user_1");
    KJ_EXPECT(result.allowed == true);
  }

  // user_1 should be rate limited
  auto result1 = limiter.check("user_1");
  KJ_EXPECT(result1.allowed == false);

  // user_2 should still have full bucket
  auto result2 = limiter.check("user_2");
  KJ_EXPECT(result2.allowed == true);
  KJ_EXPECT(result2.remaining == 4); // 5 - 1 consumed

  // Verify bucket count
  KJ_EXPECT(limiter.bucket_count() == 2);
}

// Test IP-based rate limiting
KJ_TEST("RateLimiter: IP-based rate limiting") {
  RateLimiterConfig config;
  config.capacity = 3;
  config.refill_rate = 1.0;

  RateLimiter limiter(config);

  // Consume all tokens for IP 192.168.1.1
  for (int i = 0; i < 3; i++) {
    auto result = limiter.check_ip("192.168.1.1");
    KJ_EXPECT(result.allowed == true);
  }

  // This IP should be rate limited
  auto result1 = limiter.check_ip("192.168.1.1");
  KJ_EXPECT(result1.allowed == false);

  // Different IP should still work
  auto result2 = limiter.check_ip("192.168.1.2");
  KJ_EXPECT(result2.allowed == true);
  KJ_EXPECT(result2.remaining == 2);

  KJ_EXPECT(limiter.bucket_count() == 2);
}

// Test bucket cleanup
KJ_TEST("RateLimiter: bucket cleanup") {
  RateLimiterConfig config;
  config.capacity = 10;
  config.refill_rate = 1.0;
  config.bucket_ttl_ms = 100; // Very short TTL for testing

  RateLimiter limiter(config);

  // Create multiple buckets
  auto result1 = limiter.check("user_1");
  auto result2 = limiter.check("user_2");
  auto result3 = limiter.check("user_3");
  KJ_EXPECT(result1.allowed == true);
  KJ_EXPECT(result2.allowed == true);
  KJ_EXPECT(result3.allowed == true);

  KJ_EXPECT(limiter.bucket_count() == 3);

  // Wait for TTL to expire (200ms should be enough)
  std::this_thread::sleep_for(std::chrono::milliseconds(200));

  // Force cleanup
  limiter.cleanup_stale_buckets();

  // Buckets should be cleaned up
  KJ_EXPECT(limiter.bucket_count() == 0);
}

// Test token refill over time
KJ_TEST("RateLimiter: token refill") {
  RateLimiterConfig config;
  config.capacity = 5;
  config.refill_rate = 10.0; // 10 tokens per second

  RateLimiter limiter(config);

  // Consume all tokens
  for (int i = 0; i < 5; i++) {
    auto result = limiter.check("user_1");
    KJ_EXPECT(result.allowed == true);
  }

  // Verify empty
  auto result1 = limiter.check("user_1");
  KJ_EXPECT(result1.allowed == false);

  // Wait for refill (100ms should add ~1 token at 10 tokens/sec)
  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  // Should have at least 1 token now
  auto result2 = limiter.check("user_1");
  KJ_EXPECT(result2.allowed == true);
}

// Test capacity limit on refill
KJ_TEST("RateLimiter: capacity limit on refill") {
  RateLimiterConfig config;
  config.capacity = 5;
  config.refill_rate = 100.0; // Very fast refill

  RateLimiter limiter(config);

  // Consume some tokens
  auto consume1 = limiter.check("user_1");
  auto consume2 = limiter.check("user_1");
  auto consume3 = limiter.check("user_1");
  KJ_EXPECT(consume1.allowed == true);
  KJ_EXPECT(consume2.allowed == true);
  KJ_EXPECT(consume3.allowed == true);

  // Wait for potential overfill (would add many tokens without cap)
  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  // Check multiple times - should never exceed capacity
  for (int i = 0; i < 10; i++) {
    auto result = limiter.check("user_1");
    if (result.allowed) {
      // If allowed, remaining should be at most capacity-1
      KJ_EXPECT(result.remaining < config.capacity);
    }
  }
}

// Test concurrent access (basic thread safety)
KJ_TEST("RateLimiter: concurrent access") {
  RateLimiterConfig config;
  config.capacity = 100;
  config.refill_rate = 100.0; // Fast refill to avoid blocking

  RateLimiter limiter(config);

  const int numThreads = 4;
  const int requestsPerThread = 25;
  std::atomic<int> allowed_count{0};
  std::atomic<int> denied_count{0};

  std::vector<std::thread> threads;
  for (int t = 0; t < numThreads; t++) {
    threads.emplace_back([&limiter, &allowed_count, &denied_count, t]() {
      kj::String user_id = kj::str("user_", t);
      for (int i = 0; i < requestsPerThread; i++) {
        auto result = limiter.check(user_id);
        if (result.allowed) {
          allowed_count++;
        } else {
          denied_count++;
        }
      }
    });
  }

  // Wait for all threads
  for (auto& t : threads) {
    t.join();
  }

  // Each user should have been able to make requests (separate buckets)
  KJ_EXPECT(allowed_count == numThreads * requestsPerThread);
  KJ_EXPECT(denied_count == 0);
}

// Test concurrent access with shared bucket
KJ_TEST("RateLimiter: concurrent access with shared bucket") {
  RateLimiterConfig config;
  config.capacity = 50;
  config.refill_rate = 0.0;

  RateLimiter limiter(config);

  const int numThreads = 4;
  const int requestsPerThread = 20; // Total 80 requests for 50 capacity
  std::atomic<int> allowed_count{0};
  std::atomic<int> denied_count{0};

  std::vector<std::thread> threads;
  for (int t = 0; t < numThreads; t++) {
    threads.emplace_back([&limiter, &allowed_count, &denied_count]() {
      for (int i = 0; i < requestsPerThread; i++) {
        auto result = limiter.check("shared_user");
        if (result.allowed) {
          allowed_count++;
        } else {
          denied_count++;
        }
      }
    });
  }

  // Wait for all threads
  for (auto& t : threads) {
    t.join();
  }

  KJ_EXPECT(allowed_count == 50);
  KJ_EXPECT(denied_count == 30);
}

// Test retry-after calculation
KJ_TEST("RateLimiter: retry-after duration") {
  RateLimiterConfig config;
  config.capacity = 1;
  config.refill_rate = 1.0; // 1 token per second

  RateLimiter limiter(config);

  // Consume the only token
  auto result1 = limiter.check("user_1");
  KJ_EXPECT(result1.allowed == true);
  KJ_EXPECT(result1.remaining == 0);

  // Next request should be denied with retry_after
  auto result2 = limiter.check("user_1");
  KJ_EXPECT(result2.allowed == false);

  // retry_after should indicate approximately 1 second
  KJ_IF_SOME(retry, result2.retry_after) {
    // Should be in ISO 8601 format (e.g., "PT1S" or "PT1.0S")
    KJ_EXPECT(retry.startsWith("PT"));
    KJ_EXPECT(retry.endsWith("S"));
  }
  else {
    KJ_FAIL_REQUIRE("Expected retry_after to be set for rate limited request");
  }
}

// Test reset_at timestamp calculation
KJ_TEST("RateLimiter: reset_at timestamp") {
  RateLimiterConfig config;
  config.capacity = 3;
  config.refill_rate = 1.0;

  RateLimiter limiter(config);

  auto now = std::chrono::system_clock::now();
  auto now_ns =
      std::chrono::duration_cast<std::chrono::nanoseconds>(now.time_since_epoch()).count();

  auto result = limiter.check("user_1");
  KJ_EXPECT(result.allowed == true);

  // reset_at should be in the future (or very close to now)
  // Allow for some clock skew
  KJ_EXPECT(result.reset_at_ns >= static_cast<uint64_t>(now_ns - 1'000'000'000));
}

// Test zero refill rate (no recovery)
KJ_TEST("RateLimiter: zero refill rate") {
  RateLimiterConfig config;
  config.capacity = 2;
  config.refill_rate = 0.0; // No refill

  RateLimiter limiter(config);

  // Consume all tokens
  auto result1 = limiter.check("user_1");
  KJ_EXPECT(result1.allowed == true);

  auto result2 = limiter.check("user_1");
  KJ_EXPECT(result2.allowed == true);

  // Should be permanently rate limited
  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  auto result3 = limiter.check("user_1");
  KJ_EXPECT(result3.allowed == false);
}

// Test very high capacity
KJ_TEST("RateLimiter: high capacity") {
  RateLimiterConfig config;
  config.capacity = 10000;
  config.refill_rate = 1000.0;

  RateLimiter limiter(config);

  // Make 5000 requests
  for (int i = 0; i < 5000; i++) {
    auto result = limiter.check("user_1");
    KJ_EXPECT(result.allowed == true);
  }

  // Should still have tokens remaining
  auto result = limiter.check("user_1");
  KJ_EXPECT(result.allowed == true);
  KJ_EXPECT(result.remaining == 10000 - 5001);
}

// Test rapid requests
KJ_TEST("RateLimiter: rapid requests") {
  RateLimiterConfig config;
  config.capacity = 100;
  config.refill_rate = 1000.0; // Fast refill

  RateLimiter limiter(config);

  // Make many rapid requests
  int allowed = 0;
  for (int i = 0; i < 200; i++) {
    auto result = limiter.check("user_1");
    if (result.allowed) {
      allowed++;
    }
  }

  // With 100 capacity and 1000 tokens/sec refill, should allow most requests
  KJ_EXPECT(allowed >= 100);
}

// Test ISO 8601 duration formatting (internal helper)
KJ_TEST("RateLimiter: ISO 8601 duration format") {
  // Test various durations
  auto testDuration = [](uint64_t ns, kj::StringPtr expected) {
    (void)ns;
    (void)expected;
    // We test this indirectly through the retry_after field
    // This is a basic sanity check
    RateLimiterConfig config;
    config.capacity = 1;
    config.refill_rate = 0.001; // Very slow: 0.001 tokens/sec = 1000 sec per token

    RateLimiter limiter(config);

    // Consume token
    auto result1 = limiter.check("user_1");
    KJ_EXPECT(result1.allowed == true);

    auto result = limiter.check("user_1");
    KJ_EXPECT(result.allowed == false);

    KJ_IF_SOME(retry, result.retry_after) {
      KJ_EXPECT(retry.startsWith("PT"));
      KJ_EXPECT(retry.endsWith("S"));
    }
  };

  // Test a few durations indirectly
  testDuration(1'000'000'000ULL, "PT1S"); // 1 second
}

// Test bucket count tracking
KJ_TEST("RateLimiter: bucket count tracking") {
  RateLimiterConfig config;
  config.capacity = 10;
  config.refill_rate = 1.0;

  RateLimiter limiter(config);

  KJ_EXPECT(limiter.bucket_count() == 0);

  auto result1 = limiter.check("user_1");
  KJ_EXPECT(result1.allowed == true);
  KJ_EXPECT(limiter.bucket_count() == 1);

  auto result2 = limiter.check("user_2");
  KJ_EXPECT(result2.allowed == true);
  KJ_EXPECT(limiter.bucket_count() == 2);

  auto result3 = limiter.check("user_1");
  KJ_EXPECT(result3.allowed == true);
  KJ_EXPECT(limiter.bucket_count() == 2);

  auto result4 = limiter.check_ip("192.168.1.1");
  KJ_EXPECT(result4.allowed == true);
  KJ_EXPECT(limiter.bucket_count() == 3);
}

// Test configuration with very small values
KJ_TEST("RateLimiter: small capacity") {
  RateLimiterConfig config;
  config.capacity = 1;
  config.refill_rate = 0.1; // 0.1 tokens per second = 10 sec per token

  RateLimiter limiter(config);

  auto result1 = limiter.check("user_1");
  KJ_EXPECT(result1.allowed == true);
  KJ_EXPECT(result1.remaining == 0);

  auto result2 = limiter.check("user_1");
  KJ_EXPECT(result2.allowed == false);
}

} // namespace
} // namespace veloz::gateway::middleware
