# API Usage Guide

This guide provides practical examples for using the VeloZ HTTP API.

## Overview

### Architecture

```
+----------------+     HTTP/SSE      +----------------+     stdio      +----------------+
|    Client      | <--------------> |    Gateway     | <-----------> |    Engine      |
| (curl/Python)  |                  |   (Python)     |               |    (C++)       |
+----------------+                  +----------------+               +----------------+
```

- **Gateway**: HTTP server that bridges clients to the engine
- **Engine**: C++ core that processes orders and strategies
- **Protocol**: REST API + Server-Sent Events (SSE) for real-time updates

### Base URL

```
http://127.0.0.1:8080
```

### Response Format

All responses are JSON:

```json
{
  "ok": true,
  "data": { ... }
}
```

Error responses:

```json
{
  "error": "error_code",
  "message": "Human-readable description"
}
```

### Quick Test

```bash
# Verify the gateway is running
curl http://127.0.0.1:8080/health
# Response: {"ok": true}
```

---

## Authentication

VeloZ supports two authentication methods when `VELOZ_AUTH_ENABLED=true`.

### Method 1: JWT Tokens

JWT tokens are ideal for interactive sessions and web applications.

#### Step 1: Login

```bash
curl -X POST http://127.0.0.1:8080/api/auth/login \
  -H "Content-Type: application/json" \
  -d '{
    "user_id": "admin",
    "password": "your_password"
  }'
```

**Response:**
```json
{
  "access_token": "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9...",
  "refresh_token": "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9...",
  "token_type": "Bearer",
  "expires_in": 3600
}
```

#### Step 2: Use Access Token

Include the access token in the `Authorization` header:

```bash
# Get account balance
curl http://127.0.0.1:8080/api/account \
  -H "Authorization: Bearer eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9..."
```

#### Step 3: Refresh Token

When the access token expires (default: 1 hour), use the refresh token:

```bash
curl -X POST http://127.0.0.1:8080/api/auth/refresh \
  -H "Content-Type: application/json" \
  -d '{
    "refresh_token": "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9..."
  }'
```

**Response:**
```json
{
  "access_token": "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9...",
  "token_type": "Bearer",
  "expires_in": 3600
}
```

#### Step 4: Logout

Revoke the refresh token when done:

```bash
curl -X POST http://127.0.0.1:8080/api/auth/logout \
  -H "Content-Type: application/json" \
  -d '{
    "refresh_token": "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9..."
  }'
```

### Method 2: API Keys

API keys are ideal for automated trading bots and scripts.

#### Create an API Key

Requires admin permission:

```bash
curl -X POST http://127.0.0.1:8080/api/auth/keys \
  -H "Authorization: Bearer YOUR_ADMIN_TOKEN" \
  -H "Content-Type: application/json" \
  -d '{
    "name": "trading-bot",
    "permissions": ["read", "write"]
  }'
```

**Response:**
```json
{
  "key_id": "vk_abc123",
  "api_key": "veloz_abc123def456...",
  "message": "Store this key securely. It will not be shown again."
}
```

#### Use API Key

Include the API key in the `X-API-Key` header:

```bash
curl http://127.0.0.1:8080/api/account \
  -H "X-API-Key: veloz_abc123def456..."
```

#### List API Keys

```bash
curl http://127.0.0.1:8080/api/auth/keys \
  -H "Authorization: Bearer YOUR_TOKEN"
```

#### Revoke API Key

```bash
curl -X DELETE "http://127.0.0.1:8080/api/auth/keys?key_id=vk_abc123" \
  -H "Authorization: Bearer YOUR_ADMIN_TOKEN"
```

### Permissions

| Permission | Access |
|------------|--------|
| `read` | GET endpoints (market data, orders, positions) |
| `write` | POST endpoints (place/cancel orders) |
| `admin` | User and API key management |

---

## Trading Operations

### Place a Limit Order

```bash
curl -X POST http://127.0.0.1:8080/api/order \
  -H "Content-Type: application/json" \
  -H "Authorization: Bearer YOUR_TOKEN" \
  -d '{
    "side": "BUY",
    "symbol": "BTCUSDT",
    "qty": 0.001,
    "price": 50000.0,
    "client_order_id": "my-order-001"
  }'
```

