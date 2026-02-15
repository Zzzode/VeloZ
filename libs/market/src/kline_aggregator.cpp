#include "veloz/market/kline_aggregator.h"

#include <kj/debug.h>

namespace veloz::market {

kj::StringPtr interval_to_string(KlineInterval interval) {
  switch (interval) {
  case KlineInterval::Min1:
    return "1m"_kj;
  case KlineInterval::Min5:
    return "5m"_kj;
  case KlineInterval::Min15:
    return "15m"_kj;
  case KlineInterval::Min30:
    return "30m"_kj;
  case KlineInterval::Hour1:
    return "1h"_kj;
  case KlineInterval::Hour4:
    return "4h"_kj;
  case KlineInterval::Day1:
    return "1d"_kj;
  }
  return "unknown"_kj;
}

KlineAggregator::KlineAggregator() : KlineAggregator(Config{}) {}

KlineAggregator::KlineAggregator(Config config) : config_(config) {
  // Initialize all interval states as disabled
  for (auto& state : states_) {
    state.enabled = false;
  }
}

void KlineAggregator::enable_interval(KlineInterval interval) {
  auto idx = static_cast<size_t>(interval);
  KJ_REQUIRE(idx < 7, "Invalid interval index");
  states_[idx].enabled = true;
}

void KlineAggregator::disable_interval(KlineInterval interval) {
  auto idx = static_cast<size_t>(interval);
  KJ_REQUIRE(idx < 7, "Invalid interval index");
  states_[idx].enabled = false;
  states_[idx].current = kj::none;
  states_[idx].history.clear();
}

bool KlineAggregator::is_interval_enabled(KlineInterval interval) const {
  auto idx = static_cast<size_t>(interval);
  if (idx >= 7)
    return false;
  return states_[idx].enabled;
}

kj::Vector<KlineInterval> KlineAggregator::enabled_intervals() const {
  kj::Vector<KlineInterval> result;
  for (size_t i = 0; i < 7; ++i) {
    if (states_[i].enabled) {
      result.add(static_cast<KlineInterval>(i));
    }
  }
  return result;
}

void KlineAggregator::process_trade(const TradeData& trade, int64_t timestamp_ms) {
  total_trades_++;

  for (size_t i = 0; i < 7; ++i) {
    auto& state = states_[i];
    if (!state.enabled)
      continue;

    auto interval = static_cast<KlineInterval>(i);
    int64_t interval_ms = interval_to_ms(interval);
    int64_t candle_start = align_to_interval(timestamp_ms, interval);

    KJ_IF_SOME(current, state.current) {
      // Check if we need to close the current candle
      if (candle_start > current.kline.start_time) {
        // Close current candle and start new one
        close_candle(state, interval, candle_start);
      }
      // Update current candle
      KJ_IF_SOME(updated, state.current) {
        update_candle(updated, trade);
        updated.kline.close_time = candle_start + interval_ms - 1;
        emit_update(interval, updated, false);
      }
    } else {
      // Start first candle
      AggregatedKline new_candle;
      new_candle.kline.start_time = candle_start;
      new_candle.kline.close_time = candle_start + interval_ms - 1;
      new_candle.kline.open = trade.price;
      new_candle.kline.high = trade.price;
      new_candle.kline.low = trade.price;
      new_candle.kline.close = trade.price;
      new_candle.kline.volume = trade.qty;
      new_candle.vwap = trade.price;
      new_candle.trade_count = 1;
      if (trade.is_buyer_maker) {
        new_candle.sell_volume = trade.qty;
      } else {
        new_candle.buy_volume = trade.qty;
      }
      new_candle.is_closed = false;

      state.current = kj::mv(new_candle);
      KJ_IF_SOME(c, state.current) {
        emit_update(interval, c, false);
      }
    }
  }
}

void KlineAggregator::process_event(const MarketEvent& event) {
  if (event.type != MarketEventType::Trade) {
    return;
  }

  // Process TradeData from kj::OneOf
  if (event.data.is<TradeData>()) {
    const auto& trade = event.data.get<TradeData>();
    // Convert nanoseconds to milliseconds
    int64_t timestamp_ms = event.ts_exchange_ns / 1000000;
    process_trade(trade, timestamp_ms);
  }
}

kj::Maybe<AggregatedKline> KlineAggregator::current_kline(KlineInterval interval) const {
  auto idx = static_cast<size_t>(interval);
  if (idx >= 7 || !states_[idx].enabled) {
    return kj::none;
  }

  KJ_IF_SOME(current, states_[idx].current) {
    return current;
  }
  return kj::none;
}

kj::Vector<AggregatedKline> KlineAggregator::history(KlineInterval interval, size_t count) const {
  kj::Vector<AggregatedKline> result;
  auto idx = static_cast<size_t>(interval);
  if (idx >= 7 || !states_[idx].enabled) {
    return result;
  }

  const auto& hist = states_[idx].history;
  size_t to_copy = (count == 0 || count > hist.size()) ? hist.size() : count;

  // History is stored oldest first, return newest first
  for (size_t i = 0; i < to_copy; ++i) {
    result.add(hist[hist.size() - 1 - i]);
  }
  return result;
}

kj::Vector<AggregatedKline> KlineAggregator::range(KlineInterval interval, int64_t start_ms,
                                                   int64_t end_ms) const {
  kj::Vector<AggregatedKline> result;
  auto idx = static_cast<size_t>(interval);
  if (idx >= 7 || !states_[idx].enabled) {
    return result;
  }

  const auto& hist = states_[idx].history;
  for (const auto& kline : hist) {
    if (kline.kline.start_time >= start_ms && kline.kline.start_time <= end_ms) {
      result.add(kline);
    }
  }

  // Also include current candle if in range
  KJ_IF_SOME(current, states_[idx].current) {
    if (current.kline.start_time >= start_ms && current.kline.start_time <= end_ms) {
      result.add(current);
    }
  }

  return result;
}

void KlineAggregator::set_callback(KlineCallback callback) {
  callback_ = kj::mv(callback);
}

void KlineAggregator::clear_callback() {
  callback_ = kj::none;
}

void KlineAggregator::clear(KlineInterval interval) {
  auto idx = static_cast<size_t>(interval);
  if (idx >= 7)
    return;
  states_[idx].current = kj::none;
  states_[idx].history.clear();
}

void KlineAggregator::clear_all() {
  for (auto& state : states_) {
    state.current = kj::none;
    state.history.clear();
  }
  total_trades_ = 0;
  total_candles_closed_ = 0;
}

int64_t KlineAggregator::align_to_interval(int64_t timestamp_ms, KlineInterval interval) const {
  int64_t interval_ms = interval_to_ms(interval);
  return (timestamp_ms / interval_ms) * interval_ms;
}

void KlineAggregator::close_candle(IntervalState& state, KlineInterval interval,
                                   int64_t new_start_ms) {
  KJ_IF_SOME(current, state.current) {
    current.is_closed = true;
    emit_update(interval, current, true);

    // Add to history
    state.history.add(kj::mv(current));
    total_candles_closed_++;

    // Trim history if needed
    while (state.history.size() > config_.max_history_per_interval) {
      // Remove oldest (first element)
      kj::Vector<AggregatedKline> new_history;
      for (size_t i = 1; i < state.history.size(); ++i) {
        new_history.add(kj::mv(state.history[i]));
      }
      state.history = kj::mv(new_history);
    }
  }

  // Start new candle
  int64_t interval_ms = interval_to_ms(interval);
  AggregatedKline new_candle;
  new_candle.kline.start_time = new_start_ms;
  new_candle.kline.close_time = new_start_ms + interval_ms - 1;
  new_candle.is_closed = false;
  state.current = kj::mv(new_candle);
}

void KlineAggregator::update_candle(AggregatedKline& candle, const TradeData& trade) {
  // Update OHLC
  if (candle.trade_count == 0) {
    candle.kline.open = trade.price;
    candle.kline.high = trade.price;
    candle.kline.low = trade.price;
  } else {
    if (trade.price > candle.kline.high) {
      candle.kline.high = trade.price;
    }
    if (trade.price < candle.kline.low) {
      candle.kline.low = trade.price;
    }
  }
  candle.kline.close = trade.price;

  // Update volume
  candle.kline.volume += trade.qty;
  candle.trade_count++;

  // Update buy/sell volume
  if (trade.is_buyer_maker) {
    candle.sell_volume += trade.qty;
  } else {
    candle.buy_volume += trade.qty;
  }

  // Update VWAP
  double total_value = candle.vwap * (candle.kline.volume - trade.qty) + trade.price * trade.qty;
  candle.vwap = total_value / candle.kline.volume;
}

void KlineAggregator::emit_update(KlineInterval interval, const AggregatedKline& kline,
                                  bool is_close) {
  KJ_IF_SOME(cb, callback_) {
    if ((is_close && config_.emit_on_close) || (!is_close && config_.emit_on_update)) {
      cb(interval, kline);
    }
  }
}

} // namespace veloz::market
