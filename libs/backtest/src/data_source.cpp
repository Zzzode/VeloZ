#include "veloz/backtest/data_source.h"

// std library includes with justifications
#include <algorithm> // std::remove_if - standard algorithm (no KJ equivalent)
#include <chrono>    // std::chrono - wall clock time (KJ time is async I/O only)
#include <format>    // std::format - needed for width/precision specifiers (kj::str lacks this)
#include <fstream>   // std::ifstream, std::ofstream - file I/O (no KJ equivalent)
#include <iomanip>   // std::setprecision, std::fixed - output formatting (no KJ equivalent)
#include <iostream>  // std::cerr - error output (no KJ equivalent)
#include <random>    // std::random_device, std::mt19937 - KJ has no RNG
#include <thread>    // std::this_thread::sleep_for - thread sleep (no KJ equivalent)

// KJ library includes
#include <kj/common.h>
#include <kj/debug.h>
#include <kj/exception.h>
#include <kj/filesystem.h>
#include <kj/mutex.h>
#include <kj/string.h>
#include <kj/vector.h>

// VeloZ includes (after KJ to avoid header ordering issues)
#include "veloz/core/json.h"
#include "veloz/core/logger.h"

#ifndef VELOZ_NO_CURL
#include <curl/curl.h>
#endif

namespace {

using namespace veloz::core;

// RAII wrapper for CURL handle - ensures proper cleanup
class CurlHandle {
public:
  CurlHandle() : handle_(curl_easy_init()) {
    if (!handle_) {
      std::cerr << "Failed to initialize CURL handle" << std::endl;
    }
  }

  // Disable copy - CURL handles cannot be copied
  CurlHandle(const CurlHandle&) = delete;
  CurlHandle& operator=(const CurlHandle&) = delete;

  // Enable move
  CurlHandle(CurlHandle&& other) noexcept : handle_(other.handle_) {
    other.handle_ = nullptr;
  }

  CurlHandle& operator=(CurlHandle&& other) noexcept {
    if (this != &other) {
      cleanup();
      handle_ = other.handle_;
      other.handle_ = nullptr;
    }
    return *this;
  }

  ~CurlHandle() {
    cleanup();
  }

  CURL* get() const {
    return handle_;
  }
  explicit operator bool() const {
    return handle_ != nullptr;
  }

private:
  void cleanup() {
    if (handle_) {
      curl_easy_cleanup(handle_);
      handle_ = nullptr;
    }
  }

  CURL* handle_;
};

// HTTP helper functions for Binance API
#ifndef VELOZ_NO_CURL
// Static helper for CURL write callback - uses kj::ArrayPtr<char> instead of void*/char*
size_t write_callback(void* contents, size_t size, size_t nmemb, void* userp) {
  size_t total_size = size * nmemb;
  auto* buffer = static_cast<kj::Vector<char>*>(userp);
  auto data_ptr = kj::ArrayPtr<char>(static_cast<char*>(contents), total_size);
  buffer->addAll(data_ptr.begin(), data_ptr.end());
  return total_size;
}

kj::String http_get(kj::StringPtr url, long timeout_sec = 10L) {
  CurlHandle curl;
  if (!curl) {
    return kj::str("");
  }

  kj::Vector<char> response_string;

  curl_easy_setopt(curl.get(), CURLOPT_URL, url);
  curl_easy_setopt(curl.get(), CURLOPT_WRITEFUNCTION, write_callback);
  curl_easy_setopt(curl.get(), CURLOPT_WRITEDATA, &response_string);
  curl_easy_setopt(curl.get(), CURLOPT_FOLLOWLOCATION, 1L);
  curl_easy_setopt(curl.get(), CURLOPT_TIMEOUT, timeout_sec);

  CURLcode res = curl_easy_perform(curl.get());
  if (res != CURLE_OK) {
    std::cerr << "HTTP GET failed: " << curl_easy_strerror(res) << std::endl;
  }

  // RAII cleanup happens automatically via CurlHandle destructor

  response_string.add('\0');
  return kj::String(response_string.releaseAsArray());
}
#endif

// Convert symbol to uppercase for Binance API
kj::String format_symbol(kj::StringPtr symbol) {
  kj::Vector<char> formatted;
  for (char c : symbol) {
    formatted.add(static_cast<char>(std::toupper(static_cast<unsigned char>(c))));
  }
  formatted.add('\0');
  return kj::String(formatted.releaseAsArray());
}

// Validate time frame for Binance API
bool is_valid_time_frame(kj::StringPtr time_frame) {
  static const kj::Vector<kj::StringPtr> valid_frames = [] {
    kj::Vector<kj::StringPtr> v;
    v.add("1s"_kj);
    v.add("1m"_kj);
    v.add("3m"_kj);
    v.add("5m"_kj);
    v.add("15m"_kj);
    v.add("30m"_kj);
    v.add("1h"_kj);
    v.add("2h"_kj);
    v.add("4h"_kj);
    v.add("6h"_kj);
    v.add("8h"_kj);
    v.add("12h"_kj);
    v.add("1d"_kj);
    v.add("3d"_kj);
    v.add("1w"_kj);
    v.add("1M"_kj);
    return v;
  }();

  for (const auto& frame : valid_frames) {
    if (time_frame == frame) {
      return true;
    }
  }
  return false;
}

kj::String to_lower_ascii(kj::StringPtr input) {
  auto chars = kj::heapArray<char>(input.size() + 1);
  for (size_t i = 0; i < input.size(); ++i) {
    char c = input[i];
    if (c >= 'A' && c <= 'Z') {
      c = static_cast<char>(c - 'A' + 'a');
    }
    chars[i] = c;
  }
  chars[input.size()] = '\0';
  return kj::String(kj::mv(chars));
}

} // anonymous namespace

