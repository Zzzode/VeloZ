#include "veloz/oms/order_record.h"

namespace veloz::oms {

namespace {
[[nodiscard]] kj::StringPtr side_to_string(veloz::exec::OrderSide side) {
  return (side == veloz::exec::OrderSide::Sell) ? "SELL" : "BUY";
}

[[nodiscard]] bool is_terminal_status(veloz::exec::OrderStatus status) {
  return status == veloz::exec::OrderStatus::Filled ||
         status == veloz::exec::OrderStatus::Canceled ||
         status == veloz::exec::OrderStatus::Rejected ||
         status == veloz::exec::OrderStatus::Expired;
}
} // namespace

OrderRecord::OrderRecord(veloz::exec::PlaceOrderRequest request)
    : request_(kj::mv(request)),
      last_update_ts_(std::chrono::duration_cast<std::chrono::nanoseconds>(
                          std::chrono::system_clock::now().time_since_epoch())
                          .count()) {}

const veloz::exec::PlaceOrderRequest& OrderRecord::request() const {
  return request_;
}

kj::StringPtr OrderRecord::venue_order_id() const {
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

std::int64_t OrderRecord::last_update_ts() const {
  return last_update_ts_;
}

bool OrderRecord::is_terminal() const {
  return is_terminal_status(status_);
}

void OrderRecord::apply(const veloz::exec::ExecutionReport& report) {
  if (report.venue_order_id.size() > 0) {
    venue_order_id_ = kj::heapString(report.venue_order_id);
  }
  if (report.last_fill_qty > 0.0) {
    const double new_cum = cum_qty_ + report.last_fill_qty;
    const double notional =
        (avg_price_ * cum_qty_) + (report.last_fill_price * report.last_fill_qty);
    cum_qty_ = new_cum;
    avg_price_ = (new_cum > 0.0) ? (notional / new_cum) : 0.0;
  }
  status_ = report.status;
  last_update_ts_ = report.ts_recv_ns;
}

void OrderStore::note_order_params(const veloz::exec::PlaceOrderRequest& request) {
  if (request.client_order_id.size() == 0) {
    return;
  }
  auto lock = guarded_.lockExclusive();
  std::string key(request.client_order_id.cStr());
  auto& st = lock->orders[key];
  st.client_order_id = kj::heapString(request.client_order_id);
  if (!request.symbol.value.empty()) {
    st.symbol = kj::heapString(request.symbol.value);
  }
  st.side = kj::heapString(side_to_string(request.side));
  if (request.qty > 0.0) {
    st.order_qty = request.qty;
  }
  KJ_IF_MAYBE (price, request.price) {
    if (*price > 0.0) {
      st.limit_price = *price;
    }
  }
  // Set creation timestamp if not already set
  if (st.created_ts_ns == 0) {
    st.created_ts_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
                           std::chrono::system_clock::now().time_since_epoch())
                           .count();
  }
}

void OrderStore::apply_order_update(kj::StringPtr client_order_id, kj::StringPtr symbol,
                                    kj::StringPtr side, kj::StringPtr venue_order_id,
                                    kj::StringPtr status, kj::StringPtr reason,
                                    std::int64_t ts_ns) {
  if (client_order_id.size() == 0) {
    return;
  }
  auto lock = guarded_.lockExclusive();
  std::string key(client_order_id.cStr());
  auto& st = lock->orders[key];
  st.client_order_id = kj::heapString(client_order_id);
  if (symbol.size() > 0) {
    st.symbol = kj::heapString(symbol);
  }
  if (side.size() > 0) {
    st.side = kj::heapString(side);
  }
  if (venue_order_id.size() > 0) {
    st.venue_order_id = kj::heapString(venue_order_id);
  }
  if (status.size() > 0) {
    st.status = kj::heapString(status);
  }
  if (reason.size() > 0) {
    st.reason = kj::heapString(reason);
  }
  if (ts_ns > 0) {
    st.last_ts_ns = ts_ns;
  }
}

void OrderStore::apply_fill(kj::StringPtr client_order_id, kj::StringPtr symbol, double qty,
                            double price, std::int64_t ts_ns) {
  if (client_order_id.size() == 0 || qty <= 0.0) {
    return;
  }
  auto lock = guarded_.lockExclusive();
  std::string key(client_order_id.cStr());
  auto& st = lock->orders[key];
  st.client_order_id = kj::heapString(client_order_id);
  if (symbol.size() > 0) {
    st.symbol = kj::heapString(symbol);
  }
  const double new_cum = st.executed_qty + qty;
  const double notional = (st.avg_price * st.executed_qty) + (price * qty);
  st.executed_qty = new_cum;
  st.avg_price = (new_cum > 0.0) ? (notional / new_cum) : 0.0;
  if (ts_ns > 0) {
    st.last_ts_ns = ts_ns;
  }
  KJ_IF_MAYBE (order_qty, st.order_qty) {
    if (*order_qty > 0.0) {
      if (st.executed_qty + 1e-12 >= *order_qty) {
        if (st.status != "CANCELED"_kj && st.status != "REJECTED"_kj && st.status != "EXPIRED"_kj) {
          st.status = kj::heapString("FILLED");
        }
      } else if (st.executed_qty > 0.0) {
        if (st.status != "CANCELED"_kj && st.status != "REJECTED"_kj && st.status != "EXPIRED"_kj) {
          st.status = kj::heapString("PARTIALLY_FILLED");
        }
      }
    }
  }
}

kj::Maybe<OrderState> OrderStore::get(kj::StringPtr client_order_id) const {
  if (client_order_id.size() == 0) {
    return nullptr;
  }
  auto lock = guarded_.lockExclusive();
  std::string key(client_order_id.cStr());
  auto it = lock->orders.find(key);
  if (it == lock->orders.end()) {
    return nullptr;
  }
  // Copy the OrderState since we're returning by value
  OrderState result;
  result.client_order_id = kj::heapString(it->second.client_order_id);
  result.symbol = kj::heapString(it->second.symbol);
  result.side = kj::heapString(it->second.side);
  result.order_qty = it->second.order_qty;
  result.limit_price = it->second.limit_price;
  result.executed_qty = it->second.executed_qty;
  result.avg_price = it->second.avg_price;
  result.venue_order_id = kj::heapString(it->second.venue_order_id);
  result.status = kj::heapString(it->second.status);
  result.reason = kj::heapString(it->second.reason);
  result.last_ts_ns = it->second.last_ts_ns;
  result.created_ts_ns = it->second.created_ts_ns;
  return result;
}

kj::Vector<OrderState> OrderStore::list_pending() const {
  auto lock = guarded_.lockExclusive();
  kj::Vector<OrderState> out;
  for (const auto& [_, st] : lock->orders) {
    if (st.status != "FILLED"_kj && st.status != "CANCELED"_kj && st.status != "REJECTED"_kj &&
        st.status != "EXPIRED"_kj) {
      OrderState copy;
      copy.client_order_id = kj::heapString(st.client_order_id);
      copy.symbol = kj::heapString(st.symbol);
      copy.side = kj::heapString(st.side);
      copy.order_qty = st.order_qty;
      copy.limit_price = st.limit_price;
      copy.executed_qty = st.executed_qty;
      copy.avg_price = st.avg_price;
      copy.venue_order_id = kj::heapString(st.venue_order_id);
      copy.status = kj::heapString(st.status);
      copy.reason = kj::heapString(st.reason);
      copy.last_ts_ns = st.last_ts_ns;
      copy.created_ts_ns = st.created_ts_ns;
      out.add(kj::mv(copy));
    }
  }
  return out;
}

kj::Vector<OrderState> OrderStore::list_terminal() const {
  auto lock = guarded_.lockExclusive();
  kj::Vector<OrderState> out;
  for (const auto& [_, st] : lock->orders) {
    if (st.status == "FILLED"_kj || st.status == "CANCELED"_kj || st.status == "REJECTED"_kj ||
        st.status == "EXPIRED"_kj) {
      OrderState copy;
      copy.client_order_id = kj::heapString(st.client_order_id);
      copy.symbol = kj::heapString(st.symbol);
      copy.side = kj::heapString(st.side);
      copy.order_qty = st.order_qty;
      copy.limit_price = st.limit_price;
      copy.executed_qty = st.executed_qty;
      copy.avg_price = st.avg_price;
      copy.venue_order_id = kj::heapString(st.venue_order_id);
      copy.status = kj::heapString(st.status);
      copy.reason = kj::heapString(st.reason);
      copy.last_ts_ns = st.last_ts_ns;
      copy.created_ts_ns = st.created_ts_ns;
      out.add(kj::mv(copy));
    }
  }
  return out;
}

void OrderStore::apply_execution_report(const veloz::exec::ExecutionReport& report) {
  if (report.client_order_id.size() == 0) {
    return;
  }
  auto lock = guarded_.lockExclusive();
  std::string key(report.client_order_id.cStr());
  auto& st = lock->orders[key];
  st.client_order_id = kj::heapString(report.client_order_id);
  st.symbol = kj::heapString(report.symbol.value);
  if (report.last_fill_qty > 0.0) {
    const double new_cum = st.executed_qty + report.last_fill_qty;
    const double notional =
        (st.avg_price * st.executed_qty) + (report.last_fill_price * report.last_fill_qty);
    st.executed_qty = new_cum;
    st.avg_price = (new_cum > 0.0) ? (notional / new_cum) : 0.0;
  }
  if (report.venue_order_id.size() > 0) {
    st.venue_order_id = kj::heapString(report.venue_order_id);
  }
  st.last_ts_ns = report.ts_recv_ns;
}

size_t OrderStore::count() const {
  auto lock = guarded_.lockExclusive();
  return lock->orders.size();
}

size_t OrderStore::count_pending() const {
  auto lock = guarded_.lockExclusive();
  size_t count = 0;
  for (const auto& [_, st] : lock->orders) {
    if (st.status != "FILLED"_kj && st.status != "CANCELED"_kj && st.status != "REJECTED"_kj &&
        st.status != "EXPIRED"_kj) {
      count++;
    }
  }
  return count;
}

size_t OrderStore::count_terminal() const {
  auto lock = guarded_.lockExclusive();
  size_t count = 0;
  for (const auto& [_, st] : lock->orders) {
    if (st.status == "FILLED"_kj || st.status == "CANCELED"_kj || st.status == "REJECTED"_kj ||
        st.status == "EXPIRED"_kj) {
      count++;
    }
  }
  return count;
}

void OrderStore::clear() {
  auto lock = guarded_.lockExclusive();
  lock->orders.clear();
}

kj::Vector<OrderState> OrderStore::list() const {
  auto lock = guarded_.lockExclusive();
  kj::Vector<OrderState> out;
  for (const auto& [_, st] : lock->orders) {
    OrderState copy;
    copy.client_order_id = kj::heapString(st.client_order_id);
    copy.symbol = kj::heapString(st.symbol);
    copy.side = kj::heapString(st.side);
    copy.order_qty = st.order_qty;
    copy.limit_price = st.limit_price;
    copy.executed_qty = st.executed_qty;
    copy.avg_price = st.avg_price;
    copy.venue_order_id = kj::heapString(st.venue_order_id);
    copy.status = kj::heapString(st.status);
    copy.reason = kj::heapString(st.reason);
    copy.last_ts_ns = st.last_ts_ns;
    copy.created_ts_ns = st.created_ts_ns;
    out.add(kj::mv(copy));
  }
  return out;
}

} // namespace veloz::oms
