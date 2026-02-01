# VeloZ 加密货币量化交易框架：完整方案设计

## 1. 目标与范围

### 1.1 目标

- 覆盖主流交易所（以币安为基准）现货、U 本位合约、币本位合约的行情与交易
- 低延迟、高吞吐的行情获取与交易执行能力，支持高频/做市/网格等策略形态
- 统一的“回测 / 仿真 / 实盘”策略运行接口，减少策略迁移成本
- 完整的统计与风控闭环：收益、风险、仓位、交易质量、回测报告
- AI Agent 接入：行情解读、策略建议、交易复盘、异常检测与解释
- 提供可视化交易界面：监控、策略管理、手动干预、风控状态、审计追踪

### 1.2 非目标（可后续迭代）

- 不以跨链/链上 DeFi 交易为第一目标（后续可扩展链上执行器）
- 不以“完全自动化无人值守”的 AI 交易为默认模式（默认人类可审计与可控）
- 不强依赖某单一语言/框架，优先保证核心路径性能与系统可维护性

### 1.3 核心原则

- 事件驱动：行情、订单、成交、账户、风控等均以事件流贯穿
- 分层解耦：交易所适配层、核心服务层、策略层、展示层松耦合
- 一致性优先：订单状态机、仓位/资金同步、幂等与可回放
- 可观测性优先：端到端延迟、丢包/重连、撮合回执、错误码与重试策略可追踪
- 安全优先：密钥最小权限、密钥隔离、签名与审计日志

## 2. 总体架构

### 2.1 逻辑架构（建议：模块化单体 + 可拆分服务）

在早期阶段采用“模块化单体”更利于快速迭代与一致性；当吞吐/隔离需求上来后，再将以下模块按边界拆分为独立服务（接口保持不变）：

- Market Data Service（行情服务）
- Execution Gateway（交易执行网关）
- OMS / Portfolio（订单与组合管理）
- Strategy Runtime（策略运行时）
- Backtest & Replay（回测与回放）
- Analytics (PnL/Risk)（统计分析）
- AI Agent Bridge（AI 接入桥）
- UI & API（界面与对外 API）

核心组件之间通过两类通道通信：

- 同进程内：高性能内存队列/环形缓冲（策略运行与行情/交易事件）
- 跨进程：gRPC（控制面） + 消息总线（数据面）

### 2.2 数据面与控制面

- 数据面（高频事件）：Tick、OrderBook、Kline、Trade、Funding、MarkPrice、OrderUpdate、Fill、BalanceUpdate
  - 目标：低延迟、稳定、可背压、可丢弃低优先级事件（如 UI 订阅）
- 控制面（低频指令）：启动/停止策略、参数调整、风控开关、订阅管理、权限管理
  - 目标：强一致、可审计、可重试、接口稳定

### 2.3 Monorepo 架构设计

VeloZ 采用 Monorepo 管理多语言工程（C++23 核心引擎 + Python 策略生态 + Web UI），以“接口契约统一、构建可复现、跨模块依赖可控”为目标。

#### 2.3.1 目录规划（建议）

```text
VeloZ/
  apps/
    engine/                  # C++23：核心引擎进程（行情 + 执行 + OMS + 风控）
    gateway/                 # C++23：对外 API 网关/BFF（可选，或与 engine 合并）
    backtest/                # C++23 或 Python：回测/回放任务执行器（可拆分）
    ui/                      # Web：交易界面（React）
  services/
    ai-bridge/               # AI Agent Bridge（可独立扩容）
    analytics/               # 统计与报表服务（可独立扩容）
  libs/
    core/                    # C++23：基础设施（时间、日志、配置、对象池、ringbuffer）
    market/                  # C++23：行情规范化、订阅聚合、OrderBook
    exec/                    # C++23：交易所适配、下单/撤单、回执处理
    oms/                     # C++23：订单状态机、仓位/资金、对账
    risk/                    # C++23：风控规则引擎
    common/                  # 跨模块通用代码（不含业务耦合）
  proto/                     # gRPC/Protobuf：控制面与跨服务契约（单一事实源）
  python/
    strategy-sdk/            # Python：策略 SDK（订阅、回调、下单抽象）
    research/                # Python：研究与实验（特征、训练、回测脚本）
  web/
    ui/                      # React 前端源码（如选择 apps/ui 也可只保留一处）
  infra/
    docker/                  # Dockerfile、compose、镜像发布
    k8s/                     # Kubernetes manifests（可选）
  tools/
    codegen/                 # Protobuf 代码生成与校验
    lint/                    # 统一 lint/format 配置入口
  docs/
    crypto_quant_framework_design.md
```

