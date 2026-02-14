#include "veloz/backtest/data_source.h"

#include "veloz/core/json.h"
#include "veloz/core/logger.h"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <format>
#include <fstream>
#include <iostream>
#include <kj/common.h>
#include <kj/mutex.h>
#include <kj/string.h>
#include <kj/vector.h>
#include <mutex>
#include <random>
#include <sstream>
#include <thread>

#ifndef VELOZ_NO_CURL
#include <curl/curl.h>
#endif

namespace {

using namespace veloz::core;

// HTTP helper functions for Binance API
#ifndef VELOZ_NO_CURL
// Static helper for CURL write callback
size_t write_callback(void* contents, size_t size, size_t nmemb, void* userp) {
  ((std::string*)userp)->append((char*)contents, size * nmemb);
  return size * nmemb;
}

std::string http_get(kj::StringPtr url) {
  CURL* curl = curl_easy_init();
  if (!curl) {
    return "";
  }

  std::string response_string;

  curl_easy_setopt(curl, CURLOPT_URL, url.cStr());
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_string);
  curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
  curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);

  CURLcode res = curl_easy_perform(curl);
  if (res != CURLE_OK) {
    std::cerr << "HTTP GET failed: " << curl_easy_strerror(res) << std::endl;
  }

  curl_easy_cleanup(curl);

  return response_string;
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

CSVDataSource::~CSVDataSource() {}