**Response:**
```json
{
  "ok": true,
  "client_order_id": "my-order-001",
  "venue_order_id": "123456789"
}
```

**Parameters:**

| Parameter | Required | Description |
|-----------|----------|-------------|
| `side` | Yes | `BUY` or `SELL` |
| `symbol` | Yes | Trading pair (e.g., `BTCUSDT`) |
| `qty` | Yes | Order quantity |
| `price` | Yes | Limit price |
| `client_order_id` | No | Your order ID (auto-generated if omitted) |

### Place a Market Order

For market orders, set price to 0 or use a very high/low price:

```bash
# Market buy (use high price to ensure fill)
curl -X POST http://127.0.0.1:8080/api/order \
  -H "Content-Type: application/json" \
  -d '{
    "side": "BUY",
    "symbol": "BTCUSDT",
    "qty": 0.001,
    "price": 999999.0
  }'
```

### Cancel an Order

```bash
curl -X POST http://127.0.0.1:8080/api/cancel \
  -H "Content-Type: application/json" \
  -H "Authorization: Bearer YOUR_TOKEN" \
  -d '{
    "client_order_id": "my-order-001",
    "symbol": "BTCUSDT"
  }'
```

**Response:**
```json
{
  "ok": true,
  "client_order_id": "my-order-001"
}
```

### Get Order Status

```bash
curl "http://127.0.0.1:8080/api/order_state?client_order_id=my-order-001" \
  -H "Authorization: Bearer YOUR_TOKEN"
```

**Response:**
```json
{
  "client_order_id": "my-order-001",
  "symbol": "BTCUSDT",
  "side": "BUY",
  "order_qty": 0.001,
  "limit_price": 50000.0,
  "venue_order_id": "123456789",
  "status": "FILLED",
  "executed_qty": 0.001,
  "avg_price": 49998.5,
  "last_ts_ns": 1708689600000000000
}
```

**Order Status Values:**

| Status | Description |
|--------|-------------|
| `ACCEPTED` | Order accepted by exchange |
| `PARTIALLY_FILLED` | Partially executed |
| `FILLED` | Fully executed |
| `CANCELLED` | Cancelled by user |
| `REJECTED` | Rejected by exchange |
| `EXPIRED` | Order expired |

### List All Orders

```bash
curl http://127.0.0.1:8080/api/orders_state \
  -H "Authorization: Bearer YOUR_TOKEN"
```

**Response:**
```json
{
  "items": [
    {
      "client_order_id": "my-order-001",
      "symbol": "BTCUSDT",
      "side": "BUY",
      "order_qty": 0.001,
      "status": "FILLED",
      "executed_qty": 0.001,
      "avg_price": 49998.5
    },
    {
      "client_order_id": "my-order-002",
      "symbol": "ETHUSDT",
      "side": "SELL",
      "order_qty": 0.1,
      "status": "ACCEPTED",
      "executed_qty": 0.0
    }
  ]
}
```

### Get Positions

```bash
# All positions
curl http://127.0.0.1:8080/api/positions \
  -H "Authorization: Bearer YOUR_TOKEN"

# Specific symbol
curl http://127.0.0.1:8080/api/positions/BTCUSDT \
  -H "Authorization: Bearer YOUR_TOKEN"
```

**Response:**
```json
{
  "positions": [
    {
      "symbol": "BTCUSDT",
      "venue": "binance",
      "side": "LONG",
      "qty": 0.5,
      "avg_entry_price": 49500.0,
      "current_price": 50000.0,
      "unrealized_pnl": 250.0,
      "realized_pnl": 1200.0
    }
  ]
}
```

### Get Account Balance

```bash
curl http://127.0.0.1:8080/api/account \
  -H "Authorization: Bearer YOUR_TOKEN"
```

**Response:**
```json
{
  "ts_ns": 1708689600000000000,
  "balances": [
    {"asset": "USDT", "free": 9500.0, "locked": 500.0},
    {"asset": "BTC", "free": 0.5, "locked": 0.001}
  ]
}
```

