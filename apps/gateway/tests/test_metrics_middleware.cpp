#include "kj/thread.h"
#include "middleware/metrics_middleware.h"

#include <chrono>
#include <kj/async-io.h>
#include <kj/compat/http.h>
#include <kj/test.h>
#include <thread>
#include <veloz/core/metrics.h>

namespace veloz::gateway {

namespace {

// Test metrics middleware initialization
KJ_TEST("MetricsMiddleware: initialization") {
  veloz::core::MetricsRegistry registry;
  MetricsMiddleware middleware(registry);

  // Verify metrics are registered
  auto counter_names = registry.counter_names();
  KJ_EXPECT(counter_names.size() == 2);

  auto histogram_names = registry.histogram_names();
  KJ_EXPECT(histogram_names.size() == 1);

  auto gauge_names = registry.gauge_names();
  KJ_EXPECT(gauge_names.size() == 1);
}

// Test status code categorization
KJ_TEST("MetricsMiddleware: status categorization") {
  veloz::core::MetricsRegistry registry;
  MetricsMiddleware middleware(registry);

  // Test 2xx status codes
  KJ_EXPECT(middleware.categorize_status(200) == "2xx");
  KJ_EXPECT(middleware.categorize_status(201) == "2xx");
  KJ_EXPECT(middleware.categorize_status(204) == "2xx");
  KJ_EXPECT(middleware.categorize_status(299) == "2xx");

  // Test 3xx status codes
  KJ_EXPECT(middleware.categorize_status(300) == "3xx");
  KJ_EXPECT(middleware.categorize_status(301) == "3xx");
  KJ_EXPECT(middleware.categorize_status(304) == "3xx");
  KJ_EXPECT(middleware.categorize_status(399) == "3xx");

  // Test 4xx status codes
  KJ_EXPECT(middleware.categorize_status(400) == "4xx");
  KJ_EXPECT(middleware.categorize_status(401) == "4xx");
  KJ_EXPECT(middleware.categorize_status(404) == "4xx");
  KJ_EXPECT(middleware.categorize_status(499) == "4xx");

  // Test 5xx status codes
  KJ_EXPECT(middleware.categorize_status(500) == "5xx");
  KJ_EXPECT(middleware.categorize_status(502) == "5xx");
  KJ_EXPECT(middleware.categorize_status(503) == "5xx");
  KJ_EXPECT(middleware.categorize_status(599) == "5xx");

  // Test unknown status codes
  KJ_EXPECT(middleware.categorize_status(100) == "unknown");
  KJ_EXPECT(middleware.categorize_status(199) == "unknown");
  KJ_EXPECT(middleware.categorize_status(600) == "unknown");
}

// Test path normalization
KJ_TEST("MetricsMiddleware: path normalization") {
  veloz::core::MetricsRegistry registry;
  MetricsMiddleware middleware(registry);

  // Test path with numeric ID
  KJ_EXPECT(middleware.normalize_path("/api/orders/123") == "/api/orders/{id}");
  KJ_EXPECT(middleware.normalize_path("/api/orders/456/items/789") ==
            "/api/orders/{id}/items/{id}");

  // Test path without IDs
  KJ_EXPECT(middleware.normalize_path("/api/orders") == "/api/orders");
  KJ_EXPECT(middleware.normalize_path("/api/market") == "/api/market");

  // Test root path
  KJ_EXPECT(middleware.normalize_path("/") == "/");

  // Test path with mixed segments
  KJ_EXPECT(middleware.normalize_path("/api/v1/users/42/posts/100") ==
            "/api/v1/users/{id}/posts/{id}");

  // Test path with alphanumeric segments (not normalized)
  KJ_EXPECT(middleware.normalize_path("/api/users/abc-123") == "/api/users/abc-123");
  KJ_EXPECT(middleware.normalize_path("/api/orders/order-xyz") == "/api/orders/order-xyz");
}

// Test metric recording
KJ_TEST("MetricsMiddleware: request counting") {
  veloz::core::MetricsRegistry registry;
  MetricsMiddleware middleware(registry);

  // Get the counter
  auto counter = registry.counter("http_requests_total");
  KJ_EXPECT(counter != nullptr);

  // Initial value should be 0
  KJ_EXPECT(counter->value() == 0);

  // Simulate requests
  // Create mock request context
  struct MockRequestContext {
    kj::HttpMethod method = kj::HttpMethod::GET;
    kj::StringPtr path = "/api/orders";
    const kj::HttpHeaders& headers;
    kj::AsyncInputStream& body;
    kj::HttpService::Response& response;
    kj::HashMap<kj::String, kj::String> path_params;
  };

  // Record some requests
  middleware.record_request(kj::HttpMethod::GET, "/api/orders", 200, 0.001);
  KJ_EXPECT(counter->value() == 1);

  middleware.record_request(kj::HttpMethod::POST, "/api/orders", 201, 0.002);
  KJ_EXPECT(counter->value() == 2);

  middleware.record_request(kj::HttpMethod::GET, "/api/orders/123", 404, 0.0005);
  KJ_EXPECT(counter->value() == 3);

  middleware.record_request(kj::HttpMethod::GET, "/api/market", 500, 0.1);
  KJ_EXPECT(counter->value() == 4);
}

// Test duration histogram
KJ_TEST("MetricsMiddleware: duration histogram") {
  veloz::core::MetricsRegistry registry;
  MetricsMiddleware middleware(registry);

  auto histogram = registry.histogram("http_request_duration_seconds");
  KJ_EXPECT(histogram != nullptr);

  // Record various durations
  middleware.record_request(kj::HttpMethod::GET, "/api/orders", 200, 0.001);
  middleware.record_request(kj::HttpMethod::GET, "/api/orders", 200, 0.005);
  middleware.record_request(kj::HttpMethod::GET, "/api/orders", 200, 0.01);
  middleware.record_request(kj::HttpMethod::GET, "/api/orders", 200, 0.05);
  middleware.record_request(kj::HttpMethod::GET, "/api/orders", 200, 0.1);

  // Verify histogram count
  KJ_EXPECT(histogram->count() == 5);

  // Verify sum
  double expected_sum = 0.001 + 0.005 + 0.01 + 0.05 + 0.1;
  double actual_sum = histogram->sum();
  // Allow small floating point error
  KJ_EXPECT(actual_sum > expected_sum - 0.0001);
  KJ_EXPECT(actual_sum < expected_sum + 0.0001);
}

// Test active connections gauge
KJ_TEST("MetricsMiddleware: active connections gauge") {
  veloz::core::MetricsRegistry registry;
  MetricsMiddleware middleware(registry);

  auto gauge = registry.gauge("http_active_connections");
  KJ_EXPECT(gauge != nullptr);

  // Initial value should be 0
  KJ_EXPECT(gauge->value() == 0);

  // Simulate connection increment/decrement
  gauge->increment();
  KJ_EXPECT(gauge->value() == 1);

  gauge->increment();
  KJ_EXPECT(gauge->value() == 2);

  gauge->decrement();
  KJ_EXPECT(gauge->value() == 1);

  gauge->decrement();
  KJ_EXPECT(gauge->value() == 0);
}

// Test timing accuracy
KJ_TEST("MetricsMiddleware: timing accuracy") {
  veloz::core::MetricsRegistry registry;
  MetricsMiddleware middleware(registry);

  auto histogram = registry.histogram("http_request_duration_seconds");
  KJ_EXPECT(histogram != nullptr);

  // Record a known duration
  auto start = std::chrono::steady_clock::now();
  std::this_thread::sleep_for(std::chrono::milliseconds(10));
  auto end = std::chrono::steady_clock::now();

  auto duration_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);
  double duration_sec = static_cast<double>(duration_ns.count()) / 1e9;

