# VeloZ Design System Specification

## 1. Design Principles

### 1.1 Core Principles

1. **Clarity First**: Trading interfaces must be instantly readable. No ambiguity in numbers, states, or actions.

2. **Information Density**: Professional traders need dense information displays. Optimize for data visibility without clutter.

3. **Real-Time Feedback**: Every action must have immediate visual feedback. Users must know the system is responsive.

4. **Error Prevention**: Design to prevent errors before they happen. Confirmations for destructive actions.

5. **Accessibility**: WCAG 2.1 AA compliance. Color is not the only indicator of state.

---

## 2. Technology Stack

### 2.1 Styling Approach

| Technology | Purpose |
|------------|---------|
| Tailwind CSS 4.x | Utility-first CSS framework |
| Headless UI | Unstyled, accessible UI primitives |
| Lucide React | Icon library |
| CSS Variables | Theme tokens and customization |

### 2.2 File Structure

```
src/styles/
├── globals.css      # Tailwind imports and @theme definitions
└── theme.css        # CSS custom properties for theming
```

---

## 3. Color System

### 3.1 Theme Configuration (globals.css)

The design system uses Tailwind CSS v4's `@theme` directive for defining custom colors:

```css
@import "tailwindcss";

@theme {
  /* Primary colors */
  --color-primary: #111827;
  --color-primary-50: #f9fafb;
  --color-primary-100: #f3f4f6;
  --color-primary-200: #e5e7eb;
  --color-primary-300: #d1d5db;
  --color-primary-400: #9ca3af;
  --color-primary-500: #6b7280;
  --color-primary-600: #4b5563;
  --color-primary-700: #374151;
  --color-primary-800: #1f2937;
  --color-primary-900: #111827;

  /* Success colors */
  --color-success: #059669;
  --color-success-50: #ecfdf5;
  --color-success-100: #d1fae5;
  --color-success-200: #a7f3d0;
  --color-success-500: #10b981;
  --color-success-600: #059669;
  --color-success-700: #047857;
  --color-success-800: #065f46;

  /* Warning colors */
  --color-warning: #d97706;
  --color-warning-50: #fffbeb;
  --color-warning-100: #fef3c7;
  --color-warning-500: #f59e0b;
  --color-warning-600: #d97706;
  --color-warning-700: #b45309;
  --color-warning-800: #92400e;

  /* Danger colors */
  --color-danger: #dc2626;
  --color-danger-50: #fef2f2;
  --color-danger-100: #fee2e2;
  --color-danger-500: #ef4444;
  --color-danger-600: #dc2626;
  --color-danger-700: #b91c1c;
  --color-danger-800: #991b1b;

  /* Info colors */
  --color-info: #2563eb;
  --color-info-50: #eff6ff;
  --color-info-100: #dbeafe;
  --color-info-500: #3b82f6;
  --color-info-600: #2563eb;
  --color-info-700: #1d4ed8;
  --color-info-800: #1e40af;

  /* Background colors */
  --color-background: #ffffff;
  --color-background-secondary: #f9fafb;

  /* Border colors */
  --color-border: #e5e7eb;
  --color-border-light: #f1f5f9;

  /* Text colors */
  --color-text: #111827;
  --color-text-muted: #6b7280;
}
```

### 3.2 Trading-Specific Colors (Implemented)

```css
@theme {
  /* Trading colors (buy/sell) */
  --color-buy: #059669;
  --color-buy-light: #d1fae5;
  --color-buy-dark: #065f46;
  --color-sell: #dc2626;
  --color-sell-light: #fee2e2;
  --color-sell-dark: #991b1b;
}
```

### 3.3 Strategy Type Colors (Implemented)

```css
@theme {
  --color-strategy-momentum: #7c3aed;
  --color-strategy-momentum-bg: #ede9fe;
  --color-strategy-mean-reversion: #2563eb;
  --color-strategy-mean-reversion-bg: #dbeafe;
  --color-strategy-grid: #059669;
  --color-strategy-grid-bg: #d1fae5;
  --color-strategy-market-making: #d97706;
  --color-strategy-market-making-bg: #fef3c7;
  --color-strategy-trend-following: #db2777;
  --color-strategy-trend-following-bg: #fce7f3;
}
```

