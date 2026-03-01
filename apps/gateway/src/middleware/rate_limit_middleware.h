#pragma once

#include "request_context.h"

#include <kj/mutex.h>
#include <kj/string.h>
#include <kj/time.h>

namespace veloz::gateway {

/**
 * Token bucket rate limiter.
 *
 * Limits request rate per client using the token bucket algorithm.
 */
class RateLimiter {
public:
  struct Config {
    size_t capacity;        // Maximum tokens in bucket
    double refillRate;      // Tokens per second
    size_t cleanupInterval; // Cleanup interval in ms
  };

  explicit RateLimiter(const Config& config);

  /**
   * Check if request is allowed under rate limit.
   *
   * @param identifier Client identifier (IP or key ID)
   * @param cost Number of tokens to consume
   * @return Tuple of (allowed, retryAfterMs)
   */
  struct CheckResult {
    bool allowed;
    uint64_t retryAfterMs;
    size_t remainingTokens;
    uint64_t resetTimeMs;
  };

  CheckResult check(kj::StringPtr identifier, double cost = 1.0);

private:
  struct Bucket {
    size_t tokens;
    uint64_t lastUpdateMs;
  };

  Config config_;
  kj::MutexGuarded<kj::HashMap<kj::String, Bucket>> buckets_;
  uint64_t lastCleanupMs_;
};

/**
 * Rate limiting middleware.
 *
 * Checks rate limits before allowing request to proceed.
 */
class RateLimitMiddleware : public Middleware {
public:
  explicit RateLimitMiddleware(kj::Own<RateLimiter> limiter);

  kj::Promise<void> process(RequestContext& ctx, kj::Function<kj::Promise<void>()> next) override;

private:
  kj::Own<RateLimiter> limiter_;

  kj::String getClientIdentifier(const RequestContext& ctx);
};

} // namespace veloz::gateway
