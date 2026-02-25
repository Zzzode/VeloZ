# P0-3: Strategy Marketplace

**Version**: 1.0.0
**Date**: 2026-02-25
**Status**: Design Phase
**Complexity**: High
**Estimated Time**: 5-7 weeks

---

## 1. Overview

### 1.1 Goals

- Provide a curated library of pre-built trading strategies
- Enable users to browse, preview, and deploy strategies without coding
- Support strategy parameter customization with validation
- Integrate with backtest engine for strategy evaluation
- Track strategy performance in real-time

### 1.2 Non-Goals

- User-generated strategy marketplace (community sharing)
- Strategy source code editing
- Multi-asset portfolio strategies
- Machine learning model training

---

## 2. Architecture

### 2.1 High-Level Architecture

```
                    Strategy Marketplace Architecture
                    ==================================

+------------------------------------------------------------------+
|                         React UI Layer                            |
+------------------------------------------------------------------+
|  +----------------+  +----------------+  +----------------+       |
|  | Strategy       |  | Parameter      |  | Performance    |       |
|  | Browser        |  | Configurator   |  | Dashboard      |       |
|  +----------------+  +----------------+  +----------------+       |
|           |                  |                  |                 |
|           v                  v                  v                 |
|  +----------------+  +----------------+  +----------------+       |
|  | Strategy Store |  | Backtest Store |  | Performance    |       |
|  | (Zustand)      |  | (Zustand)      |  | Store          |       |
|  +----------------+  +----------------+  +----------------+       |
+------------------------------------------------------------------+
                               |
                               v (REST API)
+------------------------------------------------------------------+
|                      Python Gateway Layer                         |
+------------------------------------------------------------------+
|  +----------------+  +----------------+  +----------------+       |
|  | Strategy       |  | Backtest       |  | Performance    |       |
|  | Repository     |  | Runner         |  | Tracker        |       |
|  +----------------+  +----------------+  +----------------+       |
|           |                  |                  |                 |
|           v                  v                  v                 |
|  +----------------+  +----------------+  +----------------+       |
|  | Template       |  | Historical     |  | Metrics        |       |
|  | Storage        |  | Data Store     |  | Database       |       |
|  +----------------+  +----------------+  +----------------+       |
+------------------------------------------------------------------+
                               |
                               v (stdio)
+------------------------------------------------------------------+
|                       C++ Engine Layer                            |
+------------------------------------------------------------------+
|  +----------------+  +----------------+  +----------------+       |
|  | Strategy       |  | Backtest       |  | Risk           |       |
|  | Runtime        |  | Engine         |  | Engine         |       |
|  | (libs/strategy)|  | (libs/backtest)|  | (libs/risk)    |       |
|  +----------------+  +----------------+  +----------------+       |
+------------------------------------------------------------------+
```

### 2.2 Component Diagram

```
+------------------------------------------------------------------+
|                    Strategy Marketplace System                    |
+------------------------------------------------------------------+
|                                                                   |
|  Strategy Repository                                              |
|  +----------------+  +----------------+  +----------------+       |
|  | Template       |  | Category       |  | Version        |       |
|  | Manager        |  | Index          |  | Control        |       |
|  +----------------+  +----------------+  +----------------+       |
|                                                                   |
|  Parameter System                                                 |
|  +----------------+  +----------------+  +----------------+       |
|  | Schema         |  | Validator      |  | Optimizer      |       |
|  | Definition     |  | Engine         |  | Hints          |       |
|  +----------------+  +----------------+  +----------------+       |
|                                                                   |
|  Execution System                                                 |
|  +----------------+  +----------------+  +----------------+       |
|  | Backtest       |  | Paper Trading  |  | Live           |       |
|  | Runner         |  | Simulator      |  | Executor       |       |
|  +----------------+  +----------------+  +----------------+       |
|                                                                   |
|  Performance Tracking                                             |
|  +----------------+  +----------------+  +----------------+       |
|  | Metrics        |  | Comparison     |  | Reporting      |       |
|  | Collector      |  | Engine         |  | Generator      |       |
|  +----------------+  +----------------+  +----------------+       |
|                                                                   |
+------------------------------------------------------------------+
```

---

## 3. Strategy Template Format

### 3.1 Template Schema

