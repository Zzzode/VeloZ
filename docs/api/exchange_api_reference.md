# Exchange API Reference

This document provides a reference for implementing exchange adapters in VeloZ. It documents API differences across supported exchanges to accelerate adapter development.

## Supported Exchanges

| Exchange | Adapter | Status | Task |
|----------|---------|--------|------|
| Binance | `BinanceAdapter` | Complete | - |
| OKX | `OKXAdapter` | 60% Complete | #10 |
| Bybit | `BybitAdapter` | Header Only | #12 |
| Coinbase | `CoinbaseAdapter` | Header Only | #13 |

## Authentication Patterns

### Binance

- **Header**: `X-MBX-APIKEY: <api_key>`
- **Signature**: HMAC-SHA256 of query string, hex-encoded
- **Signature Location**: Query parameter `&signature=<sig>`
- **Timestamp**: Query parameter `&timestamp=<ms>` (milliseconds)

```cpp
// Signature generation
kj::String signature = HmacSha256::sign(secret_key_, query_string);
params = kj::str(params, "&signature=", signature);
```

### OKX

- **Headers** (4 required):
  - `OK-ACCESS-KEY: <api_key>`
  - `OK-ACCESS-SIGN: <signature>`
  - `OK-ACCESS-TIMESTAMP: <iso_timestamp>`
  - `OK-ACCESS-PASSPHRASE: <passphrase>`
- **Signature**: HMAC-SHA256 of `timestamp + method + requestPath + body`, Base64-encoded
- **Demo Mode**: Add header `x-simulated-trading: 1`

```cpp
// Prehash string
kj::String prehash = kj::str(timestamp, method, request_path, body);
// Sign and Base64 encode
kj::String signature = base64_encode(HmacSha256::sign(secret_key_, prehash));
```

### Bybit (V5 API)

- **Headers**:
  - `X-BAPI-API-KEY: <api_key>`
  - `X-BAPI-SIGN: <signature>`
  - `X-BAPI-TIMESTAMP: <ms>`
  - `X-BAPI-RECV-WINDOW: <window>` (default 5000)
- **Signature**: HMAC-SHA256 of `timestamp + api_key + recv_window + query_string`, hex-encoded

```cpp
// Prehash string for GET requests
kj::String prehash = kj::str(timestamp, api_key_, recv_window, params);
kj::String signature = HmacSha256::sign(secret_key_, prehash);
```

### Coinbase (Advanced Trade API)

- **Header**: `Authorization: Bearer <jwt_token>`
- **Authentication**: JWT (JSON Web Token) signed with ES256 or EdDSA
- **JWT Claims**:
  - `sub`: API key name
  - `iss`: "cdp"
  - `nbf`: current timestamp (seconds)
  - `exp`: expiration (current + 120 seconds)
  - `uri`: `METHOD host/path`

```cpp
// JWT generation requires EC key signing (different from HMAC)
kj::String jwt = build_jwt_token(method, request_path);
headers.set("Authorization", kj::str("Bearer ", jwt));
```

## Symbol Format Mapping

| Exchange | Spot Format | Futures Format | Example |
|----------|-------------|----------------|---------|
| Binance | `BTCUSDT` | `BTCUSDT` | BTCUSDT |
| OKX | `BTC-USDT` | `BTC-USDT-SWAP` | BTC-USDT |
| Bybit | `BTCUSDT` | `BTCUSDT` (linear) | BTCUSDT |
| Coinbase | `BTC-USD` | N/A | BTC-USD |

### Symbol Conversion Functions

```cpp
// Binance: uppercase, no separator
kj::String BinanceAdapter::format_symbol(const SymbolId& symbol) {
    // BTCUSDT -> BTCUSDT (uppercase)
    return to_uppercase(symbol.value);
}

// OKX: uppercase with hyphen before quote currency
kj::String OKXAdapter::format_symbol(const SymbolId& symbol) {
    // BTCUSDT -> BTC-USDT
    if (ends_with(symbol.value, "USDT")) {
        return kj::str(symbol.value.slice(0, len-4), "-USDT");
    }
    return symbol.value;
}

// Coinbase: uppercase with hyphen, USD not USDT
kj::String CoinbaseAdapter::format_symbol(const SymbolId& symbol) {
    // BTCUSDT -> BTC-USD (note: different quote currency)
    // BTCUSD -> BTC-USD
}
```

## Timestamp Formats

| Exchange | Format | Example |
|----------|--------|---------|
| Binance | Milliseconds (int64) | `1708444800000` |
| OKX | ISO 8601 string | `2024-02-20T12:00:00.000Z` |
| Bybit | Milliseconds (int64) | `1708444800000` |
| Coinbase | Seconds (int64) for JWT | `1708444800` |

```cpp
// Binance/Bybit: milliseconds
int64_t get_timestamp_ms() const {
    auto now = std::chrono::system_clock::now();
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()).count();
}

// OKX: ISO 8601
kj::String get_timestamp_iso() const {
    // Returns: "2024-02-20T12:00:00.000Z"
}

// Coinbase: seconds
int64_t get_timestamp_sec() const {
    return std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
}
```

