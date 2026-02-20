#pragma once

#include "veloz/common/types.h"
#include "veloz/market/market_event.h"

#include <atomic>
#include <kj/common.h>
#include <kj/function.h>
#include <kj/hash.h>
#include <kj/map.h>
#include <kj/mutex.h>
#include <kj/string.h>
#include <kj/vector.h>

namespace veloz::market {

/**
 * @brief Subscription state for tracking lifecycle
 */
enum class SubscriptionState : std::uint8_t {
  Pending = 0,     // Subscription requested but not confirmed
  Active = 1,      // Subscription confirmed and receiving data
  Paused = 2,      // Temporarily paused (e.g., during reconnection)
  Error = 3,       // Subscription failed
  Unsubscribed = 4 // Unsubscription requested
};

/**
 * @brief Subscription entry with state and metadata
 */
struct SubscriptionEntry {
  veloz::common::SymbolId symbol;
  MarketEventType event_type{MarketEventType::Unknown};
  SubscriptionState state{SubscriptionState::Pending};
  std::int64_t created_at_ns{0};
  std::int64_t last_update_ns{0};
  std::int64_t message_count{0};
  kj::String error_message;
};

/**
 * @brief Rate limiter configuration
 */
struct RateLimitConfig {
  int max_subscriptions_per_second{10};
  int max_total_subscriptions{1000};
  int max_subscriptions_per_symbol{10};
};

/**
 * @brief Callback for subscription state changes
 */
using SubscriptionStateCallback =
    kj::Function<void(const veloz::common::SymbolId& symbol, MarketEventType event_type,
                      SubscriptionState old_state, SubscriptionState new_state)>;

/**
 * @brief Callback for connection lifecycle events
 */
using ConnectionLifecycleCallback = kj::Function<void(bool connected)>;

/**
 * @brief Enhanced subscription manager with state tracking, rate limiting, and lifecycle management
 */
class SubscriptionManager final {
public:
  SubscriptionManager();
  explicit SubscriptionManager(RateLimitConfig rate_limit_config);

  KJ_DISALLOW_COPY_AND_MOVE(SubscriptionManager);

  // Configuration
  void set_rate_limit_config(RateLimitConfig config);
  void set_state_callback(SubscriptionStateCallback callback);
  void set_connection_callback(ConnectionLifecycleCallback callback);

  // Subscribe a client to a symbol/event type
  // Returns true if subscription was accepted (may still be pending)
  bool subscribe(const veloz::common::SymbolId& symbol, MarketEventType event_type,
                 kj::StringPtr subscriber_id);

  // Unsubscribe a client from a symbol/event type
  void unsubscribe(const veloz::common::SymbolId& symbol, MarketEventType event_type,
                   kj::StringPtr subscriber_id);

  // Confirm subscription is active (called when exchange confirms)
  void confirm_subscription(const veloz::common::SymbolId& symbol, MarketEventType event_type);

  // Mark subscription as errored
  void mark_error(const veloz::common::SymbolId& symbol, MarketEventType event_type,
                  kj::StringPtr error_message);

  // Connection lifecycle
  void on_connected();
  void on_disconnected();
  bool is_connected() const;

  // Pause/resume all subscriptions (e.g., during reconnection)
  void pause_all();
  void resume_all();

  // Query methods
  [[nodiscard]] size_t subscriber_count(const veloz::common::SymbolId& symbol,
                                        MarketEventType event_type) const;

  [[nodiscard]] bool is_subscribed(const veloz::common::SymbolId& symbol,
                                   MarketEventType event_type, kj::StringPtr subscriber_id) const;

  [[nodiscard]] SubscriptionState get_state(const veloz::common::SymbolId& symbol,
                                            MarketEventType event_type) const;

  // Get all unique symbols with any subscriptions
  [[nodiscard]] kj::Vector<veloz::common::SymbolId> active_symbols() const;

  // Get all event types subscribed for a symbol
  [[nodiscard]] kj::Vector<MarketEventType>
  event_types(const veloz::common::SymbolId& symbol) const;

  // Get all subscribers for a symbol/event type
  [[nodiscard]] kj::Vector<kj::String> subscribers(const veloz::common::SymbolId& symbol,
                                                   MarketEventType event_type) const;

  // Get subscription entry (for debugging/monitoring)
  [[nodiscard]] kj::Maybe<SubscriptionEntry> get_entry(const veloz::common::SymbolId& symbol,
                                                       MarketEventType event_type) const;

  // Statistics
  [[nodiscard]] size_t total_subscriptions() const;
  [[nodiscard]] size_t pending_subscriptions() const;
  [[nodiscard]] size_t active_subscriptions() const;
  [[nodiscard]] size_t error_subscriptions() const;

  // Record message received for a subscription
  void record_message(const veloz::common::SymbolId& symbol, MarketEventType event_type);

  // Validation
  [[nodiscard]] bool validate_symbol(const veloz::common::SymbolId& symbol) const;
  [[nodiscard]] bool can_subscribe() const;

private:
  // Key: subscription key (symbol|event_type), Value: set of subscriber IDs
  kj::HashMap<kj::String, kj::HashSet<kj::String>> subscriptions_;

  // Subscription entries with state
  kj::HashMap<kj::String, SubscriptionEntry> entries_;

  // Cache of active symbols (rebuild on subscription changes)
  kj::Vector<veloz::common::SymbolId> active_symbols_cache_;

  // Rate limiting state
  RateLimitConfig rate_limit_config_;
  std::int64_t last_subscribe_time_ns_{0};
  int subscriptions_this_second_{0};

  // Connection state
  std::atomic<bool> connected_{false};

  // Callbacks
  kj::Maybe<SubscriptionStateCallback> state_callback_;
  kj::Maybe<ConnectionLifecycleCallback> connection_callback_;

  // Build subscription key from symbol and event type
  static kj::String make_subscription_key(const veloz::common::SymbolId& symbol,
                                          MarketEventType event_type);

  void rebuild_symbol_cache();
  void transition_state(const veloz::common::SymbolId& symbol, MarketEventType event_type,
                        SubscriptionState new_state);
  bool check_rate_limit();
  std::int64_t now_ns() const;
};

} // namespace veloz::market
