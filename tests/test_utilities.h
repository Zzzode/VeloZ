/**
 * @file test_utilities.h
 * @brief Common test utilities and fixtures for VeloZ integration tests
 *
 * This file provides reusable test helpers for:
 * - Creating test MarketEvents
 * - Creating test PlaceOrderRequests
 * - Callback capture utilities
 * - Time utilities for testing
 */

#pragma once

#include "veloz/common/types.h"
#include "veloz/exec/order_api.h"
#include "veloz/market/market_event.h"

#include <chrono>
#include <kj/string.h>
#include <kj/vector.h>

namespace veloz::test {

// ============================================================================
// Time Utilities
// ============================================================================

/**
 * @brief Get current timestamp in nanoseconds
 */
inline std::int64_t now_ns() {
  return std::chrono::duration_cast<std::chrono::nanoseconds>(
             std::chrono::system_clock::now().time_since_epoch())
      .count();
}

/**
 * @brief Get current timestamp in milliseconds
 */
inline std::int64_t now_ms() {
  return std::chrono::duration_cast<std::chrono::milliseconds>(
             std::chrono::system_clock::now().time_since_epoch())
      .count();
}

// ============================================================================
// MarketEvent Generators
// ============================================================================

/**
 * @brief Create a test trade event
 */
inline market::MarketEvent create_trade_event(kj::StringPtr symbol, double price, double qty,
                                              bool is_buyer_maker = false,
                                              std::int64_t trade_id = 1) {
  market::MarketEvent event;
  event.type = market::MarketEventType::Trade;
  event.venue = common::Venue::Binance;
  event.market = common::MarketKind::Spot;
  event.symbol = common::SymbolId(symbol);

  auto ts = now_ns();
  event.ts_exchange_ns = ts - 1000000; // 1ms ago
  event.ts_recv_ns = ts - 500000;      // 0.5ms ago
  event.ts_pub_ns = ts;

  market::TradeData trade;
  trade.price = price;
  trade.qty = qty;
  trade.is_buyer_maker = is_buyer_maker;
  trade.trade_id = trade_id;
  event.data = trade;

  return event;
}

/**
 * @brief Create a test book top event
 */
inline market::MarketEvent create_book_top_event(kj::StringPtr symbol, double bid_price,
                                                 double bid_qty, double ask_price, double ask_qty,
                                                 std::int64_t sequence = 1) {
  market::MarketEvent event;
  event.type = market::MarketEventType::BookTop;
  event.venue = common::Venue::Binance;
  event.market = common::MarketKind::Spot;
  event.symbol = common::SymbolId(symbol);

  auto ts = now_ns();
  event.ts_exchange_ns = ts - 1000000;
  event.ts_recv_ns = ts - 500000;
  event.ts_pub_ns = ts;

  market::BookData book;
  book.bids.add(market::BookLevel{bid_price, bid_qty});
  book.asks.add(market::BookLevel{ask_price, ask_qty});
  book.sequence = sequence;
  book.is_snapshot = false;
  event.data = kj::mv(book);

  return event;
}

/**
 * @brief Create a test book snapshot event
 */
inline market::MarketEvent create_book_snapshot_event(kj::StringPtr symbol,
                                                      kj::ArrayPtr<const market::BookLevel> bids,
                                                      kj::ArrayPtr<const market::BookLevel> asks,
                                                      std::int64_t sequence = 1) {
  market::MarketEvent event;
  event.type = market::MarketEventType::BookDelta;
  event.venue = common::Venue::Binance;
  event.market = common::MarketKind::Spot;
  event.symbol = common::SymbolId(symbol);

  auto ts = now_ns();
  event.ts_exchange_ns = ts - 1000000;
  event.ts_recv_ns = ts - 500000;
  event.ts_pub_ns = ts;

  market::BookData book;
  for (const auto& bid : bids) {
    book.bids.add(bid);
  }
  for (const auto& ask : asks) {
    book.asks.add(ask);
  }
  book.sequence = sequence;
  book.is_snapshot = true;
  event.data = kj::mv(book);

  return event;
}

/**
 * @brief Create a test kline event
 */
inline market::MarketEvent create_kline_event(kj::StringPtr symbol, double open, double high,
                                              double low, double close, double volume,
                                              std::int64_t start_time = 0) {
  market::MarketEvent event;
  event.type = market::MarketEventType::Kline;
  event.venue = common::Venue::Binance;
  event.market = common::MarketKind::Spot;
  event.symbol = common::SymbolId(symbol);

  auto ts = now_ns();
  event.ts_exchange_ns = ts - 1000000;
  event.ts_recv_ns = ts - 500000;
  event.ts_pub_ns = ts;

  market::KlineData kline;
  kline.open = open;
  kline.high = high;
  kline.low = low;
  kline.close = close;
  kline.volume = volume;
  kline.start_time = start_time > 0 ? start_time : now_ms() - 60000; // 1 minute ago
  kline.close_time = kline.start_time + 60000;                       // 1 minute duration
  event.data = kline;

  return event;
}

// ============================================================================
// Order Request Generators
// ============================================================================

/**
 * @brief Create a test place order request
 */
inline exec::PlaceOrderRequest create_order_request(kj::StringPtr client_order_id,
                                                    kj::StringPtr symbol, exec::OrderSide side,
                                                    double qty, double price) {
  exec::PlaceOrderRequest request;
  request.client_order_id = kj::heapString(client_order_id);
  request.symbol = common::SymbolId(symbol);
  request.side = side;
  request.type = exec::OrderType::Limit;
  request.tif = exec::TimeInForce::GTC;
  request.qty = qty;
  request.price = price;
  return request;
}

/**
 * @brief Create a test market order request
 */
inline exec::PlaceOrderRequest create_market_order_request(kj::StringPtr client_order_id,
                                                           kj::StringPtr symbol,
                                                           exec::OrderSide side, double qty) {
  exec::PlaceOrderRequest request;
  request.client_order_id = kj::heapString(client_order_id);
  request.symbol = common::SymbolId(symbol);
  request.side = side;
  request.type = exec::OrderType::Market;
  request.tif = exec::TimeInForce::IOC;
  request.qty = qty;
  return request;
}

// ============================================================================
// Callback Capture Utilities
// ============================================================================

/**
 * @brief Utility class to capture callback invocations for testing
 */
template <typename T>
class CallbackCapture {
public:
  void capture(T value) {
    captured_.add(kj::mv(value));
  }

