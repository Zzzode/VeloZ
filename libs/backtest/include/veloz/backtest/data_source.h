#pragma once

// KJ library includes
#include <kj/common.h>
#include <kj/memory.h>
#include <kj/refcount.h>
#include <kj/string.h>
#include <kj/vector.h>

// Include necessary headers
#include "veloz/backtest/backtest_engine.h"
#include "veloz/market/market_event.h"

namespace veloz::backtest {

// Base class for all data sources
class BaseDataSource : public IDataSource {
public:
  BaseDataSource();
  virtual ~BaseDataSource();

  bool connect() override;
  bool disconnect() override;

  virtual kj::Vector<veloz::market::MarketEvent>
  get_data(kj::StringPtr symbol, std::int64_t start_time, std::int64_t end_time,
           kj::StringPtr data_type, kj::StringPtr time_frame) override = 0;

  virtual bool download_data(kj::StringPtr symbol, std::int64_t start_time, std::int64_t end_time,
                             kj::StringPtr data_type, kj::StringPtr time_frame,
                             kj::StringPtr output_path) override = 0;

protected:
  bool is_connected_;
};

/**
 * @brief CSV file format enumeration
 */
enum class CSVFormat : uint8_t {
  Auto = 0,  ///< Auto-detect format from header
  Trade = 1, ///< Trade data: timestamp,symbol,side,price,quantity
  OHLCV = 2, ///< Candlestick data: timestamp,open,high,low,close,volume
  Book = 3   ///< Order book: timestamp,bid_price,bid_qty,ask_price,ask_qty
};

/**
 * @brief CSV parsing options
 */
struct CSVParseOptions {
  CSVFormat format{CSVFormat::Auto};
  char delimiter{','};
  bool has_header{true};
  bool skip_invalid_rows{true};
  std::int64_t max_rows{0};   ///< 0 = unlimited
  kj::String symbol_override; ///< Override symbol from file (empty = use file data)
  veloz::common::Venue venue{veloz::common::Venue::Binance};
  veloz::common::MarketKind market{veloz::common::MarketKind::Spot};
};

/**
 * @brief CSV parsing statistics
 */
struct CSVParseStats {
  std::int64_t total_rows{0};
  std::int64_t valid_rows{0};
  std::int64_t invalid_rows{0};
  std::int64_t skipped_rows{0};
  std::int64_t parse_time_ms{0};
  kj::String first_error;
  kj::Vector<kj::String> warnings;
};

/**
 * @brief Streaming callback for processing events as they are parsed
 */
using CSVStreamCallback = kj::Function<bool(veloz::market::MarketEvent&)>;

// CSV file data source
class CSVDataSource : public BaseDataSource {
public:
  CSVDataSource();
  ~CSVDataSource() noexcept override;

  kj::Vector<veloz::market::MarketEvent> get_data(kj::StringPtr symbol, std::int64_t start_time,
                                                  std::int64_t end_time, kj::StringPtr data_type,
                                                  kj::StringPtr time_frame) override;

  bool download_data(kj::StringPtr symbol, std::int64_t start_time, std::int64_t end_time,
                     kj::StringPtr data_type, kj::StringPtr time_frame,
                     kj::StringPtr output_path) override;

  void set_data_directory(kj::StringPtr directory);

  // Enhanced CSV parsing methods

  /**
   * @brief Set parsing options
   * @param options Parsing options
   */
  void set_parse_options(const CSVParseOptions& options);

  /**
   * @brief Get current parsing options
   * @return Current options
   */
  [[nodiscard]] const CSVParseOptions& get_parse_options() const;

  /**
   * @brief Load data from a specific file path
   * @param file_path Path to CSV file
   * @param start_time Start time filter (0 = no filter)
   * @param end_time End time filter (0 = no filter)
   * @return Vector of market events
   */
  kj::Vector<veloz::market::MarketEvent>
  load_file(kj::StringPtr file_path, std::int64_t start_time = 0, std::int64_t end_time = 0);

  /**
   * @brief Load data from multiple files
   * @param file_paths Vector of file paths
   * @param start_time Start time filter (0 = no filter)
   * @param end_time End time filter (0 = no filter)
   * @return Vector of market events (sorted by timestamp)
   */
  kj::Vector<veloz::market::MarketEvent> load_files(kj::ArrayPtr<const kj::String> file_paths,
                                                    std::int64_t start_time = 0,
                                                    std::int64_t end_time = 0);

  /**
   * @brief Stream data from a file with callback
   * @param file_path Path to CSV file
   * @param callback Callback function for each event (return false to stop)
   * @param start_time Start time filter (0 = no filter)
   * @param end_time End time filter (0 = no filter)
   * @return Number of events processed
   */
  std::int64_t stream_file(kj::StringPtr file_path, CSVStreamCallback callback,
                           std::int64_t start_time = 0, std::int64_t end_time = 0);

  /**
   * @brief Get parsing statistics from last operation
   * @return Parsing statistics
   */
  [[nodiscard]] const CSVParseStats& get_stats() const;

  /**
   * @brief Validate OHLCV data integrity
   * @param events Vector of events to validate
   * @return Vector of validation error messages (empty if valid)
   */
  static kj::Vector<kj::String>
  validate_ohlcv(kj::ArrayPtr<const veloz::market::MarketEvent> events);

  /**
   * @brief Detect CSV format from file header
   * @param file_path Path to CSV file
   * @return Detected format
   */
  static CSVFormat detect_format(kj::StringPtr file_path);

private:
  kj::String data_directory_;
  CSVParseOptions options_;
  CSVParseStats stats_;

