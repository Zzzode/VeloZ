#include "veloz/engine/event_emitter.h"

#include <kj/common.h>
#include <kj/debug.h>
#include <kj/string.h>
#include <kj/vector.h>
#include <sstream>

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
  std::ostringstream oss;
  oss << "{\"type\":\"order_state\",\"client_order_id\":\""
      << escape_json(st.client_order_id).cStr() << "\"";
  if (st.status.size() > 0) {
    oss << ",\"status\":\"" << escape_json(st.status).cStr() << "\"";
  }
  if (st.symbol.size() > 0) {
    oss << ",\"symbol\":\"" << escape_json(st.symbol).cStr() << "\"";
  }
  if (st.side.size() > 0) {
    oss << ",\"side\":\"" << escape_json(st.side).cStr() << "\"";
  }
  KJ_IF_MAYBE (order_qty, st.order_qty) {
    oss << ",\"order_qty\":" << *order_qty;
  }
  KJ_IF_MAYBE (limit_price, st.limit_price) {
    oss << ",\"limit_price\":" << *limit_price;
  }
  oss << ",\"executed_qty\":" << st.executed_qty << ",\"avg_price\":" << st.avg_price;
  if (st.venue_order_id.size() > 0) {
    oss << ",\"venue_order_id\":\"" << escape_json(st.venue_order_id).cStr() << "\"";
  }
  if (st.reason.size() > 0) {
    oss << ",\"reason\":\"" << escape_json(st.reason).cStr() << "\"";
  }
  if (st.last_ts_ns > 0) {
    oss << ",\"last_ts_ns\":" << st.last_ts_ns;
  }
  oss << "}";
  return kj::str(oss.str());
}

} // namespace

EventEmitter::EventEmitter(std::ostream& out) : out_(out), mu_(0) {}

void EventEmitter::emit_line(kj::StringPtr json_line) {
  auto lock = mu_.lockExclusive();
  out_ << json_line.cStr() << '\n' << std::flush;
}

void EventEmitter::emit_market(kj::StringPtr symbol, double price, std::int64_t ts_ns) {
  std::ostringstream oss;
  oss << "{\"type\":\"market\",\"symbol\":\"" << escape_json(symbol).cStr()
      << "\",\"ts_ns\":" << ts_ns << ",\"price\":" << price << "}";
  emit_line(kj::StringPtr(oss.str().c_str()));
}

void EventEmitter::emit_fill(kj::StringPtr client_order_id, kj::StringPtr symbol, double qty,
                             double price, std::int64_t ts_ns) {
  std::ostringstream oss;
  oss << "{\"type\":\"fill\",\"ts_ns\":" << ts_ns << ",\"client_order_id\":\""
      << escape_json(client_order_id).cStr() << "\",\"symbol\":\"" << escape_json(symbol).cStr()
      << "\",\"qty\":" << qty << ",\"price\":" << price << "}";
  emit_line(kj::StringPtr(oss.str().c_str()));
}

void EventEmitter::emit_order_update(kj::StringPtr client_order_id, kj::StringPtr status,
                                     kj::StringPtr symbol, kj::StringPtr side,
                                     kj::Maybe<double> qty, kj::Maybe<double> price,
                                     kj::StringPtr venue_order_id, kj::StringPtr reason,
                                     std::int64_t ts_ns) {
  std::ostringstream oss;
  oss << "{\"type\":\"order_update\",\"ts_ns\":" << ts_ns << ",\"client_order_id\":\""
      << escape_json(client_order_id).cStr() << "\"";
  if (venue_order_id.size() > 0) {
    oss << ",\"venue_order_id\":\"" << escape_json(venue_order_id).cStr() << "\"";
  }
  if (status.size() > 0) {
    oss << ",\"status\":\"" << escape_json(status).cStr() << "\"";
  }
  if (symbol.size() > 0) {
    oss << ",\"symbol\":\"" << escape_json(symbol).cStr() << "\"";
  }
  if (side.size() > 0) {
    oss << ",\"side\":\"" << escape_json(side).cStr() << "\"";
  }
  KJ_IF_MAYBE (q, qty) {
    oss << ",\"qty\":" << *q;
  }
  KJ_IF_MAYBE (p, price) {
    oss << ",\"price\":" << *p;
  }
  if (reason.size() > 0) {
    oss << ",\"reason\":\"" << escape_json(reason).cStr() << "\"";
  }
  oss << "}";
  emit_line(kj::StringPtr(oss.str().c_str()));
}

void EventEmitter::emit_order_state(const veloz::oms::OrderState& state) {
  emit_line(order_state_json(state));
}

void EventEmitter::emit_account(std::int64_t ts_ns, const kj::Vector<Balance>& balances) {
  std::ostringstream oss;
  oss << "{\"type\":\"account\",\"ts_ns\":" << ts_ns << ",\"balances\":[";
  for (size_t i = 0; i < balances.size(); ++i) {
    if (i > 0) {
      oss << ",";
    }
    const auto& b = balances[i];
    oss << "{\"asset\":\"" << escape_json(b.asset).cStr() << "\",\"free\":" << b.free
        << ",\"locked\":" << b.locked << "}";
  }
  oss << "]}";
  emit_line(kj::StringPtr(oss.str().c_str()));
}

void EventEmitter::emit_error(kj::StringPtr message, std::int64_t ts_ns) {
  std::ostringstream oss;
  oss << "{\"type\":\"error\",\"ts_ns\":" << ts_ns << ",\"message\":\""
      << escape_json(message).cStr() << "\"}";
  emit_line(kj::StringPtr(oss.str().c_str()));
}

} // namespace veloz::engine
