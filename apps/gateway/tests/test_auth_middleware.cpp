#include "auth/api_key_manager.h"
#include "auth/auth_manager.h"
#include "auth/jwt_manager.h"
#include "auth/rbac.h"
#include "middleware/auth_middleware.h"

#include <kj/async-io.h>
#include <kj/compat/http.h>
#include <kj/test.h>

namespace veloz::gateway {
namespace {

using namespace auth;

// ============================================================================
// Test Fixtures
// ============================================================================

class AuthTestFixture {
public:
  AuthTestFixture()
      : header_table_(kj::HttpHeaderTable::Builder().build()),
        jwt_manager_(kj::heap<JwtManager>("test-secret-key-for-jwt-testing-32b!"_kj)),
        api_key_manager_(kj::heap<ApiKeyManager>()),
        auth_manager_(
            kj::heap<AuthManager>(kj::heap<JwtManager>("test-secret-key-for-jwt-testing-32b!"_kj),
                                  kj::heap<ApiKeyManager>())) {}

  // Get the API key manager from auth_manager_ for tests that need to create keys
  // Note: This is a different instance than api_key_manager_ for JWT-only tests
  ApiKeyManager& auth_api_key_manager() {
    return auth_manager_->api_key_manager();
  }

  JwtManager& auth_jwt_manager() {
    return auth_manager_->jwt_manager();
  }

public:
  kj::Own<kj::HttpHeaderTable> header_table_;
  kj::Own<JwtManager> jwt_manager_;
  kj::Own<ApiKeyManager> api_key_manager_;
  kj::Own<AuthManager> auth_manager_;

  // Helper to create request context
  struct MockResponse : public kj::HttpService::Response {
    kj::Own<kj::AsyncOutputStream> send(uint status, kj::StringPtr statusText,
                                        const kj::HttpHeaders& headers,
                                        kj::Maybe<uint64_t> expectedBodySize = kj::none) override {
      (void)headers;
      (void)expectedBodySize;
      sent_status = status;
      sent_status_text = kj::str(statusText);
      return kj::heap<kj::NullStream>();
    }

    kj::Own<kj::WebSocket> acceptWebSocket(const kj::HttpHeaders&) override {
      KJ_FAIL_REQUIRE("WebSocket not supported");
    }

    uint sent_status = 0;
    kj::String sent_status_text;
  };

  // Storage for owned data that RequestContext references
  struct TestContextStorage {
    kj::HttpMethod method = kj::HttpMethod::GET;
    kj::String path = kj::str("/api/test");
    kj::String queryString = kj::str();
    kj::Own<kj::HttpHeaders> headers;
    kj::Own<kj::AsyncInputStream> body;
    MockResponse response;
    kj::String clientIP = kj::str("127.0.0.1");
    kj::Maybe<AuthInfo> authInfo;
    kj::HashMap<kj::String, kj::String> path_params;
  };

  kj::Own<TestContextStorage> createTestStorage() {
    auto storage = kj::heap<TestContextStorage>();
    storage->headers = kj::heap<kj::HttpHeaders>(*header_table_);
    storage->body = kj::heap<kj::NullStream>();
    return storage;
  }

