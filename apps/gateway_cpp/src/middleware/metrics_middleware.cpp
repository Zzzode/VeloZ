#include "middleware/metrics_middleware.h"

#include <chrono>
#include <veloz/core/metrics.h>

namespace veloz::gateway {

MetricsMiddleware::MetricsMiddleware(veloz::core::MetricsRegistry& registry) : registry_(registry) {
  // Register HTTP metrics
  if (registry_.counter("http_requests_total") == nullptr) {
    registry_.register_counter("http_requests_total", "Total HTTP requests");
  }
  if (registry_.histogram("http_request_duration_seconds") == nullptr) {
    registry_.register_histogram("http_request_duration_seconds", "Request duration in seconds",
                                 []() {
                                   kj::Vector<double> buckets;
                                   buckets.add(0.001);
                                   buckets.add(0.005);
                                   buckets.add(0.01);
                                   buckets.add(0.05);
                                   buckets.add(0.1);
                                   buckets.add(0.5);
                                   buckets.add(1.0);
                                   buckets.add(5.0);
                                   return buckets;
                                 }());
  }
  if (registry_.counter("http_requests_by_status") == nullptr) {
    registry_.register_counter("http_requests_by_status", "HTTP requests by status code");
  }
  if (registry_.gauge("http_active_connections") == nullptr) {
    registry_.register_gauge("http_active_connections", "Active HTTP connections");
  }

  // Cache metric pointers for performance
  requests_total_ = registry_.counter("http_requests_total");
  request_duration_ = registry_.histogram("http_request_duration_seconds");
  requests_by_status_ = registry_.counter("http_requests_by_status");
  active_connections_ = registry_.gauge("http_active_connections");
}

MetricsMiddleware::~MetricsMiddleware() noexcept = default;

kj::Promise<void> MetricsMiddleware::process(RequestContext& ctx,
                                             kj::Function<kj::Promise<void>()> next) {
  // Increment active connections
  if (active_connections_) {
    active_connections_->increment();
  }

  // Start timing
  auto start_time = std::chrono::steady_clock::now();

  // Call next middleware/handler
  return next()
      .then([this, &ctx, start_time]() -> kj::Promise<void> {
        // Calculate duration
        auto end_time = std::chrono::steady_clock::now();
        auto duration_ns =
            std::chrono::duration_cast<std::chrono::nanoseconds>(end_time - start_time);
        double duration_sec = static_cast<double>(duration_ns.count()) / 1e9;

        // Record metrics
        record_request(ctx.method, ctx.path, 200, duration_sec);

        // Decrement active connections
        if (active_connections_) {
          active_connections_->decrement();
        }

        return kj::READY_NOW;
      })
      .catch_([this, &ctx, start_time](kj::Exception&& e) -> kj::Promise<void> {
        // Calculate duration
        auto end_time = std::chrono::steady_clock::now();
        auto duration_ns =
            std::chrono::duration_cast<std::chrono::nanoseconds>(end_time - start_time);
        double duration_sec = static_cast<double>(duration_ns.count()) / 1e9;

        // Extract status from exception if possible (default to 500)
        unsigned int status = 500;

        // Record metrics with error status
        record_request(ctx.method, ctx.path, status, duration_sec);

        // Decrement active connections
        if (active_connections_) {
          active_connections_->decrement();
        }

        // Re-throw the exception
        return kj::mv(e);
      });
}

void MetricsMiddleware::record_request(kj::HttpMethod method, kj::StringPtr path,
                                       unsigned int status, double duration_sec) {
  // TODO: Use method and path for labeled metrics (future enhancement)
  (void)method;
  (void)path;

  // Increment total requests counter
  if (requests_total_) {
    requests_total_->increment();
  }

  // Record duration in histogram
  if (request_duration_) {
    request_duration_->observe(duration_sec);
  }

  // Increment status-specific counter
  if (requests_by_status_) {
    // Categorize status code
    kj::String status_category = categorize_status(status);
    (void)status_category; // Future: use for labeled metrics
    requests_by_status_->increment();
  }
}

kj::String MetricsMiddleware::categorize_status(unsigned int status) const {
  if (status >= 200 && status < 300) {
    return kj::str("2xx");
  } else if (status >= 300 && status < 400) {
    return kj::str("3xx");
  } else if (status >= 400 && status < 500) {
    return kj::str("4xx");
  } else if (status >= 500 && status < 600) {
    return kj::str("5xx");
  }
  return kj::str("unknown");
}

kj::String MetricsMiddleware::normalize_path(kj::StringPtr path) const {
  // Replace numeric IDs with placeholders
  // e.g., /api/orders/123 -> /api/orders/{id}
  kj::Vector<kj::String> segments;
  kj::StringPtr remaining = path;

  while (remaining.size() > 0) {
    // Find next segment
    KJ_IF_SOME(slash_pos, remaining.findFirst('/')) {
      if (slash_pos > 0) {
        kj::String segment = kj::str(remaining.slice(0, slash_pos));

        // Check if segment is numeric ID
        bool is_numeric = segment.size() > 0;
        for (char c : segment) {
          if (c < '0' || c > '9') {
            is_numeric = false;
            break;
          }
        }

        if (is_numeric) {
          segments.add(kj::str("{id}"));
        } else {
          segments.add(kj::mv(segment));
        }
      }
      segments.add(kj::str("/"));
      remaining = remaining.slice(slash_pos + 1);
    }
    else {
      // No more slashes, process remaining as last segment
      if (remaining.size() > 0) {
        kj::String segment = kj::str(remaining);

        // Check if segment is numeric ID
        bool is_numeric = segment.size() > 0;
        for (char c : segment) {
          if (c < '0' || c > '9') {
            is_numeric = false;
            break;
          }
        }

        if (is_numeric) {
          segments.add(kj::str("{id}"));
        } else {
          segments.add(kj::mv(segment));
        }
      }
      break;
    }
  }

  return kj::strArray(segments, "");
}

} // namespace veloz::gateway
