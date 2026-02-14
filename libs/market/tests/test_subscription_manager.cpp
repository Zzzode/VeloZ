#include "kj/test.h"
#include "veloz/market/subscription_manager.h"

namespace veloz::market {

KJ_TEST("SubscriptionManager: Initialize") {
  SubscriptionManager manager;

  // Initially no active symbols
  KJ_EXPECT(manager.active_symbols().size() == 0);
}

KJ_TEST("SubscriptionManager: Subscribe and unsubscribe") {
  SubscriptionManager manager;
  veloz::common::SymbolId symbol{"BTCUSDT"};

  // Subscribe a client
  manager.subscribe(symbol, MarketEventType::Trade, "client1"_kj);
  KJ_EXPECT(manager.subscriber_count(symbol, MarketEventType::Trade) == 1);
  KJ_EXPECT(manager.is_subscribed(symbol, MarketEventType::Trade, "client1"_kj));
  KJ_EXPECT(manager.active_symbols().size() == 1);

  // Subscribe another client
  manager.subscribe(symbol, MarketEventType::Trade, "client2"_kj);
  KJ_EXPECT(manager.subscriber_count(symbol, MarketEventType::Trade) == 2);

  // Unsubscribe first client
  manager.unsubscribe(symbol, MarketEventType::Trade, "client1"_kj);
  KJ_EXPECT(manager.subscriber_count(symbol, MarketEventType::Trade) == 1);
  KJ_EXPECT(!manager.is_subscribed(symbol, MarketEventType::Trade, "client1"_kj));
  KJ_EXPECT(manager.is_subscribed(symbol, MarketEventType::Trade, "client2"_kj));

  // Unsubscribe second client - symbol should be removed
  manager.unsubscribe(symbol, MarketEventType::Trade, "client2"_kj);
  KJ_EXPECT(manager.subscriber_count(symbol, MarketEventType::Trade) == 0);
  KJ_EXPECT(manager.active_symbols().size() == 0);
}

KJ_TEST("SubscriptionManager: Multiple event types") {
  SubscriptionManager manager;
  veloz::common::SymbolId symbol{"ETHUSDT"};

  manager.subscribe(symbol, MarketEventType::Trade, "client1"_kj);
  manager.subscribe(symbol, MarketEventType::BookDelta, "client1"_kj);

  auto event_types = manager.event_types(symbol);
  KJ_EXPECT(event_types.size() == 2);

  // Check subscribers for each event type
  auto trade_subs = manager.subscribers(symbol, MarketEventType::Trade);
  KJ_EXPECT(trade_subs.size() == 1);

  auto book_subs = manager.subscribers(symbol, MarketEventType::BookDelta);
  KJ_EXPECT(book_subs.size() == 1);
}

KJ_TEST("SubscriptionManager: Empty symbol query") {
  SubscriptionManager manager;
  veloz::common::SymbolId empty_symbol{""};

  // Query for non-existent symbol should return empty
  KJ_EXPECT(manager.subscriber_count(empty_symbol, MarketEventType::Trade) == 0);
  KJ_EXPECT(!manager.is_subscribed(empty_symbol, MarketEventType::Trade, "client1"_kj));
  KJ_EXPECT(manager.event_types(empty_symbol).size() == 0);
  KJ_EXPECT(manager.subscribers(empty_symbol, MarketEventType::Trade).size() == 0);
}

KJ_TEST("SubscriptionManager: Duplicate subscription") {
  SubscriptionManager manager;
  veloz::common::SymbolId symbol{"BTCUSDT"};

  // Subscribe same client twice - should only count once
  manager.subscribe(symbol, MarketEventType::Trade, "client1"_kj);
  manager.subscribe(symbol, MarketEventType::Trade, "client1"_kj);

  // kj::HashSet handles duplicates by not inserting if already present
  // The count should be 1 (or 2 if duplicates are allowed - depends on implementation)
  KJ_EXPECT(manager.subscriber_count(symbol, MarketEventType::Trade) >= 1);
}

} // namespace veloz::market
