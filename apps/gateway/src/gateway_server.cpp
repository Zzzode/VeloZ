#include "gateway_server.h"

#include <kj/debug.h>
#include <kj/memory.h>
#include <kj/string.h>
#include <kj/vector.h>

namespace veloz::gateway {

GatewayServer::GatewayServer(const kj::HttpHeaderTable& headerTable, Router& router)
    : headerTable_(headerTable), router_(router) {}

kj::Promise<void> GatewayServer::request(kj::HttpMethod method, kj::StringPtr url,
                                         const kj::HttpHeaders& headers,
                                         kj::AsyncInputStream& requestBody, Response& response) {
  // Extract path and query string from URL
  kj::String path = extractPath(url);
  kj::StringPtr queryString = extractQueryString(url);

  // Log incoming request (debug level)
  KJ_LOG(DBG, "Incoming request", "method", Router::get_method_name(method), "path", path, "query",
         queryString);

  // Try to match a route for the exact method
  KJ_IF_SOME(match, router_.match(method, path)) {
    // Route found - create context and invoke handler
    RequestContext ctx{
        .method = method,
        .path = path,
        .queryString = queryString,
        .headers = headers,
        .body = requestBody,
        .response = response,
        .headerTable = headerTable_,
        .path_params = kj::mv(match.path_params),
        .authInfo = kj::none,
        .clientIP = kj::str() // Will be populated by middleware
    };

    // Execute the handler
    return match.handler(ctx);
  }

  // No matching route for this method - check if path exists with different method
  if (router_.has_path(path)) {
    // Handle OPTIONS specially - return 200 with Allow header
    if (method == kj::HttpMethod::OPTIONS) {
      kj::Vector<kj::String> allowedMethods = router_.get_methods_for_path(path);
      // Add OPTIONS itself to the Allow header
      allowedMethods.add(kj::str("OPTIONS"));

      kj::String allowHeader = buildAllowHeader(allowedMethods);

      kj::HttpHeaders responseHeaders(headerTable_);
      responseHeaders.addPtrPtr("Allow"_kj, allowHeader);
      responseHeaders.takeOwnership(kj::mv(allowHeader));
      responseHeaders.setPtr(kj::HttpHeaderId::CONTENT_TYPE, "application/json"_kj);

      kj::String body = kj::str("{}");

      auto stream = response.send(200, "OK"_kj, responseHeaders, body.size());
      auto writePromise = stream->write(body.asBytes());
      return writePromise.attach(kj::mv(stream), kj::mv(body));
    }

    // Path exists but method not allowed - return 405
    kj::Vector<kj::String> allowedMethods = router_.get_methods_for_path(path);
    kj::String allowHeader = buildAllowHeader(allowedMethods);

    kj::HttpHeaders responseHeaders(headerTable_);
    responseHeaders.addPtrPtr("Allow"_kj, allowHeader);
    responseHeaders.takeOwnership(kj::mv(allowHeader));
    responseHeaders.setPtr(kj::HttpHeaderId::CONTENT_TYPE, "application/json"_kj);

    kj::String body = kj::str("{\"error\":\"Method Not Allowed\",\"path\":\"", path, "\"}");

    auto stream = response.send(405, "Method Not Allowed"_kj, responseHeaders, body.size());
    auto writePromise = stream->write(body.asBytes());
    return writePromise.attach(kj::mv(stream), kj::mv(body));
  }

  // No route found - return 404
  kj::HttpHeaders responseHeaders(headerTable_);
  responseHeaders.setPtr(kj::HttpHeaderId::CONTENT_TYPE, "application/json"_kj);

  kj::String body = kj::str("{\"error\":\"Not Found\",\"path\":\"", path, "\"}");

  auto stream = response.send(404, "Not Found"_kj, responseHeaders, body.size());
  auto writePromise = stream->write(body.asBytes());
  return writePromise.attach(kj::mv(stream), kj::mv(body));
}

kj::String GatewayServer::extractPath(kj::StringPtr url) {
  // Find the query string start
  KJ_IF_SOME(queryStart, url.findFirst('?')) {
    // Return path portion before query string
    return kj::str(url.slice(0, queryStart));
  }

  // No query string - return entire URL as path
  return kj::str(url);
}

kj::StringPtr GatewayServer::extractQueryString(kj::StringPtr url) {
  // Find the query string start
  KJ_IF_SOME(queryStart, url.findFirst('?')) {
    // Return query string without the leading '?'
    return url.slice(queryStart + 1);
  }

  // No query string - return empty string
  return ""_kj;
}

kj::String GatewayServer::buildAllowHeader(const kj::Vector<kj::String>& methods) {
  if (methods.size() == 0) {
    return kj::str();
  }

  // Build comma-separated list of methods
  kj::Vector<kj::StringPtr> parts;
  for (const auto& method : methods) {
    parts.add(method);
  }

  return kj::strArray(parts, ", ");
}

} // namespace veloz::gateway