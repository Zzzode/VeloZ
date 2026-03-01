#include "router.h"

#include <kj/async-io.h>
#include <kj/compat/http.h>
#include <kj/test.h>

namespace veloz::gateway {

// Mock headers and response for tests
struct MockResponse {
  kj::Own<kj::AsyncOutputStream> send(kj::uint, kj::StringPtr, const kj::HttpHeaders&,
                                      kj::Maybe<uint64_t>) {
    return kj::heap<kj::NullStream>();
  }
  kj::Own<kj::WebSocket> acceptWebSocket(const kj::HttpHeaders&) {
    KJ_FAIL_REQUIRE("not supported");
  }
};

// Test exact path matching
KJ_TEST("Router: exact path matching") {
  Router router;

  router.add_route(kj::HttpMethod::GET, "/api/orders",
                   [](RequestContext&) { return kj::READY_NOW; });

  KJ_EXPECT(router.route_count() == 1);

  // Match exact path
  KJ_IF_SOME(match, router.match(kj::HttpMethod::GET, "/api/orders")) {
    (void)match;
    KJ_EXPECT(true);
  }
  else {
    KJ_FAIL_REQUIRE("Expected match for exact path");
  }

  // Non-matching path
  KJ_IF_SOME(match, router.match(kj::HttpMethod::GET, "/api/orders/123")) {
    (void)match;
    KJ_FAIL_REQUIRE("Should not match longer path");
  }

  // Non-matching method
  KJ_IF_SOME(match, router.match(kj::HttpMethod::POST, "/api/orders")) {
    (void)match;
    KJ_FAIL_REQUIRE("Should not match different method");
  }
}

// Test parameter extraction
KJ_TEST("Router: parameter extraction") {
  Router router;

  router.add_route(kj::HttpMethod::GET, "/api/orders/{id}",
                   [](RequestContext&) { return kj::READY_NOW; });

  KJ_EXPECT(router.route_count() == 1);

  // Match with parameter
  KJ_IF_SOME(match, router.match(kj::HttpMethod::GET, "/api/orders/12345")) {
    KJ_IF_SOME(id, match.path_params.find("id"_kj)) {
      KJ_EXPECT(id == "12345");
    }
    else {
      KJ_FAIL_REQUIRE("Expected 'id' parameter");
    }
  }
  else {
    KJ_FAIL_REQUIRE("Expected match for parameterized path");
  }

  // Different parameter value
  KJ_IF_SOME(match, router.match(kj::HttpMethod::GET, "/api/orders/abc-xyz")) {
    KJ_IF_SOME(id, match.path_params.find("id"_kj)) {
      KJ_EXPECT(id == "abc-xyz");
    }
    else {
      KJ_FAIL_REQUIRE("Expected 'id' parameter");
    }
  }
  else {
    KJ_FAIL_REQUIRE("Expected match for parameterized path");
  }
}

// Test multiple parameters
KJ_TEST("Router: multiple parameters") {
  Router router;

  router.add_route(kj::HttpMethod::GET, "/api/users/{userId}/orders/{orderId}",
                   [](RequestContext&) { return kj::READY_NOW; });

  KJ_IF_SOME(match, router.match(kj::HttpMethod::GET, "/api/users/42/orders/100")) {
    KJ_IF_SOME(userId, match.path_params.find("userId"_kj)) {
      KJ_EXPECT(userId == "42");
    }
    else {
      KJ_FAIL_REQUIRE("Expected 'userId' parameter");
    }

    KJ_IF_SOME(orderId, match.path_params.find("orderId"_kj)) {
      KJ_EXPECT(orderId == "100");
    }
    else {
      KJ_FAIL_REQUIRE("Expected 'orderId' parameter");
    }
  }
  else {
    KJ_FAIL_REQUIRE("Expected match for multi-parameter path");
  }
}

// Test method-based routing
KJ_TEST("Router: method-based routing") {
  Router router;
  bool get_called = false;
  bool post_called = false;
  bool put_called = false;
  bool delete_called = false;

  router.add_route(kj::HttpMethod::GET, "/api/resource", [&get_called](RequestContext&) {
    get_called = true;
    return kj::READY_NOW;
  });

  router.add_route(kj::HttpMethod::POST, "/api/resource", [&post_called](RequestContext&) {
    post_called = true;
    return kj::READY_NOW;
  });

  router.add_route(kj::HttpMethod::PUT, "/api/resource", [&put_called](RequestContext&) {
    put_called = true;
    return kj::READY_NOW;
  });

  router.add_route(kj::HttpMethod::DELETE, "/api/resource", [&delete_called](RequestContext&) {
    delete_called = true;
    return kj::READY_NOW;
  });

  KJ_EXPECT(router.route_count() == 4);

  // Match GET
  KJ_IF_SOME(match, router.match(kj::HttpMethod::GET, "/api/resource")) {
    (void)match;
    KJ_EXPECT(true);
  }
  else {
    KJ_FAIL_REQUIRE("Expected GET match");
  }

  // Match POST
  KJ_IF_SOME(match, router.match(kj::HttpMethod::POST, "/api/resource")) {
    (void)match;
    KJ_EXPECT(true);
  }
  else {
    KJ_FAIL_REQUIRE("Expected POST match");
  }

  // Match PUT
  KJ_IF_SOME(match, router.match(kj::HttpMethod::PUT, "/api/resource")) {
    (void)match;
    KJ_EXPECT(true);
  }
  else {
    KJ_FAIL_REQUIRE("Expected PUT match");
  }

  // Match DELETE
  KJ_IF_SOME(match, router.match(kj::HttpMethod::DELETE, "/api/resource")) {
    (void)match;
    KJ_EXPECT(true);
  }
  else {
    KJ_FAIL_REQUIRE("Expected DELETE match");
  }
}

// Test 404 behavior
KJ_TEST("Router: 404 for unknown path") {
  Router router;

  router.add_route(kj::HttpMethod::GET, "/api/orders",
                   [](RequestContext&) { return kj::READY_NOW; });

  // Unknown path should not match
  KJ_IF_SOME(match, router.match(kj::HttpMethod::GET, "/api/unknown")) {
    (void)match;
    KJ_FAIL_REQUIRE("Should not match unknown path");
  }

  // Verify has_path returns false
  KJ_EXPECT(!router.has_path("/api/unknown"));
}

// Test 405 behavior
KJ_TEST("Router: 405 for wrong method") {
  Router router;

  router.add_route(kj::HttpMethod::GET, "/api/orders",
                   [](RequestContext&) { return kj::READY_NOW; });

  router.add_route(kj::HttpMethod::POST, "/api/orders",
                   [](RequestContext&) { return kj::READY_NOW; });

  // Path exists but DELETE method not allowed
  KJ_IF_SOME(match, router.match(kj::HttpMethod::DELETE, "/api/orders")) {
    (void)match;
    KJ_FAIL_REQUIRE("Should not match DELETE method");
  }

  // Path exists for GET and POST
  KJ_EXPECT(router.has_path("/api/orders"));

  // Get allowed methods
  auto methods = router.get_methods_for_path("/api/orders");
  KJ_EXPECT(methods.size() == 2);
}

// Test root path
KJ_TEST("Router: root path") {
  Router router;

  router.add_route(kj::HttpMethod::GET, "/", [](RequestContext&) { return kj::READY_NOW; });

  KJ_IF_SOME(match, router.match(kj::HttpMethod::GET, "/")) {
    (void)match;
    KJ_EXPECT(true);
  }
  else {
    KJ_FAIL_REQUIRE("Expected match for root path");
  }
}

// Test path normalization
KJ_TEST("Router: path normalization") {
  Router router;

  router.add_route(kj::HttpMethod::GET, "/api/orders",
                   [](RequestContext&) { return kj::READY_NOW; });

  // Trailing slash should be normalized
  KJ_IF_SOME(match, router.match(kj::HttpMethod::GET, "/api/orders/")) {
    (void)match;
    KJ_EXPECT(true);
  }
  else {
    KJ_FAIL_REQUIRE("Expected match with trailing slash");
  }

  // Leading slash already present
  KJ_IF_SOME(match, router.match(kj::HttpMethod::GET, "/api/orders")) {
    (void)match;
    KJ_EXPECT(true);
  }
  else {
    KJ_FAIL_REQUIRE("Expected match without trailing slash");
  }
}

// Test multiple routes with priorities
KJ_TEST("Router: multiple routes with priorities") {
  Router router;

  // More specific route first
  router.add_route(kj::HttpMethod::GET, "/api/orders/{id}",
                   [](RequestContext&) { return kj::READY_NOW; });

  // General list route
  router.add_route(kj::HttpMethod::GET, "/api/orders",
                   [](RequestContext&) { return kj::READY_NOW; });

  KJ_EXPECT(router.route_count() == 2);

  // Should match exact path
  KJ_IF_SOME(match, router.match(kj::HttpMethod::GET, "/api/orders")) {
    KJ_EXPECT(match.path_params.size() == 0);
  }
  else {
    KJ_FAIL_REQUIRE("Expected match for exact path");
  }

  // Should match parameterized path
  KJ_IF_SOME(match, router.match(kj::HttpMethod::GET, "/api/orders/123")) {
    KJ_IF_SOME(id, match.path_params.find("id"_kj)) {
      KJ_EXPECT(id == "123");
    }
    else {
      KJ_FAIL_REQUIRE("Expected 'id' parameter");
    }
  }
  else {
    KJ_FAIL_REQUIRE("Expected match for parameterized path");
  }
}

// Test many routes (performance)
KJ_TEST("Router: support 100+ routes") {
  Router router;

  // Add 100 routes
  for (int i = 0; i < 100; i++) {
    kj::String pattern = kj::str("/api/route", i);
    router.add_route(kj::HttpMethod::GET, pattern, [](RequestContext&) { return kj::READY_NOW; });
  }

  KJ_EXPECT(router.route_count() == 100);

  // Test matching last route
  KJ_IF_SOME(match, router.match(kj::HttpMethod::GET, "/api/route99")) {
    (void)match;
    KJ_EXPECT(true);
  }
  else {
    KJ_FAIL_REQUIRE("Expected match for route99");
  }

  // Test matching first route
  KJ_IF_SOME(match, router.match(kj::HttpMethod::GET, "/api/route0")) {
    (void)match;
    KJ_EXPECT(true);
  }
  else {
    KJ_FAIL_REQUIRE("Expected match for route0");
  }
}

// Test nested paths
KJ_TEST("Router: nested paths") {
  Router router;

  router.add_route(kj::HttpMethod::GET,
                   "/api/v1/users/{userId}/posts/{postId}/comments/{commentId}",
                   [](RequestContext&) { return kj::READY_NOW; });

  KJ_IF_SOME(match, router.match(kj::HttpMethod::GET, "/api/v1/users/1/posts/2/comments/3")) {
    KJ_IF_SOME(userId, match.path_params.find("userId"_kj)) {
      KJ_EXPECT(userId == "1");
    }
    else {
      KJ_FAIL_REQUIRE("Expected userId");
    }

    KJ_IF_SOME(postId, match.path_params.find("postId"_kj)) {
      KJ_EXPECT(postId == "2");
    }
    else {
      KJ_FAIL_REQUIRE("Expected postId");
    }

    KJ_IF_SOME(commentId, match.path_params.find("commentId"_kj)) {
      KJ_EXPECT(commentId == "3");
    }
    else {
      KJ_FAIL_REQUIRE("Expected commentId");
    }
  }
  else {
    KJ_FAIL_REQUIRE("Expected match for deeply nested path");
  }
}

// Test mixed literal and parameter segments
KJ_TEST("Router: mixed literal and parameter segments") {
  Router router;

  router.add_route(kj::HttpMethod::GET, "/api/orders/{orderId}/items/{itemId}",
                   [](RequestContext&) { return kj::READY_NOW; });

  // Correct path
  KJ_IF_SOME(match, router.match(kj::HttpMethod::GET, "/api/orders/123/items/456")) {
    (void)match;
    KJ_EXPECT(true);
  }
  else {
    KJ_FAIL_REQUIRE("Expected match");
  }

  // Wrong literal segment
  KJ_IF_SOME(match, router.match(kj::HttpMethod::GET, "/api/orders/123/products/456")) {
    (void)match;
    KJ_FAIL_REQUIRE("Should not match wrong literal");
  }

  // Too short
  KJ_IF_SOME(match, router.match(kj::HttpMethod::GET, "/api/orders/123/items")) {
    (void)match;
    KJ_FAIL_REQUIRE("Should not match incomplete path");
  }
}

// Test get_methods_for_path
KJ_TEST("Router: get_methods_for_path") {
  Router router;

  router.add_route(kj::HttpMethod::GET, "/api/resource",
                   [](RequestContext&) { return kj::READY_NOW; });

  router.add_route(kj::HttpMethod::POST, "/api/resource",
                   [](RequestContext&) { return kj::READY_NOW; });

  router.add_route(kj::HttpMethod::PUT, "/api/resource/{id}",
                   [](RequestContext&) { return kj::READY_NOW; });

  // Check methods for exact path
  auto methods = router.get_methods_for_path("/api/resource");
  KJ_EXPECT(methods.size() == 2);

  // Check methods for parameterized path
  auto methods_with_param = router.get_methods_for_path("/api/resource/123");
  KJ_EXPECT(methods_with_param.size() == 1);
}

} // namespace veloz::gateway
