#pragma once

#include "veloz/exec/order_api.h"

#include <kj/common.h>
#include <kj/string.h>

namespace veloz::engine {

// Command types
enum class CommandType { Order, Cancel, Query, Unknown };

struct ParsedOrder final {
  veloz::exec::PlaceOrderRequest request;
  kj::String raw_command;

  ParsedOrder() : raw_command(kj::str("")) {}
};

struct ParsedCancel final {
  kj::String client_order_id;
  kj::String raw_command;

  ParsedCancel() : client_order_id(kj::str("")), raw_command(kj::str("")) {}
};

struct ParsedQuery final {
  kj::String query_type;
  kj::String params;
  kj::String raw_command;

  ParsedQuery() : query_type(kj::str("")), params(kj::str("")), raw_command(kj::str("")) {}
};

struct ParsedCommand final {
  CommandType type;
  kj::Maybe<ParsedOrder> order;
  kj::Maybe<ParsedCancel> cancel;
  kj::Maybe<ParsedQuery> query;
  kj::String error;

  ParsedCommand() : type(CommandType::Unknown), error(kj::str("")) {}
};

[[nodiscard]] ParsedCommand parse_command(kj::StringPtr line);
[[nodiscard]] kj::Maybe<ParsedOrder> parse_order_command(kj::StringPtr line);
[[nodiscard]] kj::Maybe<ParsedCancel> parse_cancel_command(kj::StringPtr line);
[[nodiscard]] kj::Maybe<ParsedQuery> parse_query_command(kj::StringPtr line);

// Helper functions
[[nodiscard]] bool is_valid_order_side(kj::StringPtr side);
[[nodiscard]] veloz::exec::OrderSide parse_order_side(kj::StringPtr side);
[[nodiscard]] bool is_valid_order_type(kj::StringPtr type);
[[nodiscard]] veloz::exec::OrderType parse_order_type(kj::StringPtr type);
[[nodiscard]] bool is_valid_tif(kj::StringPtr tif);
[[nodiscard]] veloz::exec::TimeInForce parse_tif(kj::StringPtr tif);

} // namespace veloz::engine
