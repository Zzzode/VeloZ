#pragma once

#include <chrono>
#include <string>
#include <unordered_map>
#include <vector>

namespace veloz::risk {

// Risk metrics calculation result
struct RiskMetrics {
  // VaR (Value at Risk) daily loss value at confidence level
  double var_95{0.0};
  double var_99{0.0};

  // Maximum drawdown
  double max_drawdown{0.0};

  // Sharpe ratio
  double sharpe_ratio{0.0};

  // Win rate
  double win_rate{0.0};

  // Profit factor
  double profit_factor{0.0};

  // Average daily return
  double avg_daily_return{0.0};

  // Return standard deviation
  double return_std{0.0};

  // Total number of trades
  int total_trades{0};

  // Number of winning trades
  int winning_trades{0};

  // Number of losing trades
  int losing_trades{0};

  // Maximum consecutive wins
  int max_consecutive_wins{0};

  // Maximum consecutive losses
  int max_consecutive_losses{0};
};

// Trade history record
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

// Risk metrics calculator
class RiskMetricsCalculator final {
public:
  RiskMetricsCalculator() = default;

  // Add trade history record
  void add_trade(const TradeHistory& trade);

  // Calculate all risk metrics
  RiskMetrics calculate_all() const;

  // Calculate VaR (Value at Risk)
  void calculate_var(RiskMetrics& metrics) const;

  // Calculate maximum drawdown
  void calculate_max_drawdown(RiskMetrics& metrics) const;

  // Calculate Sharpe ratio
  void calculate_sharpe_ratio(RiskMetrics& metrics) const;

  // Calculate trade statistics
  void calculate_trade_statistics(RiskMetrics& metrics) const;

  // Get trade history records
  const std::vector<TradeHistory>& get_trades() const;

  // Clear trade history records
  void clear_trades();

  // Set risk-free rate (for Sharpe ratio calculation)
  void set_risk_free_rate(double rate);

private:
  std::vector<TradeHistory> trades_;
  double risk_free_rate_{0.0}; // Default risk-free rate is 0%
};

} // namespace veloz::risk
