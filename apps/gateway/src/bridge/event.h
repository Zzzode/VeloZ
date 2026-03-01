#pragma once

#include <cstdint>
#include <kj/common.h>
#include <kj/one-of.h>
#include <kj/string.h>
#include <kj/vector.h>

namespace veloz::gateway::bridge {

/**
 * @brief Event types for SSE streaming
 *
 * These types define the different event categories that can be broadcast
 * to SSE subscribers.
 */
enum class SseEventType : uint8_t {
  Unknown = 0,
  MarketData = 1,  ///< Market data updates (trades, order book changes)
  OrderUpdate = 2, ///< Order state changes (fill, cancel, reject)
  Account = 3,     ///< Account balance and position updates
  System = 4,      ///< System status messages
  Error = 5,       ///< Error notifications
  KeepAlive = 6,   ///< Periodic keep-alive messages
};

/**
 * @brief Convert SseEventType to string
 */
[[nodiscard]] kj::StringPtr to_string(SseEventType type);

/**
 * @brief SSE Event structure
 *
 * Represents a single event that can be broadcast to SSE subscribers.
 * Events contain a unique ID, type, and JSON-formatted data payload.
 */
struct SseEvent {
  uint64_t id{0};                           ///< Unique event ID (monotonically increasing)
  SseEventType type{SseEventType::Unknown}; ///< Event type
  uint64_t timestamp_ns{0};                 ///< Event timestamp (nanoseconds since epoch)
  kj::String data;                          ///< JSON-formatted event data

  // Default constructor
  SseEvent() = default;

  // Constructor with parameters
  SseEvent(uint64_t id, SseEventType type, uint64_t timestamp_ns, kj::String data)
      : id(id), type(type), timestamp_ns(timestamp_ns), data(kj::mv(data)) {}

  // Non-copyable, movable
  SseEvent(const SseEvent&) = delete;
  SseEvent& operator=(const SseEvent&) = delete;
  SseEvent(SseEvent&&) = default;
  SseEvent& operator=(SseEvent&&) = default;

  /**
   * @brief Format event as SSE message line
   *
   * Returns the event formatted as per SSE protocol:
   * id: <id>
   * event: <type>
   * data: <data>
   *
   */
  [[nodiscard]] kj::String format_sse() const;

  /**
   * @brief Format event as SSE message line with retry value
   *
   * Includes the retry field for reconnection delay.
   */
  [[nodiscard]] kj::String format_sse(uint64_t retry_ms) const;

  /**
   * @brief Create a keep-alive event
   */
  [[nodiscard]] static SseEvent create_keepalive(uint64_t id);

  /**
   * @brief Create a market data event
   */
  [[nodiscard]] static SseEvent create_market_data(uint64_t id, kj::String data);

  /**
   * @brief Create an order update event
   */
  [[nodiscard]] static SseEvent create_order_update(uint64_t id, kj::String data);

  /**
   * @brief Create an error event
   */
  [[nodiscard]] static SseEvent create_error(uint64_t id, kj::String data);
};

/**
 * @brief Event history entry
 *
 * Stores events in the history buffer with their sequence numbers
 * for Last-Event-ID replay support.
 */
struct SseEventHistoryEntry {
  uint64_t id;
  SseEvent event;

  SseEventHistoryEntry() = default;
  SseEventHistoryEntry(uint64_t id, SseEvent event) : id(id), event(kj::mv(event)) {}

  // Non-copyable, movable
  SseEventHistoryEntry(const SseEventHistoryEntry&) = delete;
  SseEventHistoryEntry& operator=(const SseEventHistoryEntry&) = delete;
  SseEventHistoryEntry(SseEventHistoryEntry&&) = default;
  SseEventHistoryEntry& operator=(SseEventHistoryEntry&&) = default;
};

} // namespace veloz::gateway::bridge
