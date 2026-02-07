#include <gtest/gtest.h>
#include "veloz/market/subscription_manager.h"
#include "veloz/market/market_event.h"

using namespace veloz::market;

TEST(SubscriptionManager, AddSubscription) {
    SubscriptionManager sub_mgr;
    veloz::common::SymbolId symbol{"BTCUSDT"};

    sub_mgr.subscribe(symbol, MarketEventType::Trade, "strategy_1");
    sub_mgr.subscribe(symbol, MarketEventType::BookTop, "strategy_1");

    EXPECT_EQ(sub_mgr.subscriber_count(symbol, MarketEventType::Trade), 1);
    EXPECT_EQ(sub_mgr.subscriber_count(symbol, MarketEventType::BookTop), 1);
}

TEST(SubscriptionManager, DeduplicateSameSubscriber) {
    SubscriptionManager sub_mgr;
    veloz::common::SymbolId symbol{"ETHUSDT"};

    sub_mgr.subscribe(symbol, MarketEventType::Trade, "strategy_1");
    sub_mgr.subscribe(symbol, MarketEventType::Trade, "strategy_1");  // duplicate

    EXPECT_EQ(sub_mgr.subscriber_count(symbol, MarketEventType::Trade), 1);
}

TEST(SubscriptionManager, MultipleSubscribers) {
    SubscriptionManager sub_mgr;
    veloz::common::SymbolId symbol{"BTCUSDT"};

    sub_mgr.subscribe(symbol, MarketEventType::Trade, "strategy_1");
    sub_mgr.subscribe(symbol, MarketEventType::Trade, "strategy_2");
    sub_mgr.subscribe(symbol, MarketEventType::Trade, "strategy_3");

    EXPECT_EQ(sub_mgr.subscriber_count(symbol, MarketEventType::Trade), 3);
}

TEST(SubscriptionManager, Unsubscribe) {
    SubscriptionManager sub_mgr;
    veloz::common::SymbolId symbol{"BTCUSDT"};

    sub_mgr.subscribe(symbol, MarketEventType::Trade, "strategy_1");
    sub_mgr.subscribe(symbol, MarketEventType::Trade, "strategy_2");

    sub_mgr.unsubscribe(symbol, MarketEventType::Trade, "strategy_1");

    EXPECT_EQ(sub_mgr.subscriber_count(symbol, MarketEventType::Trade), 1);
}

TEST(SubscriptionManager, GetActiveSymbols) {
    SubscriptionManager sub_mgr;

    sub_mgr.subscribe(veloz::common::SymbolId{"BTCUSDT"}, MarketEventType::Trade, "s1");
    sub_mgr.subscribe(veloz::common::SymbolId{"ETHUSDT"}, MarketEventType::Trade, "s1");
    sub_mgr.subscribe(veloz::common::SymbolId{"BTCUSDT"}, MarketEventType::BookTop, "s2");

    auto symbols = sub_mgr.active_symbols();
    EXPECT_EQ(symbols.size(), 2);
}
