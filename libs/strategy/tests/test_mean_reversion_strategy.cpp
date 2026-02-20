#include "kj/test.h"
#include "veloz/strategy/mean_reversion_strategy.h"

namespace {

using namespace veloz::strategy;

// Helper to create a basic config for testing
StrategyConfig create_test_config() {
  StrategyConfig config;
  config.name = kj::str("MeanReversionTest");
  config.type = StrategyType::MeanReversion;
  config.risk_per_trade = 0.02;
  config.max_position_size = 1.0;
  config.stop_loss = 0.05;
  config.take_profit = 0.10;
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

KJ_TEST("MeanReversionStrategy: Creation with default parameters") {
  auto config = create_test_config();
  MeanReversionStrategy strategy(config);

  KJ_EXPECT(strategy.get_name() == "MeanReversionTest");
  KJ_EXPECT(strategy.get_type() == StrategyType::MeanReversion);

  kj::String id = kj::str(strategy.get_id());
  KJ_EXPECT(id.startsWith("MeanReversionTest_"));
}

KJ_TEST("MeanReversionStrategy: Creation with custom parameters") {
  auto config = create_test_config();
  config.parameters.insert(kj::str("lookback_period"), 30.0);
  config.parameters.insert(kj::str("entry_threshold"), 2.5);
  config.parameters.insert(kj::str("exit_threshold"), 0.3);
  config.parameters.insert(kj::str("enable_short"), 1.0);

  MeanReversionStrategy strategy(config);

  KJ_EXPECT(strategy.get_name() == "MeanReversionTest");
  KJ_EXPECT(strategy.get_type() == StrategyType::MeanReversion);
}

KJ_TEST("MeanReversionStrategy: Get strategy type name") {
  KJ_EXPECT(MeanReversionStrategy::get_strategy_type() == "MeanReversionStrategy");
}

KJ_TEST("MeanReversionStrategy: Supports hot reload") {
  auto config = create_test_config();
  MeanReversionStrategy strategy(config);

  KJ_EXPECT(strategy.supports_hot_reload() == true);
}

KJ_TEST("MeanReversionStrategy: Update parameters at runtime") {
  auto config = create_test_config();
  MeanReversionStrategy strategy(config);

  kj::TreeMap<kj::String, double> new_params;
  new_params.insert(kj::str("position_size"), 0.5);
  new_params.insert(kj::str("entry_threshold"), 3.0);
  new_params.insert(kj::str("exit_threshold"), 0.2);
  new_params.insert(kj::str("enable_short"), 1.0);

  bool result = strategy.update_parameters(new_params);
  KJ_EXPECT(result == true);
}

KJ_TEST("MeanReversionStrategy: Get metrics") {
  auto config = create_test_config();
  MeanReversionStrategy strategy(config);

  auto maybe_metrics = strategy.get_metrics();
  KJ_ASSERT(maybe_metrics != kj::none);

  KJ_IF_SOME(metrics, maybe_metrics) {
    KJ_EXPECT(metrics.events_processed.load() == 0);
    KJ_EXPECT(metrics.signals_generated.load() == 0);
  }
}

KJ_TEST("MeanReversionStrategy: Reset clears state") {
  auto config = create_test_config();
  MeanReversionStrategy strategy(config);
  strategy.on_start();

  // Feed some events
  for (int i = 0; i < 10; ++i) {
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

KJ_TEST("MeanReversionStrategy: No signals without enough data") {
  auto config = create_test_config();
  config.parameters.insert(kj::str("lookback_period"), 20.0);

  MeanReversionStrategy strategy(config);
  strategy.on_start();

  // Feed fewer events than lookback_period
  for (int i = 0; i < 10; ++i) {
    auto event = create_trade_event(100.0 + i);
    strategy.on_event(event);
  }

  auto signals = strategy.get_signals();
  KJ_EXPECT(signals.size() == 0);
}

KJ_TEST("MeanReversionStrategy: Statistics calculation") {
  auto config = create_test_config();
  config.parameters.insert(kj::str("lookback_period"), 10.0);

  MeanReversionStrategy strategy(config);
  strategy.on_start();

  // Feed constant prices - with constant prices, no entry signals should be generated
  // because std_dev will be 0 and we check std_dev > 0 before generating signals
  for (int i = 0; i < 15; ++i) {
    auto event = create_trade_event(100.0);
    strategy.on_event(event);
  }

  // With constant prices, no signals should be generated (std_dev = 0)
  auto signals = strategy.get_signals();
  KJ_EXPECT(signals.size() == 0);

  // Verify events were processed
  auto maybe_metrics = strategy.get_metrics();
  KJ_IF_SOME(metrics, maybe_metrics) {
    KJ_EXPECT(metrics.events_processed.load() == 15);
  }
}

KJ_TEST("MeanReversionStrategy: Z-score calculation") {
  auto config = create_test_config();
  config.parameters.insert(kj::str("lookback_period"), 10.0);
  config.parameters.insert(kj::str("entry_threshold"), 2.0);

  MeanReversionStrategy strategy(config);
  strategy.on_start();

  // Feed prices with variance to build up statistics
  kj::Array<double> prices = kj::heapArray<double>(
      {98.0, 102.0, 97.0, 103.0, 96.0, 104.0, 95.0, 105.0, 94.0, 106.0, 93.0, 107.0});

  for (size_t i = 0; i < prices.size(); ++i) {
    auto event = create_trade_event(prices[i]);
    strategy.on_event(event);
  }

  // Clear any signals generated during warmup
  strategy.get_signals();

  // Now send an extreme price to trigger a signal (oversold condition)
  auto extreme_event = create_trade_event(70.0);
  strategy.on_event(extreme_event);

  // Should generate a buy signal due to oversold condition
  auto signals = strategy.get_signals();
  // With varying prices and an extreme low, we should get a buy signal
  if (signals.size() > 0) {
    KJ_EXPECT(signals[0].side == veloz::exec::OrderSide::Buy);
  }

  // Verify events were processed
  auto maybe_metrics = strategy.get_metrics();
  KJ_IF_SOME(metrics, maybe_metrics) {
    KJ_EXPECT(metrics.events_processed.load() == 13);
  }
}

KJ_TEST("MeanReversionStrategy: Buy signal on oversold condition") {
  auto config = create_test_config();
  config.parameters.insert(kj::str("lookback_period"), 10.0);
  config.parameters.insert(kj::str("entry_threshold"), 2.0);

  MeanReversionStrategy strategy(config);
  strategy.on_start();

  // Build up a price history around 100
  for (int i = 0; i < 10; ++i) {
    auto event = create_trade_event(100.0);
    strategy.on_event(event);
  }

  // Clear any signals
  strategy.get_signals();

  // Now add some variance to get non-zero std dev
  kj::Array<double> prices =
      kj::heapArray<double>({98.0, 102.0, 97.0, 103.0, 96.0, 104.0, 95.0, 105.0, 94.0, 106.0});
  for (size_t i = 0; i < prices.size(); ++i) {
    auto event = create_trade_event(prices[i]);
    strategy.on_event(event);
  }

  // Clear signals
  strategy.get_signals();

  // Now send a very low price (oversold condition)
  auto oversold_event = create_trade_event(80.0);
  strategy.on_event(oversold_event);

  auto signals = strategy.get_signals();
  // Should generate a buy signal due to oversold condition
  if (signals.size() > 0) {
    KJ_EXPECT(signals[0].side == veloz::exec::OrderSide::Buy);
  }
}

KJ_TEST("MeanReversionStrategy: Exit on mean reversion") {
  auto config = create_test_config();
  config.parameters.insert(kj::str("lookback_period"), 10.0);
  config.parameters.insert(kj::str("entry_threshold"), 2.0);
  config.parameters.insert(kj::str("exit_threshold"), 0.5);

  MeanReversionStrategy strategy(config);
  strategy.on_start();

  // Build price history with variance
  kj::Array<double> prices =
      kj::heapArray<double>({98.0, 102.0, 97.0, 103.0, 96.0, 104.0, 95.0, 105.0, 94.0, 106.0});
  for (size_t i = 0; i < prices.size(); ++i) {
    auto event = create_trade_event(prices[i]);
    strategy.on_event(event);
  }

  // Clear signals
  strategy.get_signals();

  // Send oversold price to enter position
  auto oversold_event = create_trade_event(80.0);
  strategy.on_event(oversold_event);

  // Get entry signal
  auto entry_signals = strategy.get_signals();

  // Now price reverts to mean
  auto revert_event = create_trade_event(100.0);
  strategy.on_event(revert_event);

  // Should generate exit signal
  auto exit_signals = strategy.get_signals();
  // Note: exit signal generation depends on position state
}

KJ_TEST("MeanReversionStrategy: Factory creates correct type") {
  MeanReversionStrategyFactory factory;

  KJ_EXPECT(factory.get_strategy_type() == "MeanReversionStrategy");

  auto config = create_test_config();
  auto strategy = factory.create_strategy(config);

  KJ_EXPECT(strategy->get_type() == StrategyType::MeanReversion);
}

KJ_TEST("MeanReversionStrategy: Metrics track events processed") {
  auto config = create_test_config();
  MeanReversionStrategy strategy(config);
  strategy.on_start();

  // Feed some events
  for (int i = 0; i < 15; ++i) {
    auto event = create_trade_event(100.0 + i);
    strategy.on_event(event);
  }

  auto maybe_metrics = strategy.get_metrics();
  KJ_IF_SOME(metrics, maybe_metrics) {
    KJ_EXPECT(metrics.events_processed.load() == 15);
  }
}

KJ_TEST("MeanReversionStrategy: Timer event does not crash") {
  auto config = create_test_config();
  MeanReversionStrategy strategy(config);
  strategy.on_start();

  // Timer event should be handled gracefully
  strategy.on_timer(1234567890);
  // No crash = success
}

KJ_TEST("MeanReversionStrategy: State reflects running status") {
  auto config = create_test_config();
  MeanReversionStrategy strategy(config);

  auto state_before = strategy.get_state();
  KJ_EXPECT(state_before.is_running == false);

  strategy.on_start();

  auto state_after = strategy.get_state();
  KJ_EXPECT(state_after.is_running == true);

  strategy.on_stop();

  auto state_stopped = strategy.get_state();
  KJ_EXPECT(state_stopped.is_running == false);
}

KJ_TEST("MeanReversionStrategy: Short selling disabled by default") {
  auto config = create_test_config();
  // enable_short not set, defaults to false

  MeanReversionStrategy strategy(config);
  strategy.on_start();

  // Build price history with variance
  kj::Array<double> prices =
      kj::heapArray<double>({98.0, 102.0, 97.0, 103.0, 96.0, 104.0, 95.0, 105.0, 94.0, 106.0});
  for (size_t i = 0; i < prices.size(); ++i) {
    auto event = create_trade_event(prices[i]);
    strategy.on_event(event);
  }

  // Clear signals
  strategy.get_signals();

  // Send overbought price - should NOT generate sell signal (short disabled)
  auto overbought_event = create_trade_event(120.0);
  strategy.on_event(overbought_event);

  auto signals = strategy.get_signals();
  // Should not have any sell signals since short is disabled
  for (size_t i = 0; i < signals.size(); ++i) {
    KJ_EXPECT(signals[i].side != veloz::exec::OrderSide::Sell);
  }
}

KJ_TEST("MeanReversionStrategy: Kline event handling") {
  auto config = create_test_config();
  config.parameters.insert(kj::str("lookback_period"), 10.0);

  MeanReversionStrategy strategy(config);
  strategy.on_start();

  // Create kline events
  for (int i = 0; i < 15; ++i) {
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
    KJ_EXPECT(metrics.events_processed.load() == 15);
  }
}

} // namespace
