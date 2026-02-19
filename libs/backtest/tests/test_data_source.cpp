#include "veloz/backtest/data_source.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <kj/refcount.h>
#include <kj/test.h>
#include <string>

using namespace veloz::backtest;

namespace {

// Helper to check if network tests should run
bool should_run_network_tests() {
  const char* run_network_tests = std::getenv("VELOZ_RUN_NETWORK_TESTS");
  return run_network_tests && std::string(run_network_tests) == "1";
}

// Helper to check if string contains substring (for std::string)
bool contains(const std::string& str, const char* substr) {
  return str.find(substr) != std::string::npos;
}

// ============================================================================
// CSVDataSource Tests
// ============================================================================

KJ_TEST("CSVDataSource: Creation") {
  auto csv_data_source = kj::rc<CSVDataSource>();
  KJ_EXPECT(csv_data_source != nullptr);
}

KJ_TEST("CSVDataSource: Connect") {
  auto csv_data_source = kj::rc<CSVDataSource>();
  KJ_EXPECT(csv_data_source->connect());
}

KJ_TEST("CSVDataSource: Disconnect") {
  auto csv_data_source = kj::rc<CSVDataSource>();
  KJ_EXPECT(csv_data_source->disconnect());
}

KJ_TEST("CSVDataSource: SetDataDirectory") {
  auto csv_data_source = kj::rc<CSVDataSource>();
  csv_data_source->set_data_directory("/tmp/data");
  // Should not throw
}

KJ_TEST("CSVDataSource: GetData") {
  auto csv_data_source = kj::rc<CSVDataSource>();
  auto events = csv_data_source->get_data("BTCUSDT", 1609459200000, 1640995200000, "kline", "1h");
  KJ_EXPECT(events.empty()); // Should be empty as not implemented yet
}

KJ_TEST("CSVDataSource: DownloadData") {
  auto csv_data_source = kj::rc<CSVDataSource>();
  const std::string test_file = "/tmp/test_csv_data_BTCUSDT_trade.csv";
  std::int64_t start_time = 1704067200000; // 2024-01-01 00:00:00 UTC
  std::int64_t end_time = 1704068100000;   // 2024-01-01 00:15:00 UTC (15 minutes)

  // Clean up test file if it exists
  std::filesystem::remove(test_file);

  // Test successful download for trade data
  bool result = csv_data_source->download_data("BTCUSDT", start_time, end_time, "trade", "",
                                               test_file.c_str());
  KJ_EXPECT(result);

  // Verify file was created
  KJ_EXPECT(std::filesystem::exists(test_file));

  // Verify file has content (should have header + some data)
  std::ifstream file(test_file);
  std::string line;
  int line_count = 0;
  while (std::getline(file, line)) {
    line_count++;
  }
  KJ_EXPECT(line_count > 1); // At least header + 1 data line
  file.close();

  // Clean up test file
  std::filesystem::remove(test_file);
}

KJ_TEST("CSVDataSource: DownloadDataInvalidParams") {
  auto csv_data_source = kj::rc<CSVDataSource>();

  // Test with invalid parameters
  KJ_EXPECT(!csv_data_source->download_data("", 1609459200000, 1640995200000, "trade", "",
                                            "test.csv")); // Empty symbol
  KJ_EXPECT(!csv_data_source->download_data("BTCUSDT", 0, 1640995200000, "trade", "",
                                            "test.csv")); // Invalid start_time
  KJ_EXPECT(!csv_data_source->download_data("BTCUSDT", 1609459200000, 0, "trade", "",
                                            "test.csv")); // Invalid end_time
  KJ_EXPECT(!csv_data_source->download_data("BTCUSDT", 1640995200000, 1609459200000, "trade", "",
                                            "test.csv")); // end_time < start_time
  KJ_EXPECT(!csv_data_source->download_data("BTCUSDT", 1609459200000, 1640995200000, "kline", "1h",
                                            "test.csv")); // Unsupported data type
}

// ============================================================================
// BinanceDataSource Tests
// ============================================================================

KJ_TEST("BinanceDataSource: Creation") {
  auto binance_data_source = kj::rc<BinanceDataSource>();
  KJ_EXPECT(binance_data_source != nullptr);
}

KJ_TEST("BinanceDataSource: Connect") {
  auto binance_data_source = kj::rc<BinanceDataSource>();
  KJ_EXPECT(binance_data_source->connect());
}

KJ_TEST("BinanceDataSource: Disconnect") {
  auto binance_data_source = kj::rc<BinanceDataSource>();
  KJ_EXPECT(binance_data_source->disconnect());
}

KJ_TEST("BinanceDataSource: SetAPIKey") {
  auto binance_data_source = kj::rc<BinanceDataSource>();
  binance_data_source->set_api_key("test_api_key");
  // Should not throw
}

KJ_TEST("BinanceDataSource: SetAPISecret") {
  auto binance_data_source = kj::rc<BinanceDataSource>();
  binance_data_source->set_api_secret("test_api_secret");
  // Should not throw
}

KJ_TEST("BinanceDataSource: GetData") {
  if (!should_run_network_tests()) {
    return; // Skip test
  }
  auto binance_data_source = kj::rc<BinanceDataSource>();
  auto events =
      binance_data_source->get_data("BTCUSDT", 1609459200000, 1640995200000, "kline", "1h");
  KJ_EXPECT(events.empty()); // Should be empty as not implemented yet
}

KJ_TEST("BinanceDataSource: DownloadData") {
  if (!should_run_network_tests()) {
    return; // Skip test
  }
  auto binance_data_source = kj::rc<BinanceDataSource>();
  KJ_EXPECT(!binance_data_source->download_data("BTCUSDT", 1609459200000, 1640995200000, "kline",
                                                "1h", "test_data.csv"));
}

// ============================================================================
// DataSourceFactory Tests
// ============================================================================

KJ_TEST("DataSourceFactory: CreateCSV") {
  auto data_source = DataSourceFactory::create_data_source("csv");
  KJ_EXPECT(data_source != nullptr);
  // Check type by calling connect()
  KJ_EXPECT(data_source->connect());
}

KJ_TEST("DataSourceFactory: CreateBinance") {
  auto data_source = DataSourceFactory::create_data_source("binance");
  KJ_EXPECT(data_source != nullptr);
  // Check type by calling connect()
  KJ_EXPECT(data_source->connect());
}

KJ_TEST("DataSourceFactory: CreateUnknown") {
  auto data_source = DataSourceFactory::create_data_source("unknown");
  KJ_EXPECT(data_source == nullptr);
}

} // namespace
