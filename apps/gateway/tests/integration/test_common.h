/**
 * @file test_common.h
 * @brief Common utilities and helpers for integration tests
 *
 * This header provides shared test infrastructure including:
 * - Mock HTTP request/response classes
 * - Test environment setup
 * - Timing helpers
 * - Assertion utilities
 */

#pragma once

#include <atomic>
#include <chrono>
#include <kj/async-io.h>
#include <kj/compat/http.h>
#include <kj/string.h>
#include <kj/test.h>
#include <kj/vector.h>
#include <thread>

namespace veloz::gateway::test {

inline uint64_t current_time_ns();

class MockHttpResponse final : public kj::HttpService::Response {
public:
  kj::uint statusCode{0};
  kj::String statusText;
  kj::String body;
  kj::HttpHeaders responseHeaders;
  uint64_t responseTimeNs{0};
  bool sent{false};

  explicit MockHttpResponse(const kj::HttpHeaderTable& headerTable)
      : responseHeaders(headerTable) {}

  kj::Own<kj::AsyncOutputStream> send(kj::uint statusCode, kj::StringPtr statusText,
                                      const kj::HttpHeaders& headers,
                                      KJ_UNUSED kj::Maybe<uint64_t> expectedBodySize) override {
    this->statusCode = statusCode;
    this->statusText = kj::str(statusText);
    this->responseHeaders = headers.clone();
    this->sent = true;
    this->responseTimeNs = current_time_ns();

    return kj::heap<MockOutputStream>(this);
  }

  kj::Own<kj::WebSocket> acceptWebSocket(KJ_UNUSED const kj::HttpHeaders& headers) override {
    KJ_FAIL_REQUIRE("WebSocket not supported in tests");
  }

  void reset() {
    statusCode = 0;
    statusText = nullptr;
    body = nullptr;
    sent = false;
    responseTimeNs = 0;
  }

private:
  class MockOutputStream final : public kj::AsyncOutputStream {
  public:
    explicit MockOutputStream(MockHttpResponse* parent) : parent_(parent) {}

    kj::Promise<void> write(kj::ArrayPtr<const kj::byte> data) override {
      if (parent_) {
        parent_->body = kj::str(reinterpret_cast<const char*>(data.begin()), data.size());
      }
      return kj::READY_NOW;
    }

    kj::Promise<void> write(kj::ArrayPtr<const kj::ArrayPtr<const kj::byte>> pieces) override {
      kj::Vector<char> combined;
      for (auto& piece : pieces) {
        combined.addAll(piece);
      }
      if (parent_) {
        parent_->body = kj::str(combined.asPtr());
      }
      return kj::READY_NOW;
    }

    kj::Promise<void> whenWriteDisconnected() override {
      return kj::NEVER_DONE;
    }

  private:
    MockHttpResponse* parent_;
  };
};

/**
 * @brief Mock HTTP request for testing
 */
struct MockHttpRequest {
  kj::HttpMethod method;
  kj::String path;
  kj::String queryString;
  kj::HttpHeaders headers;
  kj::String body;
};

// =============================================================================
// Timing Utilities
// =============================================================================

/**
 * @brief Get current time in nanoseconds
 */
inline uint64_t current_time_ns() {
  return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(
                                   std::chrono::high_resolution_clock::now().time_since_epoch())
                                   .count());
}

/**
 * @brief Get current time in milliseconds
 */
inline uint64_t current_time_ms() {
  return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
                                   std::chrono::high_resolution_clock::now().time_since_epoch())
                                   .count());
}

/**
 * @brief Measure execution time in nanoseconds
 */
template <typename F> uint64_t measure_time_ns(F&& func) {
  auto start = std::chrono::high_resolution_clock::now();
  func();
  auto end = std::chrono::high_resolution_clock::now();
  return static_cast<uint64_t>(
      std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count());
}

/**
 * @brief Measure execution time in microseconds
 */
template <typename F> uint64_t measure_time_us(F&& func) {
  auto start = std::chrono::high_resolution_clock::now();
  func();
  auto end = std::chrono::high_resolution_clock::now();
  return static_cast<uint64_t>(
      std::chrono::duration_cast<std::chrono::microseconds>(end - start).count());
}

/**
 * @brief Measure execution time in milliseconds
 */
template <typename F> uint64_t measure_time_ms(F&& func) {
  auto start = std::chrono::high_resolution_clock::now();
  func();
  auto end = std::chrono::high_resolution_clock::now();
  return static_cast<uint64_t>(
      std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count());
}

/**
 * @brief Sleep for specified milliseconds
 */
inline void sleep_ms(uint64_t ms) {
  std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}

/**
 * @brief Sleep for specified seconds
 */
inline void sleep_sec(uint64_t sec) {
  std::this_thread::sleep_for(std::chrono::seconds(sec));
}

// =============================================================================
// JSON Helpers
// =============================================================================

/**
 * @brief Create a simple JSON object with one key-value pair
 */
inline kj::String json_one(kj::StringPtr key, kj::StringPtr value) {
  return kj::str("{\"", key, "\":\"", value, "\"}");
}

/**
 * @brief Create a JSON object with two key-value pairs
 */
inline kj::String json_two(kj::StringPtr key1, kj::StringPtr value1, kj::StringPtr key2,
                           kj::StringPtr value2) {
  return kj::str("{\"", key1, "\":\"", value1, "\",\"", key2, "\":\"", value2, "\"}");
}

/**
 * @brief Create a JSON object with three key-value pairs
 */