目录可以按团队偏好调整，但建议保持三个不变的“硬边界”：

- 可部署单元在 apps/ 或 services/
- 可复用库在 libs/
- 接口契约在 proto/ 且为唯一事实源

#### 2.3.2 依赖规则（建议强约束）

- apps/ 与 services/ 可以依赖 libs/ 与 proto/；libs/ 之间只能按单向依赖（例如 market → core，exec → core，oms → core），避免环依赖
- proto/ 不依赖任何业务实现代码；生成代码进入各语言对应模块（C++/Python/TS）
- Python 策略侧只依赖“稳定 SDK”，不直接依赖 engine 内部库，避免实盘升级破坏策略
- UI 只依赖外部 API/WS 协议，不直接依赖内部数据库表结构

#### 2.3.3 构建与工具链（多语言统一视角）

- C++23：
  - CMake（主构建），建议使用 CMake Presets 固化编译器/编译参数/构建类型
  - vcpkg 或 Conan（依赖管理），建议锁定版本以保证可复现构建
- Python：
  - 以独立包形式维护 strategy-sdk，明确版本号，便于策略与引擎解耦升级
  - 研究代码与生产策略分离（research/ 与可部署策略包）
- Web：
  - 采用 TypeScript；构建工具可选 Vite/Next.js（取决于是否需要 SSR）
- 代码生成：
  - proto/ 变更触发 C++/Python/TS 的代码生成，并在 CI 做一致性校验（避免手工生成漂移）

#### 2.3.4 版本策略与发布（建议）

- 引擎与服务：独立语义化版本（engine vX.Y.Z、ai-bridge vA.B.C），避免单一大版本绑死发布节奏
- 契约版本：proto/ 的重大破坏性变更通过版本化（package/namespace）或新增字段实现向后兼容
- 策略 SDK：独立版本与变更日志；为策略运行环境提供“锁定 SDK 版本”的能力

#### 2.3.5 CI 设计（建议）

- 变更影响分析：
  - C++ 变更仅触发相关 libs/ 与 apps/ 的构建与测试
  - proto/ 变更触发全链路 codegen 校验 + 依赖模块编译
  - web/ 与 python/ 各自独立 lint/test
- 质量门禁：
  - 格式化（clang-format / black / prettier）
  - 静态分析（clang-tidy / ruff / eslint）
  - 单测 + 关键集成测试（下单幂等、订单状态机、对账一致性）

## 3. 模块设计

## 3.1 高性能行情获取模块（Market Data）

### 3.1.1 功能范围

- 多交易所接入（首期：币安；扩展：OKX、Bybit、Gate、Coinbase 等）
- 统一行情类型：
  - 现货：Trades、OrderBook（深度）、BestBidAsk、Kline、Ticker、AggTrade
  - U 本位合约：同上 + FundingRate、MarkPrice、IndexPrice、OpenInterest
  - 币本位合约：同上（注意合约面值、结算币种）
- 订阅管理：按策略/按 UI 的订阅聚合与去重
- 数据规范化：交易所 symbol、价格精度、数量精度、合约乘数等统一
- 数据质量：序列号校验、快照/增量对齐、断线重连、延迟监控、丢包统计

### 3.1.2 关键设计点

#### (1) 交易所适配层（Exchange Adapter）

- REST：用于初始快照、交易规则、历史 K 线、账户/仓位拉取
- WebSocket：用于实时行情、用户数据流（如果与交易模块复用）
- 限速与配额：按 API key / IP / endpoint 维度做令牌桶，内置排队与降级策略

#### (2) OrderBook 重建

