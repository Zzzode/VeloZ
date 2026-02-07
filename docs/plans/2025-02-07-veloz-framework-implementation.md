# VeloZ Quantitative Trading Framework Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Implement a production-grade quantitative trading framework building on the existing C++23 skeleton, following the incremental component approach defined in the design documents.

**Architecture:** Modular monolith with clean separation between layers (Market → Execution → OMS → Risk → Strategy). Each component is independently testable and upgradeable. Event-driven design with lock-free queues for high-performance data paths.

**Tech Stack:**
- C++23 (core engine) with CMake build system
- JSON (nlohmann/json) for event serialization
- Standard library only (no external dependencies yet to keep initial build simple)
- Future: Protobuf for contracts, gRPC for inter-service communication

---

## Phase 1: Market Data Layer Foundation

**Overview:** Implement robust multi-exchange market data handling with proper order book management, subscription aggregation, and data quality metrics.

### Task 1: Enhance MarketEvent Types and Payloads

**Files:**
- Modify: `libs/market/include/veloz/market/market_event.h`
- Modify: `libs/market/src/market_event.cpp`
- Create: `libs/market/tests/test_market_event.cpp`

**Step 1: Write failing tests for extended event types**

```cpp
// libs/market/tests/test_market_event.cpp
#include <gtest/gtest.h>
#include "veloz/market/market_event.h"

using namespace veloz::market;

TEST(MarketEvent, TradeEventSerialization) {
    MarketEvent event;
    event.type = MarketEventType::Trade;
    event.venue = common::Venue::Binance;
    event.market = common::MarketKind::Spot;
    event.symbol = {"BTCUSDT"};
    event.ts_exchange_ns = 1700000000000000000LL;
    event.ts_recv_ns = 1700000000000001000LL;
    event.ts_pub_ns = 1700000000000002000LL;

    // Trade payload: {"price": "50000.5", "qty": "0.1", "is_buyer_maker": false}
    event.payload = R"({"price": "50000.5", "qty": "0.1", "is_buyer_maker": false})";

    EXPECT_EQ(event.type, MarketEventType::Trade);
    EXPECT_EQ(event.venue, common::Venue::Binance);
    EXPECT_FALSE(event.payload.empty());
}

TEST(MarketEvent, BookEvent) {
    MarketEvent event;
    event.type = MarketEventType::BookTop;
    event.symbol = {"ETHUSDT"};
    // Book payload: {"bids": [["3000.0", "1.0"]], "asks": [["3001.0", "1.0"]], "seq": 123456}
    event.payload = R"({"bids": [["3000.0", "1.0"]], "asks": [["3001.0", "1.0"]], "seq": 123456})";

    EXPECT_EQ(event.type, MarketEventType::BookTop);
    EXPECT_EQ(event.symbol.value, "ETHUSDT");
}
```

**Step 2: Run test to verify it fails**

