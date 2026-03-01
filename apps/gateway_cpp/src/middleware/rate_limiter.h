#pragma once

#include <atomic>
#include <cstdint>
#include <kj/common.h>
#include <kj/compat/http.h>
#include <kj/mutex.h>
#include <kj/refcount.h>
#include <kj/string.h>
#include <mutex>
#include <string>
#include <unordered_map>

namespace veloz::gateway::middleware {

/**
 * @brief Token bucket for rate limiting
 *
 * Uses atomic counters for lock-free rate limit checks.
 */
struct TokenBucket {
  std::atomic<uint32_t> tokens;      // Current token count
  std::atomic<uint64_t> last_refill; // Last refill timestamp (nanoseconds)
  std::atomic<uint64_t> last_access; // Last access timestamp (nanoseconds)
  std::atomic<uint64_t> created_at;  // Creation timestamp (nanoseconds) for TTL

  explicit TokenBucket(uint32_t initial_tokens, uint64_t now_ns)
      : tokens(initial_tokens), last_refill(now_ns), last_access(now_ns), created_at(now_ns) {}
};

/**
 * @brief Configuration for RateLimiter
 *
 * Configurable parameters for rate limiting behavior.
 */
struct RateLimiterConfig {
  uint32_t capacity = 100;               // Maximum tokens in bucket
  double refill_rate = 10.0;             // Tokens refilled per second
  uint64_t cleanup_interval_ms = 300000; // Clean buckets every 5 minutes (300 seconds)
  uint64_t bucket_ttl_ms = 1800000;      // Bucket expires after 30 minutes (1800 seconds)
  bool per_user_limiting = true;         // true = limit by user_id, false = limit by IP address

  RateLimiterConfig() = default;
};

/**
 * @brief Result of a rate limit check
 *
 * Contains information about whether a request is allowed
 * and the current state of the token bucket.
 */
struct RateLimitResult {
  bool allowed;                      // True if request should proceed
  uint32_t remaining;                // Remaining tokens in bucket
  uint64_t reset_at_ns;              // Unix timestamp (nanoseconds) when bucket will be full
  kj::Maybe<kj::String> retry_after; // ISO8601 duration if rate limited (e.g., "PT5.2S")

  RateLimitResult() : allowed(false), remaining(0), reset_at_ns(0) {}

  RateLimitResult(bool allowed_param, uint32_t remaining_param, uint64_t reset_at_param)
      : allowed(allowed_param), remaining(remaining_param), reset_at_ns(reset_at_param) {}
};

/**
 * @brief Rate limiting middleware using Token Bucket algorithm
 *
 * This class provides high-performance, thread-safe rate limiting.
 *
 * Thread safety:
 * - Token bucket counters use std::atomic for lock-free updates
 * - Bucket lookup/insertion uses std::unordered_map (thread-safe via mutex)
 * - Cleanup runs in background task with minimal lock contention
 *
 * Usage:
 * ```cpp
 * RateLimiter limiter(RateLimiterConfig{100, 10.0});  // 100 tokens, 10/sec refill
 *
 * // Check if user can make request
 * auto result = limiter.check("user_123");
 * if (!result.allowed) {
 *   // Send 429 response with rate limit headers
 *   return send_429_response(result);
 * }
 * // Allow request to proceed
 * return handler(ctx);
 * ```
 *
 * Features:
 * - Token Bucket algorithm with configurable capacity and refill rate
 * - Per-user or per-IP rate limiting (configurable)
 * - HTTP 429 (Too Many Requests) responses with standard headers
 * - Automatic cleanup of stale buckets
 * - Performance target: <1μs per check
 */
class RateLimiter {
public:
  explicit RateLimiter(const RateLimiterConfig& config = RateLimiterConfig());

  /**
   * @brief Check rate limit for a specific user ID
   *
   * @param user_id User identifier to rate limit
   * @return RateLimitResult with check status and bucket state
   *
   * This method is thread-safe and can be called concurrently from
   * multiple threads.
   *
   * Performance: <1μs per check (cache hit)
   */
  [[nodiscard]] RateLimitResult check(kj::StringPtr user_id);

  /**
   * @brief Check rate limit for an IP address
   *
   * @param ip_address IP address to rate limit
   * @return RateLimitResult with check status and bucket state
   *
   * Use this when per_user_limiting is set to false or for
   * unauthenticated requests.
   *
   * Performance: <1μs per check (cache hit)
   */
  [[nodiscard]] RateLimitResult check_ip(kj::StringPtr ip_address);

  /**
   * @brief Get current configuration
   *
   * @return Copy of current RateLimiterConfig
   */
  [[nodiscard]] RateLimiterConfig get_config() const {
    return config_;
  }

