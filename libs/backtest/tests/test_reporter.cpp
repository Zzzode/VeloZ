#include "veloz/backtest/reporter.h"

#include <kj/test.h>
#include <kj/memory.h>
#include <kj/string.h>
#include <kj/vector.h>

#include <string>

using namespace veloz::backtest;

namespace {

// Helper function to check if kj::String contains a substring
bool contains(kj::StringPtr str, const char* substr) {
  return std::string(str.cStr()).find(substr) != std::string::npos;
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

} // namespace
