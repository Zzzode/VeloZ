#include "veloz/engine/command_parser.h"

#include <kj/common.h>
#include <kj/string.h>
#include <kj/test.h>

namespace veloz::engine {

namespace {

// ============================================================================
// Order Command Parser Tests
// ============================================================================

KJ_TEST("ParseOrderCommand: Buy") {
  auto result = parse_order_command("ORDER BUY BTCUSDT 0.5 50000.0 order001"_kj);

  KJ_IF_SOME(order, result) {
    KJ_EXPECT(order.request.symbol.value == "BTCUSDT");
    KJ_EXPECT(order.request.side == veloz::exec::OrderSide::Buy);
    KJ_EXPECT(order.request.qty == 0.5);
    KJ_IF_SOME(price, order.request.price) {
      KJ_EXPECT(price == 50000.0);
    }
    else {
      KJ_FAIL_EXPECT("price not found");
    }
    KJ_EXPECT(order.request.client_order_id == "order001"_kj);
  }
  else {
    KJ_FAIL_EXPECT("order parsing failed");
  }
}

KJ_TEST("ParseOrderCommand: Sell") {
  auto result = parse_order_command("ORDER SELL ETHUSDT 10.0 3000.0 order002"_kj);

  KJ_IF_SOME(order, result) {
    KJ_EXPECT(order.request.symbol.value == "ETHUSDT");
    KJ_EXPECT(order.request.side == veloz::exec::OrderSide::Sell);
    KJ_EXPECT(order.request.qty == 10.0);
    KJ_IF_SOME(price, order.request.price) {
      KJ_EXPECT(price == 3000.0);
    }
    else {
      KJ_FAIL_EXPECT("price not found");
    }
    KJ_EXPECT(order.request.client_order_id == "order002"_kj);
  }
  else {
    KJ_FAIL_EXPECT("order parsing failed");
  }
}

KJ_TEST("ParseOrderCommand: Buy shortcut") {
  auto result = parse_order_command("BUY BTCUSDT 0.5 50000.0 order003"_kj);

  KJ_IF_SOME(order, result) {
    KJ_EXPECT(order.request.symbol.value == "BTCUSDT");
    KJ_EXPECT(order.request.side == veloz::exec::OrderSide::Buy);
  }
  else {
    KJ_FAIL_EXPECT("order parsing failed");
  }
}

KJ_TEST("ParseOrderCommand: Sell shortcut") {
  auto result = parse_order_command("SELL ETHUSDT 10.0 3000.0 order004"_kj);

  KJ_IF_SOME(order, result) {
    KJ_EXPECT(order.request.symbol.value == "ETHUSDT");
    KJ_EXPECT(order.request.side == veloz::exec::OrderSide::Sell);
  }
  else {
    KJ_FAIL_EXPECT("order parsing failed");
  }
}

KJ_TEST("ParseOrderCommand: With order type") {
  auto result = parse_order_command("ORDER BUY BTCUSDT 0.5 50000.0 order005 MARKET GTC"_kj);

  KJ_IF_SOME(order, result) {
    KJ_EXPECT(order.request.type == veloz::exec::OrderType::Market);
    KJ_EXPECT(order.request.tif == veloz::exec::TimeInForce::GTC);
  }
  else {
    KJ_FAIL_EXPECT("order parsing failed");
  }
}

KJ_TEST("ParseOrderCommand: With market type") {
  auto result = parse_order_command("BUY BTCUSDT 0.5 0.0 order006 MARKET"_kj);

  KJ_IF_SOME(order, result) {
    KJ_EXPECT(order.request.type == veloz::exec::OrderType::Market);
  }
  else {
    KJ_FAIL_EXPECT("order parsing failed");
  }
}

KJ_TEST("ParseOrderCommand: With IOC TIF") {
  auto result = parse_order_command("ORDER BUY BTCUSDT 0.5 50000.0 order007 LIMIT IOC"_kj);

  KJ_IF_SOME(order, result) {
    KJ_EXPECT(order.request.tif == veloz::exec::TimeInForce::IOC);
  }
  else {
    KJ_FAIL_EXPECT("order parsing failed");
  }
}

KJ_TEST("ParseOrderCommand: Invalid side") {
  auto result = parse_order_command("ORDER INVALID BTCUSDT 0.5 50000.0 order008"_kj);

  KJ_IF_SOME(order, result) {
    KJ_FAIL_EXPECT("expected parse to fail with invalid side");
  }
  else {
    // Expected: parsing failed
  }
}

KJ_TEST("ParseOrderCommand: Invalid quantity") {
  auto result = parse_order_command("ORDER BUY BTCUSDT -0.5 50000.0 order009"_kj);

  KJ_IF_SOME(order, result) {
    KJ_FAIL_EXPECT("expected parse to fail with invalid quantity");
  }
  else {
    // Expected: parsing failed
  }
}

KJ_TEST("ParseOrderCommand: Invalid price") {
  auto result = parse_order_command("ORDER BUY BTCUSDT 0.5 -50000.0 order010"_kj);

  KJ_IF_SOME(order, result) {
    KJ_FAIL_EXPECT("expected parse to fail with invalid price");
  }
  else {
    // Expected: parsing failed
  }
}

KJ_TEST("ParseOrderCommand: Missing client ID") {
  auto result = parse_order_command("ORDER BUY BTCUSDT 0.5 50000.0"_kj);

  KJ_IF_SOME(order, result) {
    KJ_FAIL_EXPECT("expected parse to fail with missing client ID");
  }
  else {
    // Expected: parsing failed
  }
}

// ============================================================================
// Cancel Command Parser Tests
// ============================================================================

KJ_TEST("ParseCancelCommand: Basic") {
  auto result = parse_cancel_command("CANCEL order001"_kj);

  KJ_IF_SOME(cancel, result) {
    KJ_EXPECT(cancel.client_order_id == "order001"_kj);
  }
  else {
    KJ_FAIL_EXPECT("cancel parsing failed");
  }
}

KJ_TEST("ParseCancelCommand: Shortcut") {
  auto result = parse_cancel_command("C order002"_kj);

  KJ_IF_SOME(cancel, result) {
    KJ_EXPECT(cancel.client_order_id == "order002"_kj);
  }
  else {
    KJ_FAIL_EXPECT("cancel parsing failed");
  }
}

KJ_TEST("ParseCancelCommand: Missing ID") {
  auto result = parse_cancel_command("CANCEL"_kj);

  KJ_IF_SOME(cancel, result) {
    KJ_FAIL_EXPECT("expected parse to fail with missing ID");
  }
  else {
    // Expected: parsing failed
  }
}

// ============================================================================
// Query Command Parser Tests
// ============================================================================

KJ_TEST("ParseQueryCommand: Basic") {
  auto result = parse_query_command("QUERY account"_kj);

  KJ_IF_SOME(query, result) {
    KJ_EXPECT(query.query_type == "account"_kj);
    KJ_EXPECT(query.params == ""_kj);
  }
  else {
    KJ_FAIL_EXPECT("query parsing failed");
  }
}

KJ_TEST("ParseQueryCommand: With params") {
  auto result = parse_query_command("QUERY order BTCUSDT"_kj);

  KJ_IF_SOME(query, result) {
    KJ_EXPECT(query.query_type == "order"_kj);
    KJ_EXPECT(query.params == "BTCUSDT"_kj);
  }
  else {
    KJ_FAIL_EXPECT("query parsing failed");
  }
}

KJ_TEST("ParseQueryCommand: Shortcut") {
  auto result = parse_query_command("Q balance"_kj);

  KJ_IF_SOME(query, result) {
    KJ_EXPECT(query.query_type == "balance"_kj);
  }
  else {
    KJ_FAIL_EXPECT("query parsing failed");
  }
}

// ============================================================================
// General Command Parser Tests
// ============================================================================

KJ_TEST("ParseCommand: Order") {
  auto result = parse_command("ORDER BUY BTCUSDT 0.5 50000.0 order001"_kj);

  KJ_EXPECT(result.type == CommandType::Order);
  KJ_IF_SOME(order, result.order) {
    // Order was parsed successfully
  }
  else {
    KJ_FAIL_EXPECT("order not found in parsed command");
  }
}

KJ_TEST("ParseCommand: Cancel") {
  auto result = parse_command("CANCEL order001"_kj);

  KJ_EXPECT(result.type == CommandType::Cancel);
  KJ_IF_SOME(cancel, result.cancel) {
    // Cancel was parsed successfully
  }
  else {
    KJ_FAIL_EXPECT("cancel not found in parsed command");
  }
}

KJ_TEST("ParseCommand: Query") {
  auto result = parse_command("QUERY account"_kj);

  KJ_EXPECT(result.type == CommandType::Query);
  KJ_IF_SOME(query, result.query) {
    // Query was parsed successfully
  }
  else {
    KJ_FAIL_EXPECT("query not found in parsed command");
  }
}

KJ_TEST("ParseCommand: Unknown command") {
  auto result = parse_command("INVALID_COMMAND"_kj);

  KJ_EXPECT(result.type == CommandType::Unknown);
  KJ_EXPECT(result.error.size() > 0);
}

KJ_TEST("ParseCommand: Empty line") {
  auto result = parse_command(""_kj);

  KJ_EXPECT(result.type == CommandType::Unknown);
}

KJ_TEST("ParseCommand: Comment line") {
  auto result = parse_command("# This is a comment"_kj);

  KJ_EXPECT(result.type == CommandType::Unknown);
}

// ============================================================================
// Order Side Tests
// ============================================================================

KJ_TEST("OrderSide: Parse (case insensitive)") {
  KJ_EXPECT(parse_order_side("BUY"_kj) == veloz::exec::OrderSide::Buy);
  KJ_EXPECT(parse_order_side("buy"_kj) == veloz::exec::OrderSide::Buy);
  KJ_EXPECT(parse_order_side("Buy"_kj) == veloz::exec::OrderSide::Buy);
  KJ_EXPECT(parse_order_side("b"_kj) == veloz::exec::OrderSide::Buy);
  KJ_EXPECT(parse_order_side("SELL"_kj) == veloz::exec::OrderSide::Sell);
  KJ_EXPECT(parse_order_side("sell"_kj) == veloz::exec::OrderSide::Sell);
  KJ_EXPECT(parse_order_side("Sell"_kj) == veloz::exec::OrderSide::Sell);
  KJ_EXPECT(parse_order_side("s"_kj) == veloz::exec::OrderSide::Sell);
}

KJ_TEST("OrderSide: Is valid") {
  KJ_EXPECT(is_valid_order_side("BUY"_kj));
  KJ_EXPECT(is_valid_order_side("buy"_kj));
  KJ_EXPECT(is_valid_order_side("b"_kj));
  KJ_EXPECT(is_valid_order_side("SELL"_kj));
  KJ_EXPECT(is_valid_order_side("sell"_kj));
  KJ_EXPECT(is_valid_order_side("s"_kj));
  KJ_EXPECT(!is_valid_order_side("INVALID"_kj));
}

// ============================================================================
// Order Type Tests
// ============================================================================

KJ_TEST("OrderType: Parse (case insensitive)") {
  KJ_EXPECT(parse_order_type("LIMIT"_kj) == veloz::exec::OrderType::Limit);
  KJ_EXPECT(parse_order_type("limit"_kj) == veloz::exec::OrderType::Limit);
  KJ_EXPECT(parse_order_type("l"_kj) == veloz::exec::OrderType::Limit);
  KJ_EXPECT(parse_order_type("MARKET"_kj) == veloz::exec::OrderType::Market);
  KJ_EXPECT(parse_order_type("market"_kj) == veloz::exec::OrderType::Market);
  KJ_EXPECT(parse_order_type("m"_kj) == veloz::exec::OrderType::Market);
}

KJ_TEST("OrderType: Is valid") {
  KJ_EXPECT(is_valid_order_type("LIMIT"_kj));
  KJ_EXPECT(is_valid_order_type("limit"_kj));
  KJ_EXPECT(is_valid_order_type("l"_kj));
  KJ_EXPECT(is_valid_order_type("MARKET"_kj));
  KJ_EXPECT(is_valid_order_type("market"_kj));
  KJ_EXPECT(is_valid_order_type("m"_kj));
  KJ_EXPECT(!is_valid_order_type("INVALID"_kj));
}

// ============================================================================
// Time In Force Tests
// ============================================================================

KJ_TEST("Tif: Parse (case insensitive)") {
  KJ_EXPECT(parse_tif("GTC"_kj) == veloz::exec::TimeInForce::GTC);
  KJ_EXPECT(parse_tif("gtc"_kj) == veloz::exec::TimeInForce::GTC);
  KJ_EXPECT(parse_tif("g"_kj) == veloz::exec::TimeInForce::GTC);
  KJ_EXPECT(parse_tif("IOC"_kj) == veloz::exec::TimeInForce::IOC);
  KJ_EXPECT(parse_tif("ioc"_kj) == veloz::exec::TimeInForce::IOC);
  KJ_EXPECT(parse_tif("FOK"_kj) == veloz::exec::TimeInForce::FOK);
  KJ_EXPECT(parse_tif("fok"_kj) == veloz::exec::TimeInForce::FOK);
  KJ_EXPECT(parse_tif("GTX"_kj) == veloz::exec::TimeInForce::GTX);
  KJ_EXPECT(parse_tif("gtx"_kj) == veloz::exec::TimeInForce::GTX);
}

KJ_TEST("Tif: Is valid") {
  KJ_EXPECT(is_valid_tif("GTC"_kj));
  KJ_EXPECT(is_valid_tif("gtc"_kj));
  KJ_EXPECT(is_valid_tif("g"_kj));
  KJ_EXPECT(is_valid_tif("IOC"_kj));
  KJ_EXPECT(is_valid_tif("ioc"_kj));
  KJ_EXPECT(is_valid_tif("FOK"_kj));
  KJ_EXPECT(is_valid_tif("fok"_kj));
  KJ_EXPECT(is_valid_tif("GTX"_kj));
  KJ_EXPECT(is_valid_tif("gtx"_kj));
  KJ_EXPECT(!is_valid_tif("INVALID"_kj));
}

// ============================================================================
// Strategy Command Parser Tests
// ============================================================================

KJ_TEST("ParseStrategyCommand: Load") {
  auto result = parse_strategy_command("STRATEGY LOAD RSI MyRsiStrategy"_kj);

  KJ_IF_SOME(strategy, result) {
    KJ_EXPECT(strategy.subcommand == StrategySubCommand::Load);
    KJ_EXPECT(strategy.strategy_type == "RSI"_kj);
    KJ_EXPECT(strategy.strategy_name == "MyRsiStrategy"_kj);
  }
  else {
    KJ_FAIL_EXPECT("strategy parsing failed");
  }
}

KJ_TEST("ParseStrategyCommand: Load with params") {
  auto result = parse_strategy_command("STRATEGY LOAD MACD MyMacdStrategy fast=12 slow=26"_kj);

  KJ_IF_SOME(strategy, result) {
    KJ_EXPECT(strategy.subcommand == StrategySubCommand::Load);
    KJ_EXPECT(strategy.strategy_type == "MACD"_kj);
    KJ_EXPECT(strategy.strategy_name == "MyMacdStrategy"_kj);
    KJ_EXPECT(strategy.params == "fast=12 slow=26"_kj);
  }
  else {
    KJ_FAIL_EXPECT("strategy parsing failed");
  }
}

KJ_TEST("ParseStrategyCommand: Start") {
  auto result = parse_strategy_command("STRATEGY START strat-123456"_kj);

  KJ_IF_SOME(strategy, result) {
    KJ_EXPECT(strategy.subcommand == StrategySubCommand::Start);
    KJ_EXPECT(strategy.strategy_id == "strat-123456"_kj);
  }
  else {
    KJ_FAIL_EXPECT("strategy parsing failed");
  }
}

KJ_TEST("ParseStrategyCommand: Stop") {
  auto result = parse_strategy_command("STRATEGY STOP strat-123456"_kj);

  KJ_IF_SOME(strategy, result) {
    KJ_EXPECT(strategy.subcommand == StrategySubCommand::Stop);
    KJ_EXPECT(strategy.strategy_id == "strat-123456"_kj);
  }
  else {
    KJ_FAIL_EXPECT("strategy parsing failed");
  }
}

KJ_TEST("ParseStrategyCommand: Unload") {
  auto result = parse_strategy_command("STRATEGY UNLOAD strat-123456"_kj);

  KJ_IF_SOME(strategy, result) {
    KJ_EXPECT(strategy.subcommand == StrategySubCommand::Unload);
    KJ_EXPECT(strategy.strategy_id == "strat-123456"_kj);
  }
  else {
    KJ_FAIL_EXPECT("strategy parsing failed");
  }
}

KJ_TEST("ParseStrategyCommand: List") {
  auto result = parse_strategy_command("STRATEGY LIST"_kj);

  KJ_IF_SOME(strategy, result) {
    KJ_EXPECT(strategy.subcommand == StrategySubCommand::List);
  }
  else {
    KJ_FAIL_EXPECT("strategy parsing failed");
  }
}

KJ_TEST("ParseStrategyCommand: Status all") {
  auto result = parse_strategy_command("STRATEGY STATUS"_kj);

  KJ_IF_SOME(strategy, result) {
    KJ_EXPECT(strategy.subcommand == StrategySubCommand::Status);
    KJ_EXPECT(strategy.strategy_id == ""_kj);
  }
  else {
    KJ_FAIL_EXPECT("strategy parsing failed");
  }
}

KJ_TEST("ParseStrategyCommand: Status specific") {
  auto result = parse_strategy_command("STRATEGY STATUS strat-123456"_kj);

  KJ_IF_SOME(strategy, result) {
    KJ_EXPECT(strategy.subcommand == StrategySubCommand::Status);
    KJ_EXPECT(strategy.strategy_id == "strat-123456"_kj);
  }
  else {
    KJ_FAIL_EXPECT("strategy parsing failed");
  }
}

KJ_TEST("ParseStrategyCommand: Shortcut STRAT") {
  auto result = parse_strategy_command("STRAT LIST"_kj);

  KJ_IF_SOME(strategy, result) {
    KJ_EXPECT(strategy.subcommand == StrategySubCommand::List);
  }
  else {
    KJ_FAIL_EXPECT("strategy parsing failed");
  }
}

KJ_TEST("ParseStrategyCommand: Invalid subcommand") {
  auto result = parse_strategy_command("STRATEGY INVALID"_kj);

  KJ_IF_SOME(strategy, result) {
    KJ_FAIL_EXPECT("expected parse to fail with invalid subcommand");
  }
  else {
    // Expected: parsing failed
  }
}

KJ_TEST("ParseStrategyCommand: Load missing type") {
  auto result = parse_strategy_command("STRATEGY LOAD"_kj);

  KJ_IF_SOME(strategy, result) {
    KJ_FAIL_EXPECT("expected parse to fail with missing type");
  }
  else {
    // Expected: parsing failed
  }
}

KJ_TEST("ParseStrategyCommand: Load missing name") {
  auto result = parse_strategy_command("STRATEGY LOAD RSI"_kj);

  KJ_IF_SOME(strategy, result) {
    KJ_FAIL_EXPECT("expected parse to fail with missing name");
  }
  else {
    // Expected: parsing failed
  }
}

KJ_TEST("ParseStrategyCommand: Start missing ID") {
  auto result = parse_strategy_command("STRATEGY START"_kj);

  KJ_IF_SOME(strategy, result) {
    KJ_FAIL_EXPECT("expected parse to fail with missing ID");
  }
  else {
    // Expected: parsing failed
  }
}

KJ_TEST("ParseCommand: Strategy") {
  auto result = parse_command("STRATEGY LIST"_kj);

  KJ_EXPECT(result.type == CommandType::Strategy);
  KJ_IF_SOME(strategy, result.strategy) {
    KJ_EXPECT(strategy.subcommand == StrategySubCommand::List);
  }
  else {
    KJ_FAIL_EXPECT("strategy not found in parsed command");
  }
}

// ============================================================================
// Subscribe Command Parser Tests
// ============================================================================

KJ_TEST("ParseSubscribeCommand: Trade") {
  auto result = parse_subscribe_command("SUBSCRIBE binance BTCUSDT trade"_kj);

  KJ_IF_SOME(sub, result) {
    KJ_EXPECT(sub.venue == "binance"_kj);
    KJ_EXPECT(sub.symbol == "BTCUSDT"_kj);
    KJ_EXPECT(sub.event_type == veloz::market::MarketEventType::Trade);
  }
  else {
    KJ_FAIL_EXPECT("subscribe parsing failed");
  }
}

KJ_TEST("ParseSubscribeCommand: Depth") {
  auto result = parse_subscribe_command("SUBSCRIBE binance ETHUSDT depth"_kj);

  KJ_IF_SOME(sub, result) {
    KJ_EXPECT(sub.venue == "binance"_kj);
    KJ_EXPECT(sub.symbol == "ETHUSDT"_kj);
    KJ_EXPECT(sub.event_type == veloz::market::MarketEventType::BookDelta);
  }
  else {
    KJ_FAIL_EXPECT("subscribe parsing failed");
  }
}

KJ_TEST("ParseSubscribeCommand: BookTop") {
  auto result = parse_subscribe_command("SUBSCRIBE binance BTCUSDT book_top"_kj);

  KJ_IF_SOME(sub, result) {
    KJ_EXPECT(sub.event_type == veloz::market::MarketEventType::BookTop);
  }
  else {
    KJ_FAIL_EXPECT("subscribe parsing failed");
  }
}

KJ_TEST("ParseSubscribeCommand: Kline") {
  auto result = parse_subscribe_command("SUBSCRIBE binance BTCUSDT kline"_kj);

  KJ_IF_SOME(sub, result) {
    KJ_EXPECT(sub.event_type == veloz::market::MarketEventType::Kline);
  }
  else {
    KJ_FAIL_EXPECT("subscribe parsing failed");
  }
}

KJ_TEST("ParseSubscribeCommand: Shortcut SUB") {
  auto result = parse_subscribe_command("SUB binance BTCUSDT trade"_kj);

  KJ_IF_SOME(sub, result) {
    KJ_EXPECT(sub.venue == "binance"_kj);
    KJ_EXPECT(sub.symbol == "BTCUSDT"_kj);
    KJ_EXPECT(sub.event_type == veloz::market::MarketEventType::Trade);
  }
  else {
    KJ_FAIL_EXPECT("subscribe parsing failed");
  }
}

KJ_TEST("ParseSubscribeCommand: Missing venue") {
  auto result = parse_subscribe_command("SUBSCRIBE"_kj);

  KJ_IF_SOME(sub, result) {
    KJ_FAIL_EXPECT("expected parse to fail with missing venue");
  }
  else {
    // Expected: parsing failed
  }
}

KJ_TEST("ParseSubscribeCommand: Missing symbol") {
  auto result = parse_subscribe_command("SUBSCRIBE binance"_kj);

  KJ_IF_SOME(sub, result) {
    KJ_FAIL_EXPECT("expected parse to fail with missing symbol");
  }
  else {
    // Expected: parsing failed
  }
}

KJ_TEST("ParseSubscribeCommand: Missing event type") {
  auto result = parse_subscribe_command("SUBSCRIBE binance BTCUSDT"_kj);

  KJ_IF_SOME(sub, result) {
    KJ_FAIL_EXPECT("expected parse to fail with missing event type");
  }
  else {
    // Expected: parsing failed
  }
}

KJ_TEST("ParseSubscribeCommand: Invalid event type") {
  auto result = parse_subscribe_command("SUBSCRIBE binance BTCUSDT invalid_type"_kj);

  KJ_IF_SOME(sub, result) {
    KJ_FAIL_EXPECT("expected parse to fail with invalid event type");
  }
  else {
    // Expected: parsing failed
  }
}

// ============================================================================
// Unsubscribe Command Parser Tests
// ============================================================================

KJ_TEST("ParseUnsubscribeCommand: Trade") {
  auto result = parse_unsubscribe_command("UNSUBSCRIBE binance BTCUSDT trade"_kj);

  KJ_IF_SOME(unsub, result) {
    KJ_EXPECT(unsub.venue == "binance"_kj);
    KJ_EXPECT(unsub.symbol == "BTCUSDT"_kj);
    KJ_EXPECT(unsub.event_type == veloz::market::MarketEventType::Trade);
  }
  else {
    KJ_FAIL_EXPECT("unsubscribe parsing failed");
  }
}

KJ_TEST("ParseUnsubscribeCommand: Shortcut UNSUB") {
  auto result = parse_unsubscribe_command("UNSUB binance ETHUSDT depth"_kj);

  KJ_IF_SOME(unsub, result) {
    KJ_EXPECT(unsub.venue == "binance"_kj);
    KJ_EXPECT(unsub.symbol == "ETHUSDT"_kj);
    KJ_EXPECT(unsub.event_type == veloz::market::MarketEventType::BookDelta);
  }
  else {
    KJ_FAIL_EXPECT("unsubscribe parsing failed");
  }
}

KJ_TEST("ParseUnsubscribeCommand: Missing arguments") {
  auto result = parse_unsubscribe_command("UNSUBSCRIBE binance BTCUSDT"_kj);

  KJ_IF_SOME(unsub, result) {
    KJ_FAIL_EXPECT("expected parse to fail with missing arguments");
  }
  else {
    // Expected: parsing failed
  }
}

// ============================================================================
// General Command Parser Tests for Subscribe/Unsubscribe
// ============================================================================

KJ_TEST("ParseCommand: Subscribe") {
  auto result = parse_command("SUBSCRIBE binance BTCUSDT trade"_kj);

  KJ_EXPECT(result.type == CommandType::Subscribe);
  KJ_IF_SOME(sub, result.subscribe) {
    KJ_EXPECT(sub.venue == "binance"_kj);
    KJ_EXPECT(sub.symbol == "BTCUSDT"_kj);
    KJ_EXPECT(sub.event_type == veloz::market::MarketEventType::Trade);
  }
  else {
    KJ_FAIL_EXPECT("subscribe not found in parsed command");
  }
}

KJ_TEST("ParseCommand: Unsubscribe") {
  auto result = parse_command("UNSUBSCRIBE binance BTCUSDT trade"_kj);

  KJ_EXPECT(result.type == CommandType::Unsubscribe);
  KJ_IF_SOME(unsub, result.unsubscribe) {
    KJ_EXPECT(unsub.venue == "binance"_kj);
    KJ_EXPECT(unsub.symbol == "BTCUSDT"_kj);
    KJ_EXPECT(unsub.event_type == veloz::market::MarketEventType::Trade);
  }
  else {
    KJ_FAIL_EXPECT("unsubscribe not found in parsed command");
  }
}

// ============================================================================
// Market Event Type Parser Tests
// ============================================================================

KJ_TEST("ParseMarketEventType: Trade") {
  KJ_EXPECT(parse_market_event_type("trade"_kj) == veloz::market::MarketEventType::Trade);
  KJ_EXPECT(parse_market_event_type("TRADE"_kj) == veloz::market::MarketEventType::Trade);
  KJ_EXPECT(parse_market_event_type("t"_kj) == veloz::market::MarketEventType::Trade);
}

KJ_TEST("ParseMarketEventType: BookTop") {
  KJ_EXPECT(parse_market_event_type("booktop"_kj) == veloz::market::MarketEventType::BookTop);
  KJ_EXPECT(parse_market_event_type("book_top"_kj) == veloz::market::MarketEventType::BookTop);
  KJ_EXPECT(parse_market_event_type("ticker"_kj) == veloz::market::MarketEventType::BookTop);
}

KJ_TEST("ParseMarketEventType: BookDelta") {
  KJ_EXPECT(parse_market_event_type("bookdelta"_kj) == veloz::market::MarketEventType::BookDelta);
  KJ_EXPECT(parse_market_event_type("book_delta"_kj) == veloz::market::MarketEventType::BookDelta);
  KJ_EXPECT(parse_market_event_type("depth"_kj) == veloz::market::MarketEventType::BookDelta);
}

KJ_TEST("ParseMarketEventType: Kline") {
  KJ_EXPECT(parse_market_event_type("kline"_kj) == veloz::market::MarketEventType::Kline);
  KJ_EXPECT(parse_market_event_type("k"_kj) == veloz::market::MarketEventType::Kline);
  KJ_EXPECT(parse_market_event_type("candle"_kj) == veloz::market::MarketEventType::Kline);
}

KJ_TEST("ParseMarketEventType: Unknown") {
  KJ_EXPECT(parse_market_event_type("invalid"_kj) == veloz::market::MarketEventType::Unknown);
  KJ_EXPECT(parse_market_event_type(""_kj) == veloz::market::MarketEventType::Unknown);
}

} // namespace

} // namespace veloz::engine
