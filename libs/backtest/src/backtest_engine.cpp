#include "veloz/backtest/backtest_engine.h"

#include "veloz/backtest/analyzer.h"
#include "veloz/core/logger.h"
#include "veloz/core/priority_queue.h"
#include "veloz/market/market_event.h"
#include "veloz/oms/position.h"

#include <chrono> // std::chrono::steady_clock - KJ time types don't provide steady_clock equivalent
#include <cmath>  // std::abs, std::min - standard C++ math functions (no KJ equivalent)
#include <kj/common.h>
#include <kj/debug.h>
#include <kj/hash.h>
#include <kj/map.h>
#include <kj/memory.h>
#include <kj/mutex.h>
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

// Get current wall clock time in nanoseconds
std::int64_t wall_clock_ns() {
  return std::chrono::duration_cast<std::chrono::nanoseconds>(
             std::chrono::steady_clock::now().time_since_epoch())
      .count();
}

} // anonymous namespace

// ============================================================================
// String conversion functions
// ============================================================================

kj::StringPtr to_string(BacktestState state) {
  switch (state) {
  case BacktestState::Idle:
    return "Idle"_kj;
  case BacktestState::Initialized:
    return "Initialized"_kj;
  case BacktestState::Running:
    return "Running"_kj;
  case BacktestState::Paused:
    return "Paused"_kj;
  case BacktestState::Completed:
    return "Completed"_kj;
  case BacktestState::Stopped:
    return "Stopped"_kj;
  case BacktestState::Error:
    return "Error"_kj;
  }
  return "Unknown"_kj;
}

kj::StringPtr to_string(BacktestEventPriority priority) {
  switch (priority) {
  case BacktestEventPriority::Low:
    return "Low"_kj;
  case BacktestEventPriority::Normal:
    return "Normal"_kj;
  case BacktestEventPriority::High:
    return "High"_kj;
  case BacktestEventPriority::Critical:
    return "Critical"_kj;
  }
  return "Unknown"_kj;
}

kj::StringPtr to_string(BacktestEventType type) {
  switch (type) {
  case BacktestEventType::MarketData:
    return "MarketData"_kj;
  case BacktestEventType::OrderFill:
    return "OrderFill"_kj;
  case BacktestEventType::Timer:
    return "Timer"_kj;
  case BacktestEventType::RiskCheck:
    return "RiskCheck"_kj;
  case BacktestEventType::Custom:
    return "Custom"_kj;
  }
  return "Unknown"_kj;
}

// ============================================================================
// VirtualClock implementation
// ============================================================================

VirtualClock::VirtualClock() : current_time_ns_(0), start_time_ns_(0), end_time_ns_(0) {}

void VirtualClock::set_start_time(std::int64_t start_time_ns) {
  start_time_ns_ = start_time_ns;
  current_time_ns_.store(start_time_ns);
}

void VirtualClock::set_end_time(std::int64_t end_time_ns) {
  end_time_ns_ = end_time_ns;
}

bool VirtualClock::advance_to(std::int64_t time_ns) {
  std::int64_t current = current_time_ns_.load();
  if (time_ns < current) {
    return false; // Cannot go backwards
  }
  current_time_ns_.store(time_ns);
  return true;
}

std::int64_t VirtualClock::now_ns() const {
  return current_time_ns_.load();
}

std::int64_t VirtualClock::now_ms() const {
  return current_time_ns_.load() / 1'000'000;
}

std::int64_t VirtualClock::start_time_ns() const {
  return start_time_ns_;
}

std::int64_t VirtualClock::end_time_ns() const {
  return end_time_ns_;
}

double VirtualClock::progress() const {
  if (end_time_ns_ <= start_time_ns_) {
    return 0.0;
  }
  std::int64_t current = current_time_ns_.load();
  if (current <= start_time_ns_) {
    return 0.0;
  }
  if (current >= end_time_ns_) {
    return 1.0;
  }
  return static_cast<double>(current - start_time_ns_) /
         static_cast<double>(end_time_ns_ - start_time_ns_);
}

std::int64_t VirtualClock::elapsed_ns() const {
  std::int64_t current = current_time_ns_.load();
  if (current <= start_time_ns_) {
    return 0;
  }
  return current - start_time_ns_;
}

std::int64_t VirtualClock::remaining_ns() const {
  std::int64_t current = current_time_ns_.load();
  if (current >= end_time_ns_) {
    return 0;
  }
  return end_time_ns_ - current;
}

void VirtualClock::reset() {
  current_time_ns_.store(start_time_ns_);
}

