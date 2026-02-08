#include <gtest/gtest.h>
#include "veloz/backtest/data_source.h"

class DataSourceTest : public ::testing::Test {
protected:
    void SetUp() override {
        csv_data_source_ = std::make_shared<veloz::backtest::CSVDataSource>();
        binance_data_source_ = std::make_shared<veloz::backtest::BinanceDataSource>();
    }

    void TearDown() override {
        csv_data_source_.reset();
        binance_data_source_.reset();
    }

    std::shared_ptr<veloz::backtest::CSVDataSource> csv_data_source_;
    std::shared_ptr<veloz::backtest::BinanceDataSource> binance_data_source_;
};

TEST_F(DataSourceTest, CSVDataSourceCreation) {
    EXPECT_TRUE(csv_data_source_ != nullptr);
}

TEST_F(DataSourceTest, BinanceDataSourceCreation) {
    EXPECT_TRUE(binance_data_source_ != nullptr);
}

TEST_F(DataSourceTest, CSVDataSourceConnect) {
    EXPECT_TRUE(csv_data_source_->connect());
}

TEST_F(DataSourceTest, CSVDataSourceDisconnect) {
    EXPECT_TRUE(csv_data_source_->disconnect());
}

TEST_F(DataSourceTest, BinanceDataSourceConnect) {
    EXPECT_TRUE(binance_data_source_->connect());
}

TEST_F(DataSourceTest, BinanceDataSourceDisconnect) {
    EXPECT_TRUE(binance_data_source_->disconnect());
}

TEST_F(DataSourceTest, CSVDataSourceSetDataDirectory) {
    csv_data_source_->set_data_directory("/tmp/data");
    EXPECT_TRUE(true); // Should not throw
}

TEST_F(DataSourceTest, BinanceDataSourceSetAPIKey) {
    binance_data_source_->set_api_key("test_api_key");
    EXPECT_TRUE(true); // Should not throw
}

TEST_F(DataSourceTest, BinanceDataSourceSetAPISecret) {
    binance_data_source_->set_api_secret("test_api_secret");
    EXPECT_TRUE(true); // Should not throw
}

TEST_F(DataSourceTest, DataSourceFactoryCreateCSV) {
    auto data_source = veloz::backtest::DataSourceFactory::create_data_source("csv");
    EXPECT_TRUE(data_source != nullptr);
    EXPECT_TRUE(std::dynamic_pointer_cast<veloz::backtest::CSVDataSource>(data_source) != nullptr);
}

TEST_F(DataSourceTest, DataSourceFactoryCreateBinance) {
    auto data_source = veloz::backtest::DataSourceFactory::create_data_source("binance");
    EXPECT_TRUE(data_source != nullptr);
    EXPECT_TRUE(std::dynamic_pointer_cast<veloz::backtest::BinanceDataSource>(data_source) != nullptr);
}

TEST_F(DataSourceTest, DataSourceFactoryCreateUnknown) {
    auto data_source = veloz::backtest::DataSourceFactory::create_data_source("unknown");
    EXPECT_TRUE(data_source == nullptr);
}

TEST_F(DataSourceTest, CSVDataSourceGetData) {
    auto events = csv_data_source_->get_data("BTCUSDT", 1609459200000, 1640995200000, "kline", "1h");
    EXPECT_TRUE(events.empty()); // Should be empty as not implemented yet
}

TEST_F(DataSourceTest, BinanceDataSourceGetData) {
    auto events = binance_data_source_->get_data("BTCUSDT", 1609459200000, 1640995200000, "kline", "1h");
    EXPECT_TRUE(events.empty()); // Should be empty as not implemented yet
}

TEST_F(DataSourceTest, CSVDataSourceDownloadData) {
    EXPECT_FALSE(csv_data_source_->download_data("BTCUSDT", 1609459200000, 1640995200000, "kline", "1h", "test_data.csv"));
}

TEST_F(DataSourceTest, BinanceDataSourceDownloadData) {
    EXPECT_FALSE(binance_data_source_->download_data("BTCUSDT", 1609459200000, 1640995200000, "kline", "1h", "test_data.csv"));
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
