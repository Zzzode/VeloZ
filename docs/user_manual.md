# VeloZ 量化交易框架用户使用手册

## 1. 概述

VeloZ 是一个面向加密货币市场的量化交易框架，提供统一的回测/模拟/实盘交易运行时模型和可演进的工程架构。本手册将帮助用户快速上手使用 VeloZ 框架。

## 2. 系统安装

### 2.1 环境要求

- **操作系统**：Linux（推荐 Ubuntu 20.04 或 CentOS 8），Windows 或 macOS（仅用于开发环境）
- **编译器**：GCC 10 或更高版本，支持 C++20 标准
- **构建工具**：CMake 3.24 或更高版本
- **Python**：Python 3.8 或更高版本

### 2.2 安装步骤

#### 2.2.1 克隆代码

```bash
git clone https://github.com/your-username/VeloZ.git
cd VeloZ
```

#### 2.2.2 构建项目

```bash
# 配置 CMake
cmake --preset dev

# 构建所有目标
cmake --build --preset dev-all -j$(nproc)
```

#### 2.2.3 验证安装

```bash
# 运行引擎
./build/dev/apps/engine/veloz_engine

# 或使用脚本运行
./scripts/run_engine.sh dev
```

## 3. 快速开始

### 3.1 启动系统

#### 3.1.1 启动引擎

```bash
./build/dev/apps/engine/veloz_engine --stdio
```

#### 3.1.2 启动网关

```bash
cd apps/gateway
python3 gateway.py
```

#### 3.1.3 访问 UI

打开 `apps/ui/index.html` 文件，或通过浏览器访问 `http://127.0.0.1:8080`（如果网关正在运行）。

### 3.2 创建第一个策略

#### 3.2.1 策略配置

创建策略配置文件 `strategy_config.json`：

```json
{
  "name": "My First Strategy",
  "type": "TrendFollowing",
  "risk_per_trade": 0.01,
  "max_position_size": 1.0,
  "stop_loss": 0.05,
  "take_profit": 0.1,
  "symbols": ["BTCUSDT"],
  "parameters": {"param1": 10.0, "param2": 0.5}
}
```

#### 3.2.2 启动策略

使用 HTTP API 启动策略：

```bash
curl -X POST http://127.0.0.1:8080/api/v1/strategies \
  -H "Content-Type: application/json" \
  -d @strategy_config.json
```

## 4. 策略管理

### 4.1 策略创建

#### 4.1.1 使用 UI 创建

1. 打开 VeloZ UI
2. 导航到「策略管理」页面
3. 点击「创建策略」按钮
4. 填写策略信息并保存

#### 4.1.2 使用 API 创建

```bash
curl -X POST http://127.0.0.1:8080/api/v1/strategies \
  -H "Content-Type: application/json" \
  -d '{
    "name": "My Strategy",
    "type": "TrendFollowing",
    "risk_per_trade": 0.01,
    "max_position_size": 1.0,
    "stop_loss": 0.05,
    "take_profit": 0.1,
    "symbols": ["BTCUSDT"],
    "parameters": {"param1": 10.0, "param2": 0.5}
  }'
```

### 4.2 策略启动和停止

#### 4.2.1 启动策略

```bash
curl -X POST http://127.0.0.1:8080/api/v1/strategies/{strategy_id}/start
```

#### 4.2.2 停止策略

```bash
curl -X POST http://127.0.0.1:8080/api/v1/strategies/{strategy_id}/stop
```

### 4.3 策略监控

#### 4.3.1 查看策略状态

```bash
curl -X GET http://127.0.0.1:8080/api/v1/strategies/{strategy_id}
```

#### 4.3.2 查看所有策略

```bash
curl -X GET http://127.0.0.1:8080/api/v1/strategies
```

## 5. 市场数据

### 5.1 获取行情数据

```bash
curl -X GET "http://127.0.0.1:8080/api/v1/market/ticker?symbol=BTCUSDT"
```

### 5.2 获取订单簿数据

```bash
curl -X GET "http://127.0.0.1:8080/api/v1/market/order_book?symbol=BTCUSDT&depth=10"
```

### 5.3 获取 K 线数据