### 3.4 Order Book Depth Bars

```css
/* Utility classes for order book */
.depth-bar-bid {
  background: rgba(5, 150, 105, 0.15);
}

.depth-bar-ask {
  background: rgba(220, 38, 38, 0.15);
}
```

### 3.3 Status Colors

| Status | Color | Tailwind Class | Usage |
|--------|-------|----------------|-------|
| Running | `#10b981` | `text-success` | Strategy running, connected |
| Stopped | `#6b7280` | `text-text-muted` | Strategy stopped, inactive |
| Paused | `#f59e0b` | `text-warning-500` | Strategy paused |
| Error | `#ef4444` | `text-danger-500` | Strategy error, disconnected |
| Pending | `#3b82f6` | `text-info-500` | Order pending, processing |

---

## 4. Typography

### 4.1 Font Stack

```css
@theme {
  --font-sans: ui-sans-serif, system-ui, -apple-system, "Segoe UI", Roboto, Helvetica, Arial, sans-serif;
  --font-mono: ui-monospace, SFMono-Regular, Menlo, Monaco, Consolas, "Liberation Mono", "Courier New", monospace;
}
```

### 4.2 Type Scale (Tailwind Classes)

| Name | Tailwind Class | Size | Usage |
|------|----------------|------|-------|
| Display Large | `text-4xl font-semibold` | 36px | Page titles |
| Display Medium | `text-3xl font-semibold` | 30px | Section headers |
| Display Small | `text-2xl font-semibold` | 24px | Card titles |
| Heading Large | `text-xl font-semibold` | 20px | Subsection headers |
| Heading Medium | `text-lg font-semibold` | 18px | Component titles |
| Heading Small | `text-base font-semibold` | 16px | Small headers |
| Body Large | `text-base` | 16px | Primary body text |
| Body Medium | `text-sm` | 14px | Default body text |
| Body Small | `text-xs` | 12px | Secondary text |
| Caption | `text-xs text-text-muted` | 12px | Labels, captions |

### 4.3 Monospace for Numbers

Use the `mono` utility class for numeric displays:

```css
.mono {
  font-family: var(--font-mono);
}
```

Usage in components:
```tsx
<span className="mono text-sm font-medium">1,234.56</span>
```

### 4.4 Number Formatting Utilities

```typescript
// src/shared/utils/format.ts

export const formatPrice = (price: number, decimals = 2): string => {
  return price.toLocaleString('en-US', {
    minimumFractionDigits: decimals,
    maximumFractionDigits: decimals,
  });
};

export const formatQuantity = (qty: number, decimals = 4): string => {
  return qty.toLocaleString('en-US', {
    minimumFractionDigits: decimals,
    maximumFractionDigits: decimals,
  });
};

export const formatPnL = (pnl: number, decimals = 2): string => {
  const sign = pnl >= 0 ? '+' : '';
  return `${sign}${formatPrice(pnl, decimals)}`;
};

export const formatPercent = (value: number, decimals = 2): string => {
  const sign = value >= 0 ? '+' : '';
  return `${sign}${value.toFixed(decimals)}%`;
};
```

---

## 5. Spacing System

### 5.1 Base Unit

Base unit: **4px** (Tailwind default)

### 5.2 Spacing Scale

| Token | Tailwind | Value | Usage |
|-------|----------|-------|-------|
| 0 | `p-0` / `m-0` | 0px | No spacing |
| 1 | `p-1` / `m-1` | 4px | Tight spacing |
| 2 | `p-2` / `m-2` | 8px | Default component padding |
| 3 | `p-3` / `m-3` | 12px | Medium spacing |
| 4 | `p-4` / `m-4` | 16px | Standard padding |
| 5 | `p-5` / `m-5` | 20px | Section spacing |
| 6 | `p-6` / `m-6` | 24px | Card padding |
| 8 | `p-8` / `m-8` | 32px | Large section spacing |
| 10 | `p-10` / `m-10` | 40px | Page section spacing |
| 12 | `p-12` / `m-12` | 48px | Major section spacing |

### 5.3 Layout Grid

