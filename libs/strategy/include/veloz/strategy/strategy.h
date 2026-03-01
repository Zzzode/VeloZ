/**
 * @file strategy.h
 * @brief Core interfaces and implementations for the strategy management module
 *
 * This file contains the core interfaces and implementations for the strategy management
 * module in the VeloZ quantitative trading framework, including strategy type definitions,
 * strategy configuration, strategy state, strategy interface, strategy factory,
 * strategy manager, and base strategy implementation.
 */

#pragma once

#include "veloz/core/logger.h"
#include "veloz/exec/order_api.h"
#include "veloz/market/market_event.h"
#include "veloz/oms/position.h"

// std includes with justifications
#include <atomic> // std::atomic for lightweight counters (KJ MutexGuarded has overhead)
#include <cstdint>

// KJ library includes
#include <kj/async.h>
#include <kj/common.h>
#include <kj/function.h>
#include <kj/map.h>
#include <kj/memory.h>
#include <kj/mutex.h>
#include <kj/refcount.h>
#include <kj/string.h>
#include <kj/vector.h>

namespace veloz::strategy {

/**
 * @brief Strategy type enumeration
 *
 * Defines the strategy types supported by the framework, including common quantitative trading
 * strategy types.
 */
enum class StrategyType {
  TrendFollowing, ///< Trend following strategy
  MeanReversion,  ///< Mean reversion strategy
  Momentum,       ///< Momentum strategy
  Arbitrage,      ///< Arbitrage strategy
  MarketMaking,   ///< Market making strategy
  Grid,           ///< Grid strategy
  Custom          ///< Custom strategy
};

/**
 * @brief Strategy lifecycle status enumeration
 *
 * Defines the lifecycle states of a strategy instance.
 */
enum class StrategyStatus {
  Created, ///< Strategy created but not started
  Running, ///< Strategy is actively running
  Paused,  ///< Strategy is paused (not processing events, but state retained)
  Stopped, ///< Strategy has been stopped
  Error    ///< Strategy encountered an error
};

/**
 * @brief Strategy configuration parameters
 *
 * Contains basic configuration information for strategies, such as strategy name, type,
 * risk parameters, trading parameters, and custom parameters.
 */
struct StrategyConfig {
  kj::String name;                            ///< Strategy name
  StrategyType type;                          ///< Strategy type
  double risk_per_trade;                      ///< Risk per trade ratio (0-1)
  double max_position_size;                   ///< Maximum position size
  double stop_loss;                           ///< Stop loss ratio (0-1)
  double take_profit;                         ///< Take profit ratio (0-1)
  kj::Vector<kj::String> symbols;             ///< List of trading symbols
  kj::TreeMap<kj::String, double> parameters; ///< Strategy parameters (ordered by key)
};

/**
 * @brief Strategy running state
 *
 * Contains strategy runtime state information, such as strategy ID, name, running status,
 * profit and loss, maximum drawdown, trading statistics, etc.
 */
struct StrategyState {
  kj::String strategy_id;   ///< Strategy ID
  kj::String strategy_name; ///< Strategy name
  StrategyStatus status;    ///< Current status of the strategy
  bool is_running;          ///< Whether the strategy is running (legacy, for compatibility)
  double pnl;               ///< Cumulative profit and loss
  double total_pnl;         ///< Total profit and loss
  double max_drawdown;      ///< Maximum drawdown
  int trade_count;          ///< Number of trades
  int win_count;            ///< Number of winning trades
  int lose_count;           ///< Number of losing trades
  double win_rate;          ///< Win rate
  double profit_factor;     ///< Profit factor
};

/**
 * @brief Strategy performance metrics
 *
 * Contains performance metrics for strategy execution monitoring.
 */
struct StrategyMetrics {
  std::atomic<uint64_t> events_processed{0};      ///< Total events processed
  std::atomic<uint64_t> signals_generated{0};     ///< Total signals generated
  std::atomic<uint64_t> execution_time_ns{0};     ///< Total execution time (nanoseconds)
  std::atomic<uint64_t> max_execution_time_ns{0}; ///< Max single execution time (nanoseconds)
  std::atomic<uint64_t> last_event_time_ns{0};    ///< Timestamp of last event processed
  std::atomic<uint64_t> errors{0};                ///< Total errors encountered

