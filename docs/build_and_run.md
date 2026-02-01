# VeloZ 构建与运行

本文件提供 VeloZ 的本地构建、运行、格式化与 CI 行为说明。仓库目前以 C++23 引擎骨架为主，后续模块会逐步落地。

## 1. 环境要求

### 1.1 必备

- Linux / macOS（Windows 也可，但本文以类 Unix 为例）
- CMake >= 3.24
- 支持 C++23 的编译器
  - Linux：Clang 16+ 或 GCC 13+（以系统实际可用为准）

### 1.2 可选

- clang-format（用于 `./scripts/format.sh`）

## 2. 使用 CMake Presets 构建（推荐）

仓库提供 [CMakePresets.json](../CMakePresets.json)：

- dev：Debug + 生成 compile_commands.json
- release：Release + 生成 compile_commands.json
- asan：Clang + ASan/UBSan（用于本地自检）

### 2.1 Dev 构建

```bash
cmake --preset dev
cmake --build --preset dev -j
```

构建产物默认位于：

- `build/dev/`
- 引擎可执行文件：`build/dev/apps/engine/veloz_engine`

### 2.2 Release 构建

```bash
cmake --preset release
cmake --build --preset release -j
```

### 2.3 ASan 自检构建（可选）

```bash
cmake --preset asan
cmake --build --preset asan -j
```

## 3. 运行

### 3.1 直接运行引擎

```bash
./build/dev/apps/engine/veloz_engine
```

当前引擎为冒烟骨架：会周期性输出 heartbeat，并响应 SIGINT/SIGTERM 优雅退出。

### 3.2 使用脚本运行（推荐）

```bash
./scripts/run_engine.sh dev
```

该脚本会：

- 先调用 `./scripts/build.sh <preset>` 构建
- 再运行引擎，并使用 `timeout` 做短时冒烟

### 3.3 运行网关与 UI（最小联动）

网关使用 Python 标准库启动 HTTP 服务，并通过 stdio 桥接到引擎：

```bash
./scripts/run_gateway.sh dev
```

默认监听地址：

- `http://127.0.0.1:8080/`

如果你直接用 `file://` 打开 `apps/ui/index.html`（例如 IDE 里直接预览文件），页面顶部的 `API Base` 需要填入 `http://127.0.0.1:8080`，或通过 URL 参数指定：

- `.../index.html?api=http://127.0.0.1:8080`

提供的接口：

- `GET /health`
- `GET /api/config`
- `GET /api/market`
- `GET /api/orders`
- `GET /api/orders_state`（订单状态视图）
- `GET /api/order_state?client_order_id=...`
- `GET /api/stream`（SSE：实时事件流）
- `GET /api/execution/ping`
- `GET /api/account`
- `POST /api/order`（JSON 或 x-www-form-urlencoded）
- `POST /api/cancel`（取消单）

订单状态视图说明：

- `orders` 为事件列表（便于调试与回放）
- `orders_state` 为状态归并视图（便于 UI/策略查询），会根据 `fill` 推导 `PARTIALLY_FILLED/FILLED`

### 3.4 行情源切换（可选：Binance REST 轮询）

默认行情由引擎模拟产生（`sim`）。你也可以在启动网关前通过环境变量切换为 Binance 公共 REST 轮询：

```bash
export VELOZ_MARKET_SOURCE=binance_rest
export VELOZ_MARKET_SYMBOL=BTCUSDT
export VELOZ_BINANCE_BASE_URL=https://api.binance.com
./scripts/run_gateway.sh dev
```

### 3.5 执行通道切换（可选：Binance Spot Testnet REST + 用户数据流）

默认下单/撤单会走本地引擎模拟执行（`sim_engine`）。你也可以切换为 Binance 现货 Testnet 的 REST 下单/撤单（需要 API key/secret）：

```bash
export VELOZ_EXECUTION_MODE=binance_testnet_spot
export VELOZ_BINANCE_TRADE_BASE_URL=https://testnet.binance.vision
export VELOZ_BINANCE_API_KEY=...
export VELOZ_BINANCE_API_SECRET=...
./scripts/run_gateway.sh dev
```

说明：

- 出于安全考虑，网关不会打印/回显 API secret
- 网关会自动申请 listenKey 并连接用户数据流（WS），用于获取 `executionReport`（订单/成交回报）与 `outboundAccountPosition`（余额变化），并对外提供：
  - `GET /api/account`
  - SSE：`event: account`
- 如需覆盖默认 WS 地址，可设置：

```bash
export VELOZ_BINANCE_WS_BASE_URL=wss://testnet.binance.vision/ws
```

## 4. 常用脚本

### 4.1 构建脚本

```bash
./scripts/build.sh dev
./scripts/build.sh release
./scripts/build.sh asan
```

对应脚本：[build.sh](../scripts/build.sh)

### 4.2 格式化脚本

```bash
./scripts/format.sh
```

对应脚本：[format.sh](../scripts/format.sh)

说明：

- 默认扫描 `apps/`、`libs/` 下的 `*.h/*.hpp/*.cc/*.cpp/*.cxx`
- 直接原地格式化（`clang-format -i`）

## 5. CI 行为

GitHub Actions 工作流位于：[ci.yml](../.github/workflows/ci.yml)

当前 CI 做三件事：

- 使用 `cmake --preset dev` 配置
- 使用 `cmake --build --preset dev -j` 构建
- 运行引擎冒烟（timeout 3s）

## 6. 常见问题

### 6.1 找不到 `cmake --preset`

请确认本机 CMake 版本 >= 3.19（本仓库要求 >= 3.24）。

### 6.2 没有 `clang-format`

格式化脚本会直接报错退出。你可以安装 clang-format，或先暂时不执行格式化脚本。