```css
:root {
  --container-max-width: 1280px; /* max-w-7xl */
  --sidebar-width: 256px; /* w-64 */
  --sidebar-collapsed-width: 80px; /* w-20 */
}
```

---

## 6. Component Specifications

### 6.1 Button Component

**Variants:**

| Variant | Classes | Usage |
|---------|---------|-------|
| Primary | `bg-primary text-white hover:opacity-90` | Primary actions |
| Secondary | `bg-background-secondary text-text border border-border hover:bg-gray-100` | Secondary actions |
| Success | `bg-success text-white hover:opacity-90` | Start, confirm, buy |
| Danger | `bg-danger text-white hover:opacity-90` | Stop, delete, sell |
| Warning | `bg-warning text-white hover:opacity-90` | Pause, caution |

**Sizes:**

| Size | Classes |
|------|---------|
| Small | `px-3 py-1.5 text-xs` |
| Medium | `px-4 py-2.5 text-sm` |
| Large | `px-6 py-3 text-base` |

**Base Classes:**
```
inline-flex items-center justify-center rounded-md font-medium
transition-opacity duration-150 focus:outline-none focus:ring-2
focus:ring-offset-2 disabled:opacity-50 disabled:cursor-not-allowed
```

### 6.2 Card Component

```tsx
<div className="bg-background border border-border rounded-lg shadow-card">
  {/* Header */}
  <div className="flex items-center justify-between px-4 py-3 border-b border-border">
    <div>
      <h3 className="text-lg font-semibold text-text">{title}</h3>
      <p className="text-sm text-text-muted mt-0.5">{subtitle}</p>
    </div>
    {headerAction}
  </div>

  {/* Content */}
  <div className="p-4">{children}</div>

  {/* Footer */}
  <div className="px-4 py-3 border-t border-border bg-background-secondary rounded-b-lg">
    {footer}
  </div>
</div>
```

### 6.3 Input Component

```tsx
<div className="w-full">
  <label className="block text-sm font-medium text-text mb-1">
    {label}
  </label>
  <div className="relative">
    <input
      className={`
        px-3 py-2.5 border rounded-md text-sm transition-colors duration-150
        focus:outline-none focus:ring-2 focus:border-transparent
        disabled:bg-gray-100 disabled:cursor-not-allowed
        ${error
          ? 'border-danger focus:ring-danger bg-danger-50'
          : 'border-gray-300 focus:ring-primary'
        }
      `}
    />
  </div>
  {error && <p className="mt-1 text-sm text-danger">{error}</p>}
</div>
```

### 6.4 Status Badge

```tsx
const statusStyles = {
  running: 'bg-success-50 text-success border-success-200',
  stopped: 'bg-gray-100 text-text-muted border-gray-200',
  paused: 'bg-warning-50 text-warning-600 border-warning-200',
  error: 'bg-danger-50 text-danger border-danger-200',
  pending: 'bg-info-50 text-info-600 border-info-200',
};

<span className={`
  inline-flex items-center px-2.5 py-0.5 rounded-full text-xs font-medium border
  ${statusStyles[status]}
`}>
  <span className="w-1.5 h-1.5 rounded-full bg-current mr-1.5" />
  {label}
</span>
```

---

## 7. Trading-Specific Components

### 7.1 Order Book

```tsx
<div className="font-mono text-xs">
  {/* Header */}
  <div className="grid grid-cols-3 px-3 py-2 text-text-muted uppercase text-[10px] border-b border-border">
    <span>Price</span>
    <span className="text-right">Size</span>
    <span className="text-right">Total</span>
  </div>

  {/* Ask rows */}
  {asks.map((ask) => (
    <div className="grid grid-cols-3 px-3 py-1 relative hover:bg-background-secondary">
      <span className="text-sell">{formatPrice(ask.price)}</span>
      <span className="text-right">{formatQuantity(ask.size)}</span>
      <span className="text-right text-text-muted">{formatQuantity(ask.total)}</span>
      {/* Depth bar */}
      <div
        className="absolute inset-y-0 right-0 bg-orderbook-ask"
        style={{ width: `${ask.depthPercent}%` }}
      />
    </div>
  ))}

  {/* Spread */}
  <div className="flex justify-center items-center py-2 bg-background-secondary text-sm text-text-muted">
    Spread: {spread} ({spreadPercent}%)
  </div>

  {/* Bid rows */}
  {bids.map((bid) => (
    <div className="grid grid-cols-3 px-3 py-1 relative hover:bg-background-secondary">
      <span className="text-buy">{formatPrice(bid.price)}</span>
      <span className="text-right">{formatQuantity(bid.size)}</span>
      <span className="text-right text-text-muted">{formatQuantity(bid.total)}</span>
      {/* Depth bar */}
      <div
        className="absolute inset-y-0 right-0 bg-orderbook-bid"
        style={{ width: `${bid.depthPercent}%` }}
      />
    </div>
  ))}
</div>
```