  void reset() {
    events_processed.store(0);
    signals_generated.store(0);
    execution_time_ns.store(0);
    max_execution_time_ns.store(0);
    last_event_time_ns.store(0);
    errors.store(0);
  }

  double avg_execution_time_us() const {
    uint64_t count = events_processed.load();
    if (count == 0)
      return 0.0;
    return static_cast<double>(execution_time_ns.load()) / count / 1000.0;
  }

  double signals_per_second() const {
    uint64_t time_ns = execution_time_ns.load();
    if (time_ns == 0)
      return 0.0;
    return static_cast<double>(signals_generated.load()) * 1e9 / time_ns;
  }
};

/**
 * @brief Strategy interface
 *
 * Interface that all strategy classes must implement, containing methods for strategy
 * lifecycle management, event handling, state management, and signal generation.
 *
 * Inherits from kj::Refcounted to support kj::Rc<IStrategy> reference counting.
 * Note: kj::Rc is single-threaded; use kj::Arc for thread-safe sharing.
 */
class IStrategy : public kj::Refcounted {
public:
  /**
   * @brief Virtual destructor
   */
  ~IStrategy() noexcept(false) override = default;

  /**
   * @brief Get strategy ID
   * @return Strategy ID string
   */
  virtual kj::StringPtr get_id() const = 0;

  /**
   * @brief Get strategy name
   * @return Strategy name string
   */
  virtual kj::StringPtr get_name() const = 0;

  /**
   * @brief Get strategy type
   * @return Strategy type enum value
   */
  virtual StrategyType get_type() const = 0;

  /**
   * @brief Initialize strategy
   * @param config Strategy configuration parameters
   * @param logger Logger instance
   * @return Whether initialization was successful
   */
  virtual bool initialize(const StrategyConfig& config, veloz::core::Logger& logger) = 0;

  /**
   * @brief Start strategy
   */
  virtual void on_start() = 0;

  /**
   * @brief Stop strategy
   */
  virtual void on_stop() = 0;

  /**
   * @brief Pause strategy
   *
   * Pause the strategy without losing its state. The strategy will stop
   * processing new events but retains its internal state for resuming.
   */
  virtual void on_pause() = 0;

  /**
   * @brief Resume strategy
   *
   * Resume a paused strategy, allowing it to process events again.
   */
  virtual void on_resume() = 0;

  /**
   * @brief Handle market event
   * @param event Market event object
   */
  virtual void on_event(const veloz::market::MarketEvent& event) = 0;

  /**
   * @brief Handle position update
   * @param position Position information object
   */
  virtual void on_position_update(const veloz::oms::Position& position) = 0;

  /**
   * @brief Handle timer event
   * @param timestamp Timestamp in milliseconds
   */
  virtual void on_timer(int64_t timestamp) = 0;

  /**
   * @brief Get strategy state
   * @return Strategy state object
   */
  virtual StrategyState get_state() const = 0;

  /**
   * @brief Get trading signals
   * @return List of trading signals
   */
  virtual kj::Vector<veloz::exec::PlaceOrderRequest> get_signals() = 0;

  /**
   * @brief Reset strategy state
   */
  virtual void reset() = 0;

  // Runtime integration methods (optional - default implementations provided)

  /**
   * @brief Update strategy parameters at runtime (hot-reload)
   * @param parameters New parameter values
   * @return Whether update was successful
   */
  virtual bool update_parameters(const kj::TreeMap<kj::String, double>& parameters) = 0;

  /**
   * @brief Check if strategy supports hot-reload of parameters
   * @return Whether hot-reload is supported
   */
  virtual bool supports_hot_reload() const = 0;

  /**
   * @brief Get strategy performance metrics
   * @return Maybe to metrics (none if not tracked)
   */
  virtual kj::Maybe<const StrategyMetrics&> get_metrics() const = 0;

  /**
   * @brief Handle order rejection from risk engine
   * @param req The rejected order request
   * @param reason Rejection reason description
   *
   * Called when an order generated by this strategy is rejected by the risk engine.
   * Strategies can override this to adjust behavior (e.g., reduce position sizing,
   * cancel related quotes for market making).
   */
  virtual void on_order_rejected(const veloz::exec::PlaceOrderRequest& req,
                                 kj::StringPtr reason) = 0;
};

/**
 * @brief Strategy factory interface
 *
 * Strategy factory is used to create strategy instances, implementing strategy decoupling and
 * dynamic creation.
 *
 * Inherits from kj::Refcounted to support kj::Rc<IStrategyFactory> reference counting.
 */
class IStrategyFactory : public kj::Refcounted {
public:
  /**
   * @brief Virtual destructor
   */
  ~IStrategyFactory() noexcept(false) override = default;

