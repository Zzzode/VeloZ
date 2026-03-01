#include "veloz/risk/risk_metrics.h"

// std::sort/accumulate for statistical calculations (KJ lacks algorithm library)
#include <algorithm>
// std::abs/std::pow/std::sqrt for math operations (standard C++ library)
#include <cmath>
#include <numeric>
// kj::Vector for dynamic arrays (std::sort/accumulate work with kj::Vector iterators)

namespace veloz::risk {

void RiskMetricsCalculator::add_trade(const TradeHistory& trade) {
  // Copy trade to add to kj::Vector
  TradeHistory copy;
  copy.symbol = kj::heapString(trade.symbol);
  copy.side = kj::heapString(trade.side);
  copy.entry_price = trade.entry_price;
  copy.exit_price = trade.exit_price;
  copy.quantity = trade.quantity;
  copy.profit = trade.profit;
  copy.entry_time = trade.entry_time;
  copy.exit_time = trade.exit_time;
  trades_.add(kj::mv(copy));
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
  if (trades_.size() == 0) {
    return;
  }

  // Calculate daily returns
  kj::Vector<double> daily_returns;
  double cumulative_profit = 0.0;

  for (const auto& trade : trades_) {
    double return_pct = (trade.profit / (trade.entry_price * trade.quantity)) * 100;
    daily_returns.add(return_pct);
  }

  // Sort returns (kj::Vector iterators are raw pointers, compatible with std::sort)
  kj::Vector<double> sorted_returns;
  sorted_returns.addAll(daily_returns);
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
  if (trades_.size() == 0) {
    return;
  }

  // Calculate cumulative returns
  kj::Vector<double> cumulative_returns;
  double cumulative_profit = 0.0;
  double peak_profit = 0.0;
  double max_drawdown = 0.0;

  for (const auto& trade : trades_) {
    cumulative_profit += trade.profit;
    cumulative_returns.add(cumulative_profit);

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
  if (trades_.size() == 0) {
    return;
  }

  // Calculate daily returns
  kj::Vector<double> daily_returns;

  for (const auto& trade : trades_) {
    double return_pct = (trade.profit / (trade.entry_price * trade.quantity)) * 100;
    daily_returns.add(return_pct);
  }

  // Calculate average return (kj::Vector iterators are raw pointers, compatible with
  // std::accumulate)
  double avg_return =
      std::accumulate(daily_returns.begin(), daily_returns.end(), 0.0) / daily_returns.size();

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
  if (trades_.size() == 0) {
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

const kj::Vector<TradeHistory>& RiskMetricsCalculator::get_trades() const {
  return trades_;
}

void RiskMetricsCalculator::clear_trades() {
  trades_.clear();
}

void RiskMetricsCalculator::set_risk_free_rate(double rate) {
  risk_free_rate_ = rate;
}

// ============================================================================
// RealTimeRiskMetrics Implementation
// ============================================================================

namespace {
int64_t current_timestamp_ns() {
  return std::chrono::duration_cast<std::chrono::nanoseconds>(
             std::chrono::system_clock::now().time_since_epoch())
      .count();
}
} // namespace

void RealTimeRiskMetrics::on_position_update(kj::StringPtr symbol, double size, double price) {
  PositionValue pv;
  pv.symbol = kj::str(symbol);
  pv.size = size;
  pv.price = price;
  pv.notional = std::abs(size) * price;
  pv.unrealized_pnl = 0.0; // Will be calculated separately if needed

  auto key = kj::str(symbol);
  positions_.upsert(kj::mv(key), kj::mv(pv), [&](PositionValue& existing, PositionValue&& update) {
    existing = kj::mv(update);
  });

  recalculate_exposure();
  recalculate_concentration();
  update_drawdown();
}

void RealTimeRiskMetrics::on_price_update(kj::StringPtr symbol, double price) {
  KJ_IF_SOME(pv, positions_.find(symbol)) {
    pv.price = price;
    pv.notional = std::abs(pv.size) * price;

    recalculate_exposure();
    recalculate_concentration();
    update_drawdown();
  }
}

void RealTimeRiskMetrics::remove_position(kj::StringPtr symbol) {
  positions_.erase(symbol);
  recalculate_exposure();
  recalculate_concentration();
}

void RealTimeRiskMetrics::on_trade_complete(const TradeHistory& trade) {
  total_realized_pnl_ += trade.profit;

  // Track daily return for Sharpe calculation
  if (account_equity_ > 0.0) {
    double daily_return = trade.profit / account_equity_;
    daily_returns_.add(daily_return);
  }

  update_drawdown();
}

ExposureMetrics RealTimeRiskMetrics::get_exposure_metrics() const {
  return exposure_;
}

ConcentrationMetrics RealTimeRiskMetrics::get_concentration_metrics() const {
  return concentration_;
}

double RealTimeRiskMetrics::get_current_drawdown() const {
  return current_drawdown_;
}

double RealTimeRiskMetrics::get_peak_equity() const {
  return peak_equity_;
}

RiskMetrics RealTimeRiskMetrics::get_metrics_snapshot() const {
  RiskMetrics metrics;

  // Copy exposure metrics
  metrics.exposure = exposure_;

  // Copy concentration metrics
  metrics.concentration = concentration_;

  // Drawdown metrics
  metrics.current_drawdown = current_drawdown_;
  metrics.max_drawdown = max_drawdown_;
  metrics.drawdown_start_ns = drawdown_start_ns_;
  metrics.max_drawdown_duration_ns = max_drawdown_duration_ns_;

  // Calculate Sharpe and Sortino if we have enough data
  if (daily_returns_.size() > 1) {
    double sum = std::accumulate(daily_returns_.begin(), daily_returns_.end(), 0.0);
    double avg_return = sum / daily_returns_.size();

    // Standard deviation
    double sum_sq = 0.0;
    double sum_downside_sq = 0.0;
    for (double ret : daily_returns_) {
      sum_sq += std::pow(ret - avg_return, 2);
      if (ret < 0) {
        sum_downside_sq += std::pow(ret, 2);
      }
    }
    double std_dev = std::sqrt(sum_sq / daily_returns_.size());
    double downside_dev = std::sqrt(sum_downside_sq / daily_returns_.size());

    if (std_dev > 0.0) {
      metrics.sharpe_ratio = avg_return / std_dev * std::sqrt(252.0); // Annualized
    }
    if (downside_dev > 0.0) {
      metrics.sortino_ratio = avg_return / downside_dev * std::sqrt(252.0);
    }
    if (max_drawdown_ > 0.0) {
      double annual_return = avg_return * 252.0;
      metrics.calmar_ratio = annual_return / max_drawdown_;
    }

    metrics.avg_daily_return = avg_return;
    metrics.return_std = std_dev;
  }

  return metrics;
}

void RealTimeRiskMetrics::set_account_equity(double equity) {
  account_equity_ = equity;
  if (peak_equity_ < equity) {
    peak_equity_ = equity;
  }
  current_equity_ = equity;
  update_drawdown();
}

double RealTimeRiskMetrics::get_account_equity() const {
  return account_equity_;
}

void RealTimeRiskMetrics::reset() {
  positions_.clear();
  account_equity_ = 0.0;
  peak_equity_ = 0.0;
  current_equity_ = 0.0;
  exposure_ = ExposureMetrics{};
  concentration_ = ConcentrationMetrics{};
  current_drawdown_ = 0.0;
  drawdown_start_ns_ = 0;
  max_drawdown_duration_ns_ = 0;
  max_drawdown_ = 0.0;
  total_realized_pnl_ = 0.0;
  daily_returns_.clear();
}

size_t RealTimeRiskMetrics::position_count() const {
  return positions_.size();
}

void RealTimeRiskMetrics::recalculate_exposure() {
  exposure_ = ExposureMetrics{};

  for (auto& entry : positions_) {
    const auto& pv = entry.value;
    double signed_value = pv.size * pv.price;

    exposure_.gross_exposure += pv.notional;
    exposure_.net_exposure += signed_value;

    if (pv.size > 0) {
      exposure_.long_exposure += pv.notional;
    } else if (pv.size < 0) {
      exposure_.short_exposure += pv.notional;
    }
  }

  if (account_equity_ > 0.0) {
    exposure_.leverage_ratio = exposure_.gross_exposure / account_equity_;
    exposure_.net_leverage_ratio = std::abs(exposure_.net_exposure) / account_equity_;
  }
}

void RealTimeRiskMetrics::recalculate_concentration() {
  concentration_ = ConcentrationMetrics{};
  concentration_.position_count = static_cast<int>(positions_.size());

  if (positions_.size() == 0 || exposure_.gross_exposure <= 0.0) {
    return;
  }

  // Collect position weights
  kj::Vector<std::pair<kj::String, double>> weights;
  for (auto& entry : positions_) {
    double weight = entry.value.notional / exposure_.gross_exposure;
    weights.add(std::make_pair(kj::str(entry.key), weight));
  }

  // Sort by weight descending
  std::sort(weights.begin(), weights.end(),
            [](const auto& a, const auto& b) { return a.second > b.second; });

  // Largest position
  if (weights.size() > 0) {
    concentration_.largest_position_symbol = std::string(weights[0].first.cStr());
    concentration_.largest_position_pct = weights[0].second * 100.0;
  }

  // Top 3 concentration
  double top3_sum = 0.0;
  for (size_t i = 0; i < std::min(weights.size(), size_t(3)); ++i) {
    top3_sum += weights[i].second;
  }
  concentration_.top3_concentration_pct = top3_sum * 100.0;

  // Herfindahl-Hirschman Index (HHI)
  double hhi = 0.0;
  for (const auto& w : weights) {
    hhi += w.second * w.second;
  }
  concentration_.herfindahl_index = hhi;
}

void RealTimeRiskMetrics::update_drawdown() {
  // Calculate current equity including unrealized PnL
  double total_unrealized = 0.0;
  for (auto& entry : positions_) {
    total_unrealized += entry.value.unrealized_pnl;
  }
  current_equity_ = account_equity_ + total_realized_pnl_ + total_unrealized;

  // Update peak
  if (current_equity_ > peak_equity_) {
    peak_equity_ = current_equity_;
    drawdown_start_ns_ = 0; // Reset drawdown tracking
  }

  // Calculate current drawdown
  if (peak_equity_ > 0.0) {
    current_drawdown_ = (peak_equity_ - current_equity_) / peak_equity_;

    // Track drawdown start
    if (current_drawdown_ > 0.0 && drawdown_start_ns_ == 0) {
      drawdown_start_ns_ = current_timestamp_ns();
    }

    // Update max drawdown
    if (current_drawdown_ > max_drawdown_) {
      max_drawdown_ = current_drawdown_;
    }

    // Update max drawdown duration
    if (drawdown_start_ns_ > 0) {
      int64_t duration = current_timestamp_ns() - drawdown_start_ns_;
      if (duration > max_drawdown_duration_ns_) {
        max_drawdown_duration_ns_ = duration;
      }
    }
  }
}

// ============================================================================
// CorrelationCalculator Implementation
// ============================================================================

CorrelationCalculator::CorrelationCalculator(int window_days) : window_days_(window_days) {}

void CorrelationCalculator::add_return(kj::StringPtr symbol, double daily_return) {
  auto key = kj::str(symbol);

  KJ_IF_SOME(returns, returns_.find(symbol)) {
    returns.add(daily_return);
    // Trim to window size
    while (returns.size() > static_cast<size_t>(window_days_)) {
      // Remove oldest (shift left)
      kj::Vector<double> trimmed;
      for (size_t i = 1; i < returns.size(); ++i) {
        trimmed.add(returns[i]);
      }
      returns = kj::mv(trimmed);
    }
  }
  else {
    kj::Vector<double> new_returns;
    new_returns.add(daily_return);
    returns_.insert(kj::mv(key), kj::mv(new_returns));
  }
}

double CorrelationCalculator::get_average_correlation() const {
  if (returns_.size() < 2) {
    return 0.0;
  }

  // Collect all symbols
  kj::Vector<kj::StringPtr> symbols;
  for (auto& entry : returns_) {
    symbols.add(entry.key);
  }

  // Calculate pairwise correlations
  double sum_corr = 0.0;
  int pair_count = 0;

  for (size_t i = 0; i < symbols.size(); ++i) {
    for (size_t j = i + 1; j < symbols.size(); ++j) {
      KJ_IF_SOME(corr, get_correlation(symbols[i], symbols[j])) {
        sum_corr += corr;
        pair_count++;
      }
    }
  }

  return pair_count > 0 ? sum_corr / pair_count : 0.0;
}

double CorrelationCalculator::get_max_correlation() const {
  if (returns_.size() < 2) {
    return 0.0;
  }

  // Collect all symbols
  kj::Vector<kj::StringPtr> symbols;
  for (auto& entry : returns_) {
    symbols.add(entry.key);
  }

  double max_corr = 0.0;

  for (size_t i = 0; i < symbols.size(); ++i) {
    for (size_t j = i + 1; j < symbols.size(); ++j) {
      KJ_IF_SOME(corr, get_correlation(symbols[i], symbols[j])) {
        if (std::abs(corr) > std::abs(max_corr)) {
          max_corr = corr;
        }
      }
    }
  }

  return max_corr;
}

kj::Maybe<double> CorrelationCalculator::get_correlation(kj::StringPtr symbol1,
                                                         kj::StringPtr symbol2) const {
  const kj::Vector<double>* returns1 = nullptr;
  const kj::Vector<double>* returns2 = nullptr;

  KJ_IF_SOME(r1, returns_.find(symbol1)) {
    returns1 = &r1;
  }
  else {
    return kj::none;
  }

  KJ_IF_SOME(r2, returns_.find(symbol2)) {
    returns2 = &r2;
  }
  else {
    return kj::none;
  }

  // Need at least 2 overlapping observations
  size_t n = std::min(returns1->size(), returns2->size());
  if (n < 2) {
    return kj::none;
  }

  // Calculate means
  double sum1 = 0.0, sum2 = 0.0;
  for (size_t i = 0; i < n; ++i) {
    sum1 += (*returns1)[returns1->size() - n + i];
    sum2 += (*returns2)[returns2->size() - n + i];
  }
  double mean1 = sum1 / n;
  double mean2 = sum2 / n;

  // Calculate correlation
  double cov = 0.0, var1 = 0.0, var2 = 0.0;
  for (size_t i = 0; i < n; ++i) {
    double d1 = (*returns1)[returns1->size() - n + i] - mean1;
    double d2 = (*returns2)[returns2->size() - n + i] - mean2;
    cov += d1 * d2;
    var1 += d1 * d1;
    var2 += d2 * d2;
  }

  double denom = std::sqrt(var1 * var2);
  if (denom < 1e-10) {
    return kj::none;
  }

  return cov / denom;
}

void CorrelationCalculator::reset() {
  returns_.clear();
}

} // namespace veloz::risk