kj::Vector<veloz::market::MarketEvent>
CSVDataSource::get_data(kj::StringPtr symbol, std::int64_t start_time, std::int64_t end_time,
                        kj::StringPtr data_type, kj::StringPtr time_frame) {
  kj::Vector<veloz::market::MarketEvent> events;
  veloz::core::Logger logger;

  // Construct file path: data_directory / symbol_data_type.csv
  // Example: /data/BTCUSDT_trade.csv or /data/BTCUSDT_kline_1h.csv
  std::filesystem::path file_path;

  if (data_type == "kline"_kj && time_frame.size() > 0) {
    file_path = std::filesystem::path(data_directory_.cStr()) /
                (std::string(symbol.cStr()) + "_" + std::string(time_frame.cStr()) + ".csv");
  } else {
    file_path = std::filesystem::path(data_directory_.cStr()) /
                (std::string(symbol.cStr()) + "_" + std::string(data_type.cStr()) + ".csv");
  }

  logger.info(std::format("Reading data from: {}", file_path.string()));

  // Check if file exists and is readable
  if (!std::filesystem::exists(file_path)) {
    logger.error(std::format("File not found: {}", file_path.string()));
    return events;
  }

  // Open the file
  std::ifstream file(file_path);
  if (!file.is_open()) {
    logger.error(std::format("Failed to open file: {}", file_path.string()));
    return events;
  }

  std::string line;
  int line_number = 0;
  int skipped_lines = 0;

  try {
    while (std::getline(file, line)) {
      line_number++;

      // Skip empty lines
      if (line.empty()) {
        continue;
      }

      // Skip header line (contains column names)
      if (line_number == 1 && (line.find("timestamp") != std::string::npos ||
                               line.find("Timestamp") != std::string::npos)) {
        continue;
      }

      // Parse CSV line
      std::vector<std::string> tokens;
      std::string token;
      std::istringstream token_stream(line);

      while (std::getline(token_stream, token, ',')) {
        // Remove surrounding whitespace
        token.erase(0, token.find_first_not_of(" \t\r\n"));
        token.erase(token.find_last_not_of(" \t\r\n") + 1);
        tokens.push_back(token);
      }

      // Validate minimum number of columns
      // Expected format: timestamp,symbol,side,price,quantity
      if (tokens.size() < 5) {
        skipped_lines++;
        logger.warn(
            std::format("Skipping malformed line {}: insufficient columns (expected 5, got {})",
                        line_number, tokens.size()));
        continue;
      }

      // Parse values
      veloz::market::MarketEvent event;
      veloz::market::TradeData trade_data;

      try {
        // Parse timestamp (convert milliseconds to nanoseconds)
        std::int64_t timestamp_ms = std::stoll(tokens[0]);
        event.ts_exchange_ns = timestamp_ms * 1'000'000; // ms to ns
        event.ts_recv_ns = event.ts_exchange_ns;
        event.ts_pub_ns = event.ts_exchange_ns;

        // Set symbol
        event.symbol = veloz::common::SymbolId(tokens[1]);

        // Set venue (default to Binance for CSV data)
        event.venue = veloz::common::Venue::Binance;
        event.market = veloz::common::MarketKind::Spot;

        // Parse side and set event type
        std::string side = tokens[2];
        std::transform(side.begin(), side.end(), side.begin(), ::tolower);

        if (side == "buy") {
          trade_data.is_buyer_maker = false;
        } else if (side == "sell") {
          trade_data.is_buyer_maker = true;
        } else {
          skipped_lines++;
          logger.warn(
              std::format("Skipping malformed line {}: invalid side '{}'", line_number, side));
          continue;
        }

        // Parse price
        trade_data.price = std::stod(tokens[3]);
        if (trade_data.price <= 0) {
          skipped_lines++;
          logger.warn(std::format("Skipping malformed line {}: invalid price '{}'", line_number,
                                  tokens[3]));
          continue;
        }

        // Parse quantity
        trade_data.qty = std::stod(tokens[4]);
        if (trade_data.qty <= 0) {
          skipped_lines++;
          logger.warn(std::format("Skipping malformed line {}: invalid quantity '{}'", line_number,
                                  tokens[4]));
          continue;
        }

        // Set trade_id (use line number as fallback)
        trade_data.trade_id = static_cast<std::int64_t>(line_number);

        // Set event type
        event.type = veloz::market::MarketEventType::Trade;
        event.data = trade_data;

        // Create JSON payload for backward compatibility
        event.payload = kj::str(std::format(
            R"({{"type":"trade","symbol":"{}","timestamp":{},"price":{},"quantity":{},"side":"{}"}})",
            tokens[1], timestamp_ms, trade_data.price, trade_data.qty, tokens[2]));

        // Apply time filters
        if (start_time > 0 && event.ts_exchange_ns < start_time * 1'000'000) {
          continue;
        }
        if (end_time > 0 && event.ts_exchange_ns > end_time * 1'000'000) {
          continue;
        }

        events.add(kj::mv(event));

      } catch (const std::exception& e) {
        skipped_lines++;
        logger.warn(std::format("Skipping malformed line {}: {}", line_number, e.what()));
      }
    }

    logger.info(std::format("Successfully read {} events from {} (skipped {} malformed lines)",
                            events.size(), file_path.string(), skipped_lines));

  } catch (const std::exception& e) {
    logger.error(std::format("Error reading file {}: {}", file_path.string(), e.what()));
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
    logger.error(std::format("download_data: end_time ({}) must be greater than start_time ({})",
                             end_time, start_time));
    return false;
  }

  if (symbol.size() == 0) {
    logger.error("download_data: Symbol cannot be empty");
    return false;
  }

  // For this implementation, we only support "trade" data type
  // Other data types can be implemented in the future (kline, book)
  if (data_type != "trade"_kj) {
    logger.error(std::format(
        "download_data: Unsupported data type '{}'. Only 'trade' is supported.", data_type.cStr()));
    return false;
  }

  // Create output directory if it doesn't exist
  std::filesystem::path output_file_path(output_path.cStr());

  try {
    std::filesystem::path output_dir = output_file_path.parent_path();
    if (!output_dir.empty() && !std::filesystem::exists(output_dir)) {
      if (!std::filesystem::create_directories(output_dir)) {
        logger.error(std::format("download_data: Failed to create output directory: {}",
                                 output_dir.string()));
        return false;
      }
      logger.info(std::format("Created output directory: {}", output_dir.string()));
    }
  } catch (const std::filesystem::filesystem_error& e) {
    logger.error(std::format("download_data: Filesystem error creating directory: {}", e.what()));
    return false;
  }

  // Open output file
  std::ofstream output_file(output_file_path);
  if (!output_file.is_open()) {
    logger.error(std::format("download_data: Failed to open output file for writing: {}",
                             output_file_path.string()));
    return false;
  }

  // Write CSV header
  output_file << "timestamp,symbol,side,price,quantity\n";

  // Initialize random number generator with seed based on symbol and start_time
  // This ensures deterministic output for the same parameters
  std::uint64_t seed = static_cast<std::uint64_t>(std::hash<std::string>{}(symbol.cStr())) ^
                       static_cast<std::uint64_t>(start_time);
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

  logger.info(std::format("Generating {} synthetic trade records for symbol {} from {} to {}",
                          total_trades, symbol.cStr(), start_time, end_time));

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
    std::string side = side_dist(rng) ? "buy" : "sell";

    // Advance time with some randomness
    std::int64_t time_increment =
        avg_trade_interval_ms + static_cast<std::int64_t>(rng() % 100); // Add 0-100ms variation
    current_time = std::min(current_time + time_increment, end_time);

    // Format price to 2 decimal places
    std::string price_str = std::format("{:.2f}", current_price);

    // Format quantity to appropriate precision (up to 6 decimal places)
    std::string qty_str;
    if (quantity < 0.01) {
      qty_str = std::format("{:.6f}", quantity);
    } else if (quantity < 1.0) {
      qty_str = std::format("{:.4f}", quantity);
    } else {
      qty_str = std::format("{:.2f}", quantity);
    }

    // Write CSV record
    output_file << current_time << "," << symbol.cStr() << "," << side << "," << price_str << ","
                << qty_str << "\n";

    records_written++;
  }

  output_file.close();

  // Verify file was written successfully
  if (!output_file.good()) {
    logger.error(std::format("download_data: Error occurred while writing to file: {}",
                             output_file_path.string()));
    return false;
  }

  logger.info(std::format("Successfully generated {} trade records to: {}", records_written,
                          output_file_path.string()));

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

