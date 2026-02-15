#include "kj/test.h"
#include "veloz/exec/client_order_id.h"

#include <kj/hash.h>
#include <kj/map.h>

namespace {

using namespace veloz::exec;

KJ_TEST("ClientOrderId: Generate basic") {
  ClientOrderIdGenerator gen("STRAT"_kj);

  auto id = gen.generate();

  KJ_EXPECT(id.size() > 0);
  KJ_EXPECT(id.startsWith("STRAT-"));
}

KJ_TEST("ClientOrderId: Multiple unique IDs") {
  ClientOrderIdGenerator gen("STRAT"_kj);

  // Use KJ HashSet to track unique IDs
  kj::HashSet<kj::String> ids;
  for (int i = 0; i < 100; ++i) {
    kj::String id = gen.generate();
    ids.upsert(kj::mv(id), [](kj::String&, kj::String&&) {});
  }

  KJ_EXPECT(ids.size() == 100);
}

KJ_TEST("ClientOrderId: Parse components") {
  kj::StringPtr id = "STRAT-1700000000-123-ABCXYZ"_kj;

  auto result = ClientOrderIdGenerator::parse(id);

  KJ_EXPECT(result.strategy == "STRAT");
  KJ_EXPECT(result.timestamp == 1700000000);
  KJ_EXPECT(result.unique == "123-ABCXYZ");
}

KJ_TEST("ClientOrderId: Parse without unique") {
  kj::StringPtr id = "STRAT-1700000000-123"_kj;

  auto result = ClientOrderIdGenerator::parse(id);

  KJ_EXPECT(result.strategy == "STRAT");
  KJ_EXPECT(result.timestamp == 1700000000);
  KJ_EXPECT(result.unique.size() == 0);
}

} // namespace