- 快照 + 增量 diff：严格按交易所序列号合并，检测 gap 触发重新拉快照
- 内存表示：按价格档位的有序结构（性能优先可用数组/跳表/平衡树的封装），对外提供：
  - TopN（1/5/20/100 档）
  - 全量深度（用于做市/微观结构策略）
  - 价差、加权中间价、冲击成本等衍生指标

#### (3) 延迟与同步

- 本地时间戳：接收时刻、解析完成时刻、分发时刻
- 交换所时间戳：事件内置时间 + 服务器时间校准（定期 ping 校时）
- 指标：端到端 P99/P999 延迟、重连次数、消息处理 QPS、丢弃率

### 3.1.3 行情事件规范（建议）

统一采用内部事件模型，避免策略直接依赖交易所字段：

- MarketEvent
  - type: Trade | Book | Kline | Ticker | MarkPrice | FundingRate | ...
  - venue: Binance | OKX | ...
  - market: Spot | LinearPerp(USDT) | InversePerp(Coin) | ...
  - symbol: 统一后的 SymbolId
  - ts_exchange / ts_recv / ts_pub
  - payload: 对应类型的数据结构

## 3.2 高性能交易模块（Execution / OMS）

### 3.2.1 功能范围

- 下单/撤单/改单（按交易所能力映射）
- 账户、仓位、余额同步（REST 定期校准 + WS 增量）
- 订单状态机：
  - New → Accepted → PartiallyFilled → Filled / Canceled / Rejected / Expired
  - 支持交易所异步回执与乱序修正
- 交易路由：
  - 现货与合约分路由
  - 多交易所：最佳执行/分拆执行（可后续迭代）
- 风控前置：
  - 账户可用资金、最大杠杆、最大持仓、最大下单频率、最大订单量
  - 价格保护（偏离度/滑点上限/市价保护）
  - Kill Switch：一键停止、强平保护模式

### 3.2.2 低延迟关键路径

建议将“行情→策略→下单→回执”作为核心路径进行优化：

- 单线程事件循环 + 无锁队列（减少锁竞争）
- 预分配对象池（减少 GC/频繁分配）
- 批量发送与合并撤单（交易所支持时）
- 网络层：
  - 长连接复用
  - TLS 会话复用
  - 就近部署（低延迟地域）

### 3.2.3 幂等与一致性

- clientOrderId：内部生成且唯一，用于幂等与追踪
- Order Journal：写入顺序日志（本地 WAL 或数据库），确保故障后可恢复订单状态
- 账户对账：
  - 定时全量拉取（REST）与实时增量（WS）对齐
  - 发现不一致触发“冻结策略 + 自动修复/人工介入”

### 3.2.4 交易接口（建议）

对策略层暴露统一接口：

- place_order(symbol, side, type, qty, price?, tif?, reduce_only?, post_only?, client_order_id?)
- cancel_order(order_id or client_order_id)
- cancel_all(symbol?)
- amend_order(order_id, qty?, price?)
- get_position(symbol)
- get_account()

策略层只依赖这些接口，不直接依赖交易所 SDK。

## 3.3 策略模块（Strategy Runtime）

### 3.3.1 功能范围

- 支持策略类型：
  - 经典量化：趋势/均值回归/动量/套利
  - 高频：盘口驱动、订单流、做市、微观结构
  - 网格：区间网格、动态网格、资金管理
- 多策略并行与组合：
  - 多策略共享行情、隔离风险预算
  - 组合级别风控（总杠杆、总回撤、相关性控制）
- 生命周期：
  - init → on_start → on_event → on_timer → on_stop → on_error
- 参数管理：
  - 参数热更新（控制面）
  - 参数版本化（用于复现实验）

### 3.3.2 统一运行模型：回测/仿真/实盘一致

引入“运行环境（Environment）”抽象：

- LiveEnv：实时行情 + 实盘执行
- PaperEnv：实时行情 + 仿真撮合（用实盘规则）
- BacktestEnv：历史回放 + 仿真撮合

策略只关心：

- 订阅行情
- 接收事件回调
- 调用交易接口
- 读取当前组合状态（仓位/资金/指标）

### 3.3.3 高频策略的工程支持

