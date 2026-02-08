#pragma once

#include <string>
#include <memory>

#include "veloz/backtest/types.h"

namespace veloz::backtest {

// Backtest reporter interface
class IBacktestReporter {
public:
    virtual ~IBacktestReporter() = default;

    virtual bool generate_report(const BacktestResult& result, const std::string& output_path) = 0;
    virtual std::string generate_html_report(const BacktestResult& result) = 0;
    virtual std::string generate_json_report(const BacktestResult& result) = 0;
};

// Backtest reporter implementation
class BacktestReporter : public IBacktestReporter {
public:
    BacktestReporter();
    ~BacktestReporter() override;

    bool generate_report(const BacktestResult& result, const std::string& output_path) override;
    std::string generate_html_report(const BacktestResult& result) override;
    std::string generate_json_report(const BacktestResult& result) override;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace veloz::backtest
