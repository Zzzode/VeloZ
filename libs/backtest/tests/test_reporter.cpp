#include "veloz/backtest/reporter.h"

#include <kj/memory.h>
#include <kj/string.h>
#include <kj/test.h>
#include <kj/vector.h>

using namespace veloz::backtest;

namespace {

// Helper function to check if kj::String contains a substring
bool contains(kj::StringPtr str, kj::StringPtr substr) {
  return str.contains(substr);
}

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

// Helper to create sample result
BacktestResult create_sample_result() {
  BacktestResult result;
  result.strategy_name = kj::str("TestStrategy");
  result.symbol = kj::str("BTCUSDT");
  result.start_time = 1609459200000;
  result.end_time = 1640995200000;
  result.initial_balance = 10000.0;
  result.final_balance = 15000.0;
  result.total_return = 0.5;
  result.max_drawdown = 0.1;
  result.sharpe_ratio = 1.5;
  result.win_rate = 0.6;
  result.profit_factor = 1.8;
  result.trade_count = 100;
  result.win_count = 60;
  result.lose_count = 40;
  result.avg_win = 100.0;
  result.avg_lose = -50.0;
  result.trades = create_sample_trades();
  return result;
}

// ============================================================================
// BacktestReporter Tests
// ============================================================================

KJ_TEST("BacktestReporter: GenerateHTMLReport") {
  auto reporter = kj::heap<BacktestReporter>();
  auto result = create_sample_result();
  kj::String html = reporter->generate_html_report(result);
  KJ_EXPECT(html.size() > 0);
  KJ_EXPECT(contains(html, "VeloZ Backtest Report"));
}

KJ_TEST("BacktestReporter: GenerateJSONReport") {
  auto reporter = kj::heap<BacktestReporter>();
  auto result = create_sample_result();
  kj::String json = reporter->generate_json_report(result);
  KJ_EXPECT(json.size() > 0);
  KJ_EXPECT(contains(json, "TestStrategy"));
}

KJ_TEST("BacktestReporter: GenerateReportFile") {
  auto reporter = kj::heap<BacktestReporter>();
  auto result = create_sample_result();
  KJ_EXPECT(reporter->generate_report(result, "test_report.html"));
}

KJ_TEST("BacktestReporter: ReportContainsKeyMetrics") {
  auto reporter = kj::heap<BacktestReporter>();
  auto result = create_sample_result();
  kj::String html = reporter->generate_html_report(result);
  KJ_EXPECT(contains(html, "50%")); // Total return
  KJ_EXPECT(contains(html, "10%")); // Max drawdown
  KJ_EXPECT(contains(html, "1.5")); // Sharpe ratio
  KJ_EXPECT(contains(html, "60%")); // Win rate
  KJ_EXPECT(contains(html, "100")); // Total trades
}

KJ_TEST("BacktestReporter: HTMLReportContainsTradeHistory") {
  auto reporter = kj::heap<BacktestReporter>();
  auto result = create_sample_result();
  kj::String html = reporter->generate_html_report(result);

  // Check that trade history section exists
  KJ_EXPECT(contains(html, "Trade History"));
  KJ_EXPECT(contains(html, "<th>Time</th>"));
  KJ_EXPECT(contains(html, "<th>Symbol</th>"));
  KJ_EXPECT(contains(html, "<th>Side</th>"));
  KJ_EXPECT(contains(html, "<th>Price</th>"));
  KJ_EXPECT(contains(html, "<th>Quantity</th>"));
  KJ_EXPECT(contains(html, "<th>Fee</th>"));
  KJ_EXPECT(contains(html, "<th>P&L</th>"));
}

KJ_TEST("BacktestReporter: HTMLReportContainsTradeData") {
  auto reporter = kj::heap<BacktestReporter>();
  auto result = create_sample_result();
  kj::String html = reporter->generate_html_report(result);

  // Check that sample trade data is present
  KJ_EXPECT(contains(html, "BTCUSDT"));
  KJ_EXPECT(contains(html, "buy"));
  KJ_EXPECT(contains(html, "sell"));
  KJ_EXPECT(contains(html, "0.001")); // Fee
}

KJ_TEST("BacktestReporter: HTMLReportTradePnLColoring") {
  auto reporter = kj::heap<BacktestReporter>();
  auto result = create_sample_result();
  kj::String html = reporter->generate_html_report(result);

  // Check that positive and negative P&L have proper classes
  KJ_EXPECT(contains(html, "positive"));
  KJ_EXPECT(contains(html, "negative"));
}

KJ_TEST("BacktestReporter: JSONReportContainsTradeHistory") {
  auto reporter = kj::heap<BacktestReporter>();
  auto result = create_sample_result();
  kj::String json = reporter->generate_json_report(result);

  // Check that trades array exists
  KJ_EXPECT(contains(json, "\"trades\""));
  KJ_EXPECT(contains(json, "\"timestamp\""));
  KJ_EXPECT(contains(json, "\"symbol\""));
  KJ_EXPECT(contains(json, "\"side\""));
  KJ_EXPECT(contains(json, "\"price\""));
  KJ_EXPECT(contains(json, "\"quantity\""));
  KJ_EXPECT(contains(json, "\"fee\""));
  KJ_EXPECT(contains(json, "\"pnl\""));
}

KJ_TEST("BacktestReporter: JSONReportContainsTradeData") {
  auto reporter = kj::heap<BacktestReporter>();
  auto result = create_sample_result();
  kj::String json = reporter->generate_json_report(result);

  // Check that sample trade data is present
  KJ_EXPECT(contains(json, "BTCUSDT"));
  KJ_EXPECT(contains(json, "test_strategy"));
}

KJ_TEST("BacktestReporter: EmptyTradeHistory") {
  auto reporter = kj::heap<BacktestReporter>();
  auto result = create_sample_result();
  result.trades = kj::Vector<TradeRecord>();

  kj::String html = reporter->generate_html_report(result);
  kj::String json = reporter->generate_json_report(result);

  // Both reports should handle empty trade arrays
  KJ_EXPECT(contains(html, "Trade History"));
  KJ_EXPECT(contains(json, "\"trades\""));
}

KJ_TEST("BacktestReporter: SingleTradeReport") {
  auto reporter = kj::heap<BacktestReporter>();
  auto result = create_sample_result();

  // Create a new single-trade vector instead of copying (kj::String is not copyable)
  kj::Vector<TradeRecord> single_trade;
  TradeRecord trade;
  trade.timestamp = result.trades[0].timestamp;
  trade.symbol = kj::str(result.trades[0].symbol);
  trade.side = kj::str(result.trades[0].side);
  trade.price = result.trades[0].price;
  trade.quantity = result.trades[0].quantity;
  trade.fee = result.trades[0].fee;
  trade.pnl = result.trades[0].pnl;
  trade.strategy_id = kj::str(result.trades[0].strategy_id);
  single_trade.add(kj::mv(trade));
  result.trades = kj::mv(single_trade);
  result.trade_count = 1;
  result.win_count = 1;
  result.lose_count = 0;

  kj::String html = reporter->generate_html_report(result);
  kj::String json = reporter->generate_json_report(result);

  // Both reports should handle single trade
  KJ_EXPECT(contains(html, "1"));
  KJ_EXPECT(contains(json, "1"));
}

KJ_TEST("BacktestReporter: LargeTradeHistory") {
  auto reporter = kj::heap<BacktestReporter>();
  auto result = create_sample_result();

  // Create 1000 trades
  result.trades = kj::Vector<TradeRecord>();
  for (int i = 0; i < 1000; ++i) {
    TradeRecord trade;
    trade.timestamp = 1609459200000 + i * 1000;
    trade.symbol = kj::str("BTCUSDT");
    trade.side = kj::str((i % 2 == 0) ? "buy" : "sell");
    trade.price = 50000.0;
    trade.quantity = 0.01;
    trade.fee = 0.001;
    trade.pnl = (i % 3 == 0) ? 100.0 : -50.0;
    trade.strategy_id = kj::str("test_strategy");
    result.trades.add(kj::mv(trade));
  }
  result.trade_count = 1000;
  result.win_count = 667; // 66.7% win rate
  result.lose_count = 333;

  kj::String html = reporter->generate_html_report(result);
  kj::String json = reporter->generate_json_report(result);

  // Both reports should handle large trade arrays
  KJ_EXPECT(contains(html, "1000"));
  KJ_EXPECT(contains(json, "1000"));
}

// ============================================================================
// Enhanced Reporter Tests
// ============================================================================

KJ_TEST("BacktestReporter: ReportConfig") {
  auto reporter = kj::heap<BacktestReporter>();

  // Test default config
  const auto& default_config = reporter->get_config();
  KJ_EXPECT(default_config.include_equity_curve == true);
  KJ_EXPECT(default_config.include_drawdown_curve == true);
  KJ_EXPECT(default_config.include_trade_list == true);
  KJ_EXPECT(default_config.include_monthly_returns == true);
  KJ_EXPECT(default_config.include_trade_analysis == true);
  KJ_EXPECT(default_config.include_risk_metrics == true);

  // Set custom config
  ReportConfig custom_config;
  custom_config.include_equity_curve = false;
  custom_config.include_monthly_returns = false;
  custom_config.title = kj::str("Custom Report Title");
  custom_config.description = kj::str("Test description");
  custom_config.author = kj::str("Test Author");

  reporter->set_config(custom_config);

  const auto& updated_config = reporter->get_config();
  KJ_EXPECT(updated_config.include_equity_curve == false);
  KJ_EXPECT(updated_config.include_monthly_returns == false);
  KJ_EXPECT(updated_config.title == "Custom Report Title"_kj);
  KJ_EXPECT(updated_config.description == "Test description"_kj);
  KJ_EXPECT(updated_config.author == "Test Author"_kj);
}

KJ_TEST("BacktestReporter: GenerateCSVTrades") {
  auto reporter = kj::heap<BacktestReporter>();
  auto result = create_sample_result();

  kj::String csv = reporter->generate_csv_trades(result);

  // Check CSV header
  KJ_EXPECT(contains(csv, "timestamp,symbol,side,price,quantity,fee,pnl,strategy_id"));

  // Check CSV contains trade data
  KJ_EXPECT(contains(csv, "BTCUSDT"));
  KJ_EXPECT(contains(csv, "buy"));
  KJ_EXPECT(contains(csv, "sell"));
  KJ_EXPECT(contains(csv, "test_strategy"));
}

KJ_TEST("BacktestReporter: GenerateMarkdownReport") {
  auto reporter = kj::heap<BacktestReporter>();
  auto result = create_sample_result();

  // Add equity curve for monthly returns calculation
  for (int i = 0; i < 100; ++i) {
    EquityCurvePoint point;
    point.timestamp = 1609459200000 + i * 86400000; // Daily points
    point.equity = 10000.0 + i * 50.0;
    point.cumulative_return = static_cast<double>(i) * 0.005;
    result.equity_curve.add(point);
  }

  kj::String md = reporter->generate_markdown_report(result);

  // Check markdown structure
  KJ_EXPECT(contains(md, "# VeloZ Backtest Report"));
  KJ_EXPECT(contains(md, "## Summary"));
  KJ_EXPECT(contains(md, "| Metric | Value |"));
  KJ_EXPECT(contains(md, "TestStrategy"));
  KJ_EXPECT(contains(md, "BTCUSDT"));
}

KJ_TEST("BacktestReporter: CalculateMonthlyReturns") {
  auto result = create_sample_result();

  // Add equity curve spanning multiple months
  for (int i = 0; i < 90; ++i) {
    EquityCurvePoint point;
    point.timestamp = 1609459200000 + i * 86400000; // Daily points (Jan-Mar 2021)
    point.equity = 10000.0 + i * 50.0;
    point.cumulative_return = static_cast<double>(i) * 0.005;
    result.equity_curve.add(point);
  }

  auto monthly_returns = BacktestReporter::calculate_monthly_returns(result);

  // Should have at least 2-3 months of data
  KJ_EXPECT(monthly_returns.size() >= 2);

  // Check first month
  KJ_EXPECT(monthly_returns[0].year == 2021);
  KJ_EXPECT(monthly_returns[0].month == 1);
}

KJ_TEST("BacktestReporter: AnalyzeTrades") {
  auto result = create_sample_result();

  auto analysis = BacktestReporter::analyze_trades(result);

  // Check best/worst trades
  KJ_EXPECT(analysis.best_trade_pnl == 100.0);
  KJ_EXPECT(analysis.worst_trade_pnl == -50.0);

  // Check consecutive wins/losses are calculated
  KJ_EXPECT(analysis.max_consecutive_wins >= 1);
  KJ_EXPECT(analysis.max_consecutive_losses >= 1);
}

KJ_TEST("BacktestReporter: AnalyzeTradesEmpty") {
  BacktestResult result;
  result.trades = kj::Vector<TradeRecord>();

  auto analysis = BacktestReporter::analyze_trades(result);

  // Should handle empty trades gracefully
  KJ_EXPECT(analysis.best_trade_pnl == 0.0);
  KJ_EXPECT(analysis.worst_trade_pnl == 0.0);
  KJ_EXPECT(analysis.max_consecutive_wins == 0);
  KJ_EXPECT(analysis.max_consecutive_losses == 0);
}

KJ_TEST("BacktestReporter: CalculateExtendedMetrics") {
  auto result = create_sample_result();

  // Add drawdown curve for ulcer index calculation
  for (int i = 0; i < 100; ++i) {
    DrawdownPoint point;
    point.timestamp = 1609459200000 + i * 3600000;
    point.drawdown = (i % 10) * 0.01; // Varying drawdown
    result.drawdown_curve.add(point);
  }

  auto metrics = BacktestReporter::calculate_extended_metrics(result);

  // Check that metrics are calculated (not necessarily specific values)
  // Sortino ratio should be calculated
  KJ_EXPECT(metrics.sortino_ratio != 0.0 || result.trades.size() < 2);

  // Calmar ratio should be positive if we have positive return and drawdown
  KJ_EXPECT(metrics.calmar_ratio >= 0.0);

  // Omega ratio should be positive for profitable strategy
  KJ_EXPECT(metrics.omega_ratio >= 0.0);
}

KJ_TEST("BacktestReporter: GenerateReportFormat_CSV") {
  auto reporter = kj::heap<BacktestReporter>();
  auto result = create_sample_result();

  bool success =
      reporter->generate_report_format(result, "/tmp/test_report.csv"_kj, ReportFormat::CSV);
  KJ_EXPECT(success == true);
}

KJ_TEST("BacktestReporter: GenerateReportFormat_Markdown") {
  auto reporter = kj::heap<BacktestReporter>();
  auto result = create_sample_result();

  // Add equity curve for monthly returns
  for (int i = 0; i < 30; ++i) {
    EquityCurvePoint point;
    point.timestamp = 1609459200000 + i * 86400000;
    point.equity = 10000.0 + i * 50.0;
    point.cumulative_return = static_cast<double>(i) * 0.005;
    result.equity_curve.add(point);
  }

  bool success =
      reporter->generate_report_format(result, "/tmp/test_report.md"_kj, ReportFormat::Markdown);
  KJ_EXPECT(success == true);
}

KJ_TEST("BacktestReporter: ExportEquityCurveCSV") {
  auto reporter = kj::heap<BacktestReporter>();
  auto result = create_sample_result();

  // Add equity curve
  for (int i = 0; i < 10; ++i) {
    EquityCurvePoint point;
    point.timestamp = 1609459200000 + i * 86400000;
    point.equity = 10000.0 + i * 100.0;
    point.cumulative_return = static_cast<double>(i) * 0.01;
    result.equity_curve.add(point);
  }

  bool success = reporter->export_equity_curve_csv(result, "/tmp/test_equity.csv"_kj);
  KJ_EXPECT(success == true);
}

KJ_TEST("BacktestReporter: ExportDrawdownCurveCSV") {
  auto reporter = kj::heap<BacktestReporter>();
  auto result = create_sample_result();

  // Add drawdown curve
  for (int i = 0; i < 10; ++i) {
    DrawdownPoint point;
    point.timestamp = 1609459200000 + i * 86400000;
    point.drawdown = (i % 5) * 0.02;
    result.drawdown_curve.add(point);
  }

  bool success = reporter->export_drawdown_curve_csv(result, "/tmp/test_drawdown.csv"_kj);
  KJ_EXPECT(success == true);
}

KJ_TEST("BacktestReporter: GenerateComparisonReport") {
  auto reporter = kj::heap<BacktestReporter>();

  // Create multiple results
  kj::Vector<BacktestResult> results;

  BacktestResult result1 = create_sample_result();
  result1.strategy_name = kj::str("Strategy A");
  result1.total_return = 0.5;
  result1.max_drawdown = 0.1;
  result1.sharpe_ratio = 1.5;
  results.add(kj::mv(result1));

  BacktestResult result2;
  result2.strategy_name = kj::str("Strategy B");
  result2.symbol = kj::str("BTCUSDT");
  result2.initial_balance = 10000.0;
  result2.final_balance = 12000.0;
  result2.total_return = 0.2;
  result2.max_drawdown = 0.05;
  result2.sharpe_ratio = 2.0;
  result2.win_rate = 0.7;
  result2.profit_factor = 2.5;
  result2.trade_count = 50;
  result2.win_count = 35;
  result2.lose_count = 15;
  result2.avg_win = 80.0;
  result2.avg_lose = -40.0;
  results.add(kj::mv(result2));

  bool success =
      reporter->generate_comparison_report(results.asPtr(), "/tmp/test_comparison.html"_kj);
  KJ_EXPECT(success == true);
}

KJ_TEST("BacktestReporter: GenerateComparisonReportEmpty") {
  auto reporter = kj::heap<BacktestReporter>();

  kj::Vector<BacktestResult> empty_results;
  bool success = reporter->generate_comparison_report(empty_results.asPtr(),
                                                      "/tmp/test_comparison_empty.html"_kj);
  KJ_EXPECT(success == false); // Should fail with empty results
}

KJ_TEST("BacktestReporter: MonthlyReturnsEmpty") {
  BacktestResult result;
  result.equity_curve = kj::Vector<EquityCurvePoint>();

  auto monthly_returns = BacktestReporter::calculate_monthly_returns(result);
  KJ_EXPECT(monthly_returns.size() == 0);
}

KJ_TEST("BacktestReporter: ExtendedMetricsInsufficientData") {
  BacktestResult result;
  result.initial_balance = 10000.0;

  // Only one trade - insufficient for metrics
  TradeRecord trade;
  trade.timestamp = 1609459200000;
  trade.symbol = kj::str("BTCUSDT");
  trade.side = kj::str("buy");
  trade.price = 50000.0;
  trade.quantity = 0.01;
  trade.fee = 0.001;
  trade.pnl = 100.0;
  trade.strategy_id = kj::str("test");
  result.trades.add(kj::mv(trade));

  auto metrics = BacktestReporter::calculate_extended_metrics(result);

  // With insufficient data, metrics should be zero
  KJ_EXPECT(metrics.sortino_ratio == 0.0);
  KJ_EXPECT(metrics.calmar_ratio == 0.0);
}

} // namespace
