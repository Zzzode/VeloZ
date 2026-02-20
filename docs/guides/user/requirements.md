# VeloZ Quantitative Trading Framework Product Requirements Document (PRD)

> **Implementation Status**: See [IMPLEMENTATION_STATUS.md](../../project/reviews/IMPLEMENTATION_STATUS.md) for detailed progress tracking.
> **Last Updated**: 2026-02-15 (KJ Migration Complete)

## 1. Product Overview

VeloZ is an industrial-grade quantitative trading framework for cryptocurrency markets, providing a unified backtest/simulation/live trading runtime model and an evolvable engineering architecture. The framework is built with C++23 for the core engine to achieve low-latency trading execution, and supports multi-language strategy development (Python first).

### 1.1 Product Positioning

- **Target Users**: Quantitative traders, algorithm engineers, fintech developers, institutional investors
- **Core Value**:
  - Lower the barrier to quantitative strategy development
  - Provide high-performance trading execution engine
  - Unified strategy interface supports seamless backtest/simulation/live trading switching
  - Complete risk management and analysis system
- **Competitive Advantages**:
  - C++23 core engine provides low-latency trading capability (microsecond level)
  - Supports HFT/market-making/grid and other high-frequency strategies
  - Complete event-driven architecture
  - Powerful backtesting and analysis system
  - AI Agent integration support

### 1.2 Product Vision

To become the leading quantitative trading framework in the cryptocurrency market, providing stable, efficient, and scalable trading solutions for professional traders, supporting the complete lifecycle management from strategy research to live trading.

### 1.3 Core Principles

- **Event-Driven**: Market, orders, fills, accounts, and risks flow through events
- **Layered Decoupling**: Exchange adapters, core services, strategy layer, and presentation layer are loosely coupled
- **Consistency First**: Order state machine, position/balance reconciliation, idempotency, and replayability
- **Observability First**: End-to-end latency, loss/reconnect, execution receipts, error codes, and retry policies are traceable
- **Security First**: Minimum privilege keys, isolation, signing, and audit logging

## 2. Core Functional Requirements

### 2.1 Market Data System

#### 2.1.1 Market Data Ingestion

- **Requirement Description**: Support multiple market data source ingestion
- **Feature Details**:
  - Exchange REST API data acquisition (Binance as baseline)
  - WebSocket real-time data push
  - Historical data download and storage
  - Data standardization processing
  - Connection reuse, automatic reconnection, and rate limiting

#### 2.1.2 Data Type Support

- **Requirement Description**: Support multiple market data types
- **Feature Details**:
  - Tick data (Ticker)
  - Order book data (OrderBook)
  - K-line data (Kline)
  - Funding rate (Funding Rate)
  - Mark price (Mark Price)

#### 2.1.3 Market Data Processing

- **Requirement Description**: Provide efficient market data processing capabilities
- **Feature Details**:
  - Order book snapshot/increment alignment
  - K-line real-time aggregation
  - Indicator calculation (moving averages, RSI, MACD, etc.)
  - Market cache (Redis + memory)

### 2.2 Trading Execution System

#### 2.2.1 Order Management

- **Requirement Description**: Provide complete order lifecycle management
- **Feature Details**:
  - Order creation and submission
  - Order status query and update
  - Order cancellation
  - Order execution reports
  - Client order ID generation and idempotency guarantee

#### 2.2.2 Execution Channels

- **Requirement Description**: Support multiple trading execution channels
- **Feature Details**:
  - Simulated trading engine (Simulated)
  - Real exchange connection (Binance first)
  - Order routing and smart routing
  - Live/simulation/backtest environment isolation

#### 2.2.3 Account and Position Management

- **Requirement Description**: Provide account and position management capabilities
- **Feature Details**:
  - Real-time account balance and position query
  - Fund reconciliation and reconciliation
  - Leverage and margin management

### 2.3 Strategy Development Framework

#### 2.3.1 Strategy Interface Design