  middleware.record_request(kj::HttpMethod::GET, "/api/orders", 200, duration_sec);

  // Verify histogram recorded the duration
  KJ_EXPECT(histogram->count() == 1);
  double recorded_sum = histogram->sum();
  KJ_EXPECT(recorded_sum >= 0.009); // At least 9ms
  KJ_EXPECT(recorded_sum < 0.1);    // Less than 100ms
}

// Test concurrent metric recording
KJ_TEST("MetricsMiddleware: concurrent recording") {
  veloz::core::MetricsRegistry registry;
  MetricsMiddleware middleware(registry);

  auto counter = registry.counter("http_requests_total");
  KJ_EXPECT(counter != nullptr);

  const int numThreads = 4;
  const int requestsPerThread = 100;

  kj::Vector<kj::Own<kj::Thread>> threads;
  for (int t = 0; t < numThreads; t++) {
    threads.add(kj::heap<kj::Thread>([&middleware, t]() {
      for (int i = 0; i < requestsPerThread; i++) {
        kj::String path = kj::str("/api/orders/", t * 1000 + i);
        middleware.record_request(kj::HttpMethod::GET, path, 200, 0.001);
      }
    }));
  }

  // Wait for all threads
  threads.clear();

  // Verify total count
  KJ_EXPECT(counter->value() == numThreads * requestsPerThread);
}

// Test different HTTP methods
KJ_TEST("MetricsMiddleware: different HTTP methods") {
  veloz::core::MetricsRegistry registry;
  MetricsMiddleware middleware(registry);

  auto counter = registry.counter("http_requests_total");
  KJ_EXPECT(counter != nullptr);

  // Test different methods
  middleware.record_request(kj::HttpMethod::GET, "/api/orders", 200, 0.001);
  middleware.record_request(kj::HttpMethod::POST, "/api/orders", 201, 0.002);
  middleware.record_request(kj::HttpMethod::PUT, "/api/orders/123", 200, 0.003);
  middleware.record_request(kj::HttpMethod::DELETE, "/api/orders/123", 204, 0.001);
  middleware.record_request(kj::HttpMethod::PATCH, "/api/orders/123", 200, 0.002);

  KJ_EXPECT(counter->value() == 5);
}

// Test error status codes
KJ_TEST("MetricsMiddleware: error status codes") {
  veloz::core::MetricsRegistry registry;
  MetricsMiddleware middleware(registry);

  auto counter = registry.counter("http_requests_total");
  KJ_EXPECT(counter != nullptr);

  // Test various error codes
  middleware.record_request(kj::HttpMethod::GET, "/api/orders", 200, 0.001);
  middleware.record_request(kj::HttpMethod::GET, "/api/orders/999", 404, 0.0005);
  middleware.record_request(kj::HttpMethod::POST, "/api/orders", 400, 0.001);
  middleware.record_request(kj::HttpMethod::GET, "/api/market", 500, 0.1);
  middleware.record_request(kj::HttpMethod::GET, "/api/orders", 503, 0.05);

  KJ_EXPECT(counter->value() == 5);
}

// Test histogram bucket distribution
KJ_TEST("MetricsMiddleware: histogram bucket distribution") {
  veloz::core::MetricsRegistry registry;
  MetricsMiddleware middleware(registry);

  auto histogram = registry.histogram("http_request_duration_seconds");
  KJ_EXPECT(histogram != nullptr);

  // Record durations in different buckets
  middleware.record_request(kj::HttpMethod::GET, "/api/orders", 200,
                            0.0005);                                         // 0.5ms - bucket 0.001
  middleware.record_request(kj::HttpMethod::GET, "/api/orders", 200, 0.002); // 2ms - bucket 0.005
  middleware.record_request(kj::HttpMethod::GET, "/api/orders", 200, 0.008); // 8ms - bucket 0.01
  middleware.record_request(kj::HttpMethod::GET, "/api/orders", 200, 0.03);  // 30ms - bucket 0.05
  middleware.record_request(kj::HttpMethod::GET, "/api/orders", 200, 0.08);  // 80ms - bucket 0.1

  KJ_EXPECT(histogram->count() == 5);

  // Verify bucket counts
  auto bucket_counts = histogram->bucket_counts();
  KJ_EXPECT(bucket_counts.size() > 0);

  // Sum should be around 0.0005 + 0.002 + 0.008 + 0.03 + 0.08 = 0.1205
  double sum = histogram->sum();
  KJ_EXPECT(sum > 0.12);
  KJ_EXPECT(sum < 0.13);
}

// Test multiple middleware instances
KJ_TEST("MetricsMiddleware: multiple instances") {
  veloz::core::MetricsRegistry registry;

  // Create multiple middleware instances sharing the same registry
  MetricsMiddleware middleware1(registry);
  MetricsMiddleware middleware2(registry);

  auto counter = registry.counter("http_requests_total");
  KJ_EXPECT(counter != nullptr);

  // Record from different middleware instances
  middleware1.record_request(kj::HttpMethod::GET, "/api/orders", 200, 0.001);
  middleware2.record_request(kj::HttpMethod::GET, "/api/orders", 200, 0.002);

  // Should aggregate in the same counter
  KJ_EXPECT(counter->value() == 2);
}

// Test zero duration
KJ_TEST("MetricsMiddleware: zero duration") {
  veloz::core::MetricsRegistry registry;
  MetricsMiddleware middleware(registry);

  auto histogram = registry.histogram("http_request_duration_seconds");
  KJ_EXPECT(histogram != nullptr);

  // Record zero duration
  middleware.record_request(kj::HttpMethod::GET, "/api/orders", 200, 0.0);

  KJ_EXPECT(histogram->count() == 1);
  KJ_EXPECT(histogram->sum() == 0.0);
}

// Test very long duration
KJ_TEST("MetricsMiddleware: long duration") {
  veloz::core::MetricsRegistry registry;
  MetricsMiddleware middleware(registry);

  auto histogram = registry.histogram("http_request_duration_seconds");
  KJ_EXPECT(histogram != nullptr);

  // Record a very long duration
  middleware.record_request(kj::HttpMethod::GET, "/api/orders", 200, 5.0);

  KJ_EXPECT(histogram->count() == 1);
  KJ_EXPECT(histogram->sum() == 5.0);
}

// Test path with query string (should be ignored for normalization)
KJ_TEST("MetricsMiddleware: path with query string") {
  veloz::core::MetricsRegistry registry;
  MetricsMiddleware middleware(registry);

  // Note: The current implementation doesn't handle query strings,
  // but we test that it doesn't crash
  auto normalized = middleware.normalize_path("/api/orders/123?symbol=BTCUSDT");
  // Should normalize the path portion
  KJ_EXPECT(normalized.startsWith("/api/orders"));
}

// Test edge cases in status categorization
KJ_TEST("MetricsMiddleware: edge case status codes") {
  veloz::core::MetricsRegistry registry;
  MetricsMiddleware middleware(registry);

  // Test boundary cases
  KJ_EXPECT(middleware.categorize_status(199) == "unknown");
  KJ_EXPECT(middleware.categorize_status(200) == "2xx");
  KJ_EXPECT(middleware.categorize_status(299) == "2xx");
  KJ_EXPECT(middleware.categorize_status(300) == "3xx");
  KJ_EXPECT(middleware.categorize_status(399) == "3xx");
  KJ_EXPECT(middleware.categorize_status(400) == "4xx");
  KJ_EXPECT(middleware.categorize_status(499) == "4xx");
  KJ_EXPECT(middleware.categorize_status(500) == "5xx");
  KJ_EXPECT(middleware.categorize_status(599) == "5xx");
  KJ_EXPECT(middleware.categorize_status(600) == "unknown");
}

// Test performance: rapid metric recording
KJ_TEST("MetricsMiddleware: performance test") {
  veloz::core::MetricsRegistry registry;
  MetricsMiddleware middleware(registry);

  auto counter = registry.counter("http_requests_total");
  auto histogram = registry.histogram("http_request_duration_seconds");

  // Record 10,000 requests rapidly
  auto start = std::chrono::steady_clock::now();

  for (int i = 0; i < 10000; i++) {
    middleware.record_request(kj::HttpMethod::GET, "/api/orders", 200, 0.001);
  }

  auto end = std::chrono::steady_clock::now();
  auto duration_us = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();

  KJ_EXPECT(counter->value() == 10000);
  KJ_EXPECT(histogram->count() == 10000);

  // Performance target: <10μs overhead per request
  // So 10,000 requests should take <100,000μs (100ms)
  double avg_us = static_cast<double>(duration_us) / 10000.0;
  KJ_EXPECT(avg_us < 10.0); // Less than 10μs per request
}

} // namespace
} // namespace veloz::gateway
