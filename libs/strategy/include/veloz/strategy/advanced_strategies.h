#pragma once

#include "veloz/strategy/strategy.h"
#include <functional>

namespace veloz::strategy {

    // Advanced strategy types
    enum class AdvancedStrategyType {
        // Technical indicator based strategies
        RsiStrategy,
        MacdStrategy,
        BollingerBandsStrategy,
        StochasticOscillatorStrategy,
        IchimokuStrategy,

        // Arbitrage strategies
        CrossExchangeArbitrage,
        CrossAssetArbitrage,
        TriangularArbitrage,
        StatArbitrage,

        // High-frequency trading strategies
        MarketMakingHFT,
        MomentumHFT,
        LiquidityProvidingHFT,

        // Machine learning based strategies
        ClassificationStrategy,
        RegressionStrategy,
        ReinforcementLearningStrategy,

        // Portfolio management
        MeanVarianceOptimization,
        RiskParityPortfolio,
        HierarchicalRiskParity
    };

    // Strategy configuration for advanced strategies
    struct AdvancedStrategyConfig : public StrategyConfig {
        AdvancedStrategyType advanced_type;
        std::map<std::string, std::string> ml_model_config; // For ML strategies
        std::vector<std::string> target_symbols; // For arbitrage strategies
        std::vector<std::string> venues; // For cross-exchange strategies
    };

    // Base class for technical indicator based strategies
    class TechnicalIndicatorStrategy : public BaseStrategy {
    public:
        explicit TechnicalIndicatorStrategy(const StrategyConfig& config)
            : BaseStrategy(config) {}

        // Common technical indicator calculations
        double calculate_rsi(const std::vector<double>& prices, int period = 14) const;
        double calculate_macd(const std::vector<double>& prices, double& signal, int fast_period = 12, int slow_period = 26, int signal_period = 9) const;
        void calculate_bollinger_bands(const std::vector<double>& prices, double& upper, double& middle, double& lower, int period = 20, double std_dev = 2.0) const;
        void calculate_stochastic_oscillator(const std::vector<double>& prices, double& k, double& d, int k_period = 14, int d_period = 3) const;
        double calculate_exponential_moving_average(const std::vector<double>& prices, int period) const;
        double calculate_standard_deviation(const std::vector<double>& prices) const;
    };

    // RSI (Relative Strength Index) strategy
    class RsiStrategy : public TechnicalIndicatorStrategy {
    public:
        explicit RsiStrategy(const StrategyConfig& config);
        StrategyType get_type() const override;
        void on_event(const market::MarketEvent& event) override;
        void on_timer(int64_t timestamp) override;
        std::vector<exec::PlaceOrderRequest> get_signals() override;
        static std::string get_strategy_type();

    private:
        std::vector<double> recent_prices_;
        std::vector<exec::PlaceOrderRequest> signals_;
        int rsi_period_;
        double overbought_level_;
        double oversold_level_;
    };

    // MACD (Moving Average Convergence Divergence) strategy
    class MacdStrategy : public TechnicalIndicatorStrategy {
    public:
        explicit MacdStrategy(const StrategyConfig& config);
        StrategyType get_type() const override;
        void on_event(const market::MarketEvent& event) override;
        void on_timer(int64_t timestamp) override;
        std::vector<exec::PlaceOrderRequest> get_signals() override;
        static std::string get_strategy_type();

    private:
        std::vector<double> recent_prices_;
        std::vector<exec::PlaceOrderRequest> signals_;
        int fast_period_;
        int slow_period_;
        int signal_period_;
    };

    // Bollinger Bands strategy
    class BollingerBandsStrategy : public TechnicalIndicatorStrategy {
    public:
        explicit BollingerBandsStrategy(const StrategyConfig& config);
        StrategyType get_type() const override;
        void on_event(const market::MarketEvent& event) override;
        void on_timer(int64_t timestamp) override;
        std::vector<exec::PlaceOrderRequest> get_signals() override;
        static std::string get_strategy_type();