- **Requirement Description**: Provide simple and easy-to-use strategy interface, supporting multiple strategy types
- **Feature Details**:
  - Strategy lifecycle management (initialization, start, stop, error handling)
  - Market event callback mechanism
  - Timer function support
  - Position and order management
  - Trading signal generation interface
- **Technical Implementation**: C++23 interface + Python SDK

#### 2.3.2 Strategy Type Support

- **Requirement Description**: Support common quantitative strategy types
- **Feature Details**:
  - **Basic Strategy Types**:
    - Trend Following Strategy
    - Mean Reversion Strategy
    - Momentum Strategy
    - Arbitrage Strategy
    - Market Making Strategy
    - Grid Trading Strategy
    - Custom Strategy
  - **Advanced Strategy Types**:
    - Technical Indicator Strategies (RSI, MACD, Bollinger Bands, Stochastic Oscillator)
    - Cross-Exchange Arbitrage Strategies
    - High-Frequency Trading Strategies (HFT)
    - Machine Learning Strategies
    - Portfolio Management Strategies

#### 2.3.3 Strategy Parameter Management

- **Requirement Description**: Provide flexible parameter configuration and hot update support
- **Feature Details**:
  - Strategy parameter configuration file support (YAML/TOML)
  - Runtime parameter hot update
  - Parameter version management
  - Strategy backtest parameter optimization interface
  - Support grid search, random search, and other optimization algorithms

### 2.4 Risk Management System

#### 2.4.1 Risk Control

- **Requirement Description**: Provide multi-level risk control mechanisms
- **Feature Details**:
  - Single-trade risk limits
  - Strategy-level risk budget
  - Account-level risk control
  - Stop-loss/take-profit strategies
  - Forced liquidation and Kill Switch

#### 2.4.2 Risk Monitoring

- **Requirement Description**: Real-time monitoring of risk status
- **Feature Details**:
  - Risk indicator calculation and display
  - Risk warning and alerts
  - Risk report generation
  - Risk rule configuration and hot update

### 2.5 Analysis and Backtesting System

#### 2.5.1 Backtesting Engine

- **Requirement Description**: Provide high-performance backtesting engine
- **Feature Details**:
  - Historical data replay
  - Strategy backtesting execution
  - Backtesting result analysis
  - Event-driven backtesting architecture
  - Support multiple matching models (K-line, tick, order book penetration)

#### 2.5.2 Performance Analysis

- **Requirement Description**: Provide comprehensive performance analysis capabilities
- **Feature Details**:
  - Return curve and maximum drawdown
  - Win rate and profit factor
  - Sharpe ratio and Sortino ratio
  - Trading frequency and holding period
  - Risk-adjusted return

#### 2.5.3 Backtesting Reports

- **Requirement Description**: Generate detailed backtesting reports
- **Feature Details**:
  - Visualize backtesting results
  - Trade record analysis
  - Risk indicator charts
  - Report export (HTML, JSON, PDF)

### 2.6 AI Agent Integration

#### 2.6.1 Market Interpretation

- **Requirement Description**: Provide market data interpretation capabilities
- **Feature Details**:
  - Price trend analysis
  - Volatility cause explanation
  - Market sentiment recognition

#### 2.6.2 Strategy Suggestions

- **Requirement Description**: Provide strategy suggestion capabilities
- **Feature Details**:
  - Strategy parameter optimization suggestions
  - New strategy development suggestions
  - Strategy portfolio optimization

#### 2.6.3 Trading Review

- **Requirement Description**: Provide trading review capabilities
- **Feature Details**:
  - Abnormal trade detection
  - Trading decision explanation
  - Risk assessment

### 2.7 User Interface

#### 2.7.1 Strategy Management

- **Requirement Description**: Provide strategy management interface
- **Feature Details**:
  - Strategy creation and configuration
  - Strategy start and stop
  - Strategy parameter adjustment
  - Strategy version management

