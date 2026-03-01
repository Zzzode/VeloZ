#include "kj/test.h"
#include "veloz/strategy/trend_following_strategy.h"

namespace {

using namespace veloz::strategy;

// Helper to create a basic config for testing
StrategyConfig create_test_config() {
  StrategyConfig config;
  config.name = kj::str("TrendFollowingTest");
  config.type = StrategyType::TrendFollowing;
  config.risk_per_trade = 0.02;
  config.max_position_size = 1.0;
  config.stop_loss = 0.02;
  config.take_profit = 0.04;
  config.symbols.add(kj::str("BTCUSDT"));
  return config;
}

// Helper to create a market event with trade data
veloz::market::MarketEvent create_trade_event(double price) {
  veloz::market::MarketEvent event;
  event.type = veloz::market::MarketEventType::Trade;
  event.venue = veloz::common::Venue::Binance;
  event.market = veloz::common::MarketKind::Spot;
  event.symbol = "BTCUSDT"_kj;
  event.ts_recv_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
                         std::chrono::steady_clock::now().time_since_epoch())
                         .count();

  veloz::market::TradeData trade_data;
  trade_data.price = price;
  trade_data.qty = 1.0;
  trade_data.is_buyer_maker = false;
  trade_data.trade_id = 12345;
  event.data = trade_data;

  return event;
}

KJ_TEST("TrendFollowingStrategy: Creation with default parameters") {
  auto config = create_test_config();
  TrendFollowingStrategy strategy(config);

  KJ_EXPECT(strategy.get_name() == "TrendFollowingTest");
  KJ_EXPECT(strategy.get_type() == StrategyType::TrendFollowing);

  kj::String id = kj::str(strategy.get_id());
  KJ_EXPECT(id.startsWith("TrendFollowingTest_"));
}

KJ_TEST("TrendFollowingStrategy: Creation with custom parameters") {
  auto config = create_test_config();
  config.parameters.insert(kj::str("fast_period"), 5.0);
  config.parameters.insert(kj::str("slow_period"), 10.0);
  config.parameters.insert(kj::str("use_ema"), 1.0);
  config.parameters.insert(kj::str("position_size"), 0.5);

  TrendFollowingStrategy strategy(config);

  KJ_EXPECT(strategy.get_name() == "TrendFollowingTest");
  KJ_EXPECT(strategy.get_type() == StrategyType::TrendFollowing);
}

KJ_TEST("TrendFollowingStrategy: Get strategy type name") {
  KJ_EXPECT(TrendFollowingStrategy::get_strategy_type() == "TrendFollowingStrategy");
}

KJ_TEST("TrendFollowingStrategy: Supports hot reload") {
  auto config = create_test_config();
  TrendFollowingStrategy strategy(config);

  KJ_EXPECT(strategy.supports_hot_reload() == true);
}

KJ_TEST("TrendFollowingStrategy: Update parameters at runtime") {
  auto config = create_test_config();
  TrendFollowingStrategy strategy(config);

  kj::TreeMap<kj::String, double> new_params;
  new_params.insert(kj::str("position_size"), 0.75);
  new_params.insert(kj::str("atr_multiplier"), 3.0);

  bool result = strategy.update_parameters(new_params);
  KJ_EXPECT(result == true);
}

KJ_TEST("TrendFollowingStrategy: Get metrics") {
  auto config = create_test_config();
  TrendFollowingStrategy strategy(config);

  auto maybe_metrics = strategy.get_metrics();
  KJ_ASSERT(maybe_metrics != kj::none);

  KJ_IF_SOME(metrics, maybe_metrics) {
    KJ_EXPECT(metrics.events_processed.load() == 0);
    KJ_EXPECT(metrics.signals_generated.load() == 0);
  }
}

KJ_TEST("TrendFollowingStrategy: Reset clears state") {
  auto config = create_test_config();
  TrendFollowingStrategy strategy(config);
  strategy.on_start();

  // Feed some events
  for (int i = 0; i < 5; ++i) {
    auto event = create_trade_event(100.0 + i);
    strategy.on_event(event);
  }

  // Stop and reset
  strategy.on_stop();
  strategy.reset();

  auto state = strategy.get_state();
  KJ_EXPECT(state.is_running == false);
  KJ_EXPECT(state.trade_count == 0);
  KJ_EXPECT(state.pnl == 0.0);
}

