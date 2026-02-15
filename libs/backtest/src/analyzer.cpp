#include "veloz/backtest/analyzer.h"

// std library includes with justifications
#include <cmath> // std::abs, std::sqrt - standard C++ math functions (no KJ equivalent)

// KJ library includes
#include <kj/common.h>
#include <kj/memory.h>
#include <kj/vector.h>

namespace veloz::backtest {

struct BacktestAnalyzer::Impl {
  double calculate_profit(const TradeRecord& trade) const {
    return trade.pnl;
  }

  double calculate_return(const BacktestResult& result) const {
    return (result.final_balance - result.initial_balance) / result.initial_balance;
  }

  double calculate_average_profit(const kj::Vector<TradeRecord>& trades) const {
    if (trades.size() == 0)
      return 0.0;

    double total_profit = 0.0;
    for (const auto& trade : trades) {
      total_profit += calculate_profit(trade);
    }

    return total_profit / trades.size();
  }

  double calculate_average_win(const kj::Vector<TradeRecord>& trades) const {
    double total_win = 0.0;
    int win_count = 0;

    for (const auto& trade : trades) {
      double profit = calculate_profit(trade);
      if (profit > 0) {
        total_win += profit;
        win_count++;
      }
    }

    return win_count > 0 ? total_win / win_count : 0.0;
  }

  double calculate_average_loss(const kj::Vector<TradeRecord>& trades) const {
    double total_loss = 0.0;
    int lose_count = 0;

    for (const auto& trade : trades) {
      double profit = calculate_profit(trade);
      if (profit < 0) {
        total_loss += profit;
        lose_count++;
      }
    }

    return lose_count > 0 ? total_loss / lose_count : 0.0;
  }
};

BacktestAnalyzer::BacktestAnalyzer() : impl_(kj::heap<Impl>()) {}

BacktestAnalyzer::~BacktestAnalyzer() noexcept {}

kj::Own<BacktestResult> BacktestAnalyzer::analyze(const kj::Vector<TradeRecord>& trades) {
  auto result = kj::heap<BacktestResult>();

  result->trade_count = static_cast<int>(trades.size());
  result->win_count = 0;
  result->lose_count = 0;
  double total_win = 0.0;
  double total_loss = 0.0;

  for (const auto& trade : trades) {
    if (trade.pnl > 0) {
      result->win_count++;
      total_win += trade.pnl;
    } else if (trade.pnl < 0) {
      result->lose_count++;
      total_loss += trade.pnl;
    }
  }

  result->win_rate =
      result->trade_count > 0 ? static_cast<double>(result->win_count) / result->trade_count : 0.0;
  result->profit_factor = total_loss != 0 ? std::abs(total_win / total_loss) : 0.0;
  result->avg_win = result->win_count > 0 ? total_win / result->win_count : 0.0;
  result->avg_lose = result->lose_count > 0 ? total_loss / result->lose_count : 0.0;

  return result;
}

kj::Vector<EquityCurvePoint>
BacktestAnalyzer::calculate_equity_curve(const kj::Vector<TradeRecord>& trades,
                                         double initial_balance) {
  kj::Vector<EquityCurvePoint> equity_curve;
  double equity = initial_balance;

  // Add initial point
  if (trades.size() > 0) {
    EquityCurvePoint initial_point;
    initial_point.timestamp = trades[0].timestamp;
    initial_point.equity = initial_balance;
    initial_point.cumulative_return = 0.0;
    equity_curve.add(initial_point);
  }

  for (const auto& trade : trades) {
    equity += trade.pnl;
    EquityCurvePoint point;
    point.timestamp = trade.timestamp;
    point.equity = equity;
    point.cumulative_return = (equity - initial_balance) / initial_balance;
    equity_curve.add(point);
  }

  return equity_curve;
}

kj::Vector<DrawdownPoint>
BacktestAnalyzer::calculate_drawdown(const kj::Vector<EquityCurvePoint>& equity_curve) {
  kj::Vector<DrawdownPoint> drawdown_curve;

  if (equity_curve.size() == 0) {
    return drawdown_curve;
  }

  double peak_equity = equity_curve[0].equity;

  for (const auto& point : equity_curve) {
    if (point.equity > peak_equity) {
      peak_equity = point.equity;
    }

    DrawdownPoint drawdown_point;
    drawdown_point.timestamp = point.timestamp;
    drawdown_point.drawdown = (peak_equity - point.equity) / peak_equity;
    drawdown_curve.add(drawdown_point);
  }

  return drawdown_curve;
}

double BacktestAnalyzer::calculate_sharpe_ratio(const kj::Vector<TradeRecord>& trades) {
  if (trades.size() < 2) {
    return 0.0;
  }

  // Calculate daily returns (assuming trades are daily)
  kj::Vector<double> daily_returns;
  double previous_equity = 0.0;
  bool first_trade = true;

  for (const auto& trade : trades) {
    double current_equity = trade.price * trade.quantity;
    if (first_trade) {
      previous_equity = current_equity;
      first_trade = false;
    } else {
      double daily_return = (current_equity - previous_equity) / previous_equity;
      daily_returns.add(daily_return);
      previous_equity = current_equity;
    }
  }

  if (daily_returns.size() < 2) {
    return 0.0;
  }

  // Calculate mean and standard deviation of returns
  double mean_return = 0.0;
  for (double ret : daily_returns) {
    mean_return += ret;
  }
  mean_return /= daily_returns.size();

  double variance = 0.0;
  for (double ret : daily_returns) {
    double diff = ret - mean_return;
    variance += diff * diff;
  }
  variance /= daily_returns.size();

  double std_dev = std::sqrt(variance);

  if (std_dev == 0.0) {
    return 0.0;
  }

  // Risk-free rate assumed to be 0 for simplicity
  double risk_free_rate = 0.0;

  return (mean_return - risk_free_rate) / std_dev * std::sqrt(252); // Annualized Sharpe ratio
}

double BacktestAnalyzer::calculate_max_drawdown(const kj::Vector<EquityCurvePoint>& equity_curve) {
  auto drawdown_curve = calculate_drawdown(equity_curve);

  double max_drawdown = 0.0;
  for (const auto& point : drawdown_curve) {
    if (point.drawdown > max_drawdown) {
      max_drawdown = point.drawdown;
    }
  }

  return max_drawdown;
}

double BacktestAnalyzer::calculate_win_rate(const kj::Vector<TradeRecord>& trades) {
  if (trades.size() == 0) {
    return 0.0;
  }

  int win_count = 0;
  for (const auto& trade : trades) {
    if (trade.pnl > 0) {
      win_count++;
    }
  }

  return static_cast<double>(win_count) / trades.size();
}

double BacktestAnalyzer::calculate_profit_factor(const kj::Vector<TradeRecord>& trades) {
  double total_win = 0.0;
  double total_loss = 0.0;

  for (const auto& trade : trades) {
    if (trade.pnl > 0) {
      total_win += trade.pnl;
    } else if (trade.pnl < 0) {
      total_loss += trade.pnl;
    }
  }

  if (total_loss == 0) {
    return 0.0;
  }

  return total_win / std::abs(total_loss);
}

} // namespace veloz::backtest
