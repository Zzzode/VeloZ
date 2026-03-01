#include "handlers/metrics_handler.h"

#include "veloz/core/metrics.h"

#include <kj/compat/http.h>
#include <kj/debug.h>

namespace veloz::gateway {

// ============================================================================
// MetricsHandler Implementation
// ============================================================================

MetricsHandler::MetricsHandler(core::MetricsRegistry& registry) : registry_(registry) {}

kj::Promise<void> MetricsHandler::handleMetrics(RequestContext& ctx) {
  try {
    // Export metrics in Prometheus format
    // Performance target: <50μs
    auto prometheus_output = registry_.to_prometheus();

    // Set content type header
    kj::HttpHeaders response_headers(ctx.headerTable);
    response_headers.setPtr(kj::HttpHeaderId::CONTENT_TYPE,
                            "text/plain; version=0.0.4; charset=utf-8"_kj);

    // Send response
    auto stream = ctx.response.send(200, "OK"_kj, response_headers, prometheus_output.size());
    auto writePromise = stream->write(prometheus_output.asBytes());
    return writePromise.attach(kj::mv(stream));
  } catch (const kj::Exception& e) {
    KJ_LOG(ERROR, "Error in metrics handler", e);
    // For metrics endpoint, return plain text error
    kj::String error_body = kj::str("# ERROR: ", e.getDescription());
    kj::HttpHeaders response_headers(ctx.headerTable);
    response_headers.setPtr(kj::HttpHeaderId::CONTENT_TYPE, "text/plain; charset=utf-8"_kj);
    auto stream =
        ctx.response.send(500, "Internal Server Error"_kj, response_headers, error_body.size());
    auto writePromise = stream->write(error_body.asBytes());
    return writePromise.attach(kj::mv(stream));
  }
}

} // namespace veloz::gateway
