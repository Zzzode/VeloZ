#pragma once

#include <cstdint>
#include <kj/common.h>
#include <kj/map.h>
#include <kj/string.h>
#include <kj/vector.h>

namespace veloz::backtest {

// Backtest configuration
struct BacktestConfig {
  kj::String strategy_name;
  kj::String symbol;
  std::int64_t start_time;
  std::int64_t end_time;
  double initial_balance;
  double risk_per_trade;
  double max_position_size;
  kj::TreeMap<kj::String, double> strategy_parameters;
  kj::String data_source;
  kj::String data_type;  // "kline", "trade", "book"
  kj::String time_frame; // "1m", "5m", "1h", etc.

  BacktestConfig()
      : strategy_name(kj::str("")), symbol(kj::str("")), start_time(0), end_time(0),
        initial_balance(0.0), risk_per_trade(0.0), max_position_size(0.0), data_source(kj::str("")),
        data_type(kj::str("")), time_frame(kj::str("")) {}
};

// Trade record
struct TradeRecord {
  std::int64_t timestamp;
  kj::String symbol;
  kj::String side; // "buy" or "sell"
  double price;
  double quantity;
  double fee;
  double pnl;
  kj::String strategy_id;

  TradeRecord()
      : timestamp(0), symbol(kj::str("")), side(kj::str("")), price(0.0), quantity(0.0), fee(0.0),
        pnl(0.0), strategy_id(kj::str("")) {}
};

// Equity curve point
struct EquityCurvePoint {
  std::int64_t timestamp;
  double equity;
  double cumulative_return;
};

// Drawdown curve point
struct DrawdownPoint {
  std::int64_t timestamp;
  double drawdown;
};

// Backtest result
struct BacktestResult {
  kj::String strategy_name;
  kj::String symbol;
  std::int64_t start_time;
  std::int64_t end_time;
  double initial_balance;
  double final_balance;
  double total_return;
  double max_drawdown;
  double sharpe_ratio;
  double win_rate;
  double profit_factor;
  int trade_count;
  int win_count;
  int lose_count;
  double avg_win;
  double avg_lose;
  kj::Vector<TradeRecord> trades;
  kj::Vector<EquityCurvePoint> equity_curve;
  kj::Vector<DrawdownPoint> drawdown_curve;

  BacktestResult()
      : strategy_name(kj::str("")), symbol(kj::str("")), start_time(0), end_time(0),
        initial_balance(0.0), final_balance(0.0), total_return(0.0), max_drawdown(0.0),
        sharpe_ratio(0.0), win_rate(0.0), profit_factor(0.0), trade_count(0), win_count(0),
        lose_count(0), avg_win(0.0), avg_lose(0.0) {}
};

} // namespace veloz::backtest
