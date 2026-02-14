#include "veloz/engine/command_parser.h"

#include <algorithm>
#include <cctype>
#include <kj/common.h>
#include <kj/debug.h>
#include <kj/string.h>
#include <kj/vector.h>
#include <sstream>

namespace veloz::engine {

// Helper functions to convert strings to lowercase for case-insensitive comparison
static kj::String to_lower(kj::StringPtr s) {
  kj::Vector<char> result;
  result.reserve(s.size());
  for (char c : s) {
    result.add(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
  }
  result.add('\0');
  return kj::String(result.releaseAsArray());
}

// Helper to trim whitespace from both ends
static kj::String trim(kj::StringPtr s) {
  size_t start = 0;
  while (start < s.size() && std::isspace(static_cast<unsigned char>(s[start]))) {
    ++start;
  }
  if (start >= s.size()) {
    return kj::str("");
  }
  size_t end = s.size();
  while (end > start && std::isspace(static_cast<unsigned char>(s[end - 1]))) {
    --end;
  }
  return kj::str(s.slice(start, end));
}

// Check if string is a valid order side
bool is_valid_order_side(kj::StringPtr side) {
  kj::String lower = to_lower(side);
  return lower == "buy"_kj || lower == "sell"_kj || lower == "b"_kj || lower == "s"_kj;
}

// Parse order side string to enum
veloz::exec::OrderSide parse_order_side(kj::StringPtr side) {
  kj::String lower = to_lower(side);
  if (lower == "sell"_kj || lower == "s"_kj) {
    return veloz::exec::OrderSide::Sell;
  }
  return veloz::exec::OrderSide::Buy;
}

// Check if string is a valid order type
bool is_valid_order_type(kj::StringPtr type) {
  kj::String lower = to_lower(type);
  return lower == "limit"_kj || lower == "market"_kj || lower == "l"_kj || lower == "m"_kj;
}

// Parse order type string to enum
veloz::exec::OrderType parse_order_type(kj::StringPtr type) {
  kj::String lower = to_lower(type);
  if (lower == "market"_kj || lower == "m"_kj) {
    return veloz::exec::OrderType::Market;
  }
  return veloz::exec::OrderType::Limit;
}

// Check if string is a valid time-in-force
bool is_valid_tif(kj::StringPtr tif) {
  kj::String lower = to_lower(tif);
  return lower == "gtc"_kj || lower == "ioc"_kj || lower == "fok"_kj || lower == "gtx"_kj ||
         lower == "g"_kj;
}

// Parse TIF string to enum
veloz::exec::TimeInForce parse_tif(kj::StringPtr tif) {
  kj::String lower = to_lower(tif);
  if (lower == "ioc"_kj) {
    return veloz::exec::TimeInForce::IOC;
  } else if (lower == "fok"_kj) {
    return veloz::exec::TimeInForce::FOK;
  } else if (lower == "gtx"_kj) {
    return veloz::exec::TimeInForce::GTX;
  }
  return veloz::exec::TimeInForce::GTC;
}

// Parse a command and determine its type
ParsedCommand parse_command(kj::StringPtr line) {
  ParsedCommand result;
  result.type = CommandType::Unknown;
  kj::String trimmed_line = trim(line);

  // Skip empty lines and comments
  if (trimmed_line.size() == 0 || trimmed_line[0] == '#') {
    result.type = CommandType::Unknown;
    return result;
  }

  // std::istringstream used for tokenizing input (no KJ equivalent for stream parsing)
  std::istringstream iss(trimmed_line.cStr());
  std::string verb;
  iss >> verb;

  kj::String verb_lower = to_lower(kj::StringPtr(verb.c_str()));

  // Check for ORDER command (supports: ORDER, BUY, SELL)
  if (verb_lower == "order"_kj || verb_lower == "buy"_kj || verb_lower == "sell"_kj ||
      verb_lower == "b"_kj || verb_lower == "s"_kj) {
    auto order = parse_order_command(trimmed_line);
    KJ_IF_MAYBE (o, order) {
      result.type = CommandType::Order;
      result.order = kj::mv(*o);
    } else {
      result.error = kj::str("Failed to parse ORDER command");
    }
  }
  // Check for CANCEL command
  else if (verb_lower == "cancel"_kj || verb_lower == "c"_kj) {
    auto cancel = parse_cancel_command(trimmed_line);
    KJ_IF_MAYBE (c, cancel) {
      result.type = CommandType::Cancel;
      result.cancel = kj::mv(*c);
    } else {
      result.error = kj::str("Failed to parse CANCEL command");
    }
  }
  // Check for QUERY command
  else if (verb_lower == "query"_kj || verb_lower == "q"_kj) {
    auto query = parse_query_command(trimmed_line);
    KJ_IF_MAYBE (q, query) {
      result.type = CommandType::Query;
      result.query = kj::mv(*q);
    } else {
      result.error = kj::str("Failed to parse QUERY command");
    }
  } else {
    result.error = kj::str("Unknown command: ", verb);
  }

  return result;
}

// Parse ORDER command
// Formats:
//   ORDER <SIDE> <SYMBOL> <QTY> <PRICE> <CLIENT_ID> [TYPE] [TIF]
//   BUY <SYMBOL> <QTY> <PRICE> <CLIENT_ID> [TYPE] [TIF]
//   SELL <SYMBOL> <QTY> <PRICE> <CLIENT_ID> [TYPE] [TIF]
kj::Maybe<ParsedOrder> parse_order_command(kj::StringPtr line) {
  std::istringstream iss{std::string(line.cStr())};
  std::string verb;
  iss >> verb;

  kj::String verb_lower = to_lower(kj::StringPtr(verb.c_str()));

  // Handle implicit BUY/SELL commands
  std::string side_s;
  if (verb_lower == "buy"_kj || verb_lower == "b"_kj) {
    side_s = "buy";
  } else if (verb_lower == "sell"_kj || verb_lower == "s"_kj) {
    side_s = "sell";
  } else {
    // Explicit ORDER command - read side next
    iss >> side_s;
    if (!is_valid_order_side(kj::StringPtr(side_s.c_str()))) {
      return kj::Maybe<ParsedOrder>();
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
  if (iss >> token)
    type_str = token;
  if (iss >> token)
    tif_str = token;

  // Validate required parameters
  // Note: price can be 0 for market orders
  bool is_market_order =
      is_valid_order_type(kj::StringPtr(type_str.c_str())) &&
      parse_order_type(kj::StringPtr(type_str.c_str())) == veloz::exec::OrderType::Market;
  if (symbol.empty() || qty <= 0.0 || client_id.empty()) {
    return kj::Maybe<ParsedOrder>();
  }
  // For limit orders, price must be positive
  if (!is_market_order && price <= 0.0) {
    return kj::Maybe<ParsedOrder>();
  }

  ParsedOrder out;
  out.raw_command = kj::str(line);
  out.request.symbol.value = symbol;
  out.request.side = parse_order_side(kj::StringPtr(side_s.c_str()));

  // Parse order type
  if (is_valid_order_type(kj::StringPtr(type_str.c_str()))) {
    out.request.type = parse_order_type(kj::StringPtr(type_str.c_str()));
  } else {
    out.request.type = veloz::exec::OrderType::Limit;
  }

  // Parse TIF
  if (is_valid_tif(kj::StringPtr(tif_str.c_str()))) {
    out.request.tif = parse_tif(kj::StringPtr(tif_str.c_str()));
  } else {
    out.request.tif = veloz::exec::TimeInForce::GTC;
  }

  out.request.qty = qty;
  out.request.price = price;
  out.request.client_order_id = kj::str(client_id);

  return out;
}

// Parse CANCEL command
// Formats:
//   CANCEL <CLIENT_ID>
//   C <CLIENT_ID>
kj::Maybe<ParsedCancel> parse_cancel_command(kj::StringPtr line) {
  std::istringstream iss{std::string(line.cStr())};
  std::string verb;
  iss >> verb;

  kj::String verb_lower = to_lower(kj::StringPtr(verb.c_str()));
  if (verb_lower != "cancel"_kj && verb_lower != "c"_kj) {
    return kj::Maybe<ParsedCancel>();
  }

  std::string client_id;
  iss >> client_id;
  if (client_id.empty()) {
    return kj::Maybe<ParsedCancel>();
  }

  ParsedCancel result;
  result.client_order_id = kj::str(client_id);
  result.raw_command = kj::str(line);
  return result;
}

// Parse QUERY command
// Formats:
//   QUERY <TYPE> [PARAMS]
//   Q <TYPE> [PARAMS]
kj::Maybe<ParsedQuery> parse_query_command(kj::StringPtr line) {
  std::istringstream iss{std::string(line.cStr())};
  std::string verb;
  iss >> verb;

  kj::String verb_lower = to_lower(kj::StringPtr(verb.c_str()));
  if (verb_lower != "query"_kj && verb_lower != "q"_kj) {
    return kj::Maybe<ParsedQuery>();
  }

  std::string query_type;
  std::string params;

  iss >> query_type;
  if (query_type.empty()) {
    return kj::Maybe<ParsedQuery>();
  }

  // Read remaining parameters
  std::string token;
  while (iss >> token) {
    if (!params.empty())
      params += " ";
    params += token;
  }

  ParsedQuery result;
  result.query_type = kj::str(query_type);
  result.params = kj::str(params);
  result.raw_command = kj::str(line);
  return result;
}

} // namespace veloz::engine
