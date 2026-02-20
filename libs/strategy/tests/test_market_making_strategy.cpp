#include "kj/test.h"
#include "veloz/strategy/market_making_strategy.h"

namespace {

using namespace veloz::strategy;
using namespace veloz::market;
using namespace veloz::exec;
using namespace veloz::common;

// Helper to create a default config
StrategyConfig create_default_config() {
  StrategyConfig config;
  config.name = kj::heapString("TestMarketMaker");
  config.type = StrategyType::MarketMaking;
  config.risk_per_trade = 0.01;
  config.max_position_size = 10.0;
  config.stop_loss = 0.05;
  config.take_profit = 0.10;
  config.symbols.add(kj::heapString("BTCUSDT"));
  return config;
}

// Helper to create a ticker event (using BookData since TickerData doesn't exist in OneOf)
MarketEvent create_ticker_event(double bid, double ask) {
  MarketEvent event;
  event.type = MarketEventType::Ticker;
  event.symbol = SymbolId("BTCUSDT"_kj);
  event.ts_exchange_ns = 1000000000;
  event.ts_recv_ns = 1000000001;

  BookData book;
  BookLevel bid_level;
  bid_level.price = bid;
  bid_level.qty = 1.0;
  book.bids.add(bid_level);

  BookLevel ask_level;
  ask_level.price = ask;
  ask_level.qty = 1.0;
  book.asks.add(ask_level);

  event.data = kj::mv(book);

  return event;
}

// Helper to create a trade event
MarketEvent create_trade_event(double price, double qty, bool is_buyer_maker) {
  MarketEvent event;
  event.type = MarketEventType::Trade;
  event.symbol = SymbolId("BTCUSDT"_kj);
  event.ts_exchange_ns = 1000000000;
  event.ts_recv_ns = 1000000001;

  veloz::market::TradeData trade;
  trade.price = price;
  trade.qty = qty;
  trade.is_buyer_maker = is_buyer_maker;
  event.data = trade;

  return event;
}

KJ_TEST("MarketMakingStrategy: Basic construction") {
  auto config = create_default_config();
  MarketMakingStrategy strategy(config);

  KJ_EXPECT(strategy.get_type() == StrategyType::MarketMaking);
  KJ_EXPECT(strategy.get_name() == "TestMarketMaker"_kj);
  KJ_EXPECT(strategy.get_mid_price() == 0.0);
  KJ_EXPECT(strategy.get_inventory() == 0.0);
}

KJ_TEST("MarketMakingStrategy: Get strategy type") {
  KJ_EXPECT(MarketMakingStrategy::get_strategy_type() == "MarketMakingStrategy"_kj);
}

KJ_TEST("MarketMakingStrategy: Update mid price from ticker") {
  auto config = create_default_config();
  MarketMakingStrategy strategy(config);
  strategy.on_start();

  auto event = create_ticker_event(50000.0, 50010.0);
  strategy.on_event(event);

  KJ_EXPECT(strategy.get_mid_price() == 50005.0);
}

KJ_TEST("MarketMakingStrategy: Generate quotes on ticker") {
  auto config = create_default_config();
  config.parameters.insert(kj::heapString("base_spread"), 0.001); // 0.1%
  config.parameters.insert(kj::heapString("order_size"), 0.1);

  MarketMakingStrategy strategy(config);
  strategy.on_start();

  auto event = create_ticker_event(50000.0, 50010.0);
  strategy.on_event(event);

  auto signals = strategy.get_signals();
  // Should have both bid and ask orders
  KJ_EXPECT(signals.size() == 2);

  // Check that we have one buy and one sell
  bool has_buy = false;
  bool has_sell = false;
  for (const auto& signal : signals) {
    if (signal.side == OrderSide::Buy) {
      has_buy = true;
      KJ_EXPECT(signal.qty == 0.1);
    } else if (signal.side == OrderSide::Sell) {
      has_sell = true;
      KJ_EXPECT(signal.qty == 0.1);
    }
  }
  KJ_EXPECT(has_buy);
  KJ_EXPECT(has_sell);
}

KJ_TEST("MarketMakingStrategy: Spread calculation") {
  auto config = create_default_config();
  config.parameters.insert(kj::heapString("base_spread"), 0.002); // 0.2%
  config.parameters.insert(kj::heapString("min_spread"), 0.001);
  config.parameters.insert(kj::heapString("max_spread"), 0.01);

  MarketMakingStrategy strategy(config);
  strategy.on_start();

  auto event = create_ticker_event(50000.0, 50010.0);
  strategy.on_event(event);

  double spread = strategy.get_current_spread();
  KJ_EXPECT(spread >= 0.001); // At least min spread
  KJ_EXPECT(spread <= 0.01);  // At most max spread
}

KJ_TEST("MarketMakingStrategy: Quote prices around mid") {
  auto config = create_default_config();
  config.parameters.insert(kj::heapString("base_spread"), 0.002); // 0.2%
  config.parameters.insert(kj::heapString("order_size"), 0.1);
  config.parameters.insert(kj::heapString("volatility_adjustment"),
                           0.0); // Disable for predictable test

  MarketMakingStrategy strategy(config);
  strategy.on_start();

  auto event = create_ticker_event(50000.0, 50010.0);
  strategy.on_event(event);

  double mid = strategy.get_mid_price();
  double bid = strategy.get_bid_price();
  double ask = strategy.get_ask_price();

  // Bid should be below mid, ask should be above mid
  KJ_EXPECT(bid < mid);
  KJ_EXPECT(ask > mid);
  KJ_EXPECT(bid < ask);

  // Spread should be approximately base_spread * mid_price
  // Note: actual spread may vary due to inventory skew and other adjustments
  double expected_half_spread = 0.002 * mid / 2.0;
  // Allow 100% tolerance since spread can be adjusted by inventory skew
  KJ_EXPECT(std::abs((mid - bid) - expected_half_spread) < expected_half_spread);
  KJ_EXPECT(std::abs((ask - mid) - expected_half_spread) < expected_half_spread);
}

KJ_TEST("MarketMakingStrategy: Reset clears state") {
  auto config = create_default_config();
  MarketMakingStrategy strategy(config);
  strategy.on_start();

  auto event = create_ticker_event(50000.0, 50010.0);
  strategy.on_event(event);

  KJ_EXPECT(strategy.get_mid_price() > 0);

  strategy.reset();

  KJ_EXPECT(strategy.get_mid_price() == 0.0);
  KJ_EXPECT(strategy.get_inventory() == 0.0);
  KJ_EXPECT(strategy.get_bid_price() == 0.0);
  KJ_EXPECT(strategy.get_ask_price() == 0.0);
}

KJ_TEST("MarketMakingStrategy: Hot reload parameters") {
  auto config = create_default_config();
  config.parameters.insert(kj::heapString("base_spread"), 0.001);

  MarketMakingStrategy strategy(config);

  KJ_EXPECT(strategy.supports_hot_reload());

  kj::TreeMap<kj::String, double> new_params;
  new_params.insert(kj::heapString("base_spread"), 0.002);
  new_params.insert(kj::heapString("order_size"), 0.5);

  bool result = strategy.update_parameters(new_params);
  KJ_EXPECT(result);
}

KJ_TEST("MarketMakingStrategy: Metrics tracking") {
  auto config = create_default_config();
  MarketMakingStrategy strategy(config);
  strategy.on_start();

  // Process some events
  for (int i = 0; i < 5; ++i) {
    auto event = create_ticker_event(50000.0 + i * 10, 50010.0 + i * 10);
    strategy.on_event(event);
  }

  auto maybeMetrics = strategy.get_metrics();
  KJ_IF_SOME(metrics, maybeMetrics) {
    KJ_EXPECT(metrics.events_processed.load() == 5);
    KJ_EXPECT(metrics.signals_generated.load() > 0);
  }
  else {
    KJ_FAIL_EXPECT("Metrics should be available");
  }
}

KJ_TEST("MarketMakingStrategy: No quotes when not running") {
  auto config = create_default_config();
  MarketMakingStrategy strategy(config);
  // Don't call on_start()

  auto event = create_ticker_event(50000.0, 50010.0);
  strategy.on_event(event);

  auto signals = strategy.get_signals();
  KJ_EXPECT(signals.size() == 0);
}

KJ_TEST("MarketMakingStrategy: State tracking") {
  auto config = create_default_config();
  MarketMakingStrategy strategy(config);
  strategy.on_start();

  auto state = strategy.get_state();
  KJ_EXPECT(state.is_running);
  KJ_EXPECT(state.strategy_name == "TestMarketMaker"_kj);

  strategy.on_stop();
  state = strategy.get_state();
  KJ_EXPECT(!state.is_running);
}

KJ_TEST("MarketMakingStrategy: Factory creates strategy") {
  MarketMakingStrategyFactory factory;

  KJ_EXPECT(factory.get_strategy_type() == "MarketMakingStrategy"_kj);

  auto config = create_default_config();
  auto strategy = factory.create_strategy(config);

  KJ_EXPECT(strategy->get_type() == StrategyType::MarketMaking);
}

KJ_TEST("MarketMakingStrategy: Trade event updates price") {
  auto config = create_default_config();
  MarketMakingStrategy strategy(config);
  strategy.on_start();

  auto event = create_trade_event(50000.0, 1.0, false);
  strategy.on_event(event);

  // Mid price should be set from trade when no ticker available
  KJ_EXPECT(strategy.get_mid_price() == 50000.0);
}

} // namespace
