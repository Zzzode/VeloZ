#pragma once

#include "veloz/exec/order_api.h"

#include <optional>
#include <string>
#include <string_view>

namespace veloz::engine {

struct ParsedOrder final {
  veloz::exec::PlaceOrderRequest request;
};

struct ParsedCancel final {
  std::string client_order_id;
};

[[nodiscard]] std::optional<ParsedOrder> parse_order_command(std::string_view line);
[[nodiscard]] std::optional<ParsedCancel> parse_cancel_command(std::string_view line);

}
