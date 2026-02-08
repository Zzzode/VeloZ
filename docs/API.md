# VeloZ API 文档

## 1. 概述

VeloZ 提供了完整的 API 接口，允许开发者与系统进行交互，包括策略管理、市场数据获取、交易执行和风险管理等功能。

## 2. API 架构

### 2.1 通信协议

- **HTTP 协议**：用于控制平面操作（如策略创建、配置更新等）
- **WebSocket 协议**：用于数据平面操作（如实时市场数据推送、策略状态更新等）

### 2.2 数据格式

- **请求和响应格式**：JSON
- **编码**：UTF-8
- **时间戳**：Unix 时间戳（毫秒）

### 2.3 API 版本

API 版本通过 URL 路径进行区分，当前版本为 v1。

## 3. 认证和授权

### 3.1 API 密钥

- 所有 API 请求需要提供有效的 API 密钥
- API 密钥可以通过用户界面或 API 进行创建和管理
- API 密钥有不同的权限级别（如策略管理、交易执行、市场数据等）

### 3.2 请求签名

- 敏感操作需要进行请求签名以确保安全性
- 签名算法使用 HMAC-SHA256
- 签名需要包含时间戳以防止重放攻击

## 4. API 接口

### 4.1 策略管理 API

#### 4.1.1 创建策略

```
POST /api/v1/strategies
Content-Type: application/json

{
    "name": "My Strategy",
    "type": "TrendFollowing",
    "risk_per_trade": 0.01,
    "max_position_size": 1.0,
    "stop_loss": 0.05,
    "take_profit": 0.1,
    "symbols": ["BTCUSDT"],
    "parameters": {"param1": 10.0, "param2": 0.5}
}
```

**响应**：
```json
{
    "id": "strat-123456",
    "name": "My Strategy",
    "type": "TrendFollowing",
    "status": "stopped"
}
```

#### 4.1.2 获取策略列表

```
GET /api/v1/strategies
```

**响应**：
```json
{
    "data": [
        {
            "id": "strat-123456",
            "name": "My Strategy",
            "type": "TrendFollowing",
            "status": "running"
        },
        {
            "id": "strat-654321",
            "name": "Mean Reversion",
            "type": "MeanReversion",
            "status": "stopped"
        }
    ]
}
```

#### 4.1.3 启动策略

```
POST /api/v1/strategies/{strategy_id}/start
```

**响应**：
```json
{
    "id": "strat-123456",
    "status": "running"
}
```

#### 4.1.4 停止策略

```
POST /api/v1/strategies/{strategy_id}/stop
```

**响应**：
```json
{
    "id": "strat-123456",
    "status": "stopped"
}
```

#### 4.1.5 更新策略配置

```
PUT /api/v1/strategies/{strategy_id}
Content-Type: application/json

{
    "name": "My Updated Strategy",
    "parameters": {"param1": 15.0, "param2": 0.6}
}
```

**响应**：
```json
{
    "id": "strat-123456",
    "name": "My Updated Strategy",
    "parameters": {"param1": 15.0, "param2": 0.6}
}
```

### 4.2 市场数据 API

#### 4.2.1 获取行情数据

```
GET /api/v1/market/ticker?symbol=BTCUSDT
```

**响应**：
```json
{
    "symbol": "BTCUSDT",
    "price": 50000.0,
    "volume": 1000.0,
    "timestamp": 1620000000000
}
```

#### 4.2.2 获取订单簿数据

```
GET /api/v1/market/order_book?symbol=BTCUSDT&depth=10
```

**响应**：
```json
{
    "symbol": "BTCUSDT",
    "timestamp": 1620000000000,
    "bids": [
        {"price": 49999.0, "quantity": 0.1},
        {"price": 49998.0, "quantity": 0.2}
    ],
    "asks": [
        {"price": 50001.0, "quantity": 0.1},
        {"price": 50002.0, "quantity": 0.2}
    ]
}
```

#### 4.2.3 获取 K 线数据

