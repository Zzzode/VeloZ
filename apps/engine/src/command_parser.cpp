#include "veloz/engine/command_parser.h"

#include <sstream>
#include <algorithm>
#include <cctype>

namespace veloz::engine {

// Helper functions to convert strings to lowercase for case-insensitive comparison
static std::string to_lower(std::string_view s) {
  std::string result;
  result.reserve(s.size());
  for (char c : s) {
    result += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  }
  return result;
}

// Helper to trim whitespace from both ends
static std::string_view trim(std::string_view s) {
  auto start = s.find_first_not_of(" \t\n\r");
  if (start == std::string_view::npos) return "";
  auto end = s.find_last_not_of(" \t\n\r");
  return s.substr(start, end - start + 1);
}

// Check if string is a valid order side
bool is_valid_order_side(std::string_view side) {
  std::string lower = to_lower(side);
  return lower == "buy" || lower == "sell" || lower == "b" || lower == "s";
}

// Parse order side string to enum
veloz::exec::OrderSide parse_order_side(std::string_view side) {
  std::string lower = to_lower(side);
  if (lower == "sell" || lower == "s") {
    return veloz::exec::OrderSide::Sell;
  }
  return veloz::exec::OrderSide::Buy;
}

// Check if string is a valid order type
bool is_valid_order_type(std::string_view type) {
  std::string lower = to_lower(type);
  return lower == "limit" || lower == "market" || lower == "l" || lower == "m";
}

// Parse order type string to enum
veloz::exec::OrderType parse_order_type(std::string_view type) {
  std::string lower = to_lower(type);
  if (lower == "market" || lower == "m") {
    return veloz::exec::OrderType::Market;
  }
  return veloz::exec::OrderType::Limit;
}

// Check if string is a valid time-in-force
bool is_valid_tif(std::string_view tif) {
  std::string lower = to_lower(tif);
  return lower == "gtc" || lower == "ioc" || lower == "fok" || lower == "gtx" || lower == "g";
}

// Parse TIF string to enum
veloz::exec::TimeInForce parse_tif(std::string_view tif) {
  std::string lower = to_lower(tif);
  if (lower == "ioc") {
    return veloz::exec::TimeInForce::IOC;
  } else if (lower == "fok") {
    return veloz::exec::TimeInForce::FOK;
  } else if (lower == "gtx") {
    return veloz::exec::TimeInForce::GTX;
  }
  return veloz::exec::TimeInForce::GTC;
}

// Parse a command and determine its type
ParsedCommand parse_command(std::string_view line) {
  ParsedCommand result;
  result.type = CommandType::Unknown;
  std::string trimmed_line = std::string(trim(line));

  // Skip empty lines and comments
  if (trimmed_line.empty() || trimmed_line[0] == '#') {
    result.type = CommandType::Unknown;
    return result;
  }

  std::istringstream iss(trimmed_line);
  std::string verb;
  iss >> verb;

  std::string verb_lower = to_lower(verb);

  // Check for ORDER command (supports: ORDER, BUY, SELL)
  if (verb_lower == "order" || verb_lower == "buy" || verb_lower == "sell" ||
      verb_lower == "b" || verb_lower == "s") {
    auto order = parse_order_command(trimmed_line);
    if (order) {
      result.type = CommandType::Order;
      result.order = order;
    } else {
      result.error = "Failed to parse ORDER command";
    }
  }
  // Check for CANCEL command
  else if (verb_lower == "cancel" || verb_lower == "c") {
    auto cancel = parse_cancel_command(trimmed_line);
    if (cancel) {
      result.type = CommandType::Cancel;
      result.cancel = cancel;
    } else {
      result.error = "Failed to parse CANCEL command";
    }
  }
  // Check for QUERY command
  else if (verb_lower == "query" || verb_lower == "q") {
    auto query = parse_query_command(trimmed_line);
    if (query) {
      result.type = CommandType::Query;
      result.query = query;
    } else {
      result.error = "Failed to parse QUERY command";
    }
  }
  else {
    result.error = "Unknown command: " + verb;
  }

  return result;
}

// Parse ORDER command
// Formats:
//   ORDER <SIDE> <SYMBOL> <QTY> <PRICE> <CLIENT_ID> [TYPE] [TIF]
//   BUY <SYMBOL> <QTY> <PRICE> <CLIENT_ID> [TYPE] [TIF]
//   SELL <SYMBOL> <QTY> <PRICE> <CLIENT_ID> [TYPE] [TIF]
std::optional<ParsedOrder> parse_order_command(std::string_view line) {
  std::istringstream iss{std::string(line)};
  std::string verb;
  iss >> verb;

  std::string verb_lower = to_lower(verb);

  // Handle implicit BUY/SELL commands
  std::string side_s;
  if (verb_lower == "buy" || verb_lower == "b") {
    side_s = "buy";
  } else if (verb_lower == "sell" || verb_lower == "s") {
    side_s = "sell";
  } else {
    // Explicit ORDER command - read side next
    iss >> side_s;
    if (!is_valid_order_side(side_s)) {
      return std::nullopt;
    }
  }

  std::string symbol;
  double qty = 0.0;
  double price = 0.0;
  std::string client_id;
  std::string type_str = "limit";
  std::string tif_str = "gtc";

  // Parse required parameters
  iss >> symbol >> qty >> price >> client_id;

  // Try to parse optional parameters
  std::string token;
  if (iss >> token) type_str = token;
  if (iss >> token) tif_str = token;

  // Validate required parameters
  if (symbol.empty() || qty <= 0.0 || price <= 0.0 || client_id.empty()) {
    return std::nullopt;
  }

  ParsedOrder out;
  out.raw_command = std::string(line);
  out.request.symbol.value = symbol;
  out.request.side = parse_order_side(side_s);

  // Parse order type
  if (is_valid_order_type(type_str)) {
    out.request.type = parse_order_type(type_str);
  } else {
    out.request.type = veloz::exec::OrderType::Limit;
  }

  // Parse TIF
  if (is_valid_tif(tif_str)) {
    out.request.tif = parse_tif(tif_str);
  } else {
    out.request.tif = veloz::exec::TimeInForce::GTC;
  }

  out.request.qty = qty;
  out.request.price = price;
  out.request.client_order_id = client_id;

  return out;
}

// Parse CANCEL command
// Formats:
//   CANCEL <CLIENT_ID>
//   C <CLIENT_ID>
std::optional<ParsedCancel> parse_cancel_command(std::string_view line) {
  std::istringstream iss{std::string(line)};
  std::string verb;
  iss >> verb;

  std::string verb_lower = to_lower(verb);
  if (verb_lower != "cancel" && verb_lower != "c") {
    return std::nullopt;
  }

  std::string client_id;
  iss >> client_id;
  if (client_id.empty()) {
    return std::nullopt;
  }

  return ParsedCancel{.client_order_id = client_id, .raw_command = std::string(line)};
}

// Parse QUERY command
// Formats:
//   QUERY <TYPE> [PARAMS]
//   Q <TYPE> [PARAMS]
std::optional<ParsedQuery> parse_query_command(std::string_view line) {
  std::istringstream iss{std::string(line)};
  std::string verb;
  iss >> verb;

  std::string verb_lower = to_lower(verb);
  if (verb_lower != "query" && verb_lower != "q") {
    return std::nullopt;
  }

  std::string query_type;
  std::string params;

  iss >> query_type;
  if (query_type.empty()) {
    return std::nullopt;
  }

  // Read remaining parameters
  std::string token;
  while (iss >> token) {
    if (!params.empty()) params += " ";
    params += token;
  }

  return ParsedQuery{.query_type = query_type, .params = params, .raw_command = std::string(line)};
}

}
