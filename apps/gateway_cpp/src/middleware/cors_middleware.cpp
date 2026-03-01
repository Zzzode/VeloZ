#include "cors_middleware.h"

#include <kj/async-io.h>
#include <kj/compat/http.h>
#include <kj/debug.h>
#include <kj/string.h>

namespace veloz::gateway {

CorsMiddleware::CorsMiddleware(Config&& config) : config_(kj::mv(config)) {
  // Ensure reasonable defaults
  if (config_.maxAge <= 0) {
    config_.maxAge = 86400; // Default 24 hours
  }
}

CorsMiddleware::~CorsMiddleware() noexcept = default;

kj::Promise<void> CorsMiddleware::process(RequestContext& ctx,
                                          kj::Function<kj::Promise<void>()> next) noexcept {

  // Get Origin header from request using RequestContext helper
  kj::Maybe<kj::StringPtr> originHeader = ctx.getHeader("Origin");

  // If no Origin header present, this is not a CORS request - skip
  KJ_IF_SOME(origin, originHeader) {
    // Check if origin is allowed
    bool allowed = false;
    KJ_IF_SOME(allowedOrigin, config_.allowedOrigin) {
      if (allowedOrigin == "*"_kj) {
        allowed = true;
      } else if (allowedOrigin.startsWith("*."_kj)) {
        // Check for wildcard subdomain support (*.example.com)
        kj::StringPtr wildcardDomain = allowedOrigin.slice(1); // Remove *
        // Check if origin ends with wildcard domain
        if (origin.endsWith(wildcardDomain)) {
          // Also ensure there's exactly one subdomain before wildcard
          size_t originLen = origin.size();
          size_t domainLen = wildcardDomain.size();
          if (originLen > domainLen + 1) {
            // Count dots before domain
            size_t dotCount = 0;
            for (size_t i = 0; i < originLen - domainLen - 1; ++i) {
              if (origin[i] == '.') {
                dotCount++;
              }
            }
            allowed = (dotCount == 0); // Only single subdomain level
          }
        }
      } else {
        // Exact match required
        allowed = (origin == allowedOrigin);
      }
    }
    else {
      // If origin not allowed, proceed without CORS headers
      KJ_LOG(DBG, "CORS: Origin not in allowed list", origin);
      return next();
    }

    // If origin not allowed, proceed without CORS headers
    if (!allowed) {
      return next();
    }

    // Use the headerTable from RequestContext
    kj::HttpHeaders responseHeaders(ctx.headerTable);

    // Handle preflight OPTIONS request
    if (ctx.method == kj::HttpMethod::OPTIONS) {
      // Determine the allowed origin to send back
      kj::StringPtr allowedOriginStr = "*"_kj;
      KJ_IF_SOME(configAllowed, config_.allowedOrigin) {
        if (configAllowed == "*"_kj) {
          allowedOriginStr = "*"_kj;
        } else {
          allowedOriginStr = origin;
        }
      }

      // Add Access-Control-Allow-Origin header
      responseHeaders.addPtrPtr("Access-Control-Allow-Origin"_kj, allowedOriginStr);

      // Add Access-Control-Allow-Methods header
      if (!config_.allowedMethods.empty()) {
        kj::Vector<kj::StringPtr> methodRefs;
        for (const auto& method : config_.allowedMethods) {
          methodRefs.add(method);
        }
        kj::String methodsStr = kj::strArray(methodRefs, ", ");
        responseHeaders.addPtrPtr("Access-Control-Allow-Methods"_kj, methodsStr);
      }

      // Add Access-Control-Allow-Headers header
      if (!config_.allowedHeaders.empty()) {
        kj::Vector<kj::StringPtr> headerRefs;
        for (const auto& header : config_.allowedHeaders) {
          headerRefs.add(header);
        }
        kj::String headersStr = kj::strArray(headerRefs, ", ");
        responseHeaders.addPtrPtr("Access-Control-Allow-Headers"_kj, headersStr);
      }

      // Add Access-Control-Allow-Credentials header if enabled
      if (config_.allowCredentials) {
        responseHeaders.addPtrPtr("Access-Control-Allow-Credentials"_kj, "true"_kj);
      }

      // Add Access-Control-Max-Age header for preflight caching
      if (config_.maxAge > 0) {
        responseHeaders.addPtr("Access-Control-Max-Age"_kj, kj::str(config_.maxAge));
      }

      // Return 200 OK for preflight request
      auto stream = ctx.response.send(200, "OK"_kj, responseHeaders, kj::none);
      return kj::READY_NOW;
    }

    // For non-OPTIONS requests, CORS headers would need to be added
    // Note: In the current architecture, handlers send their own responses
    // We can't modify headers after they're sent, which is a known limitation

    return next().then([origin = kj::heapString(origin)]() {
      // Log that CORS was processed
      KJ_LOG(DBG, "CORS: Processed request with origin", origin);
    });
  }

  // No Origin header - not a CORS request, proceed normally
  return next();
}

} // namespace veloz::gateway
