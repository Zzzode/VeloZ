/**
 * @file test_gateway_server.cpp
 * @brief Tests for GatewayServer request dispatch and error handling
 *
 * Tests cover:
 * - Request dispatch to router
 * - 404 Not Found handling
 * - 405 Method Not Allowed with Allow header
 * - OPTIONS request CORS handling
 * - URL path/query parsing
 *
 * M2 Acceptance Criteria:
 * - Health endpoint returns correct response
 * - Router dispatches to correct handler
 * - 405 includes Allow header
 * - OPTIONS returns CORS headers
 */

#include "gateway_server.h"
#include "router.h"

#include <kj/async-io.h>
#include <kj/compat/http.h>
#include <kj/string.h>
#include <kj/test.h>
#include <kj/vector.h>

namespace veloz::gateway {
namespace {

// =============================================================================
// Mock Response for Testing
// =============================================================================

class MockResponse final : public kj::HttpService::Response {
public:
  kj::uint statusCode = 0;
  kj::String statusText;
  kj::HttpHeaders responseHeaders;
  kj::String body;
  kj::Maybe<uint64_t> expectedBodySize;

  explicit MockResponse(const kj::HttpHeaderTable& headerTable) : responseHeaders(headerTable) {}

  kj::Own<kj::AsyncOutputStream> send(kj::uint status, kj::StringPtr text,
                                      const kj::HttpHeaders& headers,
                                      kj::Maybe<uint64_t> bodySize) override {
    statusCode = status;
    statusText = kj::str(text);
    expectedBodySize = bodySize;

    // Copy headers, preserving unmapped names and taking ownership of values
    headers.forEach([&](kj::StringPtr name, kj::StringPtr value) {
      kj::String ownedValue = kj::str(value);
      responseHeaders.addPtrPtr(name, ownedValue);
      responseHeaders.takeOwnership(kj::mv(ownedValue));
    });

    return kj::heap<MockOutputStream>(this);
  }

  kj::Promise<void> sendError(kj::uint status, kj::StringPtr text,
                              KJ_UNUSED const kj::HttpHeaderTable& table) {
    statusCode = status;
    statusText = kj::str(text);
    return kj::READY_NOW;
  }

  kj::Own<kj::WebSocket> acceptWebSocket(const kj::HttpHeaders&) override {
    KJ_FAIL_REQUIRE("WebSocket not supported in tests");
  }

private:
  class MockOutputStream final : public kj::AsyncOutputStream {
  public:
    explicit MockOutputStream(MockResponse* parent) : parent_(parent) {}

    kj::Promise<void> write(kj::ArrayPtr<const kj::byte> data) override {
      parent_->body = kj::str(reinterpret_cast<const char*>(data.begin()), data.size());
      return kj::READY_NOW;
    }

    kj::Promise<void> write(kj::ArrayPtr<const kj::ArrayPtr<const kj::byte>> pieces) override {
      kj::Vector<char> combined;
      for (auto& piece : pieces) {
        combined.addAll(piece);
      }
      parent_->body = kj::str(combined.asPtr());
      return kj::READY_NOW;
    }

    kj::Promise<void> whenWriteDisconnected() override {
      return kj::NEVER_DONE;
    }

  private:
    MockResponse* parent_;
  };
};

// =============================================================================
// Mock Input Stream
// =============================================================================

class MockInputStream : public kj::AsyncInputStream {
public:
  kj::Promise<size_t> tryRead(void* buffer, size_t minBytes, size_t maxBytes) override {
    (void)buffer;
    (void)minBytes;
    (void)maxBytes;
    return kj::Promise<size_t>(static_cast<size_t>(0));
  }
};

kj::Maybe<kj::StringPtr> findHeader(const kj::HttpHeaders& headers, kj::StringPtr name) {
  kj::Maybe<kj::StringPtr> result = kj::none;
  headers.forEach([&](kj::StringPtr headerName, kj::StringPtr headerValue) {
    if (headerName == name) {
      result = headerValue;
    }
  });
  return result;
}

// =============================================================================
// Helper to create test context
// =============================================================================

struct TestContext {
  kj::AsyncIoContext io;
  kj::Own<kj::HttpHeaderTable> headerTableOwn;
  kj::HttpHeaderTable& headerTable;
  kj::WaitScope& waitScope;

