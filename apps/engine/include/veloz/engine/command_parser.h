#pragma once

#include "veloz/exec/order_api.h"
#include "veloz/market/market_event.h"

#include <kj/common.h>
#include <kj/string.h>

namespace veloz::engine {

// Command types
enum class CommandType { Order, Cancel, Query, Strategy, Subscribe, Unsubscribe, Unknown };

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

// Strategy subcommand types
enum class StrategySubCommand { Load, Start, Stop, Pause, Resume, Unload, List, Status, Unknown };

struct ParsedStrategy final {
  StrategySubCommand subcommand{StrategySubCommand::Unknown};
  kj::String strategy_id;   ///< Strategy ID (for start/stop/unload/status)
  kj::String strategy_type; ///< Strategy type (for load)
  kj::String strategy_name; ///< Strategy name (for load)
  kj::String params;        ///< Additional parameters as JSON or key=value pairs
  kj::String raw_command;

  ParsedStrategy()
      : strategy_id(kj::str("")), strategy_type(kj::str("")), strategy_name(kj::str("")),
        params(kj::str("")), raw_command(kj::str("")) {}
};

/**
 * @brief Parsed subscription command
 * Format: SUBSCRIBE <VENUE> <SYMBOL> <EVENT_TYPE>
 * Example: SUBSCRIBE binance BTCUSDT trade
 */
struct ParsedSubscribe final {
  kj::String venue;  ///< Exchange venue (e.g., "binance")
  kj::String symbol; ///< Trading symbol (e.g., "BTCUSDT")
  veloz::market::MarketEventType event_type{veloz::market::MarketEventType::Unknown};
  kj::String raw_command;

  ParsedSubscribe() : venue(kj::str("")), symbol(kj::str("")), raw_command(kj::str("")) {}
};

/**
 * @brief Parsed unsubscription command
 * Format: UNSUBSCRIBE <VENUE> <SYMBOL> <EVENT_TYPE>
 * Example: UNSUBSCRIBE binance BTCUSDT trade
 */
struct ParsedUnsubscribe final {
  kj::String venue;  ///< Exchange venue (e.g., "binance")
  kj::String symbol; ///< Trading symbol (e.g., "BTCUSDT")
  veloz::market::MarketEventType event_type{veloz::market::MarketEventType::Unknown};
  kj::String raw_command;

  ParsedUnsubscribe() : venue(kj::str("")), symbol(kj::str("")), raw_command(kj::str("")) {}
};

struct ParsedCommand final {
  CommandType type;
  kj::Maybe<ParsedOrder> order;
  kj::Maybe<ParsedCancel> cancel;
  kj::Maybe<ParsedQuery> query;
  kj::Maybe<ParsedStrategy> strategy;
  kj::Maybe<ParsedSubscribe> subscribe;
  kj::Maybe<ParsedUnsubscribe> unsubscribe;
  kj::String error;

  ParsedCommand() : type(CommandType::Unknown), error(kj::str("")) {}
};

[[nodiscard]] ParsedCommand parse_command(kj::StringPtr line);
[[nodiscard]] kj::Maybe<ParsedOrder> parse_order_command(kj::StringPtr line);
[[nodiscard]] kj::Maybe<ParsedCancel> parse_cancel_command(kj::StringPtr line);
[[nodiscard]] kj::Maybe<ParsedQuery> parse_query_command(kj::StringPtr line);
[[nodiscard]] kj::Maybe<ParsedStrategy> parse_strategy_command(kj::StringPtr line);
[[nodiscard]] kj::Maybe<ParsedSubscribe> parse_subscribe_command(kj::StringPtr line);
[[nodiscard]] kj::Maybe<ParsedUnsubscribe> parse_unsubscribe_command(kj::StringPtr line);

// Helper for parsing market event type
[[nodiscard]] veloz::market::MarketEventType parse_market_event_type(kj::StringPtr type_str);

// Helper functions
[[nodiscard]] bool is_valid_order_side(kj::StringPtr side);
[[nodiscard]] veloz::exec::OrderSide parse_order_side(kj::StringPtr side);
[[nodiscard]] bool is_valid_order_type(kj::StringPtr type);
[[nodiscard]] veloz::exec::OrderType parse_order_type(kj::StringPtr type);
[[nodiscard]] bool is_valid_tif(kj::StringPtr tif);
[[nodiscard]] veloz::exec::TimeInForce parse_tif(kj::StringPtr tif);

} // namespace veloz::engine
