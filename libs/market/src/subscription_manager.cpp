#include "veloz/market/subscription_manager.h"

namespace veloz::market {

std::string make_subscription_key(const veloz::common::SymbolId& symbol,
                                  MarketEventType event_type) {
  return symbol.value + "|" + std::to_string(static_cast<int>(event_type));
}

void SubscriptionManager::subscribe(const veloz::common::SymbolId& symbol,
                                    MarketEventType event_type, const std::string& subscriber_id) {
  auto key = make_subscription_key(symbol, event_type);
  subscriptions_[key].insert(subscriber_id);
  rebuild_symbol_cache();
}

void SubscriptionManager::unsubscribe(const veloz::common::SymbolId& symbol,
                                      MarketEventType event_type,
                                      const std::string& subscriber_id) {
  auto key = make_subscription_key(symbol, event_type);
  auto it = subscriptions_.find(key);
  if (it != subscriptions_.end()) {
    it->second.erase(subscriber_id);
    if (it->second.empty()) {
      subscriptions_.erase(it);
    }
    rebuild_symbol_cache();
  }
}

std::size_t SubscriptionManager::subscriber_count(const veloz::common::SymbolId& symbol,
                                                  MarketEventType event_type) const {
  auto key = make_subscription_key(symbol, event_type);
  auto it = subscriptions_.find(key);
  return it != subscriptions_.end() ? it->second.size() : 0;
}

bool SubscriptionManager::is_subscribed(const veloz::common::SymbolId& symbol,
                                        MarketEventType event_type,
                                        const std::string& subscriber_id) const {
  auto key = make_subscription_key(symbol, event_type);
  auto it = subscriptions_.find(key);
  if (it == subscriptions_.end())
    return false;
  return it->second.contains(subscriber_id);
}

std::vector<veloz::common::SymbolId> SubscriptionManager::active_symbols() const {
  return active_symbols_cache_;
}

std::vector<MarketEventType>
SubscriptionManager::event_types(const veloz::common::SymbolId& symbol) const {
  std::set<MarketEventType> types;
  for (const auto& [key, _] : subscriptions_) {
    if (key.starts_with(symbol.value + "|")) {
      // Parse event type from key
      auto pos = key.find('|');
      if (pos != std::string::npos) {
        int type_int = std::stoi(key.substr(pos + 1));
        types.insert(static_cast<MarketEventType>(type_int));
      }
    }
  }
  return std::vector<MarketEventType>(types.begin(), types.end());
}

std::vector<std::string> SubscriptionManager::subscribers(const veloz::common::SymbolId& symbol,
                                                          MarketEventType event_type) const {
  auto key = make_subscription_key(symbol, event_type);
  auto it = subscriptions_.find(key);
  if (it == subscriptions_.end())
    return {};
  return std::vector<std::string>(it->second.begin(), it->second.end());
}

void SubscriptionManager::rebuild_symbol_cache() {
  std::unordered_set<std::string> unique_symbols;
  for (const auto& [key, _] : subscriptions_) {
    auto pos = key.find('|');
    if (pos != std::string::npos) {
      unique_symbols.insert(key.substr(0, pos));
    }
  }

  active_symbols_cache_.clear();
  active_symbols_cache_.reserve(unique_symbols.size());
  for (const auto& symbol_str : unique_symbols) {
    active_symbols_cache_.push_back(veloz::common::SymbolId{symbol_str});
  }
}

} // namespace veloz::market