  // Creates a RequestContext that references the storage's data
  // Note: path_params and authInfo are default-constructed since they are value members
  RequestContext makeRequestContext(TestContextStorage& storage) {
    return RequestContext{
        .method = storage.method,
        .path = storage.path,
        .queryString = storage.queryString,
        .headers = *storage.headers,
        .body = *storage.body,
        .response = storage.response,
        .headerTable = *header_table_,
        .path_params = {},
        .authInfo = kj::none,
        .clientIP = kj::str(storage.clientIP),
    };
  }
};

// ============================================================================
// AuthManager Tests
// ============================================================================

KJ_TEST("AuthManager: authenticate with valid JWT") {
  AuthTestFixture fixture;

  // Create a valid JWT token
  kj::String token = fixture.jwt_manager_->create_access_token("user123"_kj);

  // Create headers with Bearer token
  kj::HttpHeaders headers(*fixture.header_table_);
  headers.addPtr("Authorization"_kj, kj::str("Bearer ", token));

  // Authenticate
  auto result = fixture.auth_manager_->authenticate(headers);

  KJ_IF_SOME(auth_info, result) {
    KJ_EXPECT(auth_info.user_id == "user123");
    KJ_EXPECT(auth_info.auth_method == "jwt");
  }
  else {
    KJ_FAIL_ASSERT("Expected successful authentication");
  }
}

KJ_TEST("AuthManager: authenticate with invalid JWT") {
  AuthTestFixture fixture;

  // Create headers with invalid token
  kj::HttpHeaders headers(*fixture.header_table_);
  headers.addPtrPtr("Authorization"_kj, "Bearer invalid_token"_kj);

  // Authenticate
  auto result = fixture.auth_manager_->authenticate(headers);

  KJ_EXPECT(result == kj::none, "Expected authentication to fail with invalid token");
}

KJ_TEST("AuthManager: authenticate with expired JWT") {
  AuthTestFixture fixture;

  // Create a token with very short expiry (already expired in the past)
  // Note: We'd need to modify JwtManager to support this test
  // For now, test with a manually constructed expired token

  kj::HttpHeaders headers(*fixture.header_table_);
  headers.addPtrPtr("Authorization"_kj, "Bearer expired.token.here"_kj);

  auto result = fixture.auth_manager_->authenticate(headers);

  KJ_EXPECT(result == kj::none, "Expected authentication to fail with expired token");
}

KJ_TEST("AuthManager: authenticate with valid API key") {
  AuthTestFixture fixture;

  // Create an API key using the auth_manager_'s api_key_manager
  auto key_pair = fixture.auth_api_key_manager().create_key("user456"_kj, "Test Key"_kj, [&]() {
    kj::Vector<kj::String> permissions;
    permissions.add(kj::str("trade:read"));
    permissions.add(kj::str("trade:write"));
    return permissions;
  }());

  // Create headers with API key
  kj::HttpHeaders headers(*fixture.header_table_);
  headers.addPtrPtr("X-API-Key"_kj, key_pair.raw_key);

  // Authenticate
  auto result = fixture.auth_manager_->authenticate(headers);

  KJ_IF_SOME(auth_info, result) {
    KJ_EXPECT(auth_info.user_id == "user456");
    KJ_EXPECT(auth_info.auth_method == "api_key");
    KJ_EXPECT(auth_info.permissions.size() == 2);

    // Check permissions
    bool has_read = false, has_write = false;
    for (auto& perm : auth_info.permissions) {
      if (perm == "trade:read")
        has_read = true;
      if (perm == "trade:write")
        has_write = true;
    }
    KJ_EXPECT(has_read && has_write);
  }
  else {
    KJ_FAIL_ASSERT("Expected successful API key authentication");
  }
}

KJ_TEST("AuthManager: authenticate with invalid API key") {
  AuthTestFixture fixture;

  kj::HttpHeaders headers(*fixture.header_table_);
  headers.addPtrPtr("X-API-Key"_kj, "invalid_api_key_12345"_kj);

  auto result = fixture.auth_manager_->authenticate(headers);

  KJ_EXPECT(result == kj::none, "Expected authentication to fail with invalid API key");
}

KJ_TEST("AuthManager: API key takes priority over JWT") {
  AuthTestFixture fixture;

  // Create both a JWT token and an API key
  kj::String token = fixture.jwt_manager_->create_access_token("jwt_user"_kj);
  auto key_pair = fixture.auth_api_key_manager().create_key("apikey_user"_kj, "Test Key"_kj, [&]() {
    kj::Vector<kj::String> permissions;
    permissions.add(kj::str("test:perm"));
    return permissions;
  }());

  // Create headers with both
  kj::HttpHeaders headers(*fixture.header_table_);
  headers.addPtr("Authorization"_kj, kj::str("Bearer ", token));
  headers.addPtrPtr("X-API-Key"_kj, key_pair.raw_key);

  // Authenticate - should use API key
  auto result = fixture.auth_manager_->authenticate(headers);

  KJ_IF_SOME(auth_info, result) {
    KJ_EXPECT(auth_info.user_id == "apikey_user", "Expected API key user, not JWT user");
    KJ_EXPECT(auth_info.auth_method == "api_key");
  }
  else {
    KJ_FAIL_ASSERT("Expected successful authentication");
  }
}

KJ_TEST("AuthManager: no authentication when no credentials provided") {
  AuthTestFixture fixture;

  kj::HttpHeaders headers(*fixture.header_table_);

  auto result = fixture.auth_manager_->authenticate(headers);

  KJ_EXPECT(result == kj::none, "Expected no authentication without credentials");
}

KJ_TEST("AuthManager: has_permission checks API key permissions") {
  AuthTestFixture fixture;

  // Create API key with specific permissions
  auto key_pair = fixture.auth_api_key_manager().create_key("user789"_kj, "Test Key"_kj, [&]() {
    kj::Vector<kj::String> permissions;
    permissions.add(kj::str("read:orders"));
    permissions.add(kj::str("write:orders"));
    return permissions;
  }());

  kj::HttpHeaders headers(*fixture.header_table_);
  headers.addPtrPtr("X-API-Key"_kj, key_pair.raw_key);

  auto result = fixture.auth_manager_->authenticate(headers);

  KJ_IF_SOME(auth_info, result) {
    KJ_EXPECT(fixture.auth_manager_->has_permission(auth_info, "read:orders"));
    KJ_EXPECT(fixture.auth_manager_->has_permission(auth_info, "write:orders"));
    KJ_EXPECT(!fixture.auth_manager_->has_permission(auth_info, "delete:orders"));
  }
  else {
    KJ_FAIL_ASSERT("Expected successful authentication");
  }
}

// ============================================================================
// AuthMiddleware Tests
// ============================================================================

KJ_TEST("AuthMiddleware: default config has correct public paths") {
  auto config = AuthMiddleware::default_config();

  KJ_EXPECT(config.require_auth);
  KJ_EXPECT(config.public_paths.size() >= 5);

  // Check for specific public paths
  bool has_health = false, has_auth_login = false;
  for (auto& path : config.public_paths) {
    if (path == "/health")
      has_health = true;
    if (path == "/api/auth/login")
      has_auth_login = true;
  }
  KJ_EXPECT(has_health);
  KJ_EXPECT(has_auth_login);
}

KJ_TEST("AuthMiddleware: allows public paths without auth") {
  AuthTestFixture fixture;

  auto middleware = kj::heap<AuthMiddleware>(
      kj::heap<AuthManager>(kj::heap<JwtManager>("test-secret-key-for-jwt-testing-32b!"_kj),
                            kj::heap<ApiKeyManager>()),
      nullptr, AuthMiddleware::default_config());

  auto storage = fixture.createTestStorage();
  storage->path = kj::str("/health");
  RequestContext ctx = fixture.makeRequestContext(*storage);

  bool next_called = false;
  auto next = [&next_called]() -> kj::Promise<void> {
    next_called = true;
    return kj::READY_NOW;
  };

  auto result = middleware->process(ctx, kj::mv(next));
  kj::EventLoop loop;
  kj::WaitScope waitScope(loop);
  result.wait(waitScope);

  KJ_EXPECT(next_called, "Expected middleware to call next() for public path");
}

KJ_TEST("AuthMiddleware: rejects protected paths without auth") {
  AuthTestFixture fixture;

  auto middleware = kj::heap<AuthMiddleware>(
      kj::heap<AuthManager>(kj::heap<JwtManager>("test-secret-key-for-jwt-testing-32b!"_kj),
                            kj::heap<ApiKeyManager>()),
      nullptr, AuthMiddleware::default_config());

  auto storage = fixture.createTestStorage();
  storage->path = kj::str("/api/orders"); // Protected path
  RequestContext ctx = fixture.makeRequestContext(*storage);

  bool next_called = false;
  auto next = [&next_called]() -> kj::Promise<void> {
    next_called = true;
    return kj::READY_NOW;
  };

  auto result = middleware->process(ctx, kj::mv(next));
  kj::EventLoop loop;
  kj::WaitScope waitScope(loop);
  result.wait(waitScope);

  KJ_EXPECT(!next_called, "Expected middleware to NOT call next() without auth");
  KJ_EXPECT(storage->response.sent_status == 401, "Expected 401 response");
}

KJ_TEST("AuthMiddleware: allows protected paths with valid JWT") {
  AuthTestFixture fixture;

  kj::String token = fixture.jwt_manager_->create_access_token("testuser"_kj);

  auto middleware = kj::heap<AuthMiddleware>(
      kj::heap<AuthManager>(kj::heap<JwtManager>("test-secret-key-for-jwt-testing-32b!"_kj),
                            kj::heap<ApiKeyManager>()),
      nullptr, AuthMiddleware::default_config());

  auto storage = fixture.createTestStorage();
  storage->path = kj::str("/api/orders");
  storage->headers->addPtr("Authorization"_kj, kj::str("Bearer ", token));
  RequestContext ctx = fixture.makeRequestContext(*storage);

  bool next_called = false;
  auto next = [&next_called]() -> kj::Promise<void> {
    next_called = true;
    return kj::READY_NOW;
  };

  auto result = middleware->process(ctx, kj::mv(next));
  kj::EventLoop loop;
  kj::WaitScope waitScope(loop);
  result.wait(waitScope);

  KJ_EXPECT(next_called, "Expected middleware to call next() with valid auth");
  KJ_IF_SOME(auth_info, ctx.authInfo) {
    KJ_EXPECT(auth_info.user_id == "testuser");
    KJ_EXPECT(auth_info.auth_method == "jwt");
  }
  else {
    KJ_FAIL_ASSERT("Expected authInfo to be populated");
  }
}

KJ_TEST("AuthMiddleware: allows protected paths with valid API key") {
  AuthTestFixture fixture;

  // Create shared ApiKeyManager
  auto shared_api_key_mgr = kj::heap<ApiKeyManager>();
  auto& api_key_ref = *shared_api_key_mgr; // Get reference before moving

  // Create key before moving ownership
  auto key_pair = api_key_ref.create_key("testuser"_kj, "Test Key"_kj, [&]() {
    kj::Vector<kj::String> permissions;
    permissions.add(kj::str("trade:read"));
    return permissions;
  }());

  auto middleware = kj::heap<AuthMiddleware>(
      kj::heap<AuthManager>(kj::heap<JwtManager>("test-secret-key-for-jwt-testing-32b!"_kj),
                            kj::mv(shared_api_key_mgr)),
      nullptr, AuthMiddleware::default_config());

  auto storage = fixture.createTestStorage();
  storage->path = kj::str("/api/orders");
  storage->headers->addPtrPtr("X-API-Key"_kj, key_pair.raw_key);
  RequestContext ctx = fixture.makeRequestContext(*storage);

  bool next_called = false;
  auto next = [&next_called]() -> kj::Promise<void> {
    next_called = true;
    return kj::READY_NOW;
  };

  auto result = middleware->process(ctx, kj::mv(next));
  kj::EventLoop loop;
  kj::WaitScope waitScope(loop);
  result.wait(waitScope);

  KJ_EXPECT(next_called, "Expected middleware to call next() with valid API key");
  KJ_IF_SOME(auth_info, ctx.authInfo) {
    KJ_EXPECT(auth_info.user_id == "testuser");
    KJ_EXPECT(auth_info.auth_method == "api_key");
    KJ_EXPECT(auth_info.permissions.size() == 1);
  }
  else {
    KJ_FAIL_ASSERT("Expected authInfo to be populated");
  }
}

// ============================================================================
// require_permission Tests
// ============================================================================

KJ_TEST("require_permission: allows access with correct permission") {
  AuthTestFixture fixture;

  // Create API key with required permission using auth_manager_'s api_key_manager
  auto key_pair = fixture.auth_api_key_manager().create_key("user"_kj, "Test Key"_kj, [&]() {
    kj::Vector<kj::String> permissions;
    permissions.add(kj::str("trade:write"));
    return permissions;
  }());

  auto storage = fixture.createTestStorage();
  storage->headers->addPtrPtr("X-API-Key"_kj, key_pair.raw_key);
  RequestContext ctx = fixture.makeRequestContext(*storage);

  // Simulate auth middleware populating authInfo
  auto auth_result = fixture.auth_manager_->authenticate(*storage->headers);
  KJ_IF_SOME(auth_info, auth_result) {
    ctx.authInfo = kj::mv(auth_info);
  }

  bool handler_called = false;
  auto handler = [&handler_called](RequestContext&) -> kj::Promise<void> {
    handler_called = true;
    return kj::READY_NOW;
  };

  auto protected_handler = require_permission("trade:write"_kj, kj::mv(handler));
  auto result = protected_handler(ctx);
  kj::EventLoop loop;
  kj::WaitScope waitScope(loop);
  result.wait(waitScope);

  KJ_EXPECT(handler_called, "Expected handler to be called with correct permission");
}

KJ_TEST("require_permission: denies access without permission") {
  AuthTestFixture fixture;

  // Create API key WITHOUT required permission using auth_manager_'s api_key_manager
  auto key_pair = fixture.auth_api_key_manager().create_key("user"_kj, "Test Key"_kj, [&]() {
    kj::Vector<kj::String> permissions;
    permissions.add(kj::str("trade:read"));
    return permissions;
  }());

  auto storage = fixture.createTestStorage();
  storage->headers->addPtrPtr("X-API-Key"_kj, key_pair.raw_key);
  RequestContext ctx = fixture.makeRequestContext(*storage);

  // Simulate auth middleware populating authInfo
  auto auth_result = fixture.auth_manager_->authenticate(*storage->headers);
  KJ_IF_SOME(auth_info, auth_result) {
    ctx.authInfo = kj::mv(auth_info);
  }

  bool handler_called = false;
  auto handler = [&handler_called](RequestContext&) -> kj::Promise<void> {
    handler_called = true;
    return kj::READY_NOW;
  };

  auto protected_handler = require_permission("trade:write"_kj, kj::mv(handler));
  auto result = protected_handler(ctx);
  kj::EventLoop loop;
  kj::WaitScope waitScope(loop);
  result.wait(waitScope);

  KJ_EXPECT(!handler_called, "Expected handler NOT to be called without permission");
  KJ_EXPECT(storage->response.sent_status == 403, "Expected 403 Forbidden response");
}

KJ_TEST("require_permission: denies access without authentication") {
  AuthTestFixture fixture;

  auto storage = fixture.createTestStorage();
  RequestContext ctx = fixture.makeRequestContext(*storage);
  // No auth info populated

  bool handler_called = false;
  auto handler = [&handler_called](RequestContext&) -> kj::Promise<void> {
    handler_called = true;
    return kj::READY_NOW;
  };

  auto protected_handler = require_permission("trade:write"_kj, kj::mv(handler));
  auto result = protected_handler(ctx);
  kj::EventLoop loop;
  kj::WaitScope waitScope(loop);
  result.wait(waitScope);

  KJ_EXPECT(!handler_called, "Expected handler NOT to be called without auth");
  KJ_EXPECT(storage->response.sent_status == 401, "Expected 401 Unauthorized response");
}

// ============================================================================
// Performance Tests
// ============================================================================

KJ_TEST("AuthManager: JWT verification performance < 50 microseconds") {
  AuthTestFixture fixture;

  kj::String token = fixture.jwt_manager_->create_access_token("testuser"_kj);

  kj::HttpHeaders headers(*fixture.header_table_);
  headers.addPtr("Authorization"_kj, kj::str("Bearer ", token));

  // Warm up
  for (int i = 0; i < 100; ++i) {
    fixture.auth_manager_->authenticate(headers);
  }

  // Measure
  auto start = std::chrono::high_resolution_clock::now();
  constexpr int iterations = 1000;

  for (int i = 0; i < iterations; ++i) {
    fixture.auth_manager_->authenticate(headers);
  }

  auto end = std::chrono::high_resolution_clock::now();
  auto duration_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
  double avg_us = static_cast<double>(duration_ns) / iterations / 1000.0;

  KJ_LOG(INFO, "JWT verification average time", avg_us, "microseconds");
  KJ_EXPECT(avg_us < 50.0, "JWT verification should be < 50 microseconds", avg_us);
}

KJ_TEST("AuthManager: API key validation performance < 50 microseconds") {
  AuthTestFixture fixture;

  auto key_pair = fixture.auth_api_key_manager().create_key("testuser"_kj, "Test Key"_kj, [&]() {
    kj::Vector<kj::String> permissions;
    permissions.add(kj::str("test:perm"));
    return permissions;
  }());

  kj::HttpHeaders headers(*fixture.header_table_);
  headers.addPtrPtr("X-API-Key"_kj, key_pair.raw_key);

  // Warm up
  for (int i = 0; i < 100; ++i) {
    fixture.auth_manager_->authenticate(headers);
  }

  // Measure
  auto start = std::chrono::high_resolution_clock::now();
  constexpr int iterations = 1000;

  for (int i = 0; i < iterations; ++i) {
    fixture.auth_manager_->authenticate(headers);
  }

  auto end = std::chrono::high_resolution_clock::now();
  auto duration_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
  double avg_us = static_cast<double>(duration_ns) / iterations / 1000.0;

  KJ_LOG(INFO, "API key validation average time", avg_us, "microseconds");
  KJ_EXPECT(avg_us < 50.0, "API key validation should be < 50 microseconds", avg_us);
}

// ============================================================================
// Edge Cases
// ============================================================================

KJ_TEST("AuthMiddleware: handles malformed Authorization header") {
  AuthTestFixture fixture;

  auto middleware = kj::heap<AuthMiddleware>(
      kj::heap<AuthManager>(kj::heap<JwtManager>("test-secret-key-for-jwt-testing-32b!"_kj),
                            kj::heap<ApiKeyManager>()),
      nullptr, AuthMiddleware::default_config());

  auto storage = fixture.createTestStorage();
  storage->path = kj::str("/api/orders");
  storage->headers->addPtrPtr("Authorization"_kj, "InvalidFormat"_kj);
  RequestContext ctx = fixture.makeRequestContext(*storage);

  bool next_called = false;
  auto next = [&next_called]() -> kj::Promise<void> {
    next_called = true;
    return kj::READY_NOW;
  };

  auto result = middleware->process(ctx, kj::mv(next));
  kj::EventLoop loop;
  kj::WaitScope waitScope(loop);
  result.wait(waitScope);

  KJ_EXPECT(!next_called, "Expected middleware to reject malformed auth header");
  KJ_EXPECT(storage->response.sent_status == 401, "Expected 401 response");
}

KJ_TEST("AuthMiddleware: handles empty Bearer token") {
  AuthTestFixture fixture;

  auto middleware = kj::heap<AuthMiddleware>(
      kj::heap<AuthManager>(kj::heap<JwtManager>("test-secret-key-for-jwt-testing-32b!"_kj),
                            kj::heap<ApiKeyManager>()),
      nullptr, AuthMiddleware::default_config());

  auto storage = fixture.createTestStorage();
  storage->path = kj::str("/api/orders");
  storage->headers->addPtrPtr("Authorization"_kj, "Bearer "_kj);
  RequestContext ctx = fixture.makeRequestContext(*storage);

  bool next_called = false;
  auto next = [&next_called]() -> kj::Promise<void> {
    next_called = true;
    return kj::READY_NOW;
  };

  auto result = middleware->process(ctx, kj::mv(next));
  kj::EventLoop loop;
  kj::WaitScope waitScope(loop);
  result.wait(waitScope);

  KJ_EXPECT(!next_called, "Expected middleware to reject empty token");
  KJ_EXPECT(storage->response.sent_status == 401, "Expected 401 response");
}

KJ_TEST("AuthManager: revoked API key is rejected") {
  AuthTestFixture fixture;

  // Create and then revoke an API key using auth_manager_'s api_key_manager
  auto key_pair = fixture.auth_api_key_manager().create_key("user"_kj, "Test Key"_kj, [&]() {
    kj::Vector<kj::String> permissions;
    permissions.add(kj::str("test:perm"));
    return permissions;
  }());

  // Revoke the key
  fixture.auth_api_key_manager().revoke(key_pair.key_id);

  // Try to authenticate with revoked key
  kj::HttpHeaders headers(*fixture.header_table_);
  headers.addPtrPtr("X-API-Key"_kj, key_pair.raw_key);

  auto result = fixture.auth_manager_->authenticate(headers);

  KJ_EXPECT(result == kj::none, "Expected authentication to fail with revoked key");
}

} // anonymous namespace
} // namespace veloz::gateway