    private:
        std::vector<double> recent_prices_;
        std::vector<exec::PlaceOrderRequest> signals_;
        int period_;
        double std_dev_;
    };

    // Stochastic Oscillator strategy
    class StochasticOscillatorStrategy : public TechnicalIndicatorStrategy {
    public:
        explicit StochasticOscillatorStrategy(const StrategyConfig& config);
        StrategyType get_type() const override;
        void on_event(const market::MarketEvent& event) override;
        void on_timer(int64_t timestamp) override;
        std::vector<exec::PlaceOrderRequest> get_signals() override;
        static std::string get_strategy_type();

    private:
        std::vector<double> recent_prices_;
        std::vector<exec::PlaceOrderRequest> signals_;
        int k_period_;
        int d_period_;
        double overbought_level_;
        double oversold_level_;
    };

    // Market making high-frequency trading strategy
    class MarketMakingHFTStrategy : public BaseStrategy {
    public:
        explicit MarketMakingHFTStrategy(const StrategyConfig& config);
        StrategyType get_type() const override;
        void on_event(const market::MarketEvent& event) override;
        void on_timer(int64_t timestamp) override;
        std::vector<exec::PlaceOrderRequest> get_signals() override;
        static std::string get_strategy_type();

    private:
        std::vector<exec::PlaceOrderRequest> signals_;
        double spread_;
        double order_size_;
        int refresh_rate_ms_;
        int64_t last_order_time_;
    };

    // Cross-exchange arbitrage strategy
    class CrossExchangeArbitrageStrategy : public BaseStrategy {
    public:
        explicit CrossExchangeArbitrageStrategy(const StrategyConfig& config);
        StrategyType get_type() const override;
        void on_event(const market::MarketEvent& event) override;
        void on_timer(int64_t timestamp) override;
        std::vector<exec::PlaceOrderRequest> get_signals() override;
        static std::string get_strategy_type();

    private:
        std::vector<exec::PlaceOrderRequest> signals_;
        std::map<veloz::common::Venue, double> prices_by_venue_;
        double min_profit_;
        double max_slippage_;
    };

    // Strategy portfolio management
    class StrategyPortfolioManager {
    public:
        StrategyPortfolioManager() = default;
        ~StrategyPortfolioManager() = default;

        void add_strategy(const std::shared_ptr<IStrategy>& strategy, double weight);
        void remove_strategy(const std::string& strategy_id);
        void update_strategy_weight(const std::string& strategy_id, double weight);
        std::vector<exec::PlaceOrderRequest> get_combined_signals();
        StrategyState get_portfolio_state() const;
        void set_risk_model(const std::function<double(const std::vector<double>&)>& risk_model);
        void rebalance_portfolio();

    private:
        struct StrategyWeight {
            std::shared_ptr<IStrategy> strategy;
            double weight;
        };

        std::map<std::string, StrategyWeight> strategies_;
        std::function<double(const std::vector<double>&)> risk_model_;
        double total_exposure_;
    };

    // Strategy factories for advanced strategies
    class RsiStrategyFactory : public StrategyFactory<RsiStrategy> {
    public:
        std::string get_strategy_type() const override;
    };

    class MacdStrategyFactory : public StrategyFactory<MacdStrategy> {
    public:
        std::string get_strategy_type() const override;
    };

    class BollingerBandsStrategyFactory : public StrategyFactory<BollingerBandsStrategy> {
    public:
        std::string get_strategy_type() const override;
    };

    class StochasticOscillatorStrategyFactory : public StrategyFactory<StochasticOscillatorStrategy> {
    public:
        std::string get_strategy_type() const override;
    };

    class MarketMakingHFTStrategyFactory : public StrategyFactory<MarketMakingHFTStrategy> {
    public:
        std::string get_strategy_type() const override;
    };

    class CrossExchangeArbitrageStrategyFactory : public StrategyFactory<CrossExchangeArbitrageStrategy> {
    public:
        std::string get_strategy_type() const override;
    };

} // namespace veloz::strategy