namespace veloz::backtest {

// BaseDataSource implementation
BaseDataSource::BaseDataSource() : is_connected_(false) {}

BaseDataSource::~BaseDataSource() {
  if (is_connected_) {
    disconnect();
  }
}

bool BaseDataSource::connect() {
  if (is_connected_) {
    return true;
  }

  is_connected_ = true;
  return true;
}

bool BaseDataSource::disconnect() {
  if (!is_connected_) {
    return true;
  }

  is_connected_ = false;
  return true;
}

// CSVDataSource implementation
CSVDataSource::CSVDataSource() : data_directory_(kj::str(".")) {}

CSVDataSource::~CSVDataSource() noexcept {}

kj::Vector<veloz::market::MarketEvent>
CSVDataSource::get_data(kj::StringPtr symbol, std::int64_t start_time, std::int64_t end_time,
                        kj::StringPtr data_type, kj::StringPtr time_frame) {
  kj::Vector<veloz::market::MarketEvent> events;
  veloz::core::Logger logger;

  // Construct file path using kj::Path: data_directory / symbol_data_type.csv
  // Example: /data/BTCUSDT_trade.csv or /data/BTCUSDT_kline_1h.csv
  auto dataDir = kj::Path::parse("."_kj).eval(data_directory_);
  kj::Path file_path = kj::mv(dataDir).append(data_type == "kline"_kj && time_frame.size() > 0
                                                  ? kj::str(symbol, "_", time_frame, ".csv")
                                                  : kj::str(symbol, "_", data_type, ".csv"));

  bool filePathAbsolute = data_directory_.size() > 0 && data_directory_[0] == '/';
  auto path_str = file_path.toString(filePathAbsolute);
  logger.info(kj::str("Reading data from: ", path_str.cStr()));

  auto fs = kj::newDiskFilesystem();
  auto& root = fs->getRoot();
  if (!root.exists(file_path)) {
    logger.error(kj::str("File not found: ", path_str.cStr()));
    return events;
  }
  auto file = root.openFile(file_path);
  auto file_content = file->readAllText();
  kj::StringPtr file_content_ptr = file_content;
  int line_number = 0;
  int skipped_lines = 0;

  // Use KJ exception handling pattern for file reading
  // kj::runCatchingExceptions returns Maybe<Exception> - contains exception if one was thrown
  KJ_IF_SOME(
      outer_exception, kj::runCatchingExceptions([&]() {
        size_t line_start = 0;
        for (size_t i = 0; i <= file_content_ptr.size(); ++i) {
          if (i < file_content_ptr.size() && file_content_ptr[i] != '\n') {
            continue;
          }
          auto line_slice = file_content_ptr.slice(line_start, i);
          line_start = i + 1;
          line_number++;
          if (line_slice.size() == 0) {
            continue;
          }
          if (line_slice[line_slice.size() - 1] == '\r') {
            line_slice = line_slice.first(line_slice.size() - 1);
            if (line_slice.size() == 0) {
              continue;
            }
          }
          kj::String line = kj::heapString(line_slice);
          kj::StringPtr line_ptr = line;

          // Skip empty lines
          if (line_ptr.size() == 0) {
            continue;
          }

          // Skip header line (contains column names)
          if (line_number == 1 &&
              (line_ptr.contains("timestamp"_kj) || line_ptr.contains("Timestamp"_kj))) {
            continue;
          }

          // Parse CSV line
          kj::Vector<kj::String> tokens;
          size_t token_start = 0;
          for (size_t j = 0; j <= line_ptr.size(); ++j) {
            if (j < line_ptr.size() && line_ptr[j] != ',') {
              continue;
            }
            auto token_slice = line_ptr.slice(token_start, j);
            token_start = j + 1;
            size_t start = 0;
            while (start < token_slice.size() &&
                   (token_slice[start] == ' ' || token_slice[start] == '\t' ||
                    token_slice[start] == '\r' || token_slice[start] == '\n')) {
              ++start;
            }
            size_t end = token_slice.size();
            while (end > start && (token_slice[end - 1] == ' ' || token_slice[end - 1] == '\t' ||
                                   token_slice[end - 1] == '\r' || token_slice[end - 1] == '\n')) {
              --end;
            }
            auto trimmed = token_slice.slice(start, end);
            tokens.add(kj::heapString(trimmed));
          }

          // Validate minimum number of columns
          // Expected format: timestamp,symbol,side,price,quantity
          if (tokens.size() < 5) {
            skipped_lines++;
            logger.warn(kj::str("Skipping malformed line ", line_number,
                                ": insufficient columns (expected 5, got ", tokens.size(), ")"));
            continue;
          }

          // Parse values
          veloz::market::MarketEvent event;
          veloz::market::TradeData trade_data;

          // Use KJ exception handling for parsing individual lines
          KJ_IF_SOME(inner_exception, kj::runCatchingExceptions([&]() {
                       // Parse timestamp (convert milliseconds to nanoseconds)
                       std::int64_t timestamp_ms = tokens[0].parseAs<long long>();
                       event.ts_exchange_ns = timestamp_ms * 1'000'000; // ms to ns
                       event.ts_recv_ns = event.ts_exchange_ns;
                       event.ts_pub_ns = event.ts_exchange_ns;

                       // Set symbol
                       event.symbol = veloz::common::SymbolId(tokens[1]);

                       // Set venue (default to Binance for CSV data)
                       event.venue = veloz::common::Venue::Binance;
                       event.market = veloz::common::MarketKind::Spot;

                       // Parse side and set event type
                       kj::String side_lower = to_lower_ascii(tokens[2]);

                       if (side_lower == "buy"_kj) {
                         trade_data.is_buyer_maker = false;
                       } else if (side_lower == "sell"_kj) {
                         trade_data.is_buyer_maker = true;
                       } else {
                         skipped_lines++;
                         logger.warn(kj::str("Skipping malformed line ", line_number,
                                             ": invalid side '", side_lower, "'"));
                         return;
                       }

                       // Parse price
                       trade_data.price = tokens[3].parseAs<double>();
                       if (trade_data.price <= 0) {
                         skipped_lines++;
                         logger.warn(kj::str("Skipping malformed line ", line_number,
                                             ": invalid price '", tokens[3], "'"));
                         return;
                       }

                       // Parse quantity
                       trade_data.qty = tokens[4].parseAs<double>();
                       if (trade_data.qty <= 0) {
                         skipped_lines++;
                         logger.warn(kj::str("Skipping malformed line ", line_number,
                                             ": invalid quantity '", tokens[4], "'"));
                         return;
                       }

                       // Set trade_id (use line number as fallback)
                       trade_data.trade_id = static_cast<std::int64_t>(line_number);

                       // Set event type
                       event.type = veloz::market::MarketEventType::Trade;
                       event.data = trade_data;

                       // Create JSON payload for backward compatibility
                       event.payload = kj::str(
                           "{\"type\":\"trade\",\"symbol\":\"", tokens[1],
                           "\",\"timestamp\":", timestamp_ms, ",\"price\":", trade_data.price,
                           ",\"quantity\":", trade_data.qty, ",\"side\":\"", tokens[2], "\"}");

                       // Apply time filters
                       if (start_time > 0 && event.ts_exchange_ns < start_time * 1'000'000) {
                         return;
                       }
                       if (end_time > 0 && event.ts_exchange_ns > end_time * 1'000'000) {
                         return;
                       }

                       events.add(kj::mv(event));
                     })) {
            // Inner exception occurred - log and continue to next line
            skipped_lines++;
            logger.warn(kj::str("Skipping malformed line ", line_number, ": ",
                                inner_exception.getDescription()));
          }
        }

        logger.info(kj::str("Successfully read ", events.size(), " events from ", path_str.cStr(),
                            " (skipped ", skipped_lines, " malformed lines)"));
      })) {
    // Outer exception occurred - log file reading error
    logger.error(
        kj::str("Error reading file ", path_str.cStr(), ": ", outer_exception.getDescription()));
  }

  return events;
}

bool CSVDataSource::download_data(kj::StringPtr symbol, std::int64_t start_time,
                                  std::int64_t end_time, kj::StringPtr data_type,
                                  kj::StringPtr /*time_frame*/, kj::StringPtr output_path) {
  veloz::core::Logger logger;

  // Validate parameters
  if (start_time <= 0) {
    logger.error("download_data: Invalid start_time (must be > 0)");
    return false;
  }

  if (end_time <= 0) {
    logger.error("download_data: Invalid end_time (must be > 0)");
    return false;
  }

  if (end_time <= start_time) {
    logger.error(kj::str("download_data: end_time (", end_time,
                         ") must be greater than start_time (", start_time, ")"));
    return false;
  }

  if (symbol.size() == 0) {
    logger.error("download_data: Symbol cannot be empty");
    return false;
  }

  // For this implementation, we only support "trade" data type
  // Other data types can be implemented in the future (kline, book)
  if (data_type != "trade"_kj) {
    logger.error(kj::str("download_data: Unsupported data type '", data_type,
                         "'. Only 'trade' is supported."));
    return false;
  }

  // Create output directory if it doesn't exist using kj::Path
  kj::Path output_file_path = kj::Path::parse("."_kj).eval(output_path);

  bool outputPathAbsolute = output_path.size() > 0 && output_path[0] == '/';
  try {
    // kj::Path::parent() returns parent directory
    auto parent = output_file_path.parent();
    if (parent.size() > 0) {
      auto fs = kj::newDiskFilesystem();
      auto& root = outputPathAbsolute ? fs->getRoot() : fs->getCurrent();

      if (!root.exists(parent)) {
        // Use KJ filesystem to create directory
        root.openSubdir(parent, kj::WriteMode::CREATE | kj::WriteMode::CREATE_PARENT);
        logger.info(kj::str("Created output directory: ", parent.toString(outputPathAbsolute)));
      }
    }
  } catch (const kj::Exception& e) {
    logger.error(
        kj::str("download_data: Filesystem error creating directory: ", e.getDescription()));
    return false;
  }

  // Open output file - convert kj::Path string representation to const char*
  auto output_path_str = output_file_path.toString(outputPathAbsolute);
  std::ofstream output_file(output_path_str.cStr());
  if (!output_file.is_open()) {
    logger.error(
        kj::str("download_data: Failed to open output file for writing: ", output_path_str));
    return false;
  }

  // Write CSV header
  output_file << "timestamp,symbol,side,price,quantity\n";

  // Initialize random number generator with seed based on symbol and start_time
  // This ensures deterministic output for the same parameters
  std::uint64_t seed =
      static_cast<std::uint64_t>(kj::hashCode(symbol)) ^ static_cast<std::uint64_t>(start_time);
  std::mt19937_64 rng(seed);

  // Synthetic data generation parameters
  const double base_price = 50000.0;     // Base price in USD (e.g., for BTC)
  const double price_volatility = 0.002; // 0.2% per tick
  const double trend_strength = 0.0001;  // Small upward trend per tick
  const double min_qty = 0.001;          // Minimum trade quantity
  const double max_qty = 1.0;            // Maximum trade quantity

  // Calculate duration in milliseconds
  std::int64_t duration_ms = end_time - start_time;

  // Average interval between trades (e.g., 100ms = 10 trades per second)
  const std::int64_t avg_trade_interval_ms = 100;
  const std::int64_t total_trades = std::max(std::int64_t(1), duration_ms / avg_trade_interval_ms);

  logger.info(kj::str("Generating ", total_trades, " synthetic trade records for symbol ", symbol,
                      " from ", start_time, " to ", end_time));

  // Generate synthetic data using geometric Brownian motion with trend
  double current_price = base_price;
  std::normal_distribution<double> price_dist(0.0, price_volatility);
  std::uniform_real_distribution<double> qty_dist(min_qty, max_qty);
  std::bernoulli_distribution side_dist(0.5); // 50% buy, 50% sell

  int records_written = 0;
  std::int64_t current_time = start_time;

  for (std::int64_t i = 0; i < total_trades; ++i) {
    // Generate random price change with trend
    double price_change = current_price * (price_dist(rng) + trend_strength);
    current_price += price_change;

    // Ensure price stays positive
    if (current_price <= 0) {
      current_price = base_price * 0.1; // Reset to 10% of base price
    }

    // Generate random quantity
    double quantity = qty_dist(rng);

    // Generate random side (buy/sell)
    kj::StringPtr side = side_dist(rng) ? "buy"_kj : "sell"_kj;

    // Advance time with some randomness
    std::int64_t time_increment =
        avg_trade_interval_ms + static_cast<std::int64_t>(rng() % 100); // Add 0-100ms variation
    current_time = std::min(current_time + time_increment, end_time);

    // Use std::format for precision formatting - kj::str() doesn't support {:.2f}
    kj::String price_str = kj::str(std::format("{:.2f}", current_price).c_str());

    // Format quantity to appropriate precision (up to 6 decimal places)
    kj::String qty_str;
    if (quantity < 0.01) {
      qty_str = kj::str(std::format("{:.6f}", quantity).c_str());
    } else if (quantity < 1.0) {
      qty_str = kj::str(std::format("{:.4f}", quantity).c_str());
    } else {
      qty_str = kj::str(std::format("{:.2f}", quantity).c_str());
    }

    // Write CSV record
    output_file << current_time << "," << symbol.cStr() << "," << side.cStr() << ","
                << price_str.cStr() << "," << qty_str.cStr() << "\n";

    records_written++;
  }

  output_file.close();

  // Verify file was written successfully
  if (!output_file.good()) {
    logger.error(
        kj::str("download_data: Error occurred while writing to file: ", output_path_str.cStr()));
    return false;
  }

  logger.info(kj::str("Successfully generated ", records_written,
                      " trade records to: ", output_path_str.cStr()));

  return true;
}

void CSVDataSource::set_data_directory(kj::StringPtr directory) {
  data_directory_ = kj::str(directory);
}

void CSVDataSource::set_parse_options(const CSVParseOptions& options) {
  options_.format = options.format;
  options_.delimiter = options.delimiter;
  options_.has_header = options.has_header;
  options_.skip_invalid_rows = options.skip_invalid_rows;
  options_.max_rows = options.max_rows;
  options_.symbol_override = kj::str(options.symbol_override);
  options_.venue = options.venue;
  options_.market = options.market;
}

const CSVParseOptions& CSVDataSource::get_parse_options() const {
  return options_;
}

const CSVParseStats& CSVDataSource::get_stats() const {
  return stats_;
}

kj::Vector<kj::String> CSVDataSource::tokenize_line(kj::StringPtr line) {
  kj::Vector<kj::String> tokens;
  size_t token_start = 0;
  char delim = options_.delimiter;

  for (size_t j = 0; j <= line.size(); ++j) {
    if (j < line.size() && line[j] != delim) {
      continue;
    }
    auto token_slice = line.slice(token_start, j);
    token_start = j + 1;

    // Trim whitespace
    size_t start = 0;
    while (start < token_slice.size() &&
           (token_slice[start] == ' ' || token_slice[start] == '\t' || token_slice[start] == '\r' ||
            token_slice[start] == '\n')) {
      ++start;
    }
    size_t end = token_slice.size();
    while (end > start && (token_slice[end - 1] == ' ' || token_slice[end - 1] == '\t' ||
                           token_slice[end - 1] == '\r' || token_slice[end - 1] == '\n')) {
      --end;
    }
    auto trimmed = token_slice.slice(start, end);
    tokens.add(kj::heapString(trimmed));
  }

  return tokens;
}

CSVFormat CSVDataSource::detect_format_from_header(kj::StringPtr header_line) {
  kj::String lower_header = to_lower_ascii(header_line);

  // Check for OHLCV format
  if (lower_header.contains("open"_kj) && lower_header.contains("high"_kj) &&
      lower_header.contains("low"_kj) && lower_header.contains("close"_kj)) {
    return CSVFormat::OHLCV;
  }

  // Check for Book format
  if (lower_header.contains("bid"_kj) && lower_header.contains("ask"_kj)) {
    return CSVFormat::Book;
  }

  // Check for Trade format
  if (lower_header.contains("side"_kj) ||
      (lower_header.contains("price"_kj) && lower_header.contains("quantity"_kj))) {
    return CSVFormat::Trade;
  }

  // Default to Trade format
  return CSVFormat::Trade;
}

CSVFormat CSVDataSource::detect_format(kj::StringPtr file_path) {
  veloz::core::Logger logger;

  auto fs = kj::newDiskFilesystem();
  auto path = kj::Path::parse("."_kj).eval(file_path);
  bool isAbsolute = file_path.size() > 0 && file_path[0] == '/';
  auto& root = isAbsolute ? fs->getRoot() : fs->getCurrent();

  if (!root.exists(path)) {
    logger.warn(kj::str("File not found for format detection: ", file_path));
    return CSVFormat::Auto;
  }

  auto file = root.openFile(path);
  auto content = file->readAllText();

  // Find first line
  size_t line_end = 0;
  for (size_t i = 0; i < content.size(); ++i) {
    if (content[i] == '\n') {
      line_end = i;
      break;
    }
  }

  if (line_end == 0) {
    line_end = content.size();
  }

  kj::String first_line = kj::heapString(content.slice(0, line_end));

  // Remove trailing \r if present
  if (first_line.size() > 0 && first_line[first_line.size() - 1] == '\r') {
    first_line = kj::heapString(first_line.slice(0, first_line.size() - 1));
  }

  CSVDataSource temp;
  return temp.detect_format_from_header(first_line);
}

kj::Maybe<veloz::market::MarketEvent>
CSVDataSource::parse_trade_row(kj::ArrayPtr<const kj::String> tokens, std::int64_t line_number) {
  // Expected format: timestamp,symbol,side,price,quantity
  if (tokens.size() < 5) {
    return kj::none;
  }

  veloz::market::MarketEvent event;
  veloz::market::TradeData trade_data;

  KJ_IF_SOME(exception, kj::runCatchingExceptions([&]() {
               // Parse timestamp (convert milliseconds to nanoseconds)
               std::int64_t timestamp_ms = tokens[0].parseAs<long long>();
               event.ts_exchange_ns = timestamp_ms * 1'000'000;
               event.ts_recv_ns = event.ts_exchange_ns;
               event.ts_pub_ns = event.ts_exchange_ns;

               // Set symbol
               if (options_.symbol_override.size() > 0) {
                 event.symbol = veloz::common::SymbolId(options_.symbol_override);
               } else {
                 event.symbol = veloz::common::SymbolId(tokens[1]);
               }

               // Set venue and market
               event.venue = options_.venue;
               event.market = options_.market;

               // Parse side
               kj::String side_lower = to_lower_ascii(tokens[2]);
               if (side_lower == "buy"_kj) {
                 trade_data.is_buyer_maker = false;
               } else if (side_lower == "sell"_kj) {
                 trade_data.is_buyer_maker = true;
               } else {
                 KJ_FAIL_REQUIRE("Invalid side", side_lower);
               }

               // Parse price
               trade_data.price = tokens[3].parseAs<double>();
               if (trade_data.price <= 0) {
                 KJ_FAIL_REQUIRE("Invalid price", tokens[3]);
               }

               // Parse quantity
               trade_data.qty = tokens[4].parseAs<double>();
               if (trade_data.qty <= 0) {
                 KJ_FAIL_REQUIRE("Invalid quantity", tokens[4]);
               }

               // Set trade_id
               trade_data.trade_id = line_number;

               // Set event type
               event.type = veloz::market::MarketEventType::Trade;
               event.data = trade_data;
               event.payload = kj::str("");
             })) {
    (void)exception;
    return kj::none;
  }

  return event;
}

kj::Maybe<veloz::market::MarketEvent>
CSVDataSource::parse_ohlcv_row(kj::ArrayPtr<const kj::String> tokens, std::int64_t line_number) {
  // Expected format: timestamp,open,high,low,close,volume
  // Or: timestamp,symbol,open,high,low,close,volume
  if (tokens.size() < 6) {
    return kj::none;
  }

  veloz::market::MarketEvent event;
  veloz::market::KlineData kline_data;

  KJ_IF_SOME(exception, kj::runCatchingExceptions([&]() {
               size_t offset = 0;

               // Parse timestamp
               std::int64_t timestamp_ms = tokens[0].parseAs<long long>();
               event.ts_exchange_ns = timestamp_ms * 1'000'000;
               event.ts_recv_ns = event.ts_exchange_ns;
               event.ts_pub_ns = event.ts_exchange_ns;

               // Check if second column is symbol or open price
               bool has_symbol_column = false;
               KJ_IF_SOME(ex, kj::runCatchingExceptions([&]() {
                            double test = tokens[1].parseAs<double>();
                            (void)test;
                          })) {
                 (void)ex;
                 has_symbol_column = true;
               }

               if (has_symbol_column) {
                 if (options_.symbol_override.size() > 0) {
                   event.symbol = veloz::common::SymbolId(kj::str(options_.symbol_override));
                 } else {
                   event.symbol = veloz::common::SymbolId(tokens[1]);
                 }
                 offset = 1;
               } else {
                 event.symbol = veloz::common::SymbolId(options_.symbol_override.size() > 0
                                                            ? kj::str(options_.symbol_override)
                                                            : kj::str("UNKNOWN"));
               }

               // Validate we have enough columns after offset
               if (tokens.size() < 6 + offset) {
                 KJ_FAIL_REQUIRE("Insufficient columns for OHLCV");
               }

               // Set venue and market
               event.venue = options_.venue;
               event.market = options_.market;

               // Parse OHLCV data
               kline_data.open = tokens[1 + offset].parseAs<double>();
               kline_data.high = tokens[2 + offset].parseAs<double>();
               kline_data.low = tokens[3 + offset].parseAs<double>();
               kline_data.close = tokens[4 + offset].parseAs<double>();
               kline_data.volume = tokens[5 + offset].parseAs<double>();

               // Validate OHLCV data
               if (kline_data.high < kline_data.low) {
                 KJ_FAIL_REQUIRE("High price less than low price");
               }
               if (kline_data.high < kline_data.open || kline_data.high < kline_data.close) {
                 KJ_FAIL_REQUIRE("High price less than open or close");
               }
               if (kline_data.low > kline_data.open || kline_data.low > kline_data.close) {
                 KJ_FAIL_REQUIRE("Low price greater than open or close");
               }
               if (kline_data.volume < 0) {
                 KJ_FAIL_REQUIRE("Negative volume");
               }

               kline_data.start_time = timestamp_ms;
               kline_data.close_time = timestamp_ms; // Will be set properly if available

               // Set event type
               event.type = veloz::market::MarketEventType::Kline;
               event.data = kline_data;
               event.payload = kj::str("");
             })) {
    (void)exception;
    return kj::none;
  }

  return event;
}

kj::Maybe<veloz::market::MarketEvent>
CSVDataSource::parse_book_row(kj::ArrayPtr<const kj::String> tokens, std::int64_t line_number) {
  // Expected format: timestamp,bid_price,bid_qty,ask_price,ask_qty
  // Or: timestamp,symbol,bid_price,bid_qty,ask_price,ask_qty
  if (tokens.size() < 5) {
    return kj::none;
  }

  veloz::market::MarketEvent event;
  veloz::market::BookData book_data;

  KJ_IF_SOME(exception, kj::runCatchingExceptions([&]() {
               size_t offset = 0;

               // Parse timestamp
               std::int64_t timestamp_ms = tokens[0].parseAs<long long>();
               event.ts_exchange_ns = timestamp_ms * 1'000'000;
               event.ts_recv_ns = event.ts_exchange_ns;
               event.ts_pub_ns = event.ts_exchange_ns;

               // Check if second column is symbol or price
               bool has_symbol_column = false;
               KJ_IF_SOME(ex, kj::runCatchingExceptions([&]() {
                            double test = tokens[1].parseAs<double>();
                            (void)test;
                          })) {
                 (void)ex;
                 has_symbol_column = true;
               }

               if (has_symbol_column) {
                 if (options_.symbol_override.size() > 0) {
                   event.symbol = veloz::common::SymbolId(kj::str(options_.symbol_override));
                 } else {
                   event.symbol = veloz::common::SymbolId(tokens[1]);
                 }
                 offset = 1;
               } else {
                 event.symbol = veloz::common::SymbolId(options_.symbol_override.size() > 0
                                                            ? kj::str(options_.symbol_override)
                                                            : kj::str("UNKNOWN"));
               }

               // Validate we have enough columns after offset
               if (tokens.size() < 5 + offset) {
                 KJ_FAIL_REQUIRE("Insufficient columns for Book data");
               }

               // Set venue and market
               event.venue = options_.venue;
               event.market = options_.market;

               // Parse book data
               veloz::market::BookLevel bid_level;
               bid_level.price = tokens[1 + offset].parseAs<double>();
               bid_level.qty = tokens[2 + offset].parseAs<double>();

               veloz::market::BookLevel ask_level;
               ask_level.price = tokens[3 + offset].parseAs<double>();
               ask_level.qty = tokens[4 + offset].parseAs<double>();

               // Validate book data
               if (bid_level.price <= 0 || ask_level.price <= 0) {
                 KJ_FAIL_REQUIRE("Invalid price in book data");
               }
               if (bid_level.qty < 0 || ask_level.qty < 0) {
                 KJ_FAIL_REQUIRE("Negative quantity in book data");
               }
               if (bid_level.price >= ask_level.price) {
                 KJ_FAIL_REQUIRE("Bid price >= ask price (crossed book)");
               }

               book_data.bids.add(bid_level);
               book_data.asks.add(ask_level);
               book_data.is_snapshot = true;
               book_data.sequence = line_number;

               // Set event type
               event.type = veloz::market::MarketEventType::BookTop;
               event.data = kj::mv(book_data);
               event.payload = kj::str("");
             })) {
    (void)exception;
    return kj::none;
  }

  return event;
}

kj::Vector<veloz::market::MarketEvent>
CSVDataSource::load_file(kj::StringPtr file_path, std::int64_t start_time, std::int64_t end_time) {
  kj::Vector<veloz::market::MarketEvent> events;
  veloz::core::Logger logger;

  auto start_clock = std::chrono::steady_clock::now();

  // Reset stats
  stats_.total_rows = 0;
  stats_.valid_rows = 0;
  stats_.invalid_rows = 0;
  stats_.skipped_rows = 0;
  stats_.first_error = kj::str("");
  stats_.warnings.clear();

  auto fs = kj::newDiskFilesystem();
  auto path = kj::Path::parse("."_kj).eval(file_path);
  bool isAbsolute = file_path.size() > 0 && file_path[0] == '/';
  auto& root = isAbsolute ? fs->getRoot() : fs->getCurrent();

  if (!root.exists(path)) {
    stats_.first_error = kj::str("File not found: ", file_path);
    logger.error(stats_.first_error);
    return events;
  }

  auto file = root.openFile(path);
  auto file_content = file->readAllText();
  kj::StringPtr content_ptr = file_content;

  // Determine format
  CSVFormat format = options_.format;
  bool first_line = true;

  // Process lines
  size_t line_start = 0;
  std::int64_t line_number = 0;

  for (size_t i = 0; i <= content_ptr.size(); ++i) {
    if (i < content_ptr.size() && content_ptr[i] != '\n') {
      continue;
    }

    auto line_slice = content_ptr.slice(line_start, i);
    line_start = i + 1;
    line_number++;
    stats_.total_rows++;

    // Skip empty lines
    if (line_slice.size() == 0) {
      stats_.skipped_rows++;
      continue;
    }

    // Remove trailing \r
    if (line_slice[line_slice.size() - 1] == '\r') {
      line_slice = line_slice.first(line_slice.size() - 1);
      if (line_slice.size() == 0) {
        stats_.skipped_rows++;
        continue;
      }
    }

    kj::String line = kj::heapString(line_slice);

    // Handle header line
    if (first_line && options_.has_header) {
      first_line = false;
      if (format == CSVFormat::Auto) {
        format = detect_format_from_header(line);
        logger.info(kj::str("Detected CSV format: ", format == CSVFormat::Trade   ? "Trade"_kj
                                                     : format == CSVFormat::OHLCV ? "OHLCV"_kj
                                                     : format == CSVFormat::Book  ? "Book"_kj
                                                                                  : "Unknown"_kj));
      }
      stats_.skipped_rows++;
      continue;
    }
    first_line = false;

    // Check max rows limit
    if (options_.max_rows > 0 && stats_.valid_rows >= options_.max_rows) {
      break;
    }

    // Tokenize line
    auto tokens = tokenize_line(line);

    // Parse based on format
    kj::Maybe<veloz::market::MarketEvent> maybe_event;
    switch (format) {
    case CSVFormat::Trade:
    case CSVFormat::Auto:
      maybe_event = parse_trade_row(tokens.asPtr(), line_number);
      break;
    case CSVFormat::OHLCV:
      maybe_event = parse_ohlcv_row(tokens.asPtr(), line_number);
      break;
    case CSVFormat::Book:
      maybe_event = parse_book_row(tokens.asPtr(), line_number);
      break;
    }

    KJ_IF_SOME(event, maybe_event) {
      // Apply time filters
      std::int64_t event_time_ms = event.ts_exchange_ns / 1'000'000;
      if (start_time > 0 && event_time_ms < start_time) {
        stats_.skipped_rows++;
        continue;
      }
      if (end_time > 0 && event_time_ms > end_time) {
        stats_.skipped_rows++;
        continue;
      }

      events.add(kj::mv(event));
      stats_.valid_rows++;
    }
    else {
      stats_.invalid_rows++;
      if (stats_.first_error.size() == 0) {
        stats_.first_error = kj::str("Parse error at line ", line_number);
      }
      if (!options_.skip_invalid_rows) {
        break;
      }
    }
  }

  auto end_clock = std::chrono::steady_clock::now();
  stats_.parse_time_ms =
      std::chrono::duration_cast<std::chrono::milliseconds>(end_clock - start_clock).count();

  logger.info(kj::str("Loaded ", events.size(), " events from ", file_path, " in ",
                      stats_.parse_time_ms, "ms (", stats_.invalid_rows, " invalid, ",
                      stats_.skipped_rows, " skipped)"));

  return events;
}

kj::Vector<veloz::market::MarketEvent>
CSVDataSource::load_files(kj::ArrayPtr<const kj::String> file_paths, std::int64_t start_time,
                          std::int64_t end_time) {
  kj::Vector<veloz::market::MarketEvent> all_events;
  veloz::core::Logger logger;

  // Aggregate stats
  CSVParseStats aggregate_stats;

  for (const auto& file_path : file_paths) {
    auto events = load_file(file_path, start_time, end_time);
    aggregate_stats.total_rows += stats_.total_rows;
    aggregate_stats.valid_rows += stats_.valid_rows;
    aggregate_stats.invalid_rows += stats_.invalid_rows;
    aggregate_stats.skipped_rows += stats_.skipped_rows;
    aggregate_stats.parse_time_ms += stats_.parse_time_ms;

    if (aggregate_stats.first_error.size() == 0 && stats_.first_error.size() > 0) {
      aggregate_stats.first_error = kj::str(file_path, ": ", stats_.first_error);
    }

    for (auto& event : events) {
      all_events.add(kj::mv(event));
    }
  }

  // Sort all events by timestamp
  // Note: kj::Vector doesn't have std::sort compatibility directly
  // We'll use a simple bubble sort for now (can be optimized later)
  for (size_t i = 0; i < all_events.size(); ++i) {
    for (size_t j = i + 1; j < all_events.size(); ++j) {
      if (all_events[j].ts_exchange_ns < all_events[i].ts_exchange_ns) {
        // Swap
        veloz::market::MarketEvent temp = kj::mv(all_events[i]);
        all_events[i] = kj::mv(all_events[j]);
        all_events[j] = kj::mv(temp);
      }
    }
  }

  stats_ = kj::mv(aggregate_stats);

  logger.info(kj::str("Loaded ", all_events.size(), " events from ", file_paths.size(), " files"));

  return all_events;
}

std::int64_t CSVDataSource::stream_file(kj::StringPtr file_path, CSVStreamCallback callback,
                                        std::int64_t start_time, std::int64_t end_time) {
  veloz::core::Logger logger;
  std::int64_t events_processed = 0;

  auto start_clock = std::chrono::steady_clock::now();

  // Reset stats
  stats_.total_rows = 0;
  stats_.valid_rows = 0;
  stats_.invalid_rows = 0;
  stats_.skipped_rows = 0;
  stats_.first_error = kj::str("");
  stats_.warnings.clear();

  auto fs = kj::newDiskFilesystem();
  auto path = kj::Path::parse("."_kj).eval(file_path);
  bool isAbsolute = file_path.size() > 0 && file_path[0] == '/';
  auto& root = isAbsolute ? fs->getRoot() : fs->getCurrent();

  if (!root.exists(path)) {
    stats_.first_error = kj::str("File not found: ", file_path);
    logger.error(stats_.first_error);
    return 0;
  }

  auto file = root.openFile(path);
  auto file_content = file->readAllText();
  kj::StringPtr content_ptr = file_content;

  // Determine format
  CSVFormat format = options_.format;
  bool first_line = true;

  // Process lines
  size_t line_start = 0;
  std::int64_t line_number = 0;
  bool should_continue = true;

  for (size_t i = 0; i <= content_ptr.size() && should_continue; ++i) {
    if (i < content_ptr.size() && content_ptr[i] != '\n') {
      continue;
    }

    auto line_slice = content_ptr.slice(line_start, i);
    line_start = i + 1;
    line_number++;
    stats_.total_rows++;

    // Skip empty lines
    if (line_slice.size() == 0) {
      stats_.skipped_rows++;
      continue;
    }

    // Remove trailing \r
    if (line_slice[line_slice.size() - 1] == '\r') {
      line_slice = line_slice.first(line_slice.size() - 1);
      if (line_slice.size() == 0) {
        stats_.skipped_rows++;
        continue;
      }
    }

    kj::String line = kj::heapString(line_slice);

    // Handle header line
    if (first_line && options_.has_header) {
      first_line = false;
      if (format == CSVFormat::Auto) {
        format = detect_format_from_header(line);
      }
      stats_.skipped_rows++;
      continue;
    }
    first_line = false;

    // Tokenize line
    auto tokens = tokenize_line(line);

    // Parse based on format
    kj::Maybe<veloz::market::MarketEvent> maybe_event;
    switch (format) {
    case CSVFormat::Trade:
    case CSVFormat::Auto:
      maybe_event = parse_trade_row(tokens.asPtr(), line_number);
      break;
    case CSVFormat::OHLCV:
      maybe_event = parse_ohlcv_row(tokens.asPtr(), line_number);
      break;
    case CSVFormat::Book:
      maybe_event = parse_book_row(tokens.asPtr(), line_number);
      break;
    }

    KJ_IF_SOME(event, maybe_event) {
      // Apply time filters
      std::int64_t event_time_ms = event.ts_exchange_ns / 1'000'000;
      if (start_time > 0 && event_time_ms < start_time) {
        stats_.skipped_rows++;
        continue;
      }
      if (end_time > 0 && event_time_ms > end_time) {
        stats_.skipped_rows++;
        continue;
      }

      stats_.valid_rows++;
      events_processed++;

      // Call callback - if it returns false, stop processing
      should_continue = callback(event);
    }
    else {
      stats_.invalid_rows++;
      if (stats_.first_error.size() == 0) {
        stats_.first_error = kj::str("Parse error at line ", line_number);
      }
      if (!options_.skip_invalid_rows) {
        break;
      }
    }
  }

  auto end_clock = std::chrono::steady_clock::now();
  stats_.parse_time_ms =
      std::chrono::duration_cast<std::chrono::milliseconds>(end_clock - start_clock).count();

  logger.info(kj::str("Streamed ", events_processed, " events from ", file_path));

  return events_processed;
}

kj::Vector<kj::String>
CSVDataSource::validate_ohlcv(kj::ArrayPtr<const veloz::market::MarketEvent> events) {
  kj::Vector<kj::String> errors;

  for (size_t i = 0; i < events.size(); ++i) {
    const auto& event = events[i];

    if (event.type != veloz::market::MarketEventType::Kline) {
      continue;
    }

    if (!event.data.is<veloz::market::KlineData>()) {
      errors.add(kj::str("Event ", i, ": Not a valid kline event"));
      continue;
    }

    const auto& kline = event.data.get<veloz::market::KlineData>();

    // Validate OHLCV constraints
    if (kline.high < kline.low) {
      errors.add(kj::str("Event ", i, ": High (", kline.high, ") < Low (", kline.low, ")"));
    }

    if (kline.high < kline.open) {
      errors.add(kj::str("Event ", i, ": High (", kline.high, ") < Open (", kline.open, ")"));
    }

    if (kline.high < kline.close) {
      errors.add(kj::str("Event ", i, ": High (", kline.high, ") < Close (", kline.close, ")"));
    }

    if (kline.low > kline.open) {
      errors.add(kj::str("Event ", i, ": Low (", kline.low, ") > Open (", kline.open, ")"));
    }

    if (kline.low > kline.close) {
      errors.add(kj::str("Event ", i, ": Low (", kline.low, ") > Close (", kline.close, ")"));
    }

    if (kline.volume < 0) {
      errors.add(kj::str("Event ", i, ": Negative volume (", kline.volume, ")"));
    }

    if (kline.open <= 0 || kline.high <= 0 || kline.low <= 0 || kline.close <= 0) {
      errors.add(kj::str("Event ", i, ": Non-positive price values"));
    }

    // Check timestamp ordering
    if (i > 0 && events[i - 1].type == veloz::market::MarketEventType::Kline) {
      if (event.ts_exchange_ns < events[i - 1].ts_exchange_ns) {
        errors.add(kj::str("Event ", i, ": Timestamp out of order"));
      }
    }
  }

  return errors;
}

// BinanceDataSource implementation
BinanceDataSource::BinanceDataSource()
    : api_key_(kj::str("")), api_secret_(kj::str("")),
      base_rest_url_(kj::str("https://api.binance.com")), max_retries_(3), retry_delay_ms_(1000),
      rate_limit_per_minute_(1200), rate_limit_per_second_(10) {
#ifndef VELOZ_NO_CURL
  curl_global_init(CURL_GLOBAL_DEFAULT);
#endif
}

BinanceDataSource::~BinanceDataSource() noexcept {
#ifndef VELOZ_NO_CURL
  curl_global_cleanup();
#endif
}

kj::Vector<veloz::market::MarketEvent>
BinanceDataSource::get_data(kj::StringPtr symbol, std::int64_t start_time, std::int64_t end_time,
                            kj::StringPtr data_type, kj::StringPtr time_frame) {
  kj::Vector<veloz::market::MarketEvent> events;
  veloz::core::Logger logger;

#ifdef VELOZ_NO_CURL
  logger.error("Binance API data reading requires libcurl (VELOZ_NO_CURL defined)");
  return events;
#else
  // Validate parameters
  if (symbol.size() == 0) {
    logger.error("Binance API: symbol cannot be empty");
    return events;
  }

  // Default time frame to 1h if not specified
  kj::String effective_time_frame = time_frame.size() == 0 ? kj::str("1h") : kj::str(time_frame);

  // Validate time frame for kline data
  if (data_type == "kline"_kj && !is_valid_time_frame(effective_time_frame)) {
    logger.error(kj::str("Binance API: invalid time frame '", effective_time_frame,
                         "'. Valid frames: 1s, 1m, 3m, 5m, 15m, 30m, 1h, 2h, 4h, 6h, 8h, 12h, 1d, "
                         "3d, 1w, 1M"));
    return events;
  }

  // Default to kline if data_type not specified
  kj::String effective_data_type = data_type.size() == 0 ? kj::str("kline") : kj::str(data_type);

  logger.info(kj::str("Binance API: Fetching ", effective_data_type, " data for ", symbol, " from ",
                      start_time, " to ", end_time, " (time frame: ", effective_time_frame, ")"));

  kj::String formatted_symbol = format_symbol(symbol);
  std::int64_t current_start_time = start_time;
  const std::int64_t kline_limit = 1000; // Binance API limit per request

  // For kline data, implement pagination
  if (effective_data_type == "kline"_kj) {
    int request_count = 0;
    std::int64_t total_klines = 0;

    while (current_start_time < end_time || (end_time == 0 && request_count == 0)) {
      // Build API URL using kj::str
      kj::String url =
          kj::str(base_rest_url_, "/api/v3/klines?symbol=", formatted_symbol,
                  "&interval=", effective_time_frame, "&limit=", kline_limit,
                  current_start_time > 0 ? kj::str("&startTime=", current_start_time) : kj::str(""),
                  end_time > 0 ? kj::str("&endTime=", end_time) : kj::str(""));

      // Rate limiting: wait if approaching rate limit
      rate_limit_wait();

      // Fetch data with retry
      kj::String response;
      for (int retry = 0; retry < max_retries_; ++retry) {
        response = http_get(url);

        if (response.size() > 0) {
          break;
        }

        if (retry < max_retries_ - 1) {
          logger.warn(kj::str("Binance API: Request failed, retrying (", retry + 1, "/",
                              max_retries_, ")..."));
          std::this_thread::sleep_for(std::chrono::milliseconds(retry_delay_ms_ * (retry + 1)));
        }
      }

      if (response.size() == 0) {
        logger.error("Binance API: Failed to fetch data after all retries");
        break;
      }

      // Parse JSON response using KJ exception handling
      bool should_break = false;
      bool should_continue = false;
      KJ_IF_SOME(outer_exception, kj::runCatchingExceptions([&]() {
                   auto doc = JsonDocument::parse(response);
                   auto root = doc.root();

                   // Check for API error response
                   auto code = root["code"];
                   auto msg = root["msg"];
                   if (code.is_int() && msg.is_string()) {
                     int code_val = code.get_int();
                     kj::String msg_val = msg.get_string();

                     // Handle rate limit errors (429)
                     if (code_val == -1003 || code_val == -1021) {
                       logger.warn(kj::str("Binance API: Rate limit exceeded (code ", code_val,
                                           "), waiting and retrying..."));
                       std::this_thread::sleep_for(std::chrono::seconds(1));
                       should_continue = true;
                       return;
                     }

                     logger.error(
                         kj::str("Binance API error (code ", code_val, "): ", msg_val.cStr()));
                     should_break = true;
                     return;
                   }

                   // Parse klines array
                   if (!root.is_array()) {
                     logger.error("Binance API: Unexpected response format (expected array)");
                     should_break = true;
                     return;
                   }

                   int klines_in_batch = 0;
                   for (size_t i = 0; i < root.size(); ++i) {
                     const auto kline = root[i];
                     if (!kline.is_array() || kline.size() < 11) {
                       continue;
                     }

                     // Use KJ exception handling for parsing individual klines
                     KJ_IF_SOME(inner_exception, kj::runCatchingExceptions([&]() {
                                  veloz::market::MarketEvent event;
                                  veloz::market::KlineData kline_data;

                                  // Binance kline format:
                                  // [0] Open time
                                  // [1] Open price
                                  // [2] High price
                                  // [3] Low price
                                  // [4] Close price
                                  // [5] Volume
                                  // [6] Close time
                                  // [7] Quote asset volume
                                  // [8] Number of trades
                                  // [9] Taker buy base asset volume
                                  // [10] Taker buy quote asset volume

                                  kline_data.start_time = kline[0].get_int(0LL);
                                  kline_data.open = kline[1].get_string().parseAs<double>();
                                  kline_data.high = kline[2].get_string().parseAs<double>();
                                  kline_data.low = kline[3].get_string().parseAs<double>();
                                  kline_data.close = kline[4].get_string().parseAs<double>();
                                  kline_data.volume = kline[5].get_string().parseAs<double>();
                                  kline_data.close_time = kline[6].get_int(0LL);

                                  // Set event properties
                                  event.type = veloz::market::MarketEventType::Kline;
                                  event.venue = veloz::common::Venue::Binance;
                                  event.market = veloz::common::MarketKind::Spot;
                                  event.symbol = veloz::common::SymbolId(formatted_symbol);
                                  event.ts_exchange_ns = kline_data.start_time * 1'000'000;
                                  event.ts_recv_ns =
                                      std::chrono::duration_cast<std::chrono::nanoseconds>(
                                          std::chrono::system_clock::now().time_since_epoch())
                                          .count();
                                  event.ts_pub_ns = event.ts_recv_ns;
                                  event.data = kline_data;
                                  event.payload = kj::str("");

                                  // Apply time filters
                                  if (start_time > 0 && kline_data.start_time < start_time) {
                                    return;
                                  }
                                  if (end_time > 0 && kline_data.start_time > end_time) {
                                    should_break = true;
                                    return;
                                  }

                                  events.add(kj::mv(event));
                                  klines_in_batch++;

                                  // Update start time for next pagination
                                  current_start_time = kline_data.close_time + 1;
                                })) {
                       // Inner exception - log and continue to next kline
                       logger.warn(kj::str("Binance API: Failed to parse kline data: ",
                                           inner_exception.getDescription()));
                     }

                     if (should_break) {
                       break;
                     }
                   }

                   total_klines += klines_in_batch;
                   request_count++;

                   // Check if we got fewer klines than limit - we've reached end
                   if (klines_in_batch < kline_limit) {
                     logger.info(kj::str("Binance API: Fetched ", total_klines, " klines in ",
                                         request_count, " requests (batch ended with ",
                                         klines_in_batch, " klines)"));
                     should_break = true;
                     return;
                   }

                   // Small delay between paginated requests to respect rate limits
                   std::this_thread::sleep_for(std::chrono::milliseconds(100));
                 })) {
        // Outer exception - log and break
        logger.error(kj::str("Binance API: Unexpected error: ", outer_exception.getDescription()));
        should_break = true;
      }

      if (should_continue) {
        continue;
      }
      if (should_break) {
        break;
      }
    }

    logger.info(kj::str("Binance API: Successfully fetched ", events.size(), " klines for ",
                        formatted_symbol, " (", request_count, " requests)"));

    // Sort events by timestamp
    // Note: kj::Vector doesn't have std::sort compatibility, so we use a simple approach
    // For production, consider using a sortable container or implementing a sort

    return events;
  }

  // For trade data
  if (effective_data_type == "trade"_kj) {
    // Build API URL using kj::str
    kj::String url =
        kj::str(base_rest_url_, "/api/v3/trades?symbol=", formatted_symbol, "&limit=1000");

    if (start_time > 0) {
      // Note: Binance trades endpoint doesn't support startTime directly
      // We'll fetch recent trades and filter by time
      logger.warn("Binance API: /api/v3/trades endpoint doesn't support time filtering, fetching "
                  "latest 1000 trades");
    }

    // Rate limiting
    rate_limit_wait();

    // Fetch data with retry
    kj::String response;
    for (int retry = 0; retry < max_retries_; ++retry) {
      response = http_get(url);

      if (response.size() > 0) {
        break;
      }

      if (retry < max_retries_ - 1) {
        logger.warn(kj::str("Binance API: Request failed, retrying (", retry + 1, "/", max_retries_,
                            ")..."));
        std::this_thread::sleep_for(std::chrono::milliseconds(retry_delay_ms_ * (retry + 1)));
      }
    }

    if (response.size() == 0) {
      logger.error("Binance API: Failed to fetch trades data");
      return events;
    }

    // Parse JSON response using KJ exception handling
    KJ_IF_SOME(
        outer_exception, kj::runCatchingExceptions([&]() {
          auto doc = JsonDocument::parse(response);
          auto root = doc.root();

          // Check for API error response
          auto code = root["code"];
          auto msg = root["msg"];
          if (code.is_int() && msg.is_string()) {
            int code_val = code.get_int();
            kj::String msg_val = msg.get_string();
            logger.error(kj::str("Binance API error (code ", code_val, "): ", msg_val.cStr()));
            return;
          }

          if (!root.is_array()) {
            logger.error("Binance API: Unexpected trades response format (expected array)");
            return;
          }

          bool should_break = false;
          for (size_t i = 0; i < root.size(); ++i) {
            const auto trade = root[i];

            // Use KJ exception handling for parsing individual trades
            KJ_IF_SOME(inner_exception, kj::runCatchingExceptions([&]() {
                         veloz::market::MarketEvent event;
                         veloz::market::TradeData trade_data;

                         // Binance trade format:
                         // id: trade ID
                         // price: price
                         // qty: quantity
                         // time: timestamp
                         // isBuyerMaker: true if buyer is maker
                         // isBestMatch: true if best match

                         auto priceStr = trade["price"].get_string();
                         auto qtyStr = trade["qty"].get_string();
                         trade_data.price = priceStr.parseAs<double>();
                         trade_data.qty = qtyStr.parseAs<double>();
                         trade_data.trade_id = trade["id"].get_int(0LL);
                         trade_data.is_buyer_maker = trade["isBuyerMaker"].get_bool(false);

                         std::int64_t trade_time = trade["time"].get_int(0LL);

                         // Set event properties
                         event.type = veloz::market::MarketEventType::Trade;
                         event.venue = veloz::common::Venue::Binance;
                         event.market = veloz::common::MarketKind::Spot;
                         event.symbol = veloz::common::SymbolId(formatted_symbol);
                         event.ts_exchange_ns = trade_time * 1'000'000;
                         event.ts_recv_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
                                                std::chrono::system_clock::now().time_since_epoch())
                                                .count();
                         event.ts_pub_ns = event.ts_recv_ns;
                         event.data = trade_data;
                         event.payload = kj::str("");

                         // Apply time filters
                         if (start_time > 0 && trade_time < start_time) {
                           return;
                         }
                         if (end_time > 0 && trade_time > end_time) {
                           should_break = true;
                           return;
                         }

                         events.add(kj::mv(event));
                       })) {
              // Inner exception - log and continue to next trade
              logger.warn(kj::str("Binance API: Failed to parse trade data: ",
                                  inner_exception.getDescription()));
            }

            if (should_break) {
              break;
            }
          }

          logger.info(kj::str("Binance API: Successfully fetched ", events.size(), " trades for ",
                              formatted_symbol));
        })) {
      // Outer exception - log error
      logger.error(kj::str("Binance API: Unexpected error: ", outer_exception.getDescription()));
    }

    return events;
  }

