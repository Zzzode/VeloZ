#pragma once

#include "veloz/common/types.h"
#include "veloz/market/market_event.h"

#include <set>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace veloz::market {

class SubscriptionManager final {
public:
  SubscriptionManager() = default;

  // Subscribe a client to a symbol/event type
  void subscribe(const veloz::common::SymbolId& symbol, MarketEventType event_type,
                 const std::string& subscriber_id);

  // Unsubscribe a client from a symbol/event type
  void unsubscribe(const veloz::common::SymbolId& symbol, MarketEventType event_type,
                   const std::string& subscriber_id);

  // Query methods
  [[nodiscard]] std::size_t subscriber_count(const veloz::common::SymbolId& symbol,
                                             MarketEventType event_type) const;

  [[nodiscard]] bool is_subscribed(const veloz::common::SymbolId& symbol,
                                   MarketEventType event_type,
                                   const std::string& subscriber_id) const;

  // Get all unique symbols with any subscriptions
  [[nodiscard]] std::vector<veloz::common::SymbolId> active_symbols() const;

  // Get all event types subscribed for a symbol
  [[nodiscard]] std::vector<MarketEventType>
  event_types(const veloz::common::SymbolId& symbol) const;

  // Get all subscribers for a symbol/event type
  [[nodiscard]] std::vector<std::string> subscribers(const veloz::common::SymbolId& symbol,
                                                     MarketEventType event_type) const;

private:
  // Key: symbol + event_type, Value: set of subscriber IDs
  std::unordered_map<std::string, std::unordered_set<std::string>> subscriptions_;

  // Cache of active symbols (rebuild on subscription changes)
  std::vector<veloz::common::SymbolId> active_symbols_cache_;

  void rebuild_symbol_cache();
};

} // namespace veloz::market
