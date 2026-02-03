#include "veloz/engine/event_emitter.h"

#include <sstream>

namespace veloz::engine {

namespace {

std::string escape_json(std::string_view s) {
  std::string out;
  out.reserve(s.size() + 8);
  for (char c : s) {
    switch (c) {
    case '\\':
      out += "\\\\";
      break;
    case '"':
      out += "\\\"";
      break;
    case '\n':
      out += "\\n";
      break;
    case '\r':
      out += "\\r";
      break;
    case '\t':
      out += "\\t";
      break;
    default:
      out.push_back(c);
      break;
    }
  }
  return out;
}

std::string order_state_json(const veloz::oms::OrderState& st) {
  std::ostringstream oss;
  oss << "{\"type\":\"order_state\",\"client_order_id\":\"" << escape_json(st.client_order_id)
      << "\"";
  if (!st.status.empty()) {
    oss << ",\"status\":\"" << escape_json(st.status) << "\"";
  }
  if (!st.symbol.empty()) {
    oss << ",\"symbol\":\"" << escape_json(st.symbol) << "\"";
  }
  if (!st.side.empty()) {
    oss << ",\"side\":\"" << escape_json(st.side) << "\"";
  }
  if (st.order_qty.has_value()) {
    oss << ",\"order_qty\":" << st.order_qty.value();
  }
  if (st.limit_price.has_value()) {
    oss << ",\"limit_price\":" << st.limit_price.value();
  }
  oss << ",\"executed_qty\":" << st.executed_qty << ",\"avg_price\":" << st.avg_price;
  if (!st.venue_order_id.empty()) {
    oss << ",\"venue_order_id\":\"" << escape_json(st.venue_order_id) << "\"";
  }
  if (!st.reason.empty()) {
    oss << ",\"reason\":\"" << escape_json(st.reason) << "\"";
  }
  if (st.last_ts_ns > 0) {
    oss << ",\"last_ts_ns\":" << st.last_ts_ns;
  }
  oss << "}";
  return oss.str();
}

} 

EventEmitter::EventEmitter(std::ostream& out) : out_(out) {}

void EventEmitter::emit_line(std::string_view json_line) {
  std::lock_guard<std::mutex> lk(mu_);
  out_ << json_line << '\n' << std::flush;
}

void EventEmitter::emit_market(std::string_view symbol, double price, std::int64_t ts_ns) {
  std::ostringstream oss;
  oss << "{\"type\":\"market\",\"symbol\":\"" << escape_json(symbol) << "\",\"ts_ns\":" << ts_ns
      << ",\"price\":" << price << "}";
  emit_line(oss.str());
}

void EventEmitter::emit_fill(std::string_view client_order_id, std::string_view symbol, double qty,
                             double price, std::int64_t ts_ns) {
  std::ostringstream oss;
  oss << "{\"type\":\"fill\",\"ts_ns\":" << ts_ns << ",\"client_order_id\":\""
      << escape_json(client_order_id) << "\",\"symbol\":\"" << escape_json(symbol)
      << "\",\"qty\":" << qty << ",\"price\":" << price << "}";
  emit_line(oss.str());
}

void EventEmitter::emit_order_update(std::string_view client_order_id, std::string_view status,
                                     std::string_view symbol, std::string_view side,
                                     std::optional<double> qty, std::optional<double> price,
                                     std::string_view venue_order_id, std::string_view reason,
                                     std::int64_t ts_ns) {
  std::ostringstream oss;
  oss << "{\"type\":\"order_update\",\"ts_ns\":" << ts_ns << ",\"client_order_id\":\""
      << escape_json(client_order_id) << "\"";
  if (!venue_order_id.empty()) {
    oss << ",\"venue_order_id\":\"" << escape_json(venue_order_id) << "\"";
  }
  if (!status.empty()) {
    oss << ",\"status\":\"" << escape_json(status) << "\"";
  }
  if (!symbol.empty()) {
    oss << ",\"symbol\":\"" << escape_json(symbol) << "\"";
  }
  if (!side.empty()) {
    oss << ",\"side\":\"" << escape_json(side) << "\"";
  }
  if (qty.has_value()) {
    oss << ",\"qty\":" << qty.value();
  }
  if (price.has_value()) {
    oss << ",\"price\":" << price.value();
  }
  if (!reason.empty()) {
    oss << ",\"reason\":\"" << escape_json(reason) << "\"";
  }
  oss << "}";
  emit_line(oss.str());
}

void EventEmitter::emit_order_state(const veloz::oms::OrderState& state) {
  emit_line(order_state_json(state));
}

void EventEmitter::emit_account(std::int64_t ts_ns, const std::vector<Balance>& balances) {
  std::ostringstream oss;
  oss << "{\"type\":\"account\",\"ts_ns\":" << ts_ns << ",\"balances\":[";
  for (std::size_t i = 0; i < balances.size(); ++i) {
    if (i > 0) {
      oss << ",";
    }
    const auto& b = balances[i];
    oss << "{\"asset\":\"" << escape_json(b.asset) << "\",\"free\":" << b.free
        << ",\"locked\":" << b.locked << "}";
  }
  oss << "]}";
  emit_line(oss.str());
}

void EventEmitter::emit_error(std::string_view message, std::int64_t ts_ns) {
  std::ostringstream oss;
  oss << "{\"type\":\"error\",\"ts_ns\":" << ts_ns << ",\"message\":\"" << escape_json(message)
      << "\"}";
  emit_line(oss.str());
}

}
