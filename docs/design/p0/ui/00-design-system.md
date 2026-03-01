# VeloZ P0 Design System

This document defines the unified design system for all P0 feature UIs, building upon the existing VeloZ design tokens and component patterns.

## Design Principles

### 1. Simple
- Non-technical users should understand immediately
- Progressive disclosure: show essential information first, details on demand
- Clear visual hierarchy guides users through complex workflows

### 2. Safe
- Visual cues for risky actions (red warnings, confirmation dialogs)
- Destructive actions require explicit confirmation
- Clear distinction between simulated and live trading modes

### 3. Professional
- Clean, minimal aesthetic inspires confidence
- Consistent spacing and alignment
- Financial-grade precision in data display

### 4. Consistent
- Follow existing VeloZ UI patterns
- Reuse established components
- Predictable interaction patterns

### 5. Accessible
- WCAG 2.1 AA compliance
- Keyboard navigation support
- Screen reader compatibility
- Color contrast ratios >= 4.5:1

---

## Color Palette

### Primary Colors
| Token | Hex | Usage |
|-------|-----|-------|
| `primary` | `#111827` | Primary buttons, active states |
| `primary-hover` | `#1f2937` | Button hover state |

### Semantic Colors
| Token | Hex | Usage |
|-------|-----|-------|
| `success` | `#059669` | Buy orders, positive PnL, success states |
| `success-light` | `#d1fae5` | Success badge background |
| `warning` | `#d97706` | Warnings, caution states |
| `warning-light` | `#fef3c7` | Warning badge background |
| `danger` | `#dc2626` | Sell orders, negative PnL, errors, destructive actions |
| `danger-light` | `#fee2e2` | Danger badge background |
| `info` | `#2563eb` | Information, links, highlights |
| `info-light` | `#dbeafe` | Info badge background |

### Background Colors
| Token | Hex | Usage |
|-------|-----|-------|
| `background` | `#ffffff` | Main page background |
| `background-secondary` | `#f9fafb` | Card backgrounds, panels |
| `background-tertiary` | `#f3f4f6` | Subtle backgrounds |

### P0-Specific Colors
| Token | Hex | Usage |
|-------|-----|-------|
| `simulated` | `#8b5cf6` | Simulated trading mode indicator |
| `simulated-light` | `#ede9fe` | Simulated mode backgrounds |
| `live` | `#059669` | Live trading mode indicator |
| `installer-progress` | `#2563eb` | Installation progress |

---

## Typography

### Font Families
```css
--font-sans: ui-sans-serif, system-ui, -apple-system, Segoe UI, Roboto, Helvetica, Arial, sans-serif;
--font-mono: ui-monospace, SFMono-Regular, Menlo, Monaco, Consolas, Liberation Mono, Courier New, monospace;
```

### Font Scale
| Token | Size | Line Height | Usage |
|-------|------|-------------|-------|
| `text-xxs` | 10px | 14px | Stat labels |
| `text-xs` | 11px | 16px | Table headers |
| `text-sm` | 12px | 18px | Badges, small buttons |
| `text-base` | 13px | 20px | Body text |
| `text-md` | 14px | 22px | Form inputs |
| `text-lg` | 16px | 24px | Card titles |
| `text-xl` | 18px | 28px | Section headers |
| `text-2xl` | 20px | 30px | Feature titles |
| `text-3xl` | 24px | 32px | Wizard step titles |
| `text-4xl` | 28px | 36px | Hero metrics |

---

## Spacing Scale

| Token | Value | Usage |
|-------|-------|-------|
| `spacing-1` | 4px | Badge padding |
| `spacing-2` | 8px | Button padding |
| `spacing-3` | 12px | Card padding |
| `spacing-4` | 16px | Section spacing |
| `spacing-6` | 24px | Page padding |
| `spacing-8` | 32px | Section margins |
| `spacing-12` | 48px | Wizard step spacing |

---

## Border Radius