  logger.error(kj::str("Binance API: Unsupported data type '", effective_data_type,
                       "'. Supported types: kline, trade"));
  return events;
#endif
}

void BinanceDataSource::rate_limit_wait() {
  struct RateLimitState {
    kj::Vector<std::chrono::steady_clock::time_point> request_times;
    std::chrono::steady_clock::time_point second_window_start = std::chrono::steady_clock::now();
    int requests_in_second = 0;
  };

  static kj::MutexGuarded<RateLimitState> state;
  auto lock = state.lockExclusive();
  auto now = std::chrono::steady_clock::now();

  // Clean up old request times (older than 1 minute)
  auto one_minute_ago = now - std::chrono::minutes(1);
  auto& request_times = lock->request_times;
  auto new_end = std::remove_if(request_times.begin(), request_times.end(),
                                [one_minute_ago](const auto& t) { return t < one_minute_ago; });
  request_times.truncate(new_end - request_times.begin());

  // Check per-minute rate limit
  if (request_times.size() >= static_cast<size_t>(rate_limit_per_minute_)) {
    auto oldest_time = request_times.front();
    auto wait_time = std::chrono::duration_cast<std::chrono::milliseconds>(
        oldest_time + std::chrono::minutes(1) - now);
    if (wait_time.count() > 0) {
      std::this_thread::sleep_for(wait_time);
    }
  }

  // Check per-second rate limit
  if (now - lock->second_window_start >= std::chrono::seconds(1)) {
    lock->second_window_start = now;
    lock->requests_in_second = 0;
  }
  if (lock->requests_in_second >= rate_limit_per_second_) {
    auto wait_time = std::chrono::duration_cast<std::chrono::milliseconds>(
        lock->second_window_start + std::chrono::seconds(1) - now);
    if (wait_time.count() > 0) {
      std::this_thread::sleep_for(wait_time);
    }
    lock->second_window_start = now;
    lock->requests_in_second = 0;
  }

  request_times.add(now);
  lock->requests_in_second++;
}

