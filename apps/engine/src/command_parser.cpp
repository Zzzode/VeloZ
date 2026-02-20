#include "veloz/engine/command_parser.h"

// std::tolower, std::isspace for character classification (standard C library, KJ lacks equivalent)
#include <cctype>
// std::strtod for string to double conversion (standard C library, KJ lacks equivalent)
#include <cstdlib>
#include <kj/common.h>
#include <kj/debug.h>
#include <kj/string.h>
#include <kj/vector.h>

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

// Helper to tokenize a string by whitespace
static kj::Vector<kj::String> tokenize(kj::StringPtr s) {
  kj::Vector<kj::String> tokens;
  size_t i = 0;
  while (i < s.size()) {
    // Skip whitespace
    while (i < s.size() && std::isspace(static_cast<unsigned char>(s[i]))) {
      ++i;
    }
    if (i >= s.size()) {
      break;
    }
    // Find end of token
    size_t start = i;
    while (i < s.size() && !std::isspace(static_cast<unsigned char>(s[i]))) {
      ++i;
    }
    tokens.add(kj::str(s.slice(start, i)));
  }
  return tokens;
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

  auto tokens = tokenize(trimmed_line);
  if (tokens.size() == 0) {
    return result;
  }

  kj::String verb_lower = to_lower(tokens[0]);

  // Check for ORDER command (supports: ORDER, BUY, SELL)
  if (verb_lower == "order"_kj || verb_lower == "buy"_kj || verb_lower == "sell"_kj ||
      verb_lower == "b"_kj || verb_lower == "s"_kj) {
    auto order = parse_order_command(trimmed_line);
    KJ_IF_SOME(o, order) {
      result.type = CommandType::Order;
      result.order = kj::mv(o);
    }
    else {
      result.error = kj::str("Failed to parse ORDER command");
    }
  }
  // Check for CANCEL command
  else if (verb_lower == "cancel"_kj || verb_lower == "c"_kj) {
    auto cancel = parse_cancel_command(trimmed_line);
    KJ_IF_SOME(c, cancel) {
      result.type = CommandType::Cancel;
      result.cancel = kj::mv(c);
    }
    else {
      result.error = kj::str("Failed to parse CANCEL command");
    }
  }
  // Check for QUERY command
  else if (verb_lower == "query"_kj || verb_lower == "q"_kj) {
    auto query = parse_query_command(trimmed_line);
    KJ_IF_SOME(q, query) {
      result.type = CommandType::Query;
      result.query = kj::mv(q);
    }
    else {
      result.error = kj::str("Failed to parse QUERY command");
    }
    // Check for STRATEGY command
  } else if (verb_lower == "strategy"_kj || verb_lower == "strat"_kj) {
    auto strategy = parse_strategy_command(trimmed_line);
    KJ_IF_SOME(s, strategy) {
      result.type = CommandType::Strategy;
      result.strategy = kj::mv(s);
    }
    else {
      result.error = kj::str("Failed to parse STRATEGY command");
    }
  }
  // Check for SUBSCRIBE command
  else if (verb_lower == "subscribe"_kj || verb_lower == "sub"_kj) {
    auto subscribe = parse_subscribe_command(trimmed_line);
    KJ_IF_SOME(s, subscribe) {
      result.type = CommandType::Subscribe;
      result.subscribe = kj::mv(s);
    }
    else {
      result.error = kj::str("Failed to parse SUBSCRIBE command");
    }
  }
  // Check for UNSUBSCRIBE command
  else if (verb_lower == "unsubscribe"_kj || verb_lower == "unsub"_kj) {
    auto unsubscribe = parse_unsubscribe_command(trimmed_line);
    KJ_IF_SOME(u, unsubscribe) {
      result.type = CommandType::Unsubscribe;
      result.unsubscribe = kj::mv(u);
    }
    else {
      result.error = kj::str("Failed to parse UNSUBSCRIBE command");
    }
  } else {
    result.error = kj::str("Unknown command: ", tokens[0]);
  }

  return result;
}