```yaml
# strategies/grid_trading.yaml
id: grid_trading_v1
name: Grid Trading Strategy
version: "1.0.0"
category: market_making
risk_level: medium
description: |
  Automated grid trading strategy that places buy and sell orders at
  predetermined price levels, profiting from price oscillations within a range.

author: VeloZ Team
created_at: "2026-01-15"
updated_at: "2026-02-20"

# Supported exchanges and symbols
supported_exchanges:
  - binance
  - binance_futures
  - okx

supported_symbols:
  - pattern: "*USDT"
    exchanges: [binance, okx]
  - pattern: "*BUSD"
    exchanges: [binance]

# Strategy parameters with validation
parameters:
  - name: grid_count
    type: integer
    label: Number of Grid Levels
    description: Total number of grid levels (buy + sell orders)
    default: 10
    min: 4
    max: 50
    step: 2
    category: grid_setup

  - name: price_range_percent
    type: decimal
    label: Price Range (%)
    description: Total price range as percentage of current price
    default: 0.10
    min: 0.02
    max: 0.50
    step: 0.01
    category: grid_setup

  - name: order_size
    type: decimal
    label: Order Size (base currency)
    description: Size of each grid order
    default: 0.01
    min: 0.001
    max: 10.0
    step: 0.001
    category: position

  - name: take_profit_percent
    type: decimal
    label: Take Profit (%)
    description: Close all positions when profit reaches this percentage
    default: 0.05
    min: 0.01
    max: 0.50
    optional: true
    category: exit

  - name: stop_loss_percent
    type: decimal
    label: Stop Loss (%)
    description: Close all positions when loss reaches this percentage
    default: 0.10
    min: 0.01
    max: 0.50
    optional: true
    category: exit

  - name: rebalance_threshold
    type: decimal
    label: Rebalance Threshold (%)
    description: Recreate grid when price moves outside this threshold
    default: 0.15
    min: 0.05
    max: 0.30
    category: advanced

# Parameter categories for UI grouping
parameter_categories:
  - id: grid_setup
    label: Grid Setup
    description: Configure the grid structure
    order: 1

  - id: position
    label: Position Sizing
    description: Configure order sizes
    order: 2

  - id: exit
    label: Exit Conditions
    description: Configure profit taking and stop loss
    order: 3

  - id: advanced
    label: Advanced Settings
    description: Fine-tune strategy behavior
    order: 4
    collapsed: true

# Parameter presets for quick configuration
presets:
  - name: Conservative
    description: Lower risk, smaller grid range
    values:
      grid_count: 8
      price_range_percent: 0.05
      order_size: 0.005
      stop_loss_percent: 0.05

  - name: Balanced
    description: Moderate risk and reward
    values:
      grid_count: 10
      price_range_percent: 0.10
      order_size: 0.01
      stop_loss_percent: 0.10

  - name: Aggressive
    description: Higher risk, larger grid range
    values:
      grid_count: 20
      price_range_percent: 0.20
      order_size: 0.02
      stop_loss_percent: 0.15

# Risk warnings
warnings:
  - condition: "price_range_percent > 0.30"
    message: "Large price range may result in capital being spread thin"
    severity: warning

  - condition: "grid_count > 30"
    message: "Many grid levels may increase exchange API usage"
    severity: info

  - condition: "!stop_loss_percent"
    message: "No stop loss configured - unlimited downside risk"
    severity: error

# Backtest configuration
backtest:
  default_period_days: 30
  recommended_period_days: 90
  min_period_days: 7
  default_initial_capital: 10000

# Performance benchmarks (from historical backtests)
benchmarks:
  btcusdt_30d:
    period: "2026-01-01 to 2026-01-31"
    symbol: BTCUSDT
    preset: Balanced
    metrics:
      total_return: 0.0823
      sharpe_ratio: 1.45
      max_drawdown: 0.0412
      win_rate: 0.68
      trades: 156

# Implementation reference
implementation:
  engine_class: veloz::strategy::GridStrategy
  factory_type: Grid
```

### 3.2 TypeScript Type Definitions

```typescript
// src/types/strategy-template.ts
export interface StrategyTemplate {
  id: string;
  name: string;
  version: string;
  category: StrategyCategory;
  riskLevel: 'low' | 'medium' | 'high';
  description: string;
  author: string;
  createdAt: string;
  updatedAt: string;

  supportedExchanges: string[];
  supportedSymbols: SymbolPattern[];

  parameters: ParameterDefinition[];
  parameterCategories: ParameterCategory[];
  presets: ParameterPreset[];
  warnings: ParameterWarning[];

  backtest: BacktestConfig;
  benchmarks: Record<string, BenchmarkResult>;

  implementation: {
    engineClass: string;
    factoryType: string;
  };
}

export interface ParameterDefinition {
  name: string;
  type: 'integer' | 'decimal' | 'boolean' | 'string' | 'enum';
  label: string;
  description: string;
  default: unknown;
  min?: number;
  max?: number;
  step?: number;
  options?: string[];  // For enum type
  optional?: boolean;
  category: string;
}

export interface ParameterCategory {
  id: string;
  label: string;
  description: string;
  order: number;
  collapsed?: boolean;
}

export interface ParameterPreset {
  name: string;
  description: string;
  values: Record<string, unknown>;
}

export interface ParameterWarning {
  condition: string;  // JavaScript expression
  message: string;
  severity: 'info' | 'warning' | 'error';
}

export type StrategyCategory =
  | 'trend_following'
  | 'mean_reversion'
  | 'momentum'
  | 'arbitrage'
  | 'market_making'
  | 'grid'
  | 'dca';
```

