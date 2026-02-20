#include "veloz/backtest/data_source.h"

// std library includes with justifications
#include <algorithm>  // std::remove_if - standard algorithm (no KJ equivalent)
#include <chrono>     // std::chrono - wall clock time (KJ time is async I/O only)
#include <format>     // std::format - needed for width/precision specifiers (kj::str lacks this)
#include <fstream>    // std::ifstream, std::ofstream - file I/O (no KJ equivalent)
#include <iostream>   // std::cerr - error output (no KJ equivalent)
#include <random>     // std::random_device, std::mt19937 - KJ has no RNG
#include <thread>     // std::this_thread::sleep_for - thread sleep (no KJ equivalent)

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

kj::String http_get(kj::StringPtr url) {
  CurlHandle curl;
  if (!curl) {
    return kj::str("");
  }

  kj::Vector<char> response_string;

  curl_easy_setopt(curl.get(), CURLOPT_URL, url.cStr());
  curl_easy_setopt(curl.get(), CURLOPT_WRITEFUNCTION, write_callback);
  curl_easy_setopt(curl.get(), CURLOPT_WRITEDATA, &response_string);
  curl_easy_setopt(curl.get(), CURLOPT_FOLLOWLOCATION, 1L);
  curl_easy_setopt(curl.get(), CURLOPT_TIMEOUT, 30L);

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
                                ": insufficient columns (expected 5, got ", tokens.size(), ")")
                            .cStr());
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
                                             ": invalid side '", side_lower, "'")
                                         .cStr());
                         return;
                       }

                       // Parse price
                       trade_data.price = tokens[3].parseAs<double>();
                       if (trade_data.price <= 0) {
                         skipped_lines++;
                         logger.warn(kj::str("Skipping malformed line ", line_number,
                                             ": invalid price '", tokens[3], "'")
                                         .cStr());
                         return;
                       }

                       // Parse quantity
                       trade_data.qty = tokens[4].parseAs<double>();
                       if (trade_data.qty <= 0) {
                         skipped_lines++;
                         logger.warn(kj::str("Skipping malformed line ", line_number,
                                             ": invalid quantity '", tokens[4], "'")
                                         .cStr());
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
                                inner_exception.getDescription())
                            .cStr());
          }
        }

        logger.info(kj::str("Successfully read ", events.size(), " events from ", path_str.cStr(),
                            " (skipped ", skipped_lines, " malformed lines)")
                        .cStr());
      })) {
    // Outer exception occurred - log file reading error
    logger.error(
        kj::str("Error reading file ", path_str.cStr(), ": ", outer_exception.getDescription())
            .cStr());
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
        logger.info(kj::str("Created output directory: ", parent.toString(outputPathAbsolute)).cStr());
      }
    }
  } catch (const kj::Exception& e) {
    logger.error(kj::str("download_data: Filesystem error creating directory: ", e.getDescription()).cStr());
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
                      " from ", start_time, " to ", end_time)
                  .cStr());

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
                      " trade records to: ", output_path_str.cStr())
                  .cStr());

  return true;
}