bool BinanceDataSource::download_data(kj::StringPtr symbol, std::int64_t start_time,
                                      std::int64_t end_time, kj::StringPtr data_type,
                                      kj::StringPtr time_frame, kj::StringPtr output_path) {
  veloz::core::Logger logger;

  // Validate parameters
  if (symbol.size() == 0) {
    logger.error("download_data: Symbol cannot be empty");
    return false;
  }

  if (start_time <= 0) {
    logger.error("download_data: Invalid start_time (must be > 0)");
    return false;
  }

  if (end_time <= 0) {
    logger.error("download_data: Invalid end_time (must be > 0)");
    return false;
  }

  if (end_time <= start_time) {
    logger.error(kj::str("download_data: end_time (", end_time,
                         ") must be greater than start_time (", start_time, ")"));
    return false;
  }

  if (output_path.size() == 0) {
    logger.error("download_data: Output path cannot be empty");
    return false;
  }

#ifdef VELOZ_NO_CURL
  logger.error("download_data: libcurl not available - cannot download data from Binance API");
  return false;
#else
  // For now, we only support "kline" data type (candlestick data)
  // Trade data and orderbook data can be added later
  if (data_type != "kline"_kj) {
    logger.error(kj::str("download_data: Unsupported data type '", data_type,
                         "'. Only 'kline' is currently supported."));
    return false;
  }

  if (time_frame.size() == 0) {
    logger.error("download_data: Time frame cannot be empty for kline data");
    return false;
  }

  if (!is_valid_time_frame(time_frame)) {
    logger.error(kj::str("download_data: Invalid time frame '", time_frame,
                         "'. Valid values: 1s, 1m, 3m, 5m, 15m, 30m, 1h, 2h, 4h, 6h, 8h, 12h, 1d, "
                         "3d, 1w, 1M"));
    return false;
  }

  // Create output directory if it doesn't exist using kj::Path
  kj::Path output_file_path = kj::Path::parse("."_kj).eval(output_path);

  bool outputPathAbsolute = output_path.size() > 0 && output_path[0] == '/';
  try {
    // kj::Path::parent() returns parent directory
    auto parent = output_file_path.parent();
    if (parent.size() > 0) {
      auto fs = kj::newDiskFilesystem();
      auto& root = outputPathAbsolute ? fs->getRoot() : fs->getCurrent();

      if (!root.exists(parent)) {
        // Use KJ filesystem to create directory
        root.openSubdir(parent, kj::WriteMode::CREATE | kj::WriteMode::CREATE_PARENT);
        logger.info(kj::str("Created output directory: ", parent.toString(outputPathAbsolute)));
      }
    }
  } catch (const kj::Exception& e) {
    logger.error(
        kj::str("download_data: Filesystem error creating directory: ", e.getDescription()));
    return false;
  }

  // Open output file - convert kj::Path string representation to const char*
  auto output_path_str = output_file_path.toString(outputPathAbsolute);
  std::ofstream output_file(output_path_str.cStr());
  if (!output_file.is_open()) {
    logger.error(
        kj::str("download_data: Failed to open output file for writing: ", output_path_str.cStr()));
    return false;
  }

  // Write CSV header - format: timestamp,symbol,side,price,quantity
  // For kline data, we generate two synthetic trades per candle (open and close)
  output_file << "timestamp,symbol,side,price,quantity\n";

  // Format symbol for Binance API (uppercase)
  kj::String formatted_symbol = format_symbol(symbol);

  logger.info(kj::str("Downloading kline data for ", formatted_symbol, " from ", start_time, " to ",
                      end_time, " (time frame: ", time_frame, ")"));

  // Binance API returns at most 1000 klines per request
  // We need to paginate through the time range
  const int MAX_KLINES_PER_REQUEST = 1000;
  std::int64_t current_start_time = start_time;
  int total_klines = 0;

  // Helper lambda to parse time frame to milliseconds
  auto time_frame_to_ms = [](kj::StringPtr tf) -> std::int64_t {
    if (tf == "1s"_kj)
      return 1000LL;
    if (tf == "1m"_kj)
      return 60LL * 1000LL;
    if (tf == "3m"_kj)
      return 3LL * 60LL * 1000LL;
    if (tf == "5m"_kj)
      return 5LL * 60LL * 1000LL;
    if (tf == "15m"_kj)
      return 15LL * 60LL * 1000LL;
    if (tf == "30m"_kj)
      return 30LL * 60LL * 1000LL;
    if (tf == "1h"_kj)
      return 60LL * 60LL * 1000LL;
    if (tf == "2h"_kj)
      return 2LL * 60LL * 60LL * 1000LL;
    if (tf == "4h"_kj)
      return 4LL * 60LL * 60LL * 1000LL;
    if (tf == "6h"_kj)
      return 6LL * 60LL * 60LL * 1000LL;
    if (tf == "8h"_kj)
      return 8LL * 60LL * 60LL * 1000LL;
    if (tf == "12h"_kj)
      return 12LL * 60LL * 60LL * 1000LL;
    if (tf == "1d"_kj)
      return 24LL * 60LL * 60LL * 1000LL;
    if (tf == "3d"_kj)
      return 3LL * 24LL * 60LL * 60LL * 1000LL;
    if (tf == "1w"_kj)
      return 7LL * 24LL * 60LL * 60LL * 1000LL;
    if (tf == "1M"_kj)
      return 30LL * 24LL * 60LL * 60LL * 1000LL; // Approximate
    return 60LL * 1000LL;                        // Default to 1 minute
  };

  std::int64_t interval_ms = time_frame_to_ms(time_frame);
  std::int64_t request_duration_ms = interval_ms * MAX_KLINES_PER_REQUEST;

  while (current_start_time < end_time) {
    std::int64_t request_end_time = std::min(current_start_time + request_duration_ms, end_time);

    // Build Binance API URL using kj::str
    kj::String url = kj::str(base_rest_url_, "/api/v3/klines?symbol=", formatted_symbol,
                             "&interval=", time_frame, "&startTime=", current_start_time,
                             "&endTime=", request_end_time, "&limit=", MAX_KLINES_PER_REQUEST);

    logger.info(kj::str("Fetching klines from ", current_start_time, " to ", request_end_time));

    // Fetch data from Binance API
    kj::String response = http_get(url);

    if (response.size() == 0) {
      logger.error(kj::str("download_data: Empty response from Binance API for request: ", url));
      output_file.close();
      return false;
    }

    // Parse JSON response using KJ exception handling
    bool parse_error = false;
    bool should_break = false;
    KJ_IF_SOME(exception, kj::runCatchingExceptions([&]() {
                 auto doc = JsonDocument::parse(response);
                 auto root = doc.root();

                 // Check for error
                 auto code = root["code"];
                 if (code.is_int()) {
                   auto msg = root["msg"];
                   auto msgStr = msg.get_string();
                   logger.error(kj::str("Binance API error: ", code.get_int(), " - ", msgStr));
                   parse_error = true;
                   return;
                 }

                 if (!root.is_array()) {
                   logger.error("Binance API returned unexpected response format");
                   parse_error = true;
                   return;
                 }

                 // Process klines
                 // Each kline is an array: [open_time, open, high, low, close, volume, close_time,
                 // quote_volume, trades, taker_buy_base, taker_buy_quote, ignore]
                 for (size_t i = 0; i < root.size(); ++i) {
                   const auto kline = root[i];
                   if (!kline.is_array() || kline.size() < 12) {
                     continue;
                   }

                   std::int64_t open_time = kline[0].get_int(0LL);
                   double open = kline[1].get_string().parseAs<double>();
                   double high = kline[2].get_string().parseAs<double>();
                   double low = kline[3].get_string().parseAs<double>();
                   double close = kline[4].get_string().parseAs<double>();
                   double volume = kline[5].get_string().parseAs<double>();
                   double taker_buy_base =
                       kline[9].get_string().parseAs<double>(); // Base asset buy volume
                   [[maybe_unused]] double taker_buy_quote =
                       kline[10].get_string().parseAs<double>(); // Quote asset buy volume

                   // Generate synthetic trade data from kline
                   // We create two trades: one buy at open, one sell at close
                   // This approximates the candle's price movement

                   // Trade 1: Buy at open (taker buy at open)
                   std::int64_t trade1_time = open_time;
                   kj::StringPtr trade1_side = "buy"_kj;
                   double trade1_price = open;
                   // Estimate quantity from volume (assume even distribution across trades)
                   double trade1_qty = taker_buy_base / 2.0; // Half of taker buy volume at open

                   // Trade 2: Sell at close (seller initiated at close)
                   std::int64_t trade2_time = open_time + interval_ms;
                   kj::StringPtr trade2_side = "sell"_kj;
                   double trade2_price = close;
                   double trade2_qty =
                       (volume - taker_buy_base) / 2.0; // Half of maker sell volume at close

                   // Ensure quantities are positive
                   if (trade1_qty <= 0)
                     trade1_qty = volume / 4.0;
                   if (trade2_qty <= 0)
                     trade2_qty = volume / 4.0;

                   // Write trade 1
                   output_file << trade1_time << "," << formatted_symbol.cStr() << ","
                               << trade1_side.cStr() << "," << std::fixed << std::setprecision(8)
                               << trade1_price << "," << std::fixed << std::setprecision(8)
                               << trade1_qty << "\n";

                   // Write trade 2
                   output_file << trade2_time << "," << formatted_symbol.cStr() << ","
                               << trade2_side.cStr() << "," << std::fixed << std::setprecision(8)
                               << trade2_price << "," << std::fixed << std::setprecision(8)
                               << trade2_qty << "\n";

                   total_klines++;
                 }

                 logger.info(kj::str("Processed ", root.size(), " klines"));

                 // If we got fewer klines than requested, we've reached the end
                 if (root.size() < MAX_KLINES_PER_REQUEST) {
                   should_break = true;
                 }
               })) {
      // Exception occurred - log and return error
      logger.error(kj::str("download_data: JSON parsing error: ", exception.getDescription()));
      output_file.close();
      return false;
    }

    if (parse_error) {
      output_file.close();
      return false;
    }

    if (should_break) {
      break;
    }

    // Advance to next time window
    current_start_time = request_end_time;
  }

  output_file.close();

  // Verify file was written successfully
  if (!output_file.good()) {
    logger.error(
        kj::str("download_data: Error occurred while writing to file: ", output_path_str.cStr()));
    return false;
  }

  logger.info(
      kj::str("Successfully downloaded ", total_klines, " klines to: ", output_path_str.cStr()));

  return true;
#endif // VELOZ_NO_CURL
}

