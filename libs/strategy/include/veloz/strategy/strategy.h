#pragma once

#include <string>
#include <vector>
#include <memory>
#include <map>
#include <cstdint>

#include "veloz/core/logger.h"
#include "veloz/market/market_event.h"
#include "veloz/exec/order_api.h"
#include "veloz/oms/position.h"

namespace veloz::strategy {

    // Strategy type enumeration
    enum class StrategyType {
        TrendFollowing,
        MeanReversion,
        Momentum,
        Arbitrage,
        MarketMaking,
        Grid,
        Custom
    };

    // Strategy configuration parameters
    struct StrategyConfig {
        std::string name;
        StrategyType type;
        double risk_per_trade; // Risk per trade ratio (0-1)
        double max_position_size; // Maximum position size
        double stop_loss; // Stop loss ratio (0-1)
        double take_profit; // Take profit ratio (0-1)
        std::vector<std::string> symbols; // Trading symbols
        std::map<std::string, double> parameters; // Strategy parameters
    };

    // Strategy running state
    struct StrategyState {
        std::string strategy_id;
        std::string strategy_name;
        bool is_running;
        double pnl; // Cumulative P&L
        double max_drawdown; // Maximum drawdown
        int trade_count; // Number of trades
        int win_count; // Number of winning trades
        int lose_count; // Number of losing trades
        double win_rate; // Win rate
        double profit_factor; // Profit factor
    };

    // Strategy interface
    class IStrategy {
    public:
        virtual ~IStrategy() = default;

        // Get strategy information
        virtual std::string get_id() const = 0;
        virtual std::string get_name() const = 0;
        virtual StrategyType get_type() const = 0;

        // Strategy lifecycle methods
        virtual bool initialize(const StrategyConfig& config, veloz::core::Logger& logger) = 0;
        virtual void on_start() = 0;
        virtual void on_stop() = 0;
        virtual void on_event(const veloz::market::MarketEvent& event) = 0;
        virtual void on_position_update(const veloz::oms::Position& position) = 0;
        virtual void on_timer(int64_t timestamp) = 0;

        // Get strategy state
        virtual StrategyState get_state() const = 0;

        // Get trading signals
        virtual std::vector<veloz::exec::PlaceOrderRequest> get_signals() = 0;

        // Reset strategy state
        virtual void reset() = 0;
    };

    // Strategy factory interface
    class IStrategyFactory {
    public:
        virtual ~IStrategyFactory() = default;
        virtual std::shared_ptr<IStrategy> create_strategy(const StrategyConfig& config) = 0;
        virtual std::string get_strategy_type() const = 0;
    };

    // Strategy manager
    class StrategyManager {
    public:
        StrategyManager();
        ~StrategyManager();

        // Register strategy factory
        void register_strategy_factory(const std::shared_ptr<IStrategyFactory>& factory);

        // Create strategy instance
        std::shared_ptr<IStrategy> create_strategy(const StrategyConfig& config);

        // Manage strategy lifecycle
        bool start_strategy(const std::string& strategy_id);
        bool stop_strategy(const std::string& strategy_id);
        bool remove_strategy(const std::string& strategy_id);

        // Get strategy information
        std::shared_ptr<IStrategy> get_strategy(const std::string& strategy_id) const;
        std::vector<StrategyState> get_all_strategy_states() const;
        std::vector<std::string> get_all_strategy_ids() const;

        // Event dispatch
        void on_market_event(const veloz::market::MarketEvent& event);
        void on_position_update(const veloz::oms::Position& position);
        void on_timer(int64_t timestamp);

        // Get trading signals from all strategies
        std::vector<veloz::exec::PlaceOrderRequest> get_all_signals();

    private:
        std::map<std::string, std::shared_ptr<IStrategy>> strategies_;
        std::map<std::string, std::shared_ptr<IStrategyFactory>> factories_;
        std::shared_ptr<veloz::core::Logger> logger_;
    };

    // Strategy template class
    template <typename StrategyImpl>
    class StrategyFactory : public IStrategyFactory {
    public:
        std::shared_ptr<IStrategy> create_strategy(const StrategyConfig& config) override {
            return std::make_shared<StrategyImpl>(config);
        }

        std::string get_strategy_type() const override {
            return StrategyImpl::get_strategy_type();
        }
    };

} // namespace veloz::strategy