// Parse ORDER command
// Formats:
//   ORDER <SIDE> <SYMBOL> <QTY> <PRICE> <CLIENT_ID> [TYPE] [TIF]
//   BUY <SYMBOL> <QTY> <PRICE> <CLIENT_ID> [TYPE] [TIF]
//   SELL <SYMBOL> <QTY> <PRICE> <CLIENT_ID> [TYPE] [TIF]
kj::Maybe<ParsedOrder> parse_order_command(kj::StringPtr line) {
  auto tokens = tokenize(line);
  if (tokens.size() == 0) {
    return kj::none;
  }

  kj::String verb_lower = to_lower(tokens[0]);
  size_t idx = 1;

  // Handle implicit BUY/SELL commands
  kj::String side_s;
  if (verb_lower == "buy"_kj || verb_lower == "b"_kj) {
    side_s = kj::str("buy");
  } else if (verb_lower == "sell"_kj || verb_lower == "s"_kj) {
    side_s = kj::str("sell");
  } else {
    // Explicit ORDER command - read side next
    if (idx >= tokens.size()) {
      return kj::none;
    }
    side_s = kj::str(tokens[idx]);
    if (!is_valid_order_side(side_s)) {
      return kj::none;
    }
    ++idx;
  }

  // Parse required parameters: symbol, qty, price, client_id
  if (idx + 3 >= tokens.size()) {
    return kj::none;
  }

  kj::String symbol = kj::str(tokens[idx++]);
  double qty = std::strtod(tokens[idx++].cStr(), nullptr);
  double price = std::strtod(tokens[idx++].cStr(), nullptr);
  kj::String client_id = kj::str(tokens[idx++]);

  // Parse optional parameters
  kj::String type_str = kj::str("limit");
  kj::String tif_str = kj::str("gtc");

  if (idx < tokens.size()) {
    type_str = kj::str(tokens[idx++]);
  }
  if (idx < tokens.size()) {
    tif_str = kj::str(tokens[idx++]);
  }

  // Validate required parameters
  // Note: price can be 0 for market orders
  bool is_market_order =
      is_valid_order_type(type_str) && parse_order_type(type_str) == veloz::exec::OrderType::Market;
  if (symbol.size() == 0 || qty <= 0.0 || client_id.size() == 0) {
    return kj::none;
  }
  // For limit orders, price must be positive
  if (!is_market_order && price <= 0.0) {
    return kj::none;
  }

  ParsedOrder out;
  out.raw_command = kj::str(line);
  out.request.symbol.value = kj::mv(symbol);
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
  out.request.client_order_id = kj::mv(client_id);

  return out;
}

// Parse CANCEL command
// Formats:
//   CANCEL <CLIENT_ID>
//   C <CLIENT_ID>
kj::Maybe<ParsedCancel> parse_cancel_command(kj::StringPtr line) {
  auto tokens = tokenize(line);
  if (tokens.size() < 2) {
    return kj::none;
  }

  kj::String verb_lower = to_lower(tokens[0]);
  if (verb_lower != "cancel"_kj && verb_lower != "c"_kj) {
    return kj::none;
  }

  if (tokens[1].size() == 0) {
    return kj::none;
  }

  ParsedCancel result;
  result.client_order_id = kj::str(tokens[1]);
  result.raw_command = kj::str(line);
  return result;
}

// Parse QUERY command
// Formats:
//   QUERY <TYPE> [PARAMS]
//   Q <TYPE> [PARAMS]
kj::Maybe<ParsedQuery> parse_query_command(kj::StringPtr line) {
  auto tokens = tokenize(line);
  if (tokens.size() < 2) {
    return kj::none;
  }

  kj::String verb_lower = to_lower(tokens[0]);
  if (verb_lower != "query"_kj && verb_lower != "q"_kj) {
    return kj::none;
  }

  if (tokens[1].size() == 0) {
    return kj::none;
  }

  // Collect remaining parameters
  kj::Vector<char> params_buf;
  for (size_t i = 2; i < tokens.size(); ++i) {
    if (params_buf.size() > 0) {
      params_buf.add(' ');
    }
    for (char c : tokens[i]) {
      params_buf.add(c);
    }
  }
  params_buf.add('\0');

  ParsedQuery result;
  result.query_type = kj::str(tokens[1]);
  result.params = kj::String(params_buf.releaseAsArray());
  result.raw_command = kj::str(line);
  return result;
}

