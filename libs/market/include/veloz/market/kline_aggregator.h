/**
 * @file kline_aggregator.h
 * @brief K-line (candlestick) aggregation for multiple timeframes
 *
 * This file provides real-time K-line aggregation from trade data,
 * supporting multiple concurrent timeframes (1m, 5m, 15m, 1h, etc.).
 */

#pragma once

#include "veloz/market/market_event.h"

#include <kj/common.h>
#include <kj/function.h>
#include <kj/map.h>
#include <kj/string.h>
#include <kj/vector.h>

namespace veloz::market {

/**
 * @brief Supported K-line timeframe intervals
 */
enum class KlineInterval : uint8_t {
  Min1 = 0,  ///< 1 minute
  Min5 = 1,  ///< 5 minutes
  Min15 = 2, ///< 15 minutes
  Min30 = 3, ///< 30 minutes
  Hour1 = 4, ///< 1 hour
  Hour4 = 5, ///< 4 hours
  Day1 = 6,  ///< 1 day
};

/**
 * @brief Convert interval enum to milliseconds
 */
[[nodiscard]] constexpr int64_t interval_to_ms(KlineInterval interval) {
  switch (interval) {
  case KlineInterval::Min1:
    return 60 * 1000LL;
  case KlineInterval::Min5:
    return 5 * 60 * 1000LL;
  case KlineInterval::Min15:
    return 15 * 60 * 1000LL;
  case KlineInterval::Min30:
    return 30 * 60 * 1000LL;
  case KlineInterval::Hour1:
    return 60 * 60 * 1000LL;
  case KlineInterval::Hour4:
    return 4 * 60 * 60 * 1000LL;
  case KlineInterval::Day1:
    return 24 * 60 * 60 * 1000LL;
  }
  return 60 * 1000LL; // Default to 1 minute
}

/**
 * @brief Convert interval enum to string representation
 */
[[nodiscard]] kj::StringPtr interval_to_string(KlineInterval interval);

/**
 * @brief Extended K-line data with additional statistics
 */
struct AggregatedKline {
  KlineData kline;         ///< Base K-line data (OHLCV)
  double vwap{0.0};        ///< Volume-weighted average price
  int64_t trade_count{0};  ///< Number of trades in this candle
  double buy_volume{0.0};  ///< Buy-side volume (taker buy)
  double sell_volume{0.0}; ///< Sell-side volume (taker sell)
  bool is_closed{false};   ///< Whether this candle is finalized

  auto operator<=>(const AggregatedKline&) const = default;
};

/**
 * @brief Callback type for K-line updates
 */
using KlineCallback = kj::Function<void(KlineInterval, const AggregatedKline&)>;

/**
 * @brief K-line aggregator for real-time candlestick generation
 *
 * Aggregates trade data into K-lines for multiple timeframes simultaneously.
 * Thread-safe for concurrent access.
 */
class KlineAggregator final {
public:
  /**
   * @brief Configuration for the aggregator
   */
  struct Config {
    size_t max_history_per_interval{1000}; ///< Max candles to keep per interval
    bool emit_on_update{true};             ///< Emit callback on every update
    bool emit_on_close{true};              ///< Emit callback when candle closes
  };

  KlineAggregator();
  explicit KlineAggregator(Config config);
  ~KlineAggregator() = default;

  // Non-copyable, non-movable
  KlineAggregator(const KlineAggregator&) = delete;
  KlineAggregator& operator=(const KlineAggregator&) = delete;

  /**
   * @brief Enable aggregation for a specific interval
   * @param interval The timeframe to enable
   */
  void enable_interval(KlineInterval interval);

  /**
   * @brief Disable aggregation for a specific interval
   * @param interval The timeframe to disable
   */
  void disable_interval(KlineInterval interval);

  /**
   * @brief Check if an interval is enabled
   */
  [[nodiscard]] bool is_interval_enabled(KlineInterval interval) const;

  /**
   * @brief Get list of enabled intervals
   */
  [[nodiscard]] kj::Vector<KlineInterval> enabled_intervals() const;

  /**
   * @brief Process a trade event
   * @param trade The trade data to aggregate
   * @param timestamp_ms Trade timestamp in milliseconds
   */
  void process_trade(const TradeData& trade, int64_t timestamp_ms);

  /**
   * @brief Process a trade event from MarketEvent
   * @param event The market event (must be Trade type)
   */
  void process_event(const MarketEvent& event);

  /**
   * @brief Get current (potentially incomplete) K-line for an interval
   * @param interval The timeframe
   * @return Current K-line or nullptr if interval not enabled
   */
  [[nodiscard]] kj::Maybe<AggregatedKline> current_kline(KlineInterval interval) const;

  /**
   * @brief Get historical K-lines for an interval
   * @param interval The timeframe
   * @param count Maximum number of candles to return (0 = all)
   * @return Vector of historical K-lines (newest first)
   */
  [[nodiscard]] kj::Vector<AggregatedKline> history(KlineInterval interval, size_t count = 0) const;

  /**
   * @brief Get K-lines within a time range
   * @param interval The timeframe
   * @param start_ms Start timestamp (inclusive)
   * @param end_ms End timestamp (inclusive)
   * @return Vector of K-lines in the range
   */
  [[nodiscard]] kj::Vector<AggregatedKline> range(KlineInterval interval, int64_t start_ms,
                                                  int64_t end_ms) const;

  /**
   * @brief Set callback for K-line updates
   * @param callback Function to call on K-line updates
   */
  void set_callback(KlineCallback callback);

  /**
   * @brief Clear callback
   */
  void clear_callback();

  /**
   * @brief Clear all data for an interval
   * @param interval The timeframe to clear
   */
  void clear(KlineInterval interval);

  /**
   * @brief Clear all data for all intervals
   */
  void clear_all();

  /**
   * @brief Get statistics
   */
  [[nodiscard]] int64_t total_trades_processed() const {
    return total_trades_;
  }
  [[nodiscard]] int64_t total_candles_closed() const {
    return total_candles_closed_;
  }

private:
  /**
   * @brief Internal state for a single interval
   */
  struct IntervalState {
    bool enabled{false};
    kj::Maybe<AggregatedKline> current;
    kj::Vector<AggregatedKline> history;
  };

  /**
   * @brief Calculate the start time of the candle containing a timestamp
   */
  [[nodiscard]] int64_t align_to_interval(int64_t timestamp_ms, KlineInterval interval) const;

  /**
   * @brief Close current candle and start a new one
   */
  void close_candle(IntervalState& state, KlineInterval interval, int64_t new_start_ms);

  /**
   * @brief Update current candle with trade data
   */
  void update_candle(AggregatedKline& candle, const TradeData& trade);

  /**
   * @brief Emit callback if configured
   */
  void emit_update(KlineInterval interval, const AggregatedKline& kline, bool is_close);

  Config config_;

  // State for each interval (indexed by enum value)
  IntervalState states_[7]; // One for each KlineInterval

  // Callback for updates
  kj::Maybe<KlineCallback> callback_;

  // Statistics
  int64_t total_trades_{0};
  int64_t total_candles_closed_{0};
};

} // namespace veloz::market
