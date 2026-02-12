#include "veloz/exec/client_order_id.h"

#include <gtest/gtest.h>
#include <unordered_set>

using namespace veloz::exec;

TEST(ClientOrderId, Generate) {
  ClientOrderIdGenerator gen("STRAT");

  std::string id1 = gen.generate();
  std::string id2 = gen.generate();

  EXPECT_FALSE(id1.empty());
  EXPECT_FALSE(id2.empty());
  EXPECT_NE(id1, id2); // Unique
  EXPECT_TRUE(id1.starts_with("STRAT-"));
}

TEST(ClientOrderId, HighThroughput) {
  ClientOrderIdGenerator gen("TEST");

  std::unordered_set<std::string> ids;
  for (int i = 0; i < 10000; ++i) {
    std::string id = gen.generate();
    EXPECT_FALSE(ids.contains(id)); // No duplicates
    ids.insert(id);
  }

  EXPECT_EQ(ids.size(), 10000);
}

TEST(ClientOrderId, ParseComponents) {
  std::string id = "STRAT-1700000000-123-ABC123";

  auto [strategy, timestamp, unique] = ClientOrderIdGenerator::parse(id);

  EXPECT_EQ(strategy, "STRAT");
  EXPECT_EQ(timestamp, 1700000000);
  EXPECT_EQ(unique, "ABC123");
}
