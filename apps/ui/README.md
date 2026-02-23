# VeloZ UI

Modern React-based trading interface for VeloZ quantitative trading framework.

## Quick Start

```bash
# Install dependencies
npm install

# Start development server
npm run dev

# Open browser to http://localhost:5173/
```

## Prerequisites

- Node.js >= 18.0.0
- npm >= 9.0.0

## Technology Stack

- **React 19.2** - UI framework with concurrent features
- **TypeScript 5.9** - Type-safe development
- **Vite 7.3** - Fast build tool with HMR
- **Tailwind CSS 4.2** - Utility-first styling
- **Zustand** - Lightweight state management
- **TanStack Query** - Server state management
- **Vitest** - Unit testing
- **Playwright** - E2E testing

## Available Scripts

```bash
npm run dev          # Start development server (port 5173)
npm run build        # Build for production
npm run preview      # Preview production build
npm test             # Run unit tests
npm run test:e2e     # Run E2E tests
npm run lint         # Lint code
```

## Project Structure

```
apps/ui/
├── src/
│   ├── features/          # Feature modules
│   │   ├── auth/          # Authentication & login
│   │   ├── dashboard/     # Real-time dashboard
│   │   ├── trading/       # Order placement & management
│   │   ├── strategies/    # Strategy configuration
│   │   ├── backtest/      # Backtesting interface
│   │   └── market/        # Market data & charts
│   ├── shared/            # Shared code
│   │   ├── api/           # API client, SSE, WebSocket
│   │   ├── components/    # Reusable UI components
│   │   ├── hooks/         # Custom React hooks
│   │   └── utils/         # Utility functions
│   └── styles/            # Global styles & theme
├── tests/                 # E2E tests
└── public/                # Static assets
```

## Features

### 6 Feature Modules

1. **Authentication** - JWT-based login/logout
2. **Dashboard** - Real-time overview with SSE updates
3. **Trading** - Order placement, active orders, history
4. **Strategies** - Strategy list, create, edit, delete
5. **Backtest** - Backtest runner and results visualization
6. **Market** - Market data, order book, price charts

### Communication Channels

- **REST API** - Standard HTTP JSON endpoints
- **Server-Sent Events (SSE)** - Real-time dashboard updates
- **WebSocket** - Market data streaming

## Development

### Start Full Stack

**Terminal 1: Gateway**
```bash
# From project root
./scripts/run_gateway.sh dev
```

**Terminal 2: UI**
```bash
cd apps/ui
npm run dev
```

### Configure API Endpoint

Create `.env.local`:
```bash
VITE_API_BASE_URL=http://127.0.0.1:8080
```

### Default Credentials

```
Username: admin
Password: password
```

## Testing

### Unit Tests (551 tests)

```bash
npm test                    # Run all tests
npm test -- --watch         # Watch mode
npm run test:coverage       # With coverage report
```

### E2E Tests (33 tests)

```bash
npx playwright install      # First time only
npm run test:e2e            # Run E2E tests
npm run test:e2e:ui         # Interactive mode
```

## Production Build

```bash
# Build
npm run build

# Preview
npm run preview

# Output: dist/ directory
```

Build metrics:
- Build time: ~4 seconds
- Bundle size: 360 KB (98 KB gzipped)
- TypeScript errors: 0

## Documentation

For complete setup and deployment instructions, see:
- [UI Setup and Run Guide](../../docs/guides/ui_setup_and_run.md)
- [UI Architecture](../../docs/design/ui-react-architecture.md)
- [API Documentation](../../docs/api/http_api.md)

## Browser Support

- Chrome/Edge >= 90
- Firefox >= 88
- Safari >= 14

## License

See [LICENSE](../../LICENSE) in project root.