#### 2.7.2 Monitoring and Analysis

- **Requirement Description**: Provide real-time monitoring and analysis interface
- **Feature Details**:
  - Real-time market data display
  - Strategy running status monitoring
  - Trading execution reports
  - Risk status display
  - Performance indicator charts

#### 2.7.3 Manual Intervention

- **Requirement Description**: Provide manual intervention capabilities
- **Feature Details**:
  - Manual order placement and cancellation
  - Strategy parameter adjustment
  - Risk control switches

## 3. Product Iteration Plan

### 3.1 Phase 1: Core Engine Refinement (0-3 months)

**Status**: In Progress (~90% complete)

**Core Goal**: Establish industrial-grade core engine and basic framework

**Priority Features**:

1. **Market Data System Optimization** [Status: ~60%]
   - [x] Refine Binance REST API ingestion
   - [x] Implement WebSocket ingestion
   - [ ] Implement order book snapshot/increment alignment
   - [x] Optimize K-line aggregation and indicator calculation
   - [ ] Implement data standardization processing

2. **Trading Execution System Refinement** [Status: ~85%]
   - [x] Refine order state machine and idempotency guarantee
   - [x] Implement simulated trading engine
   - [x] Optimize account and position management
   - [ ] Implement fund reconciliation and reconciliation

3. **Strategy Development Framework Refinement** [Status: ~50%]
   - [x] Implement strategy factory and strategy creation capabilities
   - [ ] Refine strategy lifecycle management (partial)
   - [ ] Provide basic strategy implementation examples (partial)
   - [ ] Develop Python strategy SDK

4. **Risk Management Foundation** [Status: ~60%]
   - [x] Implement basic risk control capabilities
   - [x] Provide risk indicator calculation
   - [ ] Implement stop-loss/take-profit strategies
   - [x] Implement forced liquidation and Kill Switch

5. **User Interface Optimization** [Status: ~80%]
   - [ ] Refine strategy management interface
   - [x] Optimize real-time monitoring capabilities
   - [x] Provide trading execution report display

### 3.2 Phase 2: Advanced Feature Development (3-6 months)

**Status**: In Progress (~45% complete)

**Core Goal**: Provide advanced strategy development and analysis capabilities

**Priority Features**:

1. **Backtesting System Implementation** [Status: ~80%]
   - [x] Develop historical data replay engine
   - [x] Implement strategy backtesting execution
   - [x] Provide backtesting result analysis
   - [ ] Implement multiple matching models

2. **Advanced Strategy Support** [Status: ~60%]
   - [x] Implement technical indicator strategies (RSI, MACD, Bollinger Bands, etc.)
   - [ ] Implement cross-exchange arbitrage strategies (partial)
   - [ ] Provide high-frequency trading strategy support (partial)
   - [ ] Implement portfolio management strategies (partial)

3. **Advanced Risk Management** [Status: 0%]
   - [ ] Implement strategy-level risk budget
   - [ ] Provide risk warning and alerts
   - [ ] Develop risk report generation
   - [ ] Implement risk rule configuration and hot update

4. **API and User Interface** [Status: ~50%]
   - [x] Provide complete REST API (OpenAPI specification)
   - [ ] Refine WebSocket interface
   - [ ] Optimize user interface experience
   - [x] Implement strategy parameter optimization capabilities

### 3.3 Phase 3: System Expansion and Optimization (6-12 months)

**Status**: Not Started (0% complete)

**Core Goal**: Achieve system scalability and production deployment

**Priority Features**:

1. **Distributed Architecture** [Status: 0%]
   - [ ] Implement microservices architecture
   - [ ] Provide horizontal scaling capability
   - [ ] Optimize system fault tolerance
   - [ ] Implement inter-service communication and consistency

2. **Production Deployment** [Status: 0%]
   - [ ] Provide Docker containerized deployment
   - [ ] Implement Kubernetes deployment
   - [ ] Optimize system monitoring and operations
   - [ ] Implement CI/CD pipeline

