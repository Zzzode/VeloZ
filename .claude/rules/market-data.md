---
paths:
  - "libs/market/**"
---

# Market Data (Order Book, WebSocket, Subscriptions)

## Overview

This skill provides guidance for market data module development in VeloZ. The market module handles order book management, WebSocket connections, market event aggregation, and subscription management.

## Order Book Management

### Order Book Implementation Pattern

**Design**: Sequence-based reconstruction with out-of-order handling

**Key Features**:
- Sequence numbers for tracking updates
- Gap detection and filling
- Best bid/ask tracking with `kj::Maybe<T>`
- Price levels management

**Interface:**

```cpp
class OrderBook {
public:
    // Update order book with sequence number
    void onUpdate(uint64_t seqNum, const OrderBookUpdate& update);

    // Get best bid (nullable)
    kj::Maybe<const OrderLevel&> bestBid() const;

    // Get best ask (nullable)
    kj::Maybe<const OrderLevel&> ask() const;

    // Get spread (nullable)
    kj::Maybe<double> spread() const;
};
```

**Thread Safety:**
- Use `kj::MutexGuarded<T>` for protecting book state
- Atomic sequence number for tracking updates

### Best Bid/Ask Updates

**Rules:**

- Use `kj::Maybe<T>` for nullable best bid/ask (no value means no liquidity)
- Spread calculation: `spread = bestAsk.price - bestBid.price` when both exist
- Separate buy and sell side updates

```cpp
// ✅ CORRECT - Use kj::Maybe for nullable values
kj::Maybe<const OrderLevel&> bestBid_;
kj::Maybe<const OrderLevel&> bestAsk_;

// Update with spread calculation
KJ_IF_SOME(bestBid_, bestBid) {
    kj::IF_SOME(bestAsk_, bestAsk) {
        double spread = bestAsk_.price - bestBid_.price;
        // Use spread...
    }
}
```

## WebSocket Implementation

### Custom WebSocket (RFC 6455)

**Status**: **COMPLETED** - Custom WebSocket implementation using KJ async I/O

**Design Pattern:**
- Frame encoding/decoding per RFC 6455
- KJ async I/O with `kj::AsyncIoStream`
- TLS support via OpenSSL
- Exponential backoff for reconnection

**Frame Types:**

| Frame Type | Opcode | Purpose |
|-------------|---------|----------|
| CONTINUATION | 0x0 | Heartbeat |
| TEXT | 0x1 | Partial text message |
| BINARY | 0x2 | Binary message |
| CLOSE | 0x8 | Connection close |
| PING | 0x9 | Keep-alive ping |
| PONG | 0xA | Keep-alive pong |

**Connection State Machine:**

```cpp
enum class ConnectionState {
    CONNECTING,
    HANDSHAKING,
    CONNECTED,
    RECONNECTING,
    CLOSED
};
```

**Reconnection Logic:**

- Exponential backoff with jitter
- Maximum retry limit
- Authentication on reconnect (if applicable)
- State tracking across connection attempts

```cpp
class WebSocketClient {
private:
    kj::MutexGuarded<ConnectionState> state_;
    uint64_t reconnectAttempt_;

    void reconnect() {
        auto lock = state_.lockExclusive();
        *lock = ConnectionState::RECONNECTING;
        reconnectAttempt_++;

        // Calculate backoff: 2^attempt * 100ms
        uint64_t delayMs = std::min<uint64_t>(1000, 1 << reconnectAttempt_) * 100;
        // Schedule reconnection...
    }
};
```

### WebSocket Message Handling

**Message Flow:**

1. Receive frame from KJ async stream
2. Decode frame (payload + metadata)
3. Route to appropriate handler
4. Emit to subscription manager

**KJ Async Pattern:**

```cpp
kj::Promise<void> processFrames(kj::Own<kj::AsyncIoStream> stream) {
    auto frames = stream->readAllText()
        .then([](kj::String data) {
            // Parse frames from data
            return processFrames(data);
        });
}
```

## Subscription Management

### Subscription Pattern

