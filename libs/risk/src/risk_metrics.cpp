#include "veloz/risk/risk_metrics.h"
#include <cmath>
#include <algorithm>
#include <numeric>
#include <vector>

namespace veloz::risk {

void RiskMetricsCalculator::add_trade(const TradeHistory& trade) {
  trades_.push_back(trade);
}

RiskMetrics RiskMetricsCalculator::calculate_all() const {
  RiskMetrics metrics;

  calculate_var(metrics);
  calculate_max_drawdown(metrics);
  calculate_sharpe_ratio(metrics);
  calculate_trade_statistics(metrics);

  return metrics;
}

void RiskMetricsCalculator::calculate_var(RiskMetrics& metrics) const {
  if (trades_.empty()) {
    return;
  }

  // Calculate daily returns
  std::vector<double> daily_returns;
  double cumulative_profit = 0.0;

  for (const auto& trade : trades_) {
    double return_pct = (trade.profit / (trade.entry_price * trade.quantity)) * 100;
    daily_returns.push_back(return_pct);
  }

  // Sort returns
  std::vector<double> sorted_returns = daily_returns;
  std::sort(sorted_returns.begin(), sorted_returns.end());

  // Calculate VaR
  int n = sorted_returns.size();
  int idx_95 = static_cast<int>(n * 0.05); // 95% confidence level
  int idx_99 = static_cast<int>(n * 0.01); // 99% confidence level

  if (idx_95 < n) {
    metrics.var_95 = std::abs(sorted_returns[idx_95]);
  }

  if (idx_99 < n) {
    metrics.var_99 = std::abs(sorted_returns[idx_99]);
  }
}

void RiskMetricsCalculator::calculate_max_drawdown(RiskMetrics& metrics) const {
  if (trades_.empty()) {
    return;
  }

  // Calculate cumulative returns
  std::vector<double> cumulative_returns;
  double cumulative_profit = 0.0;
  double peak_profit = 0.0;
  double max_drawdown = 0.0;

  for (const auto& trade : trades_) {
    cumulative_profit += trade.profit;
    cumulative_returns.push_back(cumulative_profit);

    if (cumulative_profit > peak_profit) {
      peak_profit = cumulative_profit;
    } else {
      double drawdown = (peak_profit - cumulative_profit) / peak_profit;
      if (drawdown > max_drawdown) {
        max_drawdown = drawdown;
      }
    }
  }

  metrics.max_drawdown = max_drawdown * 100; // Convert to percentage
}

void RiskMetricsCalculator::calculate_sharpe_ratio(RiskMetrics& metrics) const {
  if (trades_.empty()) {
    return;
  }

  // Calculate daily returns
  std::vector<double> daily_returns;

  for (const auto& trade : trades_) {
    double return_pct = (trade.profit / (trade.entry_price * trade.quantity)) * 100;
    daily_returns.push_back(return_pct);
  }

  // Calculate average return
  double avg_return = std::accumulate(daily_returns.begin(), daily_returns.end(), 0.0) / daily_returns.size();

  // Calculate return standard deviation
  double sum_sq = 0.0;
  for (double ret : daily_returns) {
    sum_sq += std::pow(ret - avg_return, 2);
  }
  double std_dev = std::sqrt(sum_sq / daily_returns.size());

  // Calculate Sharpe ratio
  if (std_dev > 0.0) {
    metrics.sharpe_ratio = (avg_return - risk_free_rate_) / std_dev;
  } else {
    metrics.sharpe_ratio = 0.0;
  }

  metrics.avg_daily_return = avg_return;
  metrics.return_std = std_dev;
}

void RiskMetricsCalculator::calculate_trade_statistics(RiskMetrics& metrics) const {
  if (trades_.empty()) {
    return;
  }

  metrics.total_trades = static_cast<int>(trades_.size());
  metrics.winning_trades = 0;
  metrics.losing_trades = 0;
  double total_profit = 0.0;
  double total_loss = 0.0;

  int consecutive_wins = 0;
  int consecutive_losses = 0;
  int max_consecutive_wins = 0;
  int max_consecutive_losses = 0;

  for (const auto& trade : trades_) {
    if (trade.profit > 0) {
      metrics.winning_trades++;
      total_profit += trade.profit;
      consecutive_wins++;
      consecutive_losses = 0;

      if (consecutive_wins > max_consecutive_wins) {
        max_consecutive_wins = consecutive_wins;
      }
    } else if (trade.profit < 0) {
      metrics.losing_trades++;
      total_loss += std::abs(trade.profit);
      consecutive_losses++;
      consecutive_wins = 0;

      if (consecutive_losses > max_consecutive_losses) {
        max_consecutive_losses = consecutive_losses;
      }
    }
  }

  metrics.win_rate = static_cast<double>(metrics.winning_trades) / metrics.total_trades * 100;
  metrics.profit_factor = (total_loss > 0.0) ? (total_profit / total_loss) : 0.0;
  metrics.max_consecutive_wins = max_consecutive_wins;
  metrics.max_consecutive_losses = max_consecutive_losses;
}

const std::vector<TradeHistory>& RiskMetricsCalculator::get_trades() const {
  return trades_;
}

void RiskMetricsCalculator::clear_trades() {
  trades_.clear();
}

void RiskMetricsCalculator::set_risk_free_rate(double rate) {
  risk_free_rate_ = rate;
}

} // namespace veloz::risk
