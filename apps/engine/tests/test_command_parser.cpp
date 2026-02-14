#include "veloz/engine/command_parser.h"

#include <gtest/gtest.h>
#include <kj/common.h>
#include <sstream>

namespace veloz::engine {

// Helper to check if kj::Maybe has a value
template <typename T> bool has_value(const kj::Maybe<T>& maybe) {
  bool result = false;
  KJ_IF_MAYBE (val, maybe) {
    (void)val;
    result = true;
  }
  return result;
}

// Helper to extract value from kj::Maybe with a default
template <typename T> T maybe_or(const kj::Maybe<T>& maybe, T default_value) {
  T result = default_value;
  KJ_IF_MAYBE (val, maybe) {
    result = *val;
  }
  return result;
}

class CommandParserTest : public ::testing::Test {
protected:
  void SetUp() override {}
  void TearDown() override {}
};

TEST_F(CommandParserTest, ParseOrderCommandBuy) {
  auto result = parse_order_command("ORDER BUY BTCUSDT 0.5 50000.0 order001"_kj);

  ASSERT_TRUE(has_value(result));
  KJ_IF_MAYBE (order, result) {
    EXPECT_EQ(order->request.symbol.value, "BTCUSDT");
    EXPECT_EQ(order->request.side, veloz::exec::OrderSide::Buy);
    EXPECT_DOUBLE_EQ(order->request.qty, 0.5);
    EXPECT_DOUBLE_EQ(maybe_or(order->request.price, 0.0), 50000.0);
    EXPECT_EQ(order->request.client_order_id, "order001");
  }
}

TEST_F(CommandParserTest, ParseOrderCommandSell) {
  auto result = parse_order_command("ORDER SELL ETHUSDT 10.0 3000.0 order002"_kj);

  ASSERT_TRUE(has_value(result));
  KJ_IF_MAYBE (order, result) {
    EXPECT_EQ(order->request.symbol.value, "ETHUSDT");
    EXPECT_EQ(order->request.side, veloz::exec::OrderSide::Sell);
    EXPECT_DOUBLE_EQ(order->request.qty, 10.0);
    EXPECT_DOUBLE_EQ(maybe_or(order->request.price, 0.0), 3000.0);
    EXPECT_EQ(order->request.client_order_id, "order002");
  }
}

TEST_F(CommandParserTest, ParseOrderCommandBuyShortcut) {
  auto result = parse_order_command("BUY BTCUSDT 0.5 50000.0 order003"_kj);

  ASSERT_TRUE(has_value(result));
  KJ_IF_MAYBE (order, result) {
    EXPECT_EQ(order->request.symbol.value, "BTCUSDT");
    EXPECT_EQ(order->request.side, veloz::exec::OrderSide::Buy);
  }
}

TEST_F(CommandParserTest, ParseOrderCommandSellShortcut) {
  auto result = parse_order_command("SELL ETHUSDT 10.0 3000.0 order004"_kj);

  ASSERT_TRUE(has_value(result));
  KJ_IF_MAYBE (order, result) {
    EXPECT_EQ(order->request.symbol.value, "ETHUSDT");
    EXPECT_EQ(order->request.side, veloz::exec::OrderSide::Sell);
  }
}

TEST_F(CommandParserTest, ParseOrderCommandWithOrderType) {
  auto result = parse_order_command("ORDER BUY BTCUSDT 0.5 50000.0 order005 MARKET GTC"_kj);

  ASSERT_TRUE(has_value(result));
  KJ_IF_MAYBE (order, result) {
    EXPECT_EQ(order->request.type, veloz::exec::OrderType::Market);
    EXPECT_EQ(order->request.tif, veloz::exec::TimeInForce::GTC);
  }
}

TEST_F(CommandParserTest, ParseOrderCommandWithMarketType) {
  auto result = parse_order_command("BUY BTCUSDT 0.5 0.0 order006 MARKET"_kj);

  ASSERT_TRUE(has_value(result));
  KJ_IF_MAYBE (order, result) {
    EXPECT_EQ(order->request.type, veloz::exec::OrderType::Market);
  }
}

TEST_F(CommandParserTest, ParseOrderCommandWithIOCTif) {
  auto result = parse_order_command("ORDER BUY BTCUSDT 0.5 50000.0 order007 LIMIT IOC"_kj);

  ASSERT_TRUE(has_value(result));
  KJ_IF_MAYBE (order, result) {
    EXPECT_EQ(order->request.tif, veloz::exec::TimeInForce::IOC);
  }
}

TEST_F(CommandParserTest, ParseOrderCommandInvalidSide) {
  auto result = parse_order_command("ORDER INVALID BTCUSDT 0.5 50000.0 order008"_kj);

  EXPECT_FALSE(has_value(result));
}

TEST_F(CommandParserTest, ParseOrderCommandInvalidQuantity) {
  auto result = parse_order_command("ORDER BUY BTCUSDT -0.5 50000.0 order009"_kj);

  EXPECT_FALSE(has_value(result));
}

TEST_F(CommandParserTest, ParseOrderCommandInvalidPrice) {
  auto result = parse_order_command("ORDER BUY BTCUSDT 0.5 -50000.0 order010"_kj);

  EXPECT_FALSE(has_value(result));
}

TEST_F(CommandParserTest, ParseOrderCommandMissingClientId) {
  auto result = parse_order_command("ORDER BUY BTCUSDT 0.5 50000.0"_kj);

  EXPECT_FALSE(has_value(result));
}

TEST_F(CommandParserTest, ParseCancelCommand) {
  auto result = parse_cancel_command("CANCEL order001"_kj);

  ASSERT_TRUE(has_value(result));
  KJ_IF_MAYBE (cancel, result) {
    EXPECT_EQ(cancel->client_order_id, "order001"_kj);
  }
}

TEST_F(CommandParserTest, ParseCancelCommandShortcut) {
  auto result = parse_cancel_command("C order002"_kj);

  ASSERT_TRUE(has_value(result));
  KJ_IF_MAYBE (cancel, result) {
    EXPECT_EQ(cancel->client_order_id, "order002"_kj);
  }
}

TEST_F(CommandParserTest, ParseCancelCommandMissingId) {
  auto result = parse_cancel_command("CANCEL"_kj);

  EXPECT_FALSE(has_value(result));
}

TEST_F(CommandParserTest, ParseQueryCommand) {
  auto result = parse_query_command("QUERY account"_kj);

  ASSERT_TRUE(has_value(result));
  KJ_IF_MAYBE (query, result) {
    EXPECT_EQ(query->query_type, "account"_kj);
    EXPECT_EQ(query->params, ""_kj);
  }
}

TEST_F(CommandParserTest, ParseQueryCommandWithParams) {
  auto result = parse_query_command("QUERY order BTCUSDT"_kj);

  ASSERT_TRUE(has_value(result));
  KJ_IF_MAYBE (query, result) {
    EXPECT_EQ(query->query_type, "order"_kj);
    EXPECT_EQ(query->params, "BTCUSDT"_kj);
  }
}

TEST_F(CommandParserTest, ParseQueryCommandShortcut) {
  auto result = parse_query_command("Q balance"_kj);

  ASSERT_TRUE(has_value(result));
  KJ_IF_MAYBE (query, result) {
    EXPECT_EQ(query->query_type, "balance"_kj);
  }
}

TEST_F(CommandParserTest, ParseCommandOrder) {
  auto result = parse_command("ORDER BUY BTCUSDT 0.5 50000.0 order001"_kj);

  EXPECT_EQ(result.type, CommandType::Order);
  EXPECT_TRUE(has_value(result.order));
}

TEST_F(CommandParserTest, ParseCommandCancel) {
  auto result = parse_command("CANCEL order001"_kj);

  EXPECT_EQ(result.type, CommandType::Cancel);
  EXPECT_TRUE(has_value(result.cancel));
}

TEST_F(CommandParserTest, ParseCommandQuery) {
  auto result = parse_command("QUERY account"_kj);

  EXPECT_EQ(result.type, CommandType::Query);
  EXPECT_TRUE(has_value(result.query));
}

TEST_F(CommandParserTest, ParseCommandUnknown) {
  auto result = parse_command("INVALID_COMMAND"_kj);

  EXPECT_EQ(result.type, CommandType::Unknown);
  EXPECT_TRUE(result.error.size() > 0);
}

TEST_F(CommandParserTest, ParseCommandEmptyLine) {
  auto result = parse_command(""_kj);

  EXPECT_EQ(result.type, CommandType::Unknown);
}

TEST_F(CommandParserTest, ParseCommandCommentLine) {
  auto result = parse_command("# This is a comment"_kj);

  EXPECT_EQ(result.type, CommandType::Unknown);
}

TEST_F(CommandParserTest, OrderSideToLowerCase) {
  EXPECT_EQ(parse_order_side("BUY"_kj), veloz::exec::OrderSide::Buy);
  EXPECT_EQ(parse_order_side("buy"_kj), veloz::exec::OrderSide::Buy);
  EXPECT_EQ(parse_order_side("Buy"_kj), veloz::exec::OrderSide::Buy);
  EXPECT_EQ(parse_order_side("b"_kj), veloz::exec::OrderSide::Buy);
  EXPECT_EQ(parse_order_side("SELL"_kj), veloz::exec::OrderSide::Sell);
  EXPECT_EQ(parse_order_side("sell"_kj), veloz::exec::OrderSide::Sell);
  EXPECT_EQ(parse_order_side("Sell"_kj), veloz::exec::OrderSide::Sell);
  EXPECT_EQ(parse_order_side("s"_kj), veloz::exec::OrderSide::Sell);
}

TEST_F(CommandParserTest, OrderTypeToLowerCase) {
  EXPECT_EQ(parse_order_type("LIMIT"_kj), veloz::exec::OrderType::Limit);
  EXPECT_EQ(parse_order_type("limit"_kj), veloz::exec::OrderType::Limit);
  EXPECT_EQ(parse_order_type("l"_kj), veloz::exec::OrderType::Limit);
  EXPECT_EQ(parse_order_type("MARKET"_kj), veloz::exec::OrderType::Market);
  EXPECT_EQ(parse_order_type("market"_kj), veloz::exec::OrderType::Market);
  EXPECT_EQ(parse_order_type("m"_kj), veloz::exec::OrderType::Market);
}

TEST_F(CommandParserTest, TifToLowerCase) {
  EXPECT_EQ(parse_tif("GTC"_kj), veloz::exec::TimeInForce::GTC);
  EXPECT_EQ(parse_tif("gtc"_kj), veloz::exec::TimeInForce::GTC);
  EXPECT_EQ(parse_tif("g"_kj), veloz::exec::TimeInForce::GTC);
  EXPECT_EQ(parse_tif("IOC"_kj), veloz::exec::TimeInForce::IOC);
  EXPECT_EQ(parse_tif("ioc"_kj), veloz::exec::TimeInForce::IOC);
  EXPECT_EQ(parse_tif("FOK"_kj), veloz::exec::TimeInForce::FOK);
  EXPECT_EQ(parse_tif("fok"_kj), veloz::exec::TimeInForce::FOK);
  EXPECT_EQ(parse_tif("GTX"_kj), veloz::exec::TimeInForce::GTX);
  EXPECT_EQ(parse_tif("gtx"_kj), veloz::exec::TimeInForce::GTX);
}

TEST_F(CommandParserTest, IsValidOrderSide) {
  EXPECT_TRUE(is_valid_order_side("BUY"_kj));
  EXPECT_TRUE(is_valid_order_side("buy"_kj));
  EXPECT_TRUE(is_valid_order_side("b"_kj));
  EXPECT_TRUE(is_valid_order_side("SELL"_kj));
  EXPECT_TRUE(is_valid_order_side("sell"_kj));
  EXPECT_TRUE(is_valid_order_side("s"_kj));
  EXPECT_FALSE(is_valid_order_side("INVALID"_kj));
}

TEST_F(CommandParserTest, IsValidOrderType) {
  EXPECT_TRUE(is_valid_order_type("LIMIT"_kj));
  EXPECT_TRUE(is_valid_order_type("limit"_kj));
  EXPECT_TRUE(is_valid_order_type("l"_kj));
  EXPECT_TRUE(is_valid_order_type("MARKET"_kj));
  EXPECT_TRUE(is_valid_order_type("market"_kj));
  EXPECT_TRUE(is_valid_order_type("m"_kj));
  EXPECT_FALSE(is_valid_order_type("INVALID"_kj));
}

TEST_F(CommandParserTest, IsValidTif) {
  EXPECT_TRUE(is_valid_tif("GTC"_kj));
  EXPECT_TRUE(is_valid_tif("gtc"_kj));
  EXPECT_TRUE(is_valid_tif("g"_kj));
  EXPECT_TRUE(is_valid_tif("IOC"_kj));
  EXPECT_TRUE(is_valid_tif("ioc"_kj));
  EXPECT_TRUE(is_valid_tif("FOK"_kj));
  EXPECT_TRUE(is_valid_tif("fok"_kj));
  EXPECT_TRUE(is_valid_tif("GTX"_kj));
  EXPECT_TRUE(is_valid_tif("gtx"_kj));
  EXPECT_FALSE(is_valid_tif("INVALID"_kj));
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

} // namespace veloz::engine