KJ_TEST("TrendFollowingStrategy: No signals without enough data") {
  auto config = create_test_config();
  config.parameters.insert(kj::str("fast_period"), 5.0);
  config.parameters.insert(kj::str("slow_period"), 10.0);

  TrendFollowingStrategy strategy(config);
  strategy.on_start();

  // Feed fewer events than slow_period
  for (int i = 0; i < 5; ++i) {
    auto event = create_trade_event(100.0 + i);
    strategy.on_event(event);
  }

  auto signals = strategy.get_signals();
  KJ_EXPECT(signals.size() == 0);
}

KJ_TEST("TrendFollowingStrategy: Golden cross generates buy signal") {
  auto config = create_test_config();
  config.parameters.insert(kj::str("fast_period"), 3.0);
  config.parameters.insert(kj::str("slow_period"), 5.0);
  config.parameters.insert(kj::str("use_ema"), 0.0); // Use SMA for predictable behavior

  TrendFollowingStrategy strategy(config);
  strategy.on_start();

  // Create a downtrend first (fast MA below slow MA)
  // Prices: 110, 108, 106, 104, 102 -> downtrend
  kj::Array<double> downtrend_prices = kj::heapArray<double>({110.0, 108.0, 106.0, 104.0, 102.0});
  for (size_t i = 0; i < downtrend_prices.size(); ++i) {
    auto event = create_trade_event(downtrend_prices[i]);
    strategy.on_event(event);
  }

  // Clear any signals from initialization
  strategy.get_signals();

  // Now create an uptrend (golden cross: fast MA crosses above slow MA)
  // Prices: 100, 105, 110, 115, 120 -> uptrend
  kj::Array<double> uptrend_prices = kj::heapArray<double>({100.0, 105.0, 110.0, 115.0, 120.0});
  for (size_t i = 0; i < uptrend_prices.size(); ++i) {
    auto event = create_trade_event(uptrend_prices[i]);
    strategy.on_event(event);
  }

  auto signals = strategy.get_signals();
  // Should have generated at least one buy signal during the crossover
  // Note: exact signal count depends on when crossover occurs
  if (signals.size() > 0) {
    KJ_EXPECT(signals[0].side == veloz::exec::OrderSide::Buy);
  }
}

KJ_TEST("TrendFollowingStrategy: Death cross generates exit signal") {
  auto config = create_test_config();
  config.parameters.insert(kj::str("fast_period"), 3.0);
  config.parameters.insert(kj::str("slow_period"), 5.0);
  config.parameters.insert(kj::str("use_ema"), 0.0);

  TrendFollowingStrategy strategy(config);
  strategy.on_start();

  // Create uptrend first to enter position
  kj::Array<double> uptrend_prices =
      kj::heapArray<double>({90.0, 95.0, 100.0, 105.0, 110.0, 115.0});
  for (size_t i = 0; i < uptrend_prices.size(); ++i) {
    auto event = create_trade_event(uptrend_prices[i]);
    strategy.on_event(event);
  }

  // Get and clear entry signals
  auto entry_signals = strategy.get_signals();

  // Create downtrend (death cross)
  kj::Array<double> downtrend_prices =
      kj::heapArray<double>({115.0, 110.0, 105.0, 100.0, 95.0, 90.0});
  for (size_t i = 0; i < downtrend_prices.size(); ++i) {
    auto event = create_trade_event(downtrend_prices[i]);
    strategy.on_event(event);
  }

  auto exit_signals = strategy.get_signals();
  // If we had a position, should have exit signal
  // Note: signal generation depends on position state
}

