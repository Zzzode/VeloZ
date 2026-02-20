# UI Testing Plan

This document outlines the testing approach for the VeloZ UI.

## Current Status: Implemented

Unit testing infrastructure is now set up and operational.

## Unit Tests (Vitest)

**Framework:** Vitest v1.6.1 with jsdom environment

**Run Tests:**
```bash
cd apps/ui
npm install    # First time only
npm test       # Run all tests
npm run test:watch  # Watch mode for development
```

**Current Coverage:**
- `js/utils.js` - 36 tests covering:
  - HTML escaping (XSS prevention)
  - Parameter name formatting
  - Time/uptime formatting
  - Price/quantity formatting
  - PnL formatting with sign and CSS class
  - Percentage formatting
  - Spread calculation
  - Order parameter validation
  - API URL normalization
  - Client order ID generation

- `js/strategies.js` - 37 tests covering:
  - Strategy API client functions (fetch, create, update, delete)
  - Parameter validation (int, float, string, bool, enum types)
  - Parameter type conversion
  - Strategy card HTML rendering
  - SSE subscription for real-time updates

- `js/orderbook.js` - 32 tests covering:
  - Order book snapshot and delta updates
  - Price level sorting (bids descending, asks ascending)
  - Cumulative depth calculation
  - Best bid/ask, spread, mid price calculations
  - Price level rendering with depth bars
  - Animation flags for new/updated levels

- `js/positions.js` - 45 tests covering:
  - Position data parsing (snake_case and camelCase)
  - Position side determination (long/short/flat)
  - PnL calculations (total, unrealized, realized)
  - Notional value and PnL percentage
  - Position manager update handling
  - PnL history tracking
  - Position table and summary rendering
  - API fetch functions

**Total: 150 tests passing**

## Test File Structure

```
apps/ui/
  index.html           # Main UI (static HTML)
  package.json         # Node.js dependencies
  vitest.config.js     # Vitest configuration
  TESTING_PLAN.md      # This document
  css/
    strategies.css     # Strategy-specific styles
  js/
    utils.js           # Extracted utility functions (testable)
    utils.test.js      # Unit tests (36 tests)
    strategies.js      # Strategy API client module
    strategies.test.js # Strategy module tests (37 tests)
    strategy-api.js    # API integration layer with SSE
    orderbook.js       # Order book display module
    orderbook.test.js  # Order book tests (32 tests)
    positions.js       # Position and PnL display module
    positions.test.js  # Position module tests (45 tests)
```

## Adding New Tests

1. Extract testable functions into `js/` modules
2. Create corresponding `.test.js` files
3. Use ES6 imports: `import { fn } from './module.js'`
4. Run `npm test` to verify

**Example:**
```javascript
// js/api.js
export async function fetchMarket(apiBase) {
  const res = await fetch(`${apiBase}/api/market`);
  if (!res.ok) throw new Error(`HTTP ${res.status}`);
  return res.json();
}

// js/api.test.js
import { describe, it, expect, vi } from 'vitest';
import { fetchMarket } from './api.js';

describe('fetchMarket', () => {
  it('should fetch market data', async () => {
    global.fetch = vi.fn().mockResolvedValue({
      ok: true,
      json: () => Promise.resolve({ symbol: 'BTCUSDT', price: 42000 })
    });
    const result = await fetchMarket('http://localhost:8080');
    expect(result.symbol).toBe('BTCUSDT');
  });
});
```

## Future: E2E Tests (Playwright)

When Phase 7 implementation progresses, add Playwright for end-to-end testing:

```bash
npm install -D @playwright/test
npx playwright install
```

**Planned E2E Tests:**
- Order book display updates with mock SSE
- Strategy configuration form submission
- Position/PnL display with live data
- Tab navigation and modal interactions

## CI Integration

Add to `.github/workflows/ci.yml`:
```yaml
  ui-tests:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - uses: actions/setup-node@v4
        with:
          node-version: '20'
      - run: cd apps/ui && npm ci
      - run: cd apps/ui && npm test
```

## Notes

- Testing infrastructure set up on 2026-02-20 (Task #50)
- Strategy Configuration UI module added on 2026-02-20 (Task #32)
- Order Book Display module added on 2026-02-20 (Task #30)
- Position and PnL Display module added on 2026-02-20 (Task #31)
- Vitest chosen for ESM-native support and fast execution
- jsdom environment enables DOM testing without browser
- All 150 tests passing (36 utils + 37 strategies + 32 orderbook + 45 positions)

## Strategy API Integration

The Strategy Configuration UI (Task #32) includes:

1. **Core Module** (`js/strategies.js`):
   - API client functions for CRUD operations
   - Parameter validation with type checking
   - Dynamic form generation
   - Strategy card rendering

2. **API Integration** (`js/strategy-api.js`):
   - REST API client with error handling
   - SSE subscription with reconnection
   - Connection state tracking
   - Fallback to mock data when API unavailable

3. **Styling** (`css/strategies.css`):
   - Parameter error states
   - Hot/cold parameter badges
   - Status badges (running, stopped, paused, error)
   - Toast notifications and confirmation dialogs

**Enable API Mode:**
Add `?api_mode=true` to the URL to connect to the real backend API.

## Order Book Display Integration

The Order Book Display (Task #30) includes:

1. **Core Module** (`js/orderbook.js`):
   - OrderBook class for state management
   - Snapshot and delta update handling
   - Price level sorting (bids descending, asks ascending)
   - Cumulative depth calculation
   - Update throttling for performance
   - SSE subscription for real-time updates

2. **UI Integration** (`index.html`):
   - Order Book tab with widget and stats panel
   - Symbol selection dropdown
   - Price level grouping control
   - Imbalance indicator
   - Mock data fallback when API unavailable

3. **Styling** (`css/orderbook.css`):
   - Depth bar visualization
   - Bid/ask color coding
   - Animation for new/updated levels
   - Spread display

**Features:**
- Real-time updates via SSE (when API mode enabled)
- Mock data simulation for demo
- Price level grouping (0.01, 0.1, 1, 10, 100)
- Cumulative depth display
- Best bid/ask, spread, mid price stats
- Bid/ask imbalance indicator

**Enable API Mode:**
Add `?api_mode=true` to the URL to connect to the real backend API.

## Position and PnL Display Integration

The Position and PnL Display (Task #31) includes:

1. **Core Module** (`js/positions.js`):
   - PositionData class for position state
   - PositionManager for multi-symbol tracking
   - PnL history recording
   - Update throttling for performance
   - SSE subscription for real-time updates

2. **UI Integration** (`index.html`):
   - Positions tab with portfolio summary
   - PnL summary cards (total, unrealized, realized, notional)
   - Positions table with all position details
   - PnL history chart placeholder
   - Mock data fallback when API unavailable

3. **Styling** (`css/positions.css`):
   - Position table styling
   - PnL color coding (positive/negative)
   - Side badges (long/short)
   - Summary cards
   - Connection status indicator

**Features:**
- Real-time updates via SSE (when API mode enabled)
- Mock data simulation for demo
- Multi-symbol position tracking
- Unrealized/realized PnL breakdown
- PnL percentage calculation
- Notional value tracking
- PnL history for charting

**Enable API Mode:**
Add `?api_mode=true` to the URL to connect to the real backend API.

---
*Document updated by ui-dev after Task #31 completion*
