#include "veloz/backtest/reporter.h"

#include <gtest/gtest.h>
#include <kj/memory.h>
#include <kj/string.h>
#include <kj/vector.h>
#include <string>

// Helper function to check if kj::String contains a substring
static bool contains(kj::StringPtr str, const char* substr) {
  return std::string(str.cStr()).find(substr) != std::string::npos;
}

class BacktestReporterTest : public ::testing::Test {
public:
  ~BacktestReporterTest() noexcept override = default;

protected:
  void SetUp() override {
    reporter_ = kj::heap<veloz::backtest::BacktestReporter>();
    result_ = create_sample_result();
  }

  void TearDown() override {
    reporter_ = nullptr;
  }

  kj::Own<veloz::backtest::BacktestReporter> reporter_;
  veloz::backtest::BacktestResult result_;

protected:
  veloz::backtest::BacktestResult create_sample_result() {
    veloz::backtest::BacktestResult result;
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

  kj::Vector<veloz::backtest::TradeRecord> create_sample_trades() {
    kj::Vector<veloz::backtest::TradeRecord> trades;

    for (int i = 0; i < 100; ++i) {
      veloz::backtest::TradeRecord trade;
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
};

TEST_F(BacktestReporterTest, GenerateHTMLReport) {
  kj::String html = reporter_->generate_html_report(result_);
  EXPECT_GT(html.size(), 0);
  EXPECT_TRUE(contains(html, "VeloZ Backtest Report"));
}

TEST_F(BacktestReporterTest, GenerateJSONReport) {
  kj::String json = reporter_->generate_json_report(result_);
  EXPECT_GT(json.size(), 0);
  EXPECT_TRUE(contains(json, "TestStrategy"));
}

TEST_F(BacktestReporterTest, GenerateReportFile) {
  EXPECT_TRUE(reporter_->generate_report(result_, "test_report.html"));
}

TEST_F(BacktestReporterTest, ReportContainsKeyMetrics) {
  kj::String html = reporter_->generate_html_report(result_);
  EXPECT_TRUE(contains(html, "50%")); // Total return
  EXPECT_TRUE(contains(html, "10%")); // Max drawdown
  EXPECT_TRUE(contains(html, "1.5")); // Sharpe ratio
  EXPECT_TRUE(contains(html, "60%")); // Win rate
  EXPECT_TRUE(contains(html, "100")); // Total trades
}

TEST_F(BacktestReporterTest, HTMLReportContainsTradeHistory) {
  kj::String html = reporter_->generate_html_report(result_);

  // Check that trade history section exists
  EXPECT_TRUE(contains(html, "Trade History"));
  EXPECT_TRUE(contains(html, "<th>Time</th>"));
  EXPECT_TRUE(contains(html, "<th>Symbol</th>"));
  EXPECT_TRUE(contains(html, "<th>Side</th>"));
  EXPECT_TRUE(contains(html, "<th>Price</th>"));
  EXPECT_TRUE(contains(html, "<th>Quantity</th>"));
  EXPECT_TRUE(contains(html, "<th>Fee</th>"));
  EXPECT_TRUE(contains(html, "<th>P&L</th>"));
}

TEST_F(BacktestReporterTest, HTMLReportContainsTradeData) {
  kj::String html = reporter_->generate_html_report(result_);

  // Check that sample trade data is present
  EXPECT_TRUE(contains(html, "BTCUSDT"));
  EXPECT_TRUE(contains(html, "buy"));
  EXPECT_TRUE(contains(html, "sell"));
  EXPECT_TRUE(contains(html, "0.001")); // Fee
}

TEST_F(BacktestReporterTest, HTMLReportTradePnLColoring) {
  kj::String html = reporter_->generate_html_report(result_);

  // Check that positive and negative P&L have proper classes
  EXPECT_TRUE(contains(html, "positive"));
  EXPECT_TRUE(contains(html, "negative"));
}

TEST_F(BacktestReporterTest, JSONReportContainsTradeHistory) {
  kj::String json = reporter_->generate_json_report(result_);

  // Check that trades array exists
  EXPECT_TRUE(contains(json, "\"trades\""));
  EXPECT_TRUE(contains(json, "\"timestamp\""));
  EXPECT_TRUE(contains(json, "\"symbol\""));
  EXPECT_TRUE(contains(json, "\"side\""));
  EXPECT_TRUE(contains(json, "\"price\""));
  EXPECT_TRUE(contains(json, "\"quantity\""));
  EXPECT_TRUE(contains(json, "\"fee\""));
  EXPECT_TRUE(contains(json, "\"pnl\""));
}

TEST_F(BacktestReporterTest, JSONReportContainsTradeData) {
  kj::String json = reporter_->generate_json_report(result_);

  // Check that sample trade data is present
  EXPECT_TRUE(contains(json, "BTCUSDT"));
  EXPECT_TRUE(contains(json, "test_strategy"));
}

TEST_F(BacktestReporterTest, EmptyTradeHistory) {
  veloz::backtest::BacktestResult result = create_sample_result();
  result.trades = kj::Vector<veloz::backtest::TradeRecord>();

  kj::String html = reporter_->generate_html_report(result);
  kj::String json = reporter_->generate_json_report(result);

  // Both reports should handle empty trade arrays
  EXPECT_TRUE(contains(html, "Trade History"));
  EXPECT_TRUE(contains(json, "\"trades\""));
}

TEST_F(BacktestReporterTest, SingleTradeReport) {
  veloz::backtest::BacktestResult result = create_sample_result();
  // Create a new single-trade vector instead of copying (kj::String is not copyable)
  kj::Vector<veloz::backtest::TradeRecord> single_trade;
  veloz::backtest::TradeRecord trade;
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

  kj::String html = reporter_->generate_html_report(result);
  kj::String json = reporter_->generate_json_report(result);

  // Both reports should handle single trade
  EXPECT_TRUE(contains(html, "1"));
  EXPECT_TRUE(contains(json, "1"));
}

TEST_F(BacktestReporterTest, LargeTradeHistory) {
  veloz::backtest::BacktestResult result = create_sample_result();
  // Create 1000 trades
  result.trades = kj::Vector<veloz::backtest::TradeRecord>();
  for (int i = 0; i < 1000; ++i) {
    veloz::backtest::TradeRecord trade;
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

  kj::String html = reporter_->generate_html_report(result);
  kj::String json = reporter_->generate_json_report(result);

  // Both reports should handle large trade arrays
  EXPECT_TRUE(contains(html, "1000"));
  EXPECT_TRUE(contains(json, "1000"));
}
