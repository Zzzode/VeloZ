#pragma once

#include "router.h"

#include <kj/async-io.h>
#include <kj/compat/http.h>

namespace veloz::gateway {

/**
 * @brief HTTP gateway server that dispatches requests through the Router.
 *
 * GatewayServer implements kj::HttpService and handles incoming HTTP requests
 * by routing them to registered handlers via the Router.
 *
 * Request flow:
 * 1. Parse URL to extract path and query string
 * 2. Create RequestContext with request data
 * 3. Match route via Router::match()
 * 4. Handle 404 (no route) or 405 (wrong method) errors
 * 5. Execute matched handler
 */
class GatewayServer final : public kj::HttpService {
public:
  /**
   * @brief Construct a GatewayServer with a Router reference.
   *
   * @param headerTable The HTTP header table for response headers
   * @param router The router to dispatch requests to (must outlive this server)
   */
  GatewayServer(const kj::HttpHeaderTable& headerTable, Router& router);

  kj::Promise<void> request(kj::HttpMethod method, kj::StringPtr url,
                            const kj::HttpHeaders& headers, kj::AsyncInputStream& requestBody,
                            Response& response) override;

private:
  const kj::HttpHeaderTable& headerTable_;
  Router& router_;

  /**
   * @brief Parse URL to extract path (without query string).
   *
   * @param url The full URL path (may include query string)
   * @return The path portion without query string
   */
  static kj::String extractPath(kj::StringPtr url);

  /**
   * @brief Parse URL to extract query string.
   *
   * @param url The full URL path (may include query string)
   * @return The query string (without leading '?'), or empty string if none
   */
  static kj::StringPtr extractQueryString(kj::StringPtr url);

  /**
   * @brief Build Allow header value from list of methods.
   *
   * @param methods Vector of HTTP method names
   * @return Comma-separated Allow header value
   */
  static kj::String buildAllowHeader(const kj::Vector<kj::String>& methods);
};

} // namespace veloz::gateway
