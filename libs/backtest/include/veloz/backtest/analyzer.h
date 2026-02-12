#pragma once

#include "veloz/backtest/types.h"

#include <memory>
#include <vector>

namespace veloz::backtest {

// Backtest analyzer interface
class IBacktestAnalyzer {
public:
  virtual ~IBacktestAnalyzer() = default;

  virtual std::shared_ptr<BacktestResult> analyze(const std::vector<TradeRecord>& trades) = 0;
  virtual std::vector<EquityCurvePoint>
  calculate_equity_curve(const std::vector<TradeRecord>& trades, double initial_balance) = 0;
  virtual std::vector<DrawdownPoint>
  calculate_drawdown(const std::vector<EquityCurvePoint>& equity_curve) = 0;
  virtual double calculate_sharpe_ratio(const std::vector<TradeRecord>& trades) = 0;
  virtual double calculate_max_drawdown(const std::vector<EquityCurvePoint>& equity_curve) = 0;
  virtual double calculate_win_rate(const std::vector<TradeRecord>& trades) = 0;
  virtual double calculate_profit_factor(const std::vector<TradeRecord>& trades) = 0;
};

// Backtest analyzer implementation
class BacktestAnalyzer : public IBacktestAnalyzer {
public:
  BacktestAnalyzer();
  ~BacktestAnalyzer() override;

  std::shared_ptr<BacktestResult> analyze(const std::vector<TradeRecord>& trades) override;
  std::vector<EquityCurvePoint> calculate_equity_curve(const std::vector<TradeRecord>& trades,
                                                       double initial_balance) override;
  std::vector<DrawdownPoint>
  calculate_drawdown(const std::vector<EquityCurvePoint>& equity_curve) override;
  double calculate_sharpe_ratio(const std::vector<TradeRecord>& trades) override;
  double calculate_max_drawdown(const std::vector<EquityCurvePoint>& equity_curve) override;
  double calculate_win_rate(const std::vector<TradeRecord>& trades) override;
  double calculate_profit_factor(const std::vector<TradeRecord>& trades) override;

private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

} // namespace veloz::backtest