---

## Risk Management

### Get Risk Metrics

Query the metrics endpoint for risk data:

```bash
curl http://127.0.0.1:8080/metrics | grep veloz_risk
```

**Key Risk Metrics:**

| Metric | Description |
|--------|-------------|
| `veloz_risk_var_95` | 95% Value at Risk |
| `veloz_risk_var_99` | 99% Value at Risk |
| `veloz_risk_current_drawdown` | Current drawdown from peak |
| `veloz_risk_max_drawdown` | Maximum historical drawdown |
| `veloz_risk_sharpe_ratio` | Sharpe ratio |
| `veloz_risk_sortino_ratio` | Sortino ratio |
| `veloz_risk_win_rate` | Win rate percentage |

### Get Exposure Metrics

```bash
curl http://127.0.0.1:8080/metrics | grep veloz_exposure
```

**Exposure Metrics:**

| Metric | Description |
|--------|-------------|
| `veloz_exposure_gross` | Total absolute exposure |
| `veloz_exposure_net` | Net exposure (long - short) |
| `veloz_exposure_leverage_ratio` | Leverage ratio |

### Get Concentration Metrics

```bash
curl http://127.0.0.1:8080/metrics | grep veloz_concentration
```

**Concentration Metrics:**

| Metric | Description |
|--------|-------------|
| `veloz_concentration_largest_pct` | Largest position % |
| `veloz_concentration_hhi` | Herfindahl-Hirschman Index |
| `veloz_concentration_position_count` | Number of positions |

---

## Market Data

### Get Current Price

```bash
curl http://127.0.0.1:8080/api/market
```

**Response:**
```json
{
  "symbol": "BTCUSDT",
  "price": 50000.0,
  "ts_ns": 1708689600000000000
}
```

### Subscribe to SSE Stream

Use Server-Sent Events for real-time updates:

```bash
curl -N http://127.0.0.1:8080/api/stream
```

**Event Types:**

```
event: market
data: {"symbol":"BTCUSDT","price":50000.0,"ts_ns":1708689600000000000}

event: order_update
data: {"client_order_id":"my-order-001","status":"FILLED","symbol":"BTCUSDT"}

event: fill
data: {"client_order_id":"my-order-001","symbol":"BTCUSDT","qty":0.001,"price":50000.0}

event: account
data: {"ts_ns":1708689600000000000}

event: error
data: {"message":"websocket_disconnected","ts_ns":1708689600000000000}
```

### Python SSE Client

```python
#!/usr/bin/env python3
"""VeloZ SSE client example."""

import json
import urllib.request
from datetime import datetime

def connect_sse(base_url="http://127.0.0.1:8080", token=None):
    """Connect to VeloZ SSE stream."""
    url = f"{base_url}/api/stream"
    req = urllib.request.Request(url)

    if token:
        req.add_header("Authorization", f"Bearer {token}")

    print(f"Connecting to {url}...")

    with urllib.request.urlopen(req) as response:
        buffer = ""
        for line in response:
            line = line.decode("utf-8")
            buffer += line

            if line == "\n" and buffer.strip():
                event = parse_sse_event(buffer)
                if event:
                    handle_event(event)
                buffer = ""

def parse_sse_event(buffer):
    """Parse SSE event from buffer."""
    event_type = None
    data = None

    for line in buffer.strip().split("\n"):
        if line.startswith("event:"):
            event_type = line[6:].strip()
        elif line.startswith("data:"):
            try:
                data = json.loads(line[5:].strip())
            except json.JSONDecodeError:
                continue

    if event_type and data:
        return {"type": event_type, "data": data}
    return None

def handle_event(event):
    """Handle incoming event."""
    ts = datetime.now().strftime("%H:%M:%S.%f")[:-3]
    event_type = event["type"]
    data = event["data"]

    if event_type == "market":
        print(f"[{ts}] MARKET: {data['symbol']} ${data['price']:.2f}")
    elif event_type == "order_update":
        print(f"[{ts}] ORDER: {data['client_order_id']} -> {data['status']}")
    elif event_type == "fill":
        print(f"[{ts}] FILL: {data['symbol']} {data['qty']} @ {data['price']}")
    elif event_type == "error":
        print(f"[{ts}] ERROR: {data.get('message')}")

if __name__ == "__main__":
    try:
        connect_sse()
    except KeyboardInterrupt:
        print("\nDisconnected.")
```

