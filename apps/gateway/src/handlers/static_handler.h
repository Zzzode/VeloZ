#pragma once

#include <kj/async.h>
#include <kj/compat/http.h>
#include <kj/string.h>
#include <kj/vector.h>

namespace veloz::gateway {

// Forward declaration
class StaticFileServer;

/**
 * Static file handler for serving web UI.
 *
 * Handles all non-API paths:
 * - Serves static files from configured directory
 * - SPA routing: serves index.html for non-file requests
 *
 * This is a thin wrapper around StaticFileServer.
 */
class StaticHandler {
public:
  /**
   * Construct a static handler.
   *
   * @param server Static file server instance
   */
  explicit StaticHandler(StaticFileServer& server);

  /**
   * Handle static file request.
   *
   * @param method HTTP method
   * @param url Request URL path
   * @param headers Request headers
   * @param requestBody Request body (ignored for static files)
   * @param response HTTP response object
   * @return Promise that completes when the response is sent
   */
  kj::Promise<void> handle(kj::HttpMethod method, kj::StringPtr url, const kj::HttpHeaders& headers,
                           kj::AsyncInputStream& requestBody, kj::HttpService::Response& response);

private:
  StaticFileServer& server_;
};

} // namespace veloz::gateway
