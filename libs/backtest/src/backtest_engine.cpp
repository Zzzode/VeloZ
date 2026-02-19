#include "veloz/backtest/backtest_engine.h"

#include "veloz/backtest/analyzer.h"
#include "veloz/core/logger.h"
#include "veloz/market/market_event.h"
#include "veloz/oms/position.h"

#include <cmath> // std::abs, std::min - standard C++ math functions (no KJ equivalent)
#include <kj/common.h>
#include <kj/debug.h>
#include <kj/hash.h>
#include <kj/map.h>
#include <kj/memory.h>
#include <kj/refcount.h>
#include <kj/string.h>

namespace veloz::backtest {

namespace {

// Helper function to get current price from market event
// Uses kj::OneOf API: .is<T>() and .get<T>() instead of std::holds_alternative/std::get
kj::Maybe<double> get_price_from_event(const veloz::market::MarketEvent& event) {
  switch (event.type) {
  case veloz::market::MarketEventType::Trade: {
    if (event.data.is<veloz::market::TradeData>()) {
      return event.data.get<veloz::market::TradeData>().price;
    }
    break;
  }
  case veloz::market::MarketEventType::Kline: {
    if (event.data.is<veloz::market::KlineData>()) {
      return event.data.get<veloz::market::KlineData>().close;
    }
    break;
  }
  case veloz::market::MarketEventType::BookTop: {
    if (event.data.is<veloz::market::BookData>()) {
      const auto& book = event.data.get<veloz::market::BookData>();
      // Return mid price (average of best bid and best ask)
      // kj::Vector has no empty() method, use size() == 0
      if (book.bids.size() > 0 && book.asks.size() > 0) {
        return (book.bids[0].price + book.asks[0].price) / 2.0;
      }
    }
    break;
  }
  default:
    break;
  }
  return kj::none;
}

// Helper function to convert OrderSide to string
kj::String order_side_to_string(veloz::exec::OrderSide side) {
  return side == veloz::exec::OrderSide::Buy ? kj::str("buy") : kj::str("sell");
}

// Helper function to calculate slippage-adjusted price
double calculate_fill_price(double base_price, veloz::exec::OrderSide side, double slippage_rate) {
  // Apply slippage: buy orders get worse (higher) price, sell orders get worse (lower) price
  if (side == veloz::exec::OrderSide::Buy) {
    return base_price * (1.0 + slippage_rate);
  } else {
    return base_price * (1.0 - slippage_rate);
  }
}

} // anonymous namespace

// Hash function for kj::String to use with kj::HashMap
struct StringHash {
  inline size_t operator()(const kj::String& s) const {
    return kj::hashCode(s.asPtr());
  }
};

struct BacktestEngine::Impl {
  BacktestConfig config;
  // kj::Rc for reference-counted strategy (matches strategy module's kj::Rc<IStrategy>)
  kj::Rc<veloz::strategy::IStrategy> strategy;
  // kj::Rc for reference-counted data source
  kj::Rc<IDataSource> data_source;
  BacktestResult result;
  bool is_running;
  bool is_initialized;
  kj::Maybe<kj::Function<void(double)>> progress_callback;
  // kj::Own used for unique ownership of internal logger instance
  kj::Own<veloz::core::Logger> logger;

  // Order simulation state - using kj::HashMap with kj::String keys
  kj::HashMap<kj::String, veloz::oms::Position> positions;
  double current_equity;
  double slippage_rate;
  double fee_rate;