  /**
   * @brief Create strategy instance
   * @param config Strategy configuration parameters
   * @return kj::Rc reference to strategy instance
   */
  virtual kj::Rc<IStrategy> create_strategy(const StrategyConfig& config) = 0;

  /**
   * @brief Get strategy type name
   * @return Strategy type name string
   */
  virtual kj::StringPtr get_strategy_type() const = 0;
};

/**
 * @brief Strategy manager
 *
 * Strategy manager is responsible for strategy registration, creation, starting, stopping, and
 * event dispatching.
 */
class StrategyManager {
public:
  /**
   * @brief Constructor
   */
  StrategyManager();

  /**
   * @brief Destructor
   */
  ~StrategyManager();

  /**
   * @brief Register strategy factory
   * @param factory kj::Rc reference to strategy factory
   */
  void register_strategy_factory(kj::Rc<IStrategyFactory> factory);

  /**
   * @brief Create strategy instance
   * @param config Strategy configuration parameters
   * @return kj::Rc reference to strategy instance
   */
  kj::Rc<IStrategy> create_strategy(const StrategyConfig& config);

  /**
   * @brief Start strategy
   * @param strategy_id Strategy ID
   * @return Whether start was successful
   */
  bool start_strategy(kj::StringPtr strategy_id);

  /**
   * @brief Stop strategy
   * @param strategy_id Strategy ID
   * @return Whether stop was successful
   */
  bool stop_strategy(kj::StringPtr strategy_id);

  /**
   * @brief Pause strategy
   * @param strategy_id Strategy ID
   * @return Whether pause was successful
   */
  bool pause_strategy(kj::StringPtr strategy_id);

  /**
   * @brief Resume strategy
   * @param strategy_id Strategy ID
   * @return Whether resume was successful
   */
  bool resume_strategy(kj::StringPtr strategy_id);

  /**
   * @brief Remove strategy
   * @param strategy_id Strategy ID
   * @return Whether removal was successful
   */
  bool remove_strategy(kj::StringPtr strategy_id);

  /**
   * @brief Get strategy instance
   * @param strategy_id Strategy ID
   * @return kj::Rc reference to strategy instance (null if not found)
   */
  kj::Rc<IStrategy> get_strategy(kj::StringPtr strategy_id);

  /**
   * @brief Get all strategy states
   * @return List of strategy states
   */
  kj::Vector<StrategyState> get_all_strategy_states() const;

  /**
   * @brief Get all strategy IDs
   * @return List of strategy IDs
   */
  kj::Vector<kj::String> get_all_strategy_ids() const;

  /**
   * @brief Dispatch market event
   * @param event Market event object
   */
  void on_market_event(const veloz::market::MarketEvent& event);

  /**
   * @brief Dispatch position update event
   * @param position Position information object
   */
  void on_position_update(const veloz::oms::Position& position);

  /**
   * @brief Dispatch timer event
   * @param timestamp Timestamp in milliseconds
   */
  void on_timer(int64_t timestamp);

  /**
   * @brief Get all trading signals from all strategies
   * @return List of trading signals
   */
  kj::Vector<veloz::exec::PlaceOrderRequest> get_all_signals();

  // Runtime integration methods

  /**
   * @brief Load a strategy at runtime
   * @param config Strategy configuration
   * @param logger Logger instance
   * @return Strategy ID if successful, empty string otherwise
   */
  kj::String load_strategy(const StrategyConfig& config, veloz::core::Logger& logger);

  /**
   * @brief Unload a strategy at runtime
   * @param strategy_id Strategy ID
   * @return Whether unload was successful
   */
  bool unload_strategy(kj::StringPtr strategy_id);

  /**
   * @brief Hot-reload strategy parameters
   * @param strategy_id Strategy ID
   * @param parameters New parameter values
   * @return Whether reload was successful
   */
  bool reload_parameters(kj::StringPtr strategy_id,
                         const kj::TreeMap<kj::String, double>& parameters);

  /**
   * @brief Set signal callback for routing signals to engine
   * @param callback Function called when strategies generate signals
   */
  void set_signal_callback(
      kj::Function<void(const kj::Vector<veloz::exec::PlaceOrderRequest>&)> callback);

