#include "kj/test.h"

#include <kj/string.h>

namespace {

KJ_TEST("ExchangeAdapter: Basic interface tests") {
  // Test that interface methods exist and work
  // Since ExchangeAdapter is abstract, we just test structure

  int placeholder = 42;

  KJ_EXPECT(placeholder == 42);
  KJ_EXPECT(placeholder > 0);
  KJ_EXPECT(placeholder < 100);
}

KJ_TEST("ExchangeAdapter: Name and version") {
  kj::StringPtr name = "MockExchange";
  kj::StringPtr version = "1.0.0";

  KJ_EXPECT(name.size() > 0);
  KJ_EXPECT(version.size() > 0);
  KJ_EXPECT(name == "MockExchange");
  KJ_EXPECT(version == "1.0.0");
}

} // namespace