## Rate Limits

| Exchange | REST Limit | WebSocket Limit | Notes |
|----------|------------|-----------------|-------|
| Binance | 1200 req/min | 5 msg/sec | Weight-based system |
| OKX | 20 req/2sec per endpoint | 480 msg/hour | Per-endpoint limits |
| Bybit | 120 req/sec | 500 msg/sec | Category-based |
| Coinbase | 10 req/sec | 750 msg/sec | Per-product limits |

## Order Status Mapping

Map exchange-specific status strings to internal `OrderStatus` enum:

| Internal Status | Binance | OKX | Bybit | Coinbase |
|-----------------|---------|-----|-------|----------|
| `New` | `NEW` | - | `New` | `PENDING` |
| `Accepted` | - | `live` | `New` | `OPEN` |
| `PartiallyFilled` | `PARTIALLY_FILLED` | `partially_filled` | `PartiallyFilled` | `OPEN` (partial) |
| `Filled` | `FILLED` | `filled` | `Filled` | `FILLED` |
| `Canceled` | `CANCELED` | `canceled` | `Cancelled` | `CANCELLED` |
| `Rejected` | `REJECTED` | - | `Rejected` | `FAILED` |
| `Expired` | `EXPIRED` | - | `Deactivated` | `EXPIRED` |

```cpp
// Example: OKX status parsing
OrderStatus OKXAdapter::parse_order_status(kj::StringPtr status_str) {
    if (status_str == "live") return OrderStatus::Accepted;
    if (status_str == "partially_filled") return OrderStatus::PartiallyFilled;
    if (status_str == "filled") return OrderStatus::Filled;
    if (status_str == "canceled" || status_str == "mmp_canceled")
        return OrderStatus::Canceled;
    return OrderStatus::New;
}
```

## REST API Endpoints

### Place Order

| Exchange | Method | Endpoint | Body Format |
|----------|--------|----------|-------------|
| Binance | POST | `/api/v3/order` | URL-encoded params |
| OKX | POST | `/api/v5/trade/order` | JSON body |
| Bybit | POST | `/v5/order/create` | JSON body |
| Coinbase | POST | `/api/v3/brokerage/orders` | JSON body |

### Cancel Order

| Exchange | Method | Endpoint | Identifier |
|----------|--------|----------|------------|
| Binance | DELETE | `/api/v3/order` | `origClientOrderId` |
| OKX | POST | `/api/v5/trade/cancel-order` | `clOrdId` |
| Bybit | POST | `/v5/order/cancel` | `orderLinkId` |
| Coinbase | POST | `/api/v3/brokerage/orders/batch_cancel` | `order_ids` |

### Get Order Book

| Exchange | Endpoint | Depth Param |
|----------|----------|-------------|
| Binance | `/api/v3/depth` | `limit` (5,10,20,50,100,500,1000,5000) |
| OKX | `/api/v5/market/books` | `sz` (1-400) |
| Bybit | `/v5/market/orderbook` | `limit` (1-500) |
| Coinbase | `/api/v3/brokerage/market/product_book` | `limit` |

## WebSocket Streams

### Market Data Subscription

| Exchange | URL | Subscribe Format |
|----------|-----|------------------|
| Binance | `wss://stream.binance.com:9443/ws` | `{"method":"SUBSCRIBE","params":["btcusdt@depth"],"id":1}` |
| OKX | `wss://ws.okx.com:8443/ws/v5/public` | `{"op":"subscribe","args":[{"channel":"books","instId":"BTC-USDT"}]}` |
| Bybit | `wss://stream.bybit.com/v5/public/spot` | `{"op":"subscribe","args":["orderbook.50.BTCUSDT"]}` |
| Coinbase | `wss://advanced-trade-ws.coinbase.com` | `{"type":"subscribe","product_ids":["BTC-USD"],"channel":"level2"}` |

### Order Updates (Private)

| Exchange | URL | Auth Method |
|----------|-----|-------------|
| Binance | `wss://stream.binance.com:9443/ws/<listenKey>` | Listen key from REST API |
| OKX | `wss://ws.okx.com:8443/ws/v5/private` | Login message with signature |
| Bybit | `wss://stream.bybit.com/v5/private` | Auth message with signature |
| Coinbase | `wss://advanced-trade-ws.coinbase.com` | JWT in subscribe message |

### UserDataStream Authentication Details

#### Binance

Binance uses a listen key pattern for authenticated WebSocket streams:

```cpp
kj::Promise<void> BinanceAdapter::connect_user_stream() {
    // 1. Get listen key via REST API (POST /api/v3/userDataStream)
    listen_key_ = co_await get_listen_key();

    // 2. Connect WebSocket with listen key in URL
    user_stream_ = co_await connect_websocket(
        kj::str("wss://stream.binance.com:9443/ws/", listen_key_)
    );

    // 3. Start keepalive - Binance requires ping every 30 minutes
    //    PUT /api/v3/userDataStream with listenKey
    keepalive_task_ = keep_listen_key_alive();  // Ping every 25 min

    // 4. Start read loop for order/account updates
    co_await user_stream_read_loop();
}
```

