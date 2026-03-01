# VeloZ Glossary

A comprehensive glossary of terms used throughout VeloZ documentation and the trading system.

---

## Trading Terms

### A

**Ask Price**
The lowest price at which a seller is willing to sell an asset. Also known as the offer price. See also: [Bid Price](#bid-price).

**Average Entry Price**
The weighted average price at which a position was opened, calculated from all fills that contributed to the position.

### B

**Base Asset**
The first asset in a trading pair. In BTCUSDT, BTC is the base asset. The quantity in orders refers to this asset.

**Best Bid/Ask (BBO)**
The highest bid price and lowest ask price currently available in the order book. Represents the tightest spread.

**Bid Price**
The highest price a buyer is willing to pay for an asset. See also: [Ask Price](#ask-price).

### C

**Cancel-Replace**
A pattern for modifying orders where the existing order is cancelled and a new order is placed with updated parameters. VeloZ uses this pattern for order modifications.

**Client Order ID**
A unique identifier assigned by the client (VeloZ) to track orders. Used for idempotency, tracking, and reconciliation with exchange order IDs.

**Close Position**
To exit an existing position by placing an opposite order. A long position is closed by selling; a short position is closed by buying.

### E

**Entry Price**
The price at which a position was opened. For positions built from multiple fills, this is the average entry price.

**Execution**
The process of completing a trade. When an order is matched with a counterparty, it is executed (filled).

### F

**Fill**
A completed trade where an order (or part of an order) has been matched and executed. A single order may have multiple fills.

**FOK (Fill or Kill)**
A time-in-force instruction requiring the entire order to be filled immediately or cancelled entirely.

### G

**GTC (Good Till Cancelled)**
A time-in-force instruction where the order remains active until explicitly cancelled or filled.

**GTX (Good Till Crossing)**
A time-in-force instruction that ensures the order is placed as a maker order. If it would immediately match, it is rejected.

### I

**Iceberg Order**
An execution algorithm that displays only a portion of the total order quantity, hiding the full size from the market.

**IOC (Immediate or Cancel)**
A time-in-force instruction requiring immediate execution of any available quantity, with the remainder cancelled.

### L

**Leverage**
The ratio of position size to margin used. Higher leverage amplifies both gains and losses.

**Limit Order**
An order to buy or sell at a specific price or better. Buy limits execute at the limit price or lower; sell limits at the limit price or higher.

**Liquidity**
The ease with which an asset can be bought or sold without significantly affecting its price. High liquidity means tight spreads and deep order books.

**Long Position**
A position that profits when the asset price increases. Created by buying the asset.

**Lot Size**
The minimum quantity increment for orders on an exchange. Orders must be in multiples of the lot size.

### M

**Maker**
A trader who provides liquidity by placing limit orders that rest in the order book. Makers typically pay lower fees.

**Market Order**
An order to buy or sell immediately at the best available price. Prioritizes execution speed over price.

**Min Notional**
The minimum order value (price x quantity) required by an exchange. Orders below this threshold are rejected.

### N

**Notional Value**
The total value of a position or order, calculated as quantity multiplied by price.

### O

**Order Book**
A list of all buy and sell orders for an asset, organized by price level. Shows market depth and liquidity.

**Order Lifecycle**
The sequence of states an order passes through: New, Accepted, Partially Filled, Filled, Cancelled, Rejected, or Expired.

### P

**Partial Fill**
When only a portion of an order is executed, leaving the remainder active in the order book.

**Position**
The amount of an asset held, either long (positive) or short (negative). Includes entry price, current value, and P&L.

**Post-Only**
An order flag ensuring the order is placed as a maker order. If it would immediately match, it is rejected.

**POV (Percentage of Volume)**
An execution algorithm that executes as a percentage of real-time market volume, matching market participation rate.

**Price Filter**
Exchange rules that restrict order prices to valid ranges and tick sizes.

### Q

**Quote Asset**
The second asset in a trading pair. In BTCUSDT, USDT is the quote asset. Prices are denominated in this asset.

### R

**Realized P&L**
Profit or loss from closed positions. Calculated when a position is reduced or closed.

**Reduce-Only**
An order flag that ensures the order only reduces an existing position, never increases it.

### S

**Short Position**
A position that profits when the asset price decreases. Created by selling borrowed assets.

**Side**
The direction of an order: BUY or SELL.

**Slippage**
The difference between the expected price and the actual execution price. Common in large orders or volatile markets.

**Smart Order Routing (SOR)**
Automatic routing of orders to the best exchange based on price, fees, latency, and liquidity.

**Spread**
The difference between the best bid and best ask prices. Tighter spreads indicate higher liquidity.

**Stop-Loss**
An order that triggers when the price reaches a specified level, used to limit losses on a position.

### T

**Take-Profit**
An order that triggers when the price reaches a specified level, used to lock in profits on a position.

**Taker**
A trader who removes liquidity by placing orders that immediately match with existing orders. Takers typically pay higher fees.

**Tick Size**
The minimum price increment for an asset. Prices must be in multiples of the tick size.

**Time-in-Force (TIF)**
Instructions specifying how long an order remains active. Common values: GTC, IOC, FOK, GTX.

**Trading Pair**
Two assets that can be traded against each other, such as BTCUSDT (BTC traded against USDT).

**TWAP (Time-Weighted Average Price)**
An execution algorithm that splits orders evenly over time to minimize market impact.

### U

**Unrealized P&L**
Profit or loss on open positions based on current market prices. Not yet realized until the position is closed.

### V

**Venue**
An exchange or trading platform where orders are executed. VeloZ supports multiple venues.

**Venue Order ID**
The unique identifier assigned by the exchange to track an order. Different from the client order ID.

**VWAP (Volume-Weighted Average Price)**
An execution algorithm that executes in proportion to historical volume patterns to minimize market impact.

---

## Risk Management Terms

### A

**Alert Threshold**
A configurable level that triggers notifications when a risk metric reaches a specified value.

### C

**Circuit Breaker**
An automatic mechanism that halts trading when risk thresholds are breached. Has three states: Closed (normal), Open (blocked), Half-Open (testing recovery).

**Component VaR**
The contribution of a single position to the total portfolio VaR, accounting for correlations.

**Concentration Risk**
The risk from having too much exposure in a single asset or correlated group of assets.

**Conditional VaR (CVaR)**
Also called Expected Shortfall. The expected loss given that the loss exceeds VaR. Measures tail risk.

**Correlation**
A statistical measure of how two assets move in relation to each other. Ranges from -1 (opposite) to +1 (identical).

### D

**Daily Loss Limit**
The maximum loss allowed in a single trading day before trading is halted.

**Diversification Benefit**
The reduction in portfolio risk from holding uncorrelated assets. Portfolio VaR is typically less than the sum of individual VaRs.

**Drawdown**
The decline from a peak to a trough in portfolio value. Maximum drawdown is the largest such decline.

**Dynamic Threshold**
Risk limits that automatically adjust based on market conditions such as volatility or liquidity.

### E

**Exposure**
The total value at risk in the market. Gross exposure is the sum of absolute values; net exposure is long minus short.

### G

**Gross Exposure**
The sum of absolute position values: |long positions| + |short positions|.

### H

**Herfindahl Index**
A measure of portfolio concentration. Ranges from 0 (perfectly diversified) to 1 (single position).

**Historical Simulation**
A VaR calculation method using actual historical returns to estimate potential losses.

### L

**Leverage Ratio**
The ratio of gross exposure to account equity. Higher ratios indicate more risk.

### M

**Margin**
Collateral required to open and maintain leveraged positions.

**Margin Call**
A demand for additional collateral when account equity falls below maintenance requirements.

**Marginal VaR**
The change in portfolio VaR from a small increase in a position. Used for risk budgeting.

**Maximum Drawdown**
The largest peak-to-trough decline in portfolio value over a specified period.

**Monte Carlo Simulation**
A VaR calculation method using randomly generated scenarios to estimate potential losses.

### N

**Net Exposure**
Long exposure minus short exposure. Indicates directional bias.

### P

**Parametric VaR**
A VaR calculation method assuming returns follow a normal distribution. Also called variance-covariance method.

**Position Limit**
The maximum allowed position size for a single asset or the portfolio.

**Pre-Trade Risk Check**
Risk validation performed before an order is sent to the exchange.

### R

**Risk Budget**
The allocated amount of risk (typically measured in VaR) for a strategy or portfolio.

**Risk Contribution**
The portion of total portfolio risk attributable to a specific position.

### S

**Scenario Analysis**
Evaluation of portfolio performance under hypothetical market conditions.

**Sensitivity Analysis**
Analysis of how portfolio value changes with gradual changes in a single factor.

**Sharpe Ratio**
A measure of risk-adjusted return: (return - risk-free rate) / standard deviation.

**Sortino Ratio**
A measure of risk-adjusted return using only downside deviation, not total volatility.

**Stress Test**
Evaluation of portfolio performance under extreme market conditions.

### T

**Tail Risk**
The risk of extreme losses beyond normal market movements. Measured by CVaR.

### V

**Value at Risk (VaR)**
The maximum expected loss at a given confidence level over a specified time period.

**Volatility**
A measure of price variability. Higher volatility indicates greater price swings and risk.

---

## Technical Terms

### A

**Adapter**
A component that translates between VeloZ's internal format and an exchange's specific API.

**API Key**
A credential used to authenticate with an exchange or VeloZ's API.

### B

**Backpressure**
A flow control mechanism that slows down producers when consumers cannot keep up.

**BFF (Backend for Frontend)**
The gateway component that serves as an intermediary between the UI and the engine.

### C

**Control Plane**
Low-frequency commands for system management: start/stop strategies, parameter updates, permissions.

### D

**Data Plane**
High-frequency events for trading: market data, order updates, fills, balance changes.

### E

**Engine**
The C++ core component that processes market data, executes strategies, and manages orders.

**Event-Driven**
An architecture where components communicate through events rather than direct calls.

**Event Loop**
A programming construct that waits for and dispatches events. VeloZ uses KJ's event loop.

### G

**Gateway**
The Python HTTP server that bridges the engine to external clients via REST and SSE.

### H

**HMAC**
Hash-based Message Authentication Code. Used to sign API requests for authentication.

### I

**Idempotency**
The property that an operation can be applied multiple times without changing the result. Critical for order placement.

### J

**JSON**
JavaScript Object Notation. The data format used for API requests and responses.

### K

**KJ Library**
The C++ library from Cap'n Proto used throughout VeloZ for memory management, async I/O, and utilities.

**Kline**
Candlestick data showing open, high, low, close prices and volume for a time period.

### L

**Lock-Free Queue**
A concurrent data structure that allows multiple threads to access it without locks.

### M

**Modular Monolith**
An architecture where components are logically separated but deployed as a single unit.

### N

**NDJSON**
Newline-Delimited JSON. A format where each line is a separate JSON object. Used for engine-gateway communication.

**Normalization**
Converting exchange-specific data formats to VeloZ's unified internal format.

### O

**Object Pool**
A pre-allocated collection of reusable objects to reduce memory allocation overhead.

**OMS (Order Management System)**
The component that tracks order state, positions, and balances.

### P

**Prometheus**
A monitoring system used for collecting and querying metrics.

### R

**Rate Limit**
Restrictions on the number of API requests allowed in a time period.

**Reconciliation**
The process of synchronizing local state with exchange state to ensure consistency.

**REST API**
Representational State Transfer API. The HTTP-based API for VeloZ operations.

**Ring Buffer**
A fixed-size circular buffer used for high-performance event queuing.

### S

**Sequence Number**
A monotonically increasing number used to detect gaps and ensure ordering in data streams.

**SSE (Server-Sent Events)**
A technology for pushing real-time updates from server to client over HTTP.

**Snapshot**
A complete point-in-time copy of state, such as the full order book.

### T

**Timestamp (ts_ns)**
A timestamp in nanoseconds since Unix epoch. Used for precise event ordering.

### U

**User Data Stream**
A WebSocket connection providing real-time account, order, and balance updates.

### W

**WAL (Write-Ahead Log)**
A log where changes are recorded before being applied, enabling recovery after failures.

**WebSocket**
A protocol providing full-duplex communication over a single TCP connection. Used for real-time data.

---

## Exchange-Specific Terms

### A

**API Secret**
The private key paired with an API key, used to sign requests. Must be kept confidential.

### B

**Binance Testnet**
A sandbox environment for testing without real funds. Available at testnet.binance.vision.

### C

**Coin-Margined**
Perpetual contracts where margin and settlement are in the base cryptocurrency (e.g., BTC).

### D

**Depth**
The order book data showing bid and ask prices with quantities at each price level.

### F

**Funding Rate**
A periodic payment between long and short position holders in perpetual contracts.

### I

**Index Price**
A reference price calculated from multiple exchanges, used for perpetual contract calculations.

**IP Whitelist**
A security feature restricting API access to specific IP addresses.

### L

**Listen Key**
A token used to authenticate WebSocket connections for user data streams.

### M

**Mainnet**
The production environment with real funds and live trading.

**Mark Price**
A fair price calculated to prevent manipulation, used for liquidation calculations.

### O

**Open Interest**
The total number of outstanding derivative contracts that have not been settled.

### P

**Passphrase**
An additional credential required by some exchanges (e.g., OKX) for API authentication.

**Perpetual Contract**
A derivative contract with no expiration date, using funding rates to track spot price.

### R

**recvWindow**
A time window (in milliseconds) within which a request must be received by the exchange.

### S

**Sandbox**
A test environment for development. Coinbase uses "sandbox" instead of "testnet".

**Spot Trading**
Trading where assets are exchanged immediately at current market prices.

### T

**Testnet**
A test environment that simulates the production exchange without real funds.

**Trading Rules**
Exchange-defined constraints including lot size, tick size, min notional, and price filters.

### U

**USDT-Margined**
Perpetual contracts where margin and settlement are in USDT stablecoin.

### W

**Weight**
A measure of API request cost. Exchanges track cumulative weight against rate limits.

---

## See Also

- [Trading Guide](trading-guide.md) - Complete trading operations guide
- [Risk Management Guide](risk-management.md) - Risk controls and monitoring
- [API Usage Guide](api-usage-guide.md) - API examples and reference
- [Binance Integration](binance.md) - Exchange-specific setup
- [Configuration Guide](configuration.md) - Environment variables and settings
