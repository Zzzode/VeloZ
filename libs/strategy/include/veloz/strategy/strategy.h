/**
 * @file strategy.h
 * @brief 策略管理模块的核心接口和实现
 *
 * 该文件包含了 VeloZ 量化交易框架中策略管理模块的核心接口和实现，
 * 包括策略类型定义、策略配置、策略状态、策略接口、策略工厂、
 * 策略管理器以及基础策略实现等。
 */

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

    /**
     * @brief 策略类型枚举
     *
     * 定义了框架支持的策略类型，包括常见的量化交易策略类型。
     */
    enum class StrategyType {
        TrendFollowing,   ///< 趋势跟踪策略
        MeanReversion,    ///< 均值回归策略
        Momentum,         ///< 动量策略
        Arbitrage,        ///< 套利策略
        MarketMaking,     ///< 做市策略
        Grid,             ///< 网格策略
        Custom            ///< 自定义策略
    };

    /**
     * @brief 策略配置参数
     *
     * 包含策略的基本配置信息，如策略名称、类型、风险参数、
     * 交易参数和自定义参数等。
     */
    struct StrategyConfig {
        std::string name;                      ///< 策略名称
        StrategyType type;                     ///< 策略类型
        double risk_per_trade;                 ///< 单笔交易风险比例 (0-1)
        double max_position_size;              ///< 最大持仓大小
        double stop_loss;                      ///< 止损比例 (0-1)
        double take_profit;                    ///< 止盈比例 (0-1)
        std::vector<std::string> symbols;      ///< 交易标的列表
        std::map<std::string, double> parameters; ///< 策略参数
    };

    /**
     * @brief 策略运行状态
     *
     * 包含策略的运行状态信息，如策略ID、名称、运行状态、
     * 盈亏情况、最大回撤、交易统计等。
     */
    struct StrategyState {
        std::string strategy_id;               ///< 策略ID
        std::string strategy_name;             ///< 策略名称
        bool is_running;                       ///< 是否正在运行
        double pnl;                            ///< 累计盈亏
        double total_pnl;                      ///< 总盈亏
        double max_drawdown;                   ///< 最大回撤
        int trade_count;                       ///< 交易次数
        int win_count;                         ///< 盈利交易次数
        int lose_count;                        ///< 亏损交易次数
        double win_rate;                       ///< 胜率
        double profit_factor;                  ///< 盈亏比
    };

    /**
     * @brief 策略接口
     *
     * 所有策略类必须实现的接口，包含策略生命周期管理、
     * 事件处理、状态管理和信号生成等方法。
     */
    class IStrategy {
    public:
        /**
         * @brief 虚析构函数
         */
        virtual ~IStrategy() = default;

        /**
         * @brief 获取策略ID
         * @return 策略ID字符串
         */
        virtual std::string get_id() const = 0;

        /**
         * @brief 获取策略名称
         * @return 策略名称字符串
         */
        virtual std::string get_name() const = 0;

        /**
         * @brief 获取策略类型
         * @return 策略类型枚举值
         */
        virtual StrategyType get_type() const = 0;

        /**
         * @brief 初始化策略
         * @param config 策略配置参数
         * @param logger 日志记录器
         * @return 初始化是否成功
         */
        virtual bool initialize(const StrategyConfig& config, veloz::core::Logger& logger) = 0;

        /**
         * @brief 策略启动
         */
        virtual void on_start() = 0;

        /**
         * @brief 策略停止
         */
        virtual void on_stop() = 0;

        /**
         * @brief 处理市场事件
         * @param event 市场事件对象
         */
        virtual void on_event(const veloz::market::MarketEvent& event) = 0;

        /**
         * @brief 处理持仓更新
         * @param position 持仓信息对象
         */
        virtual void on_position_update(const veloz::oms::Position& position) = 0;

        /**
         * @brief 处理定时器事件
         * @param timestamp 时间戳（毫秒）
         */
        virtual void on_timer(int64_t timestamp) = 0;

        /**
         * @brief 获取策略状态
         * @return 策略状态对象
         */
        virtual StrategyState get_state() const = 0;

        /**
         * @brief 获取交易信号
         * @return 交易信号列表
         */
        virtual std::vector<veloz::exec::PlaceOrderRequest> get_signals() = 0;

        /**
         * @brief 重置策略状态
         */
        virtual void reset() = 0;
    };

    /**
     * @brief 策略工厂接口
     *
     * 策略工厂用于创建策略实例，实现了策略的解耦和动态创建。
     */
    class IStrategyFactory {
    public:
        /**
         * @brief 虚析构函数
         */
        virtual ~IStrategyFactory() = default;

        /**
         * @brief 创建策略实例
         * @param config 策略配置参数
         * @return 策略实例的共享指针
         */
        virtual std::shared_ptr<IStrategy> create_strategy(const StrategyConfig& config) = 0;

        /**
         * @brief 获取策略类型名称
         * @return 策略类型名称字符串
         */
        virtual std::string get_strategy_type() const = 0;
    };

    /**
     * @brief 策略管理器
     *
     * 策略管理器负责策略的注册、创建、启动、停止和事件分发等管理工作。
     */
    class StrategyManager {
    public:
        /**
         * @brief 构造函数
         */
        StrategyManager();

        /**
         * @brief 析构函数
         */
        ~StrategyManager();

        /**
         * @brief 注册策略工厂
         * @param factory 策略工厂的共享指针
         */
        void register_strategy_factory(const std::shared_ptr<IStrategyFactory>& factory);

        /**
         * @brief 创建策略实例
         * @param config 策略配置参数
         * @return 策略实例的共享指针
         */
        std::shared_ptr<IStrategy> create_strategy(const StrategyConfig& config);

        /**
         * @brief 启动策略
         * @param strategy_id 策略ID
         * @return 启动是否成功
         */
        bool start_strategy(const std::string& strategy_id);

        /**
         * @brief 停止策略
         * @param strategy_id 策略ID
         * @return 停止是否成功
         */
        bool stop_strategy(const std::string& strategy_id);

        /**
         * @brief 移除策略
         * @param strategy_id 策略ID
         * @return 移除是否成功
         */
        bool remove_strategy(const std::string& strategy_id);

        /**
         * @brief 获取策略实例
         * @param strategy_id 策略ID
         * @return 策略实例的共享指针
         */
        std::shared_ptr<IStrategy> get_strategy(const std::string& strategy_id) const;

        /**
         * @brief 获取所有策略的状态
         * @return 策略状态列表
         */
        std::vector<StrategyState> get_all_strategy_states() const;

        /**
         * @brief 获取所有策略的ID
         * @return 策略ID列表
         */
        std::vector<std::string> get_all_strategy_ids() const;

        /**
         * @brief 分发市场事件
         * @param event 市场事件对象
         */
        void on_market_event(const veloz::market::MarketEvent& event);

        /**
         * @brief 分发持仓更新事件
         * @param position 持仓信息对象
         */
        void on_position_update(const veloz::oms::Position& position);

        /**
         * @brief 分发定时器事件
         * @param timestamp 时间戳（毫秒）
         */
        void on_timer(int64_t timestamp);

        /**
         * @brief 获取所有策略的交易信号
         * @return 交易信号列表
         */
        std::vector<veloz::exec::PlaceOrderRequest> get_all_signals();

    private:
        std::map<std::string, std::shared_ptr<IStrategy>> strategies_;  ///< 策略实例映射
        std::map<std::string, std::shared_ptr<IStrategyFactory>> factories_;  ///< 策略工厂映射
        std::shared_ptr<veloz::core::Logger> logger_;  ///< 日志记录器
    };

    /**
     * @brief 基础策略实现
     *
     * 提供了策略接口的基础实现，包含通用的策略管理功能，
     * 如策略ID生成、状态管理、初始化和停止等。
     */
    class BaseStrategy : public IStrategy {
    public:
        /**
         * @brief 构造函数
         * @param config 策略配置参数
         */
        explicit BaseStrategy(const StrategyConfig& config)
            : config_(config), strategy_id_(generate_strategy_id(config)), logger_ptr_(nullptr) {}

        /**
         * @brief 获取策略ID
         * @return 策略ID字符串
         */
        std::string get_id() const override {
            return strategy_id_;
        }

        /**
         * @brief 获取策略名称
         * @return 策略名称字符串
         */
        std::string get_name() const override {
            return config_.name;
        }

        /**
         * @brief 初始化策略
         * @param config 策略配置参数
         * @param logger 日志记录器
         * @return 初始化是否成功
         */
        bool initialize(const StrategyConfig& config, core::Logger& logger) override {
            logger_ptr_ = &logger;
            logger_ptr_->info(std::format("Strategy {} initialized", config.name));
            initialized_ = true;
            return true;
        }

        /**
         * @brief 策略启动
         */
        void on_start() override {
            running_ = true;
        }

        /**
         * @brief 策略停止
         */
        void on_stop() override {
            running_ = false;
        }

        /**
         * @brief 处理持仓更新
         * @param position 持仓信息对象
         */
        void on_position_update(const oms::Position& position) override {
            current_position_ = position;
        }

        /**
         * @brief 获取策略状态
         * @return 策略状态对象
         */
        StrategyState get_state() const override {
            return StrategyState{
                .strategy_id = get_id(),
                .strategy_name = get_name(),
                .is_running = running_,
                .pnl = current_pnl_,
                .max_drawdown = max_drawdown_,
                .trade_count = trade_count_,
                .win_count = win_count_,
                .lose_count = lose_count_,
                .win_rate = trade_count_ > 0 ? static_cast<double>(win_count_) / trade_count_ : 0.0,
                .profit_factor = total_profit_ > 0 ? total_profit_ / std::abs(total_loss_) : 0.0
            };
        }

        /**
         * @brief 重置策略状态
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
         * @brief 生成策略ID
         * @param config 策略配置参数
         * @return 策略ID字符串
         */
        static std::string generate_strategy_id(const StrategyConfig& config) {
            static int counter = 0;
            return config.name + "_" + std::to_string(++counter);
        }

        StrategyConfig config_;              ///< 策略配置参数
        std::string strategy_id_;           ///< 策略ID
        core::Logger* logger_ptr_;         ///< 日志记录器指针
        bool initialized_{false};          ///< 是否已初始化
        bool running_{false};              ///< 是否正在运行
        double current_pnl_{0.0};          ///< 当前盈亏
        double max_drawdown_{0.0};         ///< 最大回撤
        int trade_count_{0};               ///< 交易次数
        int win_count_{0};                 ///< 盈利交易次数
        int lose_count_{0};                ///< 亏损交易次数
        double total_profit_{0.0};         ///< 总盈利
        double total_loss_{0.0};           ///< 总亏损
        oms::Position current_position_;   ///< 当前持仓
    };

    /**
     * @brief 策略工厂模板类
     *
     * 模板类实现了策略工厂接口，用于创建特定类型的策略实例。
     * @tparam StrategyImpl 策略实现类
     */
    template <typename StrategyImpl>
    class StrategyFactory : public IStrategyFactory {
    public:
        /**
         * @brief 创建策略实例
         * @param config 策略配置参数
         * @return 策略实例的共享指针
         */
        std::shared_ptr<IStrategy> create_strategy(const StrategyConfig& config) override {
            return std::make_shared<StrategyImpl>(config);
        }

        /**
         * @brief 获取策略类型名称
         * @return 策略类型名称字符串
         */
        std::string get_strategy_type() const override {
            return StrategyImpl::get_strategy_type();
        }
    };

} // namespace veloz::strategy
