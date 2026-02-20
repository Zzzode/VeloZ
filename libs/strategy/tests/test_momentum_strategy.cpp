#include "kj/test.h"
#include "veloz/strategy/momentum_strategy.h"

namespace {

using namespace veloz::strategy;
using namespace veloz::market;
using namespace veloz::exec;
using namespace veloz::common;

// Helper to create a default config
StrategyConfig create_default_config() {
  StrategyConfig config;
  config.name = kj::heapString("TestMomentum");
  config.type = StrategyType::Momentum;
  config.risk_per_trade = 0.01;
  config.max_position_size = 10.0;
  config.stop_loss = 0.05;
  config.take_profit = 0.10;
  config.symbols.add(kj::heapString("BTCUSDT"));
  return config;
}

// Helper to create a trade event
MarketEvent create_trade_event(double price) {
  MarketEvent event;
  event.type = MarketEventType::Trade;
  event.symbol = SymbolId("BTCUSDT"_kj);
  event.ts_exchange_ns = 1000000000;
  event.ts_recv_ns = 1000000001;

  veloz::market::TradeData trade;
  trade.price = price;
  trade.qty = 1.0;
  trade.is_buyer_maker = false;
  event.data = trade;

  return event;
}

// Helper to create a kline event
MarketEvent create_kline_event(double close) {
  MarketEvent event;
  event.type = MarketEventType::Kline;
  event.symbol = SymbolId("BTCUSDT"_kj);
  event.ts_exchange_ns = 1000000000;
  event.ts_recv_ns = 1000000001;

  KlineData kline;
  kline.open = close * 0.99;
  kline.high = close * 1.01;
  kline.low = close * 0.98;
  kline.close = close;
  kline.volume = 100.0;
  event.data = kline;

  return event;
}

KJ_TEST("MomentumStrategy: Basic construction") {
  auto config = create_default_config();
  MomentumStrategy strategy(config);

  KJ_EXPECT(strategy.get_type() == StrategyType::Momentum);
  KJ_EXPECT(strategy.get_name() == "TestMomentum"_kj);
  KJ_EXPECT(strategy.get_current_roc() == 0.0);
  KJ_EXPECT(strategy.get_current_rsi() == 50.0);
  KJ_EXPECT(!strategy.is_in_position());
}

KJ_TEST("MomentumStrategy: Get strategy type") {
  KJ_EXPECT(MomentumStrategy::get_strategy_type() == "MomentumStrategy"_kj);
}

KJ_TEST("MomentumStrategy: Price updates from trade events") {
  auto config = create_default_config();
  MomentumStrategy strategy(config);
  strategy.on_start();

  auto event = create_trade_event(50000.0);
  strategy.on_event(event);

  KJ_EXPECT(strategy.get_last_price() == 50000.0);
}

KJ_TEST("MomentumStrategy: Price updates from kline events") {
  auto config = create_default_config();
  MomentumStrategy strategy(config);
  strategy.on_start();

  auto event = create_kline_event(50000.0);
  strategy.on_event(event);

  KJ_EXPECT(strategy.get_last_price() == 50000.0);
}

KJ_TEST("MomentumStrategy: ROC calculation after enough data") {
  auto config = create_default_config();
  // Use default period (14) to avoid parameter parsing issues
  // Need 16 prices for ROC calculation (period + 1 = 15, plus buffer)

  MomentumStrategy strategy(config);
  strategy.on_start();

  // Add 20 prices with upward trend
  for (int i = 0; i < 20; ++i) {
    auto event = create_trade_event(100.0 + i * 2.0); // 100, 102, 104, ..., 138
    strategy.on_event(event);
  }

  // ROC should be positive with upward trend
  double roc = strategy.get_current_roc();
  KJ_EXPECT(roc > 0.0); // Positive momentum
}

KJ_TEST("MomentumStrategy: RSI calculation") {
  auto config = create_default_config();
  // Use default period (14) to avoid parameter parsing issues

  MomentumStrategy strategy(config);
  strategy.on_start();

  // Add 20 prices with upward trend
  for (int i = 0; i < 20; ++i) {
    auto event = create_trade_event(100.0 + i * 2.0); // All gains
    strategy.on_event(event);
  }

  // RSI should be high (all gains, no losses)
  double rsi = strategy.get_current_rsi();
  KJ_EXPECT(rsi > 50.0); // Above neutral
}

KJ_TEST("MomentumStrategy: No signals without enough data") {
  auto config = create_default_config();
  config.parameters.insert(kj::str("roc_period"), 14.0);

  MomentumStrategy strategy(config);
  strategy.on_start();

  // Add only 5 prices (not enough for 14-period ROC)
  for (int i = 0; i < 5; ++i) {
    auto event = create_trade_event(50000.0 + i * 100);
    strategy.on_event(event);
  }

  auto signals = strategy.get_signals();
  KJ_EXPECT(signals.size() == 0);
}

KJ_TEST("MomentumStrategy: Buy signal on positive momentum") {
  auto config = create_default_config();
  // Disable RSI filter to test pure momentum signal
  // (With all-up prices, RSI would be overbought and block the signal)
  config.parameters.insert(kj::str("use_rsi_filter"), 0.0);

  MomentumStrategy strategy(config);
  strategy.on_start();

  // Add 20 prices with strong upward momentum (>2%)
  // Starting at 100, ending at 157 = 57% gain over 14 periods
  bool has_buy = false;
  for (int i = 0; i < 20; ++i) {
    auto event = create_trade_event(100.0 + i * 3.0); // 100, 103, 106, ..., 157
    strategy.on_event(event);

    // Check signals after each event (signals are cleared on get_signals())
    auto signals = strategy.get_signals();
    for (const auto& signal : signals) {
      if (signal.side == OrderSide::Buy) {
        has_buy = true;
      }
    }
  }

  KJ_EXPECT(has_buy);
}

KJ_TEST("MomentumStrategy: Reset clears state") {
  auto config = create_default_config();
  MomentumStrategy strategy(config);
  strategy.on_start();

  // Add some data
  for (int i = 0; i < 20; ++i) {
    auto event = create_trade_event(50000.0 + i * 100);
    strategy.on_event(event);
  }

  KJ_EXPECT(strategy.get_last_price() > 0);

  strategy.reset();

  KJ_EXPECT(strategy.get_last_price() == 0.0);
  KJ_EXPECT(strategy.get_current_roc() == 0.0);
  KJ_EXPECT(strategy.get_current_rsi() == 50.0);
  KJ_EXPECT(!strategy.is_in_position());
}

KJ_TEST("MomentumStrategy: Hot reload parameters") {
  auto config = create_default_config();
  MomentumStrategy strategy(config);

  KJ_EXPECT(strategy.supports_hot_reload());

  kj::TreeMap<kj::String, double> new_params;
  new_params.insert(kj::str("roc_period"), 20.0);
  new_params.insert(kj::str("rsi_period"), 20.0);
  new_params.insert(kj::str("momentum_threshold"), 0.03);

  bool result = strategy.update_parameters(new_params);
  KJ_EXPECT(result);
}

KJ_TEST("MomentumStrategy: Metrics tracking") {
  auto config = create_default_config();
  MomentumStrategy strategy(config);
  strategy.on_start();

  // Process some events
  for (int i = 0; i < 10; ++i) {
    auto event = create_trade_event(50000.0 + i * 100);
    strategy.on_event(event);
  }

  auto maybeMetrics = strategy.get_metrics();
  KJ_IF_SOME(metrics, maybeMetrics) {
    KJ_EXPECT(metrics.events_processed.load() == 10);
  }
  else {
    KJ_FAIL_EXPECT("Metrics should be available");
  }
}

KJ_TEST("MomentumStrategy: No signals when not running") {
  auto config = create_default_config();
  MomentumStrategy strategy(config);
  // Don't call on_start()

  for (int i = 0; i < 20; ++i) {
    auto event = create_trade_event(50000.0 + i * 500);
    strategy.on_event(event);
  }

  auto signals = strategy.get_signals();
  KJ_EXPECT(signals.size() == 0);
}

KJ_TEST("MomentumStrategy: State tracking") {
  auto config = create_default_config();
  MomentumStrategy strategy(config);
  strategy.on_start();

  auto state = strategy.get_state();
  KJ_EXPECT(state.is_running);
  KJ_EXPECT(state.strategy_name == "TestMomentum"_kj);

  strategy.on_stop();
  state = strategy.get_state();
  KJ_EXPECT(!state.is_running);
}

KJ_TEST("MomentumStrategy: Factory creates strategy") {
  MomentumStrategyFactory factory;

  KJ_EXPECT(factory.get_strategy_type() == "MomentumStrategy"_kj);

  auto config = create_default_config();
  auto strategy = factory.create_strategy(config);

  KJ_EXPECT(strategy->get_type() == StrategyType::Momentum);
}

KJ_TEST("MomentumStrategy: Timer event does not crash") {
  auto config = create_default_config();
  MomentumStrategy strategy(config);
  strategy.on_start();

  // Timer should not crash
  strategy.on_timer(1000);
  strategy.on_timer(2000);

  // Strategy should still be running
  auto state = strategy.get_state();
  KJ_EXPECT(state.is_running);
}

} // namespace
