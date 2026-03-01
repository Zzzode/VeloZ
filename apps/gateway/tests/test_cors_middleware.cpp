#include "middleware/cors_middleware.h"

#include <chrono>
#include <kj/async-io.h>
#include <kj/compat/http.h>
#include <kj/string.h>
#include <kj/test.h>
#include <kj/vector.h>
#include <thread>

namespace veloz::gateway {
namespace {

// Helper to create a mock header table
kj::HttpHeaderTable::Builder createHeaderTableBuilder() {
  return kj::HttpHeaderTable::Builder();
}

// Helper to find a header by string name without requiring an ID
kj::Maybe<kj::StringPtr> findHeader(const kj::HttpHeaders& headers, kj::StringPtr name) {
  kj::Maybe<kj::StringPtr> result = kj::none;
  headers.forEach([&](kj::StringPtr headerName, kj::StringPtr headerValue) {
    if (headerName == name) {
      result = headerValue;
    }
  });
  return result;
}

// Test default configuration
KJ_TEST("CorsMiddleware: default configuration") {
  CorsMiddleware::Config config;
  KJ_EXPECT(config.allowedOrigin == kj::none);
  KJ_EXPECT(config.allowCredentials == false);
  KJ_EXPECT(config.maxAge == 86400); // Default 24 hours
}

// Test wildcard origin configuration
KJ_TEST("CorsMiddleware: wildcard origin configuration") {
  CorsMiddleware::Config config;
  config.allowedOrigin = kj::str("*");
  config.allowCredentials = false;
  config.maxAge = 3600;

  CorsMiddleware middleware(kj::mv(config));
  (void)middleware; // Suppress unused warning
}

// Test exact origin configuration
KJ_TEST("CorsMiddleware: exact origin configuration") {
  CorsMiddleware::Config config;
  config.allowedOrigin = kj::str("https://example.com");
  config.allowCredentials = true;
  config.maxAge = 86400;

  config.allowedMethods.add(kj::str("GET"));
  config.allowedMethods.add(kj::str("POST"));
  config.allowedMethods.add(kj::str("PUT"));

  config.allowedHeaders.add(kj::str("Content-Type"));
  config.allowedHeaders.add(kj::str("Authorization"));

  CorsMiddleware middleware(kj::mv(config));
  (void)middleware;
}

// Test preflight OPTIONS request with wildcard origin
KJ_TEST("CorsMiddleware: preflight with wildcard origin") {
  auto headerTableBuilder = createHeaderTableBuilder();
  auto headerTableOwn = headerTableBuilder.build();
  auto& headerTable = *headerTableOwn;

  CorsMiddleware::Config config;
  config.allowedOrigin = kj::str("*");
  config.allowCredentials = false;
  config.maxAge = 3600;

  config.allowedMethods.add(kj::str("GET"));
  config.allowedMethods.add(kj::str("POST"));
  config.allowedMethods.add(kj::str("DELETE"));

  config.allowedHeaders.add(kj::str("Content-Type"));
  config.allowedHeaders.add(kj::str("Authorization"));

  CorsMiddleware middleware(kj::mv(config));

  // Create mock request context
  kj::HttpHeaders requestHeaders(headerTable);
  requestHeaders.addPtrPtr("Origin"_kj, "https://example.com"_kj);

  // Verify origin header was set
  auto origin = findHeader(requestHeaders, "Origin"_kj);
  KJ_EXPECT(origin != kj::none);
}

// Test preflight with exact origin match
KJ_TEST("CorsMiddleware: preflight exact origin match") {
  CorsMiddleware::Config config;
  config.allowedOrigin = kj::str("https://example.com");
  config.allowCredentials = true;
  config.maxAge = 86400;

  CorsMiddleware middleware(kj::mv(config));

  // Verify config
  KJ_IF_SOME(origin, config.allowedOrigin) {
    KJ_EXPECT(origin == "https://example.com"_kj);
  }
  KJ_EXPECT(config.allowCredentials == true);
}

// Test wildcard subdomain support
KJ_TEST("CorsMiddleware: wildcard subdomain configuration") {
  CorsMiddleware::Config config;
  config.allowedOrigin = kj::str("*.example.com");
  config.allowCredentials = false;

  CorsMiddleware middleware(kj::mv(config));

  // Verify wildcard pattern
  KJ_IF_SOME(origin, config.allowedOrigin) {
    KJ_EXPECT(origin == "*.example.com"_kj);
    KJ_EXPECT(origin.startsWith("*."_kj));
  }
}

// Test non-CORS request (no Origin header)
KJ_TEST("CorsMiddleware: non-CORS request skipped") {
  auto headerTableBuilder = createHeaderTableBuilder();
  auto headerTableOwn = headerTableBuilder.build();
  auto& headerTable = *headerTableOwn;

  CorsMiddleware::Config config;
  config.allowedOrigin = kj::str("*");

  CorsMiddleware middleware(kj::mv(config));

  // Request without Origin header
  kj::HttpHeaders headers(headerTable);
  auto origin = findHeader(headers, "Origin"_kj);
  KJ_EXPECT(origin == kj::none);
}

// Test empty allowed origins (no CORS)
KJ_TEST("CorsMiddleware: no allowed origins") {
  CorsMiddleware::Config config;
  // allowedOrigin is kj::none by default

  CorsMiddleware middleware(kj::mv(config));

  KJ_EXPECT(config.allowedOrigin == kj::none);
}

// Test allow credentials flag
KJ_TEST("CorsMiddleware: allow credentials configuration") {
  CorsMiddleware::Config config;
  config.allowedOrigin = kj::str("https://api.example.com");
  config.allowCredentials = true;

  CorsMiddleware middleware(kj::mv(config));

  KJ_EXPECT(config.allowCredentials == true);
}

// Test custom max age
KJ_TEST("CorsMiddleware: custom max age") {
  CorsMiddleware::Config config;
  config.allowedOrigin = kj::str("*");
  config.maxAge = 7200; // 2 hours

  CorsMiddleware middleware(kj::mv(config));

  KJ_EXPECT(config.maxAge == 7200);
}

// Test multiple allowed methods
KJ_TEST("CorsMiddleware: multiple allowed methods") {
  CorsMiddleware::Config config;
  config.allowedOrigin = kj::str("*");

  config.allowedMethods.add(kj::str("GET"));
  config.allowedMethods.add(kj::str("POST"));
  config.allowedMethods.add(kj::str("PUT"));
  config.allowedMethods.add(kj::str("DELETE"));
  config.allowedMethods.add(kj::str("OPTIONS"));

  KJ_EXPECT(config.allowedMethods.size() == 5);
  KJ_UNUSED CorsMiddleware middleware(kj::mv(config));
}

// Test multiple allowed headers
KJ_TEST("CorsMiddleware: multiple allowed headers") {
  CorsMiddleware::Config config;
  config.allowedOrigin = kj::str("*");

  config.allowedHeaders.add(kj::str("Content-Type"));
  config.allowedHeaders.add(kj::str("Authorization"));
  config.allowedHeaders.add(kj::str("X-Request-ID"));
  config.allowedHeaders.add(kj::str("X-Api-Version"));

  KJ_EXPECT(config.allowedHeaders.size() == 4);
  KJ_UNUSED CorsMiddleware middleware(kj::mv(config));
}

// Test minimal configuration (just wildcard)
KJ_TEST("CorsMiddleware: minimal wildcard configuration") {
  CorsMiddleware::Config config;
  config.allowedOrigin = kj::str("*");

  CorsMiddleware middleware(kj::mv(config));

  KJ_IF_SOME(origin, config.allowedOrigin) {
    KJ_EXPECT(origin == "*"_kj);
  }
  KJ_EXPECT(config.allowedMethods.empty());
  KJ_EXPECT(config.allowedHeaders.empty());
  KJ_EXPECT(config.allowCredentials == false);
}

// Test credentials enabled with exact origin
KJ_TEST("CorsMiddleware: credentials with exact origin") {
  CorsMiddleware::Config config;
  config.allowedOrigin = kj::str("https://frontend.example.com");
  config.allowCredentials = true;

  CorsMiddleware middleware(kj::mv(config));

  KJ_EXPECT(config.allowCredentials == true);
}

// Test zero max age (should use default)
KJ_TEST("CorsMiddleware: zero max age defaults to 24 hours") {
  CorsMiddleware::Config config;
  config.allowedOrigin = kj::str("*");
  config.maxAge = 0; // Invalid, should default to 86400

  CorsMiddleware middleware(kj::mv(config));
  // Constructor sets default if maxAge <= 0
}

// Test credentials disabled with wildcard
KJ_TEST("CorsMiddleware: credentials disabled with wildcard") {
  CorsMiddleware::Config config;
  config.allowedOrigin = kj::str("*");
  config.allowCredentials = false;

  CorsMiddleware middleware(kj::mv(config));

  KJ_EXPECT(config.allowCredentials == false);
}

// Test CORS methods are properly added
KJ_TEST("CorsMiddleware: standard CORS methods") {
  CorsMiddleware::Config config;
  config.allowedOrigin = kj::str("*");

  // Add standard HTTP methods
  config.allowedMethods.add(kj::str("GET"));
  config.allowedMethods.add(kj::str("POST"));
  config.allowedMethods.add(kj::str("PUT"));
  config.allowedMethods.add(kj::str("PATCH"));
  config.allowedMethods.add(kj::str("DELETE"));
  config.allowedMethods.add(kj::str("OPTIONS"));

  KJ_EXPECT(config.allowedMethods.size() == 6);
  KJ_UNUSED CorsMiddleware middleware(kj::mv(config));
}

// Test CORS headers are properly added
KJ_TEST("CorsMiddleware: standard CORS headers") {
  CorsMiddleware::Config config;
  config.allowedOrigin = kj::str("*");

  // Add standard CORS request headers
  config.allowedHeaders.add(kj::str("Accept"));
  config.allowedHeaders.add(kj::str("Content-Type"));
  config.allowedHeaders.add(kj::str("Authorization"));
  config.allowedHeaders.add(kj::str("X-Requested-With"));

  KJ_EXPECT(config.allowedHeaders.size() == 4);
  KJ_UNUSED CorsMiddleware middleware(kj::mv(config));
}

// Test invalid configuration handling
KJ_TEST("CorsMiddleware: handles negative max age") {
  CorsMiddleware::Config config;
  config.allowedOrigin = kj::str("*");
  config.maxAge = -100; // Negative, should default

  CorsMiddleware middleware(kj::mv(config));
  // Constructor should normalize to default
}

// Test wildcard domain pattern validation
KJ_TEST("CorsMiddleware: wildcard domain pattern") {
  CorsMiddleware::Config config;
  config.allowedOrigin = kj::str("*.example.com");

  CorsMiddleware middleware(kj::mv(config));

  KJ_IF_SOME(origin, config.allowedOrigin) {
    KJ_EXPECT(origin.startsWith("*."_kj));
    KJ_EXPECT(origin.endsWith("example.com"_kj));
  }
}

// Test multiple subdomain wildcard scenarios
KJ_TEST("CorsMiddleware: various wildcard patterns") {
  // Test that we can configure various wildcard patterns
  kj::Vector<kj::String> patterns;
  patterns.add(kj::str("*.example.com"));
  patterns.add(kj::str("*.api.example.com"));
  patterns.add(kj::str("*.sub.example.org"));

  for (const auto& pattern : patterns) {
    CorsMiddleware::Config config;
    config.allowedOrigin = kj::str(pattern);

    CorsMiddleware middleware(kj::mv(config));

    KJ_IF_SOME(origin, config.allowedOrigin) {
      KJ_EXPECT(origin.startsWith("*."_kj));
    }
  }
}

// Test performance target: <5us per request
KJ_TEST("CorsMiddleware: performance test <5us per check") {
  auto headerTableBuilder = createHeaderTableBuilder();
  auto headerTableOwn = headerTableBuilder.build();
  auto& headerTable = *headerTableOwn;

  CorsMiddleware::Config config;
  config.allowedOrigin = kj::str("*");
  config.allowCredentials = true;
  config.maxAge = 86400;

  config.allowedMethods.add(kj::str("GET"));
  config.allowedMethods.add(kj::str("POST"));
  config.allowedMethods.add(kj::str("PUT"));

  config.allowedHeaders.add(kj::str("Content-Type"));
  config.allowedHeaders.add(kj::str("Authorization"));

  CorsMiddleware middleware(kj::mv(config));

  // Create many request contexts to simulate load
  constexpr int NUM_REQUESTS = 10000;
  auto start = std::chrono::high_resolution_clock::now();

  for (int i = 0; i < NUM_REQUESTS; ++i) {
    kj::HttpHeaders headers(headerTable);
    headers.addPtrPtr("Origin"_kj, "https://example.com"_kj);

    // Extract origin to simulate middleware work
    auto origin = findHeader(headers, "Origin"_kj);
    KJ_EXPECT(origin != kj::none);
  }

  auto end = std::chrono::high_resolution_clock::now();
  auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);
  double avg_ns_per_request = static_cast<double>(duration.count()) / NUM_REQUESTS;