BinanceDataSource::~BinanceDataSource() {
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
    logger.error(std::format("Binance API: invalid time frame '{}'. Valid frames: 1s, 1m, 3m, 5m, "
                             "15m, 30m, 1h, 2h, 4h, 6h, 8h, 12h, 1d, 3d, 1w, 1M",
                             effective_time_frame.cStr()));
    return events;
  }

  // Default to kline if data_type not specified
  kj::String effective_data_type = data_type.size() == 0 ? kj::str("kline") : kj::str(data_type);

  logger.info(std::format("Binance API: Fetching {} data for {} from {} to {} (time frame: {})",
                          effective_data_type.cStr(), symbol.cStr(), start_time, end_time,
                          effective_time_frame.cStr()));

  kj::String formatted_symbol = format_symbol(symbol);
  std::int64_t current_start_time = start_time;
  const std::int64_t kline_limit = 1000; // Binance API limit per request

  // For kline data, implement pagination
  if (effective_data_type == "kline"_kj) {
    int request_count = 0;
    std::int64_t total_klines = 0;

    while (current_start_time < end_time || (end_time == 0 && request_count == 0)) {
      // Build API URL
      std::ostringstream url;
      url << base_rest_url_.cStr() << "/api/v3/klines"
          << "?symbol=" << formatted_symbol.cStr() << "&interval=" << effective_time_frame.cStr()
          << "&limit=" << kline_limit;

      if (current_start_time > 0) {
        url << "&startTime=" << current_start_time;
      }
      if (end_time > 0) {
        url << "&endTime=" << end_time;
      }

      // Rate limiting: wait if approaching rate limit
      rate_limit_wait();

      // Fetch data with retry
      std::string response;
      for (int retry = 0; retry < max_retries_; ++retry) {
        response = http_get(kj::StringPtr(url.str().c_str()));

        if (!response.empty()) {
          break;
        }

        if (retry < max_retries_ - 1) {
          logger.warn(std::format("Binance API: Request failed, retrying ({}/{})...", retry + 1,
                                  max_retries_));
          std::this_thread::sleep_for(std::chrono::milliseconds(retry_delay_ms_ * (retry + 1)));
        }
      }

      if (response.empty()) {
        logger.error("Binance API: Failed to fetch data after all retries");
        break;
      }

      // Parse JSON response
      try {
        auto doc = JsonDocument::parse(response);
        auto root = doc.root();

        // Check for API error response
        auto code = root["code"];
        auto msg = root["msg"];
        if (code.is_int() && msg.is_string()) {
          int code_val = code.get_int();
          std::string msg_val = msg.get_string();

          // Handle rate limit errors (429)
          if (code_val == -1003 || code_val == -1021) {
            logger.warn(std::format(
                "Binance API: Rate limit exceeded (code {}), waiting and retrying...", code_val));
            std::this_thread::sleep_for(std::chrono::seconds(1));
            continue; // Retry same request
          }

          logger.error(std::format("Binance API error (code {}): {}", code_val, msg_val));
          break;
        }

        // Parse klines array
        if (!root.is_array()) {
          logger.error("Binance API: Unexpected response format (expected array)");
          break;
        }

        int klines_in_batch = 0;
        for (size_t i = 0; i < root.size(); ++i) {
          const auto kline = root[i];
          if (!kline.is_array() || kline.size() < 11) {
            continue;
          }

          try {
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
            kline_data.open = std::stod(kline[1].get_string());
            kline_data.high = std::stod(kline[2].get_string());
            kline_data.low = std::stod(kline[3].get_string());
            kline_data.close = std::stod(kline[4].get_string());
            kline_data.volume = std::stod(kline[5].get_string());
            kline_data.close_time = kline[6].get_int(0LL);

            // Set event properties
            event.type = veloz::market::MarketEventType::Kline;
            event.venue = veloz::common::Venue::Binance;
            event.market = veloz::common::MarketKind::Spot;
            event.symbol = veloz::common::SymbolId(formatted_symbol.cStr());
            event.ts_exchange_ns = kline_data.start_time * 1'000'000;
            event.ts_recv_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
                                   std::chrono::system_clock::now().time_since_epoch())
                                   .count();
            event.ts_pub_ns = event.ts_recv_ns;
            event.data = kline_data;
            event.payload = kj::str("");

            // Apply time filters
            if (start_time > 0 && kline_data.start_time < start_time) {
              continue;
            }
            if (end_time > 0 && kline_data.start_time > end_time) {
              break; // No more data within range
            }

            events.add(kj::mv(event));
            klines_in_batch++;

            // Update start time for next pagination
            current_start_time = kline_data.close_time + 1;

          } catch (const std::exception& e) {
            logger.warn(std::format("Binance API: Failed to parse kline data: {}", e.what()));
          }
        }

        total_klines += klines_in_batch;
        request_count++;

        // Check if we got fewer klines than limit - we've reached end
        if (klines_in_batch < kline_limit) {
          logger.info(std::format(
              "Binance API: Fetched {} klines in {} requests (batch ended with {} klines)",
              total_klines, request_count, klines_in_batch));
          break;
        }

        // Small delay between paginated requests to respect rate limits
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

      } catch (const std::exception& e) {
        logger.error(std::format("Binance API: Unexpected error: {}", e.what()));
        break;
      }
    }

    logger.info(std::format("Binance API: Successfully fetched {} klines for {} ({} requests)",
                            events.size(), formatted_symbol.cStr(), request_count));

    // Sort events by timestamp
    // Note: kj::Vector doesn't have std::sort compatibility, so we use a simple approach
    // For production, consider using a sortable container or implementing a sort

    return events;
  }

  // For trade data
  if (effective_data_type == "trade"_kj) {
    // Build API URL
    std::ostringstream url;
    url << base_rest_url_.cStr() << "/api/v3/trades"
        << "?symbol=" << formatted_symbol.cStr() << "&limit=1000";

    if (start_time > 0) {
      // Note: Binance trades endpoint doesn't support startTime directly
      // We'll fetch recent trades and filter by time
      logger.warn("Binance API: /api/v3/trades endpoint doesn't support time filtering, fetching "
                  "latest 1000 trades");
    }

    // Rate limiting
    rate_limit_wait();

    // Fetch data with retry
    std::string response;
    for (int retry = 0; retry < max_retries_; ++retry) {
      response = http_get(kj::StringPtr(url.str().c_str()));

      if (!response.empty()) {
        break;
      }

      if (retry < max_retries_ - 1) {
        logger.warn(std::format("Binance API: Request failed, retrying ({}/{})...", retry + 1,
                                max_retries_));
        std::this_thread::sleep_for(std::chrono::milliseconds(retry_delay_ms_ * (retry + 1)));
      }
    }

    if (response.empty()) {
      logger.error("Binance API: Failed to fetch trades data");
      return events;
    }

    // Parse JSON response
    try {
      auto doc = JsonDocument::parse(response);
      auto root = doc.root();

      // Check for API error response
      auto code = root["code"];
      auto msg = root["msg"];
      if (code.is_int() && msg.is_string()) {
        int code_val = code.get_int();
        std::string msg_val = msg.get_string();
        logger.error(std::format("Binance API error (code {}): {}", code_val, msg_val));
        return events;
      }

      if (!root.is_array()) {
        logger.error("Binance API: Unexpected trades response format (expected array)");
        return events;
      }

      for (size_t i = 0; i < root.size(); ++i) {
        const auto trade = root[i];
        try {
          veloz::market::MarketEvent event;
          veloz::market::TradeData trade_data;

          // Binance trade format:
          // id: trade ID
          // price: price
          // qty: quantity
          // time: timestamp
          // isBuyerMaker: true if buyer is maker
          // isBestMatch: true if best match

          trade_data.price = std::stod(trade["price"].get_string());
          trade_data.qty = std::stod(trade["qty"].get_string());
          trade_data.trade_id = trade["id"].get_int(0LL);
          trade_data.is_buyer_maker = trade["isBuyerMaker"].get_bool(false);

          std::int64_t trade_time = trade["time"].get_int(0LL);

          // Set event properties
          event.type = veloz::market::MarketEventType::Trade;
          event.venue = veloz::common::Venue::Binance;
          event.market = veloz::common::MarketKind::Spot;
          event.symbol = veloz::common::SymbolId(formatted_symbol.cStr());
          event.ts_exchange_ns = trade_time * 1'000'000;
          event.ts_recv_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
                                 std::chrono::system_clock::now().time_since_epoch())
                                 .count();
          event.ts_pub_ns = event.ts_recv_ns;
          event.data = trade_data;
          event.payload = kj::str("");

          // Apply time filters
          if (start_time > 0 && trade_time < start_time) {
            continue;
          }
          if (end_time > 0 && trade_time > end_time) {
            break; // No more data within range
          }

          events.add(kj::mv(event));

        } catch (const std::exception& e) {
          logger.warn(std::format("Binance API: Failed to parse trade data: {}", e.what()));
        }
      }

      logger.info(std::format("Binance API: Successfully fetched {} trades for {}", events.size(),
                              formatted_symbol.cStr()));

    } catch (const std::exception& e) {
      logger.error(std::format("Binance API: Unexpected error: {}", e.what()));
    }

    return events;
  }

  logger.error(std::format("Binance API: Unsupported data type '{}'. Supported types: kline, trade",
                           effective_data_type.cStr()));
  return events;