---

## 4. Strategy Browser UI

### 4.1 Browser Layout

```
+------------------------------------------------------------------+
|                    Strategy Marketplace                           |
+------------------------------------------------------------------+
| [Search...                    ] [Category v] [Risk v] [Sort v]   |
+------------------------------------------------------------------+
|                                                                   |
| +------------------+  +------------------+  +------------------+  |
| | Grid Trading     |  | DCA Strategy     |  | Momentum         |  |
| | ================ |  | ================ |  | ================ |  |
| | Market Making    |  | DCA              |  | Trend Following  |  |
| | Medium Risk      |  | Low Risk         |  | High Risk        |  |
| |                  |  |                  |  |                  |  |
| | Profit from      |  | Dollar-cost      |  | Follow market    |  |
| | price swings     |  | averaging into   |  | momentum with    |  |
| | within a range   |  | positions        |  | breakout signals |  |
| |                  |  |                  |  |                  |  |
| | [View Details]   |  | [View Details]   |  | [View Details]   |  |
| +------------------+  +------------------+  +------------------+  |
|                                                                   |
| +------------------+  +------------------+  +------------------+  |
| | Mean Reversion   |  | Arbitrage        |  | TWAP Execution   |  |
| | ================ |  | ================ |  | ================ |  |
| | Mean Reversion   |  | Arbitrage        |  | Execution        |  |
| | Medium Risk      |  | Low Risk         |  | Low Risk         |  |
| | ...              |  | ...              |  | ...              |  |
| +------------------+  +------------------+  +------------------+  |
|                                                                   |
+------------------------------------------------------------------+
```

### 4.2 Strategy Detail View

```
+------------------------------------------------------------------+
|  < Back to Marketplace                                            |
+------------------------------------------------------------------+
|                                                                   |
|  Grid Trading Strategy                              [Deploy]      |
|  =====================                                            |
|  Market Making | Medium Risk | v1.0.0                            |
|                                                                   |
|  Automated grid trading strategy that places buy and sell        |
|  orders at predetermined price levels, profiting from price      |
|  oscillations within a range.                                    |
|                                                                   |
+------------------------------------------------------------------+
|  [Overview] [Parameters] [Backtest] [Performance]                |
+------------------------------------------------------------------+
|                                                                   |
|  Overview                                                         |
|  --------                                                         |
|                                                                   |
|  How it works:                                                    |
|  1. Creates a grid of buy orders below current price             |
|  2. Creates a grid of sell orders above current price            |
|  3. When a buy order fills, places a sell order above it         |
|  4. When a sell order fills, places a buy order below it         |
|  5. Profits from the spread between buy and sell prices          |
|                                                                   |
|  Best suited for:                                                 |
|  - Ranging/sideways markets                                       |
|  - High liquidity pairs (BTC, ETH)                               |
|  - Traders who want passive income                               |
|                                                                   |
|  Not recommended for:                                             |
|  - Strong trending markets                                        |
|  - Low liquidity pairs                                           |
|  - Highly volatile periods                                       |
|                                                                   |
|  Supported Exchanges: Binance, Binance Futures, OKX              |
|                                                                   |
+------------------------------------------------------------------+
```

### 4.3 Strategy Browser Component