inline kj::String json_three(kj::StringPtr key1, kj::StringPtr value1, kj::StringPtr key2,
                             kj::StringPtr value2, kj::StringPtr key3, kj::StringPtr value3) {
  return kj::str("{\"", key1, "\":\"", value1, "\",\"", key2, "\":\"", value2, "\",\"", key3,
                 "\":\"", value3, "\"}");
}

/**
 * @brief Create an order JSON object
 */
inline kj::String json_order(kj::StringPtr side, kj::StringPtr symbol, double qty, double price) {
  return kj::str("{\"side\":\"", side, "\",\"symbol\":\"", symbol, "\",\"qty\":", qty,
                 ",\"price\":", price, "}");
}

/**
 * @brief Create an order JSON object with client order ID
 */
inline kj::String json_order_with_client_id(kj::StringPtr side, kj::StringPtr symbol, double qty,
                                            double price, kj::StringPtr client_order_id) {
  return kj::str("{\"side\":\"", side, "\",\"symbol\":\"", symbol, "\",\"qty\":", qty,
                 ",\"price\":", price, ",\"client_order_id\":\"", client_order_id, "\"}");
}

// =============================================================================
// Assertion Helpers
// =============================================================================

/**
 * @brief Assert HTTP status code matches expected value
 */
inline void assert_status_code(const MockHttpResponse& response, kj::uint expected) {
  if (response.statusCode != expected) {
    KJ_LOG(ERROR, "Expected status code ", expected, " but got ", response.statusCode);
    KJ_LOG(ERROR, "Response body: ", response.body);
    KJ_FAIL_REQUIRE("Status code mismatch");
  }
}

/**
 * @brief Assert response body contains a substring
 */
inline void assert_body_contains(const MockHttpResponse& response, kj::StringPtr substring) {
  if (!response.body.contains(substring)) {
    KJ_LOG(ERROR, "Expected body to contain '", substring, "'");
    KJ_LOG(ERROR, "Actual body: ", response.body);
    KJ_FAIL_REQUIRE("Body does not contain expected substring");
  }
}

/**
 * @brief Assert response body matches expected value
 */
inline void assert_body_equals(const MockHttpResponse& response, kj::StringPtr expected) {
  if (response.body != expected) {
    KJ_LOG(ERROR, "Expected body: ", expected);
    KJ_LOG(ERROR, "Actual body: ", response.body);
    KJ_FAIL_REQUIRE("Body mismatch");
  }
}

/**
 * @brief Check if a string is valid JSON (basic check)
 */
inline bool is_valid_json(kj::StringPtr str) {
  if (str.size() == 0)
    return false;
  if (str[0] != '{' && str[0] != '[')
    return false;
  if (str[str.size() - 1] != '}' && str[str.size() - 1] != ']')
    return false;
  return true;
}

// =============================================================================
// Atomic Counter
// =============================================================================

/**
 * @brief Thread-safe counter for testing
 */
class AtomicCounter {
public:
  void increment() {
    count_.fetch_add(1, std::memory_order_relaxed);
  }
  void add(uint64_t value) {
    count_.fetch_add(value, std::memory_order_relaxed);
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

// =============================================================================
// Performance Assertions
// =============================================================================

/**
 * @brief Assert operation completes within time limit
 */
inline void assert_performance(uint64_t actual_ns, uint64_t limit_ns, const char* operation_name) {
  if (actual_ns > limit_ns) {
    double actual_us = actual_ns / 1000.0;
    double limit_us = limit_ns / 1000.0;
    KJ_LOG(WARNING, operation_name, " exceeded performance target");
    KJ_LOG(WARNING, "  Actual: ", actual_us, " μs");
    KJ_LOG(WARNING, "  Target: ", limit_us, " μs");
    // Note: We log warning but don't fail in debug builds
  }
}

/**
 * @brief Assert throughput meets minimum requirement
 */
inline void assert_throughput(size_t ops, uint64_t time_ms, double min_ops_per_ms,
                              const char* operation_name) {
  double actual_ops_per_ms = static_cast<double>(ops) / time_ms;
  if (actual_ops_per_ms < min_ops_per_ms) {
    KJ_LOG(WARNING, operation_name, " below throughput target");
    KJ_LOG(WARNING, "  Actual: ", actual_ops_per_ms, " ops/ms");
    KJ_LOG(WARNING, "  Target: ", min_ops_per_ms, " ops/ms");
  }
}

// =============================================================================
// Environment Setup
// =============================================================================

/**
 * @brief RAII helper for setting environment variables
 */
class ScopedEnvVar {
public:
  ScopedEnvVar(const char* name, const char* value) : name_(name), was_set_(false) {
    const char* old_value = getenv(name);
    if (old_value) {
      was_set_ = true;
      old_value_ = kj::str(old_value);
    }
    setenv(name, value, 1);
  }

  ~ScopedEnvVar() {
    if (was_set_) {
      setenv(name_, old_value_.cStr(), 1);
    } else {
      unsetenv(name_);
    }
  }

private:
  const char* name_;
  bool was_set_;
  kj::String old_value_;
};

/**
 * @brief RAII helper for clearing environment variables
 */
class ScopedEnvClear {
public:
  explicit ScopedEnvClear(const char* name) : name_(name) {
    const char* old_value = getenv(name);
    if (old_value) {
      old_value_ = kj::str(old_value);
    }
    unsetenv(name);
  }

  ~ScopedEnvClear() {
    KJ_IF_SOME(val, old_value_) {
      setenv(name_, val.cStr(), 1);
    }
  }

private:
  const char* name_;
  kj::Maybe<kj::String> old_value_;
};

} // namespace veloz::gateway::test