```bash
curl -X GET "http://127.0.0.1:8080/api/v1/market/kline?symbol=BTCUSDT&interval=1h&limit=24"
```

## 6. 回测系统

### 6.1 运行回测

```bash
curl -X POST http://127.0.0.1:8080/api/v1/backtest \
  -H "Content-Type: application/json" \
  -d '{
    "strategy_config": {
      "name": "Backtest Strategy",
      "type": "TrendFollowing",
      "parameters": {"param1": 10.0, "param2": 0.5}
    },
    "symbol": "BTCUSDT",
    "timeframe": "1h",
    "start_date": "2023-01-01",
    "end_date": "2023-12-31"
  }'
```

### 6.2 查询回测结果

```bash
curl -X GET http://127.0.0.1:8080/api/v1/backtest/{backtest_id}
```

## 7. 实盘交易

### 7.1 配置交易所 API 密钥

创建 API 密钥配置文件 `~/.veloz/api_keys.json`：

```json
{
  "binance": {
    "api_key": "your_binance_api_key",
    "api_secret": "your_binance_api_secret"
  }
}
```

设置文件权限：

```bash
chmod 600 ~/.veloz/api_keys.json
```

### 7.2 连接到交易所

```bash
curl -X POST http://127.0.0.1:8080/api/v1/exchange/connect \
  -H "Content-Type: application/json" \
  -d '{"exchange": "binance"}'
```

### 7.3 下单交易

```bash
curl -X POST http://127.0.0.1:8080/api/v1/orders \
  -H "Content-Type: application/json" \
  -d '{
    "symbol": "BTCUSDT",
    "side": "buy",
    "quantity": 0.1,
    "price": 50000.0,
    "type": "limit"
  }'
```

## 8. 风险管理

### 8.1 查看账户风险状态

```bash
curl -X GET http://127.0.0.1:8080/api/v1/risk/account
```

### 8.2 查看策略风险状态

```bash
curl -X GET http://127.0.0.1:8080/api/v1/risk/strategies/{strategy_id}
```

## 9. 系统监控

### 9.1 查看系统状态

```bash
curl -X GET http://127.0.0.1:8080/api/v1/health
```

### 9.2 查看系统日志

```bash
# 查看引擎日志
tail -f /path/to/VeloZ/logs/engine.log

# 查看网关日志
tail -f /path/to/VeloZ/logs/gateway.log
```

## 10. 常见问题

### 10.1 策略未产生信号

- 检查策略配置是否正确
- 检查市场数据是否正常
- 检查策略参数是否合适

### 10.2 无法连接到交易所

- 检查 API 密钥是否正确
- 检查网络连接是否正常
- 检查交易所服务器是否可用

### 10.3 回测结果不理想

- 调整策略参数
- 尝试不同的时间周期
- 检查历史数据质量

### 10.4 系统性能问题

- 检查系统资源使用情况
- 优化策略代码
- 调整系统配置

## 11. 技术支持

### 11.1 社区支持

- GitHub 仓库：https://github.com/your-username/VeloZ
- 问题跟踪：https://github.com/your-username/VeloZ/issues
- 讨论区：https://github.com/your-username/VeloZ/discussions

### 11.2 文档资源

- 产品需求文档：`docs/product_requirements.md`
- 策略开发指南：`docs/strategy_development_guide.md`
- API 文档：`docs/API.md`
- 部署和运维文档：`docs/deployment_operations.md`

### 11.3 联系方式

- 邮箱：support@veloz.com
- 微信群：添加微信 `veloz_support` 加入用户群

## 12. 版本更新

### 12.1 检查更新

```bash
git status
git log --oneline | head -10
```

### 12.2 更新代码

```bash
git pull origin master
cmake --build --preset dev-all -j$(nproc)
```

### 12.3 版本历史

- **v0.1.0**：初始版本，包含基础策略框架和回测系统
- **v0.2.0**：添加实盘交易支持和风险管理系统
- **v0.3.0**：优化策略执行引擎和市场数据系统

## 13. 许可证

VeloZ 量化交易框架遵循 MIT 许可证。详情请查看 `LICENSE` 文件。