```typescript
// src/features/marketplace/StrategyBrowser.tsx
export function StrategyBrowser() {
  const [filters, setFilters] = useState<StrategyFilters>({
    search: '',
    category: null,
    riskLevel: null,
    sortBy: 'popularity',
  });

  const { data: strategies, isLoading } = useStrategies(filters);

  return (
    <div className="strategy-browser">
      <PageHeader
        title="Strategy Marketplace"
        subtitle="Browse and deploy pre-built trading strategies"
      />

      <div className="filters">
        <Input.Search
          placeholder="Search strategies..."
          value={filters.search}
          onChange={(e) => setFilters({ ...filters, search: e.target.value })}
        />

        <Select
          placeholder="Category"
          allowClear
          value={filters.category}
          onChange={(v) => setFilters({ ...filters, category: v })}
          options={CATEGORY_OPTIONS}
        />

        <Select
          placeholder="Risk Level"
          allowClear
          value={filters.riskLevel}
          onChange={(v) => setFilters({ ...filters, riskLevel: v })}
          options={RISK_LEVEL_OPTIONS}
        />

        <Select
          value={filters.sortBy}
          onChange={(v) => setFilters({ ...filters, sortBy: v })}
          options={[
            { value: 'popularity', label: 'Most Popular' },
            { value: 'performance', label: 'Best Performance' },
            { value: 'newest', label: 'Newest' },
            { value: 'name', label: 'Name A-Z' },
          ]}
        />
      </div>

      <div className="strategy-grid">
        {isLoading ? (
          <StrategyCardSkeleton count={6} />
        ) : (
          strategies?.map((strategy) => (
            <StrategyCard
              key={strategy.id}
              strategy={strategy}
              onClick={() => navigate(`/marketplace/${strategy.id}`)}
            />
          ))
        )}
      </div>
    </div>
  );
}
```

---

## 5. Parameter Configuration UI

### 5.1 Parameter Form

```
+------------------------------------------------------------------+
|  Configure Grid Trading Strategy                                  |
+------------------------------------------------------------------+
|                                                                   |
|  Quick Setup: [Conservative] [Balanced] [Aggressive]             |
|                                                                   |
+------------------------------------------------------------------+
|  Grid Setup                                                       |
+------------------------------------------------------------------+
|                                                                   |
|  Number of Grid Levels                                            |
|  [====|==========] 10                                            |
|  Total number of grid levels (buy + sell orders)                 |
|                                                                   |
|  Price Range (%)                                                  |
|  [====|==========] 10%                                           |
|  Total price range as percentage of current price                |
|                                                                   |
+------------------------------------------------------------------+
|  Position Sizing                                                  |
+------------------------------------------------------------------+
|                                                                   |
|  Order Size (BTC)                                                 |
|  [0.01        ]                                                   |
|  Size of each grid order                                         |
|                                                                   |
|  Estimated Capital Required: $4,500 USD                          |
|                                                                   |
+------------------------------------------------------------------+
|  Exit Conditions                                                  |
+------------------------------------------------------------------+
|                                                                   |
|  [x] Take Profit                                                  |
|      [====|======] 5%                                            |
|      Close all positions when profit reaches this percentage     |
|                                                                   |
|  [x] Stop Loss                                                    |
|      [====|======] 10%                                           |
|      Close all positions when loss reaches this percentage       |
|                                                                   |
+------------------------------------------------------------------+
|  [v] Advanced Settings                                            |
+------------------------------------------------------------------+
|                                                                   |
|  Rebalance Threshold (%)                                          |
|  [====|==========] 15%                                           |
|  Recreate grid when price moves outside this threshold           |
|                                                                   |
+------------------------------------------------------------------+
|                                                                   |
|  [!] Warning: No stop loss may result in significant losses      |
|                                                                   |
|  [Run Backtest]                    [Deploy Strategy]             |
|                                                                   |
+------------------------------------------------------------------+
```

### 5.2 Parameter Form Component