3. **Security and Compliance** [Status: 0%]
   - [ ] Implement API key management
   - [ ] Provide trading signature and verification
   - [ ] Optimize system security
   - [ ] Implement audit logging and traceability

4. **AI Agent Integration** [Status: 0%]
   - [ ] Implement market interpretation capabilities
   - [ ] Provide strategy suggestion capabilities
   - [ ] Implement trading review capabilities
   - [ ] Optimize AI Agent performance

### 3.4 Phase 4: Ecosystem Building and Optimization (12+ months)

**Status**: Not Started (0% complete)

**Core Goal**: Perfect ecosystem and optimize user experience

**Priority Features**:

1. **Multi-language Support** [Status: 0%]
   - [ ] Refine Python strategy SDK
   - [ ] Provide other language support (Java, Go, etc.)
   - [ ] Optimize inter-language communication performance

2. **Strategy Marketplace** [Status: 0%]
   - [ ] Build strategy marketplace
   - [ ] Provide strategy sharing and trading capabilities
   - [ ] Implement strategy rating and evaluation

3. **Community Building** [Status: 0%]
   - [ ] Refine documentation and examples
   - [ ] Establish developer community
   - [ ] Provide technical support and training

## 4. User Experience Design

### 4.1 Strategy Development Experience

- **Simple API Design**: Provide intuitive and easy-to-use strategy interface
- **Detailed Documentation and Examples**: Provide complete API documentation and strategy examples
- **Rapid Prototyping**: Support rapid strategy prototype development and testing
- **Debugging and Monitoring**: Provide strategy debugging and monitoring capabilities

### 4.2 Backtesting Experience

- **High-Performance Backtesting**: Provide fast strategy backtesting execution
- **Visualized Backtesting Results**: Provide intuitive backtesting result visualization
- **Parameter Optimization**: Support strategy parameter optimization and tuning
- **Backtesting Reports**: Generate detailed backtesting reports

### 4.3 Live Trading Experience

- **Real-time Monitoring**: Provide real-time strategy running status monitoring
- **Risk Control**: Provide complete risk control and management
- **Trading Execution Reports**: Generate detailed trading execution reports
- **Alerts and Notifications**: Provide trading alerts and notification capabilities

### 4.4 Management Experience

- **Intuitive Interface Design**: Provide intuitive and easy-to-use management interface
- **One-Stop Management**: Support one-stop management of strategies, market, trading, and risk
- **Real-time Collaboration**: Support multi-user collaboration and permission management
- **Audit and Traceability**: Provide complete audit logging and traceability capabilities

## 5. Technical Architecture

### 5.1 System Architecture

- **C++23 Core Engine**: High-performance trading execution engine
- **Python Strategy SDK**: Strategy development and backtesting framework
- **Web UI**: User interaction interface (React)
- **REST API**: System interface (OpenAPI specification)
- **WebSocket**: Real-time data push
- **AI Agent Bridge**: AI agent integration

### 5.2 Data Architecture

- **Market Data Storage**: ClickHouse (high-frequency data) + PostgreSQL (structured data)
- **Strategy Parameter Storage**: PostgreSQL + Redis (cache)
- **Trading Record Storage**: PostgreSQL + ClickHouse
- **Risk Data Storage**: PostgreSQL + ClickHouse
- **Historical Data Storage**: S3-compatible object storage (MinIO / AWS S3)

### 5.3 Deployment Architecture

- **Local Deployment**: Standalone deployment and testing
- **Container Deployment**: Docker containerized deployment
- **Cloud Deployment**: Kubernetes cloud deployment
- **Edge Deployment**: Support edge node deployment and low-latency trading

### 5.4 Communication Architecture

- **Control Plane**: gRPC + Protobuf (strong consistency)
- **Data Plane**: NATS JetStream (high throughput)
- **Internal Communication**: Memory queues/ring buffers (low latency)
- **External Communication**: REST + WebSocket (user interaction)

