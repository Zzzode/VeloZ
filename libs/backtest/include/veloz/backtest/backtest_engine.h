#pragma once

#include "veloz/backtest/types.h"
#include "veloz/market/market_event.h"
#include "veloz/strategy/strategy.h"

// KJ library includes
#include <kj/common.h>
#include <kj/function.h>
#include <kj/memory.h>
#include <kj/refcount.h>
#include <kj/string.h>
#include <kj/vector.h>

namespace veloz::backtest {

// Forward declarations
class IDataSource;
class IBacktestEngine;

/**
 * @brief Data source interface
 *
 * Inherits from kj::Refcounted to support kj::Rc<IDataSource> reference counting.
 * Note: kj::Rc is single-threaded; use kj::Arc for thread-safe sharing.
 */
class IDataSource : public kj::Refcounted {
public:
  ~IDataSource() noexcept(false) override = default;

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
  // kj::Rc: matches strategy module's API (IStrategyFactory returns kj::Rc)
  virtual void set_strategy(kj::Rc<veloz::strategy::IStrategy> strategy) = 0;
  // kj::Rc: matches DataSourceFactory return type for reference-counted ownership
  virtual void set_data_source(kj::Rc<IDataSource> data_source) = 0;

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
  void set_strategy(kj::Rc<veloz::strategy::IStrategy> strategy) override;
  void set_data_source(kj::Rc<IDataSource> data_source) override;

  void on_progress(kj::Function<void(double progress)> callback) override;

private:
  struct Impl;
  kj::Own<Impl> impl_;
};

} // namespace veloz::backtest