```typescript
// src/features/marketplace/ParameterForm.tsx
interface ParameterFormProps {
  template: StrategyTemplate;
  onSubmit: (params: Record<string, unknown>) => void;
  onBacktest: (params: Record<string, unknown>) => void;
}

export function ParameterForm({ template, onSubmit, onBacktest }: ParameterFormProps) {
  const [values, setValues] = useState<Record<string, unknown>>(() =>
    getDefaultValues(template.parameters)
  );
  const [warnings, setWarnings] = useState<ParameterWarning[]>([]);

  // Evaluate warnings when values change
  useEffect(() => {
    const activeWarnings = template.warnings.filter((w) =>
      evaluateCondition(w.condition, values)
    );
    setWarnings(activeWarnings);
  }, [values, template.warnings]);

  const applyPreset = (preset: ParameterPreset) => {
    setValues({ ...values, ...preset.values });
  };

  const groupedParams = useMemo(() =>
    groupByCategory(template.parameters, template.parameterCategories),
    [template]
  );

  return (
    <Form layout="vertical">
      {/* Preset buttons */}
      <div className="presets">
        <span>Quick Setup:</span>
        {template.presets.map((preset) => (
          <Button
            key={preset.name}
            size="small"
            onClick={() => applyPreset(preset)}
          >
            {preset.name}
          </Button>
        ))}
      </div>

      {/* Parameter categories */}
      {groupedParams.map((group) => (
        <Collapse
          key={group.category.id}
          defaultActiveKey={group.category.collapsed ? [] : [group.category.id]}
        >
          <Collapse.Panel
            header={group.category.label}
            key={group.category.id}
          >
            <p className="category-description">{group.category.description}</p>

            {group.parameters.map((param) => (
              <ParameterInput
                key={param.name}
                definition={param}
                value={values[param.name]}
                onChange={(v) => setValues({ ...values, [param.name]: v })}
              />
            ))}
          </Collapse.Panel>
        </Collapse>
      ))}

      {/* Warnings */}
      {warnings.map((warning, i) => (
        <Alert
          key={i}
          type={warning.severity === 'error' ? 'error' : 'warning'}
          message={warning.message}
          showIcon
        />
      ))}

      {/* Actions */}
      <div className="form-actions">
        <Button onClick={() => onBacktest(values)}>
          Run Backtest
        </Button>
        <Button
          type="primary"
          onClick={() => onSubmit(values)}
          disabled={warnings.some((w) => w.severity === 'error')}
        >
          Deploy Strategy
        </Button>
      </div>
    </Form>
  );
}

// Individual parameter input component
function ParameterInput({
  definition,
  value,
  onChange,
}: {
  definition: ParameterDefinition;
  value: unknown;
  onChange: (value: unknown) => void;
}) {
  const { type, label, description, min, max, step, options, optional } = definition;

  const input = useMemo(() => {
    switch (type) {
      case 'integer':
      case 'decimal':
        return (
          <Slider
            min={min}
            max={max}
            step={step}
            value={value as number}
            onChange={onChange}
            marks={{
              [min!]: `${min}`,
              [max!]: `${max}`,
            }}
          />
        );

      case 'boolean':
        return (
          <Switch
            checked={value as boolean}
            onChange={onChange}
          />
        );

      case 'enum':
        return (
          <Select
            value={value as string}
            onChange={onChange}
            options={options?.map((o) => ({ value: o, label: o }))}
          />
        );

      default:
        return (
          <Input
            value={value as string}
            onChange={(e) => onChange(e.target.value)}
          />
        );
    }
  }, [type, value, min, max, step, options, onChange]);

  return (
    <Form.Item
      label={
        <span>
          {label}
          {optional && <span className="optional">(optional)</span>}
        </span>
      }
      tooltip={description}
    >
      {input}
    </Form.Item>
  );
}
```

---

## 6. Backtest Integration

### 6.1 Backtest Flow

```
                        Backtest Flow
                        =============

User configures parameters
           |
           v
+------------------+
| Validate Params  |
| - Range checks   |
| - Type checks    |
+------------------+
           |
           v
+------------------+
| Fetch Historical |
| Data             |
| - OHLCV candles  |
| - Trades         |
+------------------+
           |
           v
+------------------+
| Run Backtest     |
| - C++ engine     |
| - Simulate fills |
+------------------+
           |
           v
+------------------+
| Calculate Metrics|
| - Returns        |
| - Risk metrics   |
| - Trade stats    |
+------------------+
           |
           v
+------------------+
| Display Results  |
| - Equity curve   |
| - Drawdown       |
| - Trade list     |
+------------------+
```

### 6.2 Backtest Results UI

```
+------------------------------------------------------------------+
|  Backtest Results: Grid Trading on BTCUSDT                        |
+------------------------------------------------------------------+
|  Period: 2026-01-01 to 2026-01-31 (30 days)                      |
|  Initial Capital: $10,000                                         |
+------------------------------------------------------------------+
|                                                                   |
|  Performance Summary                                              |
|  +----------------+  +----------------+  +----------------+       |
|  | Total Return   |  | Sharpe Ratio   |  | Max Drawdown   |       |
|  |    +8.23%      |  |     1.45       |  |    -4.12%      |       |
|  +----------------+  +----------------+  +----------------+       |
|                                                                   |
|  +----------------+  +----------------+  +----------------+       |
|  | Win Rate       |  | Profit Factor  |  | Total Trades   |       |
|  |    68.0%       |  |     2.15       |  |      156       |       |
|  +----------------+  +----------------+  +----------------+       |
|                                                                   |
+------------------------------------------------------------------+
|  Equity Curve                                                     |
|  +---------------------------------------------------------+     |
|  |                                              ___/        |     |
|  |                                         ___/             |     |
|  |                                    ___/                  |     |
|  |                               ___/                       |     |
|  |                          ___/                            |     |
|  |                     ___/                                 |     |
|  |                ___/                                      |     |
|  |           ___/                                           |     |
|  |      ___/                                                |     |
|  | ___/                                                     |     |
|  +---------------------------------------------------------+     |
|  Jan 1                                              Jan 31       |
|                                                                   |
+------------------------------------------------------------------+
|  Drawdown                                                         |
|  +---------------------------------------------------------+     |
|  |  0% |_____________________________________________       |     |
|  | -2% |         \_____/                                    |     |
|  | -4% |                    \___/                           |     |
|  +---------------------------------------------------------+     |
|                                                                   |
+------------------------------------------------------------------+
|  Trade Distribution                                               |
|  [Histogram of trade P&L]                                        |
|                                                                   |
+------------------------------------------------------------------+
|  [Export Report]  [Compare with Benchmark]  [Deploy Strategy]    |
+------------------------------------------------------------------+
```