void BinanceDataSource::set_api_key(kj::StringPtr api_key) {
  api_key_ = kj::str(api_key);
}

void BinanceDataSource::set_api_secret(kj::StringPtr api_secret) {
  api_secret_ = kj::str(api_secret);
}

void BinanceDataSource::set_download_options(const BinanceDownloadOptions& options) {
  download_options_.parallel_download = options.parallel_download;
  download_options_.max_parallel_requests = options.max_parallel_requests;
  download_options_.validate_data = options.validate_data;
  download_options_.compress_output = options.compress_output;
  download_options_.append_to_existing = options.append_to_existing;
  download_options_.output_format = kj::str(options.output_format);
}

const BinanceDownloadOptions& BinanceDataSource::get_download_options() const {
  return download_options_;
}

kj::Vector<veloz::market::MarketEvent>
BinanceDataSource::fetch_klines_chunk(kj::StringPtr symbol, std::int64_t start_time,
                                      std::int64_t end_time, kj::StringPtr time_frame) {
  kj::Vector<veloz::market::MarketEvent> events;
  veloz::core::Logger logger;

#ifdef VELOZ_NO_CURL
  logger.error("fetch_klines_chunk: libcurl not available");
  return events;
#else
  kj::String formatted_symbol = format_symbol(symbol);
  const std::int64_t kline_limit = 1000;

  // Build API URL
  kj::String url =
      kj::str(base_rest_url_, "/api/v3/klines?symbol=", formatted_symbol, "&interval=", time_frame,
              "&limit=", kline_limit, "&startTime=", start_time, "&endTime=", end_time);

  // Rate limiting
  rate_limit_wait();

  // Fetch data with retry
  kj::String response;
  for (int retry = 0; retry < max_retries_; ++retry) {
    response = http_get(url);
    if (response.size() > 0) {
      break;
    }
    if (retry < max_retries_ - 1) {
      std::this_thread::sleep_for(std::chrono::milliseconds(retry_delay_ms_ * (retry + 1)));
    }
  }

  if (response.size() == 0) {
    logger.error("fetch_klines_chunk: Failed to fetch data after all retries");
    return events;
  }

  // Parse JSON response
  KJ_IF_SOME(
      exception, kj::runCatchingExceptions([&]() {
        auto doc = JsonDocument::parse(response);
        auto root = doc.root();

        // Check for API error
        auto code = root["code"];
        if (code.is_int()) {
          auto msg = root["msg"];
          logger.error(kj::str("Binance API error: ", code.get_int(), " - ", msg.get_string()));
          return;
        }

        if (!root.is_array()) {
          logger.error("fetch_klines_chunk: Unexpected response format");
          return;
        }

        for (size_t i = 0; i < root.size(); ++i) {
          const auto kline = root[i];
          if (!kline.is_array() || kline.size() < 11) {
            continue;
          }

          KJ_IF_SOME(inner_ex, kj::runCatchingExceptions([&]() {
                       veloz::market::MarketEvent event;
                       veloz::market::KlineData kline_data;

                       kline_data.start_time = kline[0].get_int(0LL);
                       kline_data.open = kline[1].get_string().parseAs<double>();
                       kline_data.high = kline[2].get_string().parseAs<double>();
                       kline_data.low = kline[3].get_string().parseAs<double>();
                       kline_data.close = kline[4].get_string().parseAs<double>();
                       kline_data.volume = kline[5].get_string().parseAs<double>();
                       kline_data.close_time = kline[6].get_int(0LL);

                       event.type = veloz::market::MarketEventType::Kline;
                       event.venue = veloz::common::Venue::Binance;
                       event.market = veloz::common::MarketKind::Spot;
                       event.symbol = veloz::common::SymbolId(formatted_symbol);
                       event.ts_exchange_ns = kline_data.start_time * 1'000'000;
                       event.ts_recv_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
                                              std::chrono::system_clock::now().time_since_epoch())
                                              .count();
                       event.ts_pub_ns = event.ts_recv_ns;
                       event.data = kline_data;
                       event.payload = kj::str("");

                       events.add(kj::mv(event));
                     })) {
            (void)inner_ex;
          }
        }
      })) {
    logger.error(kj::str("fetch_klines_chunk: Parse error: ", exception.getDescription()));
  }

  return events;
