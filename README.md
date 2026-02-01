# VeloZ

VeloZ 是一个面向加密货币市场的量化交易框架（C++23 核心引擎 + 多语言生态），目标是提供统一的回测/仿真/实盘运行模型，并在低延迟关键路径上具备可演进的工程结构。

## 状态

当前仓库处于早期阶段：已提供可编译可运行的 C++23 引擎骨架、基础模块划分（core/market/exec/oms/risk）以及标准化的构建/CI 配置。

## 快速开始

### 构建（推荐：CMake Presets）

```bash
cmake --preset dev
cmake --build --preset dev -j
```

### 运行引擎（冒烟）

```bash
./scripts/run_engine.sh dev
```

### 运行网关 + UI（最小联动）

```bash
./scripts/run_gateway.sh dev
```

默认监听 `http://127.0.0.1:8080/`。
如果直接打开 `apps/ui/index.html`（file://），请在页面顶部填写 API Base 为 `http://127.0.0.1:8080`。

可选：切换网关行情源（Binance REST 轮询，离线环境会自动回退到模拟行情）：

```bash
VELOZ_MARKET_SOURCE=binance_rest ./scripts/run_gateway.sh dev
```

可选：切换执行通道（Binance Spot Testnet REST，需要 API key/secret）：

```bash
VELOZ_EXECUTION_MODE=binance_testnet_spot ./scripts/run_gateway.sh dev
```

### 格式化 C++ 代码

```bash
./scripts/format.sh
```

更多说明见：[构建与运行](docs/build_and_run.md)

## 仓库结构（摘要）

- apps/engine：C++23 引擎可执行程序（当前为骨架）
- libs/core：基础设施（日志、时间、事件循环）
- libs/market：行情事件模型（占位）
- libs/exec：交易接口模型（占位）
- libs/oms：订单记录与状态聚合（最小实现）
- libs/risk：风控检查（最小实现）
- docs：设计与使用文档

## 设计文档

- [crypto_quant_framework_design.md](docs/crypto_quant_framework_design.md)

## 开源协议

Apache License 2.0，详见 [LICENSE](LICENSE)。
