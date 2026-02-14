#pragma once

#include "veloz/backtest/types.h"

#include <kj/common.h>
#include <kj/memory.h>
#include <kj/vector.h>

namespace veloz::backtest {

// Backtest analyzer interface
class IBacktestAnalyzer {
public:
  virtual ~IBacktestAnalyzer() = default;

  virtual kj::Own<BacktestResult> analyze(const kj::Vector<TradeRecord>& trades) = 0;
  virtual kj::Vector<EquityCurvePoint> calculate_equity_curve(const kj::Vector<TradeRecord>& trades,
                                                              double initial_balance) = 0;
  virtual kj::Vector<DrawdownPoint>
  calculate_drawdown(const kj::Vector<EquityCurvePoint>& equity_curve) = 0;
  virtual double calculate_sharpe_ratio(const kj::Vector<TradeRecord>& trades) = 0;
  virtual double calculate_max_drawdown(const kj::Vector<EquityCurvePoint>& equity_curve) = 0;
  virtual double calculate_win_rate(const kj::Vector<TradeRecord>& trades) = 0;
  virtual double calculate_profit_factor(const kj::Vector<TradeRecord>& trades) = 0;
};

// Backtest analyzer implementation
class BacktestAnalyzer : public IBacktestAnalyzer {
public:
  BacktestAnalyzer();
  ~BacktestAnalyzer() noexcept override;

  kj::Own<BacktestResult> analyze(const kj::Vector<TradeRecord>& trades) override;
  kj::Vector<EquityCurvePoint> calculate_equity_curve(const kj::Vector<TradeRecord>& trades,
                                                      double initial_balance) override;
  kj::Vector<DrawdownPoint>
  calculate_drawdown(const kj::Vector<EquityCurvePoint>& equity_curve) override;
  double calculate_sharpe_ratio(const kj::Vector<TradeRecord>& trades) override;
  double calculate_max_drawdown(const kj::Vector<EquityCurvePoint>& equity_curve) override;
  double calculate_win_rate(const kj::Vector<TradeRecord>& trades) override;
  double calculate_profit_factor(const kj::Vector<TradeRecord>& trades) override;

private:
  struct Impl;
  kj::Own<Impl> impl_;
};

} // namespace veloz::backtest