// Parse STRATEGY command
// Formats:
//   STRATEGY LOAD <TYPE> <NAME> [PARAMS...]
//   STRATEGY START <STRATEGY_ID>
//   STRATEGY STOP <STRATEGY_ID>
//   STRATEGY UNLOAD <STRATEGY_ID>
//   STRATEGY LIST
//   STRATEGY STATUS [STRATEGY_ID]
//   STRAT <SUBCOMMAND> ...
kj::Maybe<ParsedStrategy> parse_strategy_command(kj::StringPtr line) {
  auto tokens = tokenize(line);
  if (tokens.size() < 2) {
    return kj::none;
  }

  kj::String verb_lower = to_lower(tokens[0]);
  if (verb_lower != "strategy"_kj && verb_lower != "strat"_kj) {
    return kj::none;
  }

  kj::String subcommand_lower = to_lower(tokens[1]);
  ParsedStrategy result;
  result.raw_command = kj::str(line);

  // Parse subcommand
  if (subcommand_lower == "load"_kj || subcommand_lower == "l"_kj) {
    result.subcommand = StrategySubCommand::Load;
    // STRATEGY LOAD <TYPE> <NAME> [PARAMS...]
    if (tokens.size() < 4) {
      return kj::none;
    }
    result.strategy_type = kj::str(tokens[2]);
    result.strategy_name = kj::str(tokens[3]);

    // Collect remaining parameters
    kj::Vector<char> params_buf;
    for (size_t i = 4; i < tokens.size(); ++i) {
      if (params_buf.size() > 0) {
        params_buf.add(' ');
      }
      for (char c : tokens[i]) {
        params_buf.add(c);
      }
    }
    params_buf.add('\0');
    result.params = kj::String(params_buf.releaseAsArray());
  } else if (subcommand_lower == "start"_kj) {
    result.subcommand = StrategySubCommand::Start;
    // STRATEGY START <STRATEGY_ID>
    if (tokens.size() < 3) {
      return kj::none;
    }
    result.strategy_id = kj::str(tokens[2]);
  } else if (subcommand_lower == "stop"_kj) {
    result.subcommand = StrategySubCommand::Stop;
    // STRATEGY STOP <STRATEGY_ID>
    if (tokens.size() < 3) {
      return kj::none;
    }
    result.strategy_id = kj::str(tokens[2]);
  } else if (subcommand_lower == "pause"_kj || subcommand_lower == "p"_kj) {
    result.subcommand = StrategySubCommand::Pause;
    // STRATEGY PAUSE <STRATEGY_ID>
    if (tokens.size() < 3) {
      return kj::none;
    }
    result.strategy_id = kj::str(tokens[2]);
  } else if (subcommand_lower == "resume"_kj || subcommand_lower == "r"_kj) {
    result.subcommand = StrategySubCommand::Resume;
    // STRATEGY RESUME <STRATEGY_ID>
    if (tokens.size() < 3) {
      return kj::none;
    }
    result.strategy_id = kj::str(tokens[2]);
  } else if (subcommand_lower == "unload"_kj || subcommand_lower == "u"_kj) {
    result.subcommand = StrategySubCommand::Unload;
    // STRATEGY UNLOAD <STRATEGY_ID>
    if (tokens.size() < 3) {
      return kj::none;
    }
    result.strategy_id = kj::str(tokens[2]);
  } else if (subcommand_lower == "list"_kj || subcommand_lower == "ls"_kj) {
    result.subcommand = StrategySubCommand::List;
    // STRATEGY LIST - no additional parameters required
  } else if (subcommand_lower == "status"_kj || subcommand_lower == "stat"_kj) {
    result.subcommand = StrategySubCommand::Status;
    // STRATEGY STATUS [STRATEGY_ID]
    if (tokens.size() >= 3) {
      result.strategy_id = kj::str(tokens[2]);
    }
  } else {
    result.subcommand = StrategySubCommand::Unknown;
    return kj::none;
  }

  return result;
}

