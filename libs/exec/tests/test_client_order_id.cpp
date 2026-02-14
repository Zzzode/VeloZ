#include "kj/test.h"
#include "veloz/exec/client_order_id.h"

#include <string>
#include <unordered_set>

namespace {

using namespace veloz::exec;

KJ_TEST("ClientOrderId: Generate basic") {
  ClientOrderIdGenerator gen("STRAT");

  auto id = gen.generate();

  KJ_EXPECT(id.size() > 0);
  KJ_EXPECT(id.startsWith("STRAT-"));
}

KJ_TEST("ClientOrderId: Multiple unique IDs") {
  ClientOrderIdGenerator gen("STRAT");

  std::unordered_set<std::string> ids;
  for (int i = 0; i < 100; ++i) {
    kj::String id = gen.generate();
    ids.insert(std::string(id.cStr()));
  }

  KJ_EXPECT(ids.size() == 100);
}

KJ_TEST("ClientOrderId: Parse components") {
  kj::StringPtr id = "STRAT-1700000000-123-ABCXYZ";

  auto [strategy, timestamp, unique] = ClientOrderIdGenerator::parse(id);

  KJ_EXPECT(strategy == "STRAT");
  KJ_EXPECT(timestamp == 1700000000);
  KJ_EXPECT(unique == "123-ABCXYZ");
}

KJ_TEST("ClientOrderId: Parse without unique") {
  kj::StringPtr id = "STRAT-1700000000-123";

  auto [strategy, timestamp, unique] = ClientOrderIdGenerator::parse(id);

  KJ_EXPECT(strategy == "STRAT");
  KJ_EXPECT(timestamp == 1700000000);
  KJ_EXPECT(unique.size() == 0);
}

} // namespace