### 6.3 Backtest API

```python
# apps/gateway/backtest_runner.py
from dataclasses import dataclass
from typing import List, Optional
import subprocess
import json

@dataclass
class BacktestRequest:
    strategy_id: str
    symbol: str
    exchange: str
    start_date: str
    end_date: str
    initial_capital: float
    parameters: dict

@dataclass
class BacktestResult:
    # Performance metrics
    total_return: float
    annualized_return: float
    sharpe_ratio: float
    sortino_ratio: float
    max_drawdown: float
    calmar_ratio: float

    # Trade statistics
    total_trades: int
    winning_trades: int
    losing_trades: int
    win_rate: float
    profit_factor: float
    avg_win: float
    avg_loss: float
    largest_win: float
    largest_loss: float

    # Time series data
    equity_curve: List[dict]  # [{timestamp, equity}]
    drawdown_curve: List[dict]  # [{timestamp, drawdown}]
    trades: List[dict]  # [{entry_time, exit_time, side, pnl, ...}]


class BacktestRunner:
    def __init__(self, engine_path: str, data_dir: str):
        self.engine_path = engine_path
        self.data_dir = data_dir

    async def run_backtest(self, request: BacktestRequest) -> BacktestResult:
        # Prepare backtest config
        config = {
            'strategy': {
                'type': request.strategy_id,
                'parameters': request.parameters,
            },
            'data': {
                'symbol': request.symbol,
                'exchange': request.exchange,
                'start': request.start_date,
                'end': request.end_date,
                'source': f'{self.data_dir}/{request.exchange}',
            },
            'simulation': {
                'initial_capital': request.initial_capital,
                'commission': 0.001,  # 0.1%
                'slippage': 0.0005,  # 0.05%
            },
        }

        # Run C++ backtest engine
        result = subprocess.run(
            [self.engine_path, '--backtest', '--config', json.dumps(config)],
            capture_output=True,
            text=True,
        )

        if result.returncode != 0:
            raise RuntimeError(f"Backtest failed: {result.stderr}")

        # Parse results
        output = json.loads(result.stdout)
        return BacktestResult(**output)

    async def compare_strategies(
        self,
        requests: List[BacktestRequest]
    ) -> List[BacktestResult]:
        """Run multiple backtests for comparison"""
        import asyncio
        return await asyncio.gather(*[
            self.run_backtest(req) for req in requests
        ])
```

---

## 7. Performance Tracking

### 7.1 Live Performance Dashboard

```
+------------------------------------------------------------------+
|  Active Strategies                                                |
+------------------------------------------------------------------+
|                                                                   |
|  +-------------------------------------------------------------+ |
|  | Grid Trading (BTCUSDT)                      [Pause] [Stop]  | |
|  +-------------------------------------------------------------+ |
|  | Status: Running | Uptime: 5d 12h 34m                        | |
|  |                                                              | |
|  | +------------+  +------------+  +------------+  +----------+ | |
|  | | Today P&L  |  | Total P&L  |  | Win Rate   |  | Trades   | | |
|  | |  +$127.45  |  |  +$892.30  |  |   67.5%    |  |   234    | | |
|  | +------------+  +------------+  +------------+  +----------+ | |
|  |                                                              | |
|  | Open Positions: 3 buy, 2 sell | Grid Utilization: 50%       | |
|  +-------------------------------------------------------------+ |
|                                                                   |
|  +-------------------------------------------------------------+ |
|  | DCA Strategy (ETHUSDT)                      [Pause] [Stop]  | |
|  +-------------------------------------------------------------+ |
|  | Status: Running | Uptime: 12d 8h 15m                        | |
|  |                                                              | |
|  | +------------+  +------------+  +------------+  +----------+ | |
|  | | Today P&L  |  | Total P&L  |  | Avg Entry  |  | Position | | |
|  | |  +$45.20   |  |  +$234.50  |  |  $2,450    |  | 0.5 ETH  | | |
|  | +------------+  +------------+  +------------+  +----------+ | |
|  +-------------------------------------------------------------+ |
|                                                                   |
+------------------------------------------------------------------+
```