// Parse market event type string to enum
veloz::market::MarketEventType parse_market_event_type(kj::StringPtr type_str) {
  kj::String lower = to_lower(type_str);
  if (lower == "trade"_kj || lower == "t"_kj) {
    return veloz::market::MarketEventType::Trade;
  } else if (lower == "booktop"_kj || lower == "book_top"_kj || lower == "ticker"_kj) {
    return veloz::market::MarketEventType::BookTop;
  } else if (lower == "bookdelta"_kj || lower == "book_delta"_kj || lower == "depth"_kj) {
    return veloz::market::MarketEventType::BookDelta;
  } else if (lower == "kline"_kj || lower == "k"_kj || lower == "candle"_kj) {
    return veloz::market::MarketEventType::Kline;
  }
  return veloz::market::MarketEventType::Unknown;
}

// Parse SUBSCRIBE command
// Formats:
//   SUBSCRIBE <VENUE> <SYMBOL> <EVENT_TYPE>
//   SUB <VENUE> <SYMBOL> <EVENT_TYPE>
// Examples:
//   SUBSCRIBE binance BTCUSDT trade
//   SUB binance ETHUSDT depth
kj::Maybe<ParsedSubscribe> parse_subscribe_command(kj::StringPtr line) {
  auto tokens = tokenize(line);
  if (tokens.size() < 4) {
    return kj::none;
  }

  kj::String verb_lower = to_lower(tokens[0]);
  if (verb_lower != "subscribe"_kj && verb_lower != "sub"_kj) {
    return kj::none;
  }

  // Validate venue
  if (tokens[1].size() == 0) {
    return kj::none;
  }

  // Validate symbol
  if (tokens[2].size() == 0) {
    return kj::none;
  }

  // Parse event type
  auto event_type = parse_market_event_type(tokens[3]);
  if (event_type == veloz::market::MarketEventType::Unknown) {
    return kj::none;
  }

  ParsedSubscribe result;
  result.venue = kj::str(tokens[1]);
  result.symbol = kj::str(tokens[2]);
  result.event_type = event_type;
  result.raw_command = kj::str(line);
  return result;
}

// Parse UNSUBSCRIBE command
// Formats:
//   UNSUBSCRIBE <VENUE> <SYMBOL> <EVENT_TYPE>
//   UNSUB <VENUE> <SYMBOL> <EVENT_TYPE>
// Examples:
//   UNSUBSCRIBE binance BTCUSDT trade
//   UNSUB binance ETHUSDT depth
kj::Maybe<ParsedUnsubscribe> parse_unsubscribe_command(kj::StringPtr line) {
  auto tokens = tokenize(line);
  if (tokens.size() < 4) {
    return kj::none;
  }

  kj::String verb_lower = to_lower(tokens[0]);
  if (verb_lower != "unsubscribe"_kj && verb_lower != "unsub"_kj) {
    return kj::none;
  }

  // Validate venue
  if (tokens[1].size() == 0) {
    return kj::none;
  }

  // Validate symbol
  if (tokens[2].size() == 0) {
    return kj::none;
  }

  // Parse event type
  auto event_type = parse_market_event_type(tokens[3]);
  if (event_type == veloz::market::MarketEventType::Unknown) {
    return kj::none;
  }

  ParsedUnsubscribe result;
  result.venue = kj::str(tokens[1]);
  result.symbol = kj::str(tokens[2]);
  result.event_type = event_type;
  result.raw_command = kj::str(line);
  return result;
}

} // namespace veloz::engine
