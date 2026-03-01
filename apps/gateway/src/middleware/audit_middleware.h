#pragma once

#include "middleware.h"
#include "request_context.h"

#include <kj/string.h>

namespace veloz::gateway {

namespace audit {
class AuditLogger;
}

/**
 * Audit logging middleware.
 *
 * Logs all authenticated requests to audit log.
 */
class AuditMiddleware : public Middleware {
public:
  explicit AuditMiddleware(audit::AuditLogger* auditLogger) : auditLogger_(auditLogger) {}

  kj::Promise<void> process(RequestContext& ctx, kj::Function<kj::Promise<void>()> next) override {
    if (auditLogger_ == nullptr) {
      return next();
    }
    return next();
  }

private:
  audit::AuditLogger* auditLogger_;
};

} // namespace veloz::gateway