void CSVDataSource::set_data_directory(kj::StringPtr directory) {
  data_directory_ = kj::str(directory);
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
                         "3d, 1w, 1M")
                     .cStr());
    return events;
  }

  // Default to kline if data_type not specified
  kj::String effective_data_type = data_type.size() == 0 ? kj::str("kline") : kj::str(data_type);

  logger.info(kj::str("Binance API: Fetching ", effective_data_type, " data for ", symbol, " from ",
                      start_time, " to ", end_time, " (time frame: ", effective_time_frame, ")")
                  .cStr());

  kj::String formatted_symbol = format_symbol(symbol);
  std::int64_t current_start_time = start_time;
  const std::int64_t kline_limit = 1000; // Binance API limit per request

  // For kline data, implement pagination
  if (effective_data_type == "kline"_kj) {
    int request_count = 0;
    std::int64_t total_klines = 0;

    while (current_start_time < end_time || (end_time == 0 && request_count == 0)) {
      // Build API URL using kj::str
      kj::String url = kj::str(base_rest_url_, "/api/v3/klines?symbol=", formatted_symbol,
                               "&interval=", effective_time_frame, "&limit=", kline_limit,
                               current_start_time > 0 ? kj::str("&startTime=", current_start_time)
                                                      : kj::str(""),
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
                              max_retries_, ")...")
                          .cStr());
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

              // Handle rate limit errors (429)
              if (code_val == -1003 || code_val == -1021) {
                logger.warn(kj::str("Binance API: Rate limit exceeded (code ", code_val,
                                    "), waiting and retrying...")
                                .cStr());
                std::this_thread::sleep_for(std::chrono::seconds(1));
                should_continue = true;
                return;
              }

              logger.error(
                  kj::str("Binance API error (code ", code_val, "): ", msg_val.cStr()).cStr());
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
                           event.symbol = veloz::common::SymbolId(formatted_symbol.cStr());
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
                                    inner_exception.getDescription())
                                .cStr());
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
                                  request_count, " requests (batch ended with ", klines_in_batch,
                                  " klines)")
                              .cStr());
              should_break = true;
              return;
            }

            // Small delay between paginated requests to respect rate limits
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
          })) {
        // Outer exception - log and break
        logger.error(
            kj::str("Binance API: Unexpected error: ", outer_exception.getDescription()).cStr());
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
                        formatted_symbol, " (", request_count, " requests)")
                    .cStr());

    // Sort events by timestamp
    // Note: kj::Vector doesn't have std::sort compatibility, so we use a simple approach
    // For production, consider using a sortable container or implementing a sort

    return events;
  }

  // For trade data
  if (effective_data_type == "trade"_kj) {
    // Build API URL using kj::str
    kj::String url = kj::str(base_rest_url_, "/api/v3/trades?symbol=", formatted_symbol, "&limit=1000");

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
        logger.warn(
            kj::str("Binance API: Request failed, retrying (", retry + 1, "/", max_retries_, ")...")
                .cStr());
        std::this_thread::sleep_for(std::chrono::milliseconds(retry_delay_ms_ * (retry + 1)));
      }
    }

    if (response.size() == 0) {
      logger.error("Binance API: Failed to fetch trades data");
      return events;
    }

    // Parse JSON response using KJ exception handling
    KJ_IF_SOME(outer_exception, kj::runCatchingExceptions([&]() {
                 auto doc = JsonDocument::parse(response);
                 auto root = doc.root();

                 // Check for API error response
                 auto code = root["code"];
                 auto msg = root["msg"];
                 if (code.is_int() && msg.is_string()) {
                   int code_val = code.get_int();
                   kj::String msg_val = msg.get_string();
                   logger.error(
                       kj::str("Binance API error (code ", code_val, "): ", msg_val.cStr()).cStr());
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
                                event.symbol = veloz::common::SymbolId(formatted_symbol.cStr());
                                event.ts_exchange_ns = trade_time * 1'000'000;
                                event.ts_recv_ns =
                                    std::chrono::duration_cast<std::chrono::nanoseconds>(
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
                                         inner_exception.getDescription())
                                     .cStr());
                   }

                   if (should_break) {
                     break;
                   }
                 }

                 logger.info(kj::str("Binance API: Successfully fetched ", events.size(),
                                     " trades for ", formatted_symbol)
                                 .cStr());
               })) {
      // Outer exception - log error
      logger.error(
          kj::str("Binance API: Unexpected error: ", outer_exception.getDescription()).cStr());
    }

    return events;
  }

  logger.error(kj::str("Binance API: Unsupported data type '", effective_data_type,
                       "'. Supported types: kline, trade")
                   .cStr());
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
                         ") must be greater than start_time (", start_time, ")")
                     .cStr());
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
                         "'. Only 'kline' is currently supported.")
                     .cStr());
    return false;
  }

  if (time_frame.size() == 0) {
    logger.error("download_data: Time frame cannot be empty for kline data");
    return false;
  }

  if (!is_valid_time_frame(time_frame)) {
    logger.error(kj::str("download_data: Invalid time frame '", time_frame,
                         "'. Valid values: 1s, 1m, 3m, 5m, 15m, 30m, 1h, 2h, 4h, 6h, 8h, 12h, 1d, "
                         "3d, 1w, 1M")
                     .cStr());
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
        logger.info(kj::str("Created output directory: ", parent.toString(outputPathAbsolute)).cStr());
      }
    }
  } catch (const kj::Exception& e) {
    logger.error(kj::str("download_data: Filesystem error creating directory: ", e.getDescription()).cStr());
    return false;
  }

  // Open output file - convert kj::Path string representation to const char*
  auto output_path_str = output_file_path.toString(outputPathAbsolute);
  std::ofstream output_file(output_path_str.cStr());
  if (!output_file.is_open()) {
    logger.error(
        kj::str("download_data: Failed to open output file for writing: ", output_path_str.cStr())
            .cStr());
    return false;
  }

  // Write CSV header - format: timestamp,symbol,side,price,quantity
  // For kline data, we generate two synthetic trades per candle (open and close)
  output_file << "timestamp,symbol,side,price,quantity\n";

  // Format symbol for Binance API (uppercase)
  kj::String formatted_symbol = format_symbol(symbol);

  logger.info(kj::str("Downloading kline data for ", formatted_symbol, " from ", start_time, " to ",
                      end_time, " (time frame: ", time_frame, ")")
                  .cStr());

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

    logger.info(
        kj::str("Fetching klines from ", current_start_time, " to ", request_end_time));

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

                 logger.info(kj::str("Processed ", root.size(), " klines").cStr());

                 // If we got fewer klines than requested, we've reached the end
                 if (root.size() < MAX_KLINES_PER_REQUEST) {
                   should_break = true;
                 }
               })) {
      // Exception occurred - log and return error
      logger.error(
          kj::str("download_data: JSON parsing error: ", exception.getDescription()).cStr());
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
        kj::str("download_data: Error occurred while writing to file: ", output_path_str.cStr())
            .cStr());
    return false;
  }

  logger.info(
      kj::str("Successfully downloaded ", total_klines, " klines to: ", output_path_str.cStr())
          .cStr());

  return true;
#endif // VELOZ_NO_CURL
}

void BinanceDataSource::set_api_key(kj::StringPtr api_key) {
  api_key_ = kj::str(api_key);
}

void BinanceDataSource::set_api_secret(kj::StringPtr api_secret) {
  api_secret_ = kj::str(api_secret);
}

// DataSourceFactory implementation
kj::Rc<IDataSource> DataSourceFactory::create_data_source(kj::StringPtr type) {
  veloz::core::Logger logger;

  if (type == "csv"_kj) {
    return kj::rc<CSVDataSource>();
  } else if (type == "binance"_kj) {
    return kj::rc<BinanceDataSource>();
  }

  logger.error(kj::str("Unknown data source type: ", type).cStr());

  return nullptr;
}

} // namespace veloz::backtest
