#include "veloz/market/subscription_manager.h"

#include <chrono>
#include <kj/debug.h>

namespace veloz::market {

SubscriptionManager::SubscriptionManager() : rate_limit_config_() {}

SubscriptionManager::SubscriptionManager(RateLimitConfig rate_limit_config)
    : rate_limit_config_(rate_limit_config) {}

void SubscriptionManager::set_rate_limit_config(RateLimitConfig config) {
  rate_limit_config_ = config;
}

void SubscriptionManager::set_state_callback(SubscriptionStateCallback callback) {
  state_callback_ = kj::mv(callback);
}

void SubscriptionManager::set_connection_callback(ConnectionLifecycleCallback callback) {
  connection_callback_ = kj::mv(callback);
}

kj::String SubscriptionManager::make_subscription_key(const veloz::common::SymbolId& symbol,
                                                      MarketEventType event_type) {
  return kj::str(symbol.value, "|", static_cast<int>(event_type));
}

std::int64_t SubscriptionManager::now_ns() const {
  auto now = std::chrono::high_resolution_clock::now();
  return std::chrono::duration_cast<std::chrono::nanoseconds>(now.time_since_epoch()).count();
}

bool SubscriptionManager::check_rate_limit() {
  auto current_time = now_ns();
  auto one_second_ns = 1'000'000'000LL;

  // Reset counter if more than a second has passed
  if (current_time - last_subscribe_time_ns_ > one_second_ns) {
    subscriptions_this_second_ = 0;
    last_subscribe_time_ns_ = current_time;
  }

  // Check rate limit
  if (subscriptions_this_second_ >= rate_limit_config_.max_subscriptions_per_second) {
    KJ_LOG(WARNING, "Rate limit exceeded for subscriptions");
    return false;
  }

  // Check total subscriptions limit
  if (total_subscriptions() >= static_cast<size_t>(rate_limit_config_.max_total_subscriptions)) {
    KJ_LOG(WARNING, "Maximum total subscriptions reached");
    return false;
  }

  return true;
}

bool SubscriptionManager::validate_symbol(const veloz::common::SymbolId& symbol) const {
  // Basic validation: symbol should not be empty and should only contain valid characters
  if (symbol.value.size() == 0) {
    return false;
  }

  // Check for valid characters (alphanumeric and common separators)
  for (char c : symbol.value) {
    if (!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' ||
          c == '_' || c == '/')) {
      return false;
    }
  }

  return true;
}

bool SubscriptionManager::can_subscribe() const {
  return total_subscriptions() < static_cast<size_t>(rate_limit_config_.max_total_subscriptions);
}

bool SubscriptionManager::subscribe(const veloz::common::SymbolId& symbol,
                                    MarketEventType event_type, kj::StringPtr subscriber_id) {
  // Validate symbol
  if (!validate_symbol(symbol)) {
    KJ_LOG(WARNING, "Invalid symbol for subscription", symbol.value);
    return false;
  }

  // Check rate limit
  if (!check_rate_limit()) {
    return false;
  }

  kj::String key = make_subscription_key(symbol, event_type);

  // Find or create subscriber set for this key
  auto find_result = subscriptions_.find(key);
  KJ_IF_SOME(subscribers_ref, find_result) {
    // Key exists, check if subscriber already exists before inserting
    if (!subscribers_ref.contains(subscriber_id)) {
      subscribers_ref.insert(kj::str(subscriber_id));
    }
  }
  else {
    // Key doesn't exist, create new entry with subscriber
    kj::HashSet<kj::String> new_set;
    new_set.insert(kj::str(subscriber_id));
    subscriptions_.insert(kj::heapString(key), kj::mv(new_set));

    // Create subscription entry
    SubscriptionEntry entry;
    entry.symbol = symbol;
    entry.event_type = event_type;
    entry.state = SubscriptionState::Pending;
    entry.created_at_ns = now_ns();
    entry.last_update_ns = entry.created_at_ns;
    entries_.insert(kj::heapString(key), kj::mv(entry));
  }

  subscriptions_this_second_++;
  rebuild_symbol_cache();
  return true;
}

void SubscriptionManager::unsubscribe(const veloz::common::SymbolId& symbol,
                                      MarketEventType event_type, kj::StringPtr subscriber_id) {
  kj::String key = make_subscription_key(symbol, event_type);

  auto find_result = subscriptions_.find(key);
  KJ_IF_SOME(subscribers_ref, find_result) {
    auto& set = subscribers_ref;
    if (set.eraseMatch(subscriber_id)) {
      if (set.size() == 0) {
        subscriptions_.erase(key);
        // Update entry state before removing
        transition_state(symbol, event_type, SubscriptionState::Unsubscribed);
        entries_.erase(key);
      }
      rebuild_symbol_cache();
    }
  }
}

void SubscriptionManager::confirm_subscription(const veloz::common::SymbolId& symbol,
                                               MarketEventType event_type) {
  transition_state(symbol, event_type, SubscriptionState::Active);
}

void SubscriptionManager::mark_error(const veloz::common::SymbolId& symbol,
                                     MarketEventType event_type, kj::StringPtr error_message) {
  kj::String key = make_subscription_key(symbol, event_type);
  auto find_result = entries_.find(key);
  KJ_IF_SOME(entry_ref, find_result) {
    entry_ref.error_message = kj::heapString(error_message);
  }
  transition_state(symbol, event_type, SubscriptionState::Error);
}