// ============================================================================
// BacktestEngine implementation
// ============================================================================

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

  // State management
  std::atomic<BacktestState> state{BacktestState::Idle};
  std::atomic<bool> pause_requested{false};

  // Virtual clock for time simulation
  VirtualClock clock;

  // Event queue with priority
  kj::MutexGuarded<veloz::core::PriorityQueue<BacktestEvent>> event_queue;
  std::atomic<std::int64_t> next_sequence{0};
  std::atomic<std::int64_t> events_processed{0};
  std::atomic<std::int64_t> total_events{0};

  // Progress tracking
  std::int64_t real_start_time_ns{0};
  std::int64_t last_progress_report_ns{0};
  static constexpr std::int64_t progress_report_interval_ns = 100'000'000; // 100ms

  // Callbacks
  kj::Maybe<kj::Function<void(double)>> progress_callback;
  kj::Maybe<kj::Function<void(const BacktestProgress&)>> progress_detailed_callback;
  kj::Maybe<kj::Function<void(BacktestState, BacktestState)>> state_change_callback;

  // kj::Own used for unique ownership of internal logger instance
  kj::Own<veloz::core::Logger> logger;

  // Order simulation state - using kj::HashMap with kj::String keys
  kj::HashMap<kj::String, veloz::oms::Position> positions;
  double current_equity;
  double slippage_rate;
  double fee_rate;

  Impl()
      : state(BacktestState::Idle), pause_requested(false), current_equity(0.0),
        slippage_rate(0.001), fee_rate(0.001), logger(kj::heap<veloz::core::Logger>()) {}
};

BacktestEngine::BacktestEngine() : impl_(kj::heap<Impl>()) {}

BacktestEngine::~BacktestEngine() noexcept {}

bool BacktestEngine::initialize(const BacktestConfig& config) {
  impl_->logger->info("Initializing backtest engine");

  // Copy configuration
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

  // Initialize result
  impl_->result = BacktestResult();
  impl_->result.initial_balance = config.initial_balance;
  impl_->result.strategy_name = kj::str(config.strategy_name);
  impl_->result.symbol = kj::str(config.symbol);
  impl_->result.start_time = config.start_time;
  impl_->result.end_time = config.end_time;

  // Initialize virtual clock (convert ms to ns)
  impl_->clock.set_start_time(config.start_time * 1'000'000);
  impl_->clock.set_end_time(config.end_time * 1'000'000);

  // Initialize order simulation state
  impl_->positions.clear();
  impl_->current_equity = config.initial_balance;

  // Clear event queue
  {
    auto lock = impl_->event_queue.lockExclusive();
    lock->clear();
  }
  impl_->events_processed.store(0);
  impl_->total_events.store(0);
  impl_->next_sequence.store(0);

  transition_state(BacktestState::Initialized);
  return true;
}

void BacktestEngine::load_events_from_data_source() {
  if (impl_->data_source == nullptr) {
    impl_->logger->error("No data source set");
    return;
  }

  // Connect to data source
  if (!impl_->data_source->connect()) {
    impl_->logger->error("Failed to connect to data source");
    return;
  }

  // Get historical data
  auto market_events = impl_->data_source->get_data(impl_->config.symbol, impl_->config.start_time,
                                                    impl_->config.end_time, impl_->config.data_type,
                                                    impl_->config.time_frame);

  impl_->logger->info(kj::str("Loaded ", market_events.size(), " events from data source").cStr());

  // Convert market events to backtest events and add to queue
  {
    auto lock = impl_->event_queue.lockExclusive();
    for (size_t i = 0; i < market_events.size(); ++i) {
      BacktestEvent bt_event;
      bt_event.type = BacktestEventType::MarketData;
      bt_event.priority = BacktestEventPriority::Normal;
      bt_event.timestamp_ns = market_events[i].ts_exchange_ns;
      bt_event.sequence = impl_->next_sequence.fetch_add(1);
      bt_event.market_event = kj::mv(market_events[i]);
      bt_event.custom_data = kj::str("");
      lock->push(kj::mv(bt_event));
    }
  }

  impl_->total_events.store(impl_->next_sequence.load());
  impl_->data_source->disconnect();
}

