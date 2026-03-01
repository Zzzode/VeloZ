#pragma once

#include "auth/auth_manager.h"

#include <kj/async-io.h>
#include <kj/async.h>
#include <kj/compat/http.h>
#include <kj/memory.h>
#include <kj/string.h>

namespace veloz::gateway::test {

/**
 * @brief Test context with async I/O setup
 */
class TestContext {
public:
  TestContext()
      : io_(kj::setupAsyncIo()), headerTableOwn_(kj::heap<kj::HttpHeaderTable>()),
        headerTable_(*headerTableOwn_), waitScope_(io_.waitScope) {}

  kj::AsyncIoContext& io() {
    return io_;
  }
  kj::HttpHeaderTable& headerTable() {
    return headerTable_;
  }
  kj::WaitScope& waitScope() {
    return waitScope_;
  }

private:
  kj::AsyncIoContext io_;
  kj::Own<kj::HttpHeaderTable> headerTableOwn_;
  kj::HttpHeaderTable& headerTable_;
  kj::WaitScope& waitScope_;
};

/**
 * @brief Mock output stream for testing
 *
 * Captures all writes to a buffer for test assertions.
 */
class MockAsyncOutputStream final : public kj::AsyncOutputStream {
public:
  kj::Vector<kj::String> written_chunks;

  kj::Promise<void> write(kj::ArrayPtr<const kj::byte> buffer) override {
    written_chunks.add(
        kj::heapString(reinterpret_cast<const char*>(buffer.begin()), buffer.size()));
    return kj::READY_NOW;
  }

  kj::Promise<void> write(kj::ArrayPtr<const kj::ArrayPtr<const kj::byte>> pieces) override {
    for (auto& piece : pieces) {
      written_chunks.add(
          kj::heapString(reinterpret_cast<const char*>(piece.begin()), piece.size()));
    }
    return kj::READY_NOW;
  }

  kj::Promise<void> whenWriteDisconnected() override {
    return kj::NEVER_DONE;
  }

  kj::Promise<size_t> tryWrite(const void* buffer, size_t size) {
    written_chunks.add(kj::heapString(reinterpret_cast<const char*>(buffer), size));
    return kj::Promise<size_t>(size);
  }

  void shutdownWrite() {}

  /**
   * @brief Get all written data as a single string
   */
  kj::String get_all_output() const {
    kj::String result = kj::str("");
    for (const auto& chunk : written_chunks) {
      result = kj::str(result, chunk);
    }
    return result;
  }

  /**
   * @brief Get total bytes written
   */
  size_t total_bytes() const {
    size_t total = 0;
    for (const auto& chunk : written_chunks) {
      total += chunk.size();
    }
    return total;
  }
};

/**
 * @brief Mock input stream for testing
 */
class MockAsyncInputStream final : public kj::AsyncInputStream {
public:
  ~MockAsyncInputStream() = default;

  kj::Promise<size_t> tryRead(void* buffer, size_t minBytes, size_t maxBytes) override {
    (void)buffer;
    (void)minBytes;
    (void)maxBytes;
    return kj::Promise<size_t>(static_cast<size_t>(0));
  }
};

/**
 * @brief Mock HTTP response for testing
 *
 * Properly inherits from kj::HttpService::Response and tracks
 * response state for assertions.
 */
class MockHttpResponse : public kj::HttpService::Response {
public:
  kj::uint statusCode = 0;
  kj::String statusText;
  kj::HttpHeaders responseHeaders;
  bool errorSent = false;
  kj::String errorStatusText;

  explicit MockHttpResponse(const kj::HttpHeaderTable& headerTable)
      : responseHeaders(headerTable) {}

  /**
   * @brief Send a successful response
   *
   * Note: Not virtual in KJ, but provided as an interface.
   * Returns a mock output stream that captures writes.
   */
  kj::Own<kj::AsyncOutputStream> send(kj::uint status, kj::StringPtr text,
                                      const kj::HttpHeaders& headers,
                                      kj::Maybe<uint64_t> bodySize) override {
    (void)bodySize;
    statusCode = status;
    statusText = kj::str(text);
    responseHeaders = headers.clone();
    return kj::heap<MockAsyncOutputStream>();
  }

  /**
   * @brief Send an error response
   */
  kj::Promise<void> sendError(kj::uint status, kj::StringPtr text,
                              const kj::HttpHeaderTable& table) {
    (void)table;
    statusCode = status;
    statusText = kj::str(text);
    errorStatusText = kj::str(text);
    errorSent = true;
    return kj::READY_NOW;
  }

  kj::Own<kj::WebSocket> acceptWebSocket(const kj::HttpHeaders&) override {
    KJ_FAIL_REQUIRE("WebSocket not expected in tests");
  }
};

/**
 * @brief Mock HTTP request context for testing
 */
struct MockRequestContext {
  MockHttpResponse response;
  kj::HttpMethod method{kj::HttpMethod::GET};
  kj::String path;
  kj::String queryString;
  kj::HttpHeaders headers;
  kj::Own<MockAsyncInputStream> body;
  kj::Maybe<veloz::gateway::AuthInfo> authInfo;
  kj::String clientIP;

  explicit MockRequestContext(const kj::HttpHeaderTable& table)
      : response(table), headers(table), body(kj::heap<MockAsyncInputStream>()) {
    clientIP = kj::str("127.0.0.1");
  }
};

/**
 * @brief Helper to check if a string contains a substring
 */
inline bool responseContains(kj::StringPtr response, kj::StringPtr substr) {
  return response.find(substr) != kj::none;
}

} // namespace veloz::gateway::test