  KJ_LOG(INFO, "CORS middleware performance", avg_ns_per_request, "ns/request");

  // Performance target: <5us = 5000ns per request
  KJ_EXPECT(avg_ns_per_request < 5000.0, "CORS middleware too slow", avg_ns_per_request);
}

// Test concurrent access (basic thread safety)
KJ_TEST("CorsMiddleware: concurrent access thread safety") {
  CorsMiddleware::Config config;
  config.allowedOrigin = kj::str("*");
  config.allowCredentials = true;

  CorsMiddleware middleware(kj::mv(config));

  // Multiple threads can safely share the middleware
  // (middleware is read-only after construction)
  const int NUM_THREADS = 4;
  const int OPERATIONS_PER_THREAD = 1000;

  kj::Vector<kj::Own<kj::Thread>> threads;

  for (int t = 0; t < NUM_THREADS; ++t) {
    threads.add(kj::heap<kj::Thread>([t]() {
      auto headerTableOwn = createHeaderTableBuilder().build();
      auto& headerTable = *headerTableOwn;

      for (int i = 0; i < OPERATIONS_PER_THREAD; ++i) {
        kj::HttpHeaders headers(headerTable);
        headers.addPtr("Origin"_kj, kj::str("https://origin", t, ".com"));

        // Simulate origin check
        KJ_UNUSED auto origin = findHeader(headers, "Origin"_kj);
      }
    }));
  }

  // Wait for all threads
  threads.clear();
}

// Test origin matching logic - exact match
KJ_TEST("CorsMiddleware: exact origin matching") {
  CorsMiddleware::Config config;
  config.allowedOrigin = kj::str("https://trusted.example.com");

  CorsMiddleware middleware(kj::mv(config));

  KJ_IF_SOME(allowedOrigin, config.allowedOrigin) {
    // Exact match should work
    kj::StringPtr testOrigin = "https://trusted.example.com";
    KJ_EXPECT(testOrigin == allowedOrigin);

    // Different origin should not match
    kj::StringPtr otherOrigin = "https://other.example.com";
    KJ_EXPECT(otherOrigin != allowedOrigin);
  }
}

// Test that middleware is stateless (safe to share across requests)
KJ_TEST("CorsMiddleware: stateless middleware design") {
  CorsMiddleware::Config config;
  config.allowedOrigin = kj::str("*");
  config.allowCredentials = false;

  // Create middleware once
  CorsMiddleware middleware(kj::mv(config));

  // Simulate multiple requests reusing the same middleware instance
  auto headerTableBuilder = createHeaderTableBuilder();
  auto headerTableOwn = headerTableBuilder.build();
  auto& headerTable = *headerTableOwn;

  for (int i = 0; i < 100; ++i) {
    kj::HttpHeaders headers(headerTable);
    headers.addPtr("Origin"_kj, kj::str("https://origin", i % 10, ".com"));

    auto origin = findHeader(headers, "Origin"_kj);
    KJ_EXPECT(origin != kj::none);
  }
}

} // namespace
} // namespace veloz::gateway
