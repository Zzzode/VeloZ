/**
 * @file main.cpp
 * @brief Main entry point for gateway integration tests
 *
 * Integration tests validate end-to-end functionality of the gateway,
 * testing the complete request flow from authentication through
 * order submission to SSE notifications.
 *
 * Test categories:
 * 1. Full Flow Tests (test_full_flow.cpp)
 *    - Authentication flow
 *    - Order submission and lifecycle
 *    - SSE event delivery
 *    - Error scenarios
 *
 * 2. Rate Limiting Tests (test_rate_limiting_integration.cpp)
 *    - Token bucket algorithm
 *    - Rate limit enforcement
 *    - Recovery after rate limit
 *
 * 3. SSE Streaming Tests (test_sse_integration.cpp)
 *    - Event delivery
 *    - Reconnection handling
 *    - Keep-alive mechanism
 *
 * 4. Concurrent Access Tests (test_concurrent_access.cpp)
 *    - Thread safety
 *    - Race condition detection
 *    - Performance under load
 */

#include <cstdlib>
#include <iostream>
#include <kj/debug.h>
#include <kj/test.h>

namespace {

void print_banner() {
  std::cout << "\n";
  std::cout << "========================================\n";
  std::cout << "  VeloZ Gateway Integration Tests\n";
  std::cout << "========================================\n";
  std::cout << "\n";
  std::cout << "Test Categories:\n";
  std::cout << "  - Full request flow (auth → order → SSE)\n";
  std::cout << "  - Rate limiting enforcement\n";
  std::cout << "  - SSE streaming and reconnection\n";
  std::cout << "  - Concurrent access and thread safety\n";
  std::cout << "\n";
  std::cout << "Performance Targets:\n";
  std::cout << "  - Full flow test: <500ms\n";
  std::cout << "  - Rate limit check: <1μs\n";
  std::cout << "  - SSE event delivery: <10ms\n";
  std::cout << "  - 100 concurrent requests: <5s\n";
  std::cout << "\n";
}

void setup_test_environment() {
  // Set default test configuration
  setenv("VELOZ_JWT_SECRET", "test_secret_key_for_integration_tests_32_chars!", 1);
  setenv("VELOZ_ADMIN_PASSWORD", "integration_test_admin_password", 1);
  setenv("VELOZ_GATEWAY_HOST", "127.0.0.1", 1);
  setenv("VELOZ_GATEWAY_PORT", "18080", 1); // Use non-standard port for testing
  setenv("VELOZ_RATE_LIMIT_CAPACITY", "1000", 1);
  setenv("VELOZ_RATE_LIMIT_REFILL", "100.0", 1);
  setenv("VELOZ_AUTH_ENABLED", "true", 1);
}

void cleanup_test_environment() {
  // Clean up environment variables
  unsetenv("VELOZ_JWT_SECRET");
  unsetenv("VELOZ_ADMIN_PASSWORD");
  unsetenv("VELOZ_GATEWAY_HOST");
  unsetenv("VELOZ_GATEWAY_PORT");
  unsetenv("VELOZ_RATE_LIMIT_CAPACITY");
  unsetenv("VELOZ_RATE_LIMIT_REFILL");
  unsetenv("VELOZ_AUTH_ENABLED");
}

} // namespace

int main(int argc, char** argv) {
  print_banner();
  setup_test_environment();

  KJ_LOG(INFO, "Starting integration tests...");

  // Run all tests
  int result = kj::runTests(argc, argv);

  cleanup_test_environment();

  if (result == 0) {
    std::cout << "\n";
    std::cout << "========================================\n";
    std::cout << "  All integration tests passed!\n";
    std::cout << "========================================\n";
    std::cout << "\n";
  } else {
    std::cout << "\n";
    std::cout << "========================================\n";
    std::cout << "  Some tests failed!\n";
    std::cout << "========================================\n";
    std::cout << "\n";
  }

  return result;
}

// Define test function for linking
void register_integration_tests() {
  // This is a placeholder for test registration
  // KJ_TEST framework automatically registers tests
}
