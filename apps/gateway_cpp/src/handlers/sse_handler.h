#pragma once

#include "bridge/event_broadcaster.h"

#include <atomic>
#include <cstdint>
#include <kj/async-io.h>
#include <kj/common.h>
#include <kj/compat/http.h>
#include <kj/string.h>

namespace veloz::gateway::handlers {

/**
 * @brief Configuration for SSE handler
 */
struct SseHandlerConfig {
  uint64_t keepalive_interval_ms{10000}; ///< Keep-alive interval (default: 10s)
  uint64_t retry_ms{3000};               ///< Retry interval for reconnection (default: 3s)
  size_t max_concurrent_streams{1000};   ///< Maximum concurrent SSE connections
};

/**
 * @brief SSE (Server-Sent Events) Handler
 *
 * Handles SSE connections for real-time event streaming to clients.
 * Features:
 * - Full SSE protocol support (id, event, data, retry fields)
 * - Keep-alive messages every 10 seconds
 * - Last-Event-ID header support for replay
 * - Connection cleanup on disconnect
 * - Support for 1000+ concurrent connections
 */
class SseHandler {
public:
  explicit SseHandler(bridge::EventBroadcaster& broadcaster,
                      const SseHandlerConfig& config = SseHandlerConfig{});

  ~SseHandler();

  // Non-copyable, non-movable
  SseHandler(const SseHandler&) = delete;
  SseHandler& operator=(const SseHandler&) = delete;
  SseHandler(SseHandler&&) = delete;
  SseHandler& operator=(SseHandler&&) = delete;

  /**
   * @brief Handle an SSE connection request
   *
   * @param ctx HTTP request context
   * @return Promise that resolves when the connection is closed
   */
  kj::Promise<void> handle(kj::HttpMethod method, kj::StringPtr url, const kj::HttpHeaders& headers,
                           kj::AsyncInputStream& requestBody, kj::HttpService::Response& response);

  /**
   * @brief Get the number of active SSE connections
   */
  [[nodiscard]] size_t active_connections() const;

  /**
   * @brief Get the handler configuration
   */
  [[nodiscard]] const SseHandlerConfig& config() const {
    return config_;
  }

private:
  /**
   * @brief Send SSE event to the stream
   *
   * @param stream Output stream to write to
   * @param id Event ID
   * @param type Event type
   * @param data Event data (JSON)
   */
  kj::Promise<void> send_event(kj::AsyncOutputStream& stream, uint64_t id, kj::StringPtr type,
                               kj::StringPtr data);

  /**
   * @brief Send keep-alive comment to the stream
   *
   * Send an empty comment or a comment with a colon to maintain the connection.
   */
  kj::Promise<void> send_keepalive(kj::AsyncOutputStream& stream);

  /**
   * @brief Send SSE headers
   *
   * Sends the standard SSE response headers:
   * - Content-Type: text/event-stream
   * - Cache-Control: no-cache
   * - Connection: keep-alive
   */
  kj::Own<kj::AsyncOutputStream> send_sse_headers(kj::HttpService::Response& response,
                                                  const kj::HttpHeaderTable& header_table);

  /**
   * @brief Parse Last-Event-ID header
   *
   * @param headers HTTP headers
   * @return Last event ID, or 0 if not present
   */
  [[nodiscard]] uint64_t parse_last_event_id(const kj::HttpHeaders& headers) const;

  /**
   * @brief Send historical events to a new connection
   *
   * Used for replay when a client provides Last-Event-ID.
   */
  kj::Promise<void> replay_history(kj::AsyncOutputStream& stream, uint64_t last_id);

  /**
   * @brief Main SSE connection loop
   *
   * Subscribes to events and sends them to the client.
   * Also sends periodic keep-alive messages.
   */
  kj::Promise<void> connection_loop(kj::AsyncOutputStream& stream,
                                    bridge::SseSubscription& subscription);

  // Configuration
  const SseHandlerConfig config_;

  // Event broadcaster reference
  bridge::EventBroadcaster& broadcaster_;

  // Connection counter
  std::atomic<size_t> active_connections_{0};

  // Header table reference (initialized in constructor)
  kj::Own<const kj::HttpHeaderTable> header_table_;
};

} // namespace veloz::gateway::handlers
