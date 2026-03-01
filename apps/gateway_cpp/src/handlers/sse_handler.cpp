#include "sse_handler.h"

#include <cstdlib>
#include <kj/async-io.h>
#include <kj/common.h>
#include <kj/debug.h>
#include <kj/string.h>

namespace veloz::gateway::handlers {

namespace {

// SSE-specific header IDs (will be initialized in constructor)
static kj::Maybe<kj::HttpHeaderId> HEADER_LAST_EVENT_ID;
static kj::Maybe<kj::HttpHeaderId> HEADER_CACHE_CONTROL;
static kj::Maybe<kj::HttpHeaderId> HEADER_CONNECTION;

/**
 * @brief Initialize SSE-related HTTP headers
 */
void init_sse_headers(const kj::HttpHeaderTable& headerTable) {
  HEADER_LAST_EVENT_ID = headerTable.stringToId("Last-Event-Id"_kj);
  HEADER_CACHE_CONTROL = headerTable.stringToId("Cache-Control"_kj);
  HEADER_CONNECTION = headerTable.stringToId("Connection"_kj);
}

} // namespace

SseHandler::SseHandler(bridge::EventBroadcaster& broadcaster, const SseHandlerConfig& config)
    : config_(config), broadcaster_(broadcaster), header_table_(kj::heap<kj::HttpHeaderTable>()) {
  init_sse_headers(*header_table_);
}

SseHandler::~SseHandler() = default;

kj::Promise<void> SseHandler::handle(kj::HttpMethod method, kj::StringPtr url,
                                     const kj::HttpHeaders& headers,
                                     kj::AsyncInputStream& /* requestBody */,
                                     kj::HttpService::Response& response) {

  // Only accept GET requests on /api/stream
  if (method != kj::HttpMethod::GET) {
    return response.sendError(405, "Method Not Allowed"_kj, *header_table_);
  }

  if (url != "/api/stream"_kj) {
    return response.sendError(404, "Not Found"_kj, *header_table_);
  }

  // Check connection limit
  if (active_connections_.load(std::memory_order_relaxed) >= config_.max_concurrent_streams) {
    return response.sendError(503, "Service Unavailable"_kj, *header_table_);
  }

  // Parse Last-Event-ID header for replay
  uint64_t last_id = parse_last_event_id(headers);

  // Send SSE headers and get stream
  kj::Own<kj::AsyncOutputStream> stream = send_sse_headers(response, *header_table_);

  // Subscribe to events
  kj::Own<bridge::SseSubscription> subscription = broadcaster_.subscribe(last_id);

  // Increment connection counter
  active_connections_.fetch_add(1, std::memory_order_relaxed);

  // Build the SSE connection promise:
  // Use a deferred cleanup action for the counter
  // The helper functions take references; we attach ownership to the promise chain
  return replay_history(*stream, last_id)
      .then([this, stream = kj::mv(stream), subscription = kj::mv(subscription)]() mutable {
        // Continue with the connection loop
        return connection_loop(*stream, *subscription).attach(kj::mv(stream), kj::mv(subscription));
      })
      .then([this]() {
        // Decrement counter when done
        active_connections_.fetch_sub(1, std::memory_order_relaxed);
      });
}

kj::Promise<void> SseHandler::send_event(kj::AsyncOutputStream& stream, uint64_t id,
                                         kj::StringPtr type, kj::StringPtr data) {
  // Format SSE message: id: <id>\nevent: <type>\ndata: <data>\n\n
  auto message = kj::str("id: ", id, "\n", "event: ", type, "\n", "data: ", data, "\n\n");

  return stream.write(message.asBytes()).attach(kj::mv(message));
}

kj::Promise<void> SseHandler::send_keepalive(kj::AsyncOutputStream& stream) {
  // Send a colon-prefixed comment for keep-alive
  // This is the recommended way to send keep-alives in SSE
  auto message = kj::str(": keepalive\n\n");

  return stream.write(message.asBytes()).attach(kj::mv(message));
}

kj::Own<kj::AsyncOutputStream>
SseHandler::send_sse_headers(kj::HttpService::Response& response,
                             const kj::HttpHeaderTable& header_table) {

  kj::HttpHeaders response_headers(header_table);

  // Set SSE-specific headers using setPtr to avoid deprecation warning
  response_headers.setPtr(kj::HttpHeaderId::CONTENT_TYPE, "text/event-stream; charset=utf-8"_kj);

  KJ_IF_SOME(cache_control, HEADER_CACHE_CONTROL) {
    response_headers.setPtr(cache_control, "no-cache, no-transform"_kj);
  }
  KJ_IF_SOME(connection, HEADER_CONNECTION) {
    response_headers.setPtr(connection, "keep-alive"_kj);
  }

  // Disable Nginx buffering (if applicable)
  response_headers.addPtr("X-Accel-Buffering"_kj, kj::str("no"));

  // Send response with headers
  auto stream = response.send(200, "OK"_kj, response_headers, kj::none);

  return stream;
}

uint64_t SseHandler::parse_last_event_id(const kj::HttpHeaders& headers) const {
  // Look for Last-Event-ID header
  KJ_IF_SOME(last_event_id_header, HEADER_LAST_EVENT_ID) {
    KJ_IF_SOME(value, headers.get(last_event_id_header)) {
      // Parse ID as an integer
      // The ID should be a simple decimal number
      char* end = nullptr;
      uint64_t result = std::strtoull(value.cStr(), &end, 10);
      if (end == value.cStr() + value.size()) {
        return result;
      }
    }
  }
  return 0;
}

kj::Promise<void> SseHandler::replay_history(kj::AsyncOutputStream& stream, uint64_t last_id) {
  if (last_id == 0) {
    // No history to replay
    return kj::READY_NOW;
  }

  // Get historical events from broadcaster
  auto history = broadcaster_.get_history(last_id);

  if (history.empty()) {
    // No events to replay
    return kj::READY_NOW;
  }

  // Build a chain of promises to send each event sequentially
  kj::Promise<void> result = kj::READY_NOW;

  for (auto& event : history) {
    // Must capture event data by value since event reference becomes invalid
    uint64_t event_id = event.id;
    kj::String type_str = kj::heapString(bridge::to_string(event.type));
    kj::String event_data = kj::str(event.data);

    result = result.then([this, &stream, event_id, type_str = kj::mv(type_str),
                          event_data = kj::mv(event_data)]() mutable {
      return send_event(stream, event_id, type_str, event_data);
    });
  }

  return result;
}

kj::Promise<void> SseHandler::connection_loop(kj::AsyncOutputStream& stream,
                                              bridge::SseSubscription& subscription) {
  // Main loop that:
  // 1. Waits for next event from subscription
  // 2. Sends event to client
  // 3. Continues until subscription closes

  return subscription.next_event().then(
      [this, &stream, &subscription](kj::Maybe<bridge::SseEvent> maybe_event) mutable {
        KJ_IF_SOME(event, maybe_event) {
          // Send event and continue loop
          kj::String type_str = kj::heapString(bridge::to_string(event.type));
          kj::String event_data = kj::str(event.data);
          uint64_t event_id = event.id;

          return send_event(stream, event_id, type_str, event_data)
              .then([this, &stream, &subscription]() mutable {
                // Continue loop
                return connection_loop(stream, subscription);
              });
        }
        else {
          // Subscription closed
          return kj::Promise<void>(kj::READY_NOW);
        }
      });
}

size_t SseHandler::active_connections() const {
  return active_connections_.load(std::memory_order_relaxed);
}

} // namespace veloz::gateway::handlers
