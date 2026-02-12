#pragma once

#include <string>
#include <vector>
#include <memory>

// Include necessary headers
#include "veloz/market/market_event.h"
#include "veloz/backtest/backtest_engine.h"

namespace veloz::backtest {

// Base class for all data sources
class BaseDataSource : public IDataSource {
public:
    BaseDataSource();
    virtual ~BaseDataSource();

    bool connect() override;
    bool disconnect() override;

    virtual std::vector<veloz::market::MarketEvent> get_data(
        const std::string& symbol,
        std::int64_t start_time,
        std::int64_t end_time,
        const std::string& data_type,
        const std::string& time_frame
    ) override = 0;

    virtual bool download_data(
        const std::string& symbol,
        std::int64_t start_time,
        std::int64_t end_time,
        const std::string& data_type,
        const std::string& time_frame,
        const std::string& output_path
    ) override = 0;

protected:
    bool is_connected_;
};

// CSV file data source
class CSVDataSource : public BaseDataSource {
public:
    CSVDataSource();
    ~CSVDataSource() override;

    std::vector<veloz::market::MarketEvent> get_data(
        const std::string& symbol,
        std::int64_t start_time,
        std::int64_t end_time,
        const std::string& data_type,
        const std::string& time_frame
    ) override;

    bool download_data(
        const std::string& symbol,
        std::int64_t start_time,
        std::int64_t end_time,
        const std::string& data_type,
        const std::string& time_frame,
        const std::string& output_path
    ) override;

    void set_data_directory(const std::string& directory);

private:
    std::string data_directory_;
};

// Binance API data source
class BinanceDataSource : public BaseDataSource {
public:
    BinanceDataSource();
    ~BinanceDataSource() override;

    std::vector<veloz::market::MarketEvent> get_data(
        const std::string& symbol,
        std::int64_t start_time,
        std::int64_t end_time,
        const std::string& data_type,
        const std::string& time_frame
    ) override;

    bool download_data(
        const std::string& symbol,
        std::int64_t start_time,
        std::int64_t end_time,
        const std::string& data_type,
        const std::string& time_frame,
        const std::string& output_path
    ) override;

    void set_api_key(const std::string& api_key);
    void set_api_secret(const std::string& api_secret);

private:
    // API credentials
    std::string api_key_;
    std::string api_secret_;

    // API configuration
    std::string base_rest_url_;

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
    static std::shared_ptr<IDataSource> create_data_source(const std::string& type);
};

} // namespace veloz::backtest