bool BacktestEngine::run() {
  BacktestState current_state = impl_->state.load();
  if (current_state != BacktestState::Initialized && current_state != BacktestState::Paused) {
    impl_->logger->error(
        kj::str("Cannot run backtest from state: ", to_string(current_state)).cStr());
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
  impl_->pause_requested.store(false);
  impl_->real_start_time_ns = wall_clock_ns();

  // Load events if starting fresh (not resuming from pause)
  if (current_state == BacktestState::Initialized) {
    load_events_from_data_source();

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
      transition_state(BacktestState::Error);
      return false;
    }
  }

  transition_state(BacktestState::Running);

  // Main event loop
  auto runBacktest = [&]() -> bool {
    while (impl_->state.load() == BacktestState::Running) {
      // Check for pause request
      if (impl_->pause_requested.load()) {
        transition_state(BacktestState::Paused);
        impl_->logger->info("Backtest paused");
        return true;
      }

      // Process next event
      if (!process_single_event()) {
        // No more events - backtest complete
        break;
      }

      // Report progress periodically
      std::int64_t now = wall_clock_ns();
      if (now - impl_->last_progress_report_ns >= Impl::progress_report_interval_ns) {
        report_progress();
        impl_->last_progress_report_ns = now;
      }
    }

    // Finalize if we completed normally (not paused or stopped)
    if (impl_->state.load() == BacktestState::Running) {
      // Stop strategy
      impl_->strategy->on_stop();

      // Calculate final equity by adding unrealized PnL from remaining positions
      double total_unrealized_pnl = 0.0;
      for (const auto& entry : impl_->positions) {
        // Use end time price estimate (last known price)
        // In a real implementation, we'd track the last price per symbol
        total_unrealized_pnl += entry.value.unrealized_pnl(0.0);
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

      transition_state(BacktestState::Completed);
      impl_->logger->info("Backtest completed");
    }

    return true;
  };

  KJ_IF_SOME(exception, kj::runCatchingExceptions(runBacktest)) {
    impl_->logger->error(kj::str("Backtest failed: ", exception.getDescription()).cStr());
    transition_state(BacktestState::Error);
    return false;
  }

  // Final progress report
  report_progress();
  return true;
}

bool BacktestEngine::process_single_event() {
  BacktestEvent event;

  // Get next event from queue
  {
    auto lock = impl_->event_queue.lockExclusive();
    if (lock->empty()) {
      return false;
    }
    event = lock->popValue();
  }

  return process_event(event);
}

bool BacktestEngine::process_event(BacktestEvent& event) {
  // Advance virtual clock to event time
  impl_->clock.advance_to(event.timestamp_ns);

  // Process based on event type
  switch (event.type) {
  case BacktestEventType::MarketData: {
    // Feed event to strategy
    impl_->strategy->on_event(event.market_event);

    // Get signals from strategy
    auto signals = impl_->strategy->get_signals();

    // Get current price from market event for order simulation
    KJ_IF_SOME(current_price, get_price_from_event(event.market_event)) {
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
          impl_->logger->warn(kj::str("Order rejected: would exceed max position size ",
                                      impl_->config.max_position_size)
                                  .cStr());
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
        trade_record.timestamp = event.timestamp_ns / 1'000'000; // Convert to milliseconds
        trade_record.symbol = kj::str(symbol);
        trade_record.side = order_side_to_string(signal.side);
        trade_record.price = fill_price;
        trade_record.quantity = qty;
        trade_record.fee = fee;
        trade_record.pnl = trade_pnl;
        trade_record.strategy_id = kj::str(impl_->strategy->get_id());

        impl_->result.trades.add(kj::mv(trade_record));

        impl_->logger->info(kj::str("Order filled: ", impl_->result.trades.back().side, " ", qty,
                                    " @ ", fill_price, ", fee: ", fee, ", PnL: ", trade_pnl,
                                    ", equity: ", impl_->current_equity)
                                .cStr());

        // Notify strategy of position update
        impl_->strategy->on_position_update(position);

        // Record equity curve point
        EquityCurvePoint equity_point;
        equity_point.timestamp = event.timestamp_ns / 1'000'000;
        equity_point.equity = impl_->current_equity;
        equity_point.cumulative_return =
            (impl_->current_equity - impl_->config.initial_balance) / impl_->config.initial_balance;
        impl_->result.equity_curve.add(equity_point);
      }
    }
    break;
  }

  case BacktestEventType::Timer: {
    impl_->strategy->on_timer(event.timestamp_ns / 1'000'000);
    break;
  }

  case BacktestEventType::OrderFill:
  case BacktestEventType::RiskCheck:
  case BacktestEventType::Custom:
    // These event types can be extended in the future
    break;
  }

  impl_->events_processed.fetch_add(1);
  return true;
}