## 5.5 KJ Library Migration

**Status**: **COMPLETE** (2026-02-15)

All migratable C++ standard library types have been successfully migrated to KJ library
equivalents across all modules. This ensures consistent patterns, better memory safety,
and alignment with project standards.

| std Type | KJ Equivalent | Status |
|----------|---------------|--------|
| `kj::Own<T>` | `kj::Own<T>` | ✅ Complete |
| `std::optional<T>` | `kj::Maybe<T>` | ✅ Complete |
| `std::vector<T>` | `kj::Vector<T>` / `kj::Array<T>` | ✅ Complete |
| `std::unordered_map<K,V>` | `kj::HashMap<K,V>` | ✅ Complete |
| `std::map<K,V>` | `kj::TreeMap<K,V>` | ✅ Complete |
| `std::function<R(Args...)>` | `kj::Function<R(Args...)>` | ✅ Complete |
| `std::mutex` | `kj::Mutex` / `kj::MutexGuarded<T>` | ✅ Complete |

**Remaining std usage** is only for:
- Features KJ does not provide (std::atomic, std::chrono for wall clock)
- External API compatibility (OpenSSL, yyjson, std::filesystem)
- Standard algorithms and math functions

See [KJ Migration Complete](../../project/migration/kj_migration_complete.md) for detailed documentation.

## 6. Market Analysis

### 6.1 Market Demand

- **Quantitative Trading Growth**: Continuous growth in quantitative trading demand in cryptocurrency markets
- **Strategy Development Barrier**: Demand for lowering strategy development barriers
- **Backtesting and Simulation**: Demand for backtesting and simulated trading
- **Risk Management**: Demand for risk management and control
- **Low-Latency Trading**: Demand for high-frequency trading and low-latency execution

### 6.2 Competitive Analysis

- **Existing Frameworks**:
  - Backtrader: Python backtesting framework
  - Zipline: Python algorithmic trading library
  - Freqtrade: Cryptocurrency quantitative trading platform
  - Hummingbot: Market-making and arbitrage strategy framework
- **Competitive Advantages**:
  - High-performance C++ core engine (microsecond-level latency)
  - Unified backtest/simulation/live trading interface
  - Complete risk management system
  - Support HFT/market-making/grid and other high-frequency strategies
  - AI Agent integration support

### 6.3 Market Strategy

- **Target Market**: Professional quantitative traders, algorithm engineers, institutional investors
- **Pricing Strategy**: Open source free + enterprise support
- **Promotion Strategy**: Community building, technical documentation, example strategies, online courses
- **Partners**: Exchanges, cloud service providers, fintech companies

## 7. Risk Analysis

### 7.1 Technical Risks

- **Performance Risks**: Core engine performance optimization
- **Stability Risks**: System stability and reliability
- **Security Risks**: Trading security and data security
- **Scalability Risks**: System scalability and fault tolerance

### 7.2 Market Risks

- **Market Competition**: Competitive pressure from existing frameworks
- **Market Changes**: Uncertainty in cryptocurrency markets
- **Regulatory Risks**: Changes in cryptocurrency regulatory policies

### 7.3 Operational Risks

- **Community Building**: Community activity and ecosystem building
- **Documentation Quality**: Technical documentation quality and completeness
- **User Support**: User support and issue resolution

## 8. Conclusion

The VeloZ Quantitative Trading Framework aims to provide a unified, high-performance, and scalable quantitative trading solution. Through phased product iteration plans, we will gradually perfect the strategy development framework, market data system, trading execution system, risk management system, and analysis backtesting system, providing a complete trading solution for professional quantitative traders.

The core advantages of the framework lie in its high-performance C++ core engine, unified backtest/simulation/live trading interface, complete risk management system, and AI Agent integration support, making it an ideal choice for quantitative trading in cryptocurrency markets.
