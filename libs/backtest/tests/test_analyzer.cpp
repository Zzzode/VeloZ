#include <gtest/gtest.h>
#include "veloz/backtest/analyzer.h"

class BacktestAnalyzerTest : public ::testing::Test {
protected:
    void SetUp() override {
        analyzer_ = std::make_unique<veloz::backtest::BacktestAnalyzer>();
        trades_ = create_sample_trades();
    }

    void TearDown() override {
        analyzer_.reset();
        trades_.clear();
    }

    std::unique_ptr<veloz::backtest::BacktestAnalyzer> analyzer_;
    std::vector<veloz::backtest::TradeRecord> trades_;

private:
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

TEST_F(BacktestAnalyzerTest, CalculateEquityCurve) {
    auto equity_curve = analyzer_->calculate_equity_curve(trades_, 10000.0);
    EXPECT_FALSE(equity_curve.empty());
    EXPECT_GT(equity_curve.size(), 0);
}

TEST_F(BacktestAnalyzerTest, CalculateDrawdown) {
    auto equity_curve = analyzer_->calculate_equity_curve(trades_, 10000.0);
    auto drawdown_curve = analyzer_->calculate_drawdown(equity_curve);
    EXPECT_FALSE(drawdown_curve.empty());
    EXPECT_EQ(drawdown_curve.size(), equity_curve.size());
}

TEST_F(BacktestAnalyzerTest, CalculateSharpeRatio) {
    double sharpe_ratio = analyzer_->calculate_sharpe_ratio(trades_);
    EXPECT_GT(sharpe_ratio, 0);
}

TEST_F(BacktestAnalyzerTest, CalculateMaxDrawdown) {
    auto equity_curve = analyzer_->calculate_equity_curve(trades_, 10000.0);
    double max_drawdown = analyzer_->calculate_max_drawdown(equity_curve);
    EXPECT_GE(max_drawdown, 0);
}

TEST_F(BacktestAnalyzerTest, CalculateWinRate) {
    double win_rate = analyzer_->calculate_win_rate(trades_);
    EXPECT_GE(win_rate, 0);
    EXPECT_LE(win_rate, 1);
}

TEST_F(BacktestAnalyzerTest, CalculateProfitFactor) {
    double profit_factor = analyzer_->calculate_profit_factor(trades_);
    EXPECT_GT(profit_factor, 0);
}

TEST_F(BacktestAnalyzerTest, AnalyzeTrades) {
    auto result = analyzer_->analyze(trades_);
    EXPECT_TRUE(result);
    EXPECT_EQ(result->trade_count, static_cast<int>(trades_.size()));
    EXPECT_GE(result->win_count, 0);
    EXPECT_GE(result->lose_count, 0);
    EXPECT_GT(result->profit_factor, 0);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
