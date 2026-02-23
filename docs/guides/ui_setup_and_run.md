# VeloZ UI Setup and Run Guide

This guide provides complete instructions for setting up and running the VeloZ React UI application.

## Table of Contents

1. [Overview](#1-overview)
2. [Prerequisites](#2-prerequisites)
3. [Installation](#3-installation)
4. [Development](#4-development)
5. [Testing](#5-testing)
6. [Production Build](#6-production-build)
7. [Integration with Gateway](#7-integration-with-gateway)
8. [Troubleshooting](#8-troubleshooting)

## 1. Overview

The VeloZ UI is a modern, production-ready React application built with:

- **React 19.2** - Latest React with concurrent features
- **TypeScript 5.9** - Strict type safety
- **Vite 7.3** - Fast build tool with HMR
- **Tailwind CSS 4.2** - Utility-first CSS framework
- **Zustand** - Lightweight state management
- **TanStack Query** - Server state management
- **Vitest** - Unit testing framework
- **Playwright** - E2E testing framework

### Architecture

```
apps/ui/
├── src/
│   ├── features/          # Feature modules
│   │   ├── auth/          # Authentication
│   │   ├── dashboard/     # Dashboard with SSE
│   │   ├── trading/       # Order placement & management
│   │   ├── strategies/    # Strategy configuration
│   │   ├── backtest/      # Backtesting
│   │   └── market/        # Market data & charts
│   ├── shared/            # Shared code
│   │   ├── api/           # API client, SSE, WebSocket
│   │   ├── components/    # Reusable UI components
│   │   ├── hooks/         # Custom React hooks
│   │   └── utils/         # Utility functions
│   └── styles/            # Global styles
├── tests/                 # E2E tests
└── package.json
```

## 2. Prerequisites

### 2.1 Required Software

- **Node.js** >= 18.0.0 (recommend 20.x LTS)
- **npm** >= 9.0.0 (or **pnpm** >= 8.0.0, **yarn** >= 1.22.0)

### 2.2 Verify Installation

```bash
node --version   # Should be >= 18.0.0
npm --version    # Should be >= 9.0.0
```

### 2.3 Install Node.js (if needed)

**macOS (via Homebrew):**
```bash
brew install node@20
```

**Linux (via nvm):**
```bash
curl -o- https://raw.githubusercontent.com/nvm-sh/nvm/v0.39.0/install.sh | bash
nvm install 20
nvm use 20
```

**Windows:**
Download from [nodejs.org](https://nodejs.org/)

## 3. Installation

### 3.1 Navigate to UI Directory

```bash
cd apps/ui
```

### 3.2 Install Dependencies

```bash
npm install
```

This will install all required packages (~200 MB):
- React and React DOM
- TypeScript and build tools
- Tailwind CSS and plugins
- Testing frameworks
- Development dependencies

### 3.3 Verify Installation

```bash
npm run build
```

Expected output:
```
✓ built in 3.98s
dist/index.html                   0.46 kB │ gzip:  0.30 kB
dist/assets/index-[hash].css     12.34 kB │ gzip:  3.21 kB
dist/assets/index-[hash].js     360.12 kB │ gzip: 98.45 kB
```

## 4. Development

### 4.1 Start Development Server

```bash
npm run dev
```

Expected output:
```
  VITE v7.3.1  ready in 234 ms

  ➜  Local:   http://localhost:5173/
  ➜  Network: use --host to expose
  ➜  press h + enter to show help
```

The development server will:
- Start on `http://localhost:5173/`
- Enable Hot Module Replacement (HMR)
- Show TypeScript errors in the browser
- Auto-reload on file changes

### 4.2 Configure API Endpoint

The UI connects to the VeloZ gateway API. By default, it expects the gateway at:

```
http://127.0.0.1:8080
```

To change the API endpoint, create a `.env.local` file:

```bash
# apps/ui/.env.local
VITE_API_BASE_URL=http://localhost:8080
```

Or set it at runtime via environment variable:

```bash
VITE_API_BASE_URL=http://192.168.1.100:8080 npm run dev
```

### 4.3 Development Workflow

1. **Start the gateway** (in another terminal):
   ```bash
   # From project root
   ./scripts/run_gateway.sh dev
   ```

2. **Start the UI dev server**:
   ```bash
   cd apps/ui
   npm run dev
   ```

3. **Open browser**:
   - Navigate to `http://localhost:5173/`
   - Login with credentials (default: `admin` / `password`)

4. **Make changes**:
   - Edit files in `src/`
   - Browser auto-reloads with changes
   - TypeScript errors shown in browser console

### 4.4 Development Tools

#### React DevTools
Install browser extension:
- [Chrome](https://chrome.google.com/webstore/detail/react-developer-tools/fmkadmapgofadopljbjfkapdkoienihi)
- [Firefox](https://addons.mozilla.org/en-US/firefox/addon/react-devtools/)

#### TanStack Query DevTools
Automatically enabled in development mode. Access via floating button in bottom-right corner.

## 5. Testing

### 5.1 Unit and Integration Tests

The UI includes **551 unit and integration tests** with 100% pass rate.

#### Run All Tests

```bash
npm test
```

#### Run Tests in Watch Mode

```bash
npm test -- --watch
```

#### Run Tests with Coverage

```bash
npm run test:coverage
```

Expected output:
```
Test Files  68 passed (68)
     Tests  551 passed (551)
  Duration  12.34s

 % Coverage report from v8
---------------------------------
File       | % Stmts | % Branch | % Funcs | % Lines
---------------------------------
All files  |   94.23 |    89.45 |   92.67 |   94.12
```

#### Run Specific Test File

```bash
npm test -- src/features/trading/__tests__/OrderForm.test.tsx
```

### 5.2 E2E Tests

The UI includes **33 E2E tests** covering all user journeys.

#### Install Playwright Browsers (First Time)

```bash
npx playwright install
```

#### Run E2E Tests

```bash
npm run test:e2e
```

#### Run E2E Tests in UI Mode

```bash
npm run test:e2e:ui
```

This opens the Playwright UI for interactive test debugging.

#### E2E Test Coverage

| Test Suite | Tests | Description |
|------------|-------|-------------|
| auth.spec.ts | 5 | Login, logout, protected routes |
| dashboard.spec.ts | 6 | SSE connection, real-time updates |
| trading.spec.ts | 7 | Order placement, cancellation |
| strategies.spec.ts | 5 | Strategy CRUD operations |
| backtest.spec.ts | 4 | Backtest configuration and results |
| market.spec.ts | 4 | Market data and charts |
| error-handling.spec.ts | 2 | Error states and recovery |
| **Total** | **33** | |

### 5.3 Linting

```bash
npm run lint
```

Fix auto-fixable issues:
```bash
npm run lint -- --fix
```

## 6. Production Build

### 6.1 Build for Production

```bash
npm run build
```

This creates an optimized production build in `dist/`:
- Minified JavaScript and CSS
- Code splitting for optimal loading
- Asset hashing for cache busting
- Source maps for debugging

Build metrics:
- **Build time**: ~4 seconds
- **Bundle size**: 360 KB (98 KB gzipped)
- **TypeScript errors**: 0

### 6.2 Preview Production Build

```bash
npm run preview
```

This starts a local server to preview the production build at `http://localhost:4173/`.

### 6.3 Deploy Production Build

The `dist/` directory contains all static assets ready for deployment.

#### Option 1: Serve with Nginx

```nginx
server {
    listen 80;
    server_name veloz.example.com;
    root /path/to/apps/ui/dist;
    index index.html;

    location / {
        try_files $uri $uri/ /index.html;
    }

    location /api {
        proxy_pass http://127.0.0.1:8080;
        proxy_http_version 1.1;
        proxy_set_header Upgrade $http_upgrade;
        proxy_set_header Connection 'upgrade';
        proxy_set_header Host $host;
        proxy_cache_bypass $http_upgrade;
    }
}
```

#### Option 2: Serve with Apache

```apache
<VirtualHost *:80>
    ServerName veloz.example.com
    DocumentRoot /path/to/apps/ui/dist

    <Directory /path/to/apps/ui/dist>
        Options -Indexes +FollowSymLinks
        AllowOverride All
        Require all granted

        # React Router support
        RewriteEngine On
        RewriteBase /
        RewriteRule ^index\.html$ - [L]
        RewriteCond %{REQUEST_FILENAME} !-f
        RewriteCond %{REQUEST_FILENAME} !-d
        RewriteRule . /index.html [L]
    </Directory>

    # Proxy API requests to gateway
    ProxyPass /api http://127.0.0.1:8080/api
    ProxyPassReverse /api http://127.0.0.1:8080/api
</VirtualHost>
```

#### Option 3: Serve with Node.js (serve package)

```bash
npm install -g serve
serve -s dist -p 3000
```

## 7. Integration with Gateway

### 7.1 Gateway API Endpoints

The UI communicates with the gateway via three channels:

#### REST API (JSON)

| Endpoint | Method | Description |
|----------|--------|-------------|
| `/api/account` | GET | Account information |
| `/api/market` | GET | Market data snapshot |
| `/api/orders` | GET | Order history |
| `/api/orders_state` | GET | Current order states |
| `/api/order` | POST | Place new order |
| `/api/cancel` | POST | Cancel order |
| `/api/strategies` | GET | List strategies |
| `/api/strategy` | POST | Create/update strategy |
| `/api/backtest/run` | POST | Run backtest |
| `/api/backtest/results` | GET | Backtest results |

#### Server-Sent Events (SSE)

```
GET /api/stream
```

Real-time event stream for dashboard updates:
- `market` - Market data updates
- `order_update` - Order status changes
- `fill` - Trade executions
- `account` - Account updates
- `error` - Error notifications

#### WebSocket

```
ws://127.0.0.1:8080/ws/market
```

Real-time market data streaming:
- Order book updates
- Trade feed
- Price updates

### 7.2 Start Full Stack

#### Terminal 1: Build and Start Gateway

```bash
# From project root
./scripts/run_gateway.sh dev
```

Expected output:
```
Building engine...
Starting gateway on http://127.0.0.1:8080/
Gateway ready. Press Ctrl+C to stop.
```

#### Terminal 2: Start UI

```bash
cd apps/ui
npm run dev
```

Expected output:
```
  VITE v7.3.1  ready in 234 ms
  ➜  Local:   http://localhost:5173/
```

#### Terminal 3: (Optional) Watch Tests

```bash
cd apps/ui
npm test -- --watch
```

### 7.3 Verify Integration

1. Open browser to `http://localhost:5173/`
2. Login with default credentials
3. Check dashboard shows "Connected" status
4. Verify real-time updates are working
5. Try placing a test order

## 8. Troubleshooting

### 8.1 Port Already in Use

**Error:**
```
Port 5173 is already in use
```

**Solution:**
```bash
# Find and kill process using port 5173
lsof -ti:5173 | xargs kill -9

# Or use a different port
npm run dev -- --port 5174
```

### 8.2 Gateway Connection Failed

**Error in browser console:**
```
Failed to fetch: http://127.0.0.1:8080/api/account
```

**Solutions:**

1. **Check gateway is running:**
   ```bash
   curl http://127.0.0.1:8080/health
   ```

2. **Check CORS settings** (gateway should allow localhost:5173)

3. **Update API base URL:**
   ```bash
   # apps/ui/.env.local
   VITE_API_BASE_URL=http://127.0.0.1:8080
   ```

### 8.3 TypeScript Errors

**Error:**
```
TS2307: Cannot find module '@/shared/api'
```

**Solution:**
```bash
# Restart TypeScript server in your editor
# Or rebuild
npm run build
```

### 8.4 Tests Failing

**Error:**
```
ECONNREFUSED 127.0.0.1:8080
```

**Solution:**

Tests use MSW (Mock Service Worker) and don't require real gateway. If tests fail:

1. **Clear test cache:**
   ```bash
   npm test -- --clearCache
   ```

2. **Check MSW handlers:**
   ```bash
   npm test -- src/test/mocks/handlers.ts
   ```

### 8.5 Build Fails

**Error:**
```
Out of memory
```

**Solution:**
```bash
# Increase Node.js memory limit
NODE_OPTIONS=--max-old-space-size=4096 npm run build
```

### 8.6 E2E Tests Fail

**Error:**
```
browserType.launch: Executable doesn't exist
```

**Solution:**
```bash
# Reinstall Playwright browsers
npx playwright install
```

### 8.7 Hot Reload Not Working

**Solution:**

1. **Check file watcher limits (Linux):**
   ```bash
   echo fs.inotify.max_user_watches=524288 | sudo tee -a /etc/sysctl.conf
   sudo sysctl -p
   ```

2. **Restart dev server:**
   ```bash
   # Ctrl+C to stop
   npm run dev
   ```

### 8.8 Styles Not Loading

**Error:**
Tailwind classes not working in browser

**Solution:**

1. **Rebuild Tailwind:**
   ```bash
   rm -rf node_modules/.vite
   npm run dev
   ```

2. **Check PostCSS config:**
   ```bash
   cat postcss.config.js
   ```

## 9. Additional Resources

### 9.1 Documentation

- [React Documentation](https://react.dev/)
- [TypeScript Handbook](https://www.typescriptlang.org/docs/)
- [Vite Guide](https://vitejs.dev/guide/)
- [Tailwind CSS Docs](https://tailwindcss.com/docs)
- [TanStack Query Docs](https://tanstack.com/query/latest)
- [Vitest Docs](https://vitest.dev/)
- [Playwright Docs](https://playwright.dev/)

### 9.2 Project Documentation

- [UI Architecture](../design/ui-react-architecture.md)
- [Design Tokens](../design/baseline-design-tokens.md)
- [API Documentation](../api/http_api.md)
- [Gateway Documentation](./build_and_run.md#45-gateway-api-endpoints)

### 9.3 VS Code Extensions (Recommended)

- **ESLint** - Linting
- **Prettier** - Code formatting
- **Tailwind CSS IntelliSense** - Tailwind autocomplete
- **TypeScript Vue Plugin (Volar)** - Enhanced TypeScript support
- **Error Lens** - Inline error display

### 9.4 Browser Extensions (Recommended)

- **React Developer Tools** - React debugging
- **Redux DevTools** - State debugging (works with Zustand)

## 10. Quick Reference

### Common Commands

```bash
# Install dependencies
npm install

# Start development server
npm run dev

# Run unit tests
npm test

# Run E2E tests
npm run test:e2e

# Build for production
npm run build

# Preview production build
npm run preview

# Lint code
npm run lint

# Type check
npm run build  # TypeScript check included
```

### Environment Variables

```bash
# .env.local
VITE_API_BASE_URL=http://127.0.0.1:8080
```

### Default Credentials

```
Username: admin
Password: password
```

### Default Ports

- **UI Dev Server**: http://localhost:5173
- **Gateway API**: http://127.0.0.1:8080
- **Preview Server**: http://localhost:4173
