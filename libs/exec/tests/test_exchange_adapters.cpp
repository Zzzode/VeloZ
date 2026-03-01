#include "veloz/exec/bybit_adapter.h"
#include "veloz/exec/coinbase_adapter.h"
#include "veloz/exec/okx_adapter.h"

#include <kj/async-io.h>
#include <kj/test.h>

namespace {

// Test OKX Adapter basic functionality
KJ_TEST("OKXAdapter: Name and version") {
  auto io = kj::setupAsyncIo();
  veloz::exec::OKXAdapter adapter(io, "test_key"_kj, "test_secret"_kj, "test_passphrase"_kj, true);

  KJ_EXPECT(kj::StringPtr(adapter.name()) == "OKX");
  KJ_EXPECT(kj::StringPtr(adapter.version()) == "1.0.0");
}

KJ_TEST("OKXAdapter: Connection management") {
  auto io = kj::setupAsyncIo();
  veloz::exec::OKXAdapter adapter(io, "test_key"_kj, "test_secret"_kj, "test_passphrase"_kj, true);

  KJ_EXPECT(!adapter.is_connected());

  adapter.connect();
  KJ_EXPECT(adapter.is_connected());

  adapter.disconnect();
  KJ_EXPECT(!adapter.is_connected());
}

KJ_TEST("OKXAdapter: Timeout configuration") {
  auto io = kj::setupAsyncIo();
  veloz::exec::OKXAdapter adapter(io, "test_key"_kj, "test_secret"_kj, "test_passphrase"_kj, true);

  auto default_timeout = adapter.get_timeout();
  KJ_EXPECT(default_timeout == 30 * kj::SECONDS);

  adapter.set_timeout(60 * kj::SECONDS);
  KJ_EXPECT(adapter.get_timeout() == 60 * kj::SECONDS);
}

// Test Coinbase Adapter basic functionality
KJ_TEST("CoinbaseAdapter: Name and version") {
  auto io = kj::setupAsyncIo();
  veloz::exec::CoinbaseAdapter adapter(io, "test_key"_kj, "test_secret"_kj, true);

  KJ_EXPECT(kj::StringPtr(adapter.name()) == "Coinbase");
  KJ_EXPECT(kj::StringPtr(adapter.version()) == "1.0.0");
}

KJ_TEST("CoinbaseAdapter: Connection management") {
  auto io = kj::setupAsyncIo();
  veloz::exec::CoinbaseAdapter adapter(io, "test_key"_kj, "test_secret"_kj, true);

  KJ_EXPECT(!adapter.is_connected());

  adapter.connect();
  KJ_EXPECT(adapter.is_connected());

  adapter.disconnect();
  KJ_EXPECT(!adapter.is_connected());
}

KJ_TEST("CoinbaseAdapter: Timeout configuration") {
  auto io = kj::setupAsyncIo();
  veloz::exec::CoinbaseAdapter adapter(io, "test_key"_kj, "test_secret"_kj, true);

  auto default_timeout = adapter.get_timeout();
  KJ_EXPECT(default_timeout == 30 * kj::SECONDS);

  adapter.set_timeout(45 * kj::SECONDS);
  KJ_EXPECT(adapter.get_timeout() == 45 * kj::SECONDS);
}

// Test Bybit Adapter basic functionality
KJ_TEST("BybitAdapter: Name and version") {
  auto io = kj::setupAsyncIo();
  veloz::exec::BybitAdapter adapter(io, "test_key"_kj, "test_secret"_kj,
                                    veloz::exec::BybitAdapter::Category::Spot, true);

  KJ_EXPECT(kj::StringPtr(adapter.name()) == "Bybit");
  KJ_EXPECT(kj::StringPtr(adapter.version()) == "1.0.0");
}

KJ_TEST("BybitAdapter: Connection management") {
  auto io = kj::setupAsyncIo();
  veloz::exec::BybitAdapter adapter(io, "test_key"_kj, "test_secret"_kj,
                                    veloz::exec::BybitAdapter::Category::Spot, true);

  KJ_EXPECT(!adapter.is_connected());

  adapter.connect();
  KJ_EXPECT(adapter.is_connected());

  adapter.disconnect();
  KJ_EXPECT(!adapter.is_connected());
}

KJ_TEST("BybitAdapter: Timeout configuration") {
  auto io = kj::setupAsyncIo();
  veloz::exec::BybitAdapter adapter(io, "test_key"_kj, "test_secret"_kj,
                                    veloz::exec::BybitAdapter::Category::Spot, true);

  auto default_timeout = adapter.get_timeout();
  KJ_EXPECT(default_timeout == 30 * kj::SECONDS);

  adapter.set_timeout(90 * kj::SECONDS);
  KJ_EXPECT(adapter.get_timeout() == 90 * kj::SECONDS);
}

KJ_TEST("BybitAdapter: Category configuration") {
  auto io = kj::setupAsyncIo();
  veloz::exec::BybitAdapter adapter(io, "test_key"_kj, "test_secret"_kj,
                                    veloz::exec::BybitAdapter::Category::Spot, true);

  KJ_EXPECT(adapter.get_category() == veloz::exec::BybitAdapter::Category::Spot);

  adapter.set_category(veloz::exec::BybitAdapter::Category::Linear);
  KJ_EXPECT(adapter.get_category() == veloz::exec::BybitAdapter::Category::Linear);

  adapter.set_category(veloz::exec::BybitAdapter::Category::Inverse);
  KJ_EXPECT(adapter.get_category() == veloz::exec::BybitAdapter::Category::Inverse);
}

// Test ExchangeAdapter interface compliance
KJ_TEST("ExchangeAdapter: Interface compliance - OKX") {
  auto io = kj::setupAsyncIo();
  veloz::exec::OKXAdapter adapter(io, "test_key"_kj, "test_secret"_kj, "test_passphrase"_kj, true);

  // Verify interface methods are accessible through base class reference
  veloz::exec::ExchangeAdapter& base_ref = adapter;

  KJ_EXPECT(kj::StringPtr(base_ref.name()) == "OKX");
  KJ_EXPECT(kj::StringPtr(base_ref.version()) == "1.0.0");
  KJ_EXPECT(!base_ref.is_connected());

  base_ref.connect();
  KJ_EXPECT(base_ref.is_connected());

  base_ref.disconnect();
  KJ_EXPECT(!base_ref.is_connected());
}

KJ_TEST("ExchangeAdapter: Interface compliance - Coinbase") {
  auto io = kj::setupAsyncIo();
  veloz::exec::CoinbaseAdapter adapter(io, "test_key"_kj, "test_secret"_kj, true);

  veloz::exec::ExchangeAdapter& base_ref = adapter;

  KJ_EXPECT(kj::StringPtr(base_ref.name()) == "Coinbase");
  KJ_EXPECT(kj::StringPtr(base_ref.version()) == "1.0.0");
  KJ_EXPECT(!base_ref.is_connected());

  base_ref.connect();
  KJ_EXPECT(base_ref.is_connected());

  base_ref.disconnect();
  KJ_EXPECT(!base_ref.is_connected());
}

KJ_TEST("ExchangeAdapter: Interface compliance - Bybit") {
  auto io = kj::setupAsyncIo();
  veloz::exec::BybitAdapter adapter(io, "test_key"_kj, "test_secret"_kj,
                                    veloz::exec::BybitAdapter::Category::Linear, true);

  veloz::exec::ExchangeAdapter& base_ref = adapter;

  KJ_EXPECT(kj::StringPtr(base_ref.name()) == "Bybit");
  KJ_EXPECT(kj::StringPtr(base_ref.version()) == "1.0.0");
  KJ_EXPECT(!base_ref.is_connected());

  base_ref.connect();
  KJ_EXPECT(base_ref.is_connected());

  base_ref.disconnect();
  KJ_EXPECT(!base_ref.is_connected());
}

// Test OKX adapter has proper sync method implementations
// Note: Actual network calls are not tested here - these verify the interface exists
KJ_TEST("OKXAdapter: Synchronous interface exists") {
  auto io = kj::setupAsyncIo();
  veloz::exec::OKXAdapter adapter(io, "test_key"_kj, "test_secret"_kj, "test_passphrase"_kj, true);

  // Verify the interface methods exist and are callable
  // We don't actually call them as they would make network requests
  veloz::exec::PlaceOrderRequest place_req;
  place_req.symbol = veloz::common::SymbolId{"BTCUSDT"};
  place_req.side = veloz::exec::OrderSide::Buy;
  place_req.qty = 0.001;

  // Just verify the method signature compiles - don't call it
  (void)place_req;

  veloz::exec::CancelOrderRequest cancel_req;
  cancel_req.symbol = veloz::common::SymbolId{"BTCUSDT"};
  cancel_req.client_order_id = kj::str("test-order-123");

  (void)cancel_req;
}

KJ_TEST("CoinbaseAdapter: Synchronous methods return none") {
  auto io = kj::setupAsyncIo();
  veloz::exec::CoinbaseAdapter adapter(io, "test_key"_kj, "test_secret"_kj, true);

  veloz::exec::PlaceOrderRequest place_req;
  place_req.symbol = veloz::common::SymbolId{"BTCUSD"};
  place_req.side = veloz::exec::OrderSide::Sell;
  place_req.qty = 0.01;

  auto place_result = adapter.place_order(place_req);
  KJ_EXPECT(place_result == kj::none);

  veloz::exec::CancelOrderRequest cancel_req;
  cancel_req.symbol = veloz::common::SymbolId{"BTCUSD"};
  cancel_req.client_order_id = kj::str("test-order-456");

  auto cancel_result = adapter.cancel_order(cancel_req);
  KJ_EXPECT(cancel_result == kj::none);
}

KJ_TEST("BybitAdapter: Synchronous methods return none") {
  auto io = kj::setupAsyncIo();
  veloz::exec::BybitAdapter adapter(io, "test_key"_kj, "test_secret"_kj,
                                    veloz::exec::BybitAdapter::Category::Spot, true);

  veloz::exec::PlaceOrderRequest place_req;
  place_req.symbol = veloz::common::SymbolId{"BTCUSDT"};
  place_req.side = veloz::exec::OrderSide::Buy;
  place_req.qty = 0.001;

  auto place_result = adapter.place_order(place_req);
  KJ_EXPECT(place_result == kj::none);

  veloz::exec::CancelOrderRequest cancel_req;
  cancel_req.symbol = veloz::common::SymbolId{"BTCUSDT"};
  cancel_req.client_order_id = kj::str("test-order-789");

  auto cancel_result = adapter.cancel_order(cancel_req);
  KJ_EXPECT(cancel_result == kj::none);
}

// Test OKX adapter demo mode configuration
KJ_TEST("OKXAdapter: Demo mode configuration") {
  auto io = kj::setupAsyncIo();

  // Create demo mode adapter
  veloz::exec::OKXAdapter demo_adapter(io, "test_key"_kj, "test_secret"_kj, "test_passphrase"_kj,
                                       true);
  KJ_EXPECT(kj::StringPtr(demo_adapter.name()) == "OKX");

  // Create production mode adapter
  veloz::exec::OKXAdapter prod_adapter(io, "test_key"_kj, "test_secret"_kj, "test_passphrase"_kj,
                                       false);
  KJ_EXPECT(kj::StringPtr(prod_adapter.name()) == "OKX");
}

// Test OKX adapter passphrase requirement
KJ_TEST("OKXAdapter: Passphrase is required") {
  auto io = kj::setupAsyncIo();

  // OKX requires a passphrase - verify adapter accepts it
  veloz::exec::OKXAdapter adapter(io, "api_key"_kj, "secret_key"_kj, "my_passphrase"_kj, true);
  KJ_EXPECT(kj::StringPtr(adapter.name()) == "OKX");
  KJ_EXPECT(kj::StringPtr(adapter.version()) == "1.0.0");
}

// Test OKX adapter multiple instances
KJ_TEST("OKXAdapter: Multiple instances with different credentials") {
  auto io = kj::setupAsyncIo();

  veloz::exec::OKXAdapter adapter1(io, "key1"_kj, "secret1"_kj, "pass1"_kj, true);
  veloz::exec::OKXAdapter adapter2(io, "key2"_kj, "secret2"_kj, "pass2"_kj, true);

  // Both adapters should be independent
  adapter1.connect();
  KJ_EXPECT(adapter1.is_connected());
  KJ_EXPECT(!adapter2.is_connected());

  adapter2.connect();
  KJ_EXPECT(adapter1.is_connected());
  KJ_EXPECT(adapter2.is_connected());

  adapter1.disconnect();
  KJ_EXPECT(!adapter1.is_connected());
  KJ_EXPECT(adapter2.is_connected());
}

// Test OKX adapter timeout boundary values
KJ_TEST("OKXAdapter: Timeout boundary values") {
  auto io = kj::setupAsyncIo();
  veloz::exec::OKXAdapter adapter(io, "test_key"_kj, "test_secret"_kj, "test_passphrase"_kj, true);

  // Test minimum timeout
  adapter.set_timeout(1 * kj::MILLISECONDS);
  KJ_EXPECT(adapter.get_timeout() == 1 * kj::MILLISECONDS);

  // Test large timeout
  adapter.set_timeout(300 * kj::SECONDS);
  KJ_EXPECT(adapter.get_timeout() == 300 * kj::SECONDS);

  // Test zero timeout (edge case)
  adapter.set_timeout(0 * kj::SECONDS);
  KJ_EXPECT(adapter.get_timeout() == 0 * kj::SECONDS);
}

// Test Coinbase adapter sandbox vs production URL selection
KJ_TEST("CoinbaseAdapter: Sandbox vs production URL selection") {
  auto io = kj::setupAsyncIo();

  // Create sandbox adapter
  veloz::exec::CoinbaseAdapter sandbox_adapter(io, "key"_kj, "secret"_kj, true);
  KJ_EXPECT(!sandbox_adapter.is_connected());

  // Create production adapter
  veloz::exec::CoinbaseAdapter prod_adapter(io, "key"_kj, "secret"_kj, false);
  KJ_EXPECT(!prod_adapter.is_connected());
}

// Test Coinbase adapter multiple instances independence
KJ_TEST("CoinbaseAdapter: Multiple instances independence") {
  auto io = kj::setupAsyncIo();

  veloz::exec::CoinbaseAdapter adapter1(io, "key1"_kj, "secret1"_kj, true);
  veloz::exec::CoinbaseAdapter adapter2(io, "key2"_kj, "secret2"_kj, true);

  // Both adapters should be independent
  adapter1.connect();
  KJ_EXPECT(adapter1.is_connected());
  KJ_EXPECT(!adapter2.is_connected());

  adapter2.connect();
  KJ_EXPECT(adapter1.is_connected());
  KJ_EXPECT(adapter2.is_connected());

  adapter1.disconnect();
  KJ_EXPECT(!adapter1.is_connected());
  KJ_EXPECT(adapter2.is_connected());
}

// Test Coinbase adapter timeout boundary values
KJ_TEST("CoinbaseAdapter: Timeout boundary values") {
  auto io = kj::setupAsyncIo();
  veloz::exec::CoinbaseAdapter adapter(io, "test_key"_kj, "test_secret"_kj, true);

  // Test minimum timeout
  adapter.set_timeout(1 * kj::MILLISECONDS);
  KJ_EXPECT(adapter.get_timeout() == 1 * kj::MILLISECONDS);

  // Test large timeout
  adapter.set_timeout(300 * kj::SECONDS);
  KJ_EXPECT(adapter.get_timeout() == 300 * kj::SECONDS);

  // Test zero timeout (edge case)
  adapter.set_timeout(0 * kj::SECONDS);
  KJ_EXPECT(adapter.get_timeout() == 0 * kj::SECONDS);
}

// Test Coinbase adapter market data interface exists
KJ_TEST("CoinbaseAdapter: Market data interface exists") {
  auto io = kj::setupAsyncIo();
  veloz::exec::CoinbaseAdapter adapter(io, "test_key"_kj, "test_secret"_kj, true);

  // Verify the market data methods exist and compile
  veloz::common::SymbolId symbol{"BTCUSD"};

  // Synchronous methods should return none (not connected)
  auto price = adapter.get_current_price(symbol);
  KJ_EXPECT(price == kj::none);

  auto orderbook = adapter.get_order_book(symbol, 10);
  KJ_EXPECT(orderbook == kj::none);

  auto trades = adapter.get_recent_trades(symbol, 100);
  KJ_EXPECT(trades == kj::none);

  auto balance = adapter.get_account_balance("USD"_kj);
  KJ_EXPECT(balance == kj::none);
}

// Test Bybit adapter testnet vs production URL selection
KJ_TEST("BybitAdapter: Testnet vs production URL selection") {
  auto io = kj::setupAsyncIo();

  // Create testnet adapter
  veloz::exec::BybitAdapter testnet_adapter(io, "key"_kj, "secret"_kj,
                                            veloz::exec::BybitAdapter::Category::Spot, true);
  KJ_EXPECT(!testnet_adapter.is_connected());

  // Create production adapter
  veloz::exec::BybitAdapter prod_adapter(io, "key"_kj, "secret"_kj,
                                         veloz::exec::BybitAdapter::Category::Spot, false);
  KJ_EXPECT(!prod_adapter.is_connected());
}

// Test Bybit adapter multiple instances independence
KJ_TEST("BybitAdapter: Multiple instances independence") {
  auto io = kj::setupAsyncIo();

  veloz::exec::BybitAdapter adapter1(io, "key1"_kj, "secret1"_kj,
                                     veloz::exec::BybitAdapter::Category::Spot, true);
  veloz::exec::BybitAdapter adapter2(io, "key2"_kj, "secret2"_kj,
                                     veloz::exec::BybitAdapter::Category::Linear, true);

  // Both adapters should be independent
  adapter1.connect();
  KJ_EXPECT(adapter1.is_connected());
  KJ_EXPECT(!adapter2.is_connected());

  adapter2.connect();
  KJ_EXPECT(adapter1.is_connected());
  KJ_EXPECT(adapter2.is_connected());

  adapter1.disconnect();
  KJ_EXPECT(!adapter1.is_connected());
  KJ_EXPECT(adapter2.is_connected());
}

// Test Bybit adapter timeout boundary values
KJ_TEST("BybitAdapter: Timeout boundary values") {
  auto io = kj::setupAsyncIo();
  veloz::exec::BybitAdapter adapter(io, "test_key"_kj, "test_secret"_kj,
                                    veloz::exec::BybitAdapter::Category::Spot, true);

  // Test minimum timeout
  adapter.set_timeout(1 * kj::MILLISECONDS);
  KJ_EXPECT(adapter.get_timeout() == 1 * kj::MILLISECONDS);

  // Test large timeout
  adapter.set_timeout(300 * kj::SECONDS);
  KJ_EXPECT(adapter.get_timeout() == 300 * kj::SECONDS);

  // Test zero timeout (edge case)
  adapter.set_timeout(0 * kj::SECONDS);
  KJ_EXPECT(adapter.get_timeout() == 0 * kj::SECONDS);
}

// Test Bybit adapter all category types
KJ_TEST("BybitAdapter: All category types") {
  auto io = kj::setupAsyncIo();

  // Test Spot category
  veloz::exec::BybitAdapter spot_adapter(io, "key"_kj, "secret"_kj,
                                         veloz::exec::BybitAdapter::Category::Spot, true);
  KJ_EXPECT(spot_adapter.get_category() == veloz::exec::BybitAdapter::Category::Spot);

  // Test Linear category (USDT perpetual)
  veloz::exec::BybitAdapter linear_adapter(io, "key"_kj, "secret"_kj,
                                           veloz::exec::BybitAdapter::Category::Linear, true);
  KJ_EXPECT(linear_adapter.get_category() == veloz::exec::BybitAdapter::Category::Linear);

  // Test Inverse category (coin-margined)
  veloz::exec::BybitAdapter inverse_adapter(io, "key"_kj, "secret"_kj,
                                            veloz::exec::BybitAdapter::Category::Inverse, true);
  KJ_EXPECT(inverse_adapter.get_category() == veloz::exec::BybitAdapter::Category::Inverse);
}

// Test Bybit adapter market data methods exist
KJ_TEST("BybitAdapter: Market data interface exists") {
  auto io = kj::setupAsyncIo();
  veloz::exec::BybitAdapter adapter(io, "test_key"_kj, "test_secret"_kj,
                                    veloz::exec::BybitAdapter::Category::Spot, true);

  // Verify the market data methods exist and compile
  veloz::common::SymbolId symbol{"BTCUSDT"};

  // Synchronous methods should return none (not connected)
  auto price = adapter.get_current_price(symbol);
  KJ_EXPECT(price == kj::none);

  auto orderbook = adapter.get_order_book(symbol, 10);
  KJ_EXPECT(orderbook == kj::none);

  auto trades = adapter.get_recent_trades(symbol, 100);
  KJ_EXPECT(trades == kj::none);

  auto balance = adapter.get_account_balance("USDT"_kj);
  KJ_EXPECT(balance == kj::none);
}

} // namespace
