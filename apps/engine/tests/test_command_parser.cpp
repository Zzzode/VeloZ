#include <gtest/gtest.h>
#include "veloz/engine/command_parser.h"
#include <sstream>

namespace veloz::engine {

class CommandParserTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(CommandParserTest, ParseOrderCommandBuy) {
    std::string line = "ORDER BUY BTCUSDT 0.5 50000.0 order001";
    auto result = parse_order_command(line);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->request.symbol.value, "BTCUSDT");
    EXPECT_EQ(result->request.side, veloz::exec::OrderSide::Buy);
    EXPECT_DOUBLE_EQ(result->request.qty, 0.5);
    EXPECT_DOUBLE_EQ(result->request.price.value_or(0.0), 50000.0);
    EXPECT_EQ(result->request.client_order_id, "order001");
}

TEST_F(CommandParserTest, ParseOrderCommandSell) {
    std::string line = "ORDER SELL ETHUSDT 10.0 3000.0 order002";
    auto result = parse_order_command(line);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->request.symbol.value, "ETHUSDT");
    EXPECT_EQ(result->request.side, veloz::exec::OrderSide::Sell);
    EXPECT_DOUBLE_EQ(result->request.qty, 10.0);
    EXPECT_DOUBLE_EQ(result->request.price.value_or(0.0), 3000.0);
    EXPECT_EQ(result->request.client_order_id, "order002");
}

TEST_F(CommandParserTest, ParseOrderCommandBuyShortcut) {
    std::string line = "BUY BTCUSDT 0.5 50000.0 order003";
    auto result = parse_order_command(line);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->request.symbol.value, "BTCUSDT");
    EXPECT_EQ(result->request.side, veloz::exec::OrderSide::Buy);
}

TEST_F(CommandParserTest, ParseOrderCommandSellShortcut) {
    std::string line = "SELL ETHUSDT 10.0 3000.0 order004";
    auto result = parse_order_command(line);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->request.symbol.value, "ETHUSDT");
    EXPECT_EQ(result->request.side, veloz::exec::OrderSide::Sell);
}

TEST_F(CommandParserTest, ParseOrderCommandWithOrderType) {
    std::string line = "ORDER BUY BTCUSDT 0.5 50000.0 order005 MARKET GTC";
    auto result = parse_order_command(line);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->request.type, veloz::exec::OrderType::Market);
    EXPECT_EQ(result->request.tif, veloz::exec::TimeInForce::GTC);
}

TEST_F(CommandParserTest, ParseOrderCommandWithMarketType) {
    std::string line = "BUY BTCUSDT 0.5 0.0 order006 MARKET";
    auto result = parse_order_command(line);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->request.type, veloz::exec::OrderType::Market);
}

TEST_F(CommandParserTest, ParseOrderCommandWithIOCTif) {
    std::string line = "ORDER BUY BTCUSDT 0.5 50000.0 order007 LIMIT IOC";
    auto result = parse_order_command(line);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->request.tif, veloz::exec::TimeInForce::IOC);
}

TEST_F(CommandParserTest, ParseOrderCommandInvalidSide) {
    std::string line = "ORDER INVALID BTCUSDT 0.5 50000.0 order008";
    auto result = parse_order_command(line);

    EXPECT_FALSE(result.has_value());
}

TEST_F(CommandParserTest, ParseOrderCommandInvalidQuantity) {
    std::string line = "ORDER BUY BTCUSDT -0.5 50000.0 order009";
    auto result = parse_order_command(line);

    EXPECT_FALSE(result.has_value());
}

TEST_F(CommandParserTest, ParseOrderCommandInvalidPrice) {
    std::string line = "ORDER BUY BTCUSDT 0.5 -50000.0 order010";
    auto result = parse_order_command(line);

    EXPECT_FALSE(result.has_value());
}

TEST_F(CommandParserTest, ParseOrderCommandMissingClientId) {
    std::string line = "ORDER BUY BTCUSDT 0.5 50000.0";
    auto result = parse_order_command(line);

    EXPECT_FALSE(result.has_value());
}

TEST_F(CommandParserTest, ParseCancelCommand) {
    std::string line = "CANCEL order001";
    auto result = parse_cancel_command(line);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->client_order_id, "order001");
}

TEST_F(CommandParserTest, ParseCancelCommandShortcut) {
    std::string line = "C order002";
    auto result = parse_cancel_command(line);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->client_order_id, "order002");
}

TEST_F(CommandParserTest, ParseCancelCommandMissingId) {
    std::string line = "CANCEL";
    auto result = parse_cancel_command(line);

    EXPECT_FALSE(result.has_value());
}

TEST_F(CommandParserTest, ParseQueryCommand) {
    std::string line = "QUERY account";
    auto result = parse_query_command(line);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->query_type, "account");
    EXPECT_EQ(result->params, "");
}

TEST_F(CommandParserTest, ParseQueryCommandWithParams) {
    std::string line = "QUERY order BTCUSDT";
    auto result = parse_query_command(line);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->query_type, "order");
    EXPECT_EQ(result->params, "BTCUSDT");
}

TEST_F(CommandParserTest, ParseQueryCommandShortcut) {
    std::string line = "Q balance";
    auto result = parse_query_command(line);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->query_type, "balance");
}

TEST_F(CommandParserTest, ParseCommandOrder) {
    std::string line = "ORDER BUY BTCUSDT 0.5 50000.0 order001";
    auto result = parse_command(line);

    EXPECT_EQ(result.type, CommandType::Order);
    EXPECT_TRUE(result.order.has_value());
}