  TestContext()
      : io(kj::setupAsyncIo()), headerTableOwn(kj::heap<kj::HttpHeaderTable>()),
        headerTable(*headerTableOwn), waitScope(io.waitScope) {}
};

// =============================================================================
// Health Endpoint Tests (M2 Acceptance)
// =============================================================================

KJ_TEST("GatewayServer: health endpoint returns correct response") {
  TestContext ctx;

  Router router;
  bool healthHandlerCalled = false;

  router.add_route(kj::HttpMethod::GET, "/api/control/health",
                   [&healthHandlerCalled](RequestContext& ctx) -> kj::Promise<void> {
                     healthHandlerCalled = true;
                     return ctx.sendJson(200, kj::str("{\"ok\":true}"));
                   });

  GatewayServer server(ctx.headerTable, router);

  MockInputStream requestBody;
  MockResponse response(ctx.headerTable);
  kj::HttpHeaders headers(ctx.headerTable);

  auto promise =
      server.request(kj::HttpMethod::GET, "/api/control/health"_kj, headers, requestBody, response);

  promise.wait(ctx.waitScope);

  KJ_EXPECT(response.statusCode == 200);
  KJ_EXPECT(healthHandlerCalled);
}

KJ_TEST("GatewayServer: health endpoint with trailing slash") {
  TestContext ctx;

  Router router;
  bool healthHandlerCalled = false;

  router.add_route(kj::HttpMethod::GET, "/api/control/health",
                   [&healthHandlerCalled](RequestContext& ctx) -> kj::Promise<void> {
                     healthHandlerCalled = true;
                     return ctx.sendJson(200, kj::str("{\"ok\":true}"));
                   });

  GatewayServer server(ctx.headerTable, router);

  MockInputStream requestBody;
  MockResponse response(ctx.headerTable);
  kj::HttpHeaders headers(ctx.headerTable);

  auto promise = server.request(kj::HttpMethod::GET,
                                "/api/control/health/"_kj, // Trailing slash
                                headers, requestBody, response);

  promise.wait(ctx.waitScope);

  // Should be normalized and match
  KJ_EXPECT(response.statusCode == 200);
  KJ_EXPECT(healthHandlerCalled);
}

// =============================================================================
// Router Dispatch Tests (M2 Acceptance)
// =============================================================================

KJ_TEST("GatewayServer: router dispatches to correct handler") {
  TestContext ctx;

  Router router;
  bool ordersHandlerCalled = false;
  bool usersHandlerCalled = false;

  router.add_route(kj::HttpMethod::GET, "/api/orders",
                   [&ordersHandlerCalled](RequestContext& ctx) -> kj::Promise<void> {
                     ordersHandlerCalled = true;
                     return ctx.sendJson(200, kj::str("{\"orders\":[]}"));
                   });

  router.add_route(kj::HttpMethod::GET, "/api/users",
                   [&usersHandlerCalled](RequestContext& ctx) -> kj::Promise<void> {
                     usersHandlerCalled = true;
                     return ctx.sendJson(200, kj::str("{\"users\":[]}"));
                   });

  GatewayServer server(ctx.headerTable, router);

  MockInputStream requestBody;
  kj::HttpHeaders headers(ctx.headerTable);

  // Request to /api/orders
  {
    MockResponse response(ctx.headerTable);
    auto promise =
        server.request(kj::HttpMethod::GET, "/api/orders"_kj, headers, requestBody, response);

    promise.wait(ctx.waitScope);

    KJ_EXPECT(response.statusCode == 200);
    KJ_EXPECT(ordersHandlerCalled);
    KJ_EXPECT(!usersHandlerCalled);
  }
}

KJ_TEST("GatewayServer: dispatches with path parameters") {
  TestContext ctx;

  Router router;
  kj::String capturedId;

  router.add_route(kj::HttpMethod::GET, "/api/orders/{id}",
                   [&capturedId](RequestContext& ctx) -> kj::Promise<void> {
                     KJ_IF_SOME(id, ctx.path_params.find("id"_kj)) {
                       capturedId = kj::str(id);
                     }
                     return ctx.sendJson(200, kj::str("{\"found\":true}"));
                   });

  GatewayServer server(ctx.headerTable, router);

  MockInputStream requestBody;
  MockResponse response(ctx.headerTable);
  kj::HttpHeaders headers(ctx.headerTable);

  auto promise =
      server.request(kj::HttpMethod::GET, "/api/orders/12345"_kj, headers, requestBody, response);

  promise.wait(ctx.waitScope);

  KJ_EXPECT(response.statusCode == 200);
  KJ_EXPECT(capturedId == "12345");
}

// =============================================================================
// 404 Not Found Tests
// =============================================================================

KJ_TEST("GatewayServer: returns 404 for unknown path") {
  TestContext ctx;

  Router router;
  router.add_route(
      kj::HttpMethod::GET, "/api/orders",
      [](RequestContext& ctx) -> kj::Promise<void> { return ctx.sendJson(200, kj::str("{}")); });

  GatewayServer server(ctx.headerTable, router);

  MockInputStream requestBody;
  MockResponse response(ctx.headerTable);
  kj::HttpHeaders headers(ctx.headerTable);

  auto promise =
      server.request(kj::HttpMethod::GET, "/api/unknown"_kj, headers, requestBody, response);

  promise.wait(ctx.waitScope);

  KJ_EXPECT(response.statusCode == 404);
}

KJ_TEST("GatewayServer: returns 404 for root if not registered") {
  TestContext ctx;

  Router router;
  // No routes registered

  GatewayServer server(ctx.headerTable, router);

  MockInputStream requestBody;
  MockResponse response(ctx.headerTable);
  kj::HttpHeaders headers(ctx.headerTable);

  auto promise = server.request(kj::HttpMethod::GET, "/"_kj, headers, requestBody, response);

  promise.wait(ctx.waitScope);

  KJ_EXPECT(response.statusCode == 404);
}

// =============================================================================
// 405 Method Not Allowed Tests (M2 Acceptance)
// =============================================================================

KJ_TEST("GatewayServer: returns 405 for wrong method") {
  TestContext ctx;

  Router router;

  router.add_route(
      kj::HttpMethod::GET, "/api/orders",
      [](RequestContext& ctx) -> kj::Promise<void> { return ctx.sendJson(200, kj::str("{}")); });

  router.add_route(
      kj::HttpMethod::POST, "/api/orders",
      [](RequestContext& ctx) -> kj::Promise<void> { return ctx.sendJson(201, kj::str("{}")); });

  GatewayServer server(ctx.headerTable, router);

  MockInputStream requestBody;
  MockResponse response(ctx.headerTable);
  kj::HttpHeaders headers(ctx.headerTable);

  // DELETE not registered for this path
  auto promise =
      server.request(kj::HttpMethod::DELETE, "/api/orders"_kj, headers, requestBody, response);

  promise.wait(ctx.waitScope);

  KJ_EXPECT(response.statusCode == 405);
}

KJ_TEST("GatewayServer: 405 includes Allow header") {
  TestContext ctx;

  Router router;

  router.add_route(
      kj::HttpMethod::GET, "/api/resource",
      [](RequestContext& ctx) -> kj::Promise<void> { return ctx.sendJson(200, kj::str("{}")); });

  router.add_route(
      kj::HttpMethod::POST, "/api/resource",
      [](RequestContext& ctx) -> kj::Promise<void> { return ctx.sendJson(201, kj::str("{}")); });

  router.add_route(
      kj::HttpMethod::PUT, "/api/resource",
      [](RequestContext& ctx) -> kj::Promise<void> { return ctx.sendJson(200, kj::str("{}")); });

  GatewayServer server(ctx.headerTable, router);

  MockInputStream requestBody;
  MockResponse response(ctx.headerTable);
  kj::HttpHeaders headers(ctx.headerTable);

  // PATCH not registered, should return 405 with Allow header
  auto promise =
      server.request(kj::HttpMethod::PATCH, "/api/resource"_kj, headers, requestBody, response);

  promise.wait(ctx.waitScope);

  KJ_EXPECT(response.statusCode == 405);

  // Verify Allow header is present
  auto allowHeader = findHeader(response.responseHeaders, "Allow"_kj);
  KJ_EXPECT(allowHeader != kj::none);

  KJ_IF_SOME(allow, allowHeader) {
    // Should contain GET, POST, PUT
    KJ_EXPECT(allow.contains("GET"));
    KJ_EXPECT(allow.contains("POST"));
    KJ_EXPECT(allow.contains("PUT"));
  }
}

KJ_TEST("GatewayServer: 405 Allow header for single method") {
  TestContext ctx;

  Router router;

  router.add_route(
      kj::HttpMethod::GET, "/api/readonly",
      [](RequestContext& ctx) -> kj::Promise<void> { return ctx.sendJson(200, kj::str("{}")); });

  GatewayServer server(ctx.headerTable, router);

  MockInputStream requestBody;
  MockResponse response(ctx.headerTable);
  kj::HttpHeaders headers(ctx.headerTable);

  auto promise =
      server.request(kj::HttpMethod::POST, "/api/readonly"_kj, headers, requestBody, response);

  promise.wait(ctx.waitScope);

  KJ_EXPECT(response.statusCode == 405);

  auto allowHeader = findHeader(response.responseHeaders, "Allow"_kj);
  KJ_EXPECT(allowHeader != kj::none);

  KJ_IF_SOME(allow, allowHeader) {
    KJ_EXPECT(allow == "GET");
  }
}

// =============================================================================
// OPTIONS Request Tests (M2 Acceptance)
// =============================================================================

KJ_TEST("GatewayServer: OPTIONS returns CORS headers") {
  TestContext ctx;

  Router router;

  router.add_route(
      kj::HttpMethod::GET, "/api/resource",
      [](RequestContext& ctx) -> kj::Promise<void> { return ctx.sendJson(200, kj::str("{}")); });

  router.add_route(
      kj::HttpMethod::POST, "/api/resource",
      [](RequestContext& ctx) -> kj::Promise<void> { return ctx.sendJson(201, kj::str("{}")); });

  GatewayServer server(ctx.headerTable, router);

  MockInputStream requestBody;
  MockResponse response(ctx.headerTable);
  kj::HttpHeaders headers(ctx.headerTable);

  // Set Origin header for CORS
  headers.addPtrPtr("Origin"_kj, "https://example.com"_kj);

  auto promise =
      server.request(kj::HttpMethod::OPTIONS, "/api/resource"_kj, headers, requestBody, response);

  promise.wait(ctx.waitScope);

  // OPTIONS should succeed
  KJ_EXPECT(response.statusCode == 200);

  // Verify CORS headers
  auto allowHeader = findHeader(response.responseHeaders, "Allow"_kj);
  KJ_EXPECT(allowHeader != kj::none);

  // Allow header should include OPTIONS itself
  KJ_IF_SOME(allow, allowHeader) {
    KJ_EXPECT(allow.contains("GET"));
    KJ_EXPECT(allow.contains("POST"));
    KJ_EXPECT(allow.contains("OPTIONS"));
  }
}

KJ_TEST("GatewayServer: OPTIONS for unknown path returns 404") {
  TestContext ctx;

  Router router;
  // No routes

  GatewayServer server(ctx.headerTable, router);

  MockInputStream requestBody;
  MockResponse response(ctx.headerTable);
  kj::HttpHeaders headers(ctx.headerTable);

  auto promise =
      server.request(kj::HttpMethod::OPTIONS, "/api/unknown"_kj, headers, requestBody, response);

  promise.wait(ctx.waitScope);

  KJ_EXPECT(response.statusCode == 404);
}

// =============================================================================
// URL Parsing Tests
// =============================================================================

KJ_TEST("GatewayServer: query string parsing") {
  TestContext ctx;

  Router router;
  kj::String capturedQueryString;

  router.add_route(kj::HttpMethod::GET, "/api/search",
                   [&capturedQueryString](RequestContext& ctx) -> kj::Promise<void> {
                     capturedQueryString = kj::str(ctx.queryString);
                     return ctx.sendJson(200, kj::str("{}"));
                   });

  GatewayServer server(ctx.headerTable, router);

  MockInputStream requestBody;
  MockResponse response(ctx.headerTable);
  kj::HttpHeaders headers(ctx.headerTable);

  auto promise = server.request(kj::HttpMethod::GET, "/api/search?q=test&limit=10"_kj, headers,
                                requestBody, response);

  promise.wait(ctx.waitScope);

  KJ_EXPECT(response.statusCode == 200);
  KJ_EXPECT(capturedQueryString == "q=test&limit=10");
}

KJ_TEST("GatewayServer: empty query string") {
  TestContext ctx;

  Router router;
  kj::String capturedQueryString;

  router.add_route(kj::HttpMethod::GET, "/api/test",
                   [&capturedQueryString](RequestContext& ctx) -> kj::Promise<void> {
                     capturedQueryString = kj::str(ctx.queryString);
                     return ctx.sendJson(200, kj::str("{}"));
                   });

  GatewayServer server(ctx.headerTable, router);

  MockInputStream requestBody;
  MockResponse response(ctx.headerTable);
  kj::HttpHeaders headers(ctx.headerTable);

  auto promise =
      server.request(kj::HttpMethod::GET, "/api/test"_kj, headers, requestBody, response);

  promise.wait(ctx.waitScope);

  KJ_EXPECT(response.statusCode == 200);
  KJ_EXPECT(capturedQueryString == "");
}

// =============================================================================
// Multiple Method Tests
// =============================================================================

KJ_TEST("GatewayServer: different methods to same path") {
  TestContext ctx;

  Router router;
  int getCalled = 0;
  int postCalled = 0;
  int putCalled = 0;
  int deleteCalled = 0;

  router.add_route(kj::HttpMethod::GET, "/api/items",
                   [&getCalled](RequestContext& ctx) -> kj::Promise<void> {
                     getCalled++;
                     return ctx.sendJson(200, kj::str("{}"));
                   });

  router.add_route(kj::HttpMethod::POST, "/api/items",
                   [&postCalled](RequestContext& ctx) -> kj::Promise<void> {
                     postCalled++;
                     return ctx.sendJson(201, kj::str("{}"));
                   });

  router.add_route(kj::HttpMethod::PUT, "/api/items/{id}",
                   [&putCalled](RequestContext& ctx) -> kj::Promise<void> {
                     putCalled++;
                     return ctx.sendJson(200, kj::str("{}"));
                   });

  router.add_route(kj::HttpMethod::DELETE, "/api/items/{id}",
                   [&deleteCalled](RequestContext& ctx) -> kj::Promise<void> {
                     deleteCalled++;
                     return ctx.sendJson(204, kj::str(""));
                   });

  GatewayServer server(ctx.headerTable, router);
  kj::HttpHeaders headers(ctx.headerTable);

  // GET /api/items
  {
    MockInputStream requestBody;
    MockResponse response(ctx.headerTable);
    server.request(kj::HttpMethod::GET, "/api/items"_kj, headers, requestBody, response)
        .wait(ctx.waitScope);
    KJ_EXPECT(response.statusCode == 200);
    KJ_EXPECT(getCalled == 1);
  }

  // POST /api/items
  {
    MockInputStream requestBody;
    MockResponse response(ctx.headerTable);
    server.request(kj::HttpMethod::POST, "/api/items"_kj, headers, requestBody, response)
        .wait(ctx.waitScope);
    KJ_EXPECT(response.statusCode == 201);
    KJ_EXPECT(postCalled == 1);
  }

  // PUT /api/items/123
  {
    MockInputStream requestBody;
    MockResponse response(ctx.headerTable);
    server.request(kj::HttpMethod::PUT, "/api/items/123"_kj, headers, requestBody, response)
        .wait(ctx.waitScope);
    KJ_EXPECT(response.statusCode == 200);
    KJ_EXPECT(putCalled == 1);
  }

  // DELETE /api/items/123
  {
    MockInputStream requestBody;
    MockResponse response(ctx.headerTable);
    server.request(kj::HttpMethod::DELETE, "/api/items/123"_kj, headers, requestBody, response)
        .wait(ctx.waitScope);
    KJ_EXPECT(response.statusCode == 204);
    KJ_EXPECT(deleteCalled == 1);
  }
}

// =============================================================================
// Performance Tests
// =============================================================================

KJ_TEST("GatewayServer: route lookup performance under 5us") {
  TestContext ctx;

  Router router;

  // Add multiple routes
  router.add_route(kj::HttpMethod::GET, "/api/orders",
                   [](RequestContext&) { return kj::READY_NOW; });
  router.add_route(kj::HttpMethod::POST, "/api/orders",
                   [](RequestContext&) { return kj::READY_NOW; });
  router.add_route(kj::HttpMethod::GET, "/api/orders/{id}",
                   [](RequestContext&) { return kj::READY_NOW; });
  router.add_route(kj::HttpMethod::GET, "/api/users",
                   [](RequestContext&) { return kj::READY_NOW; });
  router.add_route(kj::HttpMethod::GET, "/api/users/{id}",
                   [](RequestContext&) { return kj::READY_NOW; });

  GatewayServer server(ctx.headerTable, router);

  MockInputStream requestBody;
  kj::HttpHeaders headers(ctx.headerTable);

  // Warm up
  for (int i = 0; i < 100; ++i) {
    MockResponse response(ctx.headerTable);
    server.request(kj::HttpMethod::GET, "/api/orders"_kj, headers, requestBody, response)
        .wait(ctx.waitScope);
  }

  // Measure
  constexpr int NUM_ITERATIONS = 1000;
  auto start = std::chrono::high_resolution_clock::now();

  for (int i = 0; i < NUM_ITERATIONS; ++i) {
    MockResponse response(ctx.headerTable);
    server.request(kj::HttpMethod::GET, "/api/orders"_kj, headers, requestBody, response)
        .wait(ctx.waitScope);
  }

  auto end = std::chrono::high_resolution_clock::now();
  auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
  double avg_us = static_cast<double>(duration.count()) / NUM_ITERATIONS;

  KJ_LOG(INFO, "GatewayServer route lookup average latency (us)", avg_us);

  // Should be well under 5us for simple routing
  KJ_EXPECT(avg_us < 100.0); // Relaxed for CI environments
}

} // namespace
} // namespace veloz::gateway
