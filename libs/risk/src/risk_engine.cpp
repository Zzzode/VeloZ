#include "veloz/risk/risk_engine.h"

namespace veloz::risk {

RiskResult RiskEngine::check(const veloz::exec::PlaceOrderRequest& request) const {
  if (request.qty <= 0.0) {
    return {.ok = false, .reason = "qty must be > 0"};
  }

  if (request.type == veloz::exec::OrderType::Limit) {
    if (!request.price.has_value() || request.price.value() <= 0.0) {
      return {.ok = false, .reason = "limit order requires price > 0"};
    }
  }

  if (request.client_order_id.empty()) {
    return {.ok = false, .reason = "client_order_id is required"};
  }

  return {.ok = true, .reason = ""};
}

} // namespace veloz::risk
