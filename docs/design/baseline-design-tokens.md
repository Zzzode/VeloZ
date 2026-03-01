# VeloZ Baseline Design Tokens

Design tokens extracted from the existing UI for the React redesign project.

**Source Files Analyzed:**
- `apps/ui/index.html` (CSS custom properties and inline styles)
- `apps/ui/css/strategies.css`
- `apps/ui/css/orderbook.css`
- `apps/ui/css/positions.css`
- `apps/ui/css/backtest.css`

**Date:** 2026-02-23
**Status:** Baseline for development - pending ui-designer validation

---

## Color Palette

### Primary Colors

| Token | Hex | Usage |
|-------|-----|-------|
| `primary` | `#111827` | Primary buttons, active tab borders |
| `primary-hover` | `#1f2937` | Button hover state |

### Semantic Colors

| Token | Hex | Usage |
|-------|-----|-------|
| `success` | `#059669` | Buy orders, positive PnL, running status |
| `success-light` | `#d1fae5` | Success badge background |
| `success-dark` | `#065f46` | Success badge text |
| `warning` | `#d97706` | Warnings, paused status |
| `warning-light` | `#fef3c7` | Warning badge background |
| `warning-dark` | `#92400e` | Warning badge text |
| `danger` | `#dc2626` | Sell orders, negative PnL, errors |
| `danger-light` | `#fee2e2` | Danger badge background |
| `danger-dark` | `#991b1b` | Danger badge text |
| `info` | `#2563eb` | Information, links, highlights |
| `info-light` | `#dbeafe` | Info badge background |
| `info-dark` | `#1d4ed8` | Info badge text |

### Background Colors

| Token | Hex | Usage |
|-------|-----|-------|
| `background` | `#ffffff` | Main page background |
| `background-secondary` | `#f9fafb` | Card backgrounds, table headers |
| `background-tertiary` | `#f3f4f6` | Subtle backgrounds, metrics panels |

### Border Colors

| Token | Hex | Usage |
|-------|-----|-------|
| `border` | `#e5e7eb` | Card borders, dividers |
| `border-light` | `#f1f5f9` | Table row borders |
| `border-input` | `#d1d5db` | Input field borders |

### Text Colors

| Token | Hex | Usage |
|-------|-----|-------|
| `text` | `#111827` | Primary text |
| `text-muted` | `#6b7280` | Secondary text, labels |
| `text-light` | `#9ca3af` | Placeholder text |

### Strategy Type Colors

| Strategy Type | Color | Background |
|--------------|-------|------------|
| Momentum | `#7c3aed` | `#ede9fe` |
| Mean Reversion | `#2563eb` | `#dbeafe` |
| Grid | `#059669` | `#d1fae5` |
| Market Making | `#d97706` | `#fef3c7` |
| Trend Following | `#db2777` | `#fce7f3` |

---

## Typography

### Font Families

| Token | Value | Usage |
|-------|-------|-------|
| `font-sans` | System UI stack | Body text, UI elements |
| `font-mono` | Monospace stack | Prices, quantities, IDs |

**System Font Stack:**
```
ui-sans-serif, system-ui, -apple-system, Segoe UI, Roboto, Helvetica, Arial, sans-serif
```

**Monospace Stack:**
```
ui-monospace, SFMono-Regular, Menlo, Monaco, Consolas, Liberation Mono, Courier New, monospace
```

### Font Sizes

| Token | Size | Line Height | Usage |
|-------|------|-------------|-------|
| `text-xxs` | 10px | 14px | Stat labels, uppercase labels |
| `text-xs` | 11px | 16px | Table column headers |
| `text-sm` | 12px | 18px | Badges, small buttons |
| `text-base` | 13px | 20px | Table cells, body text |
| `text-md` | 14px | 22px | Form inputs, buttons |
| `text-lg` | 16px | 24px | Card titles |
| `text-xl` | 18px | 28px | Section headers |
| `text-2xl` | 20px | 30px | PnL summary values |
| `text-3xl` | 24px | 32px | Large stats |
| `text-4xl` | 28px | 36px | Hero metric values |

### Font Weights

