#include "veloz/market/subscription_manager.h"

namespace veloz::market {

kj::String SubscriptionManager::make_subscription_key(const veloz::common::SymbolId& symbol,
                                                      MarketEventType event_type) {
  return kj::str(symbol.value, "|", static_cast<int>(event_type));
}

void SubscriptionManager::subscribe(const veloz::common::SymbolId& symbol,
                                    MarketEventType event_type, kj::StringPtr subscriber_id) {
  kj::String key = make_subscription_key(symbol, event_type);

  // Find or create subscriber set for this key
  auto find_result = subscriptions_.find(key);
  KJ_IF_MAYBE (subscribers_ref, find_result) {
    // Key exists, check if subscriber already exists before inserting
    // kj::HashSet::insert throws on duplicates, so we check first
    if (!(*subscribers_ref).contains(subscriber_id)) {
      (*subscribers_ref).insert(kj::str(subscriber_id));
    }
  } else {
    // Key doesn't exist, create new entry with subscriber
    kj::HashSet<kj::String> new_set;
    new_set.insert(kj::str(subscriber_id));
    subscriptions_.insert(kj::mv(key), kj::mv(new_set));
  }
  rebuild_symbol_cache();
}

void SubscriptionManager::unsubscribe(const veloz::common::SymbolId& symbol,
                                      MarketEventType event_type, kj::StringPtr subscriber_id) {
  kj::String key = make_subscription_key(symbol, event_type);

  auto find_result = subscriptions_.find(key);
  KJ_IF_MAYBE (subscribers_ref, find_result) {
    auto& set = *subscribers_ref;
    // eraseMatch takes a key directly and returns bool indicating if element was found
    if (set.eraseMatch(subscriber_id)) {
      if (set.size() == 0) {
        subscriptions_.erase(key);
      }
      rebuild_symbol_cache();
    }
  }
}

size_t SubscriptionManager::subscriber_count(const veloz::common::SymbolId& symbol,
                                             MarketEventType event_type) const {
  kj::String key = make_subscription_key(symbol, event_type);
  auto find_result = subscriptions_.find(key);
  KJ_IF_MAYBE (subscribers_ref, find_result) {
    return (*subscribers_ref).size();
  }
  return 0;
}

bool SubscriptionManager::is_subscribed(const veloz::common::SymbolId& symbol,
                                        MarketEventType event_type,
                                        kj::StringPtr subscriber_id) const {
  kj::String key = make_subscription_key(symbol, event_type);
  auto find_result = subscriptions_.find(key);
  KJ_IF_MAYBE (subscribers_ref, find_result) {
    return (*subscribers_ref).contains(subscriber_id);
  }
  return false;
}

kj::Vector<veloz::common::SymbolId> SubscriptionManager::active_symbols() const {
  kj::Vector<veloz::common::SymbolId> result;
  for (const auto& symbol : active_symbols_cache_) {
    result.add(symbol);
  }
  return result;
}

kj::Vector<MarketEventType>
SubscriptionManager::event_types(const veloz::common::SymbolId& symbol) const {
  // Use kj::HashSet to collect unique event types
  kj::HashSet<int> type_ints;
  kj::StringPtr symbol_prefix = kj::StringPtr(symbol.value);

  for (const auto& entry : subscriptions_) {
    kj::StringPtr key = entry.key;
    // Check if key starts with "symbol|"
    if (key.size() > symbol_prefix.size() + 1) {
      // Manual prefix check since kj::StringPtr doesn't have starts_with
      bool matches = true;
      for (size_t i = 0; i < symbol_prefix.size(); ++i) {
        if (key[i] != symbol_prefix[i]) {
          matches = false;
          break;
        }
      }
      if (matches && key[symbol_prefix.size()] == '|') {
        // Parse event type from key after the '|'
        kj::StringPtr suffix = key.slice(symbol_prefix.size() + 1);
        // Convert suffix to int
        int type_int = 0;
        for (size_t i = 0; i < suffix.size(); ++i) {
          if (suffix[i] >= '0' && suffix[i] <= '9') {
            type_int = type_int * 10 + (suffix[i] - '0');
          }
        }
        // Check before inserting to avoid duplicate exception
        if (!type_ints.contains(type_int)) {
          type_ints.insert(type_int);
        }
      }
    }
  }

  kj::Vector<MarketEventType> result;
  for (const auto& type_int : type_ints) {
    result.add(static_cast<MarketEventType>(type_int));
  }
  return result;
}

kj::Vector<kj::String> SubscriptionManager::subscribers(const veloz::common::SymbolId& symbol,
                                                        MarketEventType event_type) const {
  kj::Vector<kj::String> result;
  kj::String key = make_subscription_key(symbol, event_type);

  auto find_result = subscriptions_.find(key);
  KJ_IF_MAYBE (subscribers_ref, find_result) {
    for (const auto& sub : *subscribers_ref) {
      result.add(kj::str(sub.asPtr()));
    }
  }
  return result;
}

void SubscriptionManager::rebuild_symbol_cache() {
  // Use kj::HashSet to collect unique symbols
  kj::HashSet<kj::String> unique_symbols;

  for (const auto& entry : subscriptions_) {
    kj::StringPtr key = entry.key;
    // Find '|' separator
    for (size_t i = 0; i < key.size(); ++i) {
      if (key[i] == '|') {
        kj::String symbol_str = kj::str(key.slice(0, i));
        // Check before inserting to avoid duplicate exception
        if (!unique_symbols.contains(kj::StringPtr(symbol_str))) {
          unique_symbols.insert(kj::mv(symbol_str));
        }
        break;
      }
    }
  }

  active_symbols_cache_.clear();
  for (const auto& symbol_str : unique_symbols) {
    active_symbols_cache_.add(veloz::common::SymbolId{std::string(symbol_str.cStr())});
  }
}

} // namespace veloz::market
