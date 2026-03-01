#include "veloz/engine/event_emitter.h"

#include <kj/common.h>
#include <kj/debug.h>
#include <kj/io.h>
#include <kj/string.h>
#include <kj/vector.h>

namespace veloz::engine {

namespace {

kj::String escape_json(kj::StringPtr s) {
  kj::Vector<char> out;
  out.reserve(s.size() + 8);
  for (char c : s) {
    switch (c) {
    case '\\':
      out.add('\\');
      out.add('\\');
      break;
    case '"':
      out.add('\\');
      out.add('"');
      break;
    case '\n':
      out.add('\\');
      out.add('n');
      break;
    case '\r':
      out.add('\\');
      out.add('r');
      break;
    case '\t':
      out.add('\\');
      out.add('t');
      break;
    default:
      out.add(c);
      break;
    }
  }
  out.add('\0');
  return kj::String(out.releaseAsArray());
}

kj::String order_state_json(const veloz::oms::OrderState& st) {
  kj::Vector<kj::String> parts;
  parts.add(kj::str("{\"type\":\"order_state\",\"client_order_id\":\"",
                    escape_json(st.client_order_id), "\""));
  if (st.status.size() > 0) {
    parts.add(kj::str(",\"status\":\"", escape_json(st.status), "\""));
  }
  if (st.symbol.size() > 0) {
    parts.add(kj::str(",\"symbol\":\"", escape_json(st.symbol), "\""));
  }
  if (st.side.size() > 0) {
    parts.add(kj::str(",\"side\":\"", escape_json(st.side), "\""));
  }
  KJ_IF_SOME(order_qty, st.order_qty) {
    parts.add(kj::str(",\"order_qty\":", order_qty));
  }
  KJ_IF_SOME(limit_price, st.limit_price) {
    parts.add(kj::str(",\"limit_price\":", limit_price));
  }
  parts.add(kj::str(",\"executed_qty\":", st.executed_qty, ",\"avg_price\":", st.avg_price));
  if (st.venue_order_id.size() > 0) {
    parts.add(kj::str(",\"venue_order_id\":\"", escape_json(st.venue_order_id), "\""));
  }
  if (st.reason.size() > 0) {
    parts.add(kj::str(",\"reason\":\"", escape_json(st.reason), "\""));
  }
  if (st.last_ts_ns > 0) {
    parts.add(kj::str(",\"last_ts_ns\":", st.last_ts_ns));
  }
  parts.add(kj::str("}"));

  // Concatenate all parts
  size_t total_len = 0;
  for (auto& p : parts) {
    total_len += p.size();
  }
  kj::Vector<char> result;
  result.reserve(total_len + 1);
  for (auto& p : parts) {
    for (char c : p) {
      result.add(c);
    }
  }
  result.add('\0');
  return kj::String(result.releaseAsArray());
}

} // namespace

EventEmitter::EventEmitter(kj::OutputStream& out) : out_(out), mu_(0) {}

void EventEmitter::emit_line(kj::StringPtr json_line) {
  auto lock = mu_.lockExclusive();
  kj::String line = kj::str(json_line, "\n");
  out_.write(line.asBytes());
}

void EventEmitter::emit_market(kj::StringPtr symbol, double price, std::int64_t ts_ns) {
  kj::String json = kj::str("{\"type\":\"market\",\"symbol\":\"", escape_json(symbol),
                            "\",\"ts_ns\":", ts_ns, ",\"price\":", price, "}");
  emit_line(json);
}

void EventEmitter::emit_fill(kj::StringPtr client_order_id, kj::StringPtr symbol, double qty,
                             double price, std::int64_t ts_ns) {
  kj::String json = kj::str("{\"type\":\"fill\",\"ts_ns\":", ts_ns, ",\"client_order_id\":\"",
                            escape_json(client_order_id), "\",\"symbol\":\"", escape_json(symbol),
                            "\",\"qty\":", qty, ",\"price\":", price, "}");
  emit_line(json);
}

void EventEmitter::emit_order_update(kj::StringPtr client_order_id, kj::StringPtr status,
                                     kj::StringPtr symbol, kj::StringPtr side,
                                     kj::Maybe<double> qty, kj::Maybe<double> price,
                                     kj::StringPtr venue_order_id, kj::StringPtr reason,
                                     std::int64_t ts_ns) {
  kj::Vector<kj::String> parts;
  parts.add(kj::str("{\"type\":\"order_update\",\"ts_ns\":", ts_ns, ",\"client_order_id\":\"",
                    escape_json(client_order_id), "\""));
  if (venue_order_id.size() > 0) {
    parts.add(kj::str(",\"venue_order_id\":\"", escape_json(venue_order_id), "\""));
  }
  if (status.size() > 0) {
    parts.add(kj::str(",\"status\":\"", escape_json(status), "\""));
  }
  if (symbol.size() > 0) {
    parts.add(kj::str(",\"symbol\":\"", escape_json(symbol), "\""));
  }
  if (side.size() > 0) {
    parts.add(kj::str(",\"side\":\"", escape_json(side), "\""));
  }
  KJ_IF_SOME(q, qty) {
    parts.add(kj::str(",\"qty\":", q));
  }
  KJ_IF_SOME(p, price) {
    parts.add(kj::str(",\"price\":", p));
  }
  if (reason.size() > 0) {
    parts.add(kj::str(",\"reason\":\"", escape_json(reason), "\""));
  }
  parts.add(kj::str("}"));

  // Concatenate all parts
  size_t total_len = 0;
  for (auto& p : parts) {
    total_len += p.size();
  }
  kj::Vector<char> result;
  result.reserve(total_len + 1);
  for (auto& p : parts) {
    for (char c : p) {
      result.add(c);
    }
  }
  result.add('\0');
  emit_line(kj::String(result.releaseAsArray()));
}

void EventEmitter::emit_order_state(const veloz::oms::OrderState& state) {
  emit_line(order_state_json(state));
}

void EventEmitter::emit_account(std::int64_t ts_ns, const kj::Vector<Balance>& balances) {
  kj::Vector<kj::String> parts;
  parts.add(kj::str("{\"type\":\"account\",\"ts_ns\":", ts_ns, ",\"balances\":["));
  for (size_t i = 0; i < balances.size(); ++i) {
    if (i > 0) {
      parts.add(kj::str(","));
    }
    const auto& b = balances[i];
    parts.add(kj::str("{\"asset\":\"", escape_json(b.asset), "\",\"free\":", b.free,
                      ",\"locked\":", b.locked, "}"));
  }
  parts.add(kj::str("]}"));

  // Concatenate all parts
  size_t total_len = 0;
  for (auto& p : parts) {
    total_len += p.size();
  }
  kj::Vector<char> result;
  result.reserve(total_len + 1);
  for (auto& p : parts) {
    for (char c : p) {
      result.add(c);
    }
  }
  result.add('\0');
  emit_line(kj::String(result.releaseAsArray()));
}

void EventEmitter::emit_error(kj::StringPtr message, std::int64_t ts_ns) {
  kj::String json = kj::str("{\"type\":\"error\",\"ts_ns\":", ts_ns, ",\"message\":\"",
                            escape_json(message), "\"}");
  emit_line(json);
}

void EventEmitter::emit_market_event(const veloz::market::MarketEvent& event) {
  // Convert venue enum to string
  kj::StringPtr venue_str = "unknown"_kj;
  switch (event.venue) {
  case veloz::common::Venue::Binance:
    venue_str = "binance"_kj;
    break;
  case veloz::common::Venue::Okx:
    venue_str = "okx"_kj;
    break;
  case veloz::common::Venue::Bybit:
    venue_str = "bybit"_kj;
    break;
  default:
    break;
  }

  // Dispatch based on event type and data
  if (event.data.is<veloz::market::TradeData>()) {
    const auto& trade = event.data.get<veloz::market::TradeData>();
    emit_trade(event.symbol.value, venue_str, trade.price, trade.qty, trade.is_buyer_maker,
               trade.trade_id, event.ts_exchange_ns);
  } else if (event.data.is<veloz::market::BookData>()) {
    const auto& book = event.data.get<veloz::market::BookData>();
    if (event.type == veloz::market::MarketEventType::BookTop && book.bids.size() > 0 &&
        book.asks.size() > 0) {
      emit_book_top(event.symbol.value, venue_str, book.bids[0].price, book.bids[0].qty,
                    book.asks[0].price, book.asks[0].qty, event.ts_exchange_ns);
    } else if (event.type == veloz::market::MarketEventType::BookDelta) {
      emit_book_delta(event.symbol.value, venue_str, book.bids, book.asks, book.sequence,
                      event.ts_exchange_ns);
    }
  } else if (event.data.is<veloz::market::KlineData>()) {
    const auto& kline = event.data.get<veloz::market::KlineData>();
    emit_kline(event.symbol.value, venue_str, kline.open, kline.high, kline.low, kline.close,
               kline.volume, kline.start_time, kline.close_time, event.ts_exchange_ns);
  }
}

void EventEmitter::emit_trade(kj::StringPtr symbol, kj::StringPtr venue, double price, double qty,
                              bool is_buyer_maker, std::int64_t trade_id, std::int64_t ts_ns) {
  kj::String json =
      kj::str("{\"type\":\"trade\",\"symbol\":\"", escape_json(symbol), "\",\"venue\":\"",
              escape_json(venue), "\",\"price\":", price, ",\"qty\":", qty,
              ",\"is_buyer_maker\":", is_buyer_maker ? "true" : "false", ",\"trade_id\":", trade_id,
              ",\"ts_ns\":", ts_ns, "}");
  emit_line(json);
}

void EventEmitter::emit_book_top(kj::StringPtr symbol, kj::StringPtr venue, double bid_price,
                                 double bid_qty, double ask_price, double ask_qty,
                                 std::int64_t ts_ns) {
  kj::String json =
      kj::str("{\"type\":\"book_top\",\"symbol\":\"", escape_json(symbol), "\",\"venue\":\"",
              escape_json(venue), "\",\"bid_price\":", bid_price, ",\"bid_qty\":", bid_qty,
              ",\"ask_price\":", ask_price, ",\"ask_qty\":", ask_qty, ",\"ts_ns\":", ts_ns, "}");
  emit_line(json);
}

void EventEmitter::emit_book_delta(kj::StringPtr symbol, kj::StringPtr venue,
                                   const kj::Vector<veloz::market::BookLevel>& bids,
                                   const kj::Vector<veloz::market::BookLevel>& asks,
                                   std::int64_t sequence, std::int64_t ts_ns) {
  kj::Vector<kj::String> parts;
  parts.add(kj::str("{\"type\":\"book_delta\",\"symbol\":\"", escape_json(symbol),
                    "\",\"venue\":\"", escape_json(venue), "\",\"sequence\":", sequence,
                    ",\"bids\":["));

  for (size_t i = 0; i < bids.size(); ++i) {
    if (i > 0) {
      parts.add(kj::str(","));
    }
    parts.add(kj::str("[", bids[i].price, ",", bids[i].qty, "]"));
  }
  parts.add(kj::str("],\"asks\":["));

  for (size_t i = 0; i < asks.size(); ++i) {
    if (i > 0) {
      parts.add(kj::str(","));
    }
    parts.add(kj::str("[", asks[i].price, ",", asks[i].qty, "]"));
  }
  parts.add(kj::str("],\"ts_ns\":", ts_ns, "}"));

  // Concatenate all parts
  size_t total_len = 0;
  for (auto& p : parts) {
    total_len += p.size();
  }
  kj::Vector<char> result;
  result.reserve(total_len + 1);
  for (auto& p : parts) {
    for (char c : p) {
      result.add(c);
    }
  }
  result.add('\0');
  emit_line(kj::String(result.releaseAsArray()));
}

void EventEmitter::emit_kline(kj::StringPtr symbol, kj::StringPtr venue, double open, double high,
                              double low, double close, double volume, std::int64_t start_time,
                              std::int64_t close_time, std::int64_t ts_ns) {
  kj::String json =
      kj::str("{\"type\":\"kline\",\"symbol\":\"", escape_json(symbol), "\",\"venue\":\"",
              escape_json(venue), "\",\"open\":", open, ",\"high\":", high, ",\"low\":", low,
              ",\"close\":", close, ",\"volume\":", volume, ",\"start_time\":", start_time,
              ",\"close_time\":", close_time, ",\"ts_ns\":", ts_ns, "}");
  emit_line(json);
}

void EventEmitter::emit_subscription_status(kj::StringPtr symbol, kj::StringPtr event_type,
                                            kj::StringPtr status, std::int64_t ts_ns) {
  kj::String json = kj::str("{\"type\":\"subscription_status\",\"symbol\":\"", escape_json(symbol),
                            "\",\"event_type\":\"", escape_json(event_type), "\",\"status\":\"",
                            escape_json(status), "\",\"ts_ns\":", ts_ns, "}");
  emit_line(json);
}

} // namespace veloz::engine
