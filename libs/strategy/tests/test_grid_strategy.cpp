#include "kj/test.h"
#include "veloz/strategy/grid_strategy.h"

namespace {

using namespace veloz::strategy;

// Helper to create a basic config for testing
StrategyConfig create_test_config() {
  StrategyConfig config;
  config.name = kj::str("GridTest");
  config.type = StrategyType::Grid;
  config.risk_per_trade = 0.02;
  config.max_position_size = 10.0;
  config.stop_loss = 0.1;
  config.take_profit = 0.2;
  config.symbols.add(kj::str("BTCUSDT"));

  // Grid-specific parameters
  config.parameters.insert(kj::str("upper_price"), 55000.0);
  config.parameters.insert(kj::str("lower_price"), 45000.0);
  config.parameters.insert(kj::str("grid_count"), 10.0);
  config.parameters.insert(kj::str("total_investment"), 10000.0);
  config.parameters.insert(kj::str("grid_mode"), 0.0); // Arithmetic

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

// Helper to create a book event
veloz::market::MarketEvent create_book_event(double bid, double ask) {
  veloz::market::MarketEvent event;
  event.type = veloz::market::MarketEventType::BookTop;
  event.venue = veloz::common::Venue::Binance;
  event.market = veloz::common::MarketKind::Spot;
  event.symbol = "BTCUSDT"_kj;
  event.ts_recv_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
                         std::chrono::steady_clock::now().time_since_epoch())
                         .count();

  veloz::market::BookData book_data;
  veloz::market::BookLevel bid_level;
  bid_level.price = bid;
  bid_level.qty = 1.0;
  book_data.bids.add(bid_level);

  veloz::market::BookLevel ask_level;
  ask_level.price = ask;
  ask_level.qty = 1.0;
  book_data.asks.add(ask_level);

  event.data = kj::mv(book_data);

  return event;
}

KJ_TEST("GridStrategy: Creation with default parameters") {
  auto config = create_test_config();
  GridStrategy strategy(config);

  KJ_EXPECT(strategy.get_name() == "GridTest");
  KJ_EXPECT(strategy.get_type() == StrategyType::Grid);
  KJ_EXPECT(strategy.get_upper_price() == 55000.0);
  KJ_EXPECT(strategy.get_lower_price() == 45000.0);
  KJ_EXPECT(strategy.get_grid_count() == 10);
}

KJ_TEST("GridStrategy: Get strategy type name") {
  KJ_EXPECT(GridStrategy::get_strategy_type() == "GridStrategy");
}

KJ_TEST("GridStrategy: Supports hot reload") {
  auto config = create_test_config();
  GridStrategy strategy(config);

  KJ_EXPECT(strategy.supports_hot_reload() == true);
}

KJ_TEST("GridStrategy: Update parameters at runtime") {
  auto config = create_test_config();
  GridStrategy strategy(config);

  kj::TreeMap<kj::String, double> new_params;
  new_params.insert(kj::str("take_profit_pct"), 0.15);
  new_params.insert(kj::str("stop_loss_pct"), 0.05);

  bool result = strategy.update_parameters(new_params);
  KJ_EXPECT(result == true);
}

KJ_TEST("GridStrategy: Get metrics") {
  auto config = create_test_config();
  GridStrategy strategy(config);

  auto maybe_metrics = strategy.get_metrics();
  KJ_ASSERT(maybe_metrics != kj::none);

  KJ_IF_SOME(metrics, maybe_metrics) {
    KJ_EXPECT(metrics.events_processed.load() == 0);
    KJ_EXPECT(metrics.signals_generated.load() == 0);
  }
}

KJ_TEST("GridStrategy: Initialize grid on first event") {
  auto config = create_test_config();
  GridStrategy strategy(config);
  strategy.on_start();

  KJ_EXPECT(strategy.is_grid_initialized() == false);

  // Send a trade event to initialize the grid
  auto event = create_trade_event(50000.0);
  strategy.on_event(event);

  KJ_EXPECT(strategy.is_grid_initialized() == true);
  KJ_EXPECT(strategy.get_current_price() == 50000.0);
}

KJ_TEST("GridStrategy: Grid levels calculated correctly (arithmetic)") {
  auto config = create_test_config();
  GridStrategy strategy(config);
  strategy.on_start();

  auto event = create_trade_event(50000.0);
  strategy.on_event(event);

  const auto& levels = strategy.get_grid_levels();
  KJ_EXPECT(levels.size() == 10);

  // Check first and last levels
  // Arithmetic spacing: (55000 - 45000) / 9 = 1111.11
  KJ_EXPECT(levels[0].price >= 44999.0 && levels[0].price <= 45001.0);
  KJ_EXPECT(levels[9].price >= 54999.0 && levels[9].price <= 55001.0);

  // Check spacing is roughly equal
  double spacing = strategy.get_grid_spacing();
  KJ_EXPECT(spacing > 1000.0 && spacing < 1200.0);
}

KJ_TEST("GridStrategy: Grid levels calculated correctly (geometric)") {
  StrategyConfig config;
  config.name = kj::str("GridTestGeometric");
  config.type = StrategyType::Grid;
  config.risk_per_trade = 0.02;
  config.max_position_size = 10.0;
  config.symbols.add(kj::str("BTCUSDT"));
  config.parameters.insert(kj::str("upper_price"), 55000.0);
  config.parameters.insert(kj::str("lower_price"), 45000.0);
  config.parameters.insert(kj::str("grid_count"), 10.0);
  config.parameters.insert(kj::str("total_investment"), 10000.0);
  config.parameters.insert(kj::str("grid_mode"), 1.0); // Geometric
  GridStrategy strategy(config);
  strategy.on_start();

  auto event = create_trade_event(50000.0);
  strategy.on_event(event);

  const auto& levels = strategy.get_grid_levels();
  KJ_EXPECT(levels.size() == 10);

  // Check first and last levels
  KJ_EXPECT(levels[0].price >= 44999.0 && levels[0].price <= 45001.0);
  KJ_EXPECT(levels[9].price >= 54999.0 && levels[9].price <= 55001.0);

  // In geometric mode, ratio between consecutive levels should be constant
  double ratio1 = levels[1].price / levels[0].price;
  double ratio2 = levels[2].price / levels[1].price;
  KJ_EXPECT(std::abs(ratio1 - ratio2) < 0.001);
}

KJ_TEST("GridStrategy: Initial orders placed") {
  auto config = create_test_config();
  GridStrategy strategy(config);
  strategy.on_start();

  auto event = create_trade_event(50000.0);
  strategy.on_event(event);

  // Should have buy orders below current price and sell orders above
  KJ_EXPECT(strategy.get_active_buy_orders() > 0);
  // No sell orders initially since we have no inventory
  KJ_EXPECT(strategy.get_active_sell_orders() == 0);

  // Get signals
  auto signals = strategy.get_signals();
  KJ_EXPECT(signals.size() > 0);

  // All initial signals should be buy orders
  for (const auto& signal : signals) {
    KJ_EXPECT(signal.side == veloz::exec::OrderSide::Buy);
    KJ_EXPECT(signal.type == veloz::exec::OrderType::Limit);
  }
}

KJ_TEST("GridStrategy: Buy fill triggers sell order") {
  auto config = create_test_config();
  GridStrategy strategy(config);
  strategy.on_start();

  // Initialize at 50000
  auto event1 = create_trade_event(50000.0);
  strategy.on_event(event1);

  // Clear initial signals
  strategy.get_signals();

  // Price drops to trigger a buy fill
  auto event2 = create_trade_event(46000.0);
  strategy.on_event(event2);

  // Should have inventory now
  KJ_EXPECT(strategy.get_total_inventory() > 0);

  // Should have generated sell order signals
  auto signals = strategy.get_signals();
  bool has_sell = false;
  for (const auto& signal : signals) {
    if (signal.side == veloz::exec::OrderSide::Sell) {
      has_sell = true;
      break;
    }
  }
  KJ_EXPECT(has_sell == true);
}

KJ_TEST("GridStrategy: PnL tracking") {
  auto config = create_test_config();
  GridStrategy strategy(config);
  strategy.on_start();

  // Initialize at 50000
  auto event1 = create_trade_event(50000.0);
  strategy.on_event(event1);

  // Initial PnL should be 0
  KJ_EXPECT(strategy.get_realized_pnl() == 0.0);
  KJ_EXPECT(strategy.get_unrealized_pnl() == 0.0);

  // Price drops - buy fills
  auto event2 = create_trade_event(46000.0);
  strategy.on_event(event2);

  // Price rises - should have unrealized profit
  auto event3 = create_trade_event(48000.0);
  strategy.on_event(event3);

  // If we have inventory bought at lower price, unrealized PnL should be positive
  if (strategy.get_total_inventory() > 0) {
    KJ_EXPECT(strategy.get_unrealized_pnl() >= 0.0);
  }
}

KJ_TEST("GridStrategy: Reset clears state") {
  auto config = create_test_config();
  GridStrategy strategy(config);
  strategy.on_start();

  auto event = create_trade_event(50000.0);
  strategy.on_event(event);

  KJ_EXPECT(strategy.is_grid_initialized() == true);

  strategy.reset();

  KJ_EXPECT(strategy.is_grid_initialized() == false);
  KJ_EXPECT(strategy.get_current_price() == 0.0);
  KJ_EXPECT(strategy.get_total_inventory() == 0.0);
  KJ_EXPECT(strategy.get_realized_pnl() == 0.0);
  KJ_EXPECT(strategy.get_grid_levels().size() == 0);
}

KJ_TEST("GridStrategy: Invalid grid parameters") {
  StrategyConfig config;
  config.name = kj::str("InvalidGrid");
  config.type = StrategyType::Grid;
  config.symbols.add(kj::str("BTCUSDT"));

  // Invalid: upper < lower
  config.parameters.insert(kj::str("upper_price"), 45000.0);
  config.parameters.insert(kj::str("lower_price"), 55000.0);
  config.parameters.insert(kj::str("grid_count"), 10.0);

  GridStrategy strategy(config);
  strategy.on_start();

  auto event = create_trade_event(50000.0);
  strategy.on_event(event);

  // Grid should not initialize with invalid parameters
  KJ_EXPECT(strategy.is_grid_initialized() == false);
}

KJ_TEST("GridStrategy: Book event handling") {
  auto config = create_test_config();
  GridStrategy strategy(config);
  strategy.on_start();

  // Use book event instead of trade
  auto event = create_book_event(49990.0, 50010.0);
  strategy.on_event(event);

  KJ_EXPECT(strategy.is_grid_initialized() == true);
  // Mid price should be (49990 + 50010) / 2 = 50000
  KJ_EXPECT(strategy.get_current_price() == 50000.0);
}

KJ_TEST("GridStrategy: Factory creates correct type") {
  GridStrategyFactory factory;
  KJ_EXPECT(factory.get_strategy_type() == "GridStrategy");

  auto config = create_test_config();
  auto strategy = factory.create_strategy(config);

  KJ_EXPECT(strategy->get_type() == StrategyType::Grid);
  KJ_EXPECT(strategy->get_name() == "GridTest");
}

KJ_TEST("GridStrategy: State after start/stop") {
  auto config = create_test_config();
  GridStrategy strategy(config);

  auto state1 = strategy.get_state();
  KJ_EXPECT(state1.is_running == false);

  strategy.on_start();
  auto state2 = strategy.get_state();
  KJ_EXPECT(state2.is_running == true);

  strategy.on_stop();
  auto state3 = strategy.get_state();
  KJ_EXPECT(state3.is_running == false);
}

KJ_TEST("GridStrategy: Metrics updated on events") {
  auto config = create_test_config();
  GridStrategy strategy(config);
  strategy.on_start();

  auto event = create_trade_event(50000.0);
  strategy.on_event(event);

  auto maybe_metrics = strategy.get_metrics();
  KJ_IF_SOME(metrics, maybe_metrics) {
    KJ_EXPECT(metrics.events_processed.load() == 1);
  }

  strategy.on_event(event);

  KJ_IF_SOME(metrics, maybe_metrics) {
    KJ_EXPECT(metrics.events_processed.load() == 2);
  }
}

KJ_TEST("GridStrategy: Timer event handling") {
  auto config = create_test_config();
  GridStrategy strategy(config);
  strategy.on_start();

  // Timer should not crash when called
  strategy.on_timer(1234567890);
}

KJ_TEST("GridStrategy: No signals when not running") {
  auto config = create_test_config();
  GridStrategy strategy(config);

  // Don't call on_start()
  auto event = create_trade_event(50000.0);
  strategy.on_event(event);

  // Should not initialize or generate signals
  KJ_EXPECT(strategy.is_grid_initialized() == false);
  auto signals = strategy.get_signals();
  KJ_EXPECT(signals.size() == 0);
}

} // namespace