| Token | Value | Usage |
|-------|-------|-------|
| `rounded-sm` | 4px | Small buttons, badges |
| `rounded` | 6px | Inputs, selects |
| `rounded-md` | 8px | Metric panels |
| `rounded-lg` | 10px | Cards |
| `rounded-xl` | 12px | Modals, dialogs |
| `rounded-full` | 9999px | Pills, avatars |

---

## Shadows

| Token | Value | Usage |
|-------|-------|-------|
| `shadow-sm` | `0 1px 2px rgba(0,0,0,0.05)` | Subtle elevation |
| `shadow` | `0 4px 12px rgba(0,0,0,0.08)` | Card hover |
| `shadow-lg` | `0 4px 12px rgba(0,0,0,0.15)` | Modals, toasts |
| `shadow-focus` | `0 0 0 2px rgba(17,24,39,0.2)` | Focus rings |

---

## Component Patterns

### Wizard Steps
```
+--------------------------------------------------+
|  (1)---------(2)---------(3)---------(4)         |
|   o           o           o           o          |
| Welcome    Configure    Connect     Ready        |
|                                                  |
+--------------------------------------------------+
```

### Progress Indicator
```
+--------------------------------------------------+
| Installing VeloZ...                              |
|                                                  |
| [===================>                    ] 65%   |
|                                                  |
| Downloading dependencies...                      |
+--------------------------------------------------+
```

### Confirmation Dialog
```
+--------------------------------------------------+
|  [!] Confirm Action                              |
+--------------------------------------------------+
|                                                  |
|  Are you sure you want to enable live trading?   |
|                                                  |
|  This will use real funds from your exchange     |
|  account.                                        |
|                                                  |
|  +------------------------------------------+    |
|  | [ ] I understand the risks               |    |
|  +------------------------------------------+    |
|                                                  |
|          [Cancel]    [Enable Live Trading]       |
+--------------------------------------------------+
```

### Safety Badge
```
+------------------+
| [Shield] SIMULATED |
+------------------+
```

---

## Responsive Breakpoints

| Breakpoint | Width | Description |
|------------|-------|-------------|
| `xs` | 480px | Mobile landscape |
| `sm` | 640px | Small tablets |
| `md` | 768px | Tablets |
| `lg` | 1024px | Small desktops |
| `xl` | 1200px | Container max-width |
| `2xl` | 1400px | Large desktops |

---

## Animation Tokens

| Animation | Duration | Easing | Usage |
|-----------|----------|--------|-------|
| `fade-in` | 200ms | ease-out | Modal appearance |
| `slide-in` | 300ms | ease-out | Toast notifications |
| `progress` | 300ms | linear | Progress bar updates |
| `pulse` | 2s | ease-in-out | Connection status |
| `spin` | 800ms | linear | Loading spinners |

---

## Accessibility Guidelines

### Color Contrast
- Text on backgrounds: minimum 4.5:1 ratio
- Large text (18px+): minimum 3:1 ratio
- Interactive elements: minimum 3:1 ratio against adjacent colors

### Focus States
- All interactive elements must have visible focus indicators
- Focus order follows logical reading order
- Skip links for keyboard navigation

### Screen Readers
- All images have descriptive alt text
- Form fields have associated labels
- ARIA landmarks for page regions
- Live regions for dynamic content updates

### Motion
- Respect `prefers-reduced-motion` preference
- Provide static alternatives for animations
- No auto-playing animations that cannot be paused

---

## Icon Library

Using Lucide React icons for consistency with existing VeloZ UI.

### Common Icons
| Icon | Usage |
|------|-------|
| `Shield` | Security, protection |
| `AlertTriangle` | Warning |
| `AlertCircle` | Error |
| `CheckCircle` | Success |
| `Info` | Information |
| `Settings` | Configuration |
| `Play` | Start |
| `Square` | Stop |
| `Download` | Install, download |
| `Key` | API keys, authentication |
| `Eye` | Show/reveal |
| `EyeOff` | Hide/conceal |
| `Lock` | Secure, locked |
| `Unlock` | Unsecure, unlocked |
| `TrendingUp` | Positive trend |
| `TrendingDown` | Negative trend |
| `BarChart2` | Charts, analytics |
| `Zap` | Live mode |
| `TestTube` | Simulated mode |