TEST_F(CommandParserTest, ParseCommandCancel) {
    std::string line = "CANCEL order001";
    auto result = parse_command(line);

    EXPECT_EQ(result.type, CommandType::Cancel);
    EXPECT_TRUE(result.cancel.has_value());
}

TEST_F(CommandParserTest, ParseCommandQuery) {
    std::string line = "QUERY account";
    auto result = parse_command(line);

    EXPECT_EQ(result.type, CommandType::Query);
    EXPECT_TRUE(result.query.has_value());
}

TEST_F(CommandParserTest, ParseCommandUnknown) {
    std::string line = "INVALID_COMMAND";
    auto result = parse_command(line);

    EXPECT_EQ(result.type, CommandType::Unknown);
    EXPECT_FALSE(result.error.empty());
}

TEST_F(CommandParserTest, ParseCommandEmptyLine) {
    std::string line = "";
    auto result = parse_command(line);

    EXPECT_EQ(result.type, CommandType::Unknown);
}

TEST_F(CommandParserTest, ParseCommandCommentLine) {
    std::string line = "# This is a comment";
    auto result = parse_command(line);

    EXPECT_EQ(result.type, CommandType::Unknown);
}

TEST_F(CommandParserTest, OrderSideToLowerCase) {
    EXPECT_EQ(parse_order_side("BUY"), veloz::exec::OrderSide::Buy);
    EXPECT_EQ(parse_order_side("buy"), veloz::exec::OrderSide::Buy);
    EXPECT_EQ(parse_order_side("Buy"), veloz::exec::OrderSide::Buy);
    EXPECT_EQ(parse_order_side("b"), veloz::exec::OrderSide::Buy);
    EXPECT_EQ(parse_order_side("SELL"), veloz::exec::OrderSide::Sell);
    EXPECT_EQ(parse_order_side("sell"), veloz::exec::OrderSide::Sell);
    EXPECT_EQ(parse_order_side("Sell"), veloz::exec::OrderSide::Sell);
    EXPECT_EQ(parse_order_side("s"), veloz::exec::OrderSide::Sell);
}

TEST_F(CommandParserTest, OrderTypeToLowerCase) {
    EXPECT_EQ(parse_order_type("LIMIT"), veloz::exec::OrderType::Limit);
    EXPECT_EQ(parse_order_type("limit"), veloz::exec::OrderType::Limit);
    EXPECT_EQ(parse_order_type("l"), veloz::exec::OrderType::Limit);
    EXPECT_EQ(parse_order_type("MARKET"), veloz::exec::OrderType::Market);
    EXPECT_EQ(parse_order_type("market"), veloz::exec::OrderType::Market);
    EXPECT_EQ(parse_order_type("m"), veloz::exec::OrderType::Market);
}

TEST_F(CommandParserTest, TifToLowerCase) {
    EXPECT_EQ(parse_tif("GTC"), veloz::exec::TimeInForce::GTC);
    EXPECT_EQ(parse_tif("gtc"), veloz::exec::TimeInForce::GTC);
    EXPECT_EQ(parse_tif("g"), veloz::exec::TimeInForce::GTC);
    EXPECT_EQ(parse_tif("IOC"), veloz::exec::TimeInForce::IOC);
    EXPECT_EQ(parse_tif("ioc"), veloz::exec::TimeInForce::IOC);
    EXPECT_EQ(parse_tif("FOK"), veloz::exec::TimeInForce::FOK);
    EXPECT_EQ(parse_tif("fok"), veloz::exec::TimeInForce::FOK);
    EXPECT_EQ(parse_tif("GTX"), veloz::exec::TimeInForce::GTX);
    EXPECT_EQ(parse_tif("gtx"), veloz::exec::TimeInForce::GTX);
}

TEST_F(CommandParserTest, IsValidOrderSide) {
    EXPECT_TRUE(is_valid_order_side("BUY"));
    EXPECT_TRUE(is_valid_order_side("buy"));
    EXPECT_TRUE(is_valid_order_side("b"));
    EXPECT_TRUE(is_valid_order_side("SELL"));
    EXPECT_TRUE(is_valid_order_side("sell"));
    EXPECT_TRUE(is_valid_order_side("s"));
    EXPECT_FALSE(is_valid_order_side("INVALID"));
}

TEST_F(CommandParserTest, IsValidOrderType) {
    EXPECT_TRUE(is_valid_order_type("LIMIT"));
    EXPECT_TRUE(is_valid_order_type("limit"));
    EXPECT_TRUE(is_valid_order_type("l"));
    EXPECT_TRUE(is_valid_order_type("MARKET"));
    EXPECT_TRUE(is_valid_order_type("market"));
    EXPECT_TRUE(is_valid_order_type("m"));
    EXPECT_FALSE(is_valid_order_type("INVALID"));
}

TEST_F(CommandParserTest, IsValidTif) {
    EXPECT_TRUE(is_valid_tif("GTC"));
    EXPECT_TRUE(is_valid_tif("gtc"));
    EXPECT_TRUE(is_valid_tif("g"));
    EXPECT_TRUE(is_valid_tif("IOC"));
    EXPECT_TRUE(is_valid_tif("ioc"));
    EXPECT_TRUE(is_valid_tif("FOK"));
    EXPECT_TRUE(is_valid_tif("fok"));
    EXPECT_TRUE(is_valid_tif("GTX"));
    EXPECT_TRUE(is_valid_tif("gtx"));
    EXPECT_FALSE(is_valid_tif("INVALID"));
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

} // namespace veloz::engine