**Design**: Hierarchical subscription with change notifications

**Subscription States:**

```cpp
enum class SubscriptionState {
    REQUESTED,
    ACTIVE,
    PAUSED,
    CANCELLED
};
```

**Subscription Manager Interface:**

```cpp
class SubscriptionManager {
public:
    // Subscribe to symbol updates
    kj::Maybe<uint64_t> subscribeSymbol(const kj::String& symbol);

    // Unsubscribe from symbol
    void unsubscribeSymbol(uint64_t subscriptionId);

    // Pause subscription (don't receive updates)
    void pauseSubscription(uint64_t subscriptionId);

    // Resume subscription
    void resumeSubscription(uint64_t subscriptionId);

    // Get subscription state
    kj::Maybe<SubscriptionState> getSubscriptionState(uint64_t subscriptionId) const;
};
```

### Subscription Change Callbacks

Use observer pattern for subscription changes:

```cpp
using SubscriptionCallback = kj::Function<void(uint64_t, const kj::String& symbol, SubscriptionChange change)>;

class SubscriptionManager {
public:
    void addCallback(kj::Own<SubscriptionCallback> callback);

private:
    kj::Vector<kj::Own<SubscriptionCallback>> callbacks_;
};
```

## Kline Aggregation

### Aggregation Pattern

**Design**: Time-windowed aggregation from tick data

**Kline Periods:**

| Period | Description | Use Case |
|---------|-------------|----------|
| `1m` | 1-minute candles | Short-term trading |
| `5m` | 5-minute candles | Swing trading |
| `15m` | 15-minute candles | Day trading |
| `1h` | 1-hour candles | Position trading |

**Aggregator Interface:**

```cpp
class KlineAggregator {
public:
    // Update with tick data
    void onTick(const Tick& tick);

    // Get completed kline
    kj::Maybe<const Kline&> getKline(const kj::String& symbol, KlinePeriod period) const;

    // Clear aggregator state
    void clear();
};
```

### Tick Processing

**Design**: Single-threaded tick processing with priority handling

**Tick Structure:**

```cpp
struct Tick {
    kj::String symbol;
    double price;
    double quantity;
    uint64_t timestamp;
    int64_t sequenceNumber;
};
```

**Priority Handling:**

- Process high-priority ticks first
- Drop old ticks (based on sequence number)
- Aggregate ticks into klines

## Market Quality Monitoring

### Anomaly Detection

**Anomaly Types:**

```cpp
enum class AnomalyType {
    PRICE_SPIKE,       // Sudden price movement
    VOLUME_SURGE,      // Abnormal volume increase
    SPREAD_WIDENING,   // Unusual spread between bid/ask
    ORDER_IMBALANCE,  // Buy/sell volume imbalance
};
```

**Detection Rules:**

- Use statistical thresholds (e.g., 3 standard deviations)
- Time-windowed analysis (e.g., last 10 ticks)
- Configurable severity levels (INFO, WARNING, CRITICAL)

**Anomaly Reporting:**

```cpp
struct Anomaly {
    AnomalyType type;
    kj::String symbol;
    kj::String description;
    uint64_t timestamp;
    double severity;
};
```

## Thread Safety in Market Module

### Concurrency Patterns

```cpp
// ✅ CORRECT - Use kj::MutexGuarded<T> for thread-safe state
kj::MutexGuarded<OrderBookState> bookState_;

// ❌ FORBIDDEN - Separate mutex + manual lock/unlock
std::mutex bookMutex_;
bookMutex_.lock();
// ... access bookState ...
bookMutex_.unlock();
```

### Lock-Free Reads

Use atomic operations for frequently accessed counters:

```cpp
// ✅ CORRECT - Atomic counter
kj::Atomic<uint64_t> processedTicks_;

// ❌ FORBIDDEN - Mutex-protected counter
std::mutex counterMutex_;
std::lock_guard<std::mutex> lock(counterMutex_);
counter_++;
```

## Market Event Emission

### Event Types

