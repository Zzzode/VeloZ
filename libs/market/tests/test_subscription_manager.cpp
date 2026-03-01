#include "kj/test.h"
#include "veloz/market/subscription_manager.h"

namespace veloz::market {

KJ_TEST("SubscriptionManager: Initialize") {
  SubscriptionManager manager;

  // Initially no active symbols
  KJ_EXPECT(manager.active_symbols().size() == 0);
  KJ_EXPECT(manager.total_subscriptions() == 0);
  KJ_EXPECT(!manager.is_connected());
}

KJ_TEST("SubscriptionManager: Initialize with rate limit config") {
  RateLimitConfig config;
  config.max_subscriptions_per_second = 5;
  config.max_total_subscriptions = 100;

  SubscriptionManager manager(config);
  KJ_EXPECT(manager.active_symbols().size() == 0);
}

KJ_TEST("SubscriptionManager: Subscribe and unsubscribe") {
  SubscriptionManager manager;
  veloz::common::SymbolId symbol{"BTCUSDT"};

  // Subscribe a client
  KJ_EXPECT(manager.subscribe(symbol, MarketEventType::Trade, "client1"_kj));
  KJ_EXPECT(manager.subscriber_count(symbol, MarketEventType::Trade) == 1);
  KJ_EXPECT(manager.is_subscribed(symbol, MarketEventType::Trade, "client1"_kj));
  KJ_EXPECT(manager.active_symbols().size() == 1);
  KJ_EXPECT(manager.total_subscriptions() == 1);

  // Subscribe another client
  KJ_EXPECT(manager.subscribe(symbol, MarketEventType::Trade, "client2"_kj));
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
  KJ_EXPECT(manager.subscriber_count(symbol, MarketEventType::Trade) == 1);
}

KJ_TEST("SubscriptionManager: Subscription state tracking") {
  SubscriptionManager manager;
  veloz::common::SymbolId symbol{"BTCUSDT"};

  // Initial state should be Pending
  manager.subscribe(symbol, MarketEventType::Trade, "client1"_kj);
  KJ_EXPECT(manager.get_state(symbol, MarketEventType::Trade) == SubscriptionState::Pending);
  KJ_EXPECT(manager.pending_subscriptions() == 1);
  KJ_EXPECT(manager.active_subscriptions() == 0);

  // Confirm subscription
  manager.confirm_subscription(symbol, MarketEventType::Trade);
  KJ_EXPECT(manager.get_state(symbol, MarketEventType::Trade) == SubscriptionState::Active);
  KJ_EXPECT(manager.pending_subscriptions() == 0);
  KJ_EXPECT(manager.active_subscriptions() == 1);
}

KJ_TEST("SubscriptionManager: Error state") {
  SubscriptionManager manager;
  veloz::common::SymbolId symbol{"BTCUSDT"};

  manager.subscribe(symbol, MarketEventType::Trade, "client1"_kj);
  manager.mark_error(symbol, MarketEventType::Trade, "Connection refused"_kj);

  KJ_EXPECT(manager.get_state(symbol, MarketEventType::Trade) == SubscriptionState::Error);
  KJ_EXPECT(manager.error_subscriptions() == 1);

  // Check error message in entry
  auto maybe_entry = manager.get_entry(symbol, MarketEventType::Trade);
  KJ_IF_SOME(entry, maybe_entry) {
    KJ_EXPECT(entry.error_message == "Connection refused"_kj);
  }
  else {
    KJ_FAIL_EXPECT("Entry should exist");
  }
}

KJ_TEST("SubscriptionManager: Connection lifecycle") {
  SubscriptionManager manager;
  veloz::common::SymbolId symbol{"BTCUSDT"};

  manager.subscribe(symbol, MarketEventType::Trade, "client1"_kj);
  manager.confirm_subscription(symbol, MarketEventType::Trade);
  KJ_EXPECT(manager.get_state(symbol, MarketEventType::Trade) == SubscriptionState::Active);

  // Simulate disconnect - subscriptions should be paused
  manager.on_disconnected();
  KJ_EXPECT(!manager.is_connected());
  KJ_EXPECT(manager.get_state(symbol, MarketEventType::Trade) == SubscriptionState::Paused);

  // Simulate reconnect
  manager.on_connected();
  KJ_EXPECT(manager.is_connected());

  // Resume subscriptions
  manager.resume_all();
  KJ_EXPECT(manager.get_state(symbol, MarketEventType::Trade) == SubscriptionState::Pending);
}

KJ_TEST("SubscriptionManager: State callback") {
  SubscriptionManager manager;
  veloz::common::SymbolId symbol{"BTCUSDT"};

  int callback_count = 0;
  SubscriptionState last_old_state = SubscriptionState::Pending;
  SubscriptionState last_new_state = SubscriptionState::Pending;

  manager.set_state_callback([&](const veloz::common::SymbolId&, MarketEventType,
                                 SubscriptionState old_state, SubscriptionState new_state) {
    callback_count++;
    last_old_state = old_state;
    last_new_state = new_state;
  });

  manager.subscribe(symbol, MarketEventType::Trade, "client1"_kj);
  manager.confirm_subscription(symbol, MarketEventType::Trade);

  KJ_EXPECT(callback_count == 1);
  KJ_EXPECT(last_old_state == SubscriptionState::Pending);
  KJ_EXPECT(last_new_state == SubscriptionState::Active);
}

KJ_TEST("SubscriptionManager: Connection callback") {
  SubscriptionManager manager;

  int callback_count = 0;
  bool last_connected = false;

  manager.set_connection_callback([&](bool connected) {
    callback_count++;
    last_connected = connected;
  });

  manager.on_connected();
  KJ_EXPECT(callback_count == 1);
  KJ_EXPECT(last_connected == true);

  manager.on_disconnected();
  KJ_EXPECT(callback_count == 2);
  KJ_EXPECT(last_connected == false);
}

KJ_TEST("SubscriptionManager: Symbol validation") {
  SubscriptionManager manager;

  // Valid symbols
  KJ_EXPECT(manager.validate_symbol(veloz::common::SymbolId{"BTCUSDT"}));
  KJ_EXPECT(manager.validate_symbol(veloz::common::SymbolId{"BTC-USDT"}));
  KJ_EXPECT(manager.validate_symbol(veloz::common::SymbolId{"BTC_USDT"}));
  KJ_EXPECT(manager.validate_symbol(veloz::common::SymbolId{"BTC/USDT"}));

  // Invalid symbols
  KJ_EXPECT(!manager.validate_symbol(veloz::common::SymbolId{""}));
  KJ_EXPECT(!manager.validate_symbol(veloz::common::SymbolId{"BTC USDT"})); // space
  KJ_EXPECT(!manager.validate_symbol(veloz::common::SymbolId{"BTC@USDT"})); // @
}

KJ_TEST("SubscriptionManager: Invalid symbol subscription rejected") {
  SubscriptionManager manager;
  veloz::common::SymbolId invalid_symbol{""};

  // Subscription should fail for invalid symbol
  KJ_EXPECT(!manager.subscribe(invalid_symbol, MarketEventType::Trade, "client1"_kj));
  KJ_EXPECT(manager.total_subscriptions() == 0);
}

KJ_TEST("SubscriptionManager: Rate limiting") {
  RateLimitConfig config;
  config.max_subscriptions_per_second = 2;
  config.max_total_subscriptions = 100;

  SubscriptionManager manager(config);

  // First two subscriptions should succeed
  KJ_EXPECT(
      manager.subscribe(veloz::common::SymbolId{"BTCUSDT"}, MarketEventType::Trade, "client1"_kj));
  KJ_EXPECT(
      manager.subscribe(veloz::common::SymbolId{"ETHUSDT"}, MarketEventType::Trade, "client1"_kj));

  // Third subscription should fail due to rate limit
  KJ_EXPECT(
      !manager.subscribe(veloz::common::SymbolId{"XRPUSDT"}, MarketEventType::Trade, "client1"_kj));
}

KJ_TEST("SubscriptionManager: Max total subscriptions") {
  RateLimitConfig config;
  config.max_subscriptions_per_second = 100;
  config.max_total_subscriptions = 2;

  SubscriptionManager manager(config);

  // First two subscriptions should succeed
  KJ_EXPECT(
      manager.subscribe(veloz::common::SymbolId{"BTCUSDT"}, MarketEventType::Trade, "client1"_kj));
  KJ_EXPECT(
      manager.subscribe(veloz::common::SymbolId{"ETHUSDT"}, MarketEventType::Trade, "client1"_kj));

  // Third subscription should fail due to max total limit
  KJ_EXPECT(
      !manager.subscribe(veloz::common::SymbolId{"XRPUSDT"}, MarketEventType::Trade, "client1"_kj));
}

KJ_TEST("SubscriptionManager: Message recording") {
  SubscriptionManager manager;
  veloz::common::SymbolId symbol{"BTCUSDT"};

  manager.subscribe(symbol, MarketEventType::Trade, "client1"_kj);

  // Record some messages
  manager.record_message(symbol, MarketEventType::Trade);
  manager.record_message(symbol, MarketEventType::Trade);
  manager.record_message(symbol, MarketEventType::Trade);

  auto maybe_entry = manager.get_entry(symbol, MarketEventType::Trade);
  KJ_IF_SOME(entry, maybe_entry) {
    KJ_EXPECT(entry.message_count == 3);
  }
  else {
    KJ_FAIL_EXPECT("Entry should exist");
  }
}

KJ_TEST("SubscriptionManager: Get entry for non-existent subscription") {
  SubscriptionManager manager;
  veloz::common::SymbolId symbol{"BTCUSDT"};

  auto maybe_entry = manager.get_entry(symbol, MarketEventType::Trade);
  KJ_EXPECT(maybe_entry == kj::none);
}

KJ_TEST("SubscriptionManager: Statistics") {
  SubscriptionManager manager;

  manager.subscribe(veloz::common::SymbolId{"BTCUSDT"}, MarketEventType::Trade, "client1"_kj);
  manager.subscribe(veloz::common::SymbolId{"ETHUSDT"}, MarketEventType::Trade, "client1"_kj);
  manager.subscribe(veloz::common::SymbolId{"XRPUSDT"}, MarketEventType::Trade, "client1"_kj);

  KJ_EXPECT(manager.total_subscriptions() == 3);
  KJ_EXPECT(manager.pending_subscriptions() == 3);
  KJ_EXPECT(manager.active_subscriptions() == 0);
  KJ_EXPECT(manager.error_subscriptions() == 0);

  // Confirm one
  manager.confirm_subscription(veloz::common::SymbolId{"BTCUSDT"}, MarketEventType::Trade);
  KJ_EXPECT(manager.pending_subscriptions() == 2);
  KJ_EXPECT(manager.active_subscriptions() == 1);

  // Error one
  manager.mark_error(veloz::common::SymbolId{"ETHUSDT"}, MarketEventType::Trade, "Failed"_kj);
  KJ_EXPECT(manager.pending_subscriptions() == 1);
  KJ_EXPECT(manager.error_subscriptions() == 1);
}

KJ_TEST("SubscriptionManager: can_subscribe check") {
  RateLimitConfig config;
  config.max_total_subscriptions = 2;

  SubscriptionManager manager(config);

  KJ_EXPECT(manager.can_subscribe());

  manager.subscribe(veloz::common::SymbolId{"BTCUSDT"}, MarketEventType::Trade, "client1"_kj);
  KJ_EXPECT(manager.can_subscribe());

  manager.subscribe(veloz::common::SymbolId{"ETHUSDT"}, MarketEventType::Trade, "client1"_kj);
  KJ_EXPECT(!manager.can_subscribe());
}

} // namespace veloz::market