```
GET /api/v1/market/kline?symbol=BTCUSDT&interval=1h&limit=24
```

**响应**：
```json
{
    "symbol": "BTCUSDT",
    "interval": "1h",
    "data": [
        {
            "open": 49000.0,
            "high": 51000.0,
            "low": 48000.0,
            "close": 50000.0,
            "volume": 10000.0,
            "timestamp": 1620000000000
        }
    ]
}
```

### 4.3 交易执行 API

#### 4.3.1 下单

```
POST /api/v1/orders
Content-Type: application/json

{
    "symbol": "BTCUSDT",
    "side": "buy",
    "quantity": 0.1,
    "price": 50000.0,
    "type": "limit"
}
```

**响应**：
```json
{
    "id": "ord-123456",
    "symbol": "BTCUSDT",
    "side": "buy",
    "quantity": 0.1,
    "price": 50000.0,
    "type": "limit",
    "status": "pending"
}
```

#### 4.3.2 取消订单

```
DELETE /api/v1/orders/{order_id}
```

**响应**：
```json
{
    "id": "ord-123456",
    "status": "cancelled"
}
```

#### 4.3.3 获取订单详情

```
GET /api/v1/orders/{order_id}
```

**响应**：
```json
{
    "id": "ord-123456",
    "symbol": "BTCUSDT",
    "side": "buy",
    "quantity": 0.1,
    "price": 50000.0,
    "type": "limit",
    "status": "filled",
    "filled_quantity": 0.1,
    "filled_price": 50000.0
}
```

### 4.4 风险管理 API

#### 4.4.1 获取账户风险状态

```
GET /api/v1/risk/account
```

**响应**：
```json
{
    "total_balance": 10000.0,
    "used_margin": 5000.0,
    "free_margin": 5000.0,
    "margin_level": 2.0,
    "max_drawdown": 0.1,
    "current_pnl": 1000.0
}
```

#### 4.4.2 获取策略风险状态

```
GET /api/v1/risk/strategies/{strategy_id}
```

**响应**：
```json
{
    "strategy_id": "strat-123456",
    "total_trades": 100,
    "win_rate": 0.6,
    "profit_factor": 1.5,
    "max_drawdown": 0.15,
    "current_pnl": 500.0
}
```

### 4.5 回测 API

#### 4.5.1 运行回测

```
POST /api/v1/backtest
Content-Type: application/json

{
    "strategy_config": {
        "name": "Backtest Strategy",
        "type": "TrendFollowing",
        "parameters": {"param1": 10.0, "param2": 0.5}
    },
    "symbol": "BTCUSDT",
    "timeframe": "1h",
    "start_date": "2023-01-01",
    "end_date": "2023-12-31"
}
```

**响应**：
```json
{
    "id": "bt-123456",
    "status": "running"
}
```

#### 4.5.2 获取回测结果

```
GET /api/v1/backtest/{backtest_id}
```

**响应**：
```json
{
    "id": "bt-123456",
    "status": "completed",
    "total_trades": 1000,
    "win_rate": 0.55,
    "profit_factor": 1.3,
    "max_drawdown": 0.2,
    "total_pnl": 10000.0
}
```

## 5. WebSocket API

### 5.1 市场数据订阅

#### 5.1.1 订阅行情数据

```
SUBSCRIBE /api/v1/ws/market/ticker
Content-Type: application/json

{
    "symbols": ["BTCUSDT", "ETHUSDT"]
}
```

**消息格式**：
```json
{
    "type": "ticker",
    "symbol": "BTCUSDT",
    "price": 50000.0,
    "volume": 1000.0,
    "timestamp": 1620000000000
}
```

#### 5.1.2 订阅订单簿数据

```
SUBSCRIBE /api/v1/ws/market/order_book
Content-Type: application/json

{
    "symbols": ["BTCUSDT"],
    "depth": 5
}
```