#endif
}

void BinanceDataSource::rate_limit_wait() {
  // Use std::mutex here as rate limiting requires static state
  static std::mutex rate_limit_mutex;
  static std::vector<std::chrono::steady_clock::time_point> request_times;
  static std::chrono::steady_clock::time_point second_window_start =
      std::chrono::steady_clock::now();
  static int requests_in_second = 0;

  std::lock_guard<std::mutex> lock(rate_limit_mutex);
  auto now = std::chrono::steady_clock::now();

  // Clean up old request times (older than 1 minute)
  auto one_minute_ago = now - std::chrono::minutes(1);
  request_times.erase(
      std::remove_if(request_times.begin(), request_times.end(),
                     [one_minute_ago](const auto& t) { return t < one_minute_ago; }),
      request_times.end());

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
  if (now - second_window_start >= std::chrono::seconds(1)) {
    second_window_start = now;
    requests_in_second = 0;
  }
  if (requests_in_second >= rate_limit_per_second_) {
    auto wait_time = std::chrono::duration_cast<std::chrono::milliseconds>(
        second_window_start + std::chrono::seconds(1) - now);
    if (wait_time.count() > 0) {
      std::this_thread::sleep_for(wait_time);
    }
    second_window_start = now;
    requests_in_second = 0;
  }

  request_times.push_back(now);
  requests_in_second++;
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
    logger.error(std::format("download_data: end_time ({}) must be greater than start_time ({})",
                             end_time, start_time));
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
    logger.error(std::format(
        "download_data: Unsupported data type '{}'. Only 'kline' is currently supported.",
        data_type.cStr()));
    return false;
  }

  if (time_frame.size() == 0) {
    logger.error("download_data: Time frame cannot be empty for kline data");
    return false;
  }

  if (!is_valid_time_frame(time_frame)) {
    logger.error(std::format("download_data: Invalid time frame '{}'. Valid values: 1s, 1m, 3m, "
                             "5m, 15m, 30m, 1h, 2h, 4h, 6h, 8h, 12h, 1d, 3d, 1w, 1M",
                             time_frame.cStr()));
    return false;
  }

  // Create output directory if it doesn't exist
  std::filesystem::path output_file_path(output_path.cStr());

  try {
    std::filesystem::path output_dir = output_file_path.parent_path();
    if (!output_dir.empty() && !std::filesystem::exists(output_dir)) {
      if (!std::filesystem::create_directories(output_dir)) {
        logger.error(std::format("download_data: Failed to create output directory: {}",
                                 output_dir.string()));
        return false;
      }
      logger.info(std::format("Created output directory: {}", output_dir.string()));
    }
  } catch (const std::filesystem::filesystem_error& e) {
    logger.error(std::format("download_data: Filesystem error creating directory: {}", e.what()));
    return false;
  }

  // Open output file
  std::ofstream output_file(output_file_path);
  if (!output_file.is_open()) {
    logger.error(std::format("download_data: Failed to open output file for writing: {}",
                             output_file_path.string()));
    return false;
  }

  // Write CSV header - format: timestamp,symbol,side,price,quantity
  // For kline data, we generate two synthetic trades per candle (open and close)
  output_file << "timestamp,symbol,side,price,quantity\n";

  // Format symbol for Binance API (uppercase)
  kj::String formatted_symbol = format_symbol(symbol);

  logger.info(std::format("Downloading kline data for {} from {} to {} (time frame: {})",
                          formatted_symbol.cStr(), start_time, end_time, time_frame.cStr()));

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

    // Build Binance API URL
    std::ostringstream url;
    url << base_rest_url_.cStr() << "/api/v3/klines"
        << "?symbol=" << formatted_symbol.cStr() << "&interval=" << time_frame.cStr()
        << "&startTime=" << current_start_time << "&endTime=" << request_end_time
        << "&limit=" << MAX_KLINES_PER_REQUEST;

    logger.info(std::format("Fetching klines from {} to {}", current_start_time, request_end_time));

    // Fetch data from Binance API
    std::string response = http_get(kj::StringPtr(url.str().c_str()));

    if (response.empty()) {
      logger.error(
          std::format("download_data: Empty response from Binance API for request: {}", url.str()));
      output_file.close();
      return false;
    }

    // Parse JSON response
    try {
      auto doc = JsonDocument::parse(response);
      auto root = doc.root();

      // Check for error
      auto code = root["code"];
      if (code.is_int()) {
        auto msg = root["msg"];
        logger.error(std::format("Binance API error: {} - {}", code.get_int(), msg.get_string()));
        output_file.close();
        return false;
      }

      if (!root.is_array()) {
        logger.error(std::format("Binance API returned unexpected response format"));
        output_file.close();
        return false;
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
        double open = std::stod(kline[1].get_string());
        double close = std::stod(kline[4].get_string());
        double volume = std::stod(kline[5].get_string());
        double taker_buy_base = std::stod(kline[9].get_string()); // Base asset buy volume
        [[maybe_unused]] double taker_buy_quote =
            std::stod(kline[10].get_string()); // Quote asset buy volume

        // Generate synthetic trade data from kline
        // We create two trades: one buy at open, one sell at close
        // This approximates the candle's price movement

        // Trade 1: Buy at open (taker buy at open)
        std::int64_t trade1_time = open_time;
        std::string trade1_side = "buy";
        double trade1_price = open;
        // Estimate quantity from volume (assume even distribution across trades)
        double trade1_qty = taker_buy_base / 2.0; // Half of taker buy volume at open

        // Trade 2: Sell at close (seller initiated at close)
        std::int64_t trade2_time = open_time + interval_ms;
        std::string trade2_side = "sell";
        double trade2_price = close;
        double trade2_qty = (volume - taker_buy_base) / 2.0; // Half of maker sell volume at close

        // Ensure quantities are positive
        if (trade1_qty <= 0)
          trade1_qty = volume / 4.0;
        if (trade2_qty <= 0)
          trade2_qty = volume / 4.0;

        // Write trade 1
        output_file << trade1_time << "," << formatted_symbol.cStr() << "," << trade1_side << ","
                    << std::fixed << std::setprecision(8) << trade1_price << "," << std::fixed
                    << std::setprecision(8) << trade1_qty << "\n";

        // Write trade 2
        output_file << trade2_time << "," << formatted_symbol.cStr() << "," << trade2_side << ","
                    << std::fixed << std::setprecision(8) << trade2_price << "," << std::fixed
                    << std::setprecision(8) << trade2_qty << "\n";

        total_klines++;
      }

      logger.info(std::format("Processed {} klines", root.size()));

      // If we got fewer klines than requested, we've reached the end
      if (root.size() < MAX_KLINES_PER_REQUEST) {
        break;
      }

    } catch (const std::exception& e) {
      logger.error(std::format("download_data: JSON parsing error: {}", e.what()));
      output_file.close();
      return false;
    }

    // Advance to next time window
    current_start_time = request_end_time;
  }

  output_file.close();

  // Verify file was written successfully
  if (!output_file.good()) {
    logger.error(std::format("download_data: Error occurred while writing to file: {}",
                             output_file_path.string()));
    return false;
  }

  logger.info(std::format("Successfully downloaded {} klines to: {}", total_klines,
                          output_file_path.string()));

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
std::shared_ptr<IDataSource> DataSourceFactory::create_data_source(kj::StringPtr type) {
  veloz::core::Logger logger;

  if (type == "csv"_kj) {
    return std::make_shared<CSVDataSource>();
  } else if (type == "binance"_kj) {
    return std::make_shared<BinanceDataSource>();
  }

  logger.error(std::format("Unknown data source type: {}", type.cStr()));

  return nullptr;
}

} // namespace veloz::backtest