- 时间驱动与事件驱动并存：支持微秒级定时器（取决于语言与运行环境）
- 共享内存/零拷贝：行情事件从采集到策略尽量避免拷贝
- 风控快速路径：风控检查在本地内存完成，必要时异步校准

## 3.4 统计模块（PnL / 风险 / 回测）

### 3.4.1 核心指标体系

- 收益与风险：
  - 总收益、年化收益、波动率、Sharpe、Sortino、最大回撤、Calmar
  - 日/周/月收益分布、偏度/峰度、尾部风险（VaR/CVaR 可选）
- 交易质量：
  - 成交率、撤单率、平均持仓时间、滑点、手续费、成交冲击
  - 盈亏比、胜率、期望收益、连续亏损长度
- 组合维度：
  - 多标的分解、资金占用、杠杆利用率、风险预算占用

### 3.4.2 回测引擎设计

- 数据源：
  - K 线级回测（适合中低频）
  - Tick/OrderBook 回测（适合高频；成本更高）
- 撮合模型：
  - 市价：按下一笔成交或盘口冲击模型
  - 限价：基于盘口穿透/队列位置近似（可配置）
- 成本模型：
  - 手续费（maker/taker）
  - 资金费率（合约）
  - 滑点模型（线性/分段/冲击成本）
- 事件回放：
  - 统一复用策略运行模型（BacktestEnv）
  - 支持“加速回放 / 实时回放 / 指定区间回放”

### 3.4.3 数据落库与查询（建议）

- 原始高频数据：ClickHouse（列式、高吞吐写入、快查询）
- 结构化业务数据：PostgreSQL（订单、成交、账户快照、策略版本、审计）
- 缓存：Redis（订阅、热点行情、会话）
- 冷存储：对象存储（压缩归档，按日/按市场分区）

## 3.5 AI 模块（AI Agent Bridge）

### 3.5.1 目标能力

- 行情分析：
  - 多周期趋势/波动、关键支撑阻力、事件冲击解释
  - 盘口异常：大单、刷量、价差异常、深度骤降
- 操作复盘：
  - 按策略/按交易序列生成复盘报告（为什么下单、是否符合规则、哪些因素导致亏损）
  - 识别策略偏离（例如参数漂移、执行滑点恶化）
- 风险提示：
  - 连续亏损、相关性上升、杠杆过高、流动性不足
- 策略辅助：
  - 生成研究假设、提出可验证的特征与实验计划
  - 不直接自动执行为默认（建议 → 审核 → 执行）

### 3.5.2 接入方式与安全边界

- AI Agent 通过“桥接服务”访问数据，桥接服务负责：
  - 权限控制（按用户/角色、按策略、按市场）
  - 数据脱敏（隐藏 API Key、账户敏感字段）
  - 额度与成本控制（token、QPS、并发）
  - 审计记录（每次 AI 请求与响应的 trace）
- AI 读取的数据类型：
  - 近实时行情摘要（非全量深度可按需）
  - 订单/成交/仓位时间线
  - 指标与回测报告
  - 系统健康状态（延迟、断线、错误码）

### 3.5.3 典型工作流

- 交易中：AI 定期输出“市场状态 + 风险提示 + 策略偏离”摘要
- 交易后：自动生成复盘报告（可导出 PDF/HTML，后续迭代）
- 异常时：触发调查任务（自动聚合相关日志、行情片段、订单事件）

## 3.6 交易界面（UI）

### 3.6.1 核心页面

- 行情与深度：
  - K 线、盘口、成交明细、资金费率/标记价格（合约）
- 交易与仓位：
  - 下单面板（限价/市价/条件单映射）
  - 当前仓位、委托、成交、资金占用、风险度
- 策略中心：
  - 策略列表、参数配置、启动/停止、版本管理
  - 策略实时指标与告警
- 回测与报告：
  - 回测任务管理、结果对比、指标与曲线
- AI 助手：
  - 行情分析面板、复盘面板、可追溯对话记录
- 系统监控：
  - 延迟、断线、错误码、队列积压、资源使用

### 3.6.2 权限与审计

- RBAC：管理员/交易员/研究员/只读
- 审计：关键操作（启动策略、改参数、手动下单、开关风控）全记录

