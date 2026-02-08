/**
 * @file risk_engine.h
 * @brief 风险管理模块的核心接口和实现
 *
 * 该文件包含了 VeloZ 量化交易框架中风险管理模块的核心接口和实现，
 * 包括风险检查、风险预警、风险指标计算和风险控制功能等。
 *
 * 风险管理系统是框架的核心组件之一，负责在交易前和交易后进行风险评估，
 * 控制风险暴露，并提供风险预警和报告功能。
 */

#pragma once

#include "veloz/exec/order_api.h"
#include "veloz/oms/position.h"
#include "veloz/risk/risk_metrics.h"

#include <string>
#include <unordered_map>
#include <chrono>

namespace veloz::risk {

/**
 * @brief 风险检查结果结构
 *
 * 包含风险检查的结果信息，包括是否允许交易和拒绝原因。
 */
struct RiskCheckResult {
  bool allowed{true};      ///< 是否允许交易
  std::string reason;      ///< 拒绝交易的原因（如果不允许）
};

/**
 * @brief 风险预警级别枚举
 *
 * 定义了风险预警的不同级别，从低风险到临界风险。
 */
enum class RiskLevel {
  Low,     ///< 低风险
  Medium,  ///< 中等风险
  High,    ///< 高风险
  Critical ///< 临界风险
};

/**
 * @brief 风险预警信息结构
 *
 * 包含风险预警的详细信息，包括预警级别、消息、时间戳和交易品种。
 */
struct RiskAlert {
  RiskLevel level;                                   ///< 风险预警级别
  std::string message;                               ///< 预警消息
  std::chrono::steady_clock::time_point timestamp;   ///< 预警时间戳
  std::string symbol;                                ///< 关联的交易品种
};

/**
 * @brief 风险引擎类
 *
 * 负责管理和评估交易风险，包括交易前检查、交易后检查、
 * 风险预警、风险指标计算和风险控制功能。
 */
class RiskEngine final {
public:
  /**
   * @brief 构造函数
   */
  RiskEngine() = default;

  // Pre-trade checks
  /**
   * @brief 交易前风险检查
   * @param req 下单请求
   * @return 风险检查结果
   */
  [[nodiscard]] RiskCheckResult check_pre_trade(const veloz::exec::PlaceOrderRequest& req);

  // Post-trade checks
  /**
   * @brief 交易后风险检查
   * @param position 持仓信息
   * @return 风险检查结果
   */
  [[nodiscard]] RiskCheckResult check_post_trade(const veloz::oms::Position& position);

  // Configuration
  /**
   * @brief 设置账户余额
   * @param balance_usdt 账户余额（USDT）
   */
  void set_account_balance(double balance_usdt);

  /**
   * @brief 设置最大持仓大小
   * @param max_size 最大持仓大小
   */
  void set_max_position_size(double max_size);

  /**
   * @brief 设置最大杠杆
   * @param max_leverage 最大杠杆
   */
  void set_max_leverage(double max_leverage);

  /**
   * @brief 设置参考价格
   * @param price 参考价格
   */
  void set_reference_price(double price);

  /**
   * @brief 设置最大价格偏差
   * @param deviation 最大价格偏差（比例）
   */
  void set_max_price_deviation(double deviation);

  /**
   * @brief 设置最大订单频率
   * @param orders_per_second 每秒最大订单数
   */
  void set_max_order_rate(int orders_per_second);

  /**
   * @brief 设置最大订单大小
   * @param max_qty_per_order 单订单最大数量
   */
  void set_max_order_size(double max_qty_per_order);

  /**
   * @brief 设置止损功能是否启用
   * @param enabled 是否启用止损
   */
  void set_stop_loss_enabled(bool enabled);

  /**
   * @brief 设置止损比例
   * @param percentage 止损比例（0-1）
   */
  void set_stop_loss_percentage(double percentage);

  /**
   * @brief 设置止盈功能是否启用
   * @param enabled 是否启用止盈
   */
  void set_take_profit_enabled(bool enabled);

  /**
   * @brief 设置止盈比例
   * @param percentage 止盈比例（0-1）
   */
  void set_take_profit_percentage(double percentage);

