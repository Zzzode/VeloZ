#include "veloz/engine/command_parser.h"

#include <sstream>

namespace veloz::engine {

std::optional<ParsedOrder> parse_order_command(std::string_view line) {
  std::istringstream iss{std::string(line)};
  std::string verb;
  iss >> verb;
  if (verb != "ORDER") {
    return std::nullopt;
  }

  std::string side_s;
  std::string symbol;
  double qty = 0.0;
  double price = 0.0;
  std::string client_id;

  iss >> side_s >> symbol >> qty >> price >> client_id;
  if (side_s.empty() || symbol.empty() || qty <= 0.0 || price <= 0.0 || client_id.empty()) {
    return std::nullopt;
  }

  ParsedOrder out;
  out.request.symbol.value = symbol;
  out.request.side = (side_s == "SELL" || side_s == "Sell" || side_s == "sell")
                         ? veloz::exec::OrderSide::Sell
                         : veloz::exec::OrderSide::Buy;
  out.request.type = veloz::exec::OrderType::Limit;
  out.request.tif = veloz::exec::TimeInForce::GTC;
  out.request.qty = qty;
  out.request.price = price;
  out.request.client_order_id = client_id;
  return out;
}

std::optional<ParsedCancel> parse_cancel_command(std::string_view line) {
  std::istringstream iss{std::string(line)};
  std::string verb;
  iss >> verb;
  if (verb != "CANCEL") {
    return std::nullopt;
  }
  std::string client_id;
  iss >> client_id;
  if (client_id.empty()) {
    return std::nullopt;
  }
  return ParsedCancel{.client_order_id = client_id};
}

}