#### OKX

OKX uses signed login message on WebSocket connection:

```cpp
kj::Promise<void> OKXAdapter::connect_user_stream() {
    // 1. Connect to private WebSocket endpoint
    user_stream_ = co_await connect_websocket(
        "wss://ws.okx.com:8443/ws/v5/private"
    );

    // 2. Send login message with signature
    // Sign: timestamp + "GET" + "/users/self/verify"
    auto login_msg = kj::str(
        "{\"op\":\"login\",\"args\":[{"
        "\"apiKey\":\"", api_key_, "\","
        "\"passphrase\":\"", passphrase_, "\","
        "\"timestamp\":\"", timestamp, "\","
        "\"sign\":\"", signature, "\"}]}"
    );
    co_await send_message(login_msg);

    // 3. Subscribe to order updates after login confirmed
    co_await subscribe_to_orders();  // {"op":"subscribe","args":[{"channel":"orders","instType":"SPOT"}]}
}
```

#### Bybit

Bybit uses signed auth message with expiry timestamp:

```cpp
kj::Promise<void> BybitAdapter::connect_user_stream() {
    user_stream_ = co_await connect_websocket(
        "wss://stream.bybit.com/v5/private"
    );

    // Auth message with signature and 10-second expiry window
    auto expires = now_ms() + 10000;  // 10 second window
    auto signature = hmac_sha256(secret_, kj::str("GET/realtime", expires));

    auto auth_msg = kj::str(
        R"({"op":"auth","args":[")", api_key_, R"(",)", expires, R"(",")", signature, R"("]})"
    );
    co_await send_message(auth_msg);
}
```

#### Coinbase

Coinbase Advanced Trade uses JWT-based WebSocket authentication:

```cpp
kj::Promise<void> CoinbaseAdapter::connect_user_stream() {
    // Generate JWT token (ES256 or EdDSA signed)
    auto jwt = generate_jwt(api_key_, secret_);

    user_stream_ = co_await connect_websocket(
        "wss://advanced-trade-ws.coinbase.com"
    );

    // JWT included in subscribe message
    auto subscribe_msg = kj::str(
        R"({"type":"subscribe","product_ids":["BTC-USD"],"channel":"user",)",
        R"("jwt":")", jwt, R"("})"
    );
    co_await send_message(subscribe_msg);
}

// Coinbase heartbeat handling - monitor subscription health
void CoinbaseAdapter::handle_message(const kj::String& msg) {
    auto json = parse_json(msg);
    auto type = json.get("type");

    if (type == "heartbeat"_kj) {
        // Subscription heartbeat received - subscription is alive
        last_heartbeat_time_ = now_ns();
        return;
    }

    // Handle other message types (orders, fills, etc.)
}

// If no heartbeat received within 60s, subscription is stale - resubscribe
void CoinbaseAdapter::check_subscription_health() {
    if (now_ns() - last_heartbeat_time_ > 60 * kj::SECONDS) {
        resubscribe();
    }
}
```

### UserDataStream Summary

| Exchange | Auth Method | Keepalive | Notes |
|----------|-------------|-----------|-------|
| Binance | Listen key (REST API) | 30min ping | Key expires after 60min without ping |
| OKX | Signed login message | Ping/pong | Sign: timestamp + method + path |
| Bybit | Signed auth with expiry | Ping/pong | 10-second signature window |
| Coinbase | JWT in subscribe message | 30s heartbeat | ES256 or EdDSA signed JWT |

## Error Handling

### Common Error Codes

| Exchange | Rate Limit | Invalid Signature | Insufficient Balance |
|----------|------------|-------------------|---------------------|
| Binance | -1015 | -1022 | -2010 |
| OKX | 50011 | 50113 | 51008 |
| Bybit | 10006 | 10004 | 110007 |
| Coinbase | `RATE_LIMIT_EXCEEDED` | `INVALID_SIGNATURE` | `INSUFFICIENT_FUND` |

## Implementation Checklist

When implementing a new exchange adapter:

1. [ ] Implement authentication (headers + signature)
2. [ ] Implement symbol format conversion
3. [ ] Implement timestamp generation
4. [ ] Implement `place_order_async` with JSON parsing
5. [ ] Implement `cancel_order_async` with JSON parsing
6. [ ] Implement `get_current_price_async`
7. [ ] Implement `get_order_book_async`
8. [ ] Implement `get_recent_trades_async`
9. [ ] Implement `get_account_balance_async`
10. [ ] Implement `connect_async` with server time validation
11. [ ] Implement synchronous wrappers using `io_context_.waitScope`
12. [ ] Add rate limiting logic
13. [ ] Add error code handling
14. [ ] Add unit tests

## References

- Binance API: https://binance-docs.github.io/apidocs/spot/en/
- OKX API: https://www.okx.com/docs-v5/en/
- Bybit API: https://bybit-exchange.github.io/docs/v5/intro
- Coinbase API: https://docs.cdp.coinbase.com/advanced-trade/docs/welcome