bool BacktestEngine::stop() {
  BacktestState current_state = impl_->state.load();
  if (current_state != BacktestState::Running && current_state != BacktestState::Paused) {
    impl_->logger->warn(
        kj::str("Cannot stop backtest from state: ", to_string(current_state)).cStr());
    return false;
  }

  impl_->logger->info("Stopping backtest");
  transition_state(BacktestState::Stopped);
  return true;
}

bool BacktestEngine::pause() {
  if (impl_->state.load() != BacktestState::Running) {
    impl_->logger->warn("Cannot pause: backtest is not running");
    return false;
  }

  impl_->pause_requested.store(true);
  return true;
}

bool BacktestEngine::resume() {
  if (impl_->state.load() != BacktestState::Paused) {
    impl_->logger->warn("Cannot resume: backtest is not paused");
    return false;
  }

  impl_->logger->info("Resuming backtest");
  return run(); // run() handles resuming from paused state
}

bool BacktestEngine::reset() {
  impl_->logger->info("Resetting backtest engine");
  impl_->config = BacktestConfig();
  impl_->strategy = nullptr;
  impl_->data_source = nullptr;
  impl_->result = BacktestResult();

  // Reset virtual clock
  impl_->clock.set_start_time(0);
  impl_->clock.set_end_time(0);
  impl_->clock.reset();

  // Clear event queue
  {
    auto lock = impl_->event_queue.lockExclusive();
    lock->clear();
  }
  impl_->events_processed.store(0);
  impl_->total_events.store(0);
  impl_->next_sequence.store(0);

  // Reset order simulation state
  impl_->positions.clear();
  impl_->current_equity = 0.0;

  transition_state(BacktestState::Idle);
  return true;
}

BacktestState BacktestEngine::get_state() const {
  return impl_->state.load();
}

const VirtualClock& BacktestEngine::get_clock() const {
  return impl_->clock;
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

void BacktestEngine::on_progress_detailed(kj::Function<void(const BacktestProgress&)> callback) {
  impl_->progress_detailed_callback = kj::mv(callback);
}

void BacktestEngine::on_state_change(kj::Function<void(BacktestState, BacktestState)> callback) {
  impl_->state_change_callback = kj::mv(callback);
}

void BacktestEngine::add_event(BacktestEvent event) {
  event.sequence = impl_->next_sequence.fetch_add(1);
  {
    auto lock = impl_->event_queue.lockExclusive();
    lock->push(kj::mv(event));
  }
  impl_->total_events.fetch_add(1);
}

size_t BacktestEngine::pending_events() const {
  auto lock = impl_->event_queue.lockShared();
  return lock->size();
}

bool BacktestEngine::step() {
  BacktestState current_state = impl_->state.load();
  if (current_state != BacktestState::Running && current_state != BacktestState::Paused &&
      current_state != BacktestState::Initialized) {
    return false;
  }

  // If initialized, transition to running for step mode
  if (current_state == BacktestState::Initialized) {
    load_events_from_data_source();
    transition_state(BacktestState::Paused); // Step mode uses paused state
  }

  return process_single_event();
}

void BacktestEngine::transition_state(BacktestState new_state) {
  BacktestState old_state = impl_->state.exchange(new_state);
  if (old_state != new_state) {
    impl_->logger->info(
        kj::str("State transition: ", to_string(old_state), " -> ", to_string(new_state)).cStr());

    KJ_IF_SOME(callback, impl_->state_change_callback) {
      callback(old_state, new_state);
    }
  }
}

void BacktestEngine::report_progress() {
  std::int64_t processed = impl_->events_processed.load();
  std::int64_t total = impl_->total_events.load();
  double progress_fraction = total > 0 ? static_cast<double>(processed) / total : 0.0;

  // Simple progress callback
  KJ_IF_SOME(callback, impl_->progress_callback) {
    callback(progress_fraction);
  }

  // Detailed progress callback
  KJ_IF_SOME(callback, impl_->progress_detailed_callback) {
    BacktestProgress progress;
    progress.progress_fraction = progress_fraction;
    progress.events_processed = processed;
    progress.total_events = total;
    progress.current_time_ns = impl_->clock.now_ns();
    progress.elapsed_real_ns = wall_clock_ns() - impl_->real_start_time_ns;
    progress.events_per_second = progress.elapsed_real_ns > 0 ? static_cast<double>(processed) *
                                                                    1e9 / progress.elapsed_real_ns
                                                              : 0.0;
    progress.state = impl_->state.load();
    progress.message = kj::str("");
    callback(progress);
  }
}

} // namespace veloz::backtest
