#include "kj/test.h"
#include "veloz/market/binance_rest_client.h"

#include <kj/async-io.h>
#include <kj/string.h>

namespace {

using namespace veloz::market;

KJ_TEST("BinanceRestClient: Construction") {
  auto io = kj::setupAsyncIo();

  // Test mainnet construction
  BinanceRestClient mainnetClient(io, false);
  KJ_EXPECT(mainnetClient.is_connected());

  // Test testnet construction
  BinanceRestClient testnetClient(io, true);
  KJ_EXPECT(testnetClient.is_connected());
}

KJ_TEST("BinanceRestClient: Destructor does not crash") {
  auto io = kj::setupAsyncIo();
  {
    BinanceRestClient client(io, true);
    // Client goes out of scope
  }
  // If we get here without crashing, the test passes
  KJ_EXPECT(true);
}

// Note: Integration tests that make actual network calls should be in a separate
// test file that can be run manually or in CI with network access.
// The tests below verify the client can be constructed and configured properly.

KJ_TEST("BinanceRestClient: Multiple clients can coexist") {
  auto io = kj::setupAsyncIo();

  BinanceRestClient client1(io, false);
  BinanceRestClient client2(io, true);

  KJ_EXPECT(client1.is_connected());
  KJ_EXPECT(client2.is_connected());
}

} // namespace