  Impl()
      : is_running(false), is_initialized(false), current_equity(0.0), slippage_rate(0.001),
        fee_rate(0.001), logger(kj::heap<veloz::core::Logger>()) {}
};

BacktestEngine::BacktestEngine() : impl_(kj::heap<Impl>()) {}

BacktestEngine::~BacktestEngine() noexcept {}

bool BacktestEngine::initialize(const BacktestConfig& config) {
  impl_->logger->info("Initializing backtest engine");
  impl_->config.strategy_name = kj::str(config.strategy_name);
  impl_->config.symbol = kj::str(config.symbol);
  impl_->config.start_time = config.start_time;
  impl_->config.end_time = config.end_time;
  impl_->config.initial_balance = config.initial_balance;
  impl_->config.risk_per_trade = config.risk_per_trade;
  impl_->config.max_position_size = config.max_position_size;
  impl_->config.strategy_parameters.clear();
  for (const auto& entry : config.strategy_parameters) {
    impl_->config.strategy_parameters.upsert(kj::str(entry.key), entry.value);
  }
  impl_->config.data_source = kj::str(config.data_source);
  impl_->config.data_type = kj::str(config.data_type);
  impl_->config.time_frame = kj::str(config.time_frame);

  impl_->result = BacktestResult();
  impl_->result.initial_balance = config.initial_balance;
  impl_->result.strategy_name = kj::str(config.strategy_name);
  impl_->result.symbol = kj::str(config.symbol);
  impl_->result.start_time = config.start_time;
  impl_->result.end_time = config.end_time;
  impl_->is_running = false;
  impl_->is_initialized = true;

  // Initialize order simulation state
  impl_->positions.clear();
  impl_->current_equity = config.initial_balance;

  // Configure slippage and fee from config if available (using defaults otherwise)
  // These could be added to BacktestConfig in the future

  return true;
}

bool BacktestEngine::run() {
  if (!impl_->is_initialized) {
    impl_->logger->error("Backtest engine not initialized");
    return false;
  }

  if (impl_->strategy == nullptr) {
    impl_->logger->error("No strategy set");
    return false;
  }

  if (impl_->data_source == nullptr) {
    impl_->logger->error("No data source set");
    return false;
  }

  impl_->logger->info("Starting backtest");
  impl_->is_running = true;

  auto runBacktest = [&]() -> bool {
    // Connect to data source
    if (!impl_->data_source->connect()) {
      impl_->logger->error("Failed to connect to data source");
      impl_->is_running = false;
      return false;
    }

    // Get historical data
    auto market_events = impl_->data_source->get_data(
        impl_->config.symbol, impl_->config.start_time, impl_->config.end_time,
        impl_->config.data_type, impl_->config.time_frame);

    // Initialize strategy
    veloz::strategy::StrategyConfig strategy_config;
    strategy_config.name = kj::str(impl_->config.strategy_name);
    strategy_config.type = veloz::strategy::StrategyType::Custom;
    strategy_config.risk_per_trade = impl_->config.risk_per_trade;
    strategy_config.max_position_size = impl_->config.max_position_size;
    strategy_config.symbols.add(kj::str(impl_->config.symbol));
    for (const auto& entry : impl_->config.strategy_parameters) {
      strategy_config.parameters.insert(kj::str(entry.key), entry.value);
    }

    if (!impl_->strategy->initialize(strategy_config, *impl_->logger)) {
      impl_->logger->error("Failed to initialize strategy");
      impl_->data_source->disconnect();
      impl_->is_running = false;
      return false;
    }

    // Run backtest simulation
    double progress = 0.0;
    const double progress_step = market_events.size() > 0 ? 1.0 / market_events.size() : 1.0;

    for (size_t i = 0; i < market_events.size(); ++i) {
      if (!impl_->is_running) {
        break;
      }

      const auto& event = market_events[i];
      impl_->strategy->on_event(event);

      // Get signals
      auto signals = impl_->strategy->get_signals();

      // Get current price from market event for order simulation
      KJ_IF_SOME(current_price, get_price_from_event(event)) {
        // Process each signal (order request)
        for (const auto& signal : signals) {
          kj::StringPtr symbol = signal.symbol.value;
          double qty = signal.qty;

          if (qty <= 0.0) {
            impl_->logger->warn(kj::str("Invalid quantity: ", qty, " for signal").cStr());
            continue;
          }

          // Get or create position for this symbol using kj::HashMap
          kj::String symbol_key = kj::str(symbol);
          veloz::oms::Position* position_ptr = nullptr;
          KJ_IF_SOME(existing, impl_->positions.find(symbol_key)) {
            position_ptr = &existing;
          }
          else {
            // insert() returns Entry& which has .key and .value members
            auto& entry = impl_->positions.insert(
                kj::str(symbol_key), veloz::oms::Position{veloz::common::SymbolId(symbol)});
            position_ptr = &entry.value;
          }
          auto& position = *position_ptr;

          // Check position size constraints
          double new_size = position.size();
          if (signal.side == veloz::exec::OrderSide::Buy) {
            new_size += qty;
          } else {
            new_size -= qty;
          }

          // Check max position size constraint (both long and short)
          if (std::abs(new_size) > impl_->config.max_position_size) {
            impl_->logger->warn(
                kj::str("Order rejected: would exceed max position size ",
                        impl_->config.max_position_size)
                    .cStr());
            continue;
          }

          // Calculate fill price with slippage
          double fill_price =
              calculate_fill_price(current_price, signal.side, impl_->slippage_rate);

          // Calculate trade fee
          double trade_value = fill_price * qty;
          double fee = trade_value * impl_->fee_rate;

          // Get position state before applying fill for PnL calculation
          double pre_fill_realized_pnl = position.realized_pnl();

          // Apply fill to position
          position.apply_fill(signal.side, qty, fill_price);

          // Calculate PnL from this trade (change in realized PnL)
          double trade_pnl = position.realized_pnl() - pre_fill_realized_pnl;

          // Update current equity (subtract fee, add PnL)
          impl_->current_equity -= fee;

          // Create trade record
          TradeRecord trade_record;
          trade_record.timestamp = event.ts_exchange_ns / 1'000'000; // Convert to milliseconds
          trade_record.symbol = kj::str(symbol);
          trade_record.side = order_side_to_string(signal.side);
          trade_record.price = fill_price;
          trade_record.quantity = qty;
          trade_record.fee = fee;
          trade_record.pnl = trade_pnl;
          trade_record.strategy_id = kj::str(impl_->strategy->get_id());

          impl_->result.trades.add(kj::mv(trade_record));

          impl_->logger->info(
              kj::str("Order filled: ", impl_->result.trades.back().side, " ", qty, " @ ",
                      fill_price, ", fee: ", fee, ", PnL: ", trade_pnl,
                      ", equity: ", impl_->current_equity)
                  .cStr());

          // Notify strategy of position update
          impl_->strategy->on_position_update(position);
        }
      }

      // Update progress
      progress += progress_step;
      KJ_IF_SOME(callback, impl_->progress_callback) {
        callback(std::min(progress, 1.0));
      }
    }

    // Stop strategy
    impl_->strategy->on_stop();

    // Disconnect from data source
    impl_->data_source->disconnect();

    // Calculate final equity by adding unrealized PnL from remaining positions
    double total_unrealized_pnl = 0.0;
    for (const auto& entry : impl_->positions) {
      // Use last known price from the last market event for unrealized PnL
      if (market_events.size() > 0) {
        KJ_IF_SOME(last_price, get_price_from_event(market_events.back())) {
          total_unrealized_pnl += entry.value.unrealized_pnl(last_price);
        }
      }
    }

    impl_->result.final_balance = impl_->current_equity + total_unrealized_pnl;

    // Calculate backtest result
    auto analyzer = BacktestAnalyzer();
    auto analyzed_result = analyzer.analyze(impl_->result.trades);
    impl_->result.win_rate = analyzed_result->win_rate;
    impl_->result.profit_factor = analyzed_result->profit_factor;
    impl_->result.trade_count = analyzed_result->trade_count;
    impl_->result.win_count = analyzed_result->win_count;
    impl_->result.lose_count = analyzed_result->lose_count;
    impl_->result.avg_win = analyzed_result->avg_win;
    impl_->result.avg_lose = analyzed_result->avg_lose;

    impl_->is_running = false;
    impl_->logger->info("Backtest completed");

    return true;
  };

  KJ_IF_SOME(exception, kj::runCatchingExceptions(runBacktest)) {
    impl_->logger->error(kj::str("Backtest failed: ", exception.getDescription()).cStr());
    impl_->is_running = false;

    if (impl_->data_source != nullptr) {
      impl_->data_source->disconnect();
    }

    return false;
  }

  return true;
}

bool BacktestEngine::stop() {
  if (!impl_->is_running) {
    impl_->logger->warn("Backtest not running");
    return false;
  }

  impl_->logger->info("Stopping backtest");
  impl_->is_running = false;
  return true;
}

bool BacktestEngine::reset() {
  impl_->logger->info("Resetting backtest engine");
  impl_->config = BacktestConfig();
  impl_->strategy = nullptr;
  impl_->data_source = nullptr;
  impl_->result = BacktestResult();
  impl_->is_running = false;
  impl_->is_initialized = false;

  // Reset order simulation state
  impl_->positions.clear();
  impl_->current_equity = 0.0;

  return true;
}

BacktestResult BacktestEngine::get_result() const {
  BacktestResult result;
  result.strategy_name = kj::str(impl_->result.strategy_name);
  result.symbol = kj::str(impl_->result.symbol);
  result.start_time = impl_->result.start_time;
  result.end_time = impl_->result.end_time;
  result.initial_balance = impl_->result.initial_balance;
  result.final_balance = impl_->result.final_balance;
  result.total_return = impl_->result.total_return;
  result.max_drawdown = impl_->result.max_drawdown;
  result.sharpe_ratio = impl_->result.sharpe_ratio;
  result.win_rate = impl_->result.win_rate;
  result.profit_factor = impl_->result.profit_factor;
  result.trade_count = impl_->result.trade_count;
  result.win_count = impl_->result.win_count;
  result.lose_count = impl_->result.lose_count;
  result.avg_win = impl_->result.avg_win;
  result.avg_lose = impl_->result.avg_lose;

  // Copy trades
  for (const auto& trade : impl_->result.trades) {
    TradeRecord copy;
    copy.timestamp = trade.timestamp;
    copy.symbol = kj::str(trade.symbol);
    copy.side = kj::str(trade.side);
    copy.price = trade.price;
    copy.quantity = trade.quantity;
    copy.fee = trade.fee;
    copy.pnl = trade.pnl;
    copy.strategy_id = kj::str(trade.strategy_id);
    result.trades.add(kj::mv(copy));
  }

  // Copy equity curve
  for (const auto& point : impl_->result.equity_curve) {
    result.equity_curve.add(point);
  }

  // Copy drawdown curve
  for (const auto& point : impl_->result.drawdown_curve) {
    result.drawdown_curve.add(point);
  }

  return result;
}

void BacktestEngine::set_strategy(kj::Rc<veloz::strategy::IStrategy> strategy) {
  impl_->strategy = kj::mv(strategy);
}

void BacktestEngine::set_data_source(kj::Rc<IDataSource> data_source) {
  impl_->data_source = kj::mv(data_source);
}

void BacktestEngine::on_progress(kj::Function<void(double progress)> callback) {
  impl_->progress_callback = kj::mv(callback);
}

} // namespace veloz::backtest
