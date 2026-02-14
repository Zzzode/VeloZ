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

#include <cstdint>
#include <kj/common.h>
#include <kj/memory.h>
#include <kj/string.h>
#include <kj/vector.h>
#include <map>    // std::map for ordered key lookup
#include <memory> // std::shared_ptr for shared ownership (KJ uses kj::Own for unique ownership)

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
 * @brief Strategy configuration parameters
 *
 * Contains basic configuration information for strategies, such as strategy name, type,
 * risk parameters, trading parameters, and custom parameters.
 */
struct StrategyConfig {
  kj::String name;                         ///< Strategy name
  StrategyType type;                       ///< Strategy type
  double risk_per_trade;                   ///< Risk per trade ratio (0-1)
  double max_position_size;                ///< Maximum position size
  double stop_loss;                        ///< Stop loss ratio (0-1)
  double take_profit;                      ///< Take profit ratio (0-1)
  kj::Vector<kj::String> symbols;          ///< List of trading symbols
  std::map<kj::String, double> parameters; ///< Strategy parameters (std::map for ordered lookup)
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
  bool is_running;          ///< Whether the strategy is running
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
 * @brief Strategy interface
 *
 * Interface that all strategy classes must implement, containing methods for strategy
 * lifecycle management, event handling, state management, and signal generation.
 */
class IStrategy {
public:
  /**
   * @brief Virtual destructor
   */
  virtual ~IStrategy() = default;

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
};

/**
 * @brief Strategy factory interface
 *
 * Strategy factory is used to create strategy instances, implementing strategy decoupling and
 * dynamic creation.
 */
class IStrategyFactory {
public:
  /**
   * @brief Virtual destructor
   */
  virtual ~IStrategyFactory() = default;

  /**
   * @brief Create strategy instance
   * @param config Strategy configuration parameters
   * @return Shared pointer to strategy instance
   */
  virtual std::shared_ptr<IStrategy> create_strategy(const StrategyConfig& config) = 0;

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
   * @param factory Shared pointer to strategy factory
   */
  void register_strategy_factory(const std::shared_ptr<IStrategyFactory>& factory);

  /**
   * @brief Create strategy instance
   * @param config Strategy configuration parameters
   * @return Shared pointer to strategy instance
   */
  std::shared_ptr<IStrategy> create_strategy(const StrategyConfig& config);

  /**
   * @brief Start strategy
   * @param strategy_id Strategy ID
   * @return Whether start was successful
   */
  bool start_strategy(const std::string& strategy_id);

  /**
   * @brief Stop strategy
   * @param strategy_id Strategy ID
   * @return Whether stop was successful
   */
  bool stop_strategy(const std::string& strategy_id);

  /**
   * @brief Remove strategy
   * @param strategy_id Strategy ID
   * @return Whether removal was successful
   */
  bool remove_strategy(const std::string& strategy_id);

  /**
   * @brief Get strategy instance
   * @param strategy_id Strategy ID
   * @return Shared pointer to strategy instance
   */
  std::shared_ptr<IStrategy> get_strategy(const std::string& strategy_id) const;

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

private:
  // Using std::map with std::string keys for ordered lookup and compatibility
  std::map<std::string, std::shared_ptr<IStrategy>> strategies_;       ///< Strategy instance map
  std::map<std::string, std::shared_ptr<IStrategyFactory>> factories_; ///< Strategy factory map
  std::shared_ptr<veloz::core::Logger> logger_;                        ///< Logger instance
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
        strategy_id_(generate_strategy_id(config_)), logger_ptr_(nullptr) {}

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
    logger_ptr_ = &logger;
    logger_ptr_->info(std::string(kj::str("Strategy ", config.name, " initialized").cStr()));
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
  }

  /**
   * @brief Handle position update
   * @param position Position information object
   */
  void on_position_update(const oms::Position& position) override {
    current_position_ = position;
  }

  /**
   * @brief Get strategy state
   * @return Strategy state object
   */
  StrategyState get_state() const override {
    StrategyState state;
    state.strategy_id = kj::str(get_id());
    state.strategy_name = kj::str(get_name());
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

  StrategyConfig config_;          ///< Strategy configuration parameters
  kj::String strategy_id_;         ///< Strategy ID
  core::Logger* logger_ptr_;       ///< Logger pointer
  bool initialized_{false};        ///< Whether initialized
  bool running_{false};            ///< Whether running
  double current_pnl_{0.0};        ///< Current profit and loss
  double max_drawdown_{0.0};       ///< Maximum drawdown
  int trade_count_{0};             ///< Number of trades
  int win_count_{0};               ///< Number of winning trades
  int lose_count_{0};              ///< Number of losing trades
  double total_profit_{0.0};       ///< Total profit
  double total_loss_{0.0};         ///< Total loss
  oms::Position current_position_; ///< Current position
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
   * @return Shared pointer to strategy instance
   */
  std::shared_ptr<IStrategy> create_strategy(const StrategyConfig& config) override {
    return std::make_shared<StrategyImpl>(config);
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
