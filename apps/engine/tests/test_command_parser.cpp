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

  KJ_IF_SOME (order, result) {
    KJ_EXPECT(order.request.symbol.value == "BTCUSDT");
    KJ_EXPECT(order.request.side == veloz::exec::OrderSide::Buy);
    KJ_EXPECT(order.request.qty == 0.5);
    KJ_IF_SOME (price, order.request.price) {
      KJ_EXPECT(price == 50000.0);
    } else {
      KJ_FAIL_EXPECT("price not found");
    }
    KJ_EXPECT(order.request.client_order_id == "order001"_kj);
  } else {
    KJ_FAIL_EXPECT("order parsing failed");
  }
}

KJ_TEST("ParseOrderCommand: Sell") {
  auto result = parse_order_command("ORDER SELL ETHUSDT 10.0 3000.0 order002"_kj);

  KJ_IF_SOME (order, result) {
    KJ_EXPECT(order.request.symbol.value == "ETHUSDT");
    KJ_EXPECT(order.request.side == veloz::exec::OrderSide::Sell);
    KJ_EXPECT(order.request.qty == 10.0);
    KJ_IF_SOME (price, order.request.price) {
      KJ_EXPECT(price == 3000.0);
    } else {
      KJ_FAIL_EXPECT("price not found");
    }
    KJ_EXPECT(order.request.client_order_id == "order002"_kj);
  } else {
    KJ_FAIL_EXPECT("order parsing failed");
  }
}

KJ_TEST("ParseOrderCommand: Buy shortcut") {
  auto result = parse_order_command("BUY BTCUSDT 0.5 50000.0 order003"_kj);

  KJ_IF_SOME (order, result) {
    KJ_EXPECT(order.request.symbol.value == "BTCUSDT");
    KJ_EXPECT(order.request.side == veloz::exec::OrderSide::Buy);
  } else {
    KJ_FAIL_EXPECT("order parsing failed");
  }
}

KJ_TEST("ParseOrderCommand: Sell shortcut") {
  auto result = parse_order_command("SELL ETHUSDT 10.0 3000.0 order004"_kj);

  KJ_IF_SOME (order, result) {
    KJ_EXPECT(order.request.symbol.value == "ETHUSDT");
    KJ_EXPECT(order.request.side == veloz::exec::OrderSide::Sell);
  } else {
    KJ_FAIL_EXPECT("order parsing failed");
  }
}

KJ_TEST("ParseOrderCommand: With order type") {
  auto result = parse_order_command("ORDER BUY BTCUSDT 0.5 50000.0 order005 MARKET GTC"_kj);

  KJ_IF_SOME (order, result) {
    KJ_EXPECT(order.request.type == veloz::exec::OrderType::Market);
    KJ_EXPECT(order.request.tif == veloz::exec::TimeInForce::GTC);
  } else {
    KJ_FAIL_EXPECT("order parsing failed");
  }
}

KJ_TEST("ParseOrderCommand: With market type") {
  auto result = parse_order_command("BUY BTCUSDT 0.5 0.0 order006 MARKET"_kj);

  KJ_IF_SOME (order, result) {
    KJ_EXPECT(order.request.type == veloz::exec::OrderType::Market);
  } else {
    KJ_FAIL_EXPECT("order parsing failed");
  }
}

KJ_TEST("ParseOrderCommand: With IOC TIF") {
  auto result = parse_order_command("ORDER BUY BTCUSDT 0.5 50000.0 order007 LIMIT IOC"_kj);

  KJ_IF_SOME (order, result) {
    KJ_EXPECT(order.request.tif == veloz::exec::TimeInForce::IOC);
  } else {
    KJ_FAIL_EXPECT("order parsing failed");
  }
}

KJ_TEST("ParseOrderCommand: Invalid side") {
  auto result = parse_order_command("ORDER INVALID BTCUSDT 0.5 50000.0 order008"_kj);

  KJ_IF_SOME (order, result) {
    KJ_FAIL_EXPECT("expected parse to fail with invalid side");
  } else {
    // Expected: parsing failed
  }
}

KJ_TEST("ParseOrderCommand: Invalid quantity") {
  auto result = parse_order_command("ORDER BUY BTCUSDT -0.5 50000.0 order009"_kj);

  KJ_IF_SOME (order, result) {
    KJ_FAIL_EXPECT("expected parse to fail with invalid quantity");
  } else {
    // Expected: parsing failed
  }
}

