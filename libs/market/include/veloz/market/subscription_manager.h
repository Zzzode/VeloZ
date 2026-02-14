#pragma once

#include "veloz/common/types.h"
#include "veloz/market/market_event.h"

#include <kj/common.h>
#include <kj/hash.h>
#include <kj/map.h>
#include <kj/string.h>
#include <kj/vector.h>

namespace veloz::market {

class SubscriptionManager final {
public:
  SubscriptionManager() = default;

  // Subscribe a client to a symbol/event type
  void subscribe(const veloz::common::SymbolId& symbol, MarketEventType event_type,
                 kj::StringPtr subscriber_id);

  // Unsubscribe a client from a symbol/event type
  void unsubscribe(const veloz::common::SymbolId& symbol, MarketEventType event_type,
                   kj::StringPtr subscriber_id);

  // Query methods
  [[nodiscard]] size_t subscriber_count(const veloz::common::SymbolId& symbol,
                                        MarketEventType event_type) const;

  [[nodiscard]] bool is_subscribed(const veloz::common::SymbolId& symbol,
                                   MarketEventType event_type, kj::StringPtr subscriber_id) const;

  // Get all unique symbols with any subscriptions
  [[nodiscard]] kj::Vector<veloz::common::SymbolId> active_symbols() const;

  // Get all event types subscribed for a symbol
  [[nodiscard]] kj::Vector<MarketEventType>
  event_types(const veloz::common::SymbolId& symbol) const;

  // Get all subscribers for a symbol/event type
  [[nodiscard]] kj::Vector<kj::String> subscribers(const veloz::common::SymbolId& symbol,
                                                   MarketEventType event_type) const;

private:
  // Key: subscription key (symbol|event_type), Value: set of subscriber IDs
  // Using kj::HashMap with kj::String keys for KJ-native hash-based operations.
  // kj::HashSet<kj::String> stores subscriber IDs with O(1) lookup/insert/erase.
  kj::HashMap<kj::String, kj::HashSet<kj::String>> subscriptions_;

  // Cache of active symbols (rebuild on subscription changes)
  kj::Vector<veloz::common::SymbolId> active_symbols_cache_;

  // Build subscription key from symbol and event type
  static kj::String make_subscription_key(const veloz::common::SymbolId& symbol,
                                          MarketEventType event_type);

  void rebuild_symbol_cache();
};

} // namespace veloz::market
