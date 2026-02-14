#pragma once

#include "veloz/backtest/types.h"

#include <kj/common.h>
#include <kj/memory.h>
#include <kj/string.h>

namespace veloz::backtest {

// Backtest reporter interface
class IBacktestReporter {
public:
  virtual ~IBacktestReporter() = default;

  virtual bool generate_report(const BacktestResult& result, kj::StringPtr output_path) = 0;
  virtual kj::String generate_html_report(const BacktestResult& result) = 0;
  virtual kj::String generate_json_report(const BacktestResult& result) = 0;
};

// Backtest reporter implementation
class BacktestReporter : public IBacktestReporter {
public:
  BacktestReporter();
  ~BacktestReporter() noexcept override;

  bool generate_report(const BacktestResult& result, kj::StringPtr output_path) override;
  kj::String generate_html_report(const BacktestResult& result) override;
  kj::String generate_json_report(const BacktestResult& result) override;

private:
  struct Impl;
  kj::Own<Impl> impl_;
};

} // namespace veloz::backtest