## 4. VeloZ 技术选型（可替换）

本节给出 VeloZ 各模块“推荐选型 + 可替换方案 + 选择理由（偏性能/可靠性/生态）”。如果只做 MVP，可优先落地每节的“推荐选型”。

### 4.1 基础设施与运行环境

- 操作系统：Linux（Ubuntu / Debian），生产建议使用 LTS 版本
- 时钟与校时：chrony + NTP；可选 PTP（同机房更高精度）
- 容器化：Docker；编排建议 Kubernetes（或先 docker-compose）
- 入口网关：Nginx / Envoy（TLS 终止、反向代理、限流、WAF 可挂载）
- 配置管理：环境变量 + 配置文件（YAML/TOML）+ 远端配置中心（可选 Consul）

### 4.2 核心语言与工程形态

- 核心低延迟（行情/执行/OMS/事件总线）：C++23（首选）
  - 选择理由：可控延迟、极致性能、成熟的网络与系统编程生态，适合 HFT 关键路径（对象池、零拷贝、锁自由结构、CPU 亲和与 NUMA 优化）
  - 异步模型：基于 Boost.Asio（或 Asio standalone）的事件循环；可选 C++20/23 协程 co_await（视编译器与库成熟度）
  - 工程化：
    - 构建系统：CMake
    - 依赖管理：vcpkg 或 Conan
    - ABI 策略：核心服务统一编译链与标准库版本，避免混用 ABI
- 策略研究与快速迭代：Python
  - 与核心通信：gRPC（控制面）+ 共享内存/IPC（可选，追求极致延迟）
- UI：Web（React）+ BFF（后端为前端服务，可选）

### 4.3 交易所接入（REST/WS）与网络库

按语言选择成熟网络栈，保证连接稳定性与可观测性：

- C++23：
  - 网络与事件循环：Boost.Asio（或 Asio standalone）
  - HTTP：
    - Boost.Beast（与 Asio 深度集成，可控连接池与超时）
    - 可选：libcurl（成熟稳定，适合作为非关键路径/工具侧）
  - WebSocket：
    - Boost.Beast WebSocket（统一栈，便于限速/重连/指标埋点）
    - 可选：uWebSockets（更偏极致吞吐的场景）
  - TLS：OpenSSL（或 BoringSSL，视发行版与合规要求）
  - JSON：
    - simdjson（高吞吐解析，适合行情热路径）
    - nlohmann/json（易用，适合非关键路径与配置）

关键策略：

- 连接复用：长连接、HTTP keep-alive、TLS 会话复用
- 自动重连：指数退避 + 抖动；重连后触发快照重建与一致性校验
- 限速：令牌桶（按 endpoint/key 维度）+ 队列 + 降级（丢弃低优先级订阅）

### 4.4 通信、序列化与事件总线

- 控制面（低频强类型）：gRPC + Protobuf
  - 优点：接口可演进、跨语言、便于权限控制与审计
- 数据面（高频事件流）：优先 NATS JetStream；规模更大可选 Kafka
  - NATS：延迟低、运维轻、适合行情/交易事件分发
  - Kafka：吞吐与持久化强，适合海量历史数据管道与离线计算
- 进程内队列：环形缓冲/无锁队列
  - C++：boost::lockfree、folly ProducerConsumerQueue、或自研 SPSC/MPSC RingBuffer（对象池+cacheline 对齐）
- 序列化格式：
  - 内部事件：Protobuf（跨进程）+ 二进制结构体（进程内）
  - 历史存储：Parquet（列式压缩，适合回测/分析）或原始 JSON 压缩归档（可选）

### 4.5 Market Data（行情）技术选型

- 订单簿（OrderBook）：
  - 内存结构：价格档位 map（BTree/跳表/平衡树封装）+ TopN 快速视图
  - 快照/增量对齐：按交易所序列号（u/U）校验，gap 触发重拉快照
- K 线与聚合：
  - 实时聚合：内存滚动窗口 + 增量更新（避免重复扫描）
  - 指标计算：低频可用 pandas/numpy；高频建议在核心侧实现常用指标并缓存