#endif
}

bool BinanceDataSource::download_data_with_progress(
    kj::StringPtr symbol, std::int64_t start_time, std::int64_t end_time, kj::StringPtr data_type,
    kj::StringPtr time_frame, kj::StringPtr output_path,
    kj::Function<void(const BinanceDownloadProgress&)> progress_callback) {
  veloz::core::Logger logger;

#ifdef VELOZ_NO_CURL
  logger.error("download_data_with_progress: libcurl not available");
  return false;
#else
  // Validate parameters
  if (symbol.size() == 0) {
    logger.error("download_data_with_progress: Symbol cannot be empty");
    return false;
  }

  if (start_time <= 0 || end_time <= 0 || end_time <= start_time) {
    logger.error("download_data_with_progress: Invalid time range");
    return false;
  }

  if (data_type != "kline"_kj) {
    logger.error(kj::str("download_data_with_progress: Unsupported data type '", data_type,
                         "'. Only 'kline' is supported."));
    return false;
  }

  if (!is_valid_time_frame(time_frame)) {
    logger.error(kj::str("download_data_with_progress: Invalid time frame '", time_frame, "'"));
    return false;
  }

  // Calculate time frame duration in milliseconds
  auto time_frame_to_ms = [](kj::StringPtr tf) -> std::int64_t {
    if (tf == "1s"_kj)
      return 1000LL;
    if (tf == "1m"_kj)
      return 60LL * 1000LL;
    if (tf == "3m"_kj)
      return 3LL * 60LL * 1000LL;
    if (tf == "5m"_kj)
      return 5LL * 60LL * 1000LL;
    if (tf == "15m"_kj)
      return 15LL * 60LL * 1000LL;
    if (tf == "30m"_kj)
      return 30LL * 60LL * 1000LL;
    if (tf == "1h"_kj)
      return 60LL * 60LL * 1000LL;
    if (tf == "2h"_kj)
      return 2LL * 60LL * 60LL * 1000LL;
    if (tf == "4h"_kj)
      return 4LL * 60LL * 60LL * 1000LL;
    if (tf == "6h"_kj)
      return 6LL * 60LL * 60LL * 1000LL;
    if (tf == "8h"_kj)
      return 8LL * 60LL * 60LL * 1000LL;
    if (tf == "12h"_kj)
      return 12LL * 60LL * 60LL * 1000LL;
    if (tf == "1d"_kj)
      return 24LL * 60LL * 60LL * 1000LL;
    if (tf == "3d"_kj)
      return 3LL * 24LL * 60LL * 60LL * 1000LL;
    if (tf == "1w"_kj)
      return 7LL * 24LL * 60LL * 60LL * 1000LL;
    if (tf == "1M"_kj)
      return 30LL * 24LL * 60LL * 60LL * 1000LL;
    return 60LL * 1000LL;
  };

  std::int64_t interval_ms = time_frame_to_ms(time_frame);
  const std::int64_t klines_per_request = 1000;
  std::int64_t chunk_duration_ms = interval_ms * klines_per_request;

  // Calculate total chunks
  std::int64_t total_duration = end_time - start_time;
  std::int64_t total_chunks =
      (total_duration + chunk_duration_ms - 1) / chunk_duration_ms; // Ceiling division

  // Create output directory if needed
  kj::Path output_file_path = kj::Path::parse("."_kj).eval(output_path);
  bool outputPathAbsolute = output_path.size() > 0 && output_path[0] == '/';

  try {
    auto parent = output_file_path.parent();
    if (parent.size() > 0) {
      auto fs = kj::newDiskFilesystem();
      auto& root = outputPathAbsolute ? fs->getRoot() : fs->getCurrent();
      if (!root.exists(parent)) {
        root.openSubdir(parent, kj::WriteMode::CREATE | kj::WriteMode::CREATE_PARENT);
      }
    }
  } catch (const kj::Exception& e) {
    logger.error(kj::str("download_data_with_progress: Filesystem error: ", e.getDescription()));
    return false;
  }

  // Open output file
  auto output_path_str = output_file_path.toString(outputPathAbsolute);
  std::ios_base::openmode mode = std::ios::out;
  if (download_options_.append_to_existing) {
    mode |= std::ios::app;
  }
  std::ofstream output_file(output_path_str.cStr(), mode);
  if (!output_file.is_open()) {
    logger.error(
        kj::str("download_data_with_progress: Failed to open output file: ", output_path_str));
    return false;
  }

  // Write header if not appending
  if (!download_options_.append_to_existing) {
    output_file << "timestamp,open,high,low,close,volume,close_time\n";
  }

  // Initialize progress
  BinanceDownloadProgress progress;
  progress.total_chunks = total_chunks;
  progress.completed_chunks = 0;
  progress.total_records = 0;
  progress.downloaded_bytes = 0;
  progress.progress_fraction = 0.0;
  progress.status = kj::str("Starting download...");

  // Report initial progress
  progress_callback(progress);

  kj::String formatted_symbol = format_symbol(symbol);
  std::int64_t current_start = start_time;
  std::int64_t records_written = 0;

  while (current_start < end_time) {
    std::int64_t chunk_end = std::min(current_start + chunk_duration_ms, end_time);

    // Update progress status
    progress.current_date = kj::str(current_start);
    progress.status =
        kj::str("Downloading chunk ", progress.completed_chunks + 1, "/", progress.total_chunks);
    progress_callback(progress);

    // Fetch chunk
    auto events = fetch_klines_chunk(symbol, current_start, chunk_end, time_frame);

    // Write events to file
    for (const auto& event : events) {
      if (!event.data.is<veloz::market::KlineData>()) {
        continue;
      }
      const auto& kline = event.data.get<veloz::market::KlineData>();

      output_file << kline.start_time << "," << std::fixed << std::setprecision(8) << kline.open
                  << "," << kline.high << "," << kline.low << "," << kline.close << ","
                  << kline.volume << "," << kline.close_time << "\n";

      records_written++;
    }

    // Update progress
    progress.completed_chunks++;
    progress.total_records = records_written;
    progress.progress_fraction =
        static_cast<double>(progress.completed_chunks) / static_cast<double>(progress.total_chunks);
    progress_callback(progress);

    // Move to next chunk
    current_start = chunk_end;

    // Small delay between requests
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
  }

  output_file.close();

  // Final progress update
  progress.status = kj::str("Download complete: ", records_written, " records");
  progress.progress_fraction = 1.0;
  progress_callback(progress);

  // Validate if enabled
  if (download_options_.validate_data) {
    auto errors = validate_downloaded_data(output_path);
    if (errors.size() > 0) {
      logger.warn(kj::str("download_data_with_progress: Validation found ", errors.size(),
                          " issues in downloaded data"));
    }
  }

  logger.info(kj::str("download_data_with_progress: Successfully downloaded ", records_written,
                      " records to ", output_path_str));

  return true;
#endif
}