### 7.2 PnL Display

```tsx
const PnLValue = ({ value, size = 'md' }: { value: number; size?: 'sm' | 'md' | 'lg' }) => {
  const sizeClasses = {
    sm: 'text-sm',
    md: 'text-lg',
    lg: 'text-2xl',
  };

  return (
    <span className={`
      mono font-semibold
      ${sizeClasses[size]}
      ${value >= 0 ? 'text-profit' : 'text-loss'}
    `}>
      {formatPnL(value)}
    </span>
  );
};
```

### 7.3 Strategy Card

```tsx
<div className="bg-background border border-border rounded-lg p-4 hover:shadow-md transition-shadow">
  {/* Header */}
  <div className="flex justify-between items-start mb-3">
    <div>
      <h3 className="text-base font-semibold">{strategy.name}</h3>
      <p className="text-sm text-text-muted">{strategy.type}</p>
    </div>
    <StatusBadge status={strategy.status} />
  </div>

  {/* Metrics */}
  <div className="grid grid-cols-2 gap-3 p-3 bg-background-secondary rounded-md mb-4">
    <div>
      <div className="text-[10px] uppercase text-text-muted">PnL</div>
      <PnLValue value={strategy.pnl} size="sm" />
    </div>
    <div>
      <div className="text-[10px] uppercase text-text-muted">Win Rate</div>
      <span className="mono text-sm font-medium">{strategy.winRate}%</span>
    </div>
  </div>

  {/* Actions */}
  <div className="flex gap-2">
    <Button variant="success" size="sm" className="flex-1">Start</Button>
    <Button variant="secondary" size="sm" className="flex-1">Configure</Button>
  </div>
</div>
```

---

## 8. Shadows

Defined in the theme:

```css
@theme {
  --shadow-sm: 0 1px 2px rgba(0, 0, 0, 0.05);
  --shadow-card: 0 1px 3px 0 rgb(0 0 0 / 0.1), 0 1px 2px -1px rgb(0 0 0 / 0.1);
  --shadow-md: 0 4px 12px rgba(0, 0, 0, 0.1);
  --shadow-modal: 0 4px 12px rgba(0, 0, 0, 0.15);
  --shadow-toast: 0 4px 12px rgba(0, 0, 0, 0.15);
  --shadow-focus-primary: 0 0 0 2px rgba(17, 24, 39, 0.2);
  --shadow-focus-danger: 0 0 0 2px rgba(220, 38, 38, 0.2);
  --shadow-focus-info: 0 0 0 2px rgba(37, 99, 235, 0.2);
}
```

Usage: `shadow-sm`, `shadow-card`, `shadow-md`, `shadow-modal`, `shadow-toast`

---

## 8.1 Z-Index Layers

```css
@theme {
  --z-dropdown: 100;
  --z-sticky: 200;
  --z-modal-backdrop: 1000;
  --z-modal: 1100;
  --z-toast: 1200;
  --z-tooltip: 1300;
}
```

---

## 9. Animations

### 9.1 Animation Tokens

```css
@theme {
  --animate-spin: spin 0.8s linear infinite;
  --animate-spin-fast: spin 0.6s linear infinite;
  --animate-pulse: pulse 2s ease-in-out infinite;
  --animate-slide-in: slideIn 0.3s ease;
  --animate-fade-in: fadeIn 0.2s ease;
  --animate-flash-new: flash-new 0.3s ease;
  --animate-flash-update: flash-update 0.2s ease;
}
```

### 9.2 Keyframe Definitions

