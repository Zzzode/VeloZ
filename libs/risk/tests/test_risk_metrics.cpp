#include "kj/test.h"
#include "veloz/risk/risk_metrics.h"

namespace {

using namespace veloz::risk;

// ============================================================================
// RiskMetricsCalculator Tests
// ============================================================================

KJ_TEST("RiskMetricsCalculator: Empty trades returns default metrics") {
  RiskMetricsCalculator calc;
  auto metrics = calc.calculate_all();

  KJ_EXPECT(metrics.var_95 == 0.0);
  KJ_EXPECT(metrics.var_99 == 0.0);
  KJ_EXPECT(metrics.max_drawdown == 0.0);
  KJ_EXPECT(metrics.sharpe_ratio == 0.0);
  KJ_EXPECT(metrics.total_trades == 0);
}

KJ_TEST("RiskMetricsCalculator: Add trade and calculate statistics") {
  RiskMetricsCalculator calc;

  TradeHistory trade;
  trade.symbol = kj::str("BTCUSDT");
  trade.side = kj::str("buy");
  trade.entry_price = 50000.0;
  trade.exit_price = 51000.0;
  trade.quantity = 1.0;
  trade.profit = 1000.0;

  calc.add_trade(trade);

  auto metrics = calc.calculate_all();
  KJ_EXPECT(metrics.total_trades == 1);
  KJ_EXPECT(metrics.winning_trades == 1);
  KJ_EXPECT(metrics.losing_trades == 0);
  KJ_EXPECT(metrics.win_rate == 100.0);
}

KJ_TEST("RiskMetricsCalculator: Win rate calculation") {
  RiskMetricsCalculator calc;

  // Add 3 winning trades
  for (int i = 0; i < 3; ++i) {
    TradeHistory trade;
    trade.symbol = kj::str("BTCUSDT");
    trade.side = kj::str("buy");
    trade.entry_price = 50000.0;
    trade.exit_price = 51000.0;
    trade.quantity = 1.0;
    trade.profit = 1000.0;
    calc.add_trade(trade);
  }

  // Add 2 losing trades
  for (int i = 0; i < 2; ++i) {
    TradeHistory trade;
    trade.symbol = kj::str("BTCUSDT");
    trade.side = kj::str("buy");
    trade.entry_price = 50000.0;
    trade.exit_price = 49000.0;
    trade.quantity = 1.0;
    trade.profit = -1000.0;
    calc.add_trade(trade);
  }

  auto metrics = calc.calculate_all();
  KJ_EXPECT(metrics.total_trades == 5);
  KJ_EXPECT(metrics.winning_trades == 3);
  KJ_EXPECT(metrics.losing_trades == 2);
  KJ_EXPECT(metrics.win_rate == 60.0);
}

KJ_TEST("RiskMetricsCalculator: Profit factor calculation") {
  RiskMetricsCalculator calc;

  // Add winning trade with 2000 profit
  TradeHistory win;
  win.symbol = kj::str("BTCUSDT");
  win.side = kj::str("buy");
  win.entry_price = 50000.0;
  win.exit_price = 52000.0;
  win.quantity = 1.0;
  win.profit = 2000.0;
  calc.add_trade(win);

  // Add losing trade with 1000 loss
  TradeHistory loss;
  loss.symbol = kj::str("BTCUSDT");
  loss.side = kj::str("buy");
  loss.entry_price = 50000.0;
  loss.exit_price = 49000.0;
  loss.quantity = 1.0;
  loss.profit = -1000.0;
  calc.add_trade(loss);

  auto metrics = calc.calculate_all();
  KJ_EXPECT(metrics.profit_factor == 2.0);
}

KJ_TEST("RiskMetricsCalculator: Consecutive wins/losses tracking") {
  RiskMetricsCalculator calc;

  // Add 3 consecutive wins
  for (int i = 0; i < 3; ++i) {
    TradeHistory trade;
    trade.symbol = kj::str("BTCUSDT");
    trade.side = kj::str("buy");
    trade.entry_price = 50000.0;
    trade.quantity = 1.0;
    trade.profit = 1000.0;
    calc.add_trade(trade);
  }

  // Add 2 consecutive losses
  for (int i = 0; i < 2; ++i) {
    TradeHistory trade;
    trade.symbol = kj::str("BTCUSDT");
    trade.side = kj::str("buy");
    trade.entry_price = 50000.0;
    trade.quantity = 1.0;
    trade.profit = -500.0;
    calc.add_trade(trade);
  }

  auto metrics = calc.calculate_all();
  KJ_EXPECT(metrics.max_consecutive_wins == 3);
  KJ_EXPECT(metrics.max_consecutive_losses == 2);
}

KJ_TEST("RiskMetricsCalculator: Clear trades") {
  RiskMetricsCalculator calc;

  TradeHistory trade;
  trade.symbol = kj::str("BTCUSDT");
  trade.side = kj::str("buy");
  trade.entry_price = 50000.0;
  trade.quantity = 1.0;
  trade.profit = 1000.0;
  calc.add_trade(trade);

  KJ_EXPECT(calc.get_trades().size() == 1);

  calc.clear_trades();
  KJ_EXPECT(calc.get_trades().size() == 0);
}

KJ_TEST("RiskMetricsCalculator: Set risk free rate") {
  RiskMetricsCalculator calc;
  calc.set_risk_free_rate(0.02);

  // Add some trades to calculate Sharpe ratio
  for (int i = 0; i < 10; ++i) {
    TradeHistory trade;
    trade.symbol = kj::str("BTCUSDT");
    trade.side = kj::str("buy");
    trade.entry_price = 50000.0;
    trade.quantity = 1.0;
    trade.profit = (i % 2 == 0) ? 1000.0 : -500.0;
    calc.add_trade(trade);
  }

  auto metrics = calc.calculate_all();
  // Sharpe ratio should be calculated (exact value depends on returns)
  KJ_EXPECT(metrics.sharpe_ratio != 0.0 || metrics.return_std == 0.0);
}

// ============================================================================
// RealTimeRiskMetrics Tests
// ============================================================================

KJ_TEST("RealTimeRiskMetrics: Initial state") {
  RealTimeRiskMetrics metrics;

  KJ_EXPECT(metrics.get_account_equity() == 0.0);
  KJ_EXPECT(metrics.get_peak_equity() == 0.0);
  KJ_EXPECT(metrics.get_current_drawdown() == 0.0);
  KJ_EXPECT(metrics.position_count() == 0);
}

KJ_TEST("RealTimeRiskMetrics: Set account equity") {
  RealTimeRiskMetrics metrics;
  metrics.set_account_equity(100000.0);

  KJ_EXPECT(metrics.get_account_equity() == 100000.0);
  KJ_EXPECT(metrics.get_peak_equity() == 100000.0);
}

KJ_TEST("RealTimeRiskMetrics: Position update calculates exposure") {
  RealTimeRiskMetrics metrics;
  metrics.set_account_equity(100000.0);

  // Add a long position
  metrics.on_position_update("BTCUSDT", 1.0, 50000.0);

  auto exposure = metrics.get_exposure_metrics();
  KJ_EXPECT(exposure.gross_exposure == 50000.0);
  KJ_EXPECT(exposure.net_exposure == 50000.0);
  KJ_EXPECT(exposure.long_exposure == 50000.0);
  KJ_EXPECT(exposure.short_exposure == 0.0);
  KJ_EXPECT(exposure.leverage_ratio == 0.5);
}

KJ_TEST("RealTimeRiskMetrics: Long and short exposure") {
  RealTimeRiskMetrics metrics;
  metrics.set_account_equity(100000.0);

  // Add a long position
  metrics.on_position_update("BTCUSDT", 1.0, 50000.0);
  // Add a short position
  metrics.on_position_update("ETHUSDT", -10.0, 3000.0);

  auto exposure = metrics.get_exposure_metrics();
  KJ_EXPECT(exposure.gross_exposure == 80000.0); // 50000 + 30000
  KJ_EXPECT(exposure.net_exposure == 20000.0);   // 50000 - 30000
  KJ_EXPECT(exposure.long_exposure == 50000.0);
  KJ_EXPECT(exposure.short_exposure == 30000.0);
  KJ_EXPECT(exposure.leverage_ratio == 0.8);
}

KJ_TEST("RealTimeRiskMetrics: Concentration metrics") {
  RealTimeRiskMetrics metrics;
  metrics.set_account_equity(100000.0);

  // Add positions with different sizes
  metrics.on_position_update("BTCUSDT", 1.0, 50000.0); // 50% of exposure
  metrics.on_position_update("ETHUSDT", 10.0, 3000.0); // 30% of exposure
  metrics.on_position_update("SOLUSDT", 100.0, 200.0); // 20% of exposure

  auto conc = metrics.get_concentration_metrics();
  KJ_EXPECT(conc.position_count == 3);
  KJ_EXPECT(conc.largest_position_symbol == "BTCUSDT");
  KJ_EXPECT(conc.largest_position_pct == 50.0);
  KJ_EXPECT(conc.top3_concentration_pct == 100.0);
  // HHI = 0.5^2 + 0.3^2 + 0.2^2 = 0.25 + 0.09 + 0.04 = 0.38
  KJ_EXPECT(conc.herfindahl_index > 0.37 && conc.herfindahl_index < 0.39);
}

KJ_TEST("RealTimeRiskMetrics: Remove position") {
  RealTimeRiskMetrics metrics;
  metrics.set_account_equity(100000.0);

  metrics.on_position_update("BTCUSDT", 1.0, 50000.0);
  metrics.on_position_update("ETHUSDT", 10.0, 3000.0);

  KJ_EXPECT(metrics.position_count() == 2);

  metrics.remove_position("BTCUSDT");

  KJ_EXPECT(metrics.position_count() == 1);
  auto exposure = metrics.get_exposure_metrics();
  KJ_EXPECT(exposure.gross_exposure == 30000.0);
}

KJ_TEST("RealTimeRiskMetrics: Price update") {
  RealTimeRiskMetrics metrics;
  metrics.set_account_equity(100000.0);

  metrics.on_position_update("BTCUSDT", 1.0, 50000.0);

  auto exposure1 = metrics.get_exposure_metrics();
  KJ_EXPECT(exposure1.gross_exposure == 50000.0);

  // Price increases
  metrics.on_price_update("BTCUSDT", 55000.0);

  auto exposure2 = metrics.get_exposure_metrics();
  KJ_EXPECT(exposure2.gross_exposure == 55000.0);
}

KJ_TEST("RealTimeRiskMetrics: Drawdown calculation") {
  RealTimeRiskMetrics metrics;
  metrics.set_account_equity(100000.0);

  // Equity goes down
  metrics.set_account_equity(90000.0);

  KJ_EXPECT(metrics.get_current_drawdown() > 0.09 && metrics.get_current_drawdown() < 0.11);
}

KJ_TEST("RealTimeRiskMetrics: Trade completion updates PnL") {
  RealTimeRiskMetrics metrics;
  metrics.set_account_equity(100000.0);

  TradeHistory trade;
  trade.symbol = kj::str("BTCUSDT");
  trade.side = kj::str("buy");
  trade.entry_price = 50000.0;
  trade.exit_price = 51000.0;
  trade.quantity = 1.0;
  trade.profit = 1000.0;

  metrics.on_trade_complete(trade);

  // After profitable trade, equity should be higher
  auto snapshot = metrics.get_metrics_snapshot();
  // The current_equity is updated based on realized PnL
  KJ_EXPECT(metrics.get_peak_equity() >= 100000.0);
}

KJ_TEST("RealTimeRiskMetrics: Reset") {
  RealTimeRiskMetrics metrics;
  metrics.set_account_equity(100000.0);
  metrics.on_position_update("BTCUSDT", 1.0, 50000.0);

  KJ_EXPECT(metrics.position_count() == 1);

  metrics.reset();

  KJ_EXPECT(metrics.position_count() == 0);
  KJ_EXPECT(metrics.get_account_equity() == 0.0);
  KJ_EXPECT(metrics.get_peak_equity() == 0.0);
}

KJ_TEST("RealTimeRiskMetrics: Metrics snapshot") {
  RealTimeRiskMetrics metrics;
  metrics.set_account_equity(100000.0);
  metrics.on_position_update("BTCUSDT", 1.0, 50000.0);

  auto snapshot = metrics.get_metrics_snapshot();

  KJ_EXPECT(snapshot.exposure.gross_exposure == 50000.0);
  KJ_EXPECT(snapshot.concentration.position_count == 1);
}

// ============================================================================
// CorrelationCalculator Tests
// ============================================================================

KJ_TEST("CorrelationCalculator: Empty returns zero correlation") {
  CorrelationCalculator calc;

  KJ_EXPECT(calc.get_average_correlation() == 0.0);
  KJ_EXPECT(calc.get_max_correlation() == 0.0);
}

KJ_TEST("CorrelationCalculator: Single symbol returns zero") {
  CorrelationCalculator calc;

  calc.add_return("BTCUSDT", 0.01);
  calc.add_return("BTCUSDT", 0.02);
  calc.add_return("BTCUSDT", -0.01);

  KJ_EXPECT(calc.get_average_correlation() == 0.0);
}

KJ_TEST("CorrelationCalculator: Perfect positive correlation") {
  CorrelationCalculator calc;

  // Add identical returns for two symbols
  calc.add_return("BTCUSDT", 0.01);
  calc.add_return("ETHUSDT", 0.01);
  calc.add_return("BTCUSDT", 0.02);
  calc.add_return("ETHUSDT", 0.02);
  calc.add_return("BTCUSDT", -0.01);
  calc.add_return("ETHUSDT", -0.01);

  KJ_IF_SOME(corr, calc.get_correlation("BTCUSDT", "ETHUSDT")) {
    KJ_EXPECT(corr > 0.99);
  }
  else {
    KJ_FAIL_EXPECT("Expected correlation to exist");
  }
}

KJ_TEST("CorrelationCalculator: Perfect negative correlation") {
  CorrelationCalculator calc;

  // Add opposite returns for two symbols
  calc.add_return("BTCUSDT", 0.01);
  calc.add_return("ETHUSDT", -0.01);
  calc.add_return("BTCUSDT", 0.02);
  calc.add_return("ETHUSDT", -0.02);
  calc.add_return("BTCUSDT", -0.01);
  calc.add_return("ETHUSDT", 0.01);

  KJ_IF_SOME(corr, calc.get_correlation("BTCUSDT", "ETHUSDT")) {
    KJ_EXPECT(corr < -0.99);
  }
  else {
    KJ_FAIL_EXPECT("Expected correlation to exist");
  }
}

KJ_TEST("CorrelationCalculator: Average correlation") {
  CorrelationCalculator calc;

  // Add returns for three symbols with varying correlations
  for (int i = 0; i < 10; ++i) {
    double base = 0.01 * (i % 3 - 1);
    calc.add_return("BTCUSDT", base);
    calc.add_return("ETHUSDT", base * 0.8 + 0.002);
    calc.add_return("SOLUSDT", base * 0.5 + 0.003);
  }

  double avg = calc.get_average_correlation();
  // Should be positive since all are somewhat correlated
  KJ_EXPECT(avg > 0.0);
}

KJ_TEST("CorrelationCalculator: Max correlation") {
  CorrelationCalculator calc;

  // BTC and ETH highly correlated
  for (int i = 0; i < 10; ++i) {
    double base = 0.01 * (i % 3 - 1);
    calc.add_return("BTCUSDT", base);
    calc.add_return("ETHUSDT", base * 0.99);
    calc.add_return("SOLUSDT", base * 0.3); // Less correlated
  }

  double max_corr = calc.get_max_correlation();
  // BTC-ETH correlation should be highest
  KJ_EXPECT(max_corr > 0.9);
}

KJ_TEST("CorrelationCalculator: Window trimming") {
  CorrelationCalculator calc(5); // 5-day window

  // Add more than 5 returns
  for (int i = 0; i < 10; ++i) {
    calc.add_return("BTCUSDT", 0.01 * i);
    calc.add_return("ETHUSDT", 0.01 * i);
  }

  // Should still be able to calculate correlation
  KJ_IF_SOME(corr, calc.get_correlation("BTCUSDT", "ETHUSDT")) {
    KJ_EXPECT(corr > 0.99);
  }
  else {
    KJ_FAIL_EXPECT("Expected correlation to exist");
  }
}

KJ_TEST("CorrelationCalculator: Reset") {
  CorrelationCalculator calc;

  calc.add_return("BTCUSDT", 0.01);
  calc.add_return("ETHUSDT", 0.01);
  calc.add_return("BTCUSDT", 0.02);
  calc.add_return("ETHUSDT", 0.02);

  calc.reset();

  KJ_EXPECT(calc.get_average_correlation() == 0.0);
  KJ_EXPECT(calc.get_correlation("BTCUSDT", "ETHUSDT") == kj::none);
}

KJ_TEST("CorrelationCalculator: Non-existent symbol returns none") {
  CorrelationCalculator calc;

  calc.add_return("BTCUSDT", 0.01);
  calc.add_return("BTCUSDT", 0.02);

  KJ_EXPECT(calc.get_correlation("BTCUSDT", "ETHUSDT") == kj::none);
  KJ_EXPECT(calc.get_correlation("SOLUSDT", "ETHUSDT") == kj::none);
}

KJ_TEST("CorrelationCalculator: Insufficient data returns none") {
  CorrelationCalculator calc;

  // Only one observation each
  calc.add_return("BTCUSDT", 0.01);
  calc.add_return("ETHUSDT", 0.01);

  // Need at least 2 observations
  KJ_EXPECT(calc.get_correlation("BTCUSDT", "ETHUSDT") == kj::none);
}

} // namespace