void SubscriptionManager::transition_state(const veloz::common::SymbolId& symbol,
                                           MarketEventType event_type,
                                           SubscriptionState new_state) {
  kj::String key = make_subscription_key(symbol, event_type);
  auto find_result = entries_.find(key);
  KJ_IF_SOME(entry_ref, find_result) {
    SubscriptionState old_state = entry_ref.state;
    if (old_state != new_state) {
      entry_ref.state = new_state;
      entry_ref.last_update_ns = now_ns();

      // Notify callback
      KJ_IF_SOME(callback, state_callback_) {
        callback(symbol, event_type, old_state, new_state);
      }
    }
  }
}

void SubscriptionManager::on_connected() {
  connected_ = true;
  KJ_IF_SOME(callback, connection_callback_) {
    callback(true);
  }
}

void SubscriptionManager::on_disconnected() {
  connected_ = false;
  // Pause all subscriptions
  pause_all();
  KJ_IF_SOME(callback, connection_callback_) {
    callback(false);
  }
}

bool SubscriptionManager::is_connected() const {
  return connected_.load();
}

void SubscriptionManager::pause_all() {
  for (auto& entry : entries_) {
    if (entry.value.state == SubscriptionState::Active) {
      SubscriptionState old_state = entry.value.state;
      entry.value.state = SubscriptionState::Paused;
      entry.value.last_update_ns = now_ns();

      KJ_IF_SOME(callback, state_callback_) {
        callback(entry.value.symbol, entry.value.event_type, old_state, SubscriptionState::Paused);
      }
    }
  }
}

void SubscriptionManager::resume_all() {
  for (auto& entry : entries_) {
    if (entry.value.state == SubscriptionState::Paused) {
      SubscriptionState old_state = entry.value.state;
      entry.value.state = SubscriptionState::Pending;
      entry.value.last_update_ns = now_ns();

      KJ_IF_SOME(callback, state_callback_) {
        callback(entry.value.symbol, entry.value.event_type, old_state, SubscriptionState::Pending);
      }
    }
  }
}

size_t SubscriptionManager::subscriber_count(const veloz::common::SymbolId& symbol,
                                             MarketEventType event_type) const {
  kj::String key = make_subscription_key(symbol, event_type);
  auto find_result = subscriptions_.find(key);
  KJ_IF_SOME(subscribers_ref, find_result) {
    return subscribers_ref.size();
  }
  return 0;
}

bool SubscriptionManager::is_subscribed(const veloz::common::SymbolId& symbol,
                                        MarketEventType event_type,
                                        kj::StringPtr subscriber_id) const {
  kj::String key = make_subscription_key(symbol, event_type);
  auto find_result = subscriptions_.find(key);
  KJ_IF_SOME(subscribers_ref, find_result) {
    return subscribers_ref.contains(subscriber_id);
  }
  return false;
}

SubscriptionState SubscriptionManager::get_state(const veloz::common::SymbolId& symbol,
                                                 MarketEventType event_type) const {
  kj::String key = make_subscription_key(symbol, event_type);
  auto find_result = entries_.find(key);
  KJ_IF_SOME(entry_ref, find_result) {
    return entry_ref.state;
  }
  return SubscriptionState::Unsubscribed;
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
  kj::StringPtr symbol_prefix = symbol.value;

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
  KJ_IF_SOME(subscribers_ref, find_result) {
    for (const auto& sub : subscribers_ref) {
      result.add(kj::str(sub.asPtr()));
    }
  }
  return result;
}

kj::Maybe<SubscriptionEntry> SubscriptionManager::get_entry(const veloz::common::SymbolId& symbol,
                                                            MarketEventType event_type) const {
  kj::String key = make_subscription_key(symbol, event_type);
  auto find_result = entries_.find(key);
  KJ_IF_SOME(entry_ref, find_result) {
    // Return a copy
    SubscriptionEntry copy;
    copy.symbol = entry_ref.symbol;
    copy.event_type = entry_ref.event_type;
    copy.state = entry_ref.state;
    copy.created_at_ns = entry_ref.created_at_ns;
    copy.last_update_ns = entry_ref.last_update_ns;
    copy.message_count = entry_ref.message_count;
    copy.error_message = kj::heapString(entry_ref.error_message);
    return copy;
  }
  return kj::none;
}

size_t SubscriptionManager::total_subscriptions() const {
  return entries_.size();
}

size_t SubscriptionManager::pending_subscriptions() const {
  size_t count = 0;
  for (const auto& entry : entries_) {
    if (entry.value.state == SubscriptionState::Pending) {
      ++count;
    }
  }
  return count;
}

size_t SubscriptionManager::active_subscriptions() const {
  size_t count = 0;
  for (const auto& entry : entries_) {
    if (entry.value.state == SubscriptionState::Active) {
      ++count;
    }
  }
  return count;
}

size_t SubscriptionManager::error_subscriptions() const {
  size_t count = 0;
  for (const auto& entry : entries_) {
    if (entry.value.state == SubscriptionState::Error) {
      ++count;
    }
  }
  return count;
}

void SubscriptionManager::record_message(const veloz::common::SymbolId& symbol,
                                         MarketEventType event_type) {
  kj::String key = make_subscription_key(symbol, event_type);
  auto find_result = entries_.find(key);
  KJ_IF_SOME(entry_ref, find_result) {
    entry_ref.message_count++;
    entry_ref.last_update_ns = now_ns();
  }
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
    active_symbols_cache_.add(veloz::common::SymbolId{symbol_str});
  }
}

} // namespace veloz::market
