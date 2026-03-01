#pragma once

#include "event.h"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <kj/async-io.h>
#include <kj/common.h>
#include <kj/function.h>
#include <kj/memory.h>
#include <kj/mutex.h>
#include <kj/string.h>
#include <kj/vector.h>

namespace veloz::gateway::bridge {

/**
 * @brief Configuration for EventBroadcaster
 */
struct EventBroadcasterConfig {
  size_t history_size{500};              ///< Number of events to keep in history
  uint64_t keepalive_interval_ms{10000}; ///< Keep-alive interval in milliseconds
  uint64_t max_subscriptions{10000};     ///< Maximum number of concurrent subscriptions
};

// Forward declaration
class EventBroadcaster;

/**
 * @brief Subscription to SSE event stream
 *
 * Represents a client's subscription to the SSE event stream.
 * Provides a promise-based API for receiving events.
 */
class SseSubscription : public kj::Refcounted {
public:
  explicit SseSubscription(uint64_t start_id);
  ~SseSubscription() noexcept;

  /**
   * @brief Get the next event from the subscription
   *
   * Returns a promise that resolves to the next event or kj::none if
   * the subscription is closed.
   */
  kj::Promise<kj::Maybe<SseEvent>> next_event();

  /**
   * @brief Get the last event ID seen by this subscription
   */
  [[nodiscard]] uint64_t last_id() const;

  /**
   * @brief Close the subscription
   */
  void close();

  /**
   * @brief Check if the subscription is closed
   */
  [[nodiscard]] bool is_closed() const;

private:
  friend class EventBroadcaster;

  // Non-copyable
  SseSubscription(const SseSubscription&) = delete;
  SseSubscription& operator=(const SseSubscription&) = delete;

  void fulfill_event(SseEvent event);
  void fulfill_none(); // Signal end of stream

  struct SubscriptionState {
    kj::Maybe<kj::Own<kj::CrossThreadPromiseFulfiller<kj::Maybe<SseEvent>>>> fulfiller;
    kj::Vector<SseEvent> pending;
    size_t pending_head{0};
  };

  // Last event ID seen (atomic for lock-free reads)
  std::atomic<uint64_t> last_id_{0};
  std::atomic<bool> closed_{false};

  kj::MutexGuarded<SubscriptionState> state_;

  // Reference back to broadcaster for removal
  EventBroadcaster* broadcaster_{nullptr};
};

/**
 * @brief Broadcaster for SSE events
 *
 * Manages event broadcasting to SSE subscribers. Features:
 * - Subscription management with last_id tracking
 * - Lock-free event broadcasting using LockFreeQueue
 * - Event history buffer for replay
 * - Batch event delivery for efficiency
 * - Keep-alive support
 */
class EventBroadcaster {
public:
  explicit EventBroadcaster(const EventBroadcasterConfig& config = EventBroadcasterConfig{});

  ~EventBroadcaster();

  // Non-copyable, non-movable
  EventBroadcaster(const EventBroadcaster&) = delete;
  EventBroadcaster& operator=(const EventBroadcaster&) = delete;
  EventBroadcaster(EventBroadcaster&&) = delete;
  EventBroadcaster& operator=(EventBroadcaster&&) = delete;

  /**
   * @brief Subscribe to events, starting from a specific ID
   *
   * @param last_id The last event ID the client has seen (0 for new subscriptions)
   * @return Reference-counted pointer to the subscription
   */
  kj::Own<SseSubscription> subscribe(uint64_t last_id = 0);

  /**
   * @brief Broadcast a single event to all subscribers
   *
   * Thread-safe. Can be called from any thread.
   */
  void broadcast(SseEvent event);

  /**
   * @brief Broadcast multiple events in batch
   *
   * More efficient than individual broadcasts for high-volume scenarios.
   * Thread-safe. Can be called from any thread.
   */
  void broadcast_batch(kj::Vector<SseEvent> events);

  /**
   * @brief Get the current event ID
   *
   * Returns the ID of the most recently broadcast event.
   */
  [[nodiscard]] uint64_t current_id() const;

  /**
   * @brief Get event history entries from a starting ID
   *
   * Used for replay support. Returns events with IDs > last_id.
   *
   * @param last_id The starting ID (exclusive)
   * @return Vector of events to replay
   */
  [[nodiscard]] kj::Vector<SseEvent> get_history(uint64_t last_id) const;

  /**
   * @brief Get the number of active subscriptions
   */
  [[nodiscard]] size_t subscription_count() const;

  /**
   * @brief Get statistics about the broadcaster
   */
  struct Stats {
    uint64_t events_broadcast{0};
    uint64_t events_in_history{0};
    size_t active_subscriptions{0};
    uint64_t queue_size{0};
    uint64_t total_subscriptions{0};
  };
  [[nodiscard]] Stats get_stats() const;

  /**
   * @brief Remove a subscription (called by subscription destructor)
   */
  void remove_subscription(SseSubscription* sub);

private:
  // Event history entry
  struct HistoryEntry {
    uint64_t id;
    SseEvent event;

    HistoryEntry() = default;
    HistoryEntry(uint64_t id, SseEvent event) : id(id), event(kj::mv(event)) {}

    HistoryEntry(const HistoryEntry&) = delete;
    HistoryEntry& operator=(const HistoryEntry&) = delete;
    HistoryEntry(HistoryEntry&&) = default;
    HistoryEntry& operator=(HistoryEntry&&) = default;
  };

  // Deliver event to all subscriptions
  void deliver_event(const SseEvent& event);
  void deliver_batch(kj::ArrayPtr<const SseEvent> events);

  // Add event to history
  void add_to_history(SseEvent event);

  // Configuration
  const EventBroadcasterConfig config_;

  // Event ID counter
  std::atomic<uint64_t> next_id_{1};

  // Event history (circular buffer)
  mutable kj::MutexGuarded<kj::Vector<HistoryEntry>> history_;

  // Subscriptions (raw pointers, owned by callers)
  mutable kj::MutexGuarded<kj::Vector<SseSubscription*>> subscriptions_;

  // Statistics
  struct BroadcasterStats {
    std::atomic<uint64_t> events_broadcast{0};
    std::atomic<uint64_t> total_subscriptions{0};
  };
  BroadcasterStats stats_;
};

} // namespace veloz::gateway::bridge
