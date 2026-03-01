#pragma once

#include "veloz/backtest/types.h"

#include <kj/common.h>
#include <kj/memory.h>
#include <kj/string.h>
#include <kj/vector.h>

namespace veloz::backtest {

/**
 * @brief Report output format
 */
enum class ReportFormat : uint8_t { HTML = 0, JSON = 1, CSV = 2, Markdown = 3 };

/**
 * @brief Report configuration options
 */
struct ReportConfig {
  bool include_equity_curve{true};
  bool include_drawdown_curve{true};
  bool include_trade_list{true};
  bool include_monthly_returns{true};
  bool include_trade_analysis{true};
  bool include_risk_metrics{true};
  kj::String title;
  kj::String description;
  kj::String author;

  ReportConfig()
      : title(kj::str("VeloZ Backtest Report")), description(kj::str("")), author(kj::str("")) {}
};

/**
 * @brief Monthly return data
 */
struct MonthlyReturn {
  int year;
  int month;
  double return_pct;
  int trade_count;
  double max_drawdown;
};

/**
 * @brief Trade analysis statistics
 */
struct TradeAnalysis {
  // Best/worst trades
  double best_trade_pnl{0.0};
  double worst_trade_pnl{0.0};
  std::int64_t best_trade_timestamp{0};
  std::int64_t worst_trade_timestamp{0};

  // Consecutive trades
  int max_consecutive_wins{0};
  int max_consecutive_losses{0};
  int current_streak{0};
  bool current_streak_winning{false};

  // Trade duration
  double avg_trade_duration_ms{0.0};
  double max_trade_duration_ms{0.0};
  double min_trade_duration_ms{0.0};

  // Holding period analysis
  double avg_winning_duration_ms{0.0};
  double avg_losing_duration_ms{0.0};
};

/**
 * @brief Extended risk metrics
 */
struct ExtendedRiskMetrics {
  double sortino_ratio{0.0};
  double calmar_ratio{0.0};
  double omega_ratio{0.0};
  double tail_ratio{0.0};
  double value_at_risk_95{0.0};
  double expected_shortfall_95{0.0};
  double skewness{0.0};
  double kurtosis{0.0};
  double recovery_factor{0.0};
  double ulcer_index{0.0};
};

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

  // Enhanced report generation

  /**
   * @brief Set report configuration
   * @param config Report configuration options
   */
  void set_config(const ReportConfig& config);

  /**
   * @brief Get current report configuration
   * @return Current configuration
   */
  [[nodiscard]] const ReportConfig& get_config() const;

  /**
   * @brief Generate report in specified format
   * @param result Backtest result
   * @param output_path Output file path
   * @param format Output format
   * @return true if successful
   */
  bool generate_report_format(const BacktestResult& result, kj::StringPtr output_path,
                              ReportFormat format);

  /**
   * @brief Generate CSV trade list
   * @param result Backtest result
   * @return CSV string
   */
  kj::String generate_csv_trades(const BacktestResult& result);

  /**
   * @brief Generate Markdown report
   * @param result Backtest result
   * @return Markdown string
   */
  kj::String generate_markdown_report(const BacktestResult& result);

  /**
   * @brief Calculate monthly returns
   * @param result Backtest result
   * @return Vector of monthly returns
   */
  static kj::Vector<MonthlyReturn> calculate_monthly_returns(const BacktestResult& result);

  /**
   * @brief Analyze trades
   * @param result Backtest result
   * @return Trade analysis statistics
   */
  static TradeAnalysis analyze_trades(const BacktestResult& result);

  /**
   * @brief Calculate extended risk metrics
   * @param result Backtest result
   * @return Extended risk metrics
   */
  static ExtendedRiskMetrics calculate_extended_metrics(const BacktestResult& result);

  /**
   * @brief Export equity curve to CSV
   * @param result Backtest result
   * @param output_path Output file path
   * @return true if successful
   */
  bool export_equity_curve_csv(const BacktestResult& result, kj::StringPtr output_path);

  /**
   * @brief Export drawdown curve to CSV
   * @param result Backtest result
   * @param output_path Output file path
   * @return true if successful
   */
  bool export_drawdown_curve_csv(const BacktestResult& result, kj::StringPtr output_path);

  /**
   * @brief Generate comparison report for multiple results
   * @param results Vector of backtest results to compare
   * @param output_path Output file path
   * @return true if successful
   */
  bool generate_comparison_report(kj::ArrayPtr<const BacktestResult> results,
                                  kj::StringPtr output_path);

private:
  struct Impl;
  kj::Own<Impl> impl_;

  // Internal helper methods
  kj::String generate_monthly_returns_html(const kj::Vector<MonthlyReturn>& monthly_returns);
  kj::String generate_trade_analysis_html(const TradeAnalysis& analysis);
  kj::String generate_extended_metrics_html(const ExtendedRiskMetrics& metrics);
};

} // namespace veloz::backtest
