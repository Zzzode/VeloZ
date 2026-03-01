#pragma once

#include "middleware.h"
#include "request_context.h"

#include <kj/string.h>

namespace veloz::gateway {

/**
 * CORS middleware.
 *
 * Adds CORS headers to responses.
 * Configurable origins, methods, and headers.
 */
class CorsMiddleware : public Middleware {
public:
  struct Config {
    kj::Maybe<kj::String> allowedOrigin; // "*" for all origins
    kj::Vector<kj::String> allowedMethods;
    kj::Vector<kj::String> allowedHeaders;
    bool allowCredentials = false;
    int maxAge = 86400; // Max age in seconds, default 24 hours
  };

  explicit CorsMiddleware(Config&& config);
  ~CorsMiddleware() noexcept override;

  kj::Promise<void> process(RequestContext& ctx,
                            kj::Function<kj::Promise<void>()> next) noexcept override;

private:
  Config config_;
};

} // namespace veloz::gateway