  // Internal parsing methods
  kj::Maybe<veloz::market::MarketEvent> parse_trade_row(kj::ArrayPtr<const kj::String> tokens,
                                                        std::int64_t line_number);
  kj::Maybe<veloz::market::MarketEvent> parse_ohlcv_row(kj::ArrayPtr<const kj::String> tokens,
                                                        std::int64_t line_number);
  kj::Maybe<veloz::market::MarketEvent> parse_book_row(kj::ArrayPtr<const kj::String> tokens,
                                                       std::int64_t line_number);
  kj::Vector<kj::String> tokenize_line(kj::StringPtr line);
  CSVFormat detect_format_from_header(kj::StringPtr header_line);
};

/**
 * @brief Download progress callback
 */
struct BinanceDownloadProgress {
  std::int64_t total_chunks{0};
  std::int64_t completed_chunks{0};
  std::int64_t total_records{0};
  std::int64_t downloaded_bytes{0};
  double progress_fraction{0.0};
  kj::String current_date;
  kj::String status;
};

/**
 * @brief Download options for Binance data
 */
struct BinanceDownloadOptions {
  bool parallel_download{true};   ///< Enable parallel downloading
  int max_parallel_requests{4};   ///< Maximum concurrent requests
  bool validate_data{true};       ///< Validate downloaded data
  bool compress_output{false};    ///< Compress output file (gzip)
  bool append_to_existing{false}; ///< Append to existing file
  kj::String output_format;       ///< Output format: "csv" or "parquet"
};

// Binance API data source
class BinanceDataSource : public BaseDataSource {
public:
  BinanceDataSource();
  ~BinanceDataSource() noexcept override;

  kj::Vector<veloz::market::MarketEvent> get_data(kj::StringPtr symbol, std::int64_t start_time,
                                                  std::int64_t end_time, kj::StringPtr data_type,
                                                  kj::StringPtr time_frame) override;

  bool download_data(kj::StringPtr symbol, std::int64_t start_time, std::int64_t end_time,
                     kj::StringPtr data_type, kj::StringPtr time_frame,
                     kj::StringPtr output_path) override;

  void set_api_key(kj::StringPtr api_key);
  void set_api_secret(kj::StringPtr api_secret);

  // Enhanced download methods

  /**
   * @brief Set download options
   * @param options Download options
   */
  void set_download_options(const BinanceDownloadOptions& options);

  /**
   * @brief Get current download options
   * @return Current options
   */
  [[nodiscard]] const BinanceDownloadOptions& get_download_options() const;

  /**
   * @brief Download data with progress callback
   * @param symbol Trading symbol
   * @param start_time Start time in milliseconds
   * @param end_time End time in milliseconds
   * @param data_type Data type (kline, trade)
   * @param time_frame Time frame for kline data
   * @param output_path Output file path
   * @param progress_callback Callback for progress updates
   * @return true if download successful
   */
  bool
  download_data_with_progress(kj::StringPtr symbol, std::int64_t start_time, std::int64_t end_time,
                              kj::StringPtr data_type, kj::StringPtr time_frame,
                              kj::StringPtr output_path,
                              kj::Function<void(const BinanceDownloadProgress&)> progress_callback);

  /**
   * @brief Download data for multiple symbols
   * @param symbols Vector of trading symbols
   * @param start_time Start time in milliseconds
   * @param end_time End time in milliseconds
   * @param data_type Data type (kline, trade)
   * @param time_frame Time frame for kline data
   * @param output_directory Output directory path
   * @return Number of symbols successfully downloaded
   */
  int download_multiple_symbols(kj::ArrayPtr<const kj::String> symbols, std::int64_t start_time,
                                std::int64_t end_time, kj::StringPtr data_type,
                                kj::StringPtr time_frame, kj::StringPtr output_directory);

  /**
   * @brief Validate downloaded data file
   * @param file_path Path to data file
   * @return Vector of validation error messages (empty if valid)
   */
  static kj::Vector<kj::String> validate_downloaded_data(kj::StringPtr file_path);

  /**
   * @brief Get available symbols from Binance
   * @return Vector of available trading symbols
   */
  kj::Vector<kj::String> get_available_symbols();

  /**
   * @brief Get server time from Binance
   * @return Server time in milliseconds
   */
  std::int64_t get_server_time();

  /**
   * @brief Check if symbol exists on Binance
   * @param symbol Trading symbol
   * @return true if symbol exists
   */
  bool symbol_exists(kj::StringPtr symbol);

private:
  // API credentials
  kj::String api_key_;
  kj::String api_secret_;

  // API configuration
  kj::String base_rest_url_;

  // Retry configuration
  int max_retries_;
  int retry_delay_ms_;

  // Rate limiting
  int rate_limit_per_minute_;
  int rate_limit_per_second_;

  // Download options
  BinanceDownloadOptions download_options_;

  // Private helper methods
  void rate_limit_wait();

  // Internal download methods
  kj::Vector<veloz::market::MarketEvent> fetch_klines_chunk(kj::StringPtr symbol,
                                                            std::int64_t start_time,
                                                            std::int64_t end_time,
                                                            kj::StringPtr time_frame);
};

// Data source factory
class DataSourceFactory {
public:
  // kj::Rc returned to match strategy module's factory pattern (kj::Rc<IStrategy>)
  // and allow reference-counted ownership across multiple backtest engines and optimizers
  static kj::Rc<IDataSource> create_data_source(kj::StringPtr type);
};

} // namespace veloz::backtest
