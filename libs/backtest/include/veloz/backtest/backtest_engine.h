#pragma once

#include "veloz/backtest/types.h"
#include "veloz/market/market_event.h"
#include "veloz/strategy/strategy.h"

#include <functional>
#include <memory>
#include <vector>

namespace veloz::backtest {

// Forward declarations
class IDataSource;
class IBacktestEngine;

// Data source interface
class IDataSource {
public:
  virtual ~IDataSource() = default;

  virtual bool connect() = 0;
  virtual bool disconnect() = 0;

  virtual std::vector<veloz::market::MarketEvent>
  get_data(const std::string& symbol, std::int64_t start_time, std::int64_t end_time,
           const std::string& data_type, const std::string& time_frame) = 0;

  virtual bool download_data(const std::string& symbol, std::int64_t start_time,
                             std::int64_t end_time, const std::string& data_type,
                             const std::string& time_frame, const std::string& output_path) = 0;
};

// Backtest engine interface
class IBacktestEngine {
public:
  virtual ~IBacktestEngine() = default;

  virtual bool initialize(const BacktestConfig& config) = 0;
  virtual bool run() = 0;
  virtual bool stop() = 0;
  virtual bool reset() = 0;

  virtual BacktestResult get_result() const = 0;
  virtual void set_strategy(const std::shared_ptr<veloz::strategy::IStrategy>& strategy) = 0;
  virtual void set_data_source(const std::shared_ptr<IDataSource>& data_source) = 0;

  virtual void on_progress(std::function<void(double progress)> callback) = 0;
};

// Backtest engine implementation
class BacktestEngine : public IBacktestEngine {
public:
  BacktestEngine();
  ~BacktestEngine() override;

  bool initialize(const BacktestConfig& config) override;
  bool run() override;
  bool stop() override;
  bool reset() override;

  BacktestResult get_result() const override;
  void set_strategy(const std::shared_ptr<veloz::strategy::IStrategy>& strategy) override;
  void set_data_source(const std::shared_ptr<IDataSource>& data_source) override;

  void on_progress(std::function<void(double progress)> callback) override;

private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

} // namespace veloz::backtest
