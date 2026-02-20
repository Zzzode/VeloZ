#pragma once

// std::chrono for wall clock timestamps (KJ time is async I/O only)
#include <chrono>
// std::string for copyable structs (kj::String is not copyable)
#include <kj/common.h>
#include <kj/function.h>
#include <kj/map.h>
#include <kj/string.h>
#include <kj/vector.h>
#include <string>

namespace veloz::risk {

/**
 * @brief Exposure metrics for portfolio risk assessment
 */
struct ExposureMetrics {
  double gross_exposure{0.0};     // Sum of absolute position values
  double net_exposure{0.0};       // Sum of signed position values
  double long_exposure{0.0};      // Sum of long position values
  double short_exposure{0.0};     // Sum of short position values
  double leverage_ratio{0.0};     // gross_exposure / account_equity
  double net_leverage_ratio{0.0}; // net_exposure / account_equity
};

/**
 * @brief Concentration metrics for diversification analysis
 *
 * Uses std::string for largest_position_symbol because this struct
 * needs to be copyable (kj::String is not copyable).
 */
struct ConcentrationMetrics {
  std::string largest_position_symbol; // std::string for copyability
  double largest_position_pct{0.0};    // As % of total exposure
  double top3_concentration_pct{0.0};  // Top 3 positions as % of total
  double herfindahl_index{0.0};        // HHI for concentration (0-1)
  int position_count{0};
};

/**
 * @brief Risk metrics calculation result
 */
struct RiskMetrics {
  // VaR (Value at Risk) daily loss value at confidence level
  double var_95{0.0};
  double var_99{0.0};

  // Maximum drawdown
  double max_drawdown{0.0};

  // Sharpe ratio
  double sharpe_ratio{0.0};

  // Win rate
  double win_rate{0.0};

  // Profit factor
  double profit_factor{0.0};

  // Average daily return
  double avg_daily_return{0.0};

  // Return standard deviation
  double return_std{0.0};

  // Total number of trades
  int total_trades{0};

  // Number of winning trades
  int winning_trades{0};

  // Number of losing trades
  int losing_trades{0};

  // Maximum consecutive wins
  int max_consecutive_wins{0};

  // Maximum consecutive losses
  int max_consecutive_losses{0};

  // === New Phase 6 metrics ===

  // Exposure metrics
  ExposureMetrics exposure;

  // Concentration metrics
  ConcentrationMetrics concentration;

  // Correlation metrics
  double avg_correlation{0.0}; // Average pairwise correlation
  double max_correlation{0.0}; // Maximum pairwise correlation

  // Drawdown metrics
  double current_drawdown{0.0};        // Current drawdown from peak
  int64_t drawdown_start_ns{0};        // When current drawdown started
  int64_t max_drawdown_duration_ns{0}; // Longest drawdown duration

  // Risk-adjusted returns
  double sortino_ratio{0.0}; // Downside risk-adjusted return
  double calmar_ratio{0.0};  // Return / max drawdown
};

/**
 * @brief Trade history record
 */
struct TradeHistory {
  kj::String symbol;
  kj::String side;
  double entry_price{0.0};
  double exit_price{0.0};
  double quantity{0.0};
  double profit{0.0};
  // std::chrono for wall clock timestamps (KJ time is async I/O only)
  std::chrono::system_clock::time_point entry_time;
  std::chrono::system_clock::time_point exit_time;
};

/**
 * @brief Position value for real-time metrics calculation
 */
struct PositionValue {
  kj::String symbol;
  double size{0.0};     // Position size (signed)
  double price{0.0};    // Current price
  double notional{0.0}; // abs(size) * price
  double unrealized_pnl{0.0};
};

/**
 * @brief Risk metrics calculator (batch calculation from trade history)
 */
class RiskMetricsCalculator final {
public:
  RiskMetricsCalculator() = default;

  // Add trade history record
  void add_trade(const TradeHistory& trade);

  // Calculate all risk metrics
  RiskMetrics calculate_all() const;

  // Calculate VaR (Value at Risk)
  void calculate_var(RiskMetrics& metrics) const;

  // Calculate maximum drawdown
  void calculate_max_drawdown(RiskMetrics& metrics) const;

  // Calculate Sharpe ratio
  void calculate_sharpe_ratio(RiskMetrics& metrics) const;

  // Calculate trade statistics
  void calculate_trade_statistics(RiskMetrics& metrics) const;

  // Get trade history records
  const kj::Vector<TradeHistory>& get_trades() const;

  // Clear trade history records
  void clear_trades();

  // Set risk-free rate (for Sharpe ratio calculation)
  void set_risk_free_rate(double rate);

private:
  kj::Vector<TradeHistory> trades_;
  double risk_free_rate_{0.0}; // Default risk-free rate is 0%
};

/**
 * @brief Real-time risk metrics calculator
 *
 * Calculates exposure, concentration, and drawdown metrics in real-time
 * as positions and prices update.
 */
class RealTimeRiskMetrics final {
public:
  RealTimeRiskMetrics() = default;

  // Position updates
  void on_position_update(kj::StringPtr symbol, double size, double price);
  void on_price_update(kj::StringPtr symbol, double price);
  void remove_position(kj::StringPtr symbol);

  // Trade completion (for PnL tracking)
  void on_trade_complete(const TradeHistory& trade);

  // Get current metrics
  [[nodiscard]] ExposureMetrics get_exposure_metrics() const;
  [[nodiscard]] ConcentrationMetrics get_concentration_metrics() const;
  [[nodiscard]] double get_current_drawdown() const;
  [[nodiscard]] double get_peak_equity() const;

  // Get full metrics snapshot
  [[nodiscard]] RiskMetrics get_metrics_snapshot() const;

  // Configuration
  void set_account_equity(double equity);
  [[nodiscard]] double get_account_equity() const;

  // Reset state
  void reset();

  // Position count
  [[nodiscard]] size_t position_count() const;

private:
  // Recalculate derived metrics
  void recalculate_exposure();
  void recalculate_concentration();
  void update_drawdown();

  // Position tracking
  kj::HashMap<kj::String, PositionValue> positions_;

  // Account state
  double account_equity_{0.0};
  double peak_equity_{0.0};
  double current_equity_{0.0};

  // Cached metrics
  ExposureMetrics exposure_;
  ConcentrationMetrics concentration_;

  // Drawdown tracking
  double current_drawdown_{0.0};
  int64_t drawdown_start_ns_{0};
  int64_t max_drawdown_duration_ns_{0};
  double max_drawdown_{0.0};

  // PnL tracking
  double total_realized_pnl_{0.0};
  kj::Vector<double> daily_returns_; // For Sharpe/Sortino calculation
};

/**
 * @brief Correlation calculator for portfolio risk
 *
 * Calculates rolling correlation between asset returns.
 */
class CorrelationCalculator final {
public:
  explicit CorrelationCalculator(int window_days = 30);

  // Add return observation
  void add_return(kj::StringPtr symbol, double daily_return);

  // Calculate pairwise correlations
  [[nodiscard]] double get_average_correlation() const;
  [[nodiscard]] double get_max_correlation() const;
  [[nodiscard]] kj::Maybe<double> get_correlation(kj::StringPtr symbol1,
                                                  kj::StringPtr symbol2) const;

  // Reset calculator
  void reset();

private:
  int window_days_;
  // symbol -> vector of daily returns
  kj::HashMap<kj::String, kj::Vector<double>> returns_;
};

} // namespace veloz::risk