  /**
   * @brief Get number of active buckets
   *
   * Useful for monitoring memory usage.
   */
  [[nodiscard]] size_t bucket_count() const;

  /**
   * @brief Force cleanup of stale buckets
   *
   * Normally, cleanup runs automatically at cleanup_interval_ms intervals.
   * This method can be called to force an immediate cleanup.
   */
  void cleanup_stale_buckets();

  /**
   * @brief Set rate limit headers on an HTTP response
   *
   * Sets the following headers:
   * - X-RateLimit-Limit: Maximum requests per window
   * - X-RateLimit-Remaining: Remaining requests in current window
   * - X-RateLimit-Reset: Unix timestamp when bucket will be full
   * - Retry-After: ISO8601 duration if rate limited (HTTP 429 response)
   *
   * Note: reset_at is in nanoseconds but HTTP standards use seconds.
   */
  static void set_rate_limit_headers(kj::HttpHeaders& headers, const RateLimitResult& result);

  /**
   * @brief Create a 429 Too Many Requests error response
   *
   * @param result Rate limit check result that was not allowed
   * @param header_table HTTP header table for creating headers
   * @param response HTTP response object to send
   * @return Promise that completes when response is sent
   *
   * This is a convenience method for sending a standardized 429 response.
   */
  static kj::Promise<void> send_429_response(const RateLimitResult& result,
                                             const kj::HttpHeaderTable& header_table,
                                             kj::HttpService::Response& response);

  /**
   * @brief Destructor
   *
   * Cleanup task is automatically cancelled when RateLimiter is destroyed.
   */
  ~RateLimiter();

private:
  /**
   * @brief Refill tokens in a bucket based on elapsed time
   *
   * @param bucket Token bucket to refill
   * @param now_ns Current time in nanoseconds
   * @return Number of tokens after refill
   *
   * This implements the Token Bucket algorithm:
   * - Calculate elapsed time since last refill
   * - Add tokens = elapsed_time * refill_rate
   * - Cap at capacity
   */
  [[nodiscard]] uint32_t refill_bucket(TokenBucket& bucket, uint64_t now_ns);

  /**
   * @brief Get or create a token bucket for a given key
   *
   * @param key Bucket identifier (user_id or IP address)
   * @return Reference to the token bucket
   *
   * Thread-safe: Creates bucket on first access and returns existing
   * bucket on subsequent accesses.
   */
  [[nodiscard]] TokenBucket& get_or_create_bucket(kj::StringPtr key);

  /**
   * @brief Perform rate limit check on a specific bucket
   *
   * @param bucket Token bucket to check
   * @param now_ns Current time in nanoseconds
   * @return RateLimitResult with check status
   *
   * Performance target: <1μs
   */
  [[nodiscard]] RateLimitResult check_bucket(TokenBucket& bucket, uint64_t now_ns);

  /**
   * @brief Calculate retry-after duration in ISO8601 format
   *
   * @param reset_at_ns Time when bucket will have tokens available
   * @param now_ns Current time in nanoseconds
   * @return ISO8601 duration string
   *
   * Note: reset_at is in nanoseconds but HTTP standards use seconds.
   */
  [[nodiscard]] kj::String calculate_retry_after(uint64_t reset_at_ns, uint64_t now_ns);

  /**
   * @brief Get current Unix timestamp in nanoseconds
   *
   * @return Current time as nanoseconds since epoch
   *
   * Returns nanosecond precision timestamp for accurate rate limiting.
   */
  [[nodiscard]] static uint64_t current_time_ns();

  /**
   * @brief Cleanup buckets that have exceeded their TTL
   *
   * This is called automatically at cleanup_interval_ms intervals.
   * This method can also be called manually to force immediate cleanup.
   */
  void cleanup_stale_buckets_internal();

  /**
   * @brief Start the background cleanup task
   *
   * Sets up a periodic timer to call cleanup_stale_buckets_internal().
   * This is called from the constructor.
   */
  void start_cleanup_task();

  // Configuration
  RateLimiterConfig config_;

  // Bucket storage: key -> bucket
  // Uses std::unordered_map (thread-safe via mutex) - KJ HashMap not available
  // This is a known std library usage as documented in CLAUDE.md
  mutable std::mutex buckets_mutex_;
  std::unordered_map<std::string, kj::Own<TokenBucket>> buckets_;

  // Cleanup task handle (if running)
  kj::Maybe<kj::Own<kj::PromiseFulfiller<void>>> cleanup_task_;
};

} // namespace veloz::gateway::middleware
