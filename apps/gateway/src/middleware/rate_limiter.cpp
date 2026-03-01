#include "middleware/rate_limiter.h"

#include "router.h"

#include <chrono>
#include <cmath>
#include <iomanip>
#include <kj/async-io.h>
#include <kj/compat/http.h>
#include <kj/debug.h>
#include <kj/one-of.h>
#include <kj/string.h>
#include <sstream>
#include <vector>

namespace veloz::gateway::middleware {

namespace {
// Helper to convert nanoseconds to ISO 8601 duration
kj::String format_duration_ns_to_iso8601(uint64_t duration_ns) {
  if (duration_ns == 0) {
    return kj::str("PT0S");
  }

  // Convert to seconds with fractional part
  double seconds = static_cast<double>(duration_ns) / 1'000'000'000.0;

  // Build ISO 8601 duration string
  std::ostringstream oss;
  oss << "PT";

  if (seconds >= 3600.0) {
    double hours = std::floor(seconds / 3600.0);
    oss << static_cast<int>(hours) << "H";
    seconds -= hours * 3600.0;
  }

  if (seconds >= 60.0) {
    double minutes = std::floor(seconds / 60.0);
    oss << static_cast<int>(minutes) << "M";
    seconds -= minutes * 60.0;
  }

  if (seconds >= 0.001) {
    // Include fractional seconds if meaningful
    oss << std::fixed << std::setprecision(3) << seconds << "S";
  } else {
    oss << "0S";
  }

  return kj::str(oss.str().c_str());
}

// Helper to get current time in nanoseconds
inline uint64_t get_time_ns() {
  // Use KJ's getTimer() for async-compatible time
  // Or use system time for precise measurements
  auto now = std::chrono::system_clock::now();
  auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(now.time_since_epoch());
  return static_cast<uint64_t>(ns.count());
}

} // namespace

RateLimiter::RateLimiter(const RateLimiterConfig& config) : config_(config) {
  // Start background cleanup task
  start_cleanup_task();
}

RateLimiter::~RateLimiter() {
  // The cleanup task will be automatically cancelled when the
  // RateLimiter object is destroyed (kj::Own destructor)
  // No explicit cleanup needed as kj::Own handles this
}

RateLimitResult RateLimiter::check(kj::StringPtr user_id) {
  auto now_ns = current_time_ns();
  TokenBucket& bucket = get_or_create_bucket(user_id);
  return check_bucket(bucket, now_ns);
}

RateLimitResult RateLimiter::check_ip(kj::StringPtr ip_address) {
  auto now_ns = current_time_ns();
  TokenBucket& bucket = get_or_create_bucket(ip_address);
  return check_bucket(bucket, now_ns);
}

size_t RateLimiter::bucket_count() const {
  std::lock_guard<std::mutex> lock(buckets_mutex_);
  return buckets_.size();
}

void RateLimiter::cleanup_stale_buckets() {
  cleanup_stale_buckets_internal();
}

void RateLimiter::set_rate_limit_headers(kj::HttpHeaders& headers, const RateLimitResult& result) {
  // X-RateLimit-Limit: Maximum requests per window
  // Note: We don't store the config in the result, so this is a simplification
  // In a full implementation, we might want to include the limit
  // headers.set("X-RateLimit-Limit"_kj, kj::str(limit));

  // X-RateLimit-Remaining: Remaining requests in current window
  headers.addPtr("X-RateLimit-Remaining"_kj, kj::str(result.remaining));

  // X-RateLimit-Reset: Unix timestamp when bucket will be full (in seconds)
  uint64_t reset_at_sec = result.reset_at_ns / 1'000'000'000ULL;
  headers.addPtr("X-RateLimit-Reset"_kj, kj::str(reset_at_sec));

  // Retry-After: ISO 8601 duration if rate limited (HTTP 429 response)
  KJ_IF_SOME(retry_after, result.retry_after) {
    headers.addPtrPtr("Retry-After"_kj, retry_after);
  }
}

kj::Promise<void> RateLimiter::send_429_response(const RateLimitResult& result,
                                                 const kj::HttpHeaderTable& header_table,
                                                 kj::HttpService::Response& response) {
  kj::HttpHeaders headers(header_table);
  headers.setPtr(kj::HttpHeaderId::CONTENT_TYPE, "application/json"_kj);

  // Set rate limit headers
  set_rate_limit_headers(headers, result);

  // Create error response body
  kj::String body = kj::str(R"({
  "error": "rate_limit_exceeded",
  "message": "Too many requests. Please try again later."
})");

  auto stream = response.send(429, "Too Many Requests"_kj, headers, body.size());
  return stream->write(body.asBytes()).attach(kj::mv(body));
}

