#pragma once

#include <vector>
#include <string>
#include <unordered_map>
#include <chrono>

namespace veloz::risk {

// 风险指标计算结果
struct RiskMetrics {
  // VaR (Value at Risk) 在置信水平下的每日损失值
  double var_95{0.0};
  double var_99{0.0};

  // 最大回撤
  double max_drawdown{0.0};

  // 夏普比率
  double sharpe_ratio{0.0};

  // 胜率
  double win_rate{0.0};

  // 盈亏比
  double profit_factor{0.0};

  // 平均每日收益率
  double avg_daily_return{0.0};

  // 收益率标准差
  double return_std{0.0};

  // 总交易次数
  int total_trades{0};

  // 盈利交易次数
  int winning_trades{0};

  // 亏损交易次数
  int losing_trades{0};

  // 最大连续盈利次数
  int max_consecutive_wins{0};

  // 最大连续亏损次数
  int max_consecutive_losses{0};
};

// 交易历史记录
struct TradeHistory {
  std::string symbol;
  std::string side;
  double entry_price;
  double exit_price;
  double quantity;
  double profit;
  std::chrono::system_clock::time_point entry_time;
  std::chrono::system_clock::time_point exit_time;
};

// 风险指标计算器
class RiskMetricsCalculator final {
public:
  RiskMetricsCalculator() = default;

  // 添加交易历史记录
  void add_trade(const TradeHistory& trade);

  // 计算所有风险指标
  RiskMetrics calculate_all() const;

  // 计算 VaR (Value at Risk)
  void calculate_var(RiskMetrics& metrics) const;

  // 计算最大回撤
  void calculate_max_drawdown(RiskMetrics& metrics) const;

  // 计算夏普比率
  void calculate_sharpe_ratio(RiskMetrics& metrics) const;

  // 计算交易统计指标
  void calculate_trade_statistics(RiskMetrics& metrics) const;

  // 获取交易历史记录
  const std::vector<TradeHistory>& get_trades() const;

  // 清空交易历史记录
  void clear_trades();

  // 设置基准收益率（用于夏普比率计算）
  void set_risk_free_rate(double rate);

private:
  std::vector<TradeHistory> trades_;
  double risk_free_rate_{0.0}; // 默认无风险收益率为 0%
};

} // namespace veloz::risk
