#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace veloz::backtest {

// Backtest configuration
struct BacktestConfig {
  std::string strategy_name;
  std::string symbol;
  std::int64_t start_time;
  std::int64_t end_time;
  double initial_balance;
  double risk_per_trade;
  double max_position_size;
  std::map<std::string, double> strategy_parameters;
  std::string data_source;
  std::string data_type;  // "kline", "trade", "book"
  std::string time_frame; // "1m", "5m", "1h", etc.
};

// Trade record
struct TradeRecord {
  std::int64_t timestamp;
  std::string symbol;
  std::string side; // "buy" or "sell"
  double price;
  double quantity;
  double fee;
  double pnl;
  std::string strategy_id;
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
  std::string strategy_name;
  std::string symbol;
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
  std::vector<TradeRecord> trades;
  std::vector<EquityCurvePoint> equity_curve;
  std::vector<DrawdownPoint> drawdown_curve;
};

} // namespace veloz::backtest