  /**
   * @brief Process signals from all strategies and route to callback
   */
  void process_and_route_signals();

  /**
   * @brief Get aggregated metrics for all strategies
   * @return Combined metrics string
   */
  kj::String get_metrics_summary() const;

  /**
   * @brief Get metrics for a specific strategy
   * @param strategy_id Strategy ID
   * @return Maybe to metrics (none if not found)
   */
  kj::Maybe<const StrategyMetrics&> get_strategy_metrics(kj::StringPtr strategy_id) const;

  /**
   * @brief Check if a strategy is loaded
   * @param strategy_id Strategy ID
   * @return Whether strategy is loaded
   */
  bool is_strategy_loaded(kj::StringPtr strategy_id) const;

  /**
   * @brief Get count of loaded strategies
   * @return Number of loaded strategies
   */
  size_t strategy_count() const;

  /**
   * @brief Route order rejection to originating strategy
   * @param req The rejected order request (must contain strategy_id)
   * @param reason Rejection reason description
   *
   * Called by risk engine when an order is rejected. Routes the rejection
   * to the originating strategy so it can adjust behavior.
   */
  void on_order_rejected(const veloz::exec::PlaceOrderRequest& req, kj::StringPtr reason);

  /**
   * @brief Save all strategy states
   *
   * Called by persistence layer to save current state of all strategies.
   * @return true if all states saved successfully
   */
  bool save_all_states();

  /**
   * @brief Restore a strategy from saved state
   * @param strategy_id Strategy ID
   * @param state Saved strategy state
   * @return true if restoration succeeded
   */
  bool restore_state(kj::StringPtr strategy_id, const StrategyState& state);

  /**
   * @brief Get strategy config for loading
   * @param strategy_id Strategy ID
   * @return Strategy config if found
   */
  kj::Maybe<StrategyConfig> get_strategy_config(kj::StringPtr strategy_id) const;

private:
  // Thread-safe state using KJ mutex
  // kj::HashMap for fast string key lookup (unordered)
  // kj::Rc for reference-counted strategies and factories
  struct State {
    kj::HashMap<kj::String, kj::Rc<IStrategy>> strategies;
    kj::HashMap<kj::String, kj::Rc<IStrategyFactory>> factories;
    kj::Maybe<kj::Function<void(const kj::Vector<veloz::exec::PlaceOrderRequest>&)>>
        signal_callback;
    kj::HashMap<kj::String, StrategyConfig> configs; ///< Strategy configs for recovery
  };
  kj::MutexGuarded<State> state_;
  kj::Own<veloz::core::Logger> logger_;

  // Legacy accessors for backward compatibility (will be removed)
  // kj::HashMap for fast string key lookup
  // kj::Rc for reference-counted strategies and factories
  kj::HashMap<kj::String, kj::Rc<IStrategy>> strategies_;       ///< Strategy instance map
  kj::HashMap<kj::String, kj::Rc<IStrategyFactory>> factories_; ///< Strategy factory map
};

/**
 * @brief Base strategy implementation
 *
 * Provides a base implementation of the strategy interface, containing common strategy
 * management functionality such as strategy ID generation, state management, initialization, and
 * stopping.
 */
class BaseStrategy : public IStrategy {
public:
  /**
   * @brief Constructor
   * @param config Strategy configuration parameters
   */
  explicit BaseStrategy(const StrategyConfig& config)
      : config_(kj::mv(const_cast<StrategyConfig&>(config))),
        strategy_id_(generate_strategy_id(config_)) {} // logger_ defaults to kj::none

  /**
   * @brief Virtual destructor
   */
  ~BaseStrategy() noexcept override = default;

  /**
   * @brief Get strategy ID
   * @return Strategy ID string
   */
  kj::StringPtr get_id() const override {
    return strategy_id_;
  }

  /**
   * @brief Get strategy name
   * @return Strategy name string
   */
  kj::StringPtr get_name() const override {
    return config_.name;
  }

  /**
   * @brief Initialize strategy
   * @param config Strategy configuration parameters
   * @param logger Logger instance
   * @return Whether initialization was successful
   */
  bool initialize(const StrategyConfig& config, core::Logger& logger) override {
    logger_ = logger; // kj::Maybe<T&> stores reference, doesn't copy
    KJ_IF_SOME(l, logger_) {
      l.info(kj::str("Strategy ", config.name, " initialized").cStr());
    }
    initialized_ = true;
    return true;
  }

