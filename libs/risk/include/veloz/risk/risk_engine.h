#pragma once

#include "veloz/exec/order_api.h"

#include <string>

namespace veloz::risk {

struct RiskResult final {
  bool ok{false};
  std::string reason;
};

class RiskEngine final {
public:
  [[nodiscard]] RiskResult check(const veloz::exec::PlaceOrderRequest& request) const;
};

} // namespace veloz::risk
