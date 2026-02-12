#include "veloz/backtest/backtest_engine.h"

#include "veloz/backtest/analyzer.h"
#include "veloz/core/logger.h"
#include "veloz/market/market_event.h"
#include "veloz/oms/position.h"

#include <optional>
#include <unordered_map>

namespace veloz::backtest {

namespace {

// Helper function to get current price from market event
std::optional<double> get_price_from_event(const veloz::market::MarketEvent& event) {
  switch (event.type) {
  case veloz::market::MarketEventType::Trade: {
    if (std::holds_alternative<veloz::market::TradeData>(event.data)) {
      return std::get<veloz::market::TradeData>(event.data).price;
    }
    break;
  }
  case veloz::market::MarketEventType::Kline: {
    if (std::holds_alternative<veloz::market::KlineData>(event.data)) {
      return std::get<veloz::market::KlineData>(event.data).close;
    }
    break;
  }
  case veloz::market::MarketEventType::BookTop: {
    if (std::holds_alternative<veloz::market::BookData>(event.data)) {
      const auto& book = std::get<veloz::market::BookData>(event.data);
      // Return mid price (average of best bid and best ask)
      if (!book.bids.empty() && !book.asks.empty()) {
        return (book.bids[0].price + book.asks[0].price) / 2.0;
      }
    }
    break;
  }
  default:
    break;
  }
  return std::nullopt;
}

// Helper function to convert OrderSide to string
std::string order_side_to_string(veloz::exec::OrderSide side) {
  return side == veloz::exec::OrderSide::Buy ? "buy" : "sell";
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

struct BacktestEngine::Impl {
  BacktestConfig config;
  std::shared_ptr<veloz::strategy::IStrategy> strategy;
  std::shared_ptr<IDataSource> data_source;
  BacktestResult result;
  bool is_running;
  bool is_initialized;
  std::function<void(double)> progress_callback;
  std::shared_ptr<veloz::core::Logger> logger;

  // Order simulation state
  std::unordered_map<std::string, veloz::oms::Position> positions;
  double current_equity;
  double slippage_rate;
  double fee_rate;

  Impl()
      : is_running(false), is_initialized(false), current_equity(0.0), slippage_rate(0.001),
        fee_rate(0.001) {
    logger = std::make_shared<veloz::core::Logger>();
  }
};

BacktestEngine::BacktestEngine() : impl_(std::make_unique<Impl>()) {}

BacktestEngine::~BacktestEngine() {}

bool BacktestEngine::initialize(const BacktestConfig& config) {
  impl_->logger->info(std::format("Initializing backtest engine"));
  impl_->config = config;
  impl_->result = BacktestResult();
  impl_->result.initial_balance = config.initial_balance;
  impl_->result.strategy_name = config.strategy_name;
  impl_->result.symbol = config.symbol;
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
    impl_->logger->error(std::format("Backtest engine not initialized"));
    return false;
  }

  if (!impl_->strategy) {
    impl_->logger->error(std::format("No strategy set"));
    return false;
  }

  if (!impl_->data_source) {
    impl_->logger->error(std::format("No data source set"));
    return false;
  }

  impl_->logger->info(std::format("Starting backtest"));
  impl_->is_running = true;

  try {
    // Connect to data source
    if (!impl_->data_source->connect()) {
      impl_->logger->error(std::format("Failed to connect to data source"));
      impl_->is_running = false;
      return false;
    }

    // Get historical data
    auto market_events = impl_->data_source->get_data(
        impl_->config.symbol, impl_->config.start_time, impl_->config.end_time,
        impl_->config.data_type, impl_->config.time_frame);

    // Initialize strategy
    veloz::strategy::StrategyConfig strategy_config;
    strategy_config.name = impl_->config.strategy_name;
    strategy_config.type = veloz::strategy::StrategyType::Custom;
    strategy_config.risk_per_trade = impl_->config.risk_per_trade;
    strategy_config.max_position_size = impl_->config.max_position_size;
    strategy_config.symbols = {impl_->config.symbol};
    strategy_config.parameters = impl_->config.strategy_parameters;

    if (!impl_->strategy->initialize(strategy_config, *impl_->logger)) {
      impl_->logger->error(std::format("Failed to initialize strategy"));
      impl_->data_source->disconnect();
      impl_->is_running = false;
      return false;
    }

    // Run backtest simulation
    double progress = 0.0;
    const double progress_step = 1.0 / market_events.size();

    for (size_t i = 0; i < market_events.size(); ++i) {
      if (!impl_->is_running) {
        break;
      }

      const auto& event = market_events[i];
      impl_->strategy->on_event(event);

      // Get signals
      auto signals = impl_->strategy->get_signals();

      // Get current price from market event for order simulation
      auto current_price_opt = get_price_from_event(event);
      if (!current_price_opt) {
        // No price data available, skip order processing for this event
        progress += progress_step;
        if (impl_->progress_callback) {
          impl_->progress_callback(std::min(progress, 1.0));
        }
        continue;
      }
      double current_price = *current_price_opt;

      // Process each signal (order request)
      for (const auto& signal : signals) {
        const std::string& symbol = signal.symbol.value;
        double qty = signal.qty;

        if (qty <= 0.0) {
          impl_->logger->warn(std::format("Invalid quantity: {} for signal", qty));
          continue;
        }

        // Get or create position for this symbol
        if (impl_->positions.find(symbol) == impl_->positions.end()) {
          impl_->positions[symbol] = veloz::oms::Position{veloz::common::SymbolId(symbol)};
        }
        auto& position = impl_->positions[symbol];

        // Check position size constraints
        double new_size = position.size();
        if (signal.side == veloz::exec::OrderSide::Buy) {
          new_size += qty;
        } else {
          new_size -= qty;
        }

        // Check max position size constraint (both long and short)
        if (std::abs(new_size) > impl_->config.max_position_size) {
          impl_->logger->warn(std::format("Order rejected: would exceed max position size {}",
                                          impl_->config.max_position_size));
          continue;
        }

        // Calculate fill price with slippage
        double fill_price = calculate_fill_price(current_price, signal.side, impl_->slippage_rate);

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
        trade_record.symbol = symbol;
        trade_record.side = order_side_to_string(signal.side);
        trade_record.price = fill_price;
        trade_record.quantity = qty;
        trade_record.fee = fee;
        trade_record.pnl = trade_pnl;
        trade_record.strategy_id = impl_->strategy->get_id();

        impl_->result.trades.push_back(trade_record);

        impl_->logger->info(std::format("Order filled: {} {} @ {}, fee: {}, PnL: {}, equity: {}",
                                        trade_record.side, qty, fill_price, fee, trade_pnl,
                                        impl_->current_equity));

        // Notify strategy of position update
        impl_->strategy->on_position_update(position);
      }

      // Update progress
      progress += progress_step;
      if (impl_->progress_callback) {
        impl_->progress_callback(std::min(progress, 1.0));
      }
    }

    // Stop strategy
    impl_->strategy->on_stop();

    // Disconnect from data source
    impl_->data_source->disconnect();

    // Calculate final equity by adding unrealized PnL from remaining positions
    double total_unrealized_pnl = 0.0;
    for (const auto& [symbol, position] : impl_->positions) {
      // Use last known price from the last market event for unrealized PnL
      if (!market_events.empty()) {
        auto last_price_opt = get_price_from_event(market_events.back());
        if (last_price_opt) {
          total_unrealized_pnl += position.unrealized_pnl(*last_price_opt);
        }
      }
    }

    impl_->result.final_balance = impl_->current_equity + total_unrealized_pnl;

    // Calculate backtest result
    auto analyzer = BacktestAnalyzer();
    auto analyzed_result = analyzer.analyze(impl_->result.trades);
    impl_->result = *analyzed_result;

    // Overwrite the final balance in the analyzed result with our calculated value
    impl_->result.final_balance = impl_->result.final_balance;

    impl_->is_running = false;
    impl_->logger->info(std::format("Backtest completed"));

    return true;
  } catch (const std::exception& e) {
    impl_->logger->error(std::format("Backtest failed: {}", e.what()));
    impl_->is_running = false;

    if (impl_->data_source) {
      impl_->data_source->disconnect();
    }

    return false;
  }
}

bool BacktestEngine::stop() {
  if (!impl_->is_running) {
    impl_->logger->warn(std::format("Backtest not running"));
    return false;
  }

  impl_->logger->info(std::format("Stopping backtest"));
  impl_->is_running = false;
  return true;
}

bool BacktestEngine::reset() {
  impl_->logger->info(std::format("Resetting backtest engine"));
  impl_->config = BacktestConfig();
  impl_->strategy.reset();
  impl_->data_source.reset();
  impl_->result = BacktestResult();
  impl_->is_running = false;
  impl_->is_initialized = false;

  // Reset order simulation state
  impl_->positions.clear();
  impl_->current_equity = 0.0;

  return true;
}

BacktestResult BacktestEngine::get_result() const {
  return impl_->result;
}

void BacktestEngine::set_strategy(const std::shared_ptr<veloz::strategy::IStrategy>& strategy) {
  impl_->strategy = strategy;
}

void BacktestEngine::set_data_source(const std::shared_ptr<IDataSource>& data_source) {
  impl_->data_source = data_source;
}

void BacktestEngine::on_progress(std::function<void(double progress)> callback) {
  impl_->progress_callback = callback;
}

} // namespace veloz::backtest
