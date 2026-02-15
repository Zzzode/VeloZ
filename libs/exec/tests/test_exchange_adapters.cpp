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

// Test synchronous order methods return none (async-only implementation)
KJ_TEST("OKXAdapter: Synchronous methods return none") {
  auto io = kj::setupAsyncIo();
  veloz::exec::OKXAdapter adapter(io, "test_key"_kj, "test_secret"_kj, "test_passphrase"_kj, true);

  veloz::exec::PlaceOrderRequest place_req;
  place_req.symbol = veloz::common::SymbolId{"BTCUSDT"};
  place_req.side = veloz::exec::OrderSide::Buy;
  place_req.qty = 0.001;

  auto place_result = adapter.place_order(place_req);
  KJ_EXPECT(place_result == kj::none);

  veloz::exec::CancelOrderRequest cancel_req;
  cancel_req.symbol = veloz::common::SymbolId{"BTCUSDT"};
  cancel_req.client_order_id = kj::str("test-order-123");

  auto cancel_result = adapter.cancel_order(cancel_req);
  KJ_EXPECT(cancel_result == kj::none);
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

} // namespace