### 7.2 Performance Metrics Collection

```typescript
// src/types/performance.ts
export interface StrategyPerformance {
  strategyId: string;
  strategyName: string;
  symbol: string;
  status: 'running' | 'paused' | 'stopped' | 'error';
  startTime: string;
  uptime: number;  // seconds

  // P&L metrics
  todayPnl: number;
  totalPnl: number;
  unrealizedPnl: number;
  realizedPnl: number;

  // Trade metrics
  totalTrades: number;
  winningTrades: number;
  losingTrades: number;
  winRate: number;

  // Position info
  openPositions: number;
  positionValue: number;

  // Risk metrics
  maxDrawdown: number;
  currentDrawdown: number;
  sharpeRatio: number;

  // Strategy-specific metrics
  customMetrics: Record<string, number>;
}
```

### 7.3 Performance API

```
GET /api/strategies/performance
Response: { strategies: StrategyPerformance[] }

GET /api/strategies/:id/performance
Response: { performance: StrategyPerformance }

GET /api/strategies/:id/performance/history
Query: { from: string, to: string, interval: string }
Response: { history: PerformanceDataPoint[] }

GET /api/strategies/:id/trades
Query: { from: string, to: string, limit: number }
Response: { trades: Trade[] }
```

---

## 8. Strategy Repository

### 8.1 Built-in Strategies

| Strategy | Category | Risk | Description |
|----------|----------|------|-------------|
| Grid Trading | Market Making | Medium | Profit from price oscillations |
| DCA | DCA | Low | Dollar-cost averaging |
| Momentum | Trend Following | High | Follow market momentum |
| Mean Reversion | Mean Reversion | Medium | Trade price reversals |
| TWAP | Execution | Low | Time-weighted execution |
| Breakout | Trend Following | High | Trade breakouts |

### 8.2 Strategy Storage Structure

```
strategies/
+-- templates/
|   +-- grid_trading.yaml
|   +-- dca.yaml
|   +-- momentum.yaml
|   +-- mean_reversion.yaml
|   +-- twap.yaml
|   +-- breakout.yaml
+-- instances/
|   +-- user_grid_btcusdt_001.json
|   +-- user_dca_ethusdt_001.json
+-- benchmarks/
|   +-- grid_trading_benchmarks.json
|   +-- dca_benchmarks.json
+-- index.json
```

### 8.3 Strategy Repository API

```python
# apps/gateway/strategy_repository.py
from pathlib import Path
from typing import List, Optional
import yaml
import json

class StrategyRepository:
    def __init__(self, base_path: Path):
        self.base_path = base_path
        self.templates_path = base_path / 'templates'
        self.instances_path = base_path / 'instances'
        self._index = self._load_index()

    def _load_index(self) -> dict:
        index_path = self.base_path / 'index.json'
        if index_path.exists():
            return json.loads(index_path.read_text())
        return {'templates': [], 'instances': []}

    def list_templates(
        self,
        category: Optional[str] = None,
        risk_level: Optional[str] = None,
    ) -> List[dict]:
        templates = []
        for template_file in self.templates_path.glob('*.yaml'):
            template = yaml.safe_load(template_file.read_text())
            if category and template['category'] != category:
                continue
            if risk_level and template['risk_level'] != risk_level:
                continue
            templates.append(template)
        return templates

    def get_template(self, template_id: str) -> Optional[dict]:
        template_file = self.templates_path / f'{template_id}.yaml'
        if template_file.exists():
            return yaml.safe_load(template_file.read_text())
        return None

    def create_instance(
        self,
        template_id: str,
        symbol: str,
        exchange: str,
        parameters: dict,
    ) -> str:
        template = self.get_template(template_id)
        if not template:
            raise ValueError(f"Template not found: {template_id}")

        # Validate parameters
        self._validate_parameters(template, parameters)

        # Generate instance ID
        instance_id = f"user_{template_id}_{symbol.lower()}_{self._next_id()}"

        # Create instance config
        instance = {
            'id': instance_id,
            'template_id': template_id,
            'symbol': symbol,
            'exchange': exchange,
            'parameters': parameters,
            'created_at': datetime.utcnow().isoformat(),
            'status': 'created',
        }

        # Save instance
        instance_file = self.instances_path / f'{instance_id}.json'
        instance_file.write_text(json.dumps(instance, indent=2))

        return instance_id

    def _validate_parameters(self, template: dict, parameters: dict):
        for param_def in template['parameters']:
            name = param_def['name']
            value = parameters.get(name)

            if value is None:
                if param_def.get('optional'):
                    continue
                raise ValueError(f"Missing required parameter: {name}")

            # Type validation
            expected_type = param_def['type']
            if expected_type == 'integer' and not isinstance(value, int):
                raise ValueError(f"Parameter {name} must be integer")
            if expected_type == 'decimal' and not isinstance(value, (int, float)):
                raise ValueError(f"Parameter {name} must be decimal")

            # Range validation
            if 'min' in param_def and value < param_def['min']:
                raise ValueError(f"Parameter {name} below minimum: {param_def['min']}")
            if 'max' in param_def and value > param_def['max']:
                raise ValueError(f"Parameter {name} above maximum: {param_def['max']}")
```

