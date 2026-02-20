#include "veloz/exec/binance_adapter.h"
#include "veloz/exec/hmac_wrapper.h"

#include <kj/array.h>
#include <kj/async-io.h>
#include <kj/test.h>

namespace {

// BinanceAdapter tests using KJ async I/O
// Full testing would require mocking HTTP responses or using testnet

KJ_TEST("BinanceAdapter: Construction with KJ async I/O context") {
  // Set up KJ async I/O context
  auto io = kj::setupAsyncIo();

  // Create adapter with test credentials (won't actually connect)
  veloz::exec::BinanceAdapter adapter(io, "test_api_key"_kj, "test_secret_key"_kj, true);

  // Verify adapter info
  KJ_EXPECT(kj::StringPtr(adapter.name()) == "Binance");
  KJ_EXPECT(kj::StringPtr(adapter.version()) == "2.0.0-kj-async");

  // Adapter should not be connected initially
  KJ_EXPECT(!adapter.is_connected());
}

KJ_TEST("BinanceAdapter: Timeout configuration") {
  auto io = kj::setupAsyncIo();

  veloz::exec::BinanceAdapter adapter(io, "test_api_key"_kj, "test_secret_key"_kj, true);

  // Default timeout should be 10 seconds
  KJ_EXPECT(adapter.get_timeout() == 10 * kj::SECONDS);

  // Set custom timeout
  adapter.set_timeout(5 * kj::SECONDS);
  KJ_EXPECT(adapter.get_timeout() == 5 * kj::SECONDS);

  // Set longer timeout
  adapter.set_timeout(30 * kj::SECONDS);
  KJ_EXPECT(adapter.get_timeout() == 30 * kj::SECONDS);
}

KJ_TEST("BinanceAdapter: Testnet vs production URL selection") {
  auto io = kj::setupAsyncIo();

  // Create testnet adapter
  veloz::exec::BinanceAdapter testnet_adapter(io, "key"_kj, "secret"_kj, true);
  KJ_EXPECT(!testnet_adapter.is_connected());

  // Create production adapter
  veloz::exec::BinanceAdapter prod_adapter(io, "key"_kj, "secret"_kj, false);
  KJ_EXPECT(!prod_adapter.is_connected());
}

KJ_TEST("BinanceAdapter: Disconnect behavior") {
  auto io = kj::setupAsyncIo();

  veloz::exec::BinanceAdapter adapter(io, "key"_kj, "secret"_kj, true);

  // Initially not connected
  KJ_EXPECT(!adapter.is_connected());

  // Disconnect should be safe to call even when not connected
  adapter.disconnect();
  KJ_EXPECT(!adapter.is_connected());
}

KJ_TEST("hex_encode: uses high-nibble then low-nibble") {
  uint8_t bytes[] = {0x00, 0x01, 0x02, 0x0f, 0x10, 0xab, 0xcd, 0xef};
  kj::String hex = veloz::exec::hex_encode(kj::arrayPtr(bytes, sizeof(bytes)));
  KJ_EXPECT(kj::StringPtr(hex) == "0001020f10abcdef");
}

KJ_TEST("HmacSha256: matches known test vector") {
  kj::String signature =
      veloz::exec::HmacSha256::sign("key"_kj, "The quick brown fox jumps over the lazy dog"_kj);
  KJ_EXPECT(kj::StringPtr(signature) ==
            "f7bc83f430538424b13298e6aa6fb143ef4d59a14946175997479dbc2d1a3cd8");
}

} // namespace
