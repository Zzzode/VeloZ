# Risk Management User Guide

This guide explains how to use VeloZ's comprehensive risk management features to protect your trading capital and optimize risk-adjusted returns.

## Table of Contents

1. [Overview](#overview)
2. [Understanding VaR Models](#understanding-var-models)
3. [Running Stress Tests](#running-stress-tests)
4. [Scenario Analysis](#scenario-analysis)
5. [Portfolio Risk Monitoring](#portfolio-risk-monitoring)
6. [Setting Risk Limits](#setting-risk-limits)
7. [Circuit Breaker Configuration](#circuit-breaker-configuration)
8. [Real-Time Risk Alerts](#real-time-risk-alerts)
9. [Dynamic Thresholds](#dynamic-thresholds)
10. [Best Practices](#best-practices)

---

## Overview

VeloZ provides a multi-layered risk management system designed for cryptocurrency trading:

| Layer | Purpose | Components |
|-------|---------|------------|
| **Pre-Trade** | Prevent risky orders | Position limits, price deviation checks, order rate limits |
| **Real-Time** | Monitor active risk | VaR calculation, exposure tracking, drawdown monitoring |
| **Post-Trade** | Analyze and adjust | Stress testing, scenario analysis, risk attribution |
| **Automatic** | Protect capital | Circuit breakers, stop-loss, dynamic thresholds |

### Key Risk Metrics

VeloZ calculates and monitors these risk metrics:

- **VaR (Value at Risk)**: Maximum expected loss at 95% and 99% confidence levels
- **CVaR (Conditional VaR)**: Expected loss beyond VaR threshold (tail risk)
- **Maximum Drawdown**: Largest peak-to-trough decline
- **Sharpe Ratio**: Risk-adjusted return measure
- **Sortino Ratio**: Downside risk-adjusted return
- **Exposure Metrics**: Gross, net, long, and short exposure
- **Concentration Metrics**: Position concentration and diversification

---

## Understanding VaR Models

Value at Risk (VaR) estimates the maximum potential loss over a specified time period at a given confidence level.

### VaR Calculation Methods

VeloZ supports three VaR calculation methods:

#### 1. Historical Simulation

Uses actual historical returns to estimate potential losses.

**Best for**: Portfolios with sufficient historical data (252+ days recommended)

```json
{
  "var_config": {
    "method": "Historical",
    "lookback_days": 252,
    "holding_period_days": 1,
    "calculate_cvar": true
  }
}
```

**Advantages**:
- No distribution assumptions
- Captures actual market behavior
- Includes fat tails naturally

**Limitations**:
- Requires sufficient historical data
- Past may not predict future

#### 2. Parametric (Variance-Covariance)

Assumes returns follow a normal distribution.

**Best for**: Quick calculations, well-diversified portfolios

```json
{
  "var_config": {
    "method": "Parametric",
    "holding_period_days": 1
  }
}
```

**Advantages**:
- Fast calculation
- Works with limited data
- Easy to understand

**Limitations**:
- Assumes normal distribution
- May underestimate tail risk

#### 3. Monte Carlo Simulation

Generates thousands of random scenarios to estimate potential losses.

**Best for**: Complex portfolios, non-linear instruments

```json
{
  "var_config": {
    "method": "MonteCarlo",
    "monte_carlo_paths": 10000,
    "random_seed": 42
  }
}
```

**Advantages**:
- Flexible distribution assumptions
- Handles complex portfolios
- Can model non-linear risks

**Limitations**:
- Computationally intensive
- Results depend on model assumptions

### Interpreting VaR Results

```json
{
  "var_result": {
    "var_95": 5000.0,
    "var_99": 8500.0,
    "cvar_95": 6200.0,
    "cvar_99": 10500.0,
    "method": "Historical",
    "sample_size": 252,
    "mean_return": 0.0012,
    "std_dev": 0.025,
    "valid": true
  }
}
```

**Interpretation**:
- `var_95: 5000.0`: 95% confidence that daily loss won't exceed $5,000
- `var_99: 8500.0`: 99% confidence that daily loss won't exceed $8,500
- `cvar_95: 6200.0`: If loss exceeds VaR, expected loss is $6,200

### Portfolio VaR with Correlations

For multi-asset portfolios, VeloZ considers correlations between assets:

```json
{
  "portfolio_var": {
    "positions": [
      {"symbol": "BTCUSDT", "weight": 0.6, "volatility": 0.65},
      {"symbol": "ETHUSDT", "weight": 0.4, "volatility": 0.75}
    ],
    "covariances": [
      {"symbol1": "BTCUSDT", "symbol2": "ETHUSDT", "covariance": 0.42}
    ],
    "portfolio_value": 100000.0
  }
}
```

**Diversification Benefit**: When assets are not perfectly correlated, portfolio VaR is less than the sum of individual VaRs.

---

## Running Stress Tests

Stress testing evaluates portfolio performance under extreme market conditions.

### Built-in Historical Scenarios

VeloZ includes pre-configured scenarios based on actual market events:

| Scenario | Description | BTC Impact | Volatility Impact |
|----------|-------------|------------|-------------------|
| **COVID-19 Crash** | March 2020 market crash | -50% | +300% |
| **LUNA Collapse** | May 2022 UST depeg | -30% | +200% |
| **FTX Collapse** | November 2022 exchange failure | -25% | +150% |
| **Flash Crash** | Sudden drop and recovery | -15% | +100% |

### Running a Stress Test

**Via API**:
```bash
# Run specific scenario
curl -X POST http://127.0.0.1:8080/api/risk/stress-test \
  -H "Content-Type: application/json" \
  -d '{
    "scenario_id": "covid_crash",
    "positions": [
      {"symbol": "BTCUSDT", "size": 1.0, "entry_price": 50000, "current_price": 52000}
    ]
  }'
```

**Response**:
```json
{
  "scenario_id": "covid_crash",
  "scenario_name": "COVID-19 March 2020 Crash",
  "success": true,
  "base_portfolio_value": 52000.0,
  "stressed_portfolio_value": 26000.0,
  "total_pnl_impact": -26000.0,
  "total_pnl_impact_pct": -50.0,
  "position_results": [
    {
      "symbol": "BTCUSDT",
      "base_value": 52000.0,
      "stressed_value": 26000.0,
      "pnl_impact": -26000.0,
      "pnl_impact_pct": -50.0
    }
  ],
  "stressed_var_95": 7800.0,
  "stressed_var_99": 13000.0
}
```

### Creating Custom Scenarios

Define your own stress scenarios:

```bash
curl -X POST http://127.0.0.1:8080/api/risk/scenarios \
  -H "Content-Type: application/json" \
  -d '{
    "id": "custom_crash",
    "name": "Custom Market Crash",
    "description": "Hypothetical 40% market decline",
    "type": "Hypothetical",
    "shocks": [
      {"factor": "Price", "symbol": "", "shock_value": -0.40, "is_relative": true},
      {"factor": "Volatility", "symbol": "", "shock_value": 1.50, "is_relative": true}
    ]
  }'
```

### Shock Types

| Factor | Description | Example |
|--------|-------------|---------|
| **Price** | Asset price change | -30% crash |
| **Volatility** | Implied/realized vol change | +200% spike |
| **Correlation** | Cross-asset correlation | +0.3 increase |
| **Liquidity** | Spread/depth change | +500% spread |
| **FundingRate** | Perpetual funding rate | +0.1% per 8h |

### Running All Scenarios

```bash
curl -X POST http://127.0.0.1:8080/api/risk/stress-test/all \
  -H "Content-Type: application/json" \
  -d '{
    "positions": [
      {"symbol": "BTCUSDT", "size": 1.0, "entry_price": 50000, "current_price": 52000},
      {"symbol": "ETHUSDT", "size": 10.0, "entry_price": 3000, "current_price": 3200}
    ]
  }'
```

### Sensitivity Analysis

Analyze how portfolio responds to gradual changes in a single factor:

```bash
curl -X POST http://127.0.0.1:8080/api/risk/sensitivity \
  -H "Content-Type: application/json" \
  -d '{
    "factor": "Price",
    "shock_min": -0.50,
    "shock_max": 0.50,
    "num_points": 21,
    "positions": [...]
  }'
```

**Response**:
```json
{
  "factor": "Price",
  "shock_levels": [-0.50, -0.45, ..., 0.45, 0.50],
  "pnl_impacts": [-52000, -46800, ..., 46800, 52000],
  "delta": 104000.0,
  "gamma": 0.0
}
```

---

## Scenario Analysis

Scenario analysis extends stress testing with probability weighting and recovery analysis.

### Enhanced Scenarios

Enhanced scenarios include probability estimates and recovery expectations:

```json
{
  "scenario": {
    "id": "market_crash_2024",
    "name": "Hypothetical 2024 Market Crash",
    "probability": "Low",
    "probability_estimate": 0.05,
    "time_horizon_days": 7,
    "is_instantaneous": false,
    "category": "Market Crash",
    "tags": ["black_swan", "systemic"],
    "expected_recovery_days": 90,
    "recovery_rate": 0.02
  }
}
```

### Probability Levels

| Level | Probability Range | Typical Events |
|-------|-------------------|----------------|
| **VeryLow** | < 1% | Black swan events |
| **Low** | 1-5% | Major market crashes |
| **Medium** | 5-20% | Significant corrections |
| **High** | 20-50% | Normal market volatility |
| **VeryHigh** | > 50% | Expected market behavior |

### Portfolio Impact Analysis

```bash
curl -X POST http://127.0.0.1:8080/api/risk/scenario-analysis \
  -H "Content-Type: application/json" \
  -d '{
    "scenario_id": "market_crash_2024",
    "positions": [...],
    "account_equity": 100000.0,
    "margin_requirement": 20000.0
  }'
```

**Response**:
```json
{
  "scenario_id": "market_crash_2024",
  "immediate_pnl": -35000.0,
  "expected_pnl": -1750.0,
  "worst_case_pnl": -45000.0,
  "base_var_95": 5000.0,
  "stressed_var_95": 12000.0,
  "var_increase_pct": 140.0,
  "margin_call_risk": true,
  "liquidation_risk": false,
  "margin_utilization": 0.85,
  "days_to_breakeven": 45,
  "recovery_probability": 0.75
}
```

### Scenario Comparison

Compare multiple scenarios to understand worst-case exposure:

```bash
curl http://127.0.0.1:8080/api/risk/scenario-comparison
```

**Response**:
```json
{
  "scenarios_count": 5,
  "worst_pnl": -52000.0,
  "best_pnl": -8000.0,
  "average_pnl": -25000.0,
  "expected_pnl": -3500.0,
  "worst_scenario_id": "covid_crash",
  "worst_scenario_name": "COVID-19 March 2020 Crash",
  "pnl_std_dev": 15000.0,
  "pnl_5th_percentile": -48000.0,
  "pnl_95th_percentile": -10000.0
}
```

### Reverse Stress Testing

Find what scenario would cause a specific loss:

```bash
curl -X POST http://127.0.0.1:8080/api/risk/reverse-stress \
  -H "Content-Type: application/json" \
  -d '{
    "target_loss": 50000.0,
    "positions": [...]
  }'
```

This generates a scenario that would approximately cause the target loss.

---

## Portfolio Risk Monitoring

Real-time monitoring of portfolio risk metrics.

### Exposure Metrics

```bash
curl http://127.0.0.1:8080/api/risk/exposure
```

**Response**:
```json
{
  "gross_exposure": 150000.0,
  "net_exposure": 50000.0,
  "long_exposure": 100000.0,
  "short_exposure": 50000.0,
  "leverage_ratio": 1.5,
  "net_leverage_ratio": 0.5
}
```

**Metric Definitions**:
- **Gross Exposure**: Sum of absolute position values (|long| + |short|)
- **Net Exposure**: Long exposure minus short exposure
- **Leverage Ratio**: Gross exposure / account equity
- **Net Leverage Ratio**: Net exposure / account equity

### Concentration Metrics

```bash
curl http://127.0.0.1:8080/api/risk/concentration
```

**Response**:
```json
{
  "largest_position_symbol": "BTCUSDT",
  "largest_position_pct": 45.0,
  "top3_concentration_pct": 85.0,
  "herfindahl_index": 0.35,
  "position_count": 5
}
```

**Interpretation**:
- **Herfindahl Index**: 0 = perfectly diversified, 1 = single position
- **Top 3 Concentration**: Percentage of exposure in largest 3 positions

### Risk Contribution Analysis

Understand which positions contribute most to portfolio risk:

```bash
curl http://127.0.0.1:8080/api/risk/contributions
```

**Response**:
```json
{
  "contributions": [
    {
      "symbol": "BTCUSDT",
      "position_value": 52000.0,
      "weight": 0.52,
      "standalone_var": 3900.0,
      "marginal_var": 65.0,
      "component_var": 3380.0,
      "pct_contribution": 67.6,
      "diversification_benefit": 520.0
    },
    {
      "symbol": "ETHUSDT",
      "position_value": 32000.0,
      "weight": 0.32,
      "standalone_var": 2880.0,
      "marginal_var": 50.0,
      "component_var": 1600.0,
      "pct_contribution": 32.0,
      "diversification_benefit": 1280.0
    }
  ],
  "total_var_95": 5000.0,
  "undiversified_var": 6780.0,
  "diversification_benefit": 1780.0
}
```

### Risk Budget Tracking

Monitor risk budget utilization:

```bash
curl http://127.0.0.1:8080/api/risk/budget
```

**Response**:
```json
{
  "allocations": [
    {
      "name": "momentum_strategy",
      "allocated_var": 3000.0,
      "used_var": 2400.0,
      "utilization_pct": 80.0,
      "remaining_var": 600.0,
      "is_breached": false
    },
    {
      "name": "mean_reversion",
      "allocated_var": 2000.0,
      "used_var": 2200.0,
      "utilization_pct": 110.0,
      "remaining_var": -200.0,
      "is_breached": true
    }
  ],
  "total_budget": 5000.0,
  "total_used": 4600.0,
  "total_utilization_pct": 92.0
}
```

---

## Setting Risk Limits

Configure risk limits to prevent excessive exposure.

### Pre-Trade Risk Limits

```bash
# Configure via environment variables
export VELOZ_MAX_POSITION_SIZE=1.0        # Max position per symbol
export VELOZ_MAX_LEVERAGE=3.0             # Max leverage ratio
export VELOZ_MAX_ORDER_SIZE=0.1           # Max single order size
export VELOZ_MAX_ORDER_RATE=10            # Max orders per second
export VELOZ_MAX_PRICE_DEVIATION=0.05     # Max 5% from reference price
```

### Strategy-Level Risk Limits

```json
{
  "strategy_id": "momentum_1",
  "risk_limits": {
    "max_position_size": 1.0,
    "max_drawdown_pct": 10.0,
    "max_daily_loss": 1000.0,
    "max_order_size": 0.1,
    "stop_loss_pct": 2.0,
    "take_profit_pct": 5.0,
    "var_budget": 3000.0
  }
}
```

### Account-Level Risk Limits

```json
{
  "account_limits": {
    "max_gross_exposure": 500000.0,
    "max_net_exposure": 200000.0,
    "max_leverage": 5.0,
    "max_concentration_pct": 50.0,
    "max_daily_loss": 10000.0,
    "max_drawdown_pct": 20.0
  }
}
```

### Stop-Loss and Take-Profit

Enable automatic position protection:

```bash
# Enable stop-loss
export VELOZ_STOP_LOSS_ENABLED=true
export VELOZ_STOP_LOSS_PCT=0.05           # 5% stop-loss

# Enable take-profit
export VELOZ_TAKE_PROFIT_ENABLED=true
export VELOZ_TAKE_PROFIT_PCT=0.10         # 10% take-profit
```

### Risk Level Thresholds

Configure alert thresholds for different risk levels:

```json
{
  "risk_thresholds": {
    "low": 0.25,
    "medium": 0.50,
    "high": 0.75,
    "critical": 0.90
  }
}
```

---

## Circuit Breaker Configuration

Circuit breakers automatically halt trading when risk thresholds are breached.

### How Circuit Breakers Work

```
┌─────────────────────────────────────────────────────────────────┐
│                    Circuit Breaker States                       │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│   ┌──────────┐    failures    ┌──────────┐    timeout    ┌──────────┐
│   │  CLOSED  │ ─────────────> │   OPEN   │ ────────────> │ HALF-OPEN│
│   │ (Normal) │                │ (Blocked)│               │ (Testing)│
│   └──────────┘                └──────────┘               └──────────┘
│        ^                                                      │
│        │                    success                           │
│        └──────────────────────────────────────────────────────┘
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

**States**:
- **Closed**: Normal operation, requests allowed
- **Open**: Circuit tripped, requests blocked
- **Half-Open**: Testing recovery, limited requests allowed

### Configuration

```bash
# Circuit breaker settings
export VELOZ_CB_FAILURE_THRESHOLD=5       # Failures before tripping
export VELOZ_CB_TIMEOUT_MS=60000          # Time before testing recovery (1 min)
export VELOZ_CB_SUCCESS_THRESHOLD=2       # Successes needed to close
```

### Programmatic Configuration

```json
{
  "circuit_breaker": {
    "name": "trading_circuit",
    "failure_threshold": 5,
    "timeout_ms": 60000,
    "success_threshold": 2
  }
}
```

### Circuit Breaker Triggers

VeloZ trips circuit breakers when:

| Trigger | Description | Default Threshold |
|---------|-------------|-------------------|
| **Drawdown** | Account drawdown exceeds limit | 10% |
| **Daily Loss** | Daily loss exceeds limit | $10,000 |
| **Position Size** | Position exceeds maximum | 100% of limit |
| **Consecutive Losses** | Rapid consecutive losses | 5 losses |
| **Error Rate** | High API error rate | 50% |

### Monitoring Circuit Breaker Status

```bash
curl http://127.0.0.1:8080/api/risk/circuit-breaker
```

**Response**:
```json
{
  "name": "trading_circuit",
  "state": "closed",
  "stats": {
    "total_requests": 1000,
    "successful_requests": 980,
    "failed_requests": 20,
    "rejected_requests": 0,
    "state_transitions": 2,
    "failure_rate": 0.02,
    "success_rate": 0.98
  }
}
```

### Manual Reset

```bash
curl -X POST http://127.0.0.1:8080/api/risk/circuit-breaker/reset
```

---

## Real-Time Risk Alerts

Configure alerts to be notified of risk events.

### Alert Levels

| Level | Description | Action |
|-------|-------------|--------|
| **Info** | Informational update | Log only |
| **Warning** | Approaching threshold | Review positions |
| **Critical** | Threshold breached | Immediate action required |

### Configuring Alert Thresholds

```bash
# VaR thresholds (as % of budget)
export VELOZ_VAR_WARNING_THRESHOLD=0.80   # 80% of VaR budget
export VELOZ_VAR_CRITICAL_THRESHOLD=0.95  # 95% of VaR budget

# Concentration thresholds
export VELOZ_CONCENTRATION_WARNING=0.50   # 50% in single position

# Drawdown thresholds
export VELOZ_DRAWDOWN_WARNING=0.10        # 10% drawdown
```

### Alert Types

```json
{
  "alerts": [
    {
      "level": "Warning",
      "message": "VaR utilization at 85% of budget",
      "symbol": "",
      "current_value": 4250.0,
      "threshold": 5000.0,
      "timestamp_ns": 1708704000000000000
    },
    {
      "level": "Critical",
      "message": "Position concentration exceeds 60%",
      "symbol": "BTCUSDT",
      "current_value": 0.62,
      "threshold": 0.50,
      "timestamp_ns": 1708704001000000000
    }
  ]
}
```

### Webhook Notifications

Configure webhooks for external alerting:

```bash
export VELOZ_ALERT_WEBHOOK_URL=https://your-webhook.com/alerts
export VELOZ_ALERT_WEBHOOK_SECRET=your_secret
```

### SSE Alert Stream

Subscribe to real-time alerts via Server-Sent Events:

```bash
curl -N http://127.0.0.1:8080/api/stream?filter=risk_alert
```

**Event Format**:
```
event: risk_alert
data: {"level":"Warning","message":"Drawdown at 8%","current_value":0.08,"threshold":0.10}
```

---

## Dynamic Thresholds

Dynamic thresholds automatically adjust risk limits based on market conditions.

### Market Conditions

VeloZ detects and responds to different market conditions:

| Condition | Description | Threshold Adjustment |
|-----------|-------------|---------------------|
| **Normal** | Standard market behavior | Base thresholds |
| **HighVolatility** | Elevated volatility | Reduce position sizes |
| **LowLiquidity** | Wide spreads, thin books | Reduce order sizes |
| **Trending** | Strong directional move | Adjust stop-loss |
| **MeanReverting** | Range-bound market | Tighter stops |
| **Crisis** | Extreme conditions | Minimal exposure |

### Configuration

```json
{
  "dynamic_thresholds": {
    "base_max_position_size": 100.0,
    "base_max_leverage": 3.0,
    "base_stop_loss_pct": 0.05,
    "vol_scale_factor": 0.5,
    "vol_lookback_days": 20,
    "drawdown_reduction_start": 0.05,
    "drawdown_reduction_rate": 2.0,
    "reduce_before_close": false,
    "minutes_before_close": 30
  }
}
```

### How Adjustments Work

**Volatility Adjustment**:
```
adjusted_position = base_position * (1 - vol_scale_factor * volatility_percentile / 100)
```

Example: If volatility is at 80th percentile with scale factor 0.5:
- Adjustment = 1 - 0.5 * 0.80 = 0.60
- Position reduced to 60% of base

**Drawdown Adjustment**:
```
if drawdown > drawdown_reduction_start:
  reduction = (drawdown - start) * reduction_rate
  adjusted_position = base_position * (1 - reduction)
```

Example: 8% drawdown with 5% start and 2.0 rate:
- Reduction = (0.08 - 0.05) * 2.0 = 0.06
- Position reduced by 6%

### Viewing Current Adjustments

```bash
curl http://127.0.0.1:8080/api/risk/dynamic-thresholds
```

**Response**:
```json
{
  "market_condition": "HighVolatility",
  "volatility_percentile": 75.0,
  "liquidity_score": 0.8,
  "trend_strength": 45.0,
  "current_drawdown": 0.03,
  "effective_max_position_size": 62.5,
  "effective_max_leverage": 1.875,
  "effective_stop_loss_pct": 0.0625,
  "position_size_multiplier": 0.625,
  "explanation": "Position size reduced to 62.5% due to high volatility (75th percentile)"
}
```

---

## Best Practices

### 1. Start Conservative

Begin with conservative risk limits and adjust based on experience:

```json
{
  "conservative_settings": {
    "max_position_size": 0.5,
    "max_leverage": 1.0,
    "max_drawdown_pct": 5.0,
    "stop_loss_pct": 2.0
  }
}
```

### 2. Diversify Across Assets

Maintain diversification to reduce concentration risk:

- Keep single position below 30% of portfolio
- Maintain Herfindahl Index below 0.25
- Trade uncorrelated assets when possible

### 3. Use Multiple Risk Layers

Combine different risk controls:

1. **Pre-trade checks**: Prevent risky orders
2. **Position limits**: Cap exposure per asset
3. **Stop-loss orders**: Limit downside per trade
4. **Circuit breakers**: Halt trading in crisis
5. **Dynamic thresholds**: Adapt to conditions

### 4. Regular Stress Testing

Run stress tests regularly:

- **Daily**: Quick scenario check
- **Weekly**: Full scenario suite
- **Monthly**: Custom scenario review
- **After major events**: Immediate assessment

### 5. Monitor Risk Metrics Continuously

Set up monitoring dashboards for:

- Real-time VaR and CVaR
- Exposure and leverage ratios
- Concentration metrics
- Drawdown tracking
- Circuit breaker status

### 6. Document Risk Policies

Maintain clear documentation of:

- Risk limits and rationale
- Escalation procedures
- Manual override processes
- Recovery procedures

### 7. Test Recovery Procedures

Regularly test:

- Circuit breaker reset process
- Position unwinding procedures
- Emergency contact procedures
- Backup system activation

---

## Troubleshooting

### Common Issues

**VaR calculation returns invalid**:
- Ensure sufficient historical data (minimum 30 days)
- Check for missing price data
- Verify portfolio value is non-zero

**Circuit breaker won't reset**:
- Wait for timeout period to elapse
- Check if underlying issue is resolved
- Use manual reset if necessary

**Dynamic thresholds too aggressive**:
- Reduce `vol_scale_factor`
- Increase `drawdown_reduction_start`
- Review market condition detection

**Alerts not triggering**:
- Verify threshold configuration
- Check webhook URL accessibility
- Review alert callback setup

### Getting Help

- Check [Troubleshooting Guide](../deployment/troubleshooting.md)
- Review [API Documentation](../../api/README.md)
- Report issues on GitHub

---

## Summary

VeloZ's risk management system provides comprehensive protection for your trading capital:

| Feature | Purpose | Key Configuration |
|---------|---------|-------------------|
| **VaR Models** | Quantify potential losses | Method, confidence level |
| **Stress Testing** | Test extreme scenarios | Scenario selection |
| **Scenario Analysis** | Probability-weighted analysis | Probability estimates |
| **Portfolio Monitoring** | Real-time risk tracking | Alert thresholds |
| **Risk Limits** | Prevent excessive exposure | Position/leverage limits |
| **Circuit Breakers** | Automatic trading halt | Failure thresholds |
| **Risk Alerts** | Timely notifications | Warning/critical levels |
| **Dynamic Thresholds** | Adaptive risk control | Market condition response |

Remember: **Risk management is not about avoiding all risk, but about understanding and controlling it.**
