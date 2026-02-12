#include <gtest/gtest.h>
#include "veloz/backtest/reporter.h"

class BacktestReporterTest : public ::testing::Test {
protected:
    void SetUp() override {
        reporter_ = std::make_unique<veloz::backtest::BacktestReporter>();
        result_ = create_sample_result();
    }

    void TearDown() override {
        reporter_.reset();
    }

    std::unique_ptr<veloz::backtest::BacktestReporter> reporter_;
    veloz::backtest::BacktestResult result_;

private:
    veloz::backtest::BacktestResult create_sample_result() {
        veloz::backtest::BacktestResult result;
        result.strategy_name = "TestStrategy";
        result.symbol = "BTCUSDT";
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

    std::vector<veloz::backtest::TradeRecord> create_sample_trades() {
        std::vector<veloz::backtest::TradeRecord> trades;

        for (int i = 0; i < 100; ++i) {
            veloz::backtest::TradeRecord trade;
            trade.timestamp = 1609459200000 + i * 3600000; // 1 hour intervals
            trade.symbol = "BTCUSDT";
            trade.side = i % 2 == 0 ? "buy" : "sell";
            trade.price = 50000 + i * 100;
            trade.quantity = 0.01;
            trade.fee = 0.001;
            trade.pnl = (i % 3 == 0) ? 100.0 : -50.0; // 66% win rate
            trade.strategy_id = "test_strategy";
            trades.push_back(trade);
        }

        return trades;
    }
};

TEST_F(BacktestReporterTest, GenerateHTMLReport) {
    std::string html = reporter_->generate_html_report(result_);
    EXPECT_FALSE(html.empty());
    EXPECT_NE(html.find("VeloZ Backtest Report"), std::string::npos);
}

TEST_F(BacktestReporterTest, GenerateJSONReport) {
    std::string json = reporter_->generate_json_report(result_);
    EXPECT_FALSE(json.empty());
    EXPECT_NE(json.find("TestStrategy"), std::string::npos);
}

TEST_F(BacktestReporterTest, GenerateReportFile) {
    std::string test_file = "test_report.html";
    EXPECT_TRUE(reporter_->generate_report(result_, test_file));
}

TEST_F(BacktestReporterTest, ReportContainsKeyMetrics) {
    std::string html = reporter_->generate_html_report(result_);
    EXPECT_NE(html.find("50%"), std::string::npos); // Total return
    EXPECT_NE(html.find("10%"), std::string::npos); // Max drawdown
    EXPECT_NE(html.find("1.5"), std::string::npos); // Sharpe ratio
    EXPECT_NE(html.find("60%"), std::string::npos); // Win rate
    EXPECT_NE(html.find("100"), std::string::npos); // Total trades
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

// Tests for trade records in reports

TEST_F(BacktestReporterTest, HTMLReportContainsTradeHistory) {
    std::string html = reporter_->generate_html_report(result_);

    // Check that trade history section exists
    EXPECT_NE(html.find("Trade History"), std::string::npos);
    EXPECT_NE(html.find("<th>Time</th>"), std::string::npos);
    EXPECT_NE(html.find("<th>Symbol</th>"), std::string::npos);
    EXPECT_NE(html.find("<th>Side</th>"), std::string::npos);
    EXPECT_NE(html.find("<th>Price</th>"), std::string::npos);
    EXPECT_NE(html.find("<th>Quantity</th>"), std::string::npos);
    EXPECT_NE(html.find("<th>Fee</th>"), std::string::npos);
    EXPECT_NE(html.find("<th>P&L</th>"), std::string::npos);
}

TEST_F(BacktestReporterTest, HTMLReportContainsTradeData) {
    std::string html = reporter_->generate_html_report(result_);

    // Check that sample trade data is present
    EXPECT_NE(html.find("BTCUSDT"), std::string::npos);
    EXPECT_NE(html.find("buy"), std::string::npos);
    EXPECT_NE(html.find("sell"), std::string::npos);
    EXPECT_NE(html.find("0.001"), std::string::npos); // Fee
}

TEST_F(BacktestReporterTest, HTMLReportTradePnLColoring) {
    std::string html = reporter_->generate_html_report(result_);

    // Check that positive and negative P&L have proper classes
    EXPECT_NE(html.find("positive"), std::string::npos);
    EXPECT_NE(html.find("negative"), std::string::npos);
}

TEST_F(BacktestReporterTest, JSONReportContainsTradeHistory) {
    std::string json = reporter_->generate_json_report(result_);

    // Check that trades array exists
    EXPECT_NE(json.find("\"trades\""), std::string::npos);
    EXPECT_NE(json.find("\"timestamp\""), std::string::npos);
    EXPECT_NE(json.find("\"symbol\""), std::string::npos);
    EXPECT_NE(json.find("\"side\""), std::string::npos);
    EXPECT_NE(json.find("\"price\""), std::string::npos);
    EXPECT_NE(json.find("\"quantity\""), std::string::npos);
    EXPECT_NE(json.find("\"fee\""), std::string::npos);
    EXPECT_NE(json.find("\"pnl\""), std::string::npos);
}

TEST_F(BacktestReporterTest, JSONReportContainsTradeData) {
    std::string json = reporter_->generate_json_report(result_);

    // Check that sample trade data is present
    EXPECT_NE(json.find("BTCUSDT"), std::string::npos);
    EXPECT_NE(json.find("test_strategy"), std::string::npos);
}

TEST_F(BacktestReporterTest, EmptyTradeHistory) {
    veloz::backtest::BacktestResult result = create_sample_result();
    result.trades.clear();

    std::string html = reporter_->generate_html_report(result);
    std::string json = reporter_->generate_json_report(result);

    // Both reports should handle empty trade arrays
    EXPECT_NE(html.find("Trade History"), std::string::npos);
    EXPECT_NE(json.find("\"trades\""), std::string::npos);
}

TEST_F(BacktestReporterTest, SingleTradeReport) {
    veloz::backtest::BacktestResult result = create_sample_result();
    result.trades = std::vector<veloz::backtest::TradeRecord>{result.trades[0]};
    result.trade_count = 1;
    result.win_count = 1;
    result.lose_count = 0;

    std::string html = reporter_->generate_html_report(result);
    std::string json = reporter_->generate_json_report(result);

    // Both reports should handle single trade
    EXPECT_NE(html.find("1"), std::string::npos);
    EXPECT_NE(json.find("1"), std::string::npos);
}

TEST_F(BacktestReporterTest, LargeTradeHistory) {
    veloz::backtest::BacktestResult result = create_sample_result();
    // Create 1000 trades
    result.trades.clear();
    for (int i = 0; i < 1000; ++i) {
        veloz::backtest::TradeRecord trade;
        trade.timestamp = 1609459200000 + i * 1000;
        trade.symbol = "BTCUSDT";
        trade.side = (i % 2 == 0) ? "buy" : "sell";
        trade.price = 50000.0;
        trade.quantity = 0.01;
        trade.fee = 0.001;
        trade.pnl = (i % 3 == 0) ? 100.0 : -50.0;
        trade.strategy_id = "test_strategy";
        result.trades.push_back(trade);
    }
    result.trade_count = 1000;
    result.win_count = 667; // 66.7% win rate
    result.lose_count = 333;

    std::string html = reporter_->generate_html_report(result);
    std::string json = reporter_->generate_json_report(result);

    // Both reports should handle large trade arrays
    EXPECT_NE(html.find("1000"), std::string::npos);
    EXPECT_NE(json.find("1000"), std::string::npos);
}