| Event Type | Description | Fields |
|-------------|-------------|---------|
| `trade` | Trade execution | `symbol`, `price`, `qty`, `timestamp`, `side` |
| `ticker` | Best bid/ask update | `symbol`, `bid`, `ask`, `bidQty`, `askQty`, `timestamp` |
| `kline` | Kline candle close | `symbol`, `period`, `open`, `high`, `low`, `close`, `volume`, `timestamp` |
| `depth` | Order book snapshot | `symbol`, `bids`[], `asks`[], `timestamp` |
| `anomaly` | Quality issue | `symbol`, `type`, `description`, `severity`, `timestamp` |

### Event Publishing Pattern

```cpp
class MarketEventEmitter {
public:
    using EventCallback = kj::Function<void(const MarketEvent& event)>;

    // Subscribe to all market events
    void subscribe(kj::Own<EventCallback> callback);

    // Emit trade event
    void emitTrade(const Trade& trade);

    // Emit ticker event
    void emitTicker(const Ticker& ticker);

    // Emit kline event
    void emitKline(const Kline& kline);
};
```

## KJ Library Usage in Market Module

### String Handling

Use `kj::String` and `kj::StringPtr` for market symbols and descriptions:

```cpp
// ✅ CORRECT - KJ strings for symbol storage
kj::String symbol_ = "BTCUSDT"_kj;
kj::StringPtr symbolRef = symbol_;

// ❌ FORBIDDEN - std::string for symbols
std::string symbol = "BTCUSDT";
```

### Container Usage

Use `kj::Vector<T>` for dynamic market data collections:

```cpp
// ✅ CORRECT - KJ vector for price levels
kj::Vector<PriceLevel> levels_;

// ❌ FORBIDDEN - std::vector for price levels
std::vector<PriceLevel> levels_;
```

### Async Operations

Use KJ promises for WebSocket I/O and async market operations:

```cpp
// ✅ CORRECT - KJ promise for async WebSocket
kj::Promise<void> connectWebSocket(kj::StringPtr url) {
    return networkAddress.connect()
        .then([](kj::Own<kj::AsyncIoStream> stream) {
            // Handle WebSocket on stream...
        });
}
```

## Data Source Abstraction

### Data Source Interface

```cpp
class DataSource {
public:
    // Start data source
    virtual void start() = 0;

    // Stop data source
    virtual void stop() = 0;

    // Get connection status
    virtual bool isConnected() const = 0;

    // Get last update time
    virtual uint64_t lastUpdateTime() const = 0;
};
```

### Data Source Factory

```cpp
class DataSourceFactory {
public:
    static kj::Own<DataSource> createBinanceRest();
    static kj::Own<DataSource> createBinanceWebSocket();
};
```

## Market Data Quality Rules

### Data Validation

- **Timestamp consistency**: All timestamps use ISO 8601 format
- **Symbol validation**: Validate exchange symbols against whitelist
- **Price validation**: Reject invalid prices (negative, zero, out of range)
- **Quantity validation**: Reject invalid quantities (negative, zero)

### Data Integrity

- **Sequence tracking**: Use sequence numbers for detecting gaps/out-of-order
- **Checksum validation**: Verify data integrity for critical operations
- **Reconciliation**: Periodically compare order book state with exchange state

## Files to Check

- `/Users/bytedance/Develop/VeloZ/libs/market/include/veloz/market/order_book.h` - Order book interface
- `/Users/bytedance/Develop/VeloZ/libs/market/include/veloz/market/binance_websocket.h` - WebSocket implementation
- `/Users/bytedance/Develop/VeloZ/docs/kj/SKILL.md` - KJ library skill
- `/Users/bytedance/Develop/VeloZ/.claude/rules/encoding-style.md` - Encoding and style rules
- `/Users/bytedance/Develop/VeloZ/.claude/rules/cpp-engine.md` - C++ engine rules

## References

- KJ Library: https://github.com/capnproto/capnproto/tree/v2/c++/src/kj
- KJ Library Skill: Invoke `kj-library` skill for comprehensive KJ guidance
