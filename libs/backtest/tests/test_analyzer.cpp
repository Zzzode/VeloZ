#include "veloz/backtest/analyzer.h"

#include <kj/test.h>
#include <kj/memory.h>
#include <kj/string.h>
#include <kj/vector.h>

using namespace veloz::backtest;

namespace {

// Helper to create sample trades
kj::Vector<TradeRecord> create_sample_trades() {
  kj::Vector<TradeRecord> trades;

  for (int i = 0; i < 100; ++i) {
    TradeRecord trade;
    trade.timestamp = 1609459200000 + i * 3600000; // 1 hour intervals
    trade.symbol = kj::str("BTCUSDT");
    trade.side = kj::str(i % 2 == 0 ? "buy" : "sell");
    trade.price = 50000 + i * 100;
    trade.quantity = 0.01;
    trade.fee = 0.001;
    trade.pnl = (i % 3 == 0) ? 100.0 : -50.0; // 66% win rate
    trade.strategy_id = kj::str("test_strategy");
    trades.add(kj::mv(trade));
  }

  return trades;
}

// ============================================================================
// BacktestAnalyzer Tests
// ============================================================================

KJ_TEST("BacktestAnalyzer: CalculateEquityCurve") {
  auto analyzer = kj::heap<BacktestAnalyzer>();
  auto trades = create_sample_trades();
  auto equity_curve = analyzer->calculate_equity_curve(trades, 10000.0);
  KJ_EXPECT(!equity_curve.empty());
  KJ_EXPECT(equity_curve.size() > 0);
}

KJ_TEST("BacktestAnalyzer: CalculateDrawdown") {
  auto analyzer = kj::heap<BacktestAnalyzer>();
  auto trades = create_sample_trades();
  auto equity_curve = analyzer->calculate_equity_curve(trades, 10000.0);
  auto drawdown_curve = analyzer->calculate_drawdown(equity_curve);
  KJ_EXPECT(!drawdown_curve.empty());
  KJ_EXPECT(drawdown_curve.size() == equity_curve.size());
}

KJ_TEST("BacktestAnalyzer: CalculateSharpeRatio") {
  auto analyzer = kj::heap<BacktestAnalyzer>();
  auto trades = create_sample_trades();
  double sharpe_ratio = analyzer->calculate_sharpe_ratio(trades);
  KJ_EXPECT(sharpe_ratio > 0);
}

KJ_TEST("BacktestAnalyzer: CalculateMaxDrawdown") {
  auto analyzer = kj::heap<BacktestAnalyzer>();
  auto trades = create_sample_trades();
  auto equity_curve = analyzer->calculate_equity_curve(trades, 10000.0);
  double max_drawdown = analyzer->calculate_max_drawdown(equity_curve);
  KJ_EXPECT(max_drawdown >= 0);
}

KJ_TEST("BacktestAnalyzer: CalculateWinRate") {
  auto analyzer = kj::heap<BacktestAnalyzer>();
  auto trades = create_sample_trades();
  double win_rate = analyzer->calculate_win_rate(trades);
  KJ_EXPECT(win_rate >= 0);
  KJ_EXPECT(win_rate <= 1);
}

KJ_TEST("BacktestAnalyzer: CalculateProfitFactor") {
  auto analyzer = kj::heap<BacktestAnalyzer>();
  auto trades = create_sample_trades();
  double profit_factor = analyzer->calculate_profit_factor(trades);
  KJ_EXPECT(profit_factor > 0);
}

KJ_TEST("BacktestAnalyzer: AnalyzeTrades") {
  auto analyzer = kj::heap<BacktestAnalyzer>();
  auto trades = create_sample_trades();
  auto result = analyzer->analyze(trades);
  // kj::Own<BacktestResult> always holds a valid value
  KJ_EXPECT(result->trade_count == static_cast<int>(trades.size()));
  KJ_EXPECT(result->win_count >= 0);
  KJ_EXPECT(result->lose_count >= 0);
  KJ_EXPECT(result->profit_factor > 0);
}

} // namespace