  /**
   * @brief 设置风险级别阈值
   * @param level 风险级别
   * @param threshold 阈值
   */
  void set_risk_level_threshold(RiskLevel level, double threshold);

  // Position management
  /**
   * @brief 更新持仓信息
   * @param position 持仓信息
   */
  void update_position(const veloz::oms::Position& position);

  /**
   * @brief 清除所有持仓信息
   */
  void clear_positions();

  // Circuit breaker
  /**
   * @brief 检查断路器是否触发
   * @return 断路器是否触发
   */
  [[nodiscard]] bool is_circuit_breaker_tripped() const;

  /**
   * @brief 重置断路器
   */
  void reset_circuit_breaker();

  // Risk alerts
  /**
   * @brief 获取风险预警列表
   * @return 风险预警列表
   */
  std::vector<RiskAlert> get_risk_alerts() const;

  /**
   * @brief 清除风险预警列表
   */
  void clear_risk_alerts();

  /**
   * @brief 添加风险预警
   * @param level 风险级别
   * @param message 预警消息
   * @param symbol 关联的交易品种（可选）
   */
  void add_risk_alert(RiskLevel level, const std::string& message, const std::string& symbol = "");

  // Risk metrics
  /**
   * @brief 设置风险指标计算器
   * @param calculator 风险指标计算器
   */
  void set_risk_metrics_calculator(const RiskMetricsCalculator& calculator);

  /**
   * @brief 获取风险指标
   * @return 风险指标
   */
  RiskMetrics get_risk_metrics() const;

  /**
   * @brief 计算风险指标
   */
  void calculate_risk_metrics();

  // Position management and fund allocation
  /**
   * @brief 计算持仓大小
   * @param notional 名义价值
   * @param leverage 杠杆倍数
   * @return 持仓大小
   */
  [[nodiscard]] double calculate_position_size(double notional, double leverage) const;

  /**
   * @brief 计算保证金要求
   * @param notional 名义价值
   * @param leverage 杠杆倍数
   * @return 保证金要求
   */
  [[nodiscard]] double calculate_margin_requirement(double notional, double leverage) const;

  /**
   * @brief 计算可用资金
   * @return 可用资金
   */
  [[nodiscard]] double calculate_available_funds() const;

  /**
   * @brief 计算已用保证金
   * @return 已用保证金
   */
  [[nodiscard]] double calculate_used_margin() const;

private:
  // Pre-trade check helpers
  [[nodiscard]] bool check_available_funds(const veloz::exec::PlaceOrderRequest& req) const;
  [[nodiscard]] bool check_max_position(const veloz::exec::PlaceOrderRequest& req) const;
  [[nodiscard]] bool check_price_deviation(const veloz::exec::PlaceOrderRequest& req) const;
  [[nodiscard]] bool check_order_rate() const;
  [[nodiscard]] bool check_order_size(const veloz::exec::PlaceOrderRequest& req) const;
  [[nodiscard]] bool check_stop_loss(const veloz::oms::Position& position) const;
  [[nodiscard]] bool check_take_profit(const veloz::oms::Position& position) const;

  // Risk assessment
  void assess_risk_level();
  RiskLevel get_risk_level() const;

  double account_balance_{0.0};
  double max_position_size_{0.0};
  double max_leverage_{1.0};
  double reference_price_{0.0};
  double max_price_deviation_{0.1};  // 10% default
  int max_order_rate_{100};  // orders per second
  double max_order_size_{1000.0};  // max quantity per order
  bool stop_loss_enabled_{false};
  double stop_loss_percentage_{0.05};  // 5% default
  bool take_profit_enabled_{false};
  double take_profit_percentage_{0.1};  // 10% default

  std::unordered_map<std::string, veloz::oms::Position> positions_;
  std::vector<std::chrono::steady_clock::time_point> order_timestamps_;
  bool circuit_breaker_tripped_{false};
  std::chrono::steady_clock::time_point circuit_breaker_reset_time_;

  std::vector<RiskAlert> risk_alerts_;
  std::unordered_map<RiskLevel, double> risk_level_thresholds_;

  RiskMetricsCalculator metrics_calculator_;
};

} // namespace veloz::risk