int BinanceDataSource::download_multiple_symbols(kj::ArrayPtr<const kj::String> symbols,
                                                 std::int64_t start_time, std::int64_t end_time,
                                                 kj::StringPtr data_type, kj::StringPtr time_frame,
                                                 kj::StringPtr output_directory) {
  veloz::core::Logger logger;
  int successful_downloads = 0;

#ifdef VELOZ_NO_CURL
  logger.error("download_multiple_symbols: libcurl not available");
  return 0;
#else
  if (symbols.size() == 0) {
    logger.warn("download_multiple_symbols: No symbols provided");
    return 0;
  }

  // Create output directory if needed
  kj::Path output_dir_path = kj::Path::parse("."_kj).eval(output_directory);
  bool outputPathAbsolute = output_directory.size() > 0 && output_directory[0] == '/';

  try {
    auto fs = kj::newDiskFilesystem();
    auto& root = outputPathAbsolute ? fs->getRoot() : fs->getCurrent();
    if (!root.exists(output_dir_path)) {
      root.openSubdir(output_dir_path, kj::WriteMode::CREATE | kj::WriteMode::CREATE_PARENT);
    }
  } catch (const kj::Exception& e) {
    logger.error(kj::str("download_multiple_symbols: Filesystem error: ", e.getDescription()));
    return 0;
  }

  logger.info(
      kj::str("download_multiple_symbols: Starting download for ", symbols.size(), " symbols"));

  for (const auto& symbol : symbols) {
    // Generate output filename
    kj::String filename = kj::str(symbol, "_", data_type, "_", time_frame, ".csv");
    kj::String output_path = kj::str(
        output_directory,
        output_directory.size() > 0 && output_directory[output_directory.size() - 1] != '/' ? "/"
                                                                                            : "",
        filename);

    logger.info(kj::str("download_multiple_symbols: Downloading ", symbol, " to ", output_path));

    bool success = download_data(symbol, start_time, end_time, data_type, time_frame, output_path);

    if (success) {
      successful_downloads++;
      logger.info(kj::str("download_multiple_symbols: Successfully downloaded ", symbol));
    } else {
      logger.error(kj::str("download_multiple_symbols: Failed to download ", symbol));
    }

    // Delay between symbols to respect rate limits
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
  }

  logger.info(kj::str("download_multiple_symbols: Completed ", successful_downloads, "/",
                      symbols.size(), " downloads"));

  return successful_downloads;
#endif
}