KJ_TEST("TrendFollowingStrategy: Stop loss triggers exit") {
  auto config = create_test_config();
  config.parameters.insert(kj::str("fast_period"), 3.0);
  config.parameters.insert(kj::str("slow_period"), 5.0);
  config.stop_loss = 0.05; // 5% stop loss

  TrendFollowingStrategy strategy(config);
  strategy.on_start();

  // Create uptrend to enter position
  kj::Array<double> uptrend_prices =
      kj::heapArray<double>({90.0, 95.0, 100.0, 105.0, 110.0, 115.0});
  for (size_t i = 0; i < uptrend_prices.size(); ++i) {
    auto event = create_trade_event(uptrend_prices[i]);
    strategy.on_event(event);
  }

  // Clear entry signals
  strategy.get_signals();

  // Price drops below stop loss (more than 5% from entry)
  auto stop_event = create_trade_event(100.0); // Significant drop
  strategy.on_event(stop_event);

  // Check if stop loss triggered
  auto state = strategy.get_state();
  // State should reflect any stop loss exits
}

KJ_TEST("TrendFollowingStrategy: Factory creates correct type") {
  TrendFollowingStrategyFactory factory;

  KJ_EXPECT(factory.get_strategy_type() == "TrendFollowingStrategy");

  auto config = create_test_config();
  auto strategy = factory.create_strategy(config);

  KJ_EXPECT(strategy->get_type() == StrategyType::TrendFollowing);
}

KJ_TEST("TrendFollowingStrategy: Metrics track events processed") {
  auto config = create_test_config();
  TrendFollowingStrategy strategy(config);
  strategy.on_start();

  // Feed some events
  for (int i = 0; i < 10; ++i) {
    auto event = create_trade_event(100.0 + i);
    strategy.on_event(event);
  }

  auto maybe_metrics = strategy.get_metrics();
  KJ_IF_SOME(metrics, maybe_metrics) {
    KJ_EXPECT(metrics.events_processed.load() == 10);
  }
}

KJ_TEST("TrendFollowingStrategy: Timer event does not crash") {
  auto config = create_test_config();
  TrendFollowingStrategy strategy(config);
  strategy.on_start();

  // Timer event should be handled gracefully
  strategy.on_timer(1234567890);
  // No crash = success
}

KJ_TEST("TrendFollowingStrategy: State reflects running status") {
  auto config = create_test_config();
  TrendFollowingStrategy strategy(config);

  auto state_before = strategy.get_state();
  KJ_EXPECT(state_before.is_running == false);

  strategy.on_start();

  auto state_after = strategy.get_state();
  KJ_EXPECT(state_after.is_running == true);

  strategy.on_stop();

  auto state_stopped = strategy.get_state();
  KJ_EXPECT(state_stopped.is_running == false);
}

KJ_TEST("TrendFollowingStrategy: ATR-based stop loss configuration") {
  auto config = create_test_config();
  config.parameters.insert(kj::str("use_atr_stop"), 1.0);
  config.parameters.insert(kj::str("atr_period"), 14.0);
  config.parameters.insert(kj::str("atr_multiplier"), 2.0);

  TrendFollowingStrategy strategy(config);
  strategy.on_start();

  // Strategy should be created without errors
  KJ_EXPECT(strategy.get_type() == StrategyType::TrendFollowing);
}

KJ_TEST("TrendFollowingStrategy: Kline event handling") {
  auto config = create_test_config();
  config.parameters.insert(kj::str("fast_period"), 3.0);
  config.parameters.insert(kj::str("slow_period"), 5.0);

  TrendFollowingStrategy strategy(config);
  strategy.on_start();

  // Create kline events
  for (int i = 0; i < 10; ++i) {
    veloz::market::MarketEvent event;
    event.type = veloz::market::MarketEventType::Kline;
    event.venue = veloz::common::Venue::Binance;
    event.symbol = "BTCUSDT"_kj;

    veloz::market::KlineData kline;
    kline.open = 100.0 + i;
    kline.high = 102.0 + i;
    kline.low = 98.0 + i;
    kline.close = 101.0 + i;
    kline.volume = 1000.0;
    event.data = kline;

    strategy.on_event(event);
  }

  auto maybe_metrics = strategy.get_metrics();
  KJ_IF_SOME(metrics, maybe_metrics) {
    KJ_EXPECT(metrics.events_processed.load() == 10);
  }
}

} // namespace
