#pragma once

#include "veloz/backtest/types.h"
#include "veloz/market/market_event.h"
#include "veloz/strategy/strategy.h"

#include <kj/common.h>
#include <kj/function.h>
#include <kj/memory.h>
#include <kj/string.h>
#include <kj/vector.h>
#include <memory>

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

  virtual kj::Vector<veloz::market::MarketEvent>
  get_data(kj::StringPtr symbol, std::int64_t start_time, std::int64_t end_time,
           kj::StringPtr data_type, kj::StringPtr time_frame) = 0;

  virtual bool download_data(kj::StringPtr symbol, std::int64_t start_time, std::int64_t end_time,
                             kj::StringPtr data_type, kj::StringPtr time_frame,
                             kj::StringPtr output_path) = 0;
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
  // std::shared_ptr used for external API compatibility with strategy interface
  virtual void set_strategy(const std::shared_ptr<veloz::strategy::IStrategy>& strategy) = 0;
  // std::shared_ptr used for external API compatibility with data source interface
  virtual void set_data_source(const std::shared_ptr<IDataSource>& data_source) = 0;

  virtual void on_progress(kj::Function<void(double progress)> callback) = 0;
};

// Backtest engine implementation
class BacktestEngine : public IBacktestEngine {
public:
  BacktestEngine();
  ~BacktestEngine() noexcept override;

  bool initialize(const BacktestConfig& config) override;
  bool run() override;
  bool stop() override;
  bool reset() override;

  BacktestResult get_result() const override;
  void set_strategy(const std::shared_ptr<veloz::strategy::IStrategy>& strategy) override;
  void set_data_source(const std::shared_ptr<IDataSource>& data_source) override;

  void on_progress(kj::Function<void(double progress)> callback) override;

private:
  struct Impl;
  kj::Own<Impl> impl_;
};

} // namespace veloz::backtest