  /**
   * @brief Start strategy
   */
  void on_start() override {
    running_ = true;
  }

  /**
   * @brief Stop strategy
   */
  void on_stop() override {
    running_ = false;
    status_ = StrategyStatus::Stopped;
  }

  /**
   * @brief Pause strategy
   */
  void on_pause() override {
    running_ = false;
    status_ = StrategyStatus::Paused;
  }

  /**
   * @brief Resume strategy
   */
  void on_resume() override {
    running_ = true;
    status_ = StrategyStatus::Running;
  }

  /**
   * @brief Handle position update
   * @param position Position information object
   */
  void on_position_update(const oms::Position& position) override {
    current_position_ = oms::Position{position};
  }

  /**
   * @brief Get strategy state
   * @return Strategy state object
   */
  StrategyState get_state() const override {
    StrategyState state;
    state.strategy_id = kj::str(get_id());
    state.strategy_name = kj::str(get_name());
    state.status = status_;
    state.is_running = running_;
    state.pnl = current_pnl_;
    state.max_drawdown = max_drawdown_;
    state.trade_count = trade_count_;
    state.win_count = win_count_;
    state.lose_count = lose_count_;
    state.win_rate = trade_count_ > 0 ? static_cast<double>(win_count_) / trade_count_ : 0.0;
    state.profit_factor = total_profit_ > 0 ? total_profit_ / std::abs(total_loss_) : 0.0;
    return state;
  }

  /**
   * @brief Reset strategy state
   */
  void reset() override {
    current_pnl_ = 0.0;
    max_drawdown_ = 0.0;
    trade_count_ = 0;
    win_count_ = 0;
    lose_count_ = 0;
    total_profit_ = 0.0;
    total_loss_ = 0.0;
    current_position_ = oms::Position{};
  }

  // Runtime integration methods (default implementations)

  bool update_parameters(const kj::TreeMap<kj::String, double>& parameters) override {
    (void)parameters;
    return false; // Default: not supported
  }

  bool supports_hot_reload() const override {
    return false;
  }

  kj::Maybe<const StrategyMetrics&> get_metrics() const override {
    return kj::none;
  }

  void on_order_rejected(const veloz::exec::PlaceOrderRequest& req, kj::StringPtr reason) override {
    (void)req;
    (void)reason;
    // Default: no-op
  }

protected:
  /**
   * @brief Generate strategy ID
   * @param config Strategy configuration parameters
   * @return Strategy ID string
   */
  static kj::String generate_strategy_id(const StrategyConfig& config) {
    static int counter = 0;
    return kj::str(config.name, "_", ++counter);
  }

  StrategyConfig config_;                          ///< Strategy configuration parameters
  kj::String strategy_id_;                         ///< Strategy ID
  kj::Maybe<veloz::core::Logger&> logger_;         ///< Optional logger reference (non-owning)
  bool initialized_{false};                        ///< Whether initialized
  bool running_{false};                            ///< Whether running
  StrategyStatus status_{StrategyStatus::Created}; ///< Current status
  double current_pnl_{0.0};                        ///< Current profit and loss
  double max_drawdown_{0.0};                       ///< Maximum drawdown
  int trade_count_{0};                             ///< Number of trades
  int win_count_{0};                               ///< Number of winning trades
  int lose_count_{0};                              ///< Number of losing trades
  double total_profit_{0.0};                       ///< Total profit
  double total_loss_{0.0};                         ///< Total loss
  oms::Position current_position_;                 ///< Current position
};

/**
 * @brief Strategy factory template class
 *
 * Template class that implements the strategy factory interface, used to create specific types of
 * strategy instances.
 * @tparam StrategyImpl Strategy implementation class
 */
template <typename StrategyImpl> class StrategyFactory : public IStrategyFactory {
public:
  /**
   * @brief Create strategy instance
   * @param config Strategy configuration parameters
   * @return kj::Rc reference to strategy instance
   */
  kj::Rc<IStrategy> create_strategy(const StrategyConfig& config) override {
    return kj::rc<StrategyImpl>(config);
  }

  /**
   * @brief Get strategy type name
   * @return Strategy type name string
   */
  kj::StringPtr get_strategy_type() const override {
    return StrategyImpl::get_strategy_type();
  }
};

} // namespace veloz::strategy