### Reconnection with Last Event ID

Resume from the last received event:

```bash
# Get last event ID from previous connection
# Then reconnect with that ID
curl -N "http://127.0.0.1:8080/api/stream?last_id=42"
```

---

## Multi-Exchange Operations

### Check Exchange Connectivity

```bash
curl http://127.0.0.1:8080/api/execution/ping
```

**Response:**
```json
{
  "ok": true,
  "result": {}
}
```

### Get Current Configuration

```bash
curl http://127.0.0.1:8080/api/config
```

**Response:**
```json
{
  "market_source": "binance_rest",
  "market_symbol": "BTCUSDT",
  "execution_mode": "binance_testnet_spot",
  "binance_trade_enabled": true,
  "binance_user_stream_connected": true,
  "auth_enabled": true
}
```

### Configure Exchange (Environment Variables)

```bash
# Binance
export VELOZ_EXECUTION_MODE=binance_testnet_spot
export VELOZ_BINANCE_API_KEY=<your-api-key>
export VELOZ_BINANCE_API_SECRET=<your-api-secret>

# OKX
export VELOZ_EXECUTION_MODE=okx_testnet
export VELOZ_OKX_API_KEY=<your-api-key>
export VELOZ_OKX_API_SECRET=<your-api-secret>
export VELOZ_OKX_PASSPHRASE=<your-passphrase>

# Bybit
export VELOZ_EXECUTION_MODE=bybit_testnet
export VELOZ_BYBIT_API_KEY=<your-api-key>
export VELOZ_BYBIT_API_SECRET=<your-api-secret>

# Coinbase
export VELOZ_EXECUTION_MODE=coinbase_sandbox
export VELOZ_COINBASE_API_KEY=<your-api-key>
export VELOZ_COINBASE_API_SECRET=<your-api-secret>
```

---

## Error Handling

### Common Error Codes

| HTTP Code | Error | Description | Solution |
|-----------|-------|-------------|----------|
| 400 | `bad_params` | Invalid parameters | Check request body |
| 400 | `bad_json` | Invalid JSON | Validate JSON syntax |
| 401 | `authentication_required` | No auth provided | Add Authorization header |
| 401 | `invalid_token` | Token expired/invalid | Refresh or re-login |
| 401 | `invalid_api_key` | Bad API key | Check key is valid |
| 403 | `forbidden` | Insufficient permissions | Use higher permission key |
| 404 | `not_found` | Resource not found | Check order ID exists |
| 429 | `rate_limit_exceeded` | Too many requests | Wait and retry |
| 500 | `binance_not_configured` | Missing credentials | Set API keys |

### Error Response Format

```json
{
  "error": "bad_params",
  "message": "qty must be greater than 0"
}
```

### Retry Strategy

```python
import time
import urllib.request
import json

def api_request_with_retry(url, data=None, headers=None, max_retries=3):
    """Make API request with exponential backoff retry."""
    headers = headers or {}
    headers["Content-Type"] = "application/json"

    for attempt in range(max_retries):
        try:
            req = urllib.request.Request(url, headers=headers)
            if data:
                req.data = json.dumps(data).encode()

            with urllib.request.urlopen(req, timeout=30) as response:
                return json.loads(response.read())

        except urllib.error.HTTPError as e:
            if e.code == 429:  # Rate limited
                retry_after = int(e.headers.get("Retry-After", 5))
                print(f"Rate limited. Waiting {retry_after}s...")
                time.sleep(retry_after)
            elif e.code >= 500:  # Server error
                wait_time = 2 ** attempt
                print(f"Server error. Retrying in {wait_time}s...")
                time.sleep(wait_time)
            else:
                raise

        except Exception as e:
            if attempt < max_retries - 1:
                wait_time = 2 ** attempt
                print(f"Error: {e}. Retrying in {wait_time}s...")
                time.sleep(wait_time)
            else:
                raise

    raise Exception("Max retries exceeded")
```