| Token | Value | Usage |
|-------|-------|-------|
| `font-normal` | 400 | Body text |
| `font-medium` | 500 | Labels, nav items |
| `font-semibold` | 600 | Headings, table headers |
| `font-bold` | 700 | Large metrics |

---

## Spacing Scale

| Token | Value | Common Usage |
|-------|-------|--------------|
| `0.5` | 2px | Micro spacing |
| `1` | 4px | Badge padding, small gaps |
| `1.5` | 6px | Table cell padding |
| `2` | 8px | Button padding, small gaps |
| `2.5` | 10px | Input padding |
| `3` | 12px | Card padding, medium gaps |
| `4` | 16px | Section spacing |
| `5` | 20px | Large gaps |
| `6` | 24px | Page padding |
| `8` | 32px | Section margins |

---

## Border Radius

| Token | Value | Usage |
|-------|-------|-------|
| `rounded-sm` | 4px | Small buttons, badges |
| `rounded` | 6px | Inputs, selects |
| `rounded-md` | 8px | Metric panels |
| `rounded-lg` | 10px | Cards, main containers |
| `rounded-xl` | 12px | Modals, dialogs |
| `rounded-full` | 9999px | Pills, status dots |

---

## Shadows

| Token | Value | Usage |
|-------|-------|-------|
| `shadow-sm` | `0 1px 2px rgba(0,0,0,0.05)` | Subtle elevation |
| `shadow` | `0 4px 12px rgba(0,0,0,0.08)` | Card hover |
| `shadow-lg` | `0 4px 12px rgba(0,0,0,0.15)` | Toasts, modals |
| `shadow-focus-*` | `0 0 0 2px rgba(...)` | Focus rings |

---

## Breakpoints

| Token | Value | Description |
|-------|-------|-------------|
| `xs` | 480px | Mobile landscape |
| `sm` | 640px | Small tablets |
| `md` | 768px | Tablets |
| `lg` | 1024px | Small desktops |
| `xl` | 1200px | Container max-width |
| `2xl` | 1400px | Large desktops |

---

## Animations

| Animation | Duration | Usage |
|-----------|----------|-------|
| `spin` | 0.8s | Loading spinners |
| `pulse` | 2s | Connection status |
| `flash-new` | 0.3s | New order book levels |
| `flash-update` | 0.2s | Updated values |
| `slide-in` | 0.3s | Toast notifications |

---

## Component Patterns

### Buttons

```css
/* Primary Button */
padding: 10px 12px;
border-radius: 8px;
font-size: 14px;
background: #111827;
color: white;
transition: opacity 0.15s;

/* Small Button */
padding: 6px 10px;
font-size: 12px;
```

### Cards

```css
border: 1px solid #e5e7eb;
border-radius: 10px;
padding: 16px;
background: #ffffff;
```

### Inputs

```css
padding: 10px 12px;
border-radius: 8px;
border: 1px solid #d1d5db;
font-size: 14px;
```

### Badges

```css
display: inline-flex;
padding: 4px 10px;
border-radius: 9999px;
font-size: 12px;
font-weight: 500;
```

### Tables

```css
/* Header */
font-size: 12px;
font-weight: 600;
text-transform: uppercase;
letter-spacing: 0.05em;
color: #6b7280;

/* Cell */
padding: 8px 6px;
font-size: 13px;
border-bottom: 1px solid #f1f5f9;
```

---

## Assumptions for UI-Designer Validation

1. **Theme**: Light theme as default (dark mode not currently implemented)
2. **Primary Color**: Using dark gray (#111827) as primary - consider if a brand color should replace this
3. **Font Sizes**: Slightly smaller than typical (13px base) - validate for readability
4. **Border Radius**: 10px for cards is relatively large - confirm this is intentional
5. **Spacing**: 16px as standard card padding - validate consistency
6. **Status Colors**: Green/Red for buy/sell is standard for trading - confirm
7. **Strategy Colors**: Purple/Blue/Green/Amber/Pink palette - validate distinctiveness

---

## Files

- **Tailwind Config**: `/docs/design/baseline-tailwind-config.js`
- **Architecture Doc**: `/docs/design/ui-architecture.md`

Copy the Tailwind config to `apps/ui/tailwind.config.js` when starting the React project.
