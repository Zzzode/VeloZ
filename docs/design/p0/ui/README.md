# VeloZ P0 UI Design Documentation

This directory contains comprehensive UI design specifications for all P0 features of the VeloZ trading platform.

## Design Documents

| Document | Description |
|----------|-------------|
| [00-design-system.md](./00-design-system.md) | Unified design system, tokens, and component patterns |
| [01-installer-ui.md](./01-installer-ui.md) | One-click installer wizard screens |
| [02-configuration-ui.md](./02-configuration-ui.md) | GUI configuration and settings dashboard |
| [03-strategy-marketplace-ui.md](./03-strategy-marketplace-ui.md) | Strategy browsing, configuration, and deployment |
| [04-realtime-charts-ui.md](./04-realtime-charts-ui.md) | Professional charting with indicators and order entry |
| [05-security-education-ui.md](./05-security-education-ui.md) | Security onboarding and risk management UI |

## Design Principles

All P0 UI designs follow these core principles:

1. **Simple** - Non-technical users should understand immediately
2. **Safe** - Visual cues for risky actions, clear mode indicators
3. **Professional** - Clean aesthetic that inspires confidence
4. **Consistent** - Follows existing VeloZ UI patterns
5. **Accessible** - WCAG 2.1 AA compliance

## Technology Stack

The UI is built with:
- React 19.2 + TypeScript 5.9
- Vite 7.3 for build tooling
- Tailwind CSS 4.2 for styling
- Zustand for state management
- TanStack Query for data fetching
- Lucide React for icons
- Headless UI for accessible components

## Color Palette Summary

| Color | Hex | Usage |
|-------|-----|-------|
| Primary | `#111827` | Buttons, active states |
| Success | `#059669` | Buy orders, positive values, live mode |
| Warning | `#d97706` | Caution states, warnings |
| Danger | `#dc2626` | Sell orders, negative values, errors |
| Info | `#2563eb` | Information, links |
| Simulated | `#8b5cf6` | Simulated trading mode |

## Responsive Breakpoints

| Breakpoint | Width | Target |
|------------|-------|--------|
| xs | 480px | Mobile landscape |
| sm | 640px | Small tablets |
| md | 768px | Tablets |
| lg | 1024px | Small desktops |
| xl | 1200px | Standard desktops |
| 2xl | 1400px | Large displays |

## Feature Summary

### 1. One-Click Installer
- 5-screen installation wizard
- Platform-specific considerations (macOS, Windows, Linux)
- Progress tracking and error handling
- Post-install guidance

### 2. GUI Configuration
- First-run setup wizard (6 steps)
- Exchange connection management
- API key security with best practices
- Risk parameter configuration
- Connection testing UI

### 3. Strategy Marketplace
- Strategy browsing with filters and search
- Strategy cards with performance metrics
- Detailed strategy view with charts
- Parameter configuration modal
- Backtest results visualization
- Strategy comparison view

### 4. Real-Time Charts
- Multiple layout options (single, split, grid)
- Symbol and timeframe selectors
- Indicator menu with configuration
- Drawing tools toolbar
- Chart-based order entry
- Real-time price updates

### 5. Security Education
- Security onboarding wizard
- Trading mode indicator (Simulated/Live)
- Warning dialogs for risky actions
- Risk settings dashboard
- Emergency stop functionality
- Contextual education tooltips

## Implementation Priority

Based on user flow dependencies:

1. **Phase 1**: Installer UI + Configuration UI
   - Required for initial user onboarding

2. **Phase 2**: Security Education UI
   - Critical for user safety before trading

3. **Phase 3**: Strategy Marketplace UI
   - Enables strategy discovery and deployment

4. **Phase 4**: Real-Time Charts UI
   - Enhances trading experience

## Accessibility Requirements

All designs must meet:
- WCAG 2.1 AA compliance
- Keyboard navigation support
- Screen reader compatibility
- Color contrast >= 4.5:1
- Focus indicators on all interactive elements
- Reduced motion alternatives

## User Testing Plan

Each feature includes:
- Test scenarios for common user flows
- Metrics to track for success measurement
- A/B testing ideas for optimization

## Related Documentation

- [Design Tokens](./00-design-system.md) - Complete token reference
- [Existing UI Architecture](../../ui-react-architecture.md) - Current React architecture
- [Baseline Tailwind Config](../../baseline-tailwind-config.js) - Tailwind configuration

## Changelog

| Date | Version | Changes |
|------|---------|---------|
| 2026-02-25 | 1.0.0 | Initial P0 UI designs |