---

## Best Practices

### 1. Rate Limiting

VeloZ enforces rate limits. Check response headers:

```bash
curl -I http://127.0.0.1:8080/api/market
```

**Rate Limit Headers:**

| Header | Description |
|--------|-------------|
| `X-RateLimit-Limit` | Max requests per window |
| `X-RateLimit-Remaining` | Remaining requests |
| `X-RateLimit-Reset` | Reset timestamp |
| `Retry-After` | Seconds to wait (on 429) |

### 2. Idempotency

Use `client_order_id` to prevent duplicate orders:

```bash
# Same client_order_id = same order (idempotent)
curl -X POST http://127.0.0.1:8080/api/order \
  -H "Content-Type: application/json" \
  -d '{
    "side": "BUY",
    "symbol": "BTCUSDT",
    "qty": 0.001,
    "price": 50000.0,
    "client_order_id": "unique-order-id-12345"
  }'
```

### 3. Timeout Handling

Set appropriate timeouts for your use case:

```python
# Short timeout for market data
urllib.request.urlopen(url, timeout=5)

# Longer timeout for order placement
urllib.request.urlopen(url, timeout=30)
```

### 4. Connection Management

For high-frequency operations, use connection pooling:

```python
import urllib3

# Create connection pool
http = urllib3.PoolManager(
    num_pools=10,
    maxsize=10,
    retries=urllib3.Retry(total=3, backoff_factor=0.1)
)

# Reuse connections
response = http.request("GET", "http://127.0.0.1:8080/api/market")
```

### 5. Token Management

```python
class TokenManager:
    """Manage JWT token lifecycle."""

    def __init__(self, base_url, user_id, password):
        self.base_url = base_url
        self.user_id = user_id
        self.password = password
        self.access_token = None
        self.refresh_token = None
        self.expires_at = 0

    def get_token(self):
        """Get valid access token, refreshing if needed."""
        import time

        if time.time() >= self.expires_at - 60:  # Refresh 1 min early
            if self.refresh_token:
                self._refresh()
            else:
                self._login()

        return self.access_token

    def _login(self):
        """Login to get new tokens."""
        import urllib.request
        import json

        url = f"{self.base_url}/api/auth/login"
        data = {"user_id": self.user_id, "password": self.password}

        req = urllib.request.Request(url)
        req.add_header("Content-Type", "application/json")
        req.data = json.dumps(data).encode()

        with urllib.request.urlopen(req) as response:
            result = json.loads(response.read())
            self.access_token = result["access_token"]
            self.refresh_token = result["refresh_token"]
            self.expires_at = time.time() + result["expires_in"]

    def _refresh(self):
        """Refresh access token."""
        import urllib.request
        import json

        url = f"{self.base_url}/api/auth/refresh"
        data = {"refresh_token": self.refresh_token}

        req = urllib.request.Request(url)
        req.add_header("Content-Type", "application/json")
        req.data = json.dumps(data).encode()

        try:
            with urllib.request.urlopen(req) as response:
                result = json.loads(response.read())
                self.access_token = result["access_token"]
                self.expires_at = time.time() + result["expires_in"]
        except urllib.error.HTTPError:
            # Refresh token invalid, re-login
            self._login()
```

### 6. Secure Credential Storage

Never hardcode credentials:

```python
import os

# Load from environment
api_key = os.environ.get("VELOZ_API_KEY")
if not api_key:
    raise ValueError("VELOZ_API_KEY not set")

# Or load from file
with open(os.path.expanduser("~/.veloz/credentials"), "r") as f:
    credentials = json.load(f)
```

---

## Complete Python Client Example

