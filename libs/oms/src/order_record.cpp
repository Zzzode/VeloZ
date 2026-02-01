#include "veloz/oms/order_record.h"

namespace veloz::oms {

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

} // namespace veloz::oms