---

## 9. Security Considerations

### 9.1 Strategy Execution Safety

| Risk | Mitigation |
|------|------------|
| Runaway strategy | Circuit breaker integration |
| Parameter manipulation | Server-side validation |
| Unauthorized deployment | RBAC enforcement |
| Resource exhaustion | Rate limiting, position limits |

### 9.2 Backtest Data Security

| Risk | Mitigation |
|------|------------|
| Data tampering | Checksum verification |
| Unauthorized access | Authentication required |
| Resource abuse | Backtest queue limits |

---

## 10. Implementation Plan

### 10.1 Phase Breakdown

| Phase | Duration | Deliverables |
|-------|----------|--------------|
| **Phase 1: Templates** | 1 week | Template format, repository |
| **Phase 2: Browser UI** | 1.5 weeks | Strategy browser, detail view |
| **Phase 3: Parameters** | 1 week | Parameter form, validation |
| **Phase 4: Backtest** | 1.5 weeks | Backtest integration, results UI |
| **Phase 5: Deployment** | 1 week | Strategy deployment, monitoring |
| **Phase 6: Testing** | 1 week | Integration tests, E2E tests |

### 10.2 Dependencies

```
                    Implementation Dependencies
                    ===========================

+------------------+
| Template Format  |
+------------------+
        |
        v
+------------------+     +------------------+
| Strategy Repo    |---->| Browser UI       |
+------------------+     +------------------+
        |                        |
        v                        v
+------------------+     +------------------+
| Parameter System |---->| Parameter Form   |
+------------------+     +------------------+
        |                        |
        +------------------------+
                    |
                    v
            +------------------+
            | Backtest Runner  |
            +------------------+
                    |
                    v
            +------------------+
            | Deployment       |
            +------------------+
```

---

## 11. API Contracts

### 11.1 Strategy Templates API

```
GET /api/marketplace/templates
Query: { category?: string, riskLevel?: string, search?: string }
Response: { templates: StrategyTemplate[] }

GET /api/marketplace/templates/:id
Response: { template: StrategyTemplate }

GET /api/marketplace/templates/:id/benchmarks
Response: { benchmarks: Record<string, BenchmarkResult> }
```

### 11.2 Strategy Instances API

```
POST /api/strategies
Body: {
  templateId: string,
  symbol: string,
  exchange: string,
  parameters: Record<string, unknown>,
  name?: string
}
Response: { strategyId: string }

GET /api/strategies
Response: { strategies: StrategyInstance[] }

GET /api/strategies/:id
Response: { strategy: StrategyInstance }

PUT /api/strategies/:id/parameters
Body: { parameters: Record<string, unknown> }
Response: { success: boolean }

POST /api/strategies/:id/start
Response: { success: boolean }

POST /api/strategies/:id/stop
Response: { success: boolean }

POST /api/strategies/:id/pause
Response: { success: boolean }

DELETE /api/strategies/:id
Response: { success: boolean }
```

### 11.3 Backtest API

```
POST /api/backtest
Body: {
  templateId: string,
  symbol: string,
  exchange: string,
  parameters: Record<string, unknown>,
  startDate: string,
  endDate: string,
  initialCapital: number
}
Response: { jobId: string }

GET /api/backtest/:jobId
Response: { status: string, progress?: number, result?: BacktestResult }

GET /api/backtest/:jobId/trades
Response: { trades: Trade[] }
```

---

## 12. Success Metrics

| Metric | Target | Measurement |
|--------|--------|-------------|
| Strategy deployment rate | > 60% of users | Analytics |
| Backtest completion rate | > 95% | Telemetry |
| Strategy profitability | > 50% profitable | Performance data |
| User retention (strategy users) | > 70% | Analytics |
| Support tickets (strategy) | < 10% of deployments | Support system |

---

## 13. Related Documentation

- [Real-time Charting System](04-charting-system.md)
- [Security Education System](05-security-education.md)
- [Strategy Development Guide](../../tutorials/custom-strategy-development.md)
- [Backtest API Reference](../../api/backtest_api.md)