- 行情缓存：
  - Redis：存放 UI 订阅的热点行情、TopN、最近成交
  - 内存：策略侧订阅的关键数据（最小化序列化/拷贝）

### 4.6 Execution / OMS（交易执行与订单系统）技术选型

- 订单状态机与幂等：
  - clientOrderId：统一生成（雪花/ULID/时间+随机）用于追踪与幂等
  - 本地顺序日志（WAL）：建议 RocksDB（嵌入式）或直接落 PostgreSQL（简化运维）
- 用户数据流（账户/订单回执）：
  - WebSocket 用户流为主，REST 定期全量校准
  - 异常恢复：以“对账”为权威入口（冻结策略→校准→恢复）
- 风控引擎（建议独立模块，可嵌入执行网关）：
  - 本地内存检查（频率/仓位/杠杆/偏离度）+ 异步校准
  - Kill Switch：在执行网关处实现强制仅撤单模式
  - 规则表达：配置驱动（YAML/TOML）+ 热更新（控制面），避免策略绕过风控

### 4.7 Strategy Runtime（策略运行时）技术选型

- 策略 SDK（Python）：
  - 数据结构：pydantic（配置与校验）或 dataclasses（更轻）
  - 研究计算：numpy/pandas；高频路径尽量减少 Python 参与（仅决策）
- 并行模型：
  - 多策略：进程隔离（稳定优先）或线程隔离（低开销）
  - 统一事件回调：on_event + on_timer；控制面负责启停与参数热更新
- 配置与版本：
  - 策略包管理：Python venv/poetry（研究侧）；生产可冻结依赖镜像

### 4.8 回测 / 回放 / 统计（Analytics）技术选型

- 回测引擎：
  - 事件回放：按时间线驱动（MarketEvent/TradingEvent 复用）
  - 撮合：可插拔模型（K 线撮合、Tick 撮合、盘口穿透、队列位置近似）
- 计算引擎：
  - 在线统计：核心服务实时更新（PnL、仓位、风险）
  - 离线分析：ClickHouse + SQL 或 Spark（更大规模）
- 报告输出：
  - MVP：HTML（前端渲染）或 JSON 指标 + 曲线数据
  - 后续：服务端渲染 PDF（可选）

### 4.9 数据存储选型

- 结构化业务数据（订单/成交/账户/审计）：PostgreSQL
- 高频行情与分析查询：ClickHouse
- 缓存与轻量队列：Redis
- 嵌入式状态与本地日志：RocksDB（可选）
- 对象存储（冷数据归档）：S3 兼容（MinIO / AWS S3）
  - C++ 侧客户端：libpqxx（PostgreSQL）、ClickHouse C++ Client、redis-plus-plus（或 hiredis）

### 4.10 AI Agent Bridge 技术选型

- LLM/Agent 接入：
  - 接口协议：HTTP/gRPC（统一网关）+ 流式响应（SSE/WebSocket 可选）
  - 工具调用：以“只读数据工具 + 可审计的建议工具”为默认
- 向量检索（用于复盘/知识库）：
  - pgvector（集成 PostgreSQL，运维简单）或 Milvus（规模更大）
- 权限与审计：
  - 每次请求记录：prompt 摘要、数据范围、响应摘要、traceId
  - 脱敏：账户标识、API Key、用户隐私字段强制脱敏

### 4.11 API 与身份认证

- 对外 API：
  - 内部：gRPC（服务间）
  - 外部：REST（OpenAPI）或 GraphQL（UI 聚合强时）
- 鉴权：
  - JWT（短期）+ Refresh Token（长期）
  - RBAC：角色与权限矩阵（策略启停/手动交易/风控开关）
- 速率限制：
  - Nginx/Envoy 层限流 + 服务内细粒度限流

### 4.12 UI 技术选型

- 前端框架：React + TypeScript
- 图表与可视化：
  - K 线：TradingView Lightweight Charts（或 ECharts）
  - 监控面板：ECharts / Recharts
- 状态管理：Redux Toolkit / Zustand
- 实时订阅：WebSocket（或 SSE）+ 增量更新策略（避免全量刷新）
- 桌面端（可选）：Electron（生态强）或 Qt 6（原生体验与高性能）

