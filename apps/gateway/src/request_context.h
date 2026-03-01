#pragma once

#include "auth/auth_manager.h"

#include <kj/common.h>
#include <kj/compat/http.h>
#include <kj/function.h>
#include <kj/map.h>
#include <kj/string.h>
#include <kj/vector.h>

namespace veloz::gateway {

/**
 * Per-request context containing request data, authentication info,
 * and response helpers.
 *
 * This context is populated by the gateway and passed through middleware
 * to handlers.
 */
struct RequestContext {
  // Request data
  kj::HttpMethod method;
  kj::StringPtr path;
  kj::StringPtr queryString;
  const kj::HttpHeaders& headers; // const reference - handlers shouldn't modify
  kj::AsyncInputStream& body;     // reference - lifetime managed by GatewayServer

  // Response object (for sending responses)
  kj::HttpService::Response& response;

  // Header table reference (needed for creating response headers)
  const kj::HttpHeaderTable& headerTable;

  // Extracted path parameters (e.g., {id} from /api/orders/{id})
  kj::HashMap<kj::String, kj::String> path_params;

  // Authentication info (populated by AuthMiddleware)
  // Uses AuthInfo from auth_manager.h
  kj::Maybe<AuthInfo> authInfo;

  // Client info
  kj::String clientIP;

  // Response helpers
  kj::Promise<void> sendJson(int status, kj::StringPtr body);
  kj::Promise<void> sendError(int status, kj::StringPtr error);

  // Utilities
  kj::Promise<kj::String> readBodyAsString();
  kj::Maybe<kj::StringPtr> getHeader(kj::StringPtr name) const;
};

// Inline implementations for RequestContext methods

inline kj::Promise<void> RequestContext::sendJson(int status, kj::StringPtr body) {
  kj::HttpHeaders responseHeaders(headerTable);
  responseHeaders.setPtr(kj::HttpHeaderId::CONTENT_TYPE, "application/json"_kj);

  auto stream = response.send(status, "OK"_kj, responseHeaders, body.size());
  return stream->write(body.asBytes()).attach(kj::mv(stream));
}

inline kj::Promise<void> RequestContext::sendError(int status, kj::StringPtr error) {
  auto body = kj::str("{\"error\":\"", error, "\"}");
  return sendJson(status, body);
}

inline kj::Promise<kj::String> RequestContext::readBodyAsString() {
  return body.readAllText();
}

inline kj::Maybe<kj::StringPtr> RequestContext::getHeader(kj::StringPtr name) const {
  // HttpHeaders doesn't support direct string lookup, we use forEach to search
  kj::Maybe<kj::StringPtr> result = kj::none;
  headers.forEach([&](kj::StringPtr headerName, kj::StringPtr headerValue) {
    if (headerName == name) {
      result = headerValue;
    }
  });
  return result;
}

/**
 * Request timer for metrics collection.
 */
class RequestTimer {
public:
  RequestTimer();
  ~RequestTimer();

  void recordResponse(int status, size_t responseSize);
  double getElapsedMs() const;

private:
  kj::TimePoint startTime_;
};

} // namespace veloz::gateway
