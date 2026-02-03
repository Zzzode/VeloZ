#include "veloz/oms/order_record.h"

namespace veloz::oms {

namespace {

[[nodiscard]] std::string_view side_to_string(veloz::exec::OrderSide side) {
  return (side == veloz::exec::OrderSide::Sell) ? "SELL" : "BUY";
}

} // namespace

OrderRecord::OrderRecord(veloz::exec::PlaceOrderRequest request) : request_(std::move(request)) {}

const veloz::exec::PlaceOrderRequest& OrderRecord::request() const {
  return request_;
}

const std::string& OrderRecord::venue_order_id() const {
  return venue_order_id_;
}

veloz::exec::OrderStatus OrderRecord::status() const {
  return status_;
}

double OrderRecord::cum_qty() const {
  return cum_qty_;
}

double OrderRecord::avg_price() const {
  return avg_price_;
}

void OrderRecord::apply(const veloz::exec::ExecutionReport& report) {
  if (!report.venue_order_id.empty()) {
    venue_order_id_ = report.venue_order_id;
  }

  if (report.last_fill_qty > 0.0) {
    const double new_cum = cum_qty_ + report.last_fill_qty;
    const double notional =
        (avg_price_ * cum_qty_) + (report.last_fill_price * report.last_fill_qty);
    cum_qty_ = new_cum;
    avg_price_ = (new_cum > 0.0) ? (notional / new_cum) : 0.0;
  }

  status_ = report.status;
}

void OrderStore::note_order_params(const veloz::exec::PlaceOrderRequest& request) {
  if (request.client_order_id.empty()) {
    return;
  }

  std::lock_guard<std::mutex> lk(mu_);
  auto& st = orders_[request.client_order_id];
  st.client_order_id = request.client_order_id;
  if (!request.symbol.value.empty()) {
    st.symbol = request.symbol.value;
  }
  st.side = std::string(side_to_string(request.side));
  if (request.qty > 0.0) {
    st.order_qty = request.qty;
  }
  if (request.price.has_value() && request.price.value() > 0.0) {
    st.limit_price = request.price.value();
  }
}

void OrderStore::apply_order_update(std::string_view client_order_id, std::string_view symbol,
                                    std::string_view side, std::string_view venue_order_id,
                                    std::string_view status, std::string_view reason,
                                    std::int64_t ts_ns) {
  if (client_order_id.empty()) {
    return;
  }

  std::lock_guard<std::mutex> lk(mu_);
  auto& st = orders_[std::string(client_order_id)];
  st.client_order_id = std::string(client_order_id);
  if (!symbol.empty()) {
    st.symbol = std::string(symbol);
  }
  if (!side.empty()) {
    st.side = std::string(side);
  }
  if (!venue_order_id.empty()) {
    st.venue_order_id = std::string(venue_order_id);
  }
  if (!status.empty()) {
    st.status = std::string(status);
  }
  if (!reason.empty()) {
    st.reason = std::string(reason);
  }
  if (ts_ns > 0) {
    st.last_ts_ns = ts_ns;
  }
}

void OrderStore::apply_fill(std::string_view client_order_id, std::string_view symbol, double qty,
                            double price, std::int64_t ts_ns) {
  if (client_order_id.empty() || qty <= 0.0) {
    return;
  }

  std::lock_guard<std::mutex> lk(mu_);
  auto& st = orders_[std::string(client_order_id)];
  st.client_order_id = std::string(client_order_id);
  if (!symbol.empty()) {
    st.symbol = std::string(symbol);
  }
  const double new_cum = st.executed_qty + qty;
  const double notional = (st.avg_price * st.executed_qty) + (price * qty);
  st.executed_qty = new_cum;
  st.avg_price = (new_cum > 0.0) ? (notional / new_cum) : 0.0;
  if (ts_ns > 0) {
    st.last_ts_ns = ts_ns;
  }
  if (st.order_qty.has_value() && st.order_qty.value() > 0.0) {
    if (st.executed_qty + 1e-12 >= st.order_qty.value()) {
      if (st.status != "CANCELLED" && st.status != "REJECTED" && st.status != "EXPIRED") {
        st.status = "FILLED";
      }
    } else if (st.executed_qty > 0.0) {
      if (st.status != "CANCELLED" && st.status != "REJECTED" && st.status != "EXPIRED") {
        st.status = "PARTIALLY_FILLED";
      }
    }
  }
}

std::optional<OrderState> OrderStore::get(std::string_view client_order_id) const {
  if (client_order_id.empty()) {
    return std::nullopt;
  }
  std::lock_guard<std::mutex> lk(mu_);
  auto it = orders_.find(std::string(client_order_id));
  if (it == orders_.end()) {
    return std::nullopt;
  }
  return it->second;
}

std::vector<OrderState> OrderStore::list() const {
  std::lock_guard<std::mutex> lk(mu_);
  std::vector<OrderState> out;
  out.reserve(orders_.size());
  for (const auto& [_, st] : orders_) {
    out.push_back(st);
  }
  return out;
}

} // namespace veloz::oms
