#pragma once

#include "request_context.h"

#include <kj/async-io.h>
#include <kj/common.h>
#include <kj/compat/http.h>
#include <kj/function.h>
#include <kj/map.h>
#include <kj/string.h>
#include <kj/vector.h>

namespace veloz::gateway {

/**
 * High-performance HTTP request router with pattern matching and parameter extraction.
 *
 * Features:
 * - HTTP method-based routing
 * - Path pattern matching with parameters (e.g., `/api/orders/{id}`)
 * - Efficient route lookup using hash map for method + route key
 * - 404 (Not Found) and 405 (Method Not Allowed) error handling
 * - Performance target: <5us route lookup
 *
 * Usage:
 * ```cpp
 * Router router;
 * router.add_route(kj::HttpMethod::GET, "/api/orders", [](RequestContext& ctx) {
 *   // Handle GET /api/orders
 *   return ctx.response.send(200, "OK"_kj, ctx.headerTable_, kj::Maybe<uint64_t>());
 * });
 *
 * router.add_route(kj::HttpMethod::GET, "/api/orders/{id}", [](RequestContext& ctx) {
 *   KJ_IF_SOME(order_id, ctx.path_params.find("id"_kj)) {
 *     // Handle GET /api/orders/{id} with order_id parameter
 *   }
 *   return ctx.response.send(200, "OK"_kj, ctx.headerTable_, kj::Maybe<uint64_t>());
 * });
 * ```
 */
class Router {
public:
  /**
   * Handler function type for processing HTTP requests.
   * Returns a promise that completes when the response is sent.
   */
  using Handler = kj::Function<kj::Promise<void>(RequestContext&)>;

  /**
   * RouteMatch represents the result of a route lookup.
   * Contains the handler and any extracted path parameters.
   */
  struct RouteMatch {
    mutable Handler handler;
    kj::HashMap<kj::String, kj::String> path_params;
  };

  /**
   * Adds a route to the router.
   *
   * @param method The HTTP method to match (GET, POST, PUT, DELETE, etc.)
   * @param pattern The URL pattern, optionally containing parameters in braces (e.g.,
   * "/api/orders/{id}")
   * @param handler The handler function to call when the route matches
   *
   * @throws kj::Exception if the pattern is invalid
   */
  void add_route(kj::HttpMethod method, kj::StringPtr pattern, Handler handler);

  /**
   * Matches a request against registered routes.
   *
   * @param method The HTTP method of the request
   * @param path The URL path of the request
   * @return A RouteMatch if a matching route is found, or nullptr if not found
   */
  kj::Maybe<RouteMatch> match(kj::HttpMethod method, kj::StringPtr path);

  /**
   * Checks if any route exists for the given path with any HTTP method.
   * Used to determine if 405 (Method Not Allowed) should be returned.
   *
   * @param path The URL path to check
   * @return true if any route exists for this path
   */
  bool has_path(kj::StringPtr path) const;

  /**
   * Gets all HTTP methods registered for a given path.
   * Used for constructing the Allow header in 405 responses.
   *
   * @param path The URL path to check
   * @return A vector of HTTP method names for this path
   */
  kj::Vector<kj::String> get_methods_for_path(kj::StringPtr path) const;

  /**
   * Returns the total number of registered routes.
   */
  size_t route_count() const {
    return routes_.size();
  }

  /**
   * Converts an HTTP method enum to its string representation.
   *
   * @param method The HTTP method
   * @return The method name as a string (e.g., "GET", "POST")
   */
  static kj::String get_method_name(kj::HttpMethod method);

private:
  /**
   * Route represents a single registered route.
   */
  struct Route {
    kj::HttpMethod method;
    kj::String pattern;
    Handler handler;

    // Pre-parsed pattern segments and parameter names
    struct Segment {
      kj::String value; // Either a literal segment or parameter name
      bool is_param;    // True if this segment is a parameter (e.g., {id})
    };
    kj::Vector<Segment> segments;
  };

  // All registered routes
  kj::Vector<Route> routes_;

  /**
   * Parses a pattern into segments, identifying parameter placeholders.
   * Parameters are enclosed in braces: {param_name}
   *
   * @param pattern The pattern to parse
   * @return A vector of segments representing the pattern
   */
  kj::Vector<Route::Segment> parse_pattern(kj::StringPtr pattern);

  /**
   * Matches a pattern against a path, extracting parameters.
   *
   * @param pattern_segments The parsed pattern segments
   * @param path The request path to match
   * @param path_params [out] The extracted path parameters
   * @return true if the path matches the pattern
   */
  bool match_pattern(const kj::Vector<Route::Segment>& pattern_segments, kj::StringPtr path,
                     kj::HashMap<kj::String, kj::String>& path_params) const;

  /**
   * Normalizes a path by ensuring it starts with / and has no trailing slash (except root).
   *
   * @param path The path to normalize
   * @return A normalized path string
   */
  static kj::String normalize_path(kj::StringPtr);
};

} // namespace veloz::gateway
