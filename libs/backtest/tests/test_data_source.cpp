#include "veloz/backtest/data_source.h"

#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>

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
  EXPECT_TRUE(std::dynamic_pointer_cast<veloz::backtest::BinanceDataSource>(data_source) !=
              nullptr);
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
  auto events =
      binance_data_source_->get_data("BTCUSDT", 1609459200000, 1640995200000, "kline", "1h");
  EXPECT_TRUE(events.empty()); // Should be empty as not implemented yet
}

TEST_F(DataSourceTest, CSVDataSourceDownloadData) {
  // Test download_data with valid parameters for "trade" data type
  const std::string test_file = "/tmp/test_csv_data_BTCUSDT_trade.csv";
  std::int64_t start_time = 1704067200000; // 2024-01-01 00:00:00 UTC
  std::int64_t end_time = 1704068100000;   // 2024-01-01 00:15:00 UTC (15 minutes)

  // Clean up test file if it exists
  std::filesystem::remove(test_file);

  // Test successful download for trade data
  bool result =
      csv_data_source_->download_data("BTCUSDT", start_time, end_time, "trade", "", test_file);
  EXPECT_TRUE(result);

  // Verify file was created
  EXPECT_TRUE(std::filesystem::exists(test_file));

  // Verify file has content (should have header + some data)
  std::ifstream file(test_file);
  std::string line;
  int line_count = 0;
  while (std::getline(file, line)) {
    line_count++;
  }
  EXPECT_GT(line_count, 1); // At least header + 1 data line
  file.close();

  // Clean up test file
  std::filesystem::remove(test_file);
}

TEST_F(DataSourceTest, CSVDataSourceDownloadDataInvalidParams) {
  // Test with invalid parameters
  EXPECT_FALSE(csv_data_source_->download_data("", 1609459200000, 1640995200000, "trade", "",
                                               "test.csv")); // Empty symbol
  EXPECT_FALSE(csv_data_source_->download_data("BTCUSDT", 0, 1640995200000, "trade", "",
                                               "test.csv")); // Invalid start_time
  EXPECT_FALSE(csv_data_source_->download_data("BTCUSDT", 1609459200000, 0, "trade", "",
                                               "test.csv")); // Invalid end_time
  EXPECT_FALSE(csv_data_source_->download_data("BTCUSDT", 1640995200000, 1609459200000, "trade", "",
                                               "test.csv")); // end_time < start_time
  EXPECT_FALSE(csv_data_source_->download_data("BTCUSDT", 1609459200000, 1640995200000, "kline",
                                               "1h", "test.csv")); // Unsupported data type
}

TEST_F(DataSourceTest, BinanceDataSourceDownloadData) {
  EXPECT_FALSE(binance_data_source_->download_data("BTCUSDT", 1609459200000, 1640995200000, "kline",
                                                   "1h", "test_data.csv"));
}