KJ_TEST("ParseOrderCommand: Invalid price") {
  auto result = parse_order_command("ORDER BUY BTCUSDT 0.5 -50000.0 order010"_kj);

  KJ_IF_SOME (order, result) {
    KJ_FAIL_EXPECT("expected parse to fail with invalid price");
  } else {
    // Expected: parsing failed
  }
}

KJ_TEST("ParseOrderCommand: Missing client ID") {
  auto result = parse_order_command("ORDER BUY BTCUSDT 0.5 50000.0"_kj);

  KJ_IF_SOME (order, result) {
    KJ_FAIL_EXPECT("expected parse to fail with missing client ID");
  } else {
    // Expected: parsing failed
  }
}

// ============================================================================
// Cancel Command Parser Tests
// ============================================================================

KJ_TEST("ParseCancelCommand: Basic") {
  auto result = parse_cancel_command("CANCEL order001"_kj);

  KJ_IF_SOME (cancel, result) {
    KJ_EXPECT(cancel.client_order_id == "order001"_kj);
  } else {
    KJ_FAIL_EXPECT("cancel parsing failed");
  }
}

KJ_TEST("ParseCancelCommand: Shortcut") {
  auto result = parse_cancel_command("C order002"_kj);

  KJ_IF_SOME (cancel, result) {
    KJ_EXPECT(cancel.client_order_id == "order002"_kj);
  } else {
    KJ_FAIL_EXPECT("cancel parsing failed");
  }
}

KJ_TEST("ParseCancelCommand: Missing ID") {
  auto result = parse_cancel_command("CANCEL"_kj);

  KJ_IF_SOME (cancel, result) {
    KJ_FAIL_EXPECT("expected parse to fail with missing ID");
  } else {
    // Expected: parsing failed
  }
}

// ============================================================================
// Query Command Parser Tests
// ============================================================================

KJ_TEST("ParseQueryCommand: Basic") {
  auto result = parse_query_command("QUERY account"_kj);

  KJ_IF_SOME (query, result) {
    KJ_EXPECT(query.query_type == "account"_kj);
    KJ_EXPECT(query.params == ""_kj);
  } else {
    KJ_FAIL_EXPECT("query parsing failed");
  }
}

KJ_TEST("ParseQueryCommand: With params") {
  auto result = parse_query_command("QUERY order BTCUSDT"_kj);

  KJ_IF_SOME (query, result) {
    KJ_EXPECT(query.query_type == "order"_kj);
    KJ_EXPECT(query.params == "BTCUSDT"_kj);
  } else {
    KJ_FAIL_EXPECT("query parsing failed");
  }
}

KJ_TEST("ParseQueryCommand: Shortcut") {
  auto result = parse_query_command("Q balance"_kj);

  KJ_IF_SOME (query, result) {
    KJ_EXPECT(query.query_type == "balance"_kj);
  } else {
    KJ_FAIL_EXPECT("query parsing failed");
  }
}

// ============================================================================
// General Command Parser Tests
// ============================================================================

KJ_TEST("ParseCommand: Order") {
  auto result = parse_command("ORDER BUY BTCUSDT 0.5 50000.0 order001"_kj);

  KJ_EXPECT(result.type == CommandType::Order);
  KJ_IF_SOME (order, result.order) {
    // Order was parsed successfully
  } else {
    KJ_FAIL_EXPECT("order not found in parsed command");
  }
}

KJ_TEST("ParseCommand: Cancel") {
  auto result = parse_command("CANCEL order001"_kj);

  KJ_EXPECT(result.type == CommandType::Cancel);
  KJ_IF_SOME (cancel, result.cancel) {
    // Cancel was parsed successfully
  } else {
    KJ_FAIL_EXPECT("cancel not found in parsed command");
  }
}

KJ_TEST("ParseCommand: Query") {
  auto result = parse_command("QUERY account"_kj);

  KJ_EXPECT(result.type == CommandType::Query);
  KJ_IF_SOME (query, result.query) {
    // Query was parsed successfully
  } else {
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

} // namespace

} // namespace veloz::engine