  [[nodiscard]] size_t count() const {
    return captured_.size();
  }

  [[nodiscard]] bool empty() const {
    return captured_.size() == 0;
  }

  [[nodiscard]] const T& at(size_t index) const {
    return captured_[index];
  }

  [[nodiscard]] const T& last() const {
    return captured_[captured_.size() - 1];
  }

  void clear() {
    captured_.clear();
  }

private:
  kj::Vector<T> captured_;
};

/**
 * @brief Specialized capture for MarketEvents
 */
class MarketEventCapture {
public:
  void capture(const market::MarketEvent& event) {
    // Store event type and symbol for verification
    EventRecord record;
    record.type = event.type;
    record.symbol = kj::heapString(event.symbol.value);
    record.ts_pub_ns = event.ts_pub_ns;
    events_.add(kj::mv(record));
  }

  [[nodiscard]] size_t count() const {
    return events_.size();
  }

  [[nodiscard]] size_t count_by_type(market::MarketEventType type) const {
    size_t count = 0;
    for (const auto& event : events_) {
      if (event.type == type) {
        count++;
      }
    }
    return count;
  }

  [[nodiscard]] size_t count_by_symbol(kj::StringPtr symbol) const {
    size_t count = 0;
    for (const auto& event : events_) {
      if (event.symbol == symbol) {
        count++;
      }
    }
    return count;
  }

  void clear() {
    events_.clear();
  }

private:
  struct EventRecord {
    market::MarketEventType type;
    kj::String symbol;
    std::int64_t ts_pub_ns;
  };
  kj::Vector<EventRecord> events_;
};

/**
 * @brief Specialized capture for PlaceOrderRequests
 */
class OrderRequestCapture {
public:
  void capture(const exec::PlaceOrderRequest& request) {
    OrderRecord record;
    record.client_order_id = kj::heapString(request.client_order_id);
    record.symbol = kj::heapString(request.symbol.value);
    record.side = request.side;
    record.qty = request.qty;
    KJ_IF_SOME(price, request.price) {
      record.price = price;
    }
    orders_.add(kj::mv(record));
  }

  [[nodiscard]] size_t count() const {
    return orders_.size();
  }

  [[nodiscard]] bool has_order(kj::StringPtr client_order_id) const {
    for (const auto& order : orders_) {
      if (order.client_order_id == client_order_id) {
        return true;
      }
    }
    return false;
  }

  void clear() {
    orders_.clear();
  }

private:
  struct OrderRecord {
    kj::String client_order_id;
    kj::String symbol;
    exec::OrderSide side;
    double qty;
    double price{0.0};
  };
  kj::Vector<OrderRecord> orders_;
};

// ============================================================================
// Test Data Constants
// ============================================================================

namespace symbols {
constexpr const char* BTCUSDT = "BTCUSDT";
constexpr const char* ETHUSDT = "ETHUSDT";
constexpr const char* BNBUSDT = "BNBUSDT";
} // namespace symbols

namespace prices {
constexpr double BTC_PRICE = 50000.0;
constexpr double ETH_PRICE = 3000.0;
constexpr double BNB_PRICE = 300.0;
} // namespace prices

} // namespace veloz::test
