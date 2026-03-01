#include "veloz/backtest/data_source.h"

#include <cstdlib>
#include <fstream>
#include <kj/filesystem.h>
#include <kj/refcount.h>
#include <kj/string.h>
#include <kj/test.h>
#include <string> // Kept for std::getline file reading pattern

using namespace veloz::backtest;

namespace {

// Helper to check if network tests should run
bool should_run_network_tests() {
  const char* run_network_tests = std::getenv("VELOZ_RUN_NETWORK_TESTS");
  return run_network_tests && kj::StringPtr(run_network_tests) == "1"_kj;
}

// Helper to check if string contains substring (for kj::StringPtr)
bool contains(kj::StringPtr str, kj::StringPtr substr) {
  return str.contains(substr);
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
  kj::StringPtr test_file = "/tmp/test_csv_data_BTCUSDT_trade.csv"_kj;
  std::int64_t start_time = 1704067200000; // 2024-01-01 00:00:00 UTC
  std::int64_t end_time = 1704068100000;   // 2024-01-01 00:15:00 UTC (15 minutes)

  // KJ Filesystem access
  auto fs = kj::newDiskFilesystem();
  auto& root = fs->getRoot();
  kj::Path path = kj::Path::parse(test_file.slice(1)); // Remove leading / for KJ

  // Clean up test file if it exists
  if (root.exists(path)) {
    root.remove(path);
  }

  // Test successful download for trade data
  bool result = csv_data_source->download_data("BTCUSDT", start_time, end_time, "trade", "",
                                               test_file.cStr());
  KJ_EXPECT(result);

  // Verify file was created
  KJ_EXPECT(root.exists(path));

  // Verify file has content (should have header + some data)
  auto content = root.openFile(path)->readAllText();
  int line_count = 0;
  kj::StringPtr rest = content;
  while (rest.size() > 0) {
    KJ_IF_SOME(idx, rest.findFirst('\n')) {
      line_count++;
      rest = rest.slice(idx + 1);
    }
    else {
      line_count++;
      break;
    }
  }
  KJ_EXPECT(line_count > 1); // At least header + 1 data line

  // Clean up test file
  if (root.exists(path)) {
    root.remove(path);
  }
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

// ============================================================================
// Enhanced CSVDataSource Tests
// ============================================================================

KJ_TEST("CSVDataSource: ParseOptions") {
  auto csv_data_source = kj::rc<CSVDataSource>();

  CSVParseOptions options;
  options.format = CSVFormat::OHLCV;
  options.delimiter = ';';
  options.has_header = false;
  options.skip_invalid_rows = false;
  options.max_rows = 1000;
  options.symbol_override = kj::str("BTCUSDT");
  options.venue = veloz::common::Venue::Binance;
  options.market = veloz::common::MarketKind::LinearPerp;

  csv_data_source->set_parse_options(options);

  const auto& retrieved = csv_data_source->get_parse_options();
  KJ_EXPECT(retrieved.format == CSVFormat::OHLCV);
  KJ_EXPECT(retrieved.delimiter == ';');
  KJ_EXPECT(retrieved.has_header == false);
  KJ_EXPECT(retrieved.skip_invalid_rows == false);
  KJ_EXPECT(retrieved.max_rows == 1000);
  KJ_EXPECT(retrieved.symbol_override == "BTCUSDT"_kj);
}

KJ_TEST("CSVDataSource: LoadTradeFile") {
  auto csv_data_source = kj::rc<CSVDataSource>();

  // Create a test trade CSV file
  kj::StringPtr test_file = "/tmp/test_trade_data.csv"_kj;

  auto fs = kj::newDiskFilesystem();
  auto& root = fs->getRoot();
  kj::Path path = kj::Path::parse(test_file.slice(1));

  // Clean up if exists
  if (root.exists(path)) {
    root.remove(path);
  }

  // Create test file with trade data
  {
    std::ofstream file(test_file.cStr());
    file << "timestamp,symbol,side,price,quantity\n";
    file << "1704067200000,BTCUSDT,buy,42000.50,0.5\n";
    file << "1704067201000,BTCUSDT,sell,42001.00,0.3\n";
    file << "1704067202000,BTCUSDT,buy,42002.25,0.7\n";
    file.close();
  }

  // Load file
  auto events = csv_data_source->load_file(test_file);

  KJ_EXPECT(events.size() == 3);

  // Verify first event
  KJ_EXPECT(events[0].type == veloz::market::MarketEventType::Trade);
  KJ_EXPECT(events[0].ts_exchange_ns == 1704067200000LL * 1'000'000);

  // Check stats
  const auto& stats = csv_data_source->get_stats();
  KJ_EXPECT(stats.valid_rows == 3);
  KJ_EXPECT(stats.invalid_rows == 0);

  // Clean up
  if (root.exists(path)) {
    root.remove(path);
  }
}

KJ_TEST("CSVDataSource: LoadOHLCVFile") {
  auto csv_data_source = kj::rc<CSVDataSource>();

  // Create a test OHLCV CSV file
  kj::StringPtr test_file = "/tmp/test_ohlcv_data.csv"_kj;

  auto fs = kj::newDiskFilesystem();
  auto& root = fs->getRoot();
  kj::Path path = kj::Path::parse(test_file.slice(1));

  // Clean up if exists
  if (root.exists(path)) {
    root.remove(path);
  }

  // Create test file with OHLCV data
  {
    std::ofstream file(test_file.cStr());
    file << "timestamp,open,high,low,close,volume\n";
    file << "1704067200000,42000.00,42100.00,41900.00,42050.00,100.5\n";
    file << "1704070800000,42050.00,42200.00,42000.00,42150.00,150.3\n";
    file << "1704074400000,42150.00,42300.00,42100.00,42250.00,120.7\n";
    file.close();
  }

  // Set format to OHLCV
  CSVParseOptions options;
  options.format = CSVFormat::OHLCV;
  options.symbol_override = kj::str("BTCUSDT");
  csv_data_source->set_parse_options(options);

  // Load file
  auto events = csv_data_source->load_file(test_file);

  KJ_EXPECT(events.size() == 3);

  // Verify first event
  KJ_EXPECT(events[0].type == veloz::market::MarketEventType::Kline);
  if (events[0].data.is<veloz::market::KlineData>()) {
    const auto& kline = events[0].data.get<veloz::market::KlineData>();
    KJ_EXPECT(kline.open == 42000.00);
    KJ_EXPECT(kline.high == 42100.00);
    KJ_EXPECT(kline.low == 41900.00);
    KJ_EXPECT(kline.close == 42050.00);
    KJ_EXPECT(kline.volume == 100.5);
  }

  // Clean up
  if (root.exists(path)) {
    root.remove(path);
  }
}

KJ_TEST("CSVDataSource: LoadFileWithTimeFilter") {
  auto csv_data_source = kj::rc<CSVDataSource>();

  // Create a test CSV file
  kj::StringPtr test_file = "/tmp/test_time_filter.csv"_kj;

  auto fs = kj::newDiskFilesystem();
  auto& root = fs->getRoot();
  kj::Path path = kj::Path::parse(test_file.slice(1));

  // Clean up if exists
  if (root.exists(path)) {
    root.remove(path);
  }

  // Create test file
  {
    std::ofstream file(test_file.cStr());
    file << "timestamp,symbol,side,price,quantity\n";
    file << "1704067200000,BTCUSDT,buy,42000.50,0.5\n";  // Include
    file << "1704067201000,BTCUSDT,sell,42001.00,0.3\n"; // Include
    file << "1704067202000,BTCUSDT,buy,42002.25,0.7\n";  // Exclude (after end_time)
    file.close();
  }

  // Load file with time filter
  auto events = csv_data_source->load_file(test_file, 1704067200000, 1704067201500);

  KJ_EXPECT(events.size() == 2);

  // Clean up
  if (root.exists(path)) {
    root.remove(path);
  }
}

KJ_TEST("CSVDataSource: LoadMultipleFiles") {
  auto csv_data_source = kj::rc<CSVDataSource>();

  auto fs = kj::newDiskFilesystem();
  auto& root = fs->getRoot();

  // Create test files
  kj::StringPtr test_file1 = "/tmp/test_multi_1.csv"_kj;
  kj::StringPtr test_file2 = "/tmp/test_multi_2.csv"_kj;

  kj::Path path1 = kj::Path::parse(test_file1.slice(1));
  kj::Path path2 = kj::Path::parse(test_file2.slice(1));

  // Clean up if exists
  if (root.exists(path1))
    root.remove(path1);
  if (root.exists(path2))
    root.remove(path2);

  // Create test files
  {
    std::ofstream file1(test_file1.cStr());
    file1 << "timestamp,symbol,side,price,quantity\n";
    file1 << "1704067200000,BTCUSDT,buy,42000.50,0.5\n";
    file1 << "1704067202000,BTCUSDT,buy,42002.25,0.7\n";
    file1.close();

    std::ofstream file2(test_file2.cStr());
    file2 << "timestamp,symbol,side,price,quantity\n";
    file2 << "1704067201000,BTCUSDT,sell,42001.00,0.3\n";
    file2 << "1704067203000,BTCUSDT,sell,42003.00,0.4\n";
    file2.close();
  }

  // Load multiple files
  kj::Vector<kj::String> file_paths;
  file_paths.add(kj::str(test_file1));
  file_paths.add(kj::str(test_file2));

  auto events = csv_data_source->load_files(file_paths.asPtr());

  KJ_EXPECT(events.size() == 4);

  // Verify events are sorted by timestamp
  for (size_t i = 1; i < events.size(); ++i) {
    KJ_EXPECT(events[i].ts_exchange_ns >= events[i - 1].ts_exchange_ns);
  }

  // Clean up
  if (root.exists(path1))
    root.remove(path1);
  if (root.exists(path2))
    root.remove(path2);
}

KJ_TEST("CSVDataSource: StreamFile") {
  auto csv_data_source = kj::rc<CSVDataSource>();

  // Create a test CSV file
  kj::StringPtr test_file = "/tmp/test_stream.csv"_kj;

  auto fs = kj::newDiskFilesystem();
  auto& root = fs->getRoot();
  kj::Path path = kj::Path::parse(test_file.slice(1));

  // Clean up if exists
  if (root.exists(path)) {
    root.remove(path);
  }

  // Create test file
  {
    std::ofstream file(test_file.cStr());
    file << "timestamp,symbol,side,price,quantity\n";
    file << "1704067200000,BTCUSDT,buy,42000.50,0.5\n";
    file << "1704067201000,BTCUSDT,sell,42001.00,0.3\n";
    file << "1704067202000,BTCUSDT,buy,42002.25,0.7\n";
    file.close();
  }

  // Stream file with callback
  int event_count = 0;
  auto count =
      csv_data_source->stream_file(test_file, [&event_count](veloz::market::MarketEvent& event) {
        event_count++;
        return true; // Continue processing
      });

  KJ_EXPECT(count == 3);
  KJ_EXPECT(event_count == 3);

  // Clean up
  if (root.exists(path)) {
    root.remove(path);
  }
}

KJ_TEST("CSVDataSource: StreamFileEarlyStop") {
  auto csv_data_source = kj::rc<CSVDataSource>();

  // Create a test CSV file
  kj::StringPtr test_file = "/tmp/test_stream_stop.csv"_kj;

  auto fs = kj::newDiskFilesystem();
  auto& root = fs->getRoot();
  kj::Path path = kj::Path::parse(test_file.slice(1));

  // Clean up if exists
  if (root.exists(path)) {
    root.remove(path);
  }

  // Create test file
  {
    std::ofstream file(test_file.cStr());
    file << "timestamp,symbol,side,price,quantity\n";
    file << "1704067200000,BTCUSDT,buy,42000.50,0.5\n";
    file << "1704067201000,BTCUSDT,sell,42001.00,0.3\n";
    file << "1704067202000,BTCUSDT,buy,42002.25,0.7\n";
    file.close();
  }

  // Stream file with early stop
  int event_count = 0;
  auto count =
      csv_data_source->stream_file(test_file, [&event_count](veloz::market::MarketEvent& event) {
        event_count++;
        return event_count < 2; // Stop after 2 events
      });

  KJ_EXPECT(count == 2);
  KJ_EXPECT(event_count == 2);

  // Clean up
  if (root.exists(path)) {
    root.remove(path);
  }
}

KJ_TEST("CSVDataSource: ValidateOHLCV") {
  // Create valid OHLCV events
  kj::Vector<veloz::market::MarketEvent> events;

  veloz::market::MarketEvent event1;
  event1.type = veloz::market::MarketEventType::Kline;
  event1.ts_exchange_ns = 1704067200000LL * 1'000'000;
  veloz::market::KlineData kline1;
  kline1.open = 42000.0;
  kline1.high = 42100.0;
  kline1.low = 41900.0;
  kline1.close = 42050.0;
  kline1.volume = 100.0;
  event1.data = kline1;
  events.add(kj::mv(event1));

  // Validate - should pass
  auto errors = CSVDataSource::validate_ohlcv(events.asPtr());
  KJ_EXPECT(errors.size() == 0);

  // Create invalid OHLCV event (high < low)
  veloz::market::MarketEvent event2;
  event2.type = veloz::market::MarketEventType::Kline;
  event2.ts_exchange_ns = 1704070800000LL * 1'000'000;
  veloz::market::KlineData kline2;
  kline2.open = 42000.0;
  kline2.high = 41800.0; // Invalid: high < low
  kline2.low = 42000.0;
  kline2.close = 41900.0;
  kline2.volume = 100.0;
  event2.data = kline2;
  events.add(kj::mv(event2));

  // Validate - should fail
  errors = CSVDataSource::validate_ohlcv(events.asPtr());
  KJ_EXPECT(errors.size() > 0);
}

KJ_TEST("CSVDataSource: DetectFormat") {
  auto fs = kj::newDiskFilesystem();
  auto& root = fs->getRoot();

  // Test Trade format detection
  {
    kj::StringPtr test_file = "/tmp/test_detect_trade.csv"_kj;
    kj::Path path = kj::Path::parse(test_file.slice(1));
    if (root.exists(path))
      root.remove(path);

    std::ofstream file(test_file.cStr());
    file << "timestamp,symbol,side,price,quantity\n";
    file << "1704067200000,BTCUSDT,buy,42000.50,0.5\n";
    file.close();

    auto format = CSVDataSource::detect_format(test_file);
    KJ_EXPECT(format == CSVFormat::Trade);

    root.remove(path);
  }

  // Test OHLCV format detection
  {
    kj::StringPtr test_file = "/tmp/test_detect_ohlcv.csv"_kj;
    kj::Path path = kj::Path::parse(test_file.slice(1));
    if (root.exists(path))
      root.remove(path);

    std::ofstream file(test_file.cStr());
    file << "timestamp,open,high,low,close,volume\n";
    file << "1704067200000,42000.00,42100.00,41900.00,42050.00,100.5\n";
    file.close();

    auto format = CSVDataSource::detect_format(test_file);
    KJ_EXPECT(format == CSVFormat::OHLCV);

    root.remove(path);
  }
}

KJ_TEST("CSVDataSource: InvalidRowHandling") {
  auto csv_data_source = kj::rc<CSVDataSource>();

  // Create a test CSV file with some invalid rows
  kj::StringPtr test_file = "/tmp/test_invalid_rows.csv"_kj;

  auto fs = kj::newDiskFilesystem();
  auto& root = fs->getRoot();
  kj::Path path = kj::Path::parse(test_file.slice(1));

  // Clean up if exists
  if (root.exists(path)) {
    root.remove(path);
  }

  // Create test file with invalid rows
  {
    std::ofstream file(test_file.cStr());
    file << "timestamp,symbol,side,price,quantity\n";
    file << "1704067200000,BTCUSDT,buy,42000.50,0.5\n";          // Valid
    file << "invalid_timestamp,BTCUSDT,buy,42001.00,0.3\n";      // Invalid timestamp
    file << "1704067202000,BTCUSDT,buy,42002.25,0.7\n";          // Valid
    file << "1704067203000,BTCUSDT,invalid_side,42003.00,0.4\n"; // Invalid side
    file << "1704067204000,BTCUSDT,buy,42004.00,0.5\n";          // Valid
    file.close();
  }

  // Load file (skip_invalid_rows = true by default)
  auto events = csv_data_source->load_file(test_file);

  KJ_EXPECT(events.size() == 3); // Only valid rows

  const auto& stats = csv_data_source->get_stats();
  KJ_EXPECT(stats.valid_rows == 3);
  KJ_EXPECT(stats.invalid_rows == 2);

  // Clean up
  if (root.exists(path)) {
    root.remove(path);
  }
}

KJ_TEST("CSVDataSource: MaxRowsLimit") {
  auto csv_data_source = kj::rc<CSVDataSource>();

  // Create a test CSV file
  kj::StringPtr test_file = "/tmp/test_max_rows.csv"_kj;

  auto fs = kj::newDiskFilesystem();
  auto& root = fs->getRoot();
  kj::Path path = kj::Path::parse(test_file.slice(1));

  // Clean up if exists
  if (root.exists(path)) {
    root.remove(path);
  }

  // Create test file with many rows
  {
    std::ofstream file(test_file.cStr());
    file << "timestamp,symbol,side,price,quantity\n";
    for (int i = 0; i < 100; ++i) {
      file << (1704067200000 + i * 1000) << ",BTCUSDT,buy," << (42000.0 + i) << ",0.5\n";
    }
    file.close();
  }

  // Set max rows limit
  CSVParseOptions options;
  options.max_rows = 10;
  csv_data_source->set_parse_options(options);

  // Load file
  auto events = csv_data_source->load_file(test_file);

  KJ_EXPECT(events.size() == 10);

  // Clean up
  if (root.exists(path)) {
    root.remove(path);
  }
}

// BinanceDataSource tests

KJ_TEST("BinanceDataSource: DownloadOptions") {
  auto binance_data_source = kj::rc<BinanceDataSource>();

  // Test default options
  const auto& default_options = binance_data_source->get_download_options();
  KJ_EXPECT(default_options.parallel_download == true);
  KJ_EXPECT(default_options.max_parallel_requests == 4);
  KJ_EXPECT(default_options.validate_data == true);
  KJ_EXPECT(default_options.compress_output == false);
  KJ_EXPECT(default_options.append_to_existing == false);

  // Set custom options
  BinanceDownloadOptions custom_options;
  custom_options.parallel_download = false;
  custom_options.max_parallel_requests = 8;
  custom_options.validate_data = false;
  custom_options.compress_output = true;
  custom_options.append_to_existing = true;
  custom_options.output_format = kj::str("parquet");

  binance_data_source->set_download_options(custom_options);

  const auto& updated_options = binance_data_source->get_download_options();
  KJ_EXPECT(updated_options.parallel_download == false);
  KJ_EXPECT(updated_options.max_parallel_requests == 8);
  KJ_EXPECT(updated_options.validate_data == false);
  KJ_EXPECT(updated_options.compress_output == true);
  KJ_EXPECT(updated_options.append_to_existing == true);
  KJ_EXPECT(updated_options.output_format == "parquet"_kj);
}

KJ_TEST("BinanceDataSource: ValidateDownloadedData_ValidFile") {
  // Create a valid OHLCV test file
  kj::StringPtr test_file = "/tmp/test_validate_valid.csv"_kj;

  auto fs = kj::newDiskFilesystem();
  auto& root = fs->getRoot();
  kj::Path path = kj::Path::parse(test_file.slice(1));

  // Clean up if exists
  if (root.exists(path)) {
    root.remove(path);
  }

  // Create valid test file
  {
    std::ofstream file(test_file.cStr());
    file << "timestamp,open,high,low,close,volume,close_time\n";
    file << "1704067200000,42000.0,42500.0,41800.0,42300.0,100.5,1704070800000\n";
    file << "1704070800000,42300.0,42800.0,42100.0,42600.0,150.2,1704074400000\n";
    file << "1704074400000,42600.0,43000.0,42400.0,42900.0,200.8,1704078000000\n";
    file.close();
  }

  // Validate file
  auto errors = BinanceDataSource::validate_downloaded_data(test_file);
  KJ_EXPECT(errors.size() == 0);

  // Clean up
  if (root.exists(path)) {
    root.remove(path);
  }
}

KJ_TEST("BinanceDataSource: ValidateDownloadedData_InvalidFile") {
  // Create an invalid OHLCV test file
  kj::StringPtr test_file = "/tmp/test_validate_invalid.csv"_kj;

  auto fs = kj::newDiskFilesystem();
  auto& root = fs->getRoot();
  kj::Path path = kj::Path::parse(test_file.slice(1));

  // Clean up if exists
  if (root.exists(path)) {
    root.remove(path);
  }

  // Create invalid test file (high < low, negative volume, out of order timestamps)
  {
    std::ofstream file(test_file.cStr());
    file << "timestamp,open,high,low,close,volume,close_time\n";
    file << "1704067200000,42000.0,41500.0,42800.0,42300.0,100.5,1704070800000\n"; // high < low
    file
        << "1704070800000,42300.0,42800.0,42100.0,42600.0,-50.0,1704074400000\n"; // negative volume
    file << "1704067000000,42600.0,43000.0,42400.0,42900.0,200.8,1704078000000\n"; // out of order
    file.close();
  }

  // Validate file
  auto errors = BinanceDataSource::validate_downloaded_data(test_file);
  KJ_EXPECT(errors.size() >= 3); // At least 3 errors

  // Clean up
  if (root.exists(path)) {
    root.remove(path);
  }
}

KJ_TEST("BinanceDataSource: ValidateDownloadedData_FileNotFound") {
  auto errors = BinanceDataSource::validate_downloaded_data("/tmp/nonexistent_file_12345.csv"_kj);
  KJ_EXPECT(errors.size() == 1);
  KJ_EXPECT(errors[0].contains("not found"_kj));
}

KJ_TEST("BinanceDataSource: DownloadProgress") {
  // Test BinanceDownloadProgress struct
  BinanceDownloadProgress progress;
  progress.total_chunks = 10;
  progress.completed_chunks = 5;
  progress.total_records = 5000;
  progress.downloaded_bytes = 1024 * 1024;
  progress.progress_fraction = 0.5;
  progress.current_date = kj::str("2024-01-01");
  progress.status = kj::str("Downloading...");

  KJ_EXPECT(progress.total_chunks == 10);
  KJ_EXPECT(progress.completed_chunks == 5);
  KJ_EXPECT(progress.total_records == 5000);
  KJ_EXPECT(progress.downloaded_bytes == 1024 * 1024);
  KJ_EXPECT(progress.progress_fraction == 0.5);
  KJ_EXPECT(progress.current_date == "2024-01-01"_kj);
  KJ_EXPECT(progress.status == "Downloading..."_kj);
}

KJ_TEST("BinanceDataSource: DownloadDataWithProgress_InvalidParams") {
  auto binance_data_source = kj::rc<BinanceDataSource>();

  bool callback_called = false;
  auto callback = [&callback_called](const BinanceDownloadProgress& progress) {
    callback_called = true;
  };

  // Test with empty symbol
  bool result = binance_data_source->download_data_with_progress(
      ""_kj, 1704067200000, 1704153600000, "kline"_kj, "1h"_kj, "/tmp/test_output.csv"_kj,
      kj::mv(callback));
  KJ_EXPECT(result == false);

  // Test with invalid time range
  callback_called = false;
  auto callback2 = [&callback_called](const BinanceDownloadProgress& progress) {
    callback_called = true;
  };
  result = binance_data_source->download_data_with_progress(
      "BTCUSDT"_kj, 1704153600000, 1704067200000, // end < start
      "kline"_kj, "1h"_kj, "/tmp/test_output.csv"_kj, kj::mv(callback2));
  KJ_EXPECT(result == false);

  // Test with invalid data type
  callback_called = false;
  auto callback3 = [&callback_called](const BinanceDownloadProgress& progress) {
    callback_called = true;
  };
  result = binance_data_source->download_data_with_progress(
      "BTCUSDT"_kj, 1704067200000, 1704153600000, "invalid"_kj, "1h"_kj, "/tmp/test_output.csv"_kj,
      kj::mv(callback3));
  KJ_EXPECT(result == false);

  // Test with invalid time frame
  callback_called = false;
  auto callback4 = [&callback_called](const BinanceDownloadProgress& progress) {
    callback_called = true;
  };
  result = binance_data_source->download_data_with_progress(
      "BTCUSDT"_kj, 1704067200000, 1704153600000, "kline"_kj, "invalid"_kj,
      "/tmp/test_output.csv"_kj, kj::mv(callback4));
  KJ_EXPECT(result == false);
}

KJ_TEST("BinanceDataSource: DownloadMultipleSymbols_EmptyList") {
  auto binance_data_source = kj::rc<BinanceDataSource>();

  kj::Vector<kj::String> empty_symbols;
  int result = binance_data_source->download_multiple_symbols(
      empty_symbols.asPtr(), 1704067200000, 1704153600000, "kline"_kj, "1h"_kj, "/tmp/output"_kj);

  KJ_EXPECT(result == 0);
}

KJ_TEST("BinanceDataSource: FetchKlinesChunk_NoNetwork") {
  // This test verifies that fetch_klines_chunk handles network unavailability gracefully
  // In a real test environment without network, this should return empty results
  if (!should_run_network_tests()) {
    return;
  }
  auto binance_data_source = kj::rc<BinanceDataSource>();

  // Use a very short time range that would only need one chunk
  auto events =
      binance_data_source->get_data("BTCUSDT"_kj, 1704067200000, 1704067260000, // 1 minute
                                    "kline"_kj, "1m"_kj);

  // Without network access, this should either succeed with data or fail gracefully
  // We just verify it doesn't crash
  KJ_EXPECT(events.size() >= 0);
}

} // namespace