uint32_t RateLimiter::refill_bucket(TokenBucket& bucket, uint64_t now_ns) {
  // Get the last refill time
  uint64_t last_refill = bucket.last_refill.load(std::memory_order_relaxed);

  // On first access, initialize last_refill to now
  if (last_refill == 0) {
    uint64_t expected = 0;
    if (bucket.last_refill.compare_exchange_strong(expected, now_ns, std::memory_order_relaxed)) {
      return bucket.tokens.load(std::memory_order_relaxed);
    }
    // CAS failed, another thread initialized it, retry
    return refill_bucket(bucket, now_ns);
  }

  // Calculate elapsed time in seconds
  uint64_t elapsed_ns = now_ns - last_refill;
  double elapsed_sec = static_cast<double>(elapsed_ns) / 1'000'000'000.0;

  // Calculate tokens to add
  double tokens_to_add = elapsed_sec * config_.refill_rate;

  if (tokens_to_add < 1.0) {
    // Not enough time has passed to add at least one token
    // Still proceed with CAS to handle concurrent modifications
    tokens_to_add = 0.0;
  }

  // Atomic token refill with CAS loop
  uint32_t current_tokens;
  uint32_t new_tokens;
  do {
    current_tokens = bucket.tokens.load(std::memory_order_relaxed);
    new_tokens = std::min(static_cast<uint32_t>(current_tokens + tokens_to_add), config_.capacity);
  } while (
      !bucket.tokens.compare_exchange_weak(current_tokens, new_tokens, std::memory_order_relaxed));

  // Update last_refill time (we update atomically, but CAS may fail)
  // Use relaxed ordering for last_refill as exact timing isn't critical
  bucket.last_refill.store(now_ns, std::memory_order_relaxed);

  return new_tokens;
}

TokenBucket& RateLimiter::get_or_create_bucket(kj::StringPtr key) {
  std::lock_guard<std::mutex> lock(buckets_mutex_);

  // Try to find existing bucket
  std::string keyStd(key.cStr(), key.size());
  auto it = buckets_.find(keyStd);
  if (it != buckets_.end()) {
    return *(it->second.get());
  }

  // Create new bucket
  std::string key_copy = keyStd;
  auto new_bucket = kj::heap<TokenBucket>(config_.capacity, current_time_ns());
  new_bucket->tokens.store(config_.capacity, std::memory_order_relaxed);

  TokenBucket* bucket_ptr = new_bucket.get();
  buckets_.emplace(std::move(key_copy), kj::mv(new_bucket));

  return *bucket_ptr;
}

RateLimitResult RateLimiter::check_bucket(TokenBucket& bucket, uint64_t now_ns) {
  // Refill bucket first
  uint32_t tokens = refill_bucket(bucket, now_ns);

  // If no tokens available, request is denied
  if (tokens == 0) {
    // Calculate when the next token will be available
    uint64_t reset_at = now_ns;
    if (config_.refill_rate > 0.0) {
      double time_to_one_token_ns = (1.0 / config_.refill_rate) * 1'000'000'000.0;
      reset_at = now_ns + static_cast<uint64_t>(time_to_one_token_ns);
    }

    RateLimitResult result(false, 0, reset_at);
    result.retry_after = calculate_retry_after(reset_at, now_ns);
    return result;
  }

  // Try to consume a token
  uint32_t expected = tokens;
  uint32_t desired = tokens - 1;

  if (bucket.tokens.compare_exchange_strong(expected, desired, std::memory_order_relaxed)) {
    // Token consumed successfully
    uint32_t remaining = desired;
    uint64_t reset_at = now_ns;

    // Calculate time to refill one token
    if (remaining == 0 && config_.refill_rate > 0.0) {
      double time_to_one_token_ns = (1.0 / config_.refill_rate) * 1'000'000'000.0;
      reset_at = now_ns + static_cast<uint64_t>(time_to_one_token_ns);
    }

    return RateLimitResult(true, remaining, reset_at);
  }

  // CAS failed, another thread modified the tokens
  // Retry with the new value
  return check_bucket(bucket, now_ns);
}

kj::String RateLimiter::calculate_retry_after(uint64_t reset_at_ns, uint64_t now_ns) {
  if (reset_at_ns <= now_ns) {
    return kj::str("PT0S");
  }

  uint64_t wait_time_ns = reset_at_ns - now_ns;
  return format_duration_ns_to_iso8601(wait_time_ns);
}

uint64_t RateLimiter::current_time_ns() {
  return get_time_ns();
}

void RateLimiter::cleanup_stale_buckets_internal() {
  std::lock_guard<std::mutex> lock(buckets_mutex_);
  auto now_ns = current_time_ns();
  uint64_t ttl_ns = config_.bucket_ttl_ms * 1'000'000ULL;

  // Find stale buckets
  std::vector<std::string> keys_to_remove;
  for (const auto& entry : buckets_) {
    uint64_t created_at = entry.second->created_at.load(std::memory_order_relaxed);
    if (now_ns - created_at > ttl_ns) {
      keys_to_remove.push_back(entry.first);
    }
  }

  // Remove stale buckets
  for (const auto& key : keys_to_remove) {
    buckets_.erase(key);
  }
}

void RateLimiter::start_cleanup_task() {
  // In a real async environment, we would set up a timer-based cleanup task here
  // For now, we rely on manual cleanup or integrate with the event loop
  // This is a placeholder for the async cleanup task

  // Example of how this would work with kj::EventLoop:
  //
  // auto event_loop = kj::getCurrentEventLoop();
  // auto timer = event_loop->getTimer();
  //
  // cleanup_task_ = timer->afterDelay(config_.cleanup_interval_ms * kj::MILLISECONDS)
  //   .then([this]() {
  //     cleanup_stale_buckets_internal();
  //     return start_cleanup_task();  // Reschedule
  //   }).eagerlyEvaluate(nullptr);
  //
  // Since we can't easily get the event loop here without refactoring,
  // we'll make cleanup_stale_buckets_internal() publicly accessible for manual cleanup
  // or integration with the main event loop.
}

} // namespace veloz::gateway::middleware