### 4.13 可观测性与运维

- 统一链路：OpenTelemetry（Trace/Metrics/Logs）
- 指标：Prometheus + Grafana
- 日志：Loki（轻量）或 Elasticsearch（检索能力更强）
- 告警：Alertmanager（与 Grafana/Prometheus 生态融合）
- C++ 侧埋点库：
  - 日志：spdlog
  - 指标：prometheus-cpp（或 StatsD/OTel Metrics）
  - Trace：opentelemetry-cpp
- 健康检查：
  - Liveness/Readiness
  - 关键依赖：交易所连接、延迟、队列积压、对账状态

### 4.14 CI/CD 与交付

- 构建：GitHub Actions / GitLab CI
- 制品：Docker 镜像 + 版本号（语义化）
- 发布：
  - 灰度/滚动升级（Kubernetes）
  - 回滚：保留上一个稳定镜像与配置版本

### 4.15 安全与密钥管理

- 密钥存储：云 KMS（首选）或 HashiCorp Vault；本地可用系统密钥环
- 运行隔离：
  - 研究环境与实盘环境隔离（网络、权限、数据）
  - 实盘 API Key 单独账户、最小权限、IP 白名单
- 审计：关键操作 append-only 存储（PostgreSQL + 只追加表 / 对象存储归档）

## 5. 数据模型与接口契约（概要）

### 5.1 核心实体

- Symbol：统一标的（含市场类型、合约乘数、精度、最小下单量）
- Order：内部订单（clientOrderId、venueOrderId、状态机、时间戳）
- Fill：成交记录（价格、数量、手续费、流动性角色）
- Position：仓位（方向、数量、均价、未实现盈亏、保证金）
- Account：余额与保证金信息
- StrategyRun：一次运行实例（参数版本、启动者、环境、回测区间）

### 5.2 事件时间线（可回放）

- MarketEvent：行情事件
- TradingEvent：订单回执/成交/余额更新
- SystemEvent：断线、重连、限速、风控触发、异常堆栈

系统必须保证：

- 事件可持久化（至少交易与关键行情摘要）
- 事件可重放（用于回测复现与事故排查）

## 6. 性能目标（建议指标）

按单机部署、内网环境的可衡量目标（实际取决于语言与机器）：

- 行情处理：单标的深度 + 成交流 QPS 可线性扩展；P99 处理延迟 < 5ms（采集到分发）
- 下单路径：策略触发到请求发出 P99 < 2ms（不含网络）
- 回执处理：成交/回执到策略可见 P99 < 5ms
- 稳定性：断线自动重连，重连后数据自愈；连续运行 7x24 无内存泄漏

## 7. 风控与安全

- API Key 管理：
  - 最小权限（仅交易/仅读分离）
  - IP 白名单
  - 密钥加密存储（KMS/系统密钥环/硬件加密可选）
- 交易风控：
  - 频率限制、仓位限制、价格偏离限制、最大回撤触发暂停
  - 风险事件触发告警与自动降级（停止策略、只允许撤单）
- 审计与取证：
  - 关键操作不可篡改日志（可用 append-only 存储）

## 8. 部署形态

- 本地研究：单机一体化（行情 + 回测 + UI）
- 云端实盘：分层部署
  - 靠近交易所的执行节点（降低网络延迟）
  - 统计与 AI 节点可独立扩容
- 灾备：
  - 热备执行节点（共享订单与状态恢复能力）
  - 关键数据异地备份

## 9. 迭代路线（建议）

### Phase 1：可用的最小闭环（MVP）

- 币安现货 + U 本位合约：行情（WS+REST）+ 交易（下单/撤单）+ 策略运行（事件驱动）
- 基础统计：PnL、成交明细、手续费、简单回测（K 线）
- 简版 UI：行情、仓位、订单、策略启停

### Phase 2：高性能与回放

- OrderBook 重建与 Tick 回放
- 仿真撮合与滑点/费用模型
- 端到端延迟监控与自动降级

### Phase 3：AI 增强与多交易所

- AI 复盘、异常检测、研究助手
- 增加 OKX/Bybit 等交易所适配
- 组合风控与多策略资金分配