kj::Vector<kj::String> BinanceDataSource::validate_downloaded_data(kj::StringPtr file_path) {
  kj::Vector<kj::String> errors;
  veloz::core::Logger logger;

  auto fs = kj::newDiskFilesystem();
  auto path = kj::Path::parse("."_kj).eval(file_path);
  bool isAbsolute = file_path.size() > 0 && file_path[0] == '/';
  auto& root = isAbsolute ? fs->getRoot() : fs->getCurrent();

  if (!root.exists(path)) {
    errors.add(kj::str("File not found: ", file_path));
    return errors;
  }

  auto file = root.openFile(path);
  auto content = file->readAllText();
  kj::StringPtr content_ptr = content;

  std::int64_t line_number = 0;
  std::int64_t prev_timestamp = 0;
  bool first_data_line = true;

  size_t line_start = 0;
  for (size_t i = 0; i <= content_ptr.size(); ++i) {
    if (i < content_ptr.size() && content_ptr[i] != '\n') {
      continue;
    }

    auto line_slice = content_ptr.slice(line_start, i);
    line_start = i + 1;
    line_number++;

    if (line_slice.size() == 0) {
      continue;
    }

    // Remove trailing \r
    if (line_slice[line_slice.size() - 1] == '\r') {
      line_slice = line_slice.first(line_slice.size() - 1);
    }

    // Skip header line
    if (line_number == 1) {
      continue;
    }

    // Parse line
    kj::String line = kj::heapString(line_slice);

    // Tokenize
    kj::Vector<kj::String> tokens;
    size_t token_start = 0;
    for (size_t j = 0; j <= line.size(); ++j) {
      if (j < line.size() && line[j] != ',') {
        continue;
      }
      auto token = line.slice(token_start, j);
      token_start = j + 1;
      tokens.add(kj::heapString(token));
    }

    // Validate OHLCV format (7 columns: timestamp,open,high,low,close,volume,close_time)
    if (tokens.size() < 6) {
      errors.add(kj::str("Line ", line_number, ": Insufficient columns (expected 6+, got ",
                         tokens.size(), ")"));
      continue;
    }

    KJ_IF_SOME(exception, kj::runCatchingExceptions([&]() {
                 std::int64_t timestamp = tokens[0].parseAs<long long>();
                 double open = tokens[1].parseAs<double>();
                 double high = tokens[2].parseAs<double>();
                 double low = tokens[3].parseAs<double>();
                 double close = tokens[4].parseAs<double>();
                 double volume = tokens[5].parseAs<double>();

                 // Validate OHLCV constraints
                 if (high < low) {
                   errors.add(
                       kj::str("Line ", line_number, ": High (", high, ") < Low (", low, ")"));
                 }
                 if (high < open || high < close) {
                   errors.add(kj::str("Line ", line_number, ": High (", high, ") < Open or Close"));
                 }
                 if (low > open || low > close) {
                   errors.add(kj::str("Line ", line_number, ": Low (", low, ") > Open or Close"));
                 }
                 if (volume < 0) {
                   errors.add(kj::str("Line ", line_number, ": Negative volume (", volume, ")"));
                 }
                 if (open <= 0 || high <= 0 || low <= 0 || close <= 0) {
                   errors.add(kj::str("Line ", line_number, ": Non-positive price values"));
                 }

                 // Validate timestamp ordering
                 if (!first_data_line && timestamp <= prev_timestamp) {
                   errors.add(kj::str("Line ", line_number, ": Timestamp out of order (", timestamp,
                                      " <= ", prev_timestamp, ")"));
                 }

                 prev_timestamp = timestamp;
                 first_data_line = false;
               })) {
      errors.add(kj::str("Line ", line_number, ": Parse error - ", exception.getDescription()));
    }
  }

  if (errors.size() == 0) {
    logger.info(kj::str("validate_downloaded_data: File ", file_path, " is valid"));
  } else {
    logger.warn(kj::str("validate_downloaded_data: File ", file_path, " has ", errors.size(),
                        " validation errors"));
  }

  return errors;
}

kj::Vector<kj::String> BinanceDataSource::get_available_symbols() {
  kj::Vector<kj::String> symbols;
  veloz::core::Logger logger;

#ifdef VELOZ_NO_CURL
  logger.error("get_available_symbols: libcurl not available");
  return symbols;
#else
  kj::String url = kj::str(base_rest_url_, "/api/v3/exchangeInfo");

  rate_limit_wait();

  kj::String response = http_get(url);
  if (response.size() == 0) {
    logger.error("get_available_symbols: Failed to fetch exchange info");
    return symbols;
  }

  KJ_IF_SOME(exception, kj::runCatchingExceptions([&]() {
               auto doc = JsonDocument::parse(response);
               auto root = doc.root();

               // Check for error
               auto code = root["code"];
               if (code.is_int()) {
                 auto msg = root["msg"];
                 logger.error(
                     kj::str("Binance API error: ", code.get_int(), " - ", msg.get_string()));
                 return;
               }

               auto symbols_array = root["symbols"];
               if (!symbols_array.is_array()) {
                 logger.error("get_available_symbols: Unexpected response format");
                 return;
               }

               for (size_t i = 0; i < symbols_array.size(); ++i) {
                 auto symbol_obj = symbols_array[i];
                 auto symbol_name = symbol_obj["symbol"];
                 auto status = symbol_obj["status"];

                 if (symbol_name.is_string() && status.is_string() &&
                     status.get_string() == "TRADING"_kj) {
                   symbols.add(symbol_name.get_string());
                 }
               }
             })) {
    logger.error(kj::str("get_available_symbols: Parse error: ", exception.getDescription()));
  }

  logger.info(kj::str("get_available_symbols: Found ", symbols.size(), " trading symbols"));

  return symbols;
#endif
}

std::int64_t BinanceDataSource::get_server_time() {
  veloz::core::Logger logger;

#ifdef VELOZ_NO_CURL
  logger.error("get_server_time: libcurl not available");
  return 0;
#else
  kj::String url = kj::str(base_rest_url_, "/api/v3/time");

  rate_limit_wait();

  kj::String response = http_get(url);
  if (response.size() == 0) {
    logger.error("get_server_time: Failed to fetch server time");
    return 0;
  }

  std::int64_t server_time = 0;

  KJ_IF_SOME(exception, kj::runCatchingExceptions([&]() {
               auto doc = JsonDocument::parse(response);
               auto root = doc.root();

               // Check for error
               auto code = root["code"];
               if (code.is_int()) {
                 auto msg = root["msg"];
                 logger.error(
                     kj::str("Binance API error: ", code.get_int(), " - ", msg.get_string()));
                 return;
               }

               auto time_val = root["serverTime"];
               if (time_val.is_int()) {
                 server_time = time_val.get_int(0LL);
               }
             })) {
    logger.error(kj::str("get_server_time: Parse error: ", exception.getDescription()));
  }

  return server_time;
#endif
}

bool BinanceDataSource::symbol_exists(kj::StringPtr symbol) {
  veloz::core::Logger logger;

#ifdef VELOZ_NO_CURL
  logger.error("symbol_exists: libcurl not available");
  return false;
#else
  kj::String formatted_symbol = format_symbol(symbol);
  kj::String url = kj::str(base_rest_url_, "/api/v3/exchangeInfo?symbol=", formatted_symbol);

  rate_limit_wait();

  kj::String response = http_get(url);
  if (response.size() == 0) {
    return false;
  }

  bool exists = false;

  KJ_IF_SOME(exception, kj::runCatchingExceptions([&]() {
               auto doc = JsonDocument::parse(response);
               auto root = doc.root();

               // Check for error (symbol not found returns error)
               auto code = root["code"];
               if (code.is_int()) {
                 // Symbol doesn't exist
                 return;
               }

               auto symbols_array = root["symbols"];
               if (symbols_array.is_array() && symbols_array.size() > 0) {
                 exists = true;
               }
             })) {
    (void)exception;
  }

  return exists;
#endif
}

// DataSourceFactory implementation
kj::Rc<IDataSource> DataSourceFactory::create_data_source(kj::StringPtr type) {
  veloz::core::Logger logger;

  if (type == "csv"_kj) {
    return kj::rc<CSVDataSource>();
  } else if (type == "binance"_kj) {
    return kj::rc<BinanceDataSource>();
  }

  logger.error(kj::str("Unknown data source type: ", type));

  return nullptr;
}

} // namespace veloz::backtest
