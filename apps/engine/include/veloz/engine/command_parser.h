#pragma once

#include "veloz/exec/order_api.h"

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace veloz::engine {

// Command types
enum class CommandType { Order, Cancel, Query, Unknown };

struct ParsedOrder final {
  veloz::exec::PlaceOrderRequest request;
  std::string raw_command;
};

struct ParsedCancel final {
  std::string client_order_id;
  std::string raw_command;
};

struct ParsedQuery final {
  std::string query_type;
  std::string params;
  std::string raw_command;
};

struct ParsedCommand final {
  CommandType type;
  std::optional<ParsedOrder> order;
  std::optional<ParsedCancel> cancel;
  std::optional<ParsedQuery> query;
  std::string error;
};

[[nodiscard]] ParsedCommand parse_command(std::string_view line);
[[nodiscard]] std::optional<ParsedOrder> parse_order_command(std::string_view line);
[[nodiscard]] std::optional<ParsedCancel> parse_cancel_command(std::string_view line);
[[nodiscard]] std::optional<ParsedQuery> parse_query_command(std::string_view line);

// Helper functions
[[nodiscard]] bool is_valid_order_side(std::string_view side);
[[nodiscard]] veloz::exec::OrderSide parse_order_side(std::string_view side);
[[nodiscard]] bool is_valid_order_type(std::string_view type);
[[nodiscard]] veloz::exec::OrderType parse_order_type(std::string_view type);
[[nodiscard]] bool is_valid_tif(std::string_view tif);
[[nodiscard]] veloz::exec::TimeInForce parse_tif(std::string_view tif);

} // namespace veloz::engine
