#pragma once

#include <kj/common.h>
#include <kj/memory.h>
#include <kj/string.h>
#include <kj/vector.h>
#include <memory>

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

// CSV file data source
class CSVDataSource : public BaseDataSource {
public:
  CSVDataSource();
  ~CSVDataSource() override;

  kj::Vector<veloz::market::MarketEvent> get_data(kj::StringPtr symbol, std::int64_t start_time,
                                                  std::int64_t end_time, kj::StringPtr data_type,
                                                  kj::StringPtr time_frame) override;

  bool download_data(kj::StringPtr symbol, std::int64_t start_time, std::int64_t end_time,
                     kj::StringPtr data_type, kj::StringPtr time_frame,
                     kj::StringPtr output_path) override;

  void set_data_directory(kj::StringPtr directory);

private:
  kj::String data_directory_;
};

// Binance API data source
class BinanceDataSource : public BaseDataSource {
public:
  BinanceDataSource();
  ~BinanceDataSource() override;

  kj::Vector<veloz::market::MarketEvent> get_data(kj::StringPtr symbol, std::int64_t start_time,
                                                  std::int64_t end_time, kj::StringPtr data_type,
                                                  kj::StringPtr time_frame) override;

  bool download_data(kj::StringPtr symbol, std::int64_t start_time, std::int64_t end_time,
                     kj::StringPtr data_type, kj::StringPtr time_frame,
                     kj::StringPtr output_path) override;

  void set_api_key(kj::StringPtr api_key);
  void set_api_secret(kj::StringPtr api_secret);

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

  // Private helper methods
  void rate_limit_wait();
};

// Data source factory
class DataSourceFactory {
public:
  // std::shared_ptr used for external API compatibility with IDataSource interface
  static std::shared_ptr<IDataSource> create_data_source(kj::StringPtr type);
};

} // namespace veloz::backtest