```python
#!/usr/bin/env python3
"""Complete VeloZ API client example."""

import json
import urllib.request
import urllib.error
import time
import os

class VeloZClient:
    """VeloZ API client with authentication and error handling."""

    def __init__(self, base_url="http://127.0.0.1:8080"):
        self.base_url = base_url
        self.token = None

    def login(self, user_id, password):
        """Login and store access token."""
        result = self._post("/api/auth/login", {
            "user_id": user_id,
            "password": password
        })
        self.token = result["access_token"]
        return result

    def set_api_key(self, api_key):
        """Use API key for authentication."""
        self.token = api_key
        self._use_api_key = True

    def get_market(self):
        """Get current market data."""
        return self._get("/api/market")

    def get_account(self):
        """Get account balance."""
        return self._get("/api/account")

    def get_positions(self):
        """Get all positions."""
        return self._get("/api/positions")

    def place_order(self, side, symbol, qty, price, client_order_id=None):
        """Place a limit order."""
        data = {
            "side": side,
            "symbol": symbol,
            "qty": qty,
            "price": price
        }
        if client_order_id:
            data["client_order_id"] = client_order_id
        return self._post("/api/order", data)

    def cancel_order(self, client_order_id, symbol=None):
        """Cancel an order."""
        data = {"client_order_id": client_order_id}
        if symbol:
            data["symbol"] = symbol
        return self._post("/api/cancel", data)

    def get_order(self, client_order_id):
        """Get order status."""
        return self._get(f"/api/order_state?client_order_id={client_order_id}")

    def get_orders(self):
        """Get all orders."""
        return self._get("/api/orders_state")

    def _get(self, path):
        """Make GET request."""
        return self._request("GET", path)

    def _post(self, path, data):
        """Make POST request."""
        return self._request("POST", path, data)

    def _request(self, method, path, data=None):
        """Make HTTP request with retry logic."""
        url = f"{self.base_url}{path}"
        headers = {"Content-Type": "application/json"}

        if self.token:
            if hasattr(self, "_use_api_key") and self._use_api_key:
                headers["X-API-Key"] = self.token
            else:
                headers["Authorization"] = f"Bearer {self.token}"

        req = urllib.request.Request(url, headers=headers, method=method)
        if data:
            req.data = json.dumps(data).encode()

        for attempt in range(3):
            try:
                with urllib.request.urlopen(req, timeout=30) as response:
                    return json.loads(response.read())
            except urllib.error.HTTPError as e:
                if e.code == 429:
                    retry_after = int(e.headers.get("Retry-After", 5))
                    time.sleep(retry_after)
                elif e.code >= 500:
                    time.sleep(2 ** attempt)
                else:
                    error_body = e.read().decode()
                    raise Exception(f"HTTP {e.code}: {error_body}")
            except Exception as e:
                if attempt < 2:
                    time.sleep(2 ** attempt)
                else:
                    raise

        raise Exception("Max retries exceeded")


# Usage example
if __name__ == "__main__":
    client = VeloZClient()

    # Check if auth is needed
    auth_enabled = os.environ.get("VELOZ_AUTH_ENABLED", "false") == "true"

    if auth_enabled:
        password = os.environ.get("VELOZ_ADMIN_PASSWORD")
        if password:
            client.login("admin", password)
            print("Logged in successfully")

    # Get market data
    market = client.get_market()
    print(f"Market: {market['symbol']} @ ${market['price']}")

    # Get account
    account = client.get_account()
    for balance in account.get("balances", []):
        print(f"  {balance['asset']}: {balance['free']} (free) + {balance['locked']} (locked)")

    # Place an order
    order = client.place_order(
        side="BUY",
        symbol="BTCUSDT",
        qty=0.001,
        price=50000.0,
        client_order_id=f"test-{int(time.time())}"
    )
    print(f"Order placed: {order['client_order_id']}")

    # Check order status
    time.sleep(1)
    status = client.get_order(order["client_order_id"])
    print(f"Order status: {status['status']}")
```

---

## Related Documentation

- [HTTP API Reference](../../api/http_api.md) - Complete endpoint reference
- [SSE API](../../api/sse_api.md) - Server-Sent Events documentation
- [Configuration Guide](configuration.md) - Environment variables
- [Monitoring Guide](monitoring.md) - Metrics and observability
