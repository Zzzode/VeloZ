#pragma once

#include "veloz/core/event_loop.h"
#include "veloz/engine/event_emitter.h"
#include "veloz/market/binance_websocket.h"
#include "veloz/market/market_event.h"

#include <kj/async-io.h>
#include <kj/common.h>
#include <kj/function.h>
#include <kj/map.h>
#include <kj/memory.h>
#include <kj/mutex.h>
#include <kj/string.h>

namespace veloz::engine {

/**
 * @brief Manages WebSocket connections to market data sources
 *
 * This class coordinates WebSocket connections to exchanges (Binance, etc.),
 * handles subscriptions, and routes market events to the engine's event emitter.
 */
class MarketDataManager final {
public:
  /**
   * @brief Configuration for the market data manager
   */
  struct Config {
    bool use_testnet = false;   ///< Use testnet endpoints
    bool auto_reconnect = true; ///< Automatically reconnect on disconnect
  };

  /**
   * @brief Construct a new MarketDataManager
   * @param ioContext KJ async I/O context for network operations
   * @param emitter Event emitter for publishing market events
   * @param config Configuration options
   */
  MarketDataManager(kj::AsyncIoContext& ioContext, EventEmitter& emitter, Config config,
                    kj::Maybe<veloz::core::EventLoop&> event_loop = kj::none);
  ~MarketDataManager();

  // Disable copy and move
  KJ_DISALLOW_COPY_AND_MOVE(MarketDataManager);

  /**
   * @brief Start the market data manager
   * @return Promise that completes when the manager stops
   */
  kj::Promise<void> start();

  /**
   * @brief Stop the market data manager
   */
  void stop();

  /**
   * @brief Check if the manager is running
   */
  [[nodiscard]] bool is_running() const;

  /**
   * @brief Subscribe to market data for a symbol
   * @param venue Exchange venue (e.g., "binance")
   * @param symbol Trading symbol (e.g., "BTCUSDT")
   * @param event_type Type of market event to subscribe to
   * @return Promise that resolves to true if subscription succeeded
   */
  kj::Promise<bool> subscribe(kj::StringPtr venue, const veloz::common::SymbolId& symbol,
                              veloz::market::MarketEventType event_type);

  /**
   * @brief Unsubscribe from market data for a symbol
   * @param venue Exchange venue
   * @param symbol Trading symbol
   * @param event_type Type of market event to unsubscribe from
   * @return Promise that resolves to true if unsubscription succeeded
   */
  kj::Promise<bool> unsubscribe(kj::StringPtr venue, const veloz::common::SymbolId& symbol,
                                veloz::market::MarketEventType event_type);

  /**
   * @brief Get connection status for a venue
   * @param venue Exchange venue
   * @return true if connected, false otherwise
   */
  [[nodiscard]] bool is_venue_connected(kj::StringPtr venue) const;

  /**
   * @brief Get statistics
   */
  [[nodiscard]] std::int64_t total_events_received() const;
  [[nodiscard]] std::int64_t total_subscriptions() const;

private:
  friend struct MarketDataManagerTestAccess;

  // Handle incoming market event from WebSocket
  void on_market_event(const veloz::market::MarketEvent& event);
  kj::Vector<veloz::core::EventTag>
  build_market_event_tags(const veloz::market::MarketEvent& event) const;
  veloz::core::EventPriority
  market_event_priority(const veloz::market::MarketEvent& event) const;
  veloz::market::MarketEvent clone_market_event(const veloz::market::MarketEvent& event) const;

  // Get or create WebSocket for venue
  kj::Maybe<veloz::market::BinanceWebSocket&> get_binance_websocket();

  Config config_;
  kj::AsyncIoContext& ioContext_;
  EventEmitter& emitter_;
  kj::Maybe<veloz::core::EventLoop&> event_loop_;

  // Running state
  std::atomic<bool> running_{false};

  // WebSocket connections per venue
  kj::Maybe<kj::Own<veloz::market::BinanceWebSocket>> binance_ws_;

  // Statistics
  std::atomic<std::int64_t> total_events_{0};
  std::atomic<std::int64_t> total_subscriptions_{0};
};

} // namespace veloz::engine