```css
@keyframes spin {
  to { transform: rotate(360deg); }
}

@keyframes pulse {
  0%, 100% { opacity: 1; }
  50% { opacity: 0.5; }
}

@keyframes slideIn {
  from {
    transform: translateX(100%);
    opacity: 0;
  }
  to {
    transform: translateX(0);
    opacity: 1;
  }
}

@keyframes fadeIn {
  from { opacity: 0; }
  to { opacity: 1; }
}

@keyframes flash-new {
  0% { background-color: rgba(37, 99, 235, 0.3); }
  100% { background-color: transparent; }
}

@keyframes flash-update {
  0% { background-color: rgba(234, 179, 8, 0.2); }
  100% { background-color: transparent; }
}
```

### 9.2 Reduced Motion Support

```css
@media (prefers-reduced-motion: reduce) {
  * {
    animation-duration: 0.01ms !important;
    transition-duration: 0.01ms !important;
  }
}
```

---

## 10. Responsive Breakpoints

Tailwind default breakpoints:

| Breakpoint | Min Width | Usage |
|------------|-----------|-------|
| `sm` | 640px | Mobile landscape |
| `md` | 768px | Tablet |
| `lg` | 1024px | Desktop |
| `xl` | 1280px | Large desktop |
| `2xl` | 1536px | Extra large |

### 10.1 Responsive Patterns

```tsx
// Stack on mobile, row on desktop
<div className="flex flex-col md:flex-row gap-4">

// Hide on mobile
<div className="hidden md:block">

// Different grid columns
<div className="grid grid-cols-1 md:grid-cols-2 lg:grid-cols-4 gap-4">
```

---

## 11. Icon Library

Using **Lucide React** for icons.

### 11.1 Icon Sizes

| Size | Class | Usage |
|------|-------|-------|
| Small | `w-3.5 h-3.5` | Inline with text |
| Medium | `w-4 h-4` | Buttons, inputs |
| Large | `w-5 h-5` | Headers, emphasis |
| XLarge | `w-6 h-6` | Empty states, alerts |

### 11.2 Trading Icons

| Concept | Lucide Icon | Usage |
|---------|-------------|-------|
| Buy/Long | `ArrowUp` | Buy orders, long positions |
| Sell/Short | `ArrowDown` | Sell orders, short positions |
| Profit | `TrendingUp` | Positive PnL |
| Loss | `TrendingDown` | Negative PnL |
| Running | `Play` | Strategy running |
| Stopped | `Square` | Strategy stopped |
| Paused | `Pause` | Strategy paused |
| Error | `AlertCircle` | Error state |
| Settings | `Settings` | Configuration |
| Chart | `LineChart` | Charts, analytics |
| Order | `FileText` | Orders |
| Position | `Wallet` | Positions |
| Strategy | `Bot` | Strategies |
| Backtest | `FlaskConical` | Backtesting |

---

## 12. Dark Mode (Future)

Dark mode support is prepared in `theme.css`:

```css
@media (prefers-color-scheme: dark) {
  :root.dark-mode {
    --color-bg: #111827;
    --color-bg-secondary: #1f2937;
    --color-border: #374151;
    --color-border-light: #4b5563;
    --color-text: #f9fafb;
    --color-text-muted: #9ca3af;
  }
}
```

Implementation requires:
1. Theme toggle component
2. Theme context/store
3. Persistence in localStorage

---

## 13. Implementation Checklist

- [x] Configure Tailwind CSS 4.x with custom theme
- [x] Create CSS custom properties file
- [x] Create core UI components (Button, Card, Input, Select, Modal, Toast)
- [x] Create layout components (Header, Sidebar, MainLayout)
- [x] Implement typography utility classes
- [x] Add trading-specific colors (buy/sell, profit/loss)
- [x] Add strategy type colors
- [x] Add order book depth bar styles
- [x] Add z-index layer system
- [x] Add animation keyframes (spin, pulse, slideIn, fadeIn, flash)
- [ ] Implement dark mode toggle
- [ ] Create trading-specific components (OrderBook, PnLDisplay)
- [ ] Test color contrast for accessibility
- [ ] Verify animations respect reduced motion
- [ ] Document component usage examples