**消息格式**：
```json
{
    "type": "order_book",
    "symbol": "BTCUSDT",
    "timestamp": 1620000000000,
    "bids": [
        {"price": 49999.0, "quantity": 0.1},
        {"price": 49998.0, "quantity": 0.2}
    ],
    "asks": [
        {"price": 50001.0, "quantity": 0.1},
        {"price": 50002.0, "quantity": 0.2}
    ]
}
```

### 5.2 策略状态订阅

#### 5.2.1 订阅策略状态

```
SUBSCRIBE /api/v1/ws/strategies/{strategy_id}
```

**消息格式**：
```json
{
    "type": "strategy_state",
    "id": "strat-123456",
    "name": "My Strategy",
    "status": "running",
    "pnl": 1000.0,
    "trade_count": 10,
    "win_count": 6
}
```

### 5.3 订单状态订阅

#### 5.3.1 订阅订单状态

```
SUBSCRIBE /api/v1/ws/orders/{strategy_id}
```

**消息格式**：
```json
{
    "type": "order_status",
    "id": "ord-123456",
    "symbol": "BTCUSDT",
    "side": "buy",
    "status": "filled",
    "filled_quantity": 0.1,
    "filled_price": 50000.0
}
```

## 6. 错误处理

### 6.1 错误码

| 错误码 | 描述 |
|-------|------|
| 400 | 参数错误 |
| 401 | 认证失败 |
| 403 | 权限不足 |
| 404 | 资源未找到 |
| 409 | 资源冲突 |
| 500 | 服务器内部错误 |

### 6.2 错误响应格式

```json
{
    "code": 400,
    "message": "参数错误",
    "details": "参数 'symbol' 不能为空"
}
```

## 7. 最佳实践

### 7.1 请求频率限制

- 为了避免服务器过载，API 有请求频率限制
- 具体限制根据 API 类型和用户权限而定
- 请遵守 API 文档中规定的频率限制

### 7.2 错误处理

- 对每个 API 请求进行错误处理
- 根据错误码进行相应的处理逻辑
- 对重试机制进行适当的实现

### 7.3 数据格式验证

- 在发送请求前验证数据格式
- 对接收的数据进行格式验证
- 处理数据格式错误的情况

### 7.4 连接管理

- 对 WebSocket 连接进行适当的管理
- 处理连接断开和重连的情况
- 对长时间未使用的连接进行清理

## 8. 示例代码

### 8.1 Python 示例

```python
import requests
import json

# API 基础地址
base_url = "http://localhost:8080/api/v1"

# API 密钥
api_key = "your_api_key_here"

# 请求头
headers = {
    "Content-Type": "application/json",
    "X-API-Key": api_key
}

# 获取策略列表
def get_strategies():
    response = requests.get(f"{base_url}/strategies", headers=headers)
    if response.status_code == 200:
        return response.json()
    else:
        raise Exception(f"Error: {response.status_code} - {response.text}")

# 创建策略
def create_strategy(name, strategy_type, parameters):
    data = {
        "name": name,
        "type": strategy_type,
        "risk_per_trade": 0.01,
        "max_position_size": 1.0,
        "stop_loss": 0.05,
        "take_profit": 0.1,
        "symbols": ["BTCUSDT"],
        "parameters": parameters
    }
    response = requests.post(f"{base_url}/strategies", headers=headers, json=data)
    if response.status_code == 201:
        return response.json()
    else:
        raise Exception(f"Error: {response.status_code} - {response.text}")

# 启动策略
def start_strategy(strategy_id):
    response = requests.post(f"{base_url}/strategies/{strategy_id}/start", headers=headers)
    if response.status_code == 200:
        return response.json()
    else:
        raise Exception(f"Error: {response.status_code} - {response.text}")

# 获取行情数据
def get_ticker(symbol):
    response = requests.get(f"{base_url}/market/ticker?symbol={symbol}", headers=headers)
    if response.status_code == 200:
        return response.json()
    else:
        raise Exception(f"Error: {response.status_code} - {response.text}")

# 主函数
if __name__ == "__main__":
    try:
        # 获取策略列表
        strategies = get_strategies()
        print("策略列表:")
        for strategy in strategies["data"]:
            print(f"- {strategy['name']} ({strategy['type']}) - 状态: {strategy['status']}")

        # 创建新策略
        new_strategy = create_strategy("My Strategy", "TrendFollowing", {"param1": 10.0, "param2": 0.5})
        print(f"\n创建策略成功: {new_strategy['name']} (ID: {new_strategy['id']})")

        # 启动策略
        start_result = start_strategy(new_strategy["id"])
        print(f"启动策略成功: 状态 - {start_result['status']}")

        # 获取行情数据
        ticker = get_ticker("BTCUSDT")
        print(f"\nBTCUSDT 行情数据:")
        print(f"价格: {ticker['price']}")
        print(f"成交量: {ticker['volume']}")

    except Exception as e:
        print(f"\n错误: {e}")
```

