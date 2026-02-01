#pragma once

#include "veloz/exec/order_api.h"

#include <string>

namespace veloz::oms {

class OrderRecord final {
public:
  explicit OrderRecord(veloz::exec::PlaceOrderRequest request);

  [[nodiscard]] const veloz::exec::PlaceOrderRequest& request() const;
  [[nodiscard]] const std::string& venue_order_id() const;
  [[nodiscard]] veloz::exec::OrderStatus status() const;
  [[nodiscard]] double cum_qty() const;
  [[nodiscard]] double avg_price() const;

  void apply(const veloz::exec::ExecutionReport& report);

private:
  veloz::exec::PlaceOrderRequest request_;
  std::string venue_order_id_;
  veloz::exec::OrderStatus status_{veloz::exec::OrderStatus::New};
  double cum_qty_{0.0};
  double avg_price_{0.0};
};

} // namespace veloz::oms