Run: `cmake --build build/dev --target veloz_market_tests`
Expected: FAIL (test_market_event.cpp doesn't exist yet)

**Step 3: Extend MarketEvent with new types and helper methods**

```cpp
// libs/market/include/veloz/market/market_event.h
#pragma once

#include "veloz/common/types.h"

#include <cstdint>
#include <string>
#include <variant>

namespace veloz::market {

enum class MarketEventType : std::uint8_t {
  Unknown = 0,
  Trade = 1,
  BookTop = 2,
  BookDelta = 3,
  Kline = 4,
  Ticker = 5,
  FundingRate = 6,
  MarkPrice = 7,
};

struct TradeData {
  double price{0.0};
  double qty{0.0};
  bool is_buyer_maker{false};
  std::int64_t trade_id{0};
};

struct BookLevel {
  double price{0.0};
  double qty{0.0};
};

struct BookData {
  std::vector<BookLevel> bids;
  std::vector<BookLevel> asks;
  std::int64_t sequence{0};
};

struct KlineData {
  double open{0.0};
  double high{0.0};
  double low{0.0};
  double close{0.0};
  double volume{0.0};
  std::int64_t start_time{0};
  std::int64_t close_time{0};
};

struct MarketEvent final {
  MarketEventType type{MarketEventType::Unknown};
  common::Venue venue{common::Venue::Unknown};
  common::MarketKind market{common::MarketKind::Unknown};
  common::SymbolId symbol{};

  std::int64_t ts_exchange_ns{0};
  std::int64_t ts_recv_ns{0};
  std::int64_t ts_pub_ns{0};

  // Variant for typed access (optional, can parse from payload)
  std::variant<std::monostate, TradeData, BookData, KlineData> data;

  // Raw JSON payload for backward compatibility
  std::string payload;

  // Helper: latency from exchange to publish
  [[nodiscard]] std::int64_t exchange_to_pub_ns() const {
    return ts_pub_ns - ts_exchange_ns;
  }

  // Helper: receive to publish latency
  [[nodiscard]] std::int64_t recv_to_pub_ns() const {
    return ts_pub_ns - ts_recv_ns;
  }
};

} // namespace veloz::market
```

**Step 4: Add test target to CMakeLists.txt**

```cmake
# libs/market/CMakeLists.txt (create if not exists)
add_library(veloz_market
  src/market_event.cpp
)

target_include_directories(veloz_market
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(veloz_market
  PUBLIC
    veloz_common
)

# Tests
enable_testing()
add_executable(veloz_market_tests
  tests/test_market_event.cpp
)

target_link_libraries(veloz_market_tests
  PRIVATE
    veloz_market
    GTest::gtest
    GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(veloz_market_tests)
```

**Step 5: Run test to verify it passes**

Run: `cmake --build build/dev --target veloz_market_tests && ./build/dev/libs/market/veloz_market_tests`
Expected: PASS

**Step 6: Commit**

```bash
git add libs/market/
git commit -m "feat(market): extend MarketEvent with typed data structures"
```

---

### Task 2: Implement OrderBook with Snapshot + Delta

**Files:**
- Create: `libs/market/include/veloz/market/order_book.h`
- Create: `libs/market/src/order_book.cpp`
- Create: `libs/market/tests/test_order_book.cpp`

**Step 1: Write failing test for OrderBook**

```cpp
// libs/market/tests/test_order_book.cpp
#include <gtest/gtest.h>
#include "veloz/market/order_book.h"
#include "veloz/market/market_event.h"

using namespace veloz::market;

TEST(OrderBook, Initialization) {
    OrderBook book;
    book.apply_snapshot({}, {});
    EXPECT_TRUE(book.bids().empty());
    EXPECT_TRUE(book.asks().empty());
}

TEST(OrderBook, ApplySnapshot) {
    OrderBook book;
    std::vector<BookLevel> bids = {{50000.0, 1.5}, {49999.0, 2.0}};
    std::vector<BookLevel> asks = {{50001.0, 1.0}, {50002.0, 0.5}};

    book.apply_snapshot(bids, asks, 100);

    EXPECT_EQ(book.bids().size(), 2);
    EXPECT_EQ(book.bids()[0].price, 50000.0);
    EXPECT_EQ(book.bids()[0].qty, 1.5);

    EXPECT_EQ(book.asks().size(), 2);
    EXPECT_EQ(book.asks()[0].price, 50001.0);
    EXPECT_EQ(book.sequence(), 100);
}

TEST(OrderBook, ApplyDelta) {
    OrderBook book;
    std::vector<BookLevel> bids = {{50000.0, 1.5}};
    std::vector<BookLevel> asks = {{50001.0, 1.0}};
    book.apply_snapshot(bids, asks, 100);

    // Update bid qty
    book.apply_delta(BookLevel{50000.0, 1.0}, true, 101);
    EXPECT_EQ(book.bids()[0].qty, 1.0);

    // Remove ask
    book.apply_delta(BookLevel{50001.0, 0.0}, false, 102);
    EXPECT_TRUE(book.asks().empty());
}

TEST(OrderBook, BestBidAsk) {
    OrderBook book;
    std::vector<BookLevel> bids = {{50000.0, 1.5}, {49999.0, 2.0}};
    std::vector<BookLevel> asks = {{50001.0, 1.0}};
    book.apply_snapshot(bids, asks, 100);

    auto best_bid = book.best_bid();
    auto best_ask = book.best_ask();

    ASSERT_TRUE(best_bid.has_value());
    ASSERT_TRUE(best_ask.has_value());
    EXPECT_EQ(best_bid->price, 50000.0);
    EXPECT_EQ(best_ask->price, 50001.0);
}
```

**Step 2: Run test to verify it fails**

Run: `cmake --build build/dev --target veloz_market_tests`
Expected: FAIL (order_book.h doesn't exist)

**Step 3: Implement OrderBook class**

```cpp
// libs/market/include/veloz/market/order_book.h
#pragma once

#include "veloz/market/market_event.h"

#include <map>
#include <optional>
#include <vector>

namespace veloz::market {

class OrderBook final {
public:
    OrderBook() = default;

    // Apply full snapshot (replaces existing book)
    void apply_snapshot(const std::vector<BookLevel>& bids,
                        const std::vector<BookLevel>& asks,
                        std::int64_t sequence);

    // Apply incremental delta (update/delete a level)
    void apply_delta(const BookLevel& level, bool is_bid, std::int64_t sequence);

    // Query methods
    [[nodiscard]] const std::vector<BookLevel>& bids() const;
    [[nodiscard]] const std::vector<BookLevel>& asks() const;
    [[nodiscard]] std::optional<BookLevel> best_bid() const;
    [[nodiscard]] std::optional<BookLevel> best_ask() const;
    [[nodiscard]] double spread() const;
    [[nodiscard]] double mid_price() const;
    [[nodiscard]] std::int64_t sequence() const;

    // Get top N levels
    [[nodiscard]] std::vector<BookLevel> top_bids(std::size_t n) const;
    [[nodiscard]] std::vector<BookLevel> top_asks(std::size_t n) const;

private:
    void rebuild_cache();

    std::map<double, double, std::greater<double>> bids_; // descending
    std::map<double, double> asks_;                       // ascending

    // Cache for fast queries (bids[0] = best bid, asks[0] = best ask)
    std::vector<BookLevel> bids_cache_;
    std::vector<BookLevel> asks_cache_;

    std::int64_t sequence_{0};
};

} // namespace veloz::market
```

**Step 4: Implement OrderBook methods**

```cpp
// libs/market/src/order_book.cpp
#include "veloz/market/order_book.h"

namespace veloz::market {

void OrderBook::apply_snapshot(const std::vector<BookLevel>& bids,
                                const std::vector<BookLevel>& asks,
                                std::int64_t sequence) {
    bids_.clear();
    asks_.clear();

    for (const auto& level : bids) {
        if (level.qty > 0.0) {
            bids_[level.price] = level.qty;
        }
    }

    for (const auto& level : asks) {
        if (level.qty > 0.0) {
            asks_[level.price] = level.qty;
        }
    }

    sequence_ = sequence;
    rebuild_cache();
}

void OrderBook::apply_delta(const BookLevel& level, bool is_bid, std::int64_t sequence) {
    // Reject out-of-order updates
    if (sequence <= sequence_) {
        return;
    }

    auto& book = is_bid ? bids_ : asks_;

    if (level.qty == 0.0) {
        book.erase(level.price);  // delete
    } else {
        book[level.price] = level.qty;  // update/insert
    }

    sequence_ = sequence;
    rebuild_cache();
}

const std::vector<BookLevel>& OrderBook::bids() const {
    return bids_cache_;
}

const std::vector<BookLevel>& OrderBook::asks() const {
    return asks_cache_;
}

std::optional<BookLevel> OrderBook::best_bid() const {
    if (bids_cache_.empty()) return std::nullopt;
    return bids_cache_.front();
}

std::optional<BookLevel> OrderBook::best_ask() const {
    if (asks_cache_.empty()) return std::nullopt;
    return asks_cache_.front();
}

double OrderBook::spread() const {
    auto bid = best_bid();
    auto ask = best_ask();
    if (!bid || !ask) return 0.0;
    return ask->price - bid->price;
}

double OrderBook::mid_price() const {
    auto bid = best_bid();
    auto ask = best_ask();
    if (!bid || !ask) return 0.0;
    return (bid->price + ask->price) / 2.0;
}

std::int64_t OrderBook::sequence() const {
    return sequence_;
}

std::vector<BookLevel> OrderBook::top_bids(std::size_t n) const {
    if (n >= bids_cache_.size()) return bids_cache_;
    return std::vector<BookLevel>(bids_cache_.begin(), bids_cache_.begin() + n);
}

std::vector<BookLevel> OrderBook::top_asks(std::size_t n) const {
    if (n >= asks_cache_.size()) return asks_cache_;
    return std::vector<BookLevel>(asks_cache_.begin(), asks_cache_.begin() + n);
}

void OrderBook::rebuild_cache() {
    bids_cache_.clear();
    bids_cache_.reserve(bids_.size());
    for (const auto& [price, qty] : bids_) {
        bids_cache_.push_back({price, qty});
    }

    asks_cache_.clear();
    asks_cache_.reserve(asks_.size());
    for (const auto& [price, qty] : asks_) {
        asks_cache_.push_back({price, qty});
    }
}

} // namespace veloz::market
```

**Step 5: Add test to CMakeLists.txt**

Update `libs/market/CMakeLists.txt`:
```cmake
add_library(veloz_market
  src/market_event.cpp
  src/order_book.cpp
)

# Add test_order_book.cpp to test executable
add_executable(veloz_market_tests
  tests/test_market_event.cpp
  tests/test_order_book.cpp
)
```

**Step 6: Run test to verify it passes**

Run: `cmake --build build/dev --target veloz_market_tests && ./build/dev/libs/market/veloz_market_tests`
Expected: PASS

**Step 7: Commit**

```bash
git add libs/market/
git commit -m "feat(market): implement OrderBook with snapshot/delta support"
```

---

### Task 3: Add Market Subscription Manager

**Files:**
- Create: `libs/market/include/veloz/market/subscription_manager.h`
- Create: `libs/market/src/subscription_manager.cpp`
- Create: `libs/market/tests/test_subscription_manager.cpp`

**Step 1: Write failing test**

```cpp
// libs/market/tests/test_subscription_manager.cpp
#include <gtest/gtest.h>
#include "veloz/market/subscription_manager.h"
#include "veloz/market/market_event.h"

using namespace veloz::market;

TEST(SubscriptionManager, AddSubscription) {
    SubscriptionManager sub_mgr;
    SymbolId symbol{"BTCUSDT"};

    sub_mgr.subscribe(symbol, MarketEventType::Trade, "strategy_1");
    sub_mgr.subscribe(symbol, MarketEventType::BookTop, "strategy_1");

    EXPECT_EQ(sub_mgr.subscriber_count(symbol, MarketEventType::Trade), 1);
    EXPECT_EQ(sub_mgr.subscriber_count(symbol, MarketEventType::BookTop), 1);
}

TEST(SubscriptionManager, DeduplicateSameSubscriber) {
    SubscriptionManager sub_mgr;
    SymbolId symbol{"ETHUSDT"};

    sub_mgr.subscribe(symbol, MarketEventType::Trade, "strategy_1");
    sub_mgr.subscribe(symbol, MarketEventType::Trade, "strategy_1");  // duplicate

    EXPECT_EQ(sub_mgr.subscriber_count(symbol, MarketEventType::Trade), 1);
}

TEST(SubscriptionManager, MultipleSubscribers) {
    SubscriptionManager sub_mgr;
    SymbolId symbol{"BTCUSDT"};

    sub_mgr.subscribe(symbol, MarketEventType::Trade, "strategy_1");
    sub_mgr.subscribe(symbol, MarketEventType::Trade, "strategy_2");
    sub_mgr.subscribe(symbol, MarketEventType::Trade, "strategy_3");

    EXPECT_EQ(sub_mgr.subscriber_count(symbol, MarketEventType::Trade), 3);
}

TEST(SubscriptionManager, Unsubscribe) {
    SubscriptionManager sub_mgr;
    SymbolId symbol{"BTCUSDT"};

    sub_mgr.subscribe(symbol, MarketEventType::Trade, "strategy_1");
    sub_mgr.subscribe(symbol, MarketEventType::Trade, "strategy_2");

    sub_mgr.unsubscribe(symbol, MarketEventType::Trade, "strategy_1");

    EXPECT_EQ(sub_mgr.subscriber_count(symbol, MarketEventType::Trade), 1);
}

TEST(SubscriptionManager, GetActiveSymbols) {
    SubscriptionManager sub_mgr;

    sub_mgr.subscribe(SymbolId{"BTCUSDT"}, MarketEventType::Trade, "s1");
    sub_mgr.subscribe(SymbolId{"ETHUSDT"}, MarketEventType::Trade, "s1");
    sub_mgr.subscribe(SymbolId{"BTCUSDT"}, MarketEventType::BookTop, "s2");

    auto symbols = sub_mgr.active_symbols();
    EXPECT_EQ(symbols.size(), 2);
}
```

**Step 2: Run test to verify it fails**

Run: `cmake --build build/dev --target veloz_market_tests`
Expected: FAIL (subscription_manager.h doesn't exist)

**Step 3: Implement SubscriptionManager**

```cpp
// libs/market/include/veloz/market/subscription_manager.h
#pragma once

#include "veloz/common/types.h"
#include "veloz/market/market_event.h"

#include <set>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace veloz::market {

class SubscriptionManager final {
public:
    SubscriptionManager() = default;

    // Subscribe a client to a symbol/event type
    void subscribe(const common::SymbolId& symbol,
                   MarketEventType event_type,
                   const std::string& subscriber_id);

    // Unsubscribe a client from a symbol/event type
    void unsubscribe(const common::SymbolId& symbol,
                     MarketEventType event_type,
                     const std::string& subscriber_id);

    // Query methods
    [[nodiscard]] std::size_t subscriber_count(const common::SymbolId& symbol,
                                                MarketEventType event_type) const;

    [[nodiscard]] bool is_subscribed(const common::SymbolId& symbol,
                                     MarketEventType event_type,
                                     const std::string& subscriber_id) const;

    // Get all unique symbols with any subscriptions
    [[nodiscard]] std::vector<common::SymbolId> active_symbols() const;

    // Get all event types subscribed for a symbol
    [[nodiscard]] std::vector<MarketEventType> event_types(const common::SymbolId& symbol) const;

    // Get all subscribers for a symbol/event type
    [[nodiscard]] std::vector<std::string> subscribers(const common::SymbolId& symbol,
                                                       MarketEventType event_type) const;

private:
    // Key: symbol + event_type, Value: set of subscriber IDs
    std::unordered_map<std::string, std::unordered_set<std::string>> subscriptions_;

    // Cache of active symbols (rebuild on subscription changes)
    std::vector<common::SymbolId> active_symbols_cache_;

    void rebuild_symbol_cache();
};

} // namespace veloz::market
```

**Step 4: Implement SubscriptionManager methods**

```cpp
// libs/market/src/subscription_manager.cpp
#include "veloz/market/subscription_manager.h"
#include <fmt/format.h>

namespace veloz::market {

std::string make_subscription_key(const common::SymbolId& symbol, MarketEventType event_type) {
    return fmt::format("{}|{}", symbol.value, static_cast<int>(event_type));
}

void SubscriptionManager::subscribe(const common::SymbolId& symbol,
                                      MarketEventType event_type,
                                      const std::string& subscriber_id) {
    auto key = make_subscription_key(symbol, event_type);
    subscriptions_[key].insert(subscriber_id);
    rebuild_symbol_cache();
}

void SubscriptionManager::unsubscribe(const common::SymbolId& symbol,
                                        MarketEventType event_type,
                                        const std::string& subscriber_id) {
    auto key = make_subscription_key(symbol, event_type);
    auto it = subscriptions_.find(key);
    if (it != subscriptions_.end()) {
        it->second.erase(subscriber_id);
        if (it->second.empty()) {
            subscriptions_.erase(it);
        }
        rebuild_symbol_cache();
    }
}

std::size_t SubscriptionManager::subscriber_count(const common::SymbolId& symbol,
                                                     MarketEventType event_type) const {
    auto key = make_subscription_key(symbol, event_type);
    auto it = subscriptions_.find(key);
    return it != subscriptions_.end() ? it->second.size() : 0;
}

bool SubscriptionManager::is_subscribed(const common::SymbolId& symbol,
                                        MarketEventType event_type,
                                        const std::string& subscriber_id) const {
    auto key = make_subscription_key(symbol, event_type);
    auto it = subscriptions_.find(key);
    if (it == subscriptions_.end()) return false;
    return it->second.contains(subscriber_id);
}

std::vector<common::SymbolId> SubscriptionManager::active_symbols() const {
    return active_symbols_cache_;
}

std::vector<MarketEventType> SubscriptionManager::event_types(const common::SymbolId& symbol) const {
    std::set<MarketEventType> types;
    for (const auto& [key, _] : subscriptions_) {
        if (key.starts_with(symbol.value + "|")) {
            // Parse event type from key
            auto pos = key.find('|');
            if (pos != std::string::npos) {
                int type_int = std::stoi(key.substr(pos + 1));
                types.insert(static_cast<MarketEventType>(type_int));
            }
        }
    }
    return std::vector<MarketEventType>(types.begin(), types.end());
}

std::vector<std::string> SubscriptionManager::subscribers(const common::SymbolId& symbol,
                                                           MarketEventType event_type) const {
    auto key = make_subscription_key(symbol, event_type);
    auto it = subscriptions_.find(key);
    if (it == subscriptions_.end()) return {};
    return std::vector<std::string>(it->second.begin(), it->second.end());
}

void SubscriptionManager::rebuild_symbol_cache() {
    std::unordered_set<std::string> unique_symbols;
    for (const auto& [key, _] : subscriptions_) {
        auto pos = key.find('|');
        if (pos != std::string::npos) {
            unique_symbols.insert(key.substr(0, pos));
        }
    }

    active_symbols_cache_.clear();
    active_symbols_cache_.reserve(unique_symbols.size());
    for (const auto& symbol_str : unique_symbols) {
        active_symbols_cache_.push_back({symbol_str});
    }
}

} // namespace veloz::market
```

**Step 5: Add fmt library dependency to CMakeLists.txt**

Note: For now, we'll use simple string concatenation instead of fmt to avoid external dependencies. Update the implementation:

```cpp
// Simplified version without fmt dependency
std::string make_subscription_key(const common::SymbolId& symbol, MarketEventType event_type) {
    return symbol.value + "|" + std::to_string(static_cast<int>(event_type));
}
```

**Step 6: Add test to CMakeLists.txt and run**

Update `libs/market/CMakeLists.txt`:
```cmake
add_executable(veloz_market_tests
  tests/test_market_event.cpp
  tests/test_order_book.cpp
  tests/test_subscription_manager.cpp
)
```

Run: `cmake --build build/dev --target veloz_market_tests && ./build/dev/libs/market/veloz_market_tests`
Expected: PASS

**Step 7: Commit**

```bash
git add libs/market/
git commit -m "feat(market): add subscription manager for event fanout"
```

---

### Task 4: Add Data Quality Metrics

**Files:**
- Create: `libs/market/include/veloz/market/metrics.h`
- Create: `libs/market/src/metrics.cpp`
- Create: `libs/market/tests/test_metrics.cpp`

**Step 1: Write failing test**

```cpp
// libs/market/tests/test_metrics.cpp
#include <gtest/gtest.h>
#include "veloz/market/metrics.h"

using namespace veloz::market;

TEST(MarketMetrics, Initialize) {
    MarketMetrics metrics;
    EXPECT_EQ(metrics.event_count(), 0);
    EXPECT_EQ(metrics.drop_count(), 0);
}

TEST(MarketMetrics, RecordEvent) {
    MarketMetrics metrics;
    metrics.record_event_latency_ns(1000);
    metrics.record_event_latency_ns(2000);
    metrics.record_event_latency_ns(3000);

    EXPECT_EQ(metrics.event_count(), 3);
    EXPECT_EQ(metrics.avg_latency_ns(), 2000);
}

TEST(MarketMetrics, RecordDrop) {
    MarketMetrics metrics;
    metrics.record_drop();

    EXPECT_EQ(metrics.drop_count(), 1);
}

TEST(MarketMetrics, Percentiles) {
    MarketMetrics metrics;
    for (int i = 1; i <= 100; ++i) {
        metrics.record_event_latency_ns(i * 1000);  // 1ms to 100ms
    }

    auto p50 = metrics.percentile_ns(50);
    auto p99 = metrics.percentile_ns(99);

    EXPECT_NEAR(p50, 50000, 1000);  // 50ms
    EXPECT_NEAR(p99, 99000, 1000);  // 99ms
}

TEST(MarketMetrics, ReconnectTracking) {
    MarketMetrics metrics;
    metrics.record_reconnect();

    EXPECT_EQ(metrics.reconnect_count(), 1);
}
```

**Step 2: Run test to verify it fails**

Run: `cmake --build build/dev --target veloz_market_tests`
Expected: FAIL (metrics.h doesn't exist)

**Step 3: Implement MarketMetrics**

```cpp
// libs/market/include/veloz/market/metrics.h
#pragma once

#include <cstdint>
#include <deque>
#include <vector>

namespace veloz::market {

class MarketMetrics final {
public:
    MarketMetrics() = default;

    // Event tracking
    void record_event_latency_ns(std::int64_t latency_ns);
    void record_drop();
    void record_reconnect();
    void record_gap(std::int64_t expected_seq, std::int64_t actual_seq);

    // Query methods
    [[nodiscard]] std::size_t event_count() const;
    [[nodiscard]] std::size_t drop_count() const;
    [[nodiscard]] std::size_t reconnect_count() const;
    [[nodiscard]] std::int64_t avg_latency_ns() const;

    // Percentile calculation (p50, p99, p99.9)
    [[nodiscard]] std::int64_t percentile_ns(double percentile) const;

    // Reset metrics
    void reset();

private:
    std::size_t event_count_{0};
    std::size_t drop_count_{0};
    std::size_t reconnect_count_{0};

    // Circular buffer for latency samples (keep last N samples)
    std::deque<std::int64_t> latency_samples_;
    static constexpr std::size_t MAX_SAMPLES = 10000;
};

} // namespace veloz::market
```

**Step 4: Implement MarketMetrics methods**

```cpp
// libs/market/src/metrics.cpp
#include "veloz/market/metrics.h"
#include <algorithm>
#include <numeric>

namespace veloz::market {

void MarketMetrics::record_event_latency_ns(std::int64_t latency_ns) {
    ++event_count_;
    latency_samples_.push_back(latency_ns);
    if (latency_samples_.size() > MAX_SAMPLES) {
        latency_samples_.pop_front();
    }
}

void MarketMetrics::record_drop() {
    ++drop_count_;
}

void MarketMetrics::record_reconnect() {
    ++reconnect_count_;
}

void MarketMetrics::record_gap(std::int64_t expected_seq, std::int64_t actual_seq) {
    // Could track gap size for analytics
    ++drop_count_;
}

std::size_t MarketMetrics::event_count() const {
    return event_count_;
}

std::size_t MarketMetrics::drop_count() const {
    return drop_count_;
}

std::size_t MarketMetrics::reconnect_count() const {
    return reconnect_count_;
}

std::int64_t MarketMetrics::avg_latency_ns() const {
    if (latency_samples_.empty()) return 0;
    std::int64_t sum = std::accumulate(latency_samples_.begin(), latency_samples_.end(), 0LL);
    return sum / static_cast<std::int64_t>(latency_samples_.size());
}

std::int64_t MarketMetrics::percentile_ns(double percentile) const {
    if (latency_samples_.empty()) return 0;

    std::vector<std::int64_t> sorted(latency_samples_.begin(), latency_samples_.end());
    std::sort(sorted.begin(), sorted.end());

    std::size_t index = static_cast<std::size_t>(
        std::ceil(percentile / 100.0 * sorted.size()) - 1
    );
    index = std::min(index, sorted.size() - 1);

    return sorted[index];
}

void MarketMetrics::reset() {
    event_count_ = 0;
    drop_count_ = 0;
    reconnect_count_ = 0;
    latency_samples_.clear();
}

} // namespace veloz::market
```

**Step 5: Add test to CMakeLists.txt and run**

Update `libs/market/CMakeLists.txt`:
```cmake
add_executable(veloz_market_tests
  tests/test_market_event.cpp
  tests/test_order_book.cpp
  tests/test_subscription_manager.cpp
  tests/test_metrics.cpp
)
```

Run: `cmake --build build/dev --target veloz_market_tests && ./build/dev/libs/market/veloz_market_tests`
Expected: PASS

**Step 6: Commit**

```bash
git add libs/market/
git commit -m "feat(market): add data quality metrics tracking"
```

---

## Phase 2: Execution Layer

**Overview:** Implement exchange adapters, order routing, and idempotent order placement with proper state machine.

### Task 5: Implement Exchange Adapter Interface

**Files:**
- Create: `libs/exec/include/veloz/exec/exchange_adapter.h`
- Create: `libs/exec/tests/test_exchange_adapter_interface.cpp`

**Step 1: Write failing test for adapter interface**

```cpp
// libs/exec/tests/test_exchange_adapter_interface.cpp
#include <gtest/gtest.h>
#include "veloz/exec/exchange_adapter.h"
#include "veloz/exec/order_api.h"

using namespace veloz::exec;

// Mock adapter for testing
class MockExchangeAdapter : public ExchangeAdapter {
public:
    std::optional<ExecutionReport> place_order(const PlaceOrderRequest& req) override {
        ExecutionReport report;
        report.symbol = req.symbol;
        report.client_order_id = req.client_order_id;
        report.venue_order_id = "VENUE123";
        report.status = OrderStatus::Accepted;
        return report;
    }

    std::optional<ExecutionReport> cancel_order(const CancelOrderRequest& req) override {
        ExecutionReport report;
        report.symbol = req.symbol;
        report.client_order_id = req.client_order_id;
        report.venue_order_id = "VENUE123";
        report.status = OrderStatus::Canceled;
        return report;
    }

    bool is_connected() const override { return true; }
    void connect() override {}
    void disconnect() override {}
};

TEST(ExchangeAdapter, PlaceOrder) {
    MockExchangeAdapter adapter;

    PlaceOrderRequest req;
    req.symbol = {"BTCUSDT"};
    req.side = OrderSide::Buy;
    req.type = OrderType::Limit;
    req.qty = 0.5;
    req.price = 50000.0;
    req.client_order_id = "CLIENT123";

    auto report = adapter.place_order(req);
    ASSERT_TRUE(report.has_value());
    EXPECT_EQ(report->status, OrderStatus::Accepted);
    EXPECT_EQ(report->venue_order_id, "VENUE123");
}

TEST(ExchangeAdapter, CancelOrder) {
    MockExchangeAdapter adapter;

    CancelOrderRequest req;
    req.symbol = {"BTCUSDT"};
    req.client_order_id = "CLIENT123";

    auto report = adapter.cancel_order(req);
    ASSERT_TRUE(report.has_value());
    EXPECT_EQ(report->status, OrderStatus::Canceled);
}
```

**Step 2: Run test to verify it fails**

Run: `cmake --build build/dev --target veloz_exec_tests`
Expected: FAIL (exchange_adapter.h doesn't exist)

**Step 3: Define ExchangeAdapter interface**

```cpp
// libs/exec/include/veloz/exec/exchange_adapter.h
#pragma once

#include "veloz/exec/order_api.h"

#include <optional>
#include <string>

namespace veloz::exec {

class ExchangeAdapter {
public:
    virtual ~ExchangeAdapter() = default;

    // Place an order, returns execution report
    virtual std::optional<ExecutionReport> place_order(const PlaceOrderRequest& req) = 0;

    // Cancel an order, returns execution report
    virtual std::optional<ExecutionReport> cancel_order(const CancelOrderRequest& req) = 0;

    // Connection management
    virtual bool is_connected() const = 0;
    virtual void connect() = 0;
    virtual void disconnect() = 0;

    // Get adapter info
    [[nodiscard]] virtual const char* name() const = 0;
    [[nodiscard]] virtual const char* version() const = 0;
};

} // namespace veloz::exec
```

**Step 4: Create CMakeLists.txt for exec tests**

```cmake
# libs/exec/CMakeLists.txt (add if not exists)
# Add test target
add_executable(veloz_exec_tests
  tests/test_exchange_adapter_interface.cpp
)

target_link_libraries(veloz_exec_tests
  PRIVATE
    veloz_exec
    GTest::gtest
    GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(veloz_exec_tests)
```

**Step 5: Run test to verify it passes**

Run: `cmake --build build/dev --target veloz_exec_tests && ./build/dev/libs/exec/veloz_exec_tests`
Expected: PASS

**Step 6: Commit**

```bash
git add libs/exec/
git commit -m "feat(exec): define exchange adapter interface"
```

---

### Task 6: Implement Order Routing

**Files:**
- Create: `libs/exec/include/veloz/exec/order_router.h`
- Create: `libs/exec/src/order_router.cpp`
- Create: `libs/exec/tests/test_order_router.cpp`

**Step 1: Write failing test**

```cpp
// libs/exec/tests/test_order_router.cpp
#include <gtest/gtest.h>
#include "veloz/exec/order_router.h"
#include "veloz/exec/exchange_adapter.h"

using namespace veloz::exec;

class MockAdapter : public ExchangeAdapter {
public:
    std::string name_;

    MockAdapter(const std::string& name) : name_(name) {}

    std::optional<ExecutionReport> place_order(const PlaceOrderRequest& req) override {
        ExecutionReport report;
        report.symbol = req.symbol;
        report.client_order_id = req.client_order_id;
        report.venue_order_id = name_ + "_" + req.client_order_id;
        report.status = OrderStatus::Accepted;
        return report;
    }

    std::optional<ExecutionReport> cancel_order(const CancelOrderRequest& req) override {
        ExecutionReport report;
        report.symbol = req.symbol;
        report.client_order_id = req.client_order_id;
        report.status = OrderStatus::Canceled;
        return report;
    }

    bool is_connected() const override { return true; }
    void connect() override {}
    void disconnect() override {}

    const char* name() const override { return name_.c_str(); }
    const char* version() const override { return "1.0.0"; }
};

TEST(OrderRouter, RegisterAdapter) {
    OrderRouter router;
    auto adapter = std::make_shared<MockAdapter>("binance");

    router.register_adapter(common::Venue::Binance, adapter);

    EXPECT_TRUE(router.has_adapter(common::Venue::Binance));
}

TEST(OrderRouter, RouteOrder) {
    OrderRouter router;
    auto adapter = std::make_shared<MockAdapter>("binance");
    router.register_adapter(common::Venue::Binance, adapter);

    PlaceOrderRequest req;
    req.symbol = {"BTCUSDT"};
    req.client_order_id = "CLIENT123";

    auto report = router.place_order(common::Venue::Binance, req);
    ASSERT_TRUE(report.has_value());
    EXPECT_EQ(report->venue_order_id, "binance_CLIENT123");
}

TEST(OrderRouter, RouteToDefaultVenue) {
    OrderRouter router;
    auto adapter = std::make_shared<MockAdapter>("binance");
    router.register_adapter(common::Venue::Binance, adapter);
    router.set_default_venue(common::Venue::Binance);

    PlaceOrderRequest req;
    req.symbol = {"BTCUSDT"};
    req.client_order_id = "CLIENT123";

    // Route without specifying venue
    auto report = router.place_order(req);
    ASSERT_TRUE(report.has_value());
    EXPECT_EQ(report->venue_order_id, "binance_CLIENT123");
}
```

**Step 2: Run test to verify it fails**

Run: `cmake --build build/dev --target veloz_exec_tests`
Expected: FAIL (order_router.h doesn't exist)

**Step 3: Implement OrderRouter**

```cpp
// libs/exec/include/veloz/exec/order_router.h
#pragma once

#include "veloz/common/types.h"
#include "veloz/exec/exchange_adapter.h"
#include "veloz/exec/order_api.h"

#include <memory>
#include <optional>
#include <unordered_map>

namespace veloz::exec {

class OrderRouter final {
public:
    OrderRouter() = default;

    // Register an adapter for a venue
    void register_adapter(common::Venue venue, std::shared_ptr<ExchangeAdapter> adapter);

    // Unregister an adapter
    void unregister_adapter(common::Venue venue);

    // Set default venue (when not specified in order request)
    void set_default_venue(common::Venue venue);

    // Check if adapter exists for venue
    [[nodiscard]] bool has_adapter(common::Venue venue) const;

    // Route order to specific venue or default
    std::optional<ExecutionReport> place_order(common::Venue venue,
                                                const PlaceOrderRequest& req);

    std::optional<ExecutionReport> place_order(const PlaceOrderRequest& req);

    // Route cancel to specific venue
    std::optional<ExecutionReport> cancel_order(common::Venue venue,
                                                 const CancelOrderRequest& req);

    // Get adapter for venue
    [[nodiscard]] std::shared_ptr<ExchangeAdapter> get_adapter(common::Venue venue) const;

private:
    std::unordered_map<common::Venue, std::shared_ptr<ExchangeAdapter>> adapters_;
    std::optional<common::Venue> default_venue_;
};

} // namespace veloz::exec
```

**Step 4: Implement OrderRouter methods**

```cpp
// libs/exec/src/order_router.cpp
#include "veloz/exec/order_router.h"
#include <stdexcept>

namespace veloz::exec {

void OrderRouter::register_adapter(common::Venue venue,
                                   std::shared_ptr<ExchangeAdapter> adapter) {
    if (!adapter) {
        throw std::invalid_argument("Adapter cannot be null");
    }
    adapters_[venue] = adapter;
}

void OrderRouter::unregister_adapter(common::Venue venue) {
    adapters_.erase(venue);
}

void OrderRouter::set_default_venue(common::Venue venue) {
    default_venue_ = venue;
}

bool OrderRouter::has_adapter(common::Venue venue) const {
    return adapters_.contains(venue);
}

std::optional<ExecutionReport> OrderRouter::place_order(common::Venue venue,
                                                           const PlaceOrderRequest& req) {
    auto adapter = get_adapter(venue);
    if (!adapter) {
        return std::nullopt;
    }
    return adapter->place_order(req);
}

std::optional<ExecutionReport> OrderRouter::place_order(const PlaceOrderRequest& req) {
    if (!default_venue_) {
        return std::nullopt;
    }
    return place_order(*default_venue_, req);
}

std::optional<ExecutionReport> OrderRouter::cancel_order(common::Venue venue,
                                                            const CancelOrderRequest& req) {
    auto adapter = get_adapter(venue);
    if (!adapter) {
        return std::nullopt;
    }
    return adapter->cancel_order(req);
}

std::shared_ptr<ExchangeAdapter> OrderRouter::get_adapter(common::Venue venue) const {
    auto it = adapters_.find(venue);
    if (it == adapters_.end()) {
        return nullptr;
    }
    return it->second;
}

} // namespace veloz::exec
```

**Step 5: Add test to CMakeLists.txt and run**

Update `libs/exec/CMakeLists.txt`:
```cmake
add_executable(veloz_exec_tests
  tests/test_exchange_adapter_interface.cpp
  tests/test_order_router.cpp
)
```

Run: `cmake --build build/dev --target veloz_exec_tests && ./build/dev/libs/exec/veloz_exec_tests`
Expected: PASS

**Step 6: Commit**

```bash
git add libs/exec/
git commit -m "feat(exec): implement order routing with adapter selection"
```

---

### Task 7: Implement ClientOrderId Generator

**Files:**
- Create: `libs/exec/include/veloz/exec/client_order_id.h`
- Create: `libs/exec/src/client_order_id.cpp`
- Create: `libs/exec/tests/test_client_order_id.cpp`

**Step 1: Write failing test**

```cpp
// libs/exec/tests/test_client_order_id.cpp
#include <gtest/gtest.h>
#include "veloz/exec/client_order_id.h"

using namespace veloz::exec;

TEST(ClientOrderId, Generate) {
    ClientOrderIdGenerator gen("STRAT");

    std::string id1 = gen.generate();
    std::string id2 = gen.generate();

    EXPECT_FALSE(id1.empty());
    EXPECT_FALSE(id2.empty());
    EXPECT_NE(id1, id2);  // Unique
    EXPECT_TRUE(id1.starts_with("STRAT-"));
}

TEST(ClientOrderId, HighThroughput) {
    ClientOrderIdGenerator gen("TEST");

    std::unordered_set<std::string> ids;
    for (int i = 0; i < 10000; ++i) {
        std::string id = gen.generate();
        EXPECT_FALSE(ids.contains(id));  // No duplicates
        ids.insert(id);
    }

    EXPECT_EQ(ids.size(), 10000);
}

TEST(ClientOrderId, ParseComponents) {
    std::string id = "STRAT-1700000000-ABC123";

    auto [strategy, timestamp, unique] = ClientOrderIdGenerator::parse(id);

    EXPECT_EQ(strategy, "STRAT");
    EXPECT_EQ(timestamp, 1700000000);
    EXPECT_EQ(unique, "ABC123");
}
```

**Step 2: Run test to verify it fails**

Run: `cmake --build build/dev --target veloz_exec_tests`
Expected: FAIL (client_order_id.h doesn't exist)

**Step 3: Implement ClientOrderIdGenerator**

```cpp
// libs/exec/include/veloz/exec/client_order_id.h
#pragma once

#include <cstdint>
#include <string>
#include <tuple>

namespace veloz::exec {

class ClientOrderIdGenerator final {
public:
    explicit ClientOrderIdGenerator(const std::string& strategy_id);

    // Generate unique client order ID
    [[nodiscard]] std::string generate();

    // Parse components: {strategy, timestamp, unique}
    static std::tuple<std::string, std::int64_t, std::string> parse(const std::string& client_order_id);

private:
    std::string strategy_id_;
    std::int64_t sequence_{0};
};

} // namespace veloz::exec
```

**Step 4: Implement ClientOrderIdGenerator methods**

```cpp
// libs/exec/src/client_order_id.cpp
#include "veloz/exec/client_order_id.h"
#include <chrono>
#include <random>
#include <sstream>

namespace veloz::exec {

ClientOrderIdGenerator::ClientOrderIdGenerator(const std::string& strategy_id)
    : strategy_id_(strategy_id) {}

std::string ClientOrderIdGenerator::generate() {
    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();

    ++sequence_;

    // Generate random component for uniqueness across processes
    static thread_local std::random_device rd;
    static thread_local std::mt19937 gen(rd());
    static thread_local std::uniform_int_distribution<> dis(0, 15);

    std::ostringstream oss;
    oss << strategy_id_ << "-" << timestamp << "-" << sequence_ << "-";
    for (int i = 0; i < 4; ++i) {
        oss << std::hex << dis(gen);
    }

    return oss.str();
}

std::tuple<std::string, std::int64_t, std::string>
ClientOrderIdGenerator::parse(const std::string& client_order_id) {
    // Format: STRATEGY-TIMESTAMP-SEQUENCE-RANDOM
    size_t first_dash = client_order_id.find('-');
    size_t second_dash = client_order_id.find('-', first_dash + 1);
    size_t third_dash = client_order_id.find('-', second_dash + 1);

    if (first_dash == std::string::npos ||
        second_dash == std::string::npos ||
        third_dash == std::string::npos) {
        return {"", 0, ""};
    }

    std::string strategy = client_order_id.substr(0, first_dash);
    std::int64_t timestamp = std::stoll(client_order_id.substr(first_dash + 1, second_dash - first_dash - 1));
    std::string unique = client_order_id.substr(third_dash + 1);

    return {strategy, timestamp, unique};
}

} // namespace veloz::exec
```

**Step 5: Add test to CMakeLists.txt and run**

Update `libs/exec/CMakeLists.txt`:
```cmake
add_executable(veloz_exec_tests
  tests/test_exchange_adapter_interface.cpp
  tests/test_order_router.cpp
  tests/test_client_order_id.cpp
)
```

Run: `cmake --build build/dev --target veloz_exec_tests && ./build/dev/libs/exec/veloz_exec_tests`
Expected: PASS

**Step 6: Commit**

```bash
git add libs/exec/
git commit -m "feat(exec): add client order ID generator for idempotency"
```

---

## Phase 3: OMS Enhancements

**Overview:** Enhance order state machine with proper transitions, position aggregation, and account reconciliation.

### Task 8: Implement Order State Machine

**Files:**
- Modify: `libs/oms/include/veloz/oms/order_record.h`
- Modify: `libs/oms/src/order_record.cpp`
- Create: `libs/oms/tests/test_order_state_machine.cpp`

**Step 1: Write failing test for state machine**

```cpp
// libs/oms/tests/test_order_state_machine.cpp
#include <gtest/gtest.h>
#include "veloz/oms/order_record.h"
#include "veloz/exec/order_api.h"

using namespace veloz::oms;
using namespace veloz::exec;

TEST(OrderStateMachine, NewToAccepted) {
    PlaceOrderRequest req;
    req.symbol = {"BTCUSDT"};
    req.qty = 1.0;
    req.price = 50000.0;
    req.client_order_id = "TEST1";

    OrderRecord record(req);

    ExecutionReport report;
    report.symbol = req.symbol;
    report.client_order_id = req.client_order_id;
    report.venue_order_id = "VENUE1";
    report.status = OrderStatus::Accepted;

    record.apply(report);

    EXPECT_EQ(record.status(), OrderStatus::Accepted);
    EXPECT_EQ(record.venue_order_id(), "VENUE1");
}

TEST(OrderStateMachine, PartiallyFilled) {
    PlaceOrderRequest req;
    req.symbol = {"BTCUSDT"};
    req.qty = 1.0;
    req.price = 50000.0;
    req.client_order_id = "TEST1";

    OrderRecord record(req);

    ExecutionReport report;
    report.symbol = req.symbol;
    report.client_order_id = req.client_order_id;
    report.venue_order_id = "VENUE1";
    report.status = OrderStatus::PartiallyFilled;
    report.last_fill_qty = 0.3;
    report.last_fill_price = 50000.0;

    record.apply(report);

    EXPECT_EQ(record.status(), OrderStatus::PartiallyFilled);
    EXPECT_EQ(record.cum_qty(), 0.3);
    EXPECT_EQ(record.avg_price(), 50000.0);
}

TEST(OrderStateMachine, FullyFilled) {
    PlaceOrderRequest req;
    req.symbol = {"BTCUSDT"};
    req.qty = 1.0;
    req.price = 50000.0;
    req.client_order_id = "TEST1";

    OrderRecord record(req);

    // First fill: 0.3
    ExecutionReport fill1;
    fill1.symbol = req.symbol;
    fill1.client_order_id = req.client_order_id;
    fill1.venue_order_id = "VENUE1";
    fill1.status = OrderStatus::PartiallyFilled;
    fill1.last_fill_qty = 0.3;
    fill1.last_fill_price = 50000.0;
    record.apply(fill1);

    // Second fill: 0.7 (complete)
    ExecutionReport fill2;
    fill2.symbol = req.symbol;
    fill2.client_order_id = req.client_order_id;
    fill2.venue_order_id = "VENUE1";
    fill2.status = OrderStatus::Filled;
    fill2.last_fill_qty = 0.7;
    fill2.last_fill_price = 50100.0;
    record.apply(fill2);

    EXPECT_EQ(record.status(), OrderStatus::Filled);
    EXPECT_EQ(record.cum_qty(), 1.0);
    EXPECT_NEAR(record.avg_price(), 50070.0, 0.01);  // Weighted avg
}

TEST(OrderStateMachine, Rejected) {
    PlaceOrderRequest req;
    req.symbol = {"BTCUSDT"};
    req.qty = 1.0;
    req.price = 50000.0;
    req.client_order_id = "TEST1";

    OrderRecord record(req);

    ExecutionReport report;
    report.symbol = req.symbol;
    report.client_order_id = req.client_order_id;
    report.status = OrderStatus::Rejected;

    record.apply(report);

    EXPECT_EQ(record.status(), OrderStatus::Rejected);
}
```

**Step 2: Run test to verify it fails**

Run: `cmake --build build/dev --target veloz_oms_tests`
Expected: FAIL (order_record implementation incomplete)

**Step 3: Enhance OrderRecord with proper state machine**

```cpp
// libs/oms/include/veloz/oms/order_record.h
#pragma once

#include "veloz/exec/order_api.h"

#include <string>

namespace veloz::oms {

class OrderRecord final {
public:
  explicit OrderRecord(veloz::exec::PlaceOrderRequest request);

  [[nodiscard]] const veloz::exec::PlaceOrderRequest& request() const;
  [[nodiscard]] const std::string& venue_order_id() const;
  [[nodiscard]] veloz::exec::OrderStatus status() const;
  [[nodiscard]] double cum_qty() const;
  [[nodiscard]] double avg_price() const;
  [[nodiscard]] double remaining_qty() const;

  // Apply execution report (handle state transitions)
  void apply(const veloz::exec::ExecutionReport& report);

  // Check if order is terminal (no more state changes possible)
  [[nodiscard]] bool is_terminal() const;

  // Check if order is active (can still be filled/canceled)
  [[nodiscard]] bool is_active() const;

private:
  void validate_transition(veloz::exec::OrderStatus new_status);

  veloz::exec::PlaceOrderRequest request_;
  std::string venue_order_id_;
  veloz::exec::OrderStatus status_{veloz::exec::OrderStatus::New};
  double cum_qty_{0.0};
  double avg_price_{0.0};
};

} // namespace veloz::oms
```

**Step 4: Implement OrderRecord with state machine**

```cpp
// libs/oms/src/order_record.cpp
#include "veloz/oms/order_record.h"
#include <stdexcept>
#include <unordered_map>

namespace veloz::oms {

// Valid state transitions
const std::unordered_map<veloz::exec::OrderStatus, std::unordered_set<veloz::exec::OrderStatus>>
VALID_TRANSITIONS = {
    {veloz::exec::OrderStatus::New, {veloz::exec::OrderStatus::Accepted, veloz::exec::OrderStatus::Rejected}},
    {veloz::exec::OrderStatus::Accepted, {
        veloz::exec::OrderStatus::PartiallyFilled,
        veloz::exec::OrderStatus::Filled,
        veloz::exec::OrderStatus::Canceled,
        veloz::exec::OrderStatus::Rejected
    }},
    {veloz::exec::OrderStatus::PartiallyFilled, {
        veloz::exec::OrderStatus::PartiallyFilled,
        veloz::exec::OrderStatus::Filled,
        veloz::exec::OrderStatus::Canceled,
        veloz::exec::OrderStatus::Rejected
    }},
};

OrderRecord::OrderRecord(veloz::exec::PlaceOrderRequest request)
    : request_(std::move(request)) {}

const veloz::exec::PlaceOrderRequest& OrderRecord::request() const {
  return request_;
}

const std::string& OrderRecord::venue_order_id() const {
  return venue_order_id_;
}

veloz::exec::OrderStatus OrderRecord::status() const {
  return status_;
}

double OrderRecord::cum_qty() const {
  return cum_qty_;
}

double OrderRecord::avg_price() const {
  return avg_price_;
}

double OrderRecord::remaining_qty() const {
  return request_.qty - cum_qty_;
}

void OrderRecord::apply(const veloz::exec::ExecutionReport& report) {
  validate_transition(report.status);

  // Update venue order ID on first update
  if (venue_order_id_.empty() && !report.venue_order_id.empty()) {
    venue_order_id_ = report.venue_order_id;
  }

  // Update status
  status_ = report.status;

  // Update fill information
  if (report.last_fill_qty > 0.0) {
    double old_qty = cum_qty_;
    cum_qty_ += report.last_fill_qty;

    // Update weighted average price
    if (report.last_fill_price > 0.0) {
      double old_notional = old_qty * avg_price_;
      double new_notional = report.last_fill_qty * report.last_fill_price;
      avg_price_ = (old_notional + new_notional) / cum_qty_;
    }
  }
}

void OrderRecord::validate_transition(veloz::exec::OrderStatus new_status) {
  // Allow self-transitions for duplicate reports
  if (new_status == status_) {
    return;
  }

  auto it = VALID_TRANSITIONS.find(status_);
  if (it == VALID_TRANSITIONS.end()) {
    throw std::invalid_argument("Invalid transition from terminal state");
  }

  if (!it->second.contains(new_status)) {
    throw std::invalid_argument("Invalid state transition");
  }
}

bool OrderRecord::is_terminal() const {
  return status_ == veloz::exec::OrderStatus::Filled ||
         status_ == veloz::exec::OrderStatus::Canceled ||
         status_ == veloz::exec::OrderStatus::Rejected ||
         status_ == veloz::exec::OrderStatus::Expired;
}

bool OrderRecord::is_active() const {
  return !is_terminal();
}

} // namespace veloz::oms
```

**Step 5: Create OMS tests CMakeLists.txt**

```cmake
# libs/oms/CMakeLists.txt (add test target)
add_executable(veloz_oms_tests
  tests/test_order_state_machine.cpp
)

target_link_libraries(veloz_oms_tests
  PRIVATE
    veloz_oms
    GTest::gtest
    GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(veloz_oms_tests)
```

**Step 6: Run test to verify it passes**

Run: `cmake --build build/dev --target veloz_oms_tests && ./build/dev/libs/oms/veloz_oms_tests`
Expected: PASS

**Step 7: Commit**

```bash
git add libs/oms/
git commit -m "feat(oms): implement proper order state machine"
```

---

### Task 9: Implement Position Aggregation

**Files:**
- Create: `libs/oms/include/veloz/oms/position.h`
- Create: `libs/oms/src/position.cpp`
- Create: `libs/oms/tests/test_position.cpp`

**Step 1: Write failing test**

```cpp
// libs/oms/tests/test_position.cpp
#include <gtest/gtest.h>
#include "veloz/oms/position.h"
#include "veloz/exec/order_api.h"

using namespace veloz::oms;
using namespace veloz::exec;

TEST(Position, Initialize) {
    Position pos({"BTCUSDT"});

    EXPECT_EQ(pos.size(), 0.0);
    EXPECT_EQ(pos.avg_price(), 0.0);
    EXPECT_EQ(pos.side(), PositionSide::None);
}

TEST(Position, OpenLong) {
    Position pos({"BTCUSDT"});

    // Buy 1.0 @ 50000
    pos.apply_fill(OrderSide::Buy, 1.0, 50000.0);

    EXPECT_EQ(pos.size(), 1.0);
    EXPECT_EQ(pos.avg_price(), 50000.0);
    EXPECT_EQ(pos.side(), PositionSide::Long);
}

TEST(Position, AddToLong) {
    Position pos({"BTCUSDT"});

    // Buy 1.0 @ 50000
    pos.apply_fill(OrderSide::Buy, 1.0, 50000.0);

    // Buy 0.5 @ 51000
    pos.apply_fill(OrderSide::Buy, 0.5, 51000.0);

    EXPECT_EQ(pos.size(), 1.5);
    EXPECT_NEAR(pos.avg_price(), 50333.33, 0.01);  // Weighted avg
}

TEST(Position, PartialClose) {
    Position pos({"BTCUSDT"});

    // Buy 1.0 @ 50000
    pos.apply_fill(OrderSide::Buy, 1.0, 50000.0);

    // Sell 0.3 @ 51000
    pos.apply_fill(OrderSide::Sell, 0.3, 51000.0);

    EXPECT_EQ(pos.size(), 0.7);
    EXPECT_EQ(pos.avg_price(), 50000.0);  // Avg price unchanged
    EXPECT_EQ(pos.realized_pnl(), 300.0);  // 0.3 * (51000 - 50000)
}

TEST(Position, FullClose) {
    Position pos({"BTCUSDT"});

    // Buy 1.0 @ 50000
    pos.apply_fill(OrderSide::Buy, 1.0, 50000.0);

    // Sell 1.0 @ 51000
    pos.apply_fill(OrderSide::Sell, 1.0, 51000.0);

    EXPECT_EQ(pos.size(), 0.0);
    EXPECT_EQ(pos.side(), PositionSide::None);
    EXPECT_EQ(pos.realized_pnl(), 1000.0);
}

TEST(Position, UnrealizedPnL) {
    Position pos({"BTCUSDT"});

    // Buy 1.0 @ 50000
    pos.apply_fill(OrderSide::Buy, 1.0, 50000.0);

    // Current price: 51000
    EXPECT_EQ(pos.unrealized_pnl(51000.0), 1000.0);
}
```

**Step 2: Run test to verify it fails**

Run: `cmake --build build/dev --target veloz_oms_tests`
Expected: FAIL (position.h doesn't exist)

**Step 3: Implement Position class**

```cpp
// libs/oms/include/veloz/oms/position.h
#pragma once

#include "veloz/common/types.h"
#include "veloz/exec/order_api.h"

#include <cstdint>

namespace veloz::oms {

enum class PositionSide : std::uint8_t {
  None = 0,
  Long = 1,
  Short = 2,
};

class Position final {
public:
  explicit Position(common::SymbolId symbol);

  [[nodiscard]] const common::SymbolId& symbol() const;
  [[nodiscard]] double size() const;
  [[nodiscard]] double avg_price() const;
  [[nodiscard]] PositionSide side() const;

  [[nodiscard]] double realized_pnl() const;
  [[nodiscard]] double unrealized_pnl(double current_price) const;

  // Apply a fill to update position
  void apply_fill(veloz::exec::OrderSide side, double qty, double price);

  // Reset position (e.g., after reconciliation)
  void reset();

private:
  common::SymbolId symbol_;
  double size_{0.0};         // Positive = long, Negative = short
  double avg_price_{0.0};    // Average entry price
  double realized_pnl_{0.0}; // Cumulative realized PnL
};

} // namespace veloz::oms
```

**Step 4: Implement Position methods**

```cpp
// libs/oms/src/position.cpp
#include "veloz/oms/position.h"
#include <cmath>

namespace veloz::oms {

Position::Position(common::SymbolId symbol)
    : symbol_(std::move(symbol)) {}

const common::SymbolId& Position::symbol() const {
  return symbol_;
}

double Position::size() const {
  return size_;
}

double Position::avg_price() const {
  return avg_price_;
}

PositionSide Position::side() const {
  if (std::abs(size_) < 1e-9) return PositionSide::None;
  return size_ > 0 ? PositionSide::Long : PositionSide::Short;
}

double Position::realized_pnl() const {
  return realized_pnl_;
}

double Position::unrealized_pnl(double current_price) const {
  if (std::abs(size_) < 1e-9) return 0.0;

  if (size_ > 0) {
    // Long: (current - avg) * size
    return (current_price - avg_price_) * size_;
  } else {
    // Short: (avg - current) * |size|
    return (avg_price_ - current_price) * (-size_);
  }
}

void Position::apply_fill(veloz::exec::OrderSide side, double qty, double price) {
  if (side == veloz::exec::OrderSide::Buy) {
    // Buy: add to long or reduce short
    if (size_ < 0) {
      // Reducing short position
      double close_qty = std::min(qty, -size_);
      realized_pnl_ += close_qty * (avg_price_ - price);
      size_ += close_qty;
      qty -= close_qty;
    }

    if (qty > 0) {
      // Opening or adding to long
      if (size_ + qty > 0) {
        avg_price_ = (avg_price_ * size_ + price * qty) / (size_ + qty);
      } else {
        avg_price_ = price;
      }
      size_ += qty;
    }
  } else {
    // Sell: add to short or reduce long
    if (size_ > 0) {
      // Reducing long position
      double close_qty = std::min(qty, size_);
      realized_pnl_ += close_qty * (price - avg_price_);
      size_ -= close_qty;
      qty -= close_qty;
    }

    if (qty > 0) {
      // Opening or adding to short
      if (size_ - qty < 0) {
        avg_price_ = (avg_price_ * (-size_) + price * qty) / ((-size_) + qty);
      } else {
        avg_price_ = price;
      }
      size_ -= qty;
    }
  }

  // Reset if position is closed
  if (std::abs(size_) < 1e-9) {
    size_ = 0.0;
    avg_price_ = 0.0;
  }
}

void Position::reset() {
  size_ = 0.0;
  avg_price_ = 0.0;
  realized_pnl_ = 0.0;
}

} // namespace veloz::oms
```

**Step 5: Add test to CMakeLists.txt and run**

Update `libs/oms/CMakeLists.txt`:
```cmake
add_executable(veloz_oms_tests
  tests/test_order_state_machine.cpp
  tests/test_position.cpp
)
```

Run: `cmake --build build/dev --target veloz_oms_tests && ./build/dev/libs/oms/veloz_oms_tests`
Expected: PASS

**Step 6: Commit**

```bash
git add libs/oms/
git commit -m "feat(oms): implement position aggregation with PnL tracking"
```

---

## Phase 4: Risk Engine

**Overview:** Implement pre-trade risk checks, circuit breakers, and post-trade validation.

### Task 10: Implement Pre-Trade Risk Checks

**Files:**
- Modify: `libs/risk/include/veloz/risk/risk_engine.h`
- Modify: `libs/risk/src/risk_engine.cpp`
- Create: `libs/risk/tests/test_risk_engine.cpp`

**Step 1: Write failing test**

```cpp
// libs/risk/tests/test_risk_engine.cpp
#include <gtest/gtest.h>
#include "veloz/risk/risk_engine.h"
#include "veloz/oms/position.h"
#include "veloz/exec/order_api.h"

using namespace veloz::risk;
using namespace veloz::exec;

TEST(RiskEngine, CheckAvailableFunds) {
    RiskEngine engine;

    engine.set_account_balance(10000.0);  // USDT

    PlaceOrderRequest req;
    req.symbol = {"BTCUSDT"};
    req.side = OrderSide::Buy;
    req.qty = 0.1;
    req.price = 50000.0;  // Needs 5000 USDT

    auto result = engine.check_pre_trade(req);
    EXPECT_TRUE(result.allowed);
}

TEST(RiskEngine, RejectInsufficientFunds) {
    RiskEngine engine;

    engine.set_account_balance(1000.0);  // Only 1000 USDT

    PlaceOrderRequest req;
    req.symbol = {"BTCUSDT"};
    req.side = OrderSide::Buy;
    req.qty = 0.1;
    req.price = 50000.0;  // Needs 5000 USDT

    auto result = engine.check_pre_trade(req);
    EXPECT_FALSE(result.allowed);
    EXPECT_TRUE(result.reason.find("Insufficient funds") != std::string::npos);
}

TEST(RiskEngine, CheckMaxPosition) {
    RiskEngine engine;

    engine.set_max_position_size(1.0);

    PlaceOrderRequest req;
    req.symbol = {"BTCUSDT"};
    req.side = OrderSide::Buy;
    req.qty = 2.0;  // Exceeds max

    auto result = engine.check_pre_trade(req);
    EXPECT_FALSE(result.allowed);
}

TEST(RiskEngine, CheckPriceDeviation) {
    RiskEngine engine;

    engine.set_reference_price(50000.0);
    engine.set_max_price_deviation(0.05);  // 5%

    PlaceOrderRequest req;
    req.symbol = {"BTCUSDT"};
    req.side = OrderSide::Buy;
    req.qty = 0.1;
    req.price = 53000.0;  // 6% above reference

    auto result = engine.check_pre_trade(req);
    EXPECT_FALSE(result.allowed);
}
```

**Step 2: Run test to verify it fails**

Run: `cmake --build build/dev --target veloz_risk_tests`
Expected: FAIL (risk_engine incomplete)

**Step 3: Enhance RiskEngine**

```cpp
// libs/risk/include/veloz/risk/risk_engine.h
#pragma once

#include "veloz/exec/order_api.h"

#include <string>

namespace veloz::risk {

struct RiskCheckResult {
  bool allowed{true};
  std::string reason;
};

class RiskEngine final {
public:
  RiskEngine() = default;

  // Pre-trade checks
  [[nodiscard]] RiskCheckResult check_pre_trade(const veloz::exec::PlaceOrderRequest& req);

  // Configuration
  void set_account_balance(double balance_usdt);
  void set_max_position_size(double max_size);
  void set_max_leverage(double max_leverage);
  void set_reference_price(double price);
  void set_max_price_deviation(double deviation);

private:
  [[nodiscard]] bool check_available_funds(const veloz::exec::PlaceOrderRequest& req) const;
  [[nodiscard]] bool check_max_position(const veloz::exec::PlaceOrderRequest& req) const;
  [[nodiscard]] bool check_price_deviation(const veloz::exec::PlaceOrderRequest& req) const;

  double account_balance_{0.0};
  double max_position_size_{0.0};
  double max_leverage_{1.0};
  double reference_price_{0.0};
  double max_price_deviation_{0.1};  // 10% default
};

} // namespace veloz::risk
```

**Step 4: Implement RiskEngine**

```cpp
// libs/risk/src/risk_engine.cpp
#include "veloz/risk/risk_engine.h"
#include <fmt/format.h>
#include <cmath>

namespace veloz::risk {

RiskCheckResult RiskEngine::check_pre_trade(const veloz::exec::PlaceOrderRequest& req) {
  // Check available funds
  if (!check_available_funds(req)) {
    return {false, "Insufficient funds"};
  }

  // Check max position
  if (!check_max_position(req)) {
    return {false, fmt::format("Order size {} exceeds max position {}", req.qty, max_position_size_)};
  }

  // Check price deviation
  if (!check_price_deviation(req)) {
    double deviation = std::abs((req.price.value_or(0) - reference_price_) / reference_price_);
    return {false, fmt::format("Price deviation {:.2%} exceeds max {:.2%}", deviation, max_price_deviation_)};
  }

  return {true, ""};
}

void RiskEngine::set_account_balance(double balance_usdt) {
  account_balance_ = balance_usdt;
}

void RiskEngine::set_max_position_size(double max_size) {
  max_position_size_ = max_size;
}

void RiskEngine::set_max_leverage(double max_leverage) {
  max_leverage_ = max_leverage;
}

void RiskEngine::set_reference_price(double price) {
  reference_price_ = price;
}

void RiskEngine::set_max_price_deviation(double deviation) {
  max_price_deviation_ = deviation;
}

bool RiskEngine::check_available_funds(const veloz::exec::PlaceOrderRequest& req) const {
  if (!req.price.has_value()) {
    return true;  // Market orders checked elsewhere
  }

  double notional = req.qty * req.price.value();
  return notional <= account_balance_;
}

bool RiskEngine::check_max_position(const veloz::exec::PlaceOrderRequest& req) const {
  if (max_position_size_ <= 0.0) {
    return true;  // No limit
  }
  return req.qty <= max_position_size_;
}

bool RiskEngine::check_price_deviation(const veloz::exec::PlaceOrderRequest& req) const {
  if (reference_price_ <= 0.0 || !req.price.has_value()) {
    return true;  // No reference price or market order
  }

  double deviation = std::abs((req.price.value() - reference_price_) / reference_price_);
  return deviation <= max_price_deviation_;
}

} // namespace veloz::risk
```

**Step 5: Create risk tests CMakeLists.txt**

```cmake
# libs/risk/CMakeLists.txt (add test target)
add_executable(veloz_risk_tests
  tests/test_risk_engine.cpp
)

target_link_libraries(veloz_risk_tests
  PRIVATE
    veloz_risk
    veloz_exec
    veloz_oms
    GTest::gtest
    GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(veloz_risk_tests)
```

**Step 6: Run test to verify it passes**

Note: fmt library not available, use string concatenation instead.

Update implementation to use simple string formatting:

```cpp
#include <sstream>

// Replace fmt::format with:
std::string format(const std::string& fmt, ...) {
    std::ostringstream oss;
    // Simple implementation for key cases
    // Or just use static string messages
    return oss.str();
}
```

Simplified version without fmt:
```cpp
// In check_pre_trade:
if (!check_available_funds(req)) {
    return {false, "Insufficient funds"};
}

if (!check_max_position(req)) {
    return {false, "Order size exceeds max position"};
}

if (!check_price_deviation(req)) {
    return {false, "Price deviation exceeds max"};
}
```

Run: `cmake --build build/dev --target veloz_risk_tests && ./build/dev/libs/risk/veloz_risk_tests`
Expected: PASS

**Step 7: Commit**

```bash
git add libs/risk/
git commit -m "feat(risk): implement pre-trade risk checks"
```

---

### Task 11: Implement Circuit Breaker

**Files:**
- Create: `libs/risk/include/veloz/risk/circuit_breaker.h`
- Create: `libs/risk/src/circuit_breaker.cpp`
- Create: `libs/risk/tests/test_circuit_breaker.cpp`

**Step 1: Write failing test**

```cpp
// libs/risk/tests/test_circuit_breaker.cpp
#include <gtest/gtest.h>
#include "veloz/risk/circuit_breaker.h"
#include <thread>
#include <chrono>

using namespace veloz::risk;

TEST(CircuitBreaker, AllowRequestsWhenOpen) {
    CircuitBreaker cb;

    EXPECT_TRUE(cb.allow_request());
}

TEST(CircuitBreaker, TripOnFailures) {
    CircuitBreaker cb;
    cb.set_failure_threshold(3);

    cb.record_failure();
    cb.record_failure();
    EXPECT_TRUE(cb.allow_request());  // Still open

    cb.record_failure();  // Trip!
    EXPECT_FALSE(cb.allow_request());  // Now open
}

TEST(CircuitBreaker, AutoResetAfterTimeout) {
    CircuitBreaker cb;
    cb.set_failure_threshold(2);
    cb.set_timeout_ms(100);

    cb.record_failure();
    cb.record_failure();
    EXPECT_FALSE(cb.allow_request());

    std::this_thread::sleep_for(std::chrono::milliseconds(150));
    EXPECT_TRUE(cb.allow_request());  // Should be auto-reset
}

TEST(CircuitBreaker, ManualReset) {
    CircuitBreaker cb;
    cb.set_failure_threshold(2);

    cb.record_failure();
    cb.record_failure();
    EXPECT_FALSE(cb.allow_request());

    cb.reset();
    EXPECT_TRUE(cb.allow_request());
}

TEST(CircuitBreaker, HalfOpenState) {
    CircuitBreaker cb;
    cb.set_failure_threshold(2);

    cb.record_failure();
    cb.record_failure();
    EXPECT_FALSE(cb.allow_request());

    cb.reset();  // Go to half-open
    EXPECT_TRUE(cb.allow_request());

    cb.record_success();
    EXPECT_TRUE(cb.allow_request());  // Stay closed
}
```

**Step 2: Run test to verify it fails**

Run: `cmake --build build/dev --target veloz_risk_tests`
Expected: FAIL (circuit_breaker.h doesn't exist)

**Step 3: Implement CircuitBreaker**

```cpp
// libs/risk/include/veloz/risk/circuit_breaker.h
#pragma once

#include <cstdint>
#include <mutex>

namespace veloz::risk {

enum class CircuitState {
  Closed,   // Normal operation
  Open,     // Circuit is tripped, blocking requests
  HalfOpen, // Testing if service recovered
};

class CircuitBreaker final {
public:
  CircuitBreaker() = default;

  // Check if request is allowed
  [[nodiscard]] bool allow_request();

  // Record outcome
  void record_success();
  void record_failure();

  // Manual reset
  void reset();

  // Configuration
  void set_failure_threshold(std::size_t threshold);
  void set_timeout_ms(std::int64_t timeout_ms);
  void set_success_threshold(std::size_t threshold);

  // Query state
  [[nodiscard]] CircuitState state() const;

private:
  void check_auto_reset();

  std::mutex mutex_;
  CircuitState state_{CircuitState::Closed};
  std::size_t failure_count_{0};
  std::size_t success_count_{0};
  std::int64_t last_failure_time_ms_{0};

  std::size_t failure_threshold_{5};
  std::int64_t timeout_ms_{60000};  // 1 minute default
  std::size_t success_threshold_{2};  // Need 2 successes in half-open to close
};

} // namespace veloz::risk
```

**Step 4: Implement CircuitBreaker methods**

```cpp
// libs/risk/src/circuit_breaker.cpp
#include "veloz/risk/circuit_breaker.h"
#include <chrono>

namespace veloz::risk {

bool CircuitBreaker::allow_request() {
  std::lock_guard<std::mutex> lock(mutex_);

  check_auto_reset();

  return state_ != CircuitState::Open;
}

void CircuitBreaker::record_success() {
  std::lock_guard<std::mutex> lock(mutex_);

  if (state_ == CircuitState::HalfOpen) {
    success_count_++;
    if (success_count_ >= success_threshold_) {
      state_ = CircuitState::Closed;
      failure_count_ = 0;
      success_count_ = 0;
    }
  } else if (state_ == CircuitState::Closed) {
    failure_count_ = 0;
  }
}

void CircuitBreaker::record_failure() {
  std::lock_guard<std::mutex> lock(mutex_);

  failure_count_++;

  if (state_ == CircuitState::HalfOpen) {
    // Back to open immediately
    state_ = CircuitState::Open;
    success_count_ = 0;
  } else if (state_ == CircuitState::Closed &&
             failure_count_ >= failure_threshold_) {
    state_ = CircuitState::Open;
  }

  auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::system_clock::now().time_since_epoch()).count();
  last_failure_time_ms_ = now;
}

void CircuitBreaker::reset() {
  std::lock_guard<std::mutex> lock(mutex_);

  state_ = CircuitState::HalfOpen;
  success_count_ = 0;
}

void CircuitBreaker::set_failure_threshold(std::size_t threshold) {
  std::lock_guard<std::mutex> lock(mutex_);
  failure_threshold_ = threshold;
}

void CircuitBreaker::set_timeout_ms(std::int64_t timeout_ms) {
  std::lock_guard<std::mutex> lock(mutex_);
  timeout_ms_ = timeout_ms;
}

void CircuitBreaker::set_success_threshold(std::size_t threshold) {
  std::lock_guard<std::mutex> lock(mutex_);
  success_threshold_ = threshold;
}

CircuitState CircuitBreaker::state() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return state_;
}

void CircuitBreaker::check_auto_reset() {
  if (state_ != CircuitState::Open) return;

  auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::system_clock::now().time_since_epoch()).count();

  if (now - last_failure_time_ms_ >= timeout_ms_) {
    state_ = CircuitState::HalfOpen;
    success_count_ = 0;
  }
}

} // namespace veloz::risk
```

**Step 5: Add test to CMakeLists.txt and run**

Update `libs/risk/CMakeLists.txt`:
```cmake
add_executable(veloz_risk_tests
  tests/test_risk_engine.cpp
  tests/test_circuit_breaker.cpp
)
```

Run: `cmake --build build/dev --target veloz_risk_tests && ./build/dev/libs/risk/veloz_risk_tests`
Expected: PASS

**Step 6: Commit**

```bash
git add libs/risk/
git commit -m "feat(risk): implement circuit breaker for fault tolerance"
```

---

## Integration and Engine Enhancement

**Overview:** Integrate all components into the engine and enhance the stdio simulation.

### Task 12: Integrate Components into Engine

**Files:**
- Modify: `apps/engine/include/veloz/engine/engine_app.h`
- Modify: `apps/engine/src/engine_app.cpp`
- Modify: `apps/engine/include/veloz/engine/stdio_engine.h`
- Modify: `apps/engine/src/stdio_engine.cpp`

**Step 1: Write failing integration test**

```cpp
// apps/engine/tests/test_integration.cpp
#include <gtest/gtest.h>
#include "veloz/engine/stdio_engine.h"
#include "veloz/exec/order_api.h"

using namespace veloz::engine;
using namespace veloz::exec;

TEST(EngineIntegration, PlaceAndFillOrder) {
    StdioEngine engine;

    PlaceOrderRequest req;
    req.symbol = {"BTCUSDT"};
    req.side = OrderSide::Buy;
    req.type = OrderType::Limit;
    req.qty = 0.5;
    req.price = 50000.0;
    req.client_order_id = "TEST1";

    // Place order
    auto result = engine.place_order(req);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->status, OrderStatus::Accepted);

    // Check order store
    auto state = engine.get_order_state("TEST1");
    ASSERT_TRUE(state.has_value());
    EXPECT_EQ(state->status, "Accepted");

    // Simulate fill
    engine.simulate_fill("TEST1", 0.5, 50000.0);

    // Check updated state
    state = engine.get_order_state("TEST1");
    ASSERT_TRUE(state.has_value());
    EXPECT_EQ(state->status, "Filled");
    EXPECT_EQ(state->executed_qty, 0.5);
}
```

**Step 2: Run test to verify it fails**

Run: `cmake --build build/dev --target veloz_engine_tests`
Expected: FAIL (integration incomplete)

**Step 3: Enhance engine with integrated components**

This requires modifying existing engine files to use the new components. Key changes:

- Add OrderRouter to StdioEngine
- Add RiskEngine for pre-trade checks
- Update order handling flow
- Integrate Position tracking

(Implementation details would go here - see actual files for current structure)

**Step 4: Add test to engine CMakeLists.txt and run**

Update `apps/engine/CMakeLists.txt`:
```cmake
add_executable(veloz_engine_tests
  tests/test_integration.cpp
)

target_link_libraries(veloz_engine_tests
  PRIVATE
    veloz_engine
    veloz_market
    veloz_exec
    veloz_oms
    veloz_risk
    GTest::gtest
    GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(veloz_engine_tests)
```

Run: `cmake --build build/dev --target veloz_engine_tests && ./build/dev/apps/engine/veloz_engine_tests`
Expected: PASS

**Step 5: Commit**

```bash
git add apps/engine/
git commit -m "feat(engine): integrate market, exec, oms, risk components"
```

---

### Task 13: Update Build System with GoogleTest

**Files:**
- Modify: `CMakeLists.txt`
- Modify: `.github/workflows/ci.yml`

**Step 1: Update root CMakeLists.txt to enable testing**

```cmake
# Add to CMakeLists.txt
find_package(GTest REQUIRED)

# Enable testing in all subdirectories
enable_testing()
```

**Step 2: Update CI to run tests**

```yaml
# .github/workflows/ci.yml (add test step)
- name: Run tests
  run: |
    ctest --preset dev --output-on-failure
```

**Step 3: Verify build and tests**

Run: `cmake --preset dev && cmake --build --preset dev -j && ctest --preset dev --output-on-failure`
Expected: All tests pass

**Step 4: Commit**

```bash
git add CMakeLists.txt .github/workflows/ci.yml
git commit -m "ci: enable GoogleTest integration and CI test execution"
```

---

## Summary

This plan implements the core VeloZ quantitative trading framework in 4 phases:

1. **Phase 1 - Market Data Layer** (Tasks 1-4): Enhanced MarketEvent, OrderBook, SubscriptionManager, Metrics
2. **Phase 2 - Execution Layer** (Tasks 5-7): ExchangeAdapter, OrderRouter, ClientOrderIdGenerator
3. **Phase 3 - OMS Enhancements** (Tasks 8-9): OrderStateMachine, Position
4. **Phase 4 - Risk Engine** (Tasks 10-11): Pre-trade checks, CircuitBreaker
5. **Integration** (Tasks 12-13): Engine integration, build system updates

Each component is independently testable, incrementally buildable, and follows TDD principles. The design aligns with the specifications in the design documents while building on the existing skeleton.

---

## Next Steps After This Plan

Once this plan is complete, the foundation will be in place for:
- **Phase 5**: Exchange-specific adapters (Binance, OKX)
- **Phase 6**: Strategy runtime and SDK
- **Phase 7**: Analytics and backtest
- **Phase 8**: AI Agent Bridge

Each subsequent phase can be planned and implemented using the same incremental approach.
