#pragma once

#include "request_context.h"

#include <kj/async.h>
#include <kj/function.h>
#include <kj/string.h>

namespace veloz::gateway {

/**
 * Base interface for middleware components.
 *
 * Middleware components can intercept and modify HTTP requests before
 * they reach handlers, and can also modify responses.
 *
 * Middleware process() methods receive:
 * - ctx: The request context
 * - next: A callable that invokes the next middleware/handler in the chain
 *
 * Typical middleware pattern:
 * 1. Process request (e.g., auth, rate limiting, logging)
 * 2. Optionally modify the context or return early
 * 3. Call next() to continue the chain (or return early to short-circuit)
 */
class Middleware {
public:
  virtual ~Middleware() noexcept = default;

  /**
   * Process the request through this middleware.
   *
   * @param ctx The request context
   * @param next Function to call to continue to next middleware/handler
   * @return Promise that completes when request processing is done
   *
   * To short-circuit the middleware chain (e.g., on auth failure),
   * return early without calling next().
   */
  virtual kj::Promise<void> process(RequestContext& ctx,
                                    kj::Function<kj::Promise<void>()> next) = 0;
};

} // namespace veloz::gateway