### 8.2 JavaScript 示例

```javascript
const axios = require('axios');

// API 基础地址
const baseUrl = "http://localhost:8080/api/v1";

// API 密钥
const apiKey = "your_api_key_here";

// 请求配置
const axiosConfig = {
    headers: {
        "Content-Type": "application/json",
        "X-API-Key": apiKey
    }
};

// 获取策略列表
async function getStrategies() {
    try {
        const response = await axios.get(`${baseUrl}/strategies`, axiosConfig);
        return response.data;
    } catch (error) {
        throw new Error(`获取策略列表失败: ${error.response?.data?.message || error.message}`);
    }
}

// 创建策略
async function createStrategy(name, strategyType, parameters) {
    try {
        const data = {
            "name": name,
            "type": strategyType,
            "risk_per_trade": 0.01,
            "max_position_size": 1.0,
            "stop_loss": 0.05,
            "take_profit": 0.1,
            "symbols": ["BTCUSDT"],
            "parameters": parameters
        };
        const response = await axios.post(`${baseUrl}/strategies`, data, axiosConfig);
        return response.data;
    } catch (error) {
        throw new Error(`创建策略失败: ${error.response?.data?.message || error.message}`);
    }
}

// 启动策略
async function startStrategy(strategyId) {
    try {
        const response = await axios.post(`${baseUrl}/strategies/${strategyId}/start`, {}, axiosConfig);
        return response.data;
    } catch (error) {
        throw new Error(`启动策略失败: ${error.response?.data?.message || error.message}`);
    }
}

// 获取行情数据
async function getTicker(symbol) {
    try {
        const response = await axios.get(`${baseUrl}/market/ticker?symbol=${symbol}`, axiosConfig);
        return response.data;
    } catch (error) {
        throw new Error(`获取行情数据失败: ${error.response?.data?.message || error.message}`);
    }
}

// 主函数
async function main() {
    try {
        // 获取策略列表
        const strategies = await getStrategies();
        console.log("策略列表:");
        strategies.data.forEach(strategy => {
            console.log(`- ${strategy.name} (${strategy.type}) - 状态: ${strategy.status}`);
        });

        // 创建新策略
        const newStrategy = await createStrategy("My Strategy", "TrendFollowing", {"param1": 10.0, "param2": 0.5});
        console.log(`\n创建策略成功: ${newStrategy.name} (ID: ${newStrategy.id})`);

        // 启动策略
        const startResult = await startStrategy(newStrategy.id);
        console.log(`启动策略成功: 状态 - ${startResult.status}`);

        // 获取行情数据
        const ticker = await getTicker("BTCUSDT");
        console.log("\nBTCUSDT 行情数据:");
        console.log(`价格: ${ticker.price}`);
        console.log(`成交量: ${ticker.volume}`);

    } catch (error) {
        console.error(`\n错误: ${error.message}`);
    }
}

// 运行主函数
main();
```

## 9. 总结

VeloZ 提供了完整的 API 接口，允许开发者与系统进行交互。API 接口支持策略管理、市场数据获取、交易执行和风险管理等功能。通过使用这些 API，开发者可以创建复杂的量化交易系统和应用程序。
