#pragma once

#include "veloz/market/market_event.h"

#include <kj/async-io.h>
#include <kj/common.h>
#include <kj/compat/http.h>
#include <kj/memory.h>
#include <kj/string.h>

namespace kj {
class TlsContext;
} // namespace kj

namespace veloz::market {

/**
 * @brief REST client for Binance market data API
 *
 * Provides async methods for fetching market data snapshots from Binance REST API.
 * Used by ManagedOrderBook for depth snapshot synchronization.
 */
class BinanceRestClient final {
public:
  /**
   * @brief Construct a Binance REST client
   * @param ioContext KJ async I/O context for network operations
   * @param testnet Use testnet endpoints if true
   */
  BinanceRestClient(kj::AsyncIoContext& ioContext, bool testnet = false);
  ~BinanceRestClient();

  KJ_DISALLOW_COPY_AND_MOVE(BinanceRestClient);

  /**
   * @brief Fetch depth snapshot from REST API
   *
   * Returns a BookData with is_snapshot=true and sequence set to lastUpdateId.
   * This is used by ManagedOrderBook for initial synchronization.
   *
   * @param symbol Trading symbol (e.g., "BTCUSDT")
   * @param depth Number of levels to fetch (default 100, max 5000)
   * @return Promise resolving to BookData snapshot
   */
  kj::Promise<BookData> fetch_depth_snapshot(kj::StringPtr symbol, int depth = 100);

  /**
   * @brief Fetch current price from REST API
   * @param symbol Trading symbol
   * @return Promise resolving to current price
   */
  kj::Promise<kj::Maybe<double>> fetch_price(kj::StringPtr symbol);

  /**
   * @brief Check if client is connected (has valid TLS context)
   */
  [[nodiscard]] bool is_connected() const;

private:
  kj::Promise<kj::String> http_get(kj::StringPtr endpoint, kj::StringPtr params);
  kj::Promise<kj::Own<kj::HttpClient>> get_http_client();

  kj::AsyncIoContext& ioContext_;
  kj::Own<kj::HttpHeaderTable> headerTable_;
  kj::Own<kj::TlsContext> tlsContext_;
  kj::String baseUrl_;
  bool testnet_;
};

} // namespace veloz::market
