# VeloZ Component Specifications

This document specifies UI components using **Tailwind CSS + Headless UI + Lucide React**.

## Technology Stack

| Library | Purpose |
|---------|---------|
| Tailwind CSS 4.x | Utility-first styling |
| Headless UI | Accessible UI primitives (Dialog, Transition, Listbox, etc.) |
| Lucide React | Icon library |
| React Hook Form + Zod | Form handling and validation |

---

## 1. Authentication Components

### 1.1 Login

**Location**: `src/features/auth/components/Login.tsx`

**Purpose**: User authentication with JWT tokens

**Implementation**:
```tsx
import { useForm } from 'react-hook-form';
import { zodResolver } from '@hookform/resolvers/zod';
import { z } from 'zod';
import { Button, Input, Card } from '@/shared/components';

const loginSchema = z.object({
  user_id: z.string().min(1, 'User ID is required'),
  password: z.string().min(1, 'Password is required'),
});

export function Login() {
  const { register, handleSubmit, formState: { errors } } = useForm({
    resolver: zodResolver(loginSchema),
  });

  return (
    <div className="min-h-screen flex items-center justify-center bg-background-secondary p-4">
      <Card className="w-full max-w-md">
        <div className="text-center mb-6">
          <h1 className="text-2xl font-bold text-text">VeloZ</h1>
          <p className="text-text-muted mt-1">Sign in to your account</p>
        </div>

        <form onSubmit={handleSubmit(onSubmit)} className="space-y-4">
          <Input
            label="User ID"
            {...register('user_id')}
            error={errors.user_id?.message}
          />
          <Input
            label="Password"
            type="password"
            {...register('password')}
            error={errors.password?.message}
          />
          <Button type="submit" fullWidth isLoading={isLoading}>
            Sign In
          </Button>
        </form>
      </Card>
    </div>
  );
}
```

**Accessibility**:
- Form labels linked to inputs via `htmlFor`
- Error messages use `aria-invalid` and `aria-describedby`
- Focus management on error
- Enter key submits form

---

### 1.2 ProtectedRoute

**Location**: `src/features/auth/components/ProtectedRoute.tsx`

**Purpose**: Require authentication for routes

**Props**:
```typescript
interface ProtectedRouteProps {
  children: React.ReactNode;
  redirectTo?: string;  // Default: '/login'
}
```

**Implementation**:
```tsx
import { Navigate, useLocation } from 'react-router-dom';
import { useAuthStore } from '../store';

export function ProtectedRoute({ children, redirectTo = '/login' }: ProtectedRouteProps) {
  const { isAuthenticated } = useAuthStore();
  const location = useLocation();

  if (!isAuthenticated) {
    return <Navigate to={redirectTo} state={{ from: location }} replace />;
  }

  return <>{children}</>;
}
```

---

### 1.3 SessionExpiredModal

**Purpose**: Notify user of session expiration

**Implementation using Headless UI**:
```tsx
import { Modal, Button } from '@/shared/components';

export function SessionExpiredModal({ isOpen, onReLogin, onLogout }) {
  return (
    <Modal
      isOpen={isOpen}
      onClose={() => {}}
      title="Session Expired"
      closeOnOverlayClick={false}
      showCloseButton={false}
    >
      <p className="text-text-muted">
        Your session has expired. Please log in again to continue.
      </p>
      <div className="flex gap-3 mt-6">
        <Button variant="primary" onClick={onReLogin} fullWidth>
          Log In Again
        </Button>
        <Button variant="secondary" onClick={onLogout} fullWidth>
          Log Out
        </Button>
      </div>
    </Modal>
  );
}
```

---

## 2. Core UI Components

### 2.1 Button

**Location**: `src/shared/components/Button/Button.tsx`

**Props**:
```typescript
interface ButtonProps extends ButtonHTMLAttributes<HTMLButtonElement> {
  variant?: 'primary' | 'secondary' | 'success' | 'danger' | 'warning';
  size?: 'sm' | 'md' | 'lg';
  isLoading?: boolean;
  leftIcon?: ReactNode;
  rightIcon?: ReactNode;
  fullWidth?: boolean;
}
```

**Variant Classes**:
```typescript
const variantClasses = {
  primary: 'bg-primary text-white hover:opacity-90 focus:ring-primary',
  secondary: 'bg-background-secondary text-text border border-border hover:bg-gray-100 focus:ring-gray-500',
  success: 'bg-success text-white hover:opacity-90 focus:ring-success',
  danger: 'bg-danger text-white hover:opacity-90 focus:ring-danger',
  warning: 'bg-warning text-white hover:opacity-90 focus:ring-warning',
};
```

**Size Classes**:
```typescript
const sizeClasses = {
  sm: 'px-3 py-1.5 text-xs',
  md: 'px-4 py-2.5 text-sm',
  lg: 'px-6 py-3 text-base',
};
```

**Base Classes**:
```
inline-flex items-center justify-center rounded-md font-medium
transition-opacity duration-150 focus:outline-none focus:ring-2
focus:ring-offset-2 disabled:opacity-50 disabled:cursor-not-allowed
```

---

### 2.2 Input

**Location**: `src/shared/components/Input/Input.tsx`

**Props**:
```typescript
interface InputProps extends InputHTMLAttributes<HTMLInputElement> {
  label?: string;
  error?: string;
  helperText?: string;
  leftIcon?: ReactNode;
  rightIcon?: ReactNode;
  fullWidth?: boolean;
}
```

**Implementation Pattern**:
```tsx
<div className={fullWidth ? 'w-full' : ''}>
  {label && (
    <label className="block text-sm font-medium text-text mb-1">
      {label}
    </label>
  )}
  <div className="relative">
    {leftIcon && (
      <div className="absolute inset-y-0 left-0 pl-3 flex items-center pointer-events-none text-text-muted">
        {leftIcon}
      </div>
    )}
    <input
      className={`
        px-3 py-2.5 border rounded-md text-sm transition-colors duration-150
        focus:outline-none focus:ring-2 focus:border-transparent
        disabled:bg-gray-100 disabled:cursor-not-allowed
        ${error ? 'border-danger focus:ring-danger bg-danger-50' : 'border-gray-300 focus:ring-primary'}
        ${leftIcon ? 'pl-10' : ''}
        ${rightIcon ? 'pr-10' : ''}
        ${fullWidth ? 'w-full' : ''}
      `}
      aria-invalid={error ? 'true' : 'false'}
    />
  </div>
  {error && <p className="mt-1 text-sm text-danger">{error}</p>}
</div>
```

---

### 2.3 Select

**Location**: `src/shared/components/Select/Select.tsx`

**Props**:
```typescript
interface SelectOption {
  value: string;
  label: string;
  disabled?: boolean;
}

interface SelectProps extends SelectHTMLAttributes<HTMLSelectElement> {
  label?: string;
  error?: string;
  helperText?: string;
  options: SelectOption[];
  placeholder?: string;
  fullWidth?: boolean;
}
```

---

### 2.4 Card

**Location**: `src/shared/components/Card/Card.tsx`

**Props**:
```typescript
interface CardProps extends HTMLAttributes<HTMLDivElement> {
  title?: string;
  subtitle?: string;
  headerAction?: ReactNode;
  footer?: ReactNode;
  noPadding?: boolean;
}
```

**Implementation**:
```tsx
<div className="bg-background border border-border rounded-lg shadow-card">
  {hasHeader && (
    <div className="flex items-center justify-between px-4 py-3 border-b border-border">
      <div>
        <h3 className="text-lg font-semibold text-text">{title}</h3>
        {subtitle && <p className="text-sm text-text-muted mt-0.5">{subtitle}</p>}
      </div>
      {headerAction}
    </div>
  )}
  <div className={noPadding ? '' : 'p-4'}>{children}</div>
  {footer && (
    <div className="px-4 py-3 border-t border-border bg-background-secondary rounded-b-lg">
      {footer}
    </div>
  )}
</div>
```

---

### 2.5 Modal

**Location**: `src/shared/components/Modal/Modal.tsx`

**Uses**: Headless UI `Dialog`, `Transition`

**Props**:
```typescript
interface ModalProps {
  isOpen: boolean;
  onClose: () => void;
  title?: string;
  description?: string;
  children: ReactNode;
  footer?: ReactNode;
  size?: 'sm' | 'md' | 'lg' | 'xl';
  closeOnOverlayClick?: boolean;
  showCloseButton?: boolean;
}
```

**Implementation Pattern**:
```tsx
import { Dialog, DialogPanel, DialogTitle, Transition, TransitionChild } from '@headlessui/react';
import { X } from 'lucide-react';

<Transition show={isOpen} as={Fragment}>
  <Dialog className="relative z-50" onClose={onClose}>
    {/* Backdrop */}
    <TransitionChild
      enter="ease-out duration-200"
      enterFrom="opacity-0"
      enterTo="opacity-100"
      leave="ease-in duration-150"
      leaveFrom="opacity-100"
      leaveTo="opacity-0"
    >
      <div className="fixed inset-0 bg-black/50" />
    </TransitionChild>

    {/* Modal */}
    <div className="fixed inset-0 overflow-y-auto">
      <div className="flex min-h-full items-center justify-center p-4">
        <TransitionChild
          enter="ease-out duration-200"
          enterFrom="opacity-0 scale-95"
          enterTo="opacity-100 scale-100"
          leave="ease-in duration-150"
          leaveFrom="opacity-100 scale-100"
          leaveTo="opacity-0 scale-95"
        >
          <DialogPanel className="w-full max-w-md rounded-xl bg-background shadow-modal">
            {/* Header with close button */}
            <div className="flex items-start justify-between px-6 py-4 border-b border-border">
              <DialogTitle className="text-lg font-semibold text-text">
                {title}
              </DialogTitle>
              <button onClick={onClose} className="p-1 rounded-md text-text-muted hover:text-text">
                <X className="h-5 w-5" />
              </button>
            </div>
            {/* Content */}
            <div className="px-6 py-4">{children}</div>
          </DialogPanel>
        </TransitionChild>
      </div>
    </div>
  </Dialog>
</Transition>
```

---

### 2.6 Toast

**Location**: `src/shared/components/Toast/Toast.tsx`

**Props**:
```typescript
interface Toast {
  id: string;
  type: 'success' | 'error' | 'warning' | 'info';
  title: string;
  message?: string;
  duration?: number;
}
```

**Type Styles**:
```typescript
const typeStyles = {
  success: {
    bg: 'bg-success-50',
    border: 'border-success-200',
    icon: CheckCircle,
    iconColor: 'text-success',
  },
  error: {
    bg: 'bg-danger-50',
    border: 'border-danger-200',
    icon: XCircle,
    iconColor: 'text-danger',
  },
  warning: {
    bg: 'bg-warning-50',
    border: 'border-warning-200',
    icon: AlertTriangle,
    iconColor: 'text-warning',
  },
  info: {
    bg: 'bg-info-50',
    border: 'border-info-200',
    icon: Info,
    iconColor: 'text-info',
  },
};
```

---

### 2.7 Spinner

**Location**: `src/shared/components/Spinner/Spinner.tsx`

**Props**:
```typescript
interface SpinnerProps {
  size?: 'sm' | 'md' | 'lg';
  className?: string;
}
```

**Size Classes**:
```typescript
const sizeClasses = {
  sm: 'h-4 w-4',
  md: 'h-6 w-6',
  lg: 'h-8 w-8',
};
```

---

## 3. Layout Components

### 3.1 MainLayout

**Location**: `src/shared/components/Layout/MainLayout.tsx`

**Structure**:
```tsx
<div className="min-h-screen flex flex-col bg-background-secondary">
  <Header onMenuClick={() => setSidebarOpen(true)} />
  <div className="flex flex-1">
    <Sidebar isOpen={sidebarOpen} onClose={() => setSidebarOpen(false)} />
    <main className="flex-1 p-4 lg:p-6 overflow-auto">
      <div className="max-w-7xl mx-auto">
        <Outlet />
      </div>
    </main>
  </div>
  <Footer />
</div>
```

---

### 3.2 Sidebar

**Location**: `src/shared/components/Layout/Sidebar.tsx`

**Navigation Items with Lucide Icons**:
```typescript
import {
  LayoutDashboard,
  TrendingUp,
  LineChart,
  Layers,
  FlaskConical,
  Settings,
} from 'lucide-react';

const navItems = [
  { to: '/dashboard', label: 'Dashboard', icon: LayoutDashboard },
  { to: '/trading', label: 'Trading', icon: TrendingUp },
  { to: '/market', label: 'Market', icon: LineChart },
  { to: '/strategies', label: 'Strategies', icon: Layers },
  { to: '/backtest', label: 'Backtest', icon: FlaskConical },
  { to: '/settings', label: 'Settings', icon: Settings },
];
```

**NavLink Active State**:
```tsx
<NavLink
  to={item.to}
  className={({ isActive }) =>
    `flex items-center gap-3 px-3 py-2.5 rounded-md text-sm font-medium transition-colors ${
      isActive
        ? 'bg-primary text-white'
        : 'text-text-muted hover:text-text hover:bg-gray-100'
    }`
  }
>
  <item.icon className="h-5 w-5" />
  {item.label}
</NavLink>
```

---

### 3.3 Header

**Location**: `src/shared/components/Layout/Header.tsx`

**Components**:
- Logo (links to dashboard)
- Mobile menu button (hamburger icon)
- Connection status indicator
- User avatar with dropdown

---

## 4. Trading Components

### 4.1 OrderBook

**Purpose**: Real-time order book display with depth visualization

**Props**:
```typescript
interface OrderBookProps {
  symbol: string;
  maxLevels?: number;        // Default: 15
  showDepthBars?: boolean;   // Default: true
  priceDecimals?: number;    // Default: 2
  qtyDecimals?: number;      // Default: 4
  onPriceClick?: (price: number, side: 'bid' | 'ask') => void;
}
```

**Implementation Pattern**:
```tsx
<div className="font-mono text-xs">
  {/* Header */}
  <div className="grid grid-cols-3 px-3 py-2 text-text-muted uppercase text-[10px] border-b border-border">
    <span>Price</span>
    <span className="text-right">Size</span>
    <span className="text-right">Total</span>
  </div>

  {/* Ask rows (sells) */}
  {asks.map((ask) => (
    <div className="grid grid-cols-3 px-3 py-1 relative hover:bg-background-secondary">
      <span className="text-sell">{formatPrice(ask.price)}</span>
      <span className="text-right">{formatQuantity(ask.size)}</span>
      <span className="text-right text-text-muted">{formatQuantity(ask.total)}</span>
      <div
        className="absolute inset-y-0 right-0 depth-bar-ask"
        style={{ width: `${ask.depthPercent}%` }}
      />
    </div>
  ))}

  {/* Spread */}
  <div className="flex justify-center py-2 bg-background-secondary text-sm text-text-muted">
    Spread: {spread} ({spreadPercent}%)
  </div>

  {/* Bid rows (buys) */}
  {bids.map((bid) => (
    <div className="grid grid-cols-3 px-3 py-1 relative hover:bg-background-secondary">
      <span className="text-buy">{formatPrice(bid.price)}</span>
      <span className="text-right">{formatQuantity(bid.size)}</span>
      <span className="text-right text-text-muted">{formatQuantity(bid.total)}</span>
      <div
        className="absolute inset-y-0 right-0 depth-bar-bid"
        style={{ width: `${bid.depthPercent}%` }}
      />
    </div>
  ))}
</div>
```

---

### 4.2 OrderForm

**Purpose**: Place new orders with validation

**Props**:
```typescript
interface OrderFormProps {
  symbol: string;
  defaultSide?: 'BUY' | 'SELL';
  onSubmit: (order: NewOrder) => Promise<OrderResult>;
  disabled?: boolean;
}
```

**Side Toggle Pattern**:
```tsx
<div className="flex gap-2">
  <Button
    variant={side === 'BUY' ? 'success' : 'secondary'}
    onClick={() => setSide('BUY')}
    className="flex-1"
  >
    Buy
  </Button>
  <Button
    variant={side === 'SELL' ? 'danger' : 'secondary'}
    onClick={() => setSide('SELL')}
    className="flex-1"
  >
    Sell
  </Button>
</div>
```

---

### 4.3 OrderList

**Purpose**: Display and manage orders

**Columns**:
| Column | Width | Description |
|--------|-------|-------------|
| Time | 150px | Order timestamp |
| Symbol | 100px | Trading pair |
| Side | 60px | BUY/SELL with color |
| Type | 80px | LIMIT/MARKET |
| Price | 100px | Limit price (mono font) |
| Quantity | 100px | Order quantity (mono font) |
| Filled | 100px | Executed quantity |
| Status | 100px | Status badge |
| Actions | 80px | Cancel button |

**Side Color Classes**:
```tsx
<span className={side === 'BUY' ? 'text-buy' : 'text-sell'}>
  {side}
</span>
```

---

## 5. Position Components

### 5.1 PositionTable

**Props**:
```typescript
interface PositionTableProps {
  positions: Position[];
  showActions?: boolean;
  onClosePosition?: (symbol: string) => void;
}
```

**PnL Display Pattern**:
```tsx
<span className={`mono font-medium ${pnl >= 0 ? 'text-buy' : 'text-sell'}`}>
  {pnl >= 0 ? '+' : ''}{formatPrice(pnl)}
</span>
```

---

### 5.2 PnLSummary

**Props**:
```typescript
interface PnLSummaryProps {
  totalPnl: number;
  unrealizedPnl: number;
  realizedPnl: number;
  notionalValue: number;
}
```

**Layout Pattern**:
```tsx
<div className="grid grid-cols-2 md:grid-cols-4 gap-4">
  <Card>
    <div className="text-sm text-text-muted">Total PnL</div>
    <div className={`text-2xl font-bold mono ${totalPnl >= 0 ? 'text-buy' : 'text-sell'}`}>
      {totalPnl >= 0 ? '+' : ''}{formatPrice(totalPnl)}
    </div>
  </Card>
  {/* ... more cards */}
</div>
```

---

## 6. Strategy Components

### 6.1 StrategyCard

**Props**:
```typescript
interface StrategyCardProps {
  strategy: Strategy;
  onStart: () => void;
  onStop: () => void;
  onEdit: () => void;
}
```

**Status Badge Pattern**:
```tsx
const statusStyles = {
  running: 'bg-success-50 text-success border-success-200',
  stopped: 'bg-gray-100 text-text-muted border-gray-200',
  paused: 'bg-warning-50 text-warning-600 border-warning-200',
  error: 'bg-danger-50 text-danger border-danger-200',
};

<span className={`inline-flex items-center px-2.5 py-0.5 rounded-full text-xs font-medium border ${statusStyles[status]}`}>
  <span className="w-1.5 h-1.5 rounded-full bg-current mr-1.5" />
  {status}
</span>
```

**Strategy Type Colors**:
```tsx
const typeColors = {
  momentum: 'bg-strategy-momentum-bg text-strategy-momentum',
  'mean-reversion': 'bg-strategy-mean-reversion-bg text-strategy-mean-reversion',
  grid: 'bg-strategy-grid-bg text-strategy-grid',
  'market-making': 'bg-strategy-market-making-bg text-strategy-market-making',
  'trend-following': 'bg-strategy-trend-following-bg text-strategy-trend-following',
};
```

---

## 7. Backtest Components

### 7.1 BacktestForm

**Props**:
```typescript
interface BacktestFormProps {
  strategies: StrategyType[];
  onSubmit: (config: BacktestConfig) => Promise<void>;
  isRunning?: boolean;
}
```

**Fields**:
- Strategy type (Select)
- Symbol (Select)
- Date range (two date inputs)
- Initial balance (Input with number type)
- Strategy parameters (dynamic based on type)

---

### 7.2 BacktestResults

**Props**:
```typescript
interface BacktestResultsProps {
  results: BacktestResult[];
  selectedId?: string;
  onSelect: (id: string) => void;
}
```

**Metrics Display**:
```tsx
<div className="grid grid-cols-2 gap-4 p-4 bg-background-secondary rounded-lg">
  <div>
    <div className="text-xs text-text-muted uppercase">Return</div>
    <div className={`text-lg font-semibold mono ${returnPct >= 0 ? 'text-buy' : 'text-sell'}`}>
      {returnPct >= 0 ? '+' : ''}{returnPct.toFixed(2)}%
    </div>
  </div>
  <div>
    <div className="text-xs text-text-muted uppercase">Sharpe Ratio</div>
    <div className="text-lg font-semibold mono">{sharpe.toFixed(2)}</div>
  </div>
</div>
```

---

## 8. Icon Reference (Lucide React)

| Concept | Icon | Import |
|---------|------|--------|
| Dashboard | `LayoutDashboard` | `lucide-react` |
| Trading | `TrendingUp` | `lucide-react` |
| Market | `LineChart` | `lucide-react` |
| Strategies | `Layers` | `lucide-react` |
| Backtest | `FlaskConical` | `lucide-react` |
| Settings | `Settings` | `lucide-react` |
| Close | `X` | `lucide-react` |
| Menu | `Menu` | `lucide-react` |
| Success | `CheckCircle` | `lucide-react` |
| Error | `XCircle` | `lucide-react` |
| Warning | `AlertTriangle` | `lucide-react` |
| Info | `Info` | `lucide-react` |
| Play | `Play` | `lucide-react` |
| Pause | `Pause` | `lucide-react` |
| Stop | `Square` | `lucide-react` |
| Edit | `Pencil` | `lucide-react` |
| Delete | `Trash2` | `lucide-react` |
| Refresh | `RefreshCw` | `lucide-react` |
| Download | `Download` | `lucide-react` |

---

## 9. Implementation Priority

### Phase 1 (Completed)
- [x] Button
- [x] Input
- [x] Select
- [x] Card
- [x] Modal
- [x] Toast
- [x] Spinner
- [x] MainLayout
- [x] Header
- [x] Sidebar
- [x] Footer
- [x] ErrorBoundary
- [x] Login
- [x] ProtectedRoute

### Phase 2 (In Progress)
- [ ] OrderBook
- [ ] OrderForm
- [ ] OrderList
- [ ] PositionTable
- [ ] PnLSummary

### Phase 3 (Pending)
- [ ] StrategyCard
- [ ] StrategyForm
- [ ] BacktestForm
- [ ] BacktestResults
- [ ] EquityCurve

### Phase 4 (Future)
- [ ] AuditLogViewer
- [ ] PnLChart
- [ ] TradeAnalysis
- [ ] SessionExpiredModal
- [ ] ThemeToggle (dark mode)

---

## 10. Advanced Component Patterns

This section documents proven patterns from the actual VeloZ implementation.

### 10.1 Data Tables with Tailwind

**Location**: `src/features/trading/components/OrderHistory.tsx`, `src/features/backtest/components/TradeListTable.tsx`

**Basic Table Structure**:
```tsx
<div className="overflow-x-auto">
  <table className="w-full text-sm">
    <thead>
      <tr className="border-b border-border">
        <th className="text-left py-3 px-2 font-semibold text-text-muted text-xs uppercase tracking-wide">
          Column Header
        </th>
        {/* More headers */}
      </tr>
    </thead>
    <tbody>
      {data.map((item) => (
        <tr key={item.id} className="border-b border-border hover:bg-background-secondary/50">
          <td className="px-3 py-2 text-sm">{item.value}</td>
          {/* More cells */}
        </tr>
      ))}
    </tbody>
  </table>
</div>
```

**Filter Tabs Pattern**:
```tsx
const [filter, setFilter] = useState<'all' | 'open' | 'filled' | 'cancelled'>('all');

<div className="flex gap-2 mb-4 border-b border-border pb-2">
  {(['all', 'open', 'filled', 'cancelled'] as const).map((status) => (
    <button
      key={status}
      onClick={() => setFilter(status)}
      className={`px-3 py-1.5 text-sm font-medium rounded-md transition-colors ${
        filter === status
          ? 'bg-primary text-white'
          : 'text-text-muted hover:bg-background-secondary'
      }`}
    >
      {status.charAt(0).toUpperCase() + status.slice(1)}
    </button>
  ))}
</div>
```

**Pagination Pattern**:
```tsx
const [currentPage, setCurrentPage] = useState(0);
const pageSize = 10;
const totalPages = Math.ceil(data.length / pageSize);

const paginatedData = useMemo(() => {
  const start = currentPage * pageSize;
  return data.slice(start, start + pageSize);
}, [data, currentPage, pageSize]);

{totalPages > 1 && (
  <div className="flex items-center justify-between mt-4 pt-4 border-t border-border">
    <span className="text-sm text-text-muted">
      Showing {currentPage * pageSize + 1} - {Math.min((currentPage + 1) * pageSize, data.length)} of {data.length}
    </span>
    <div className="flex gap-2">
      <Button
        variant="secondary"
        size="sm"
        onClick={() => setCurrentPage((p) => Math.max(0, p - 1))}
        disabled={currentPage === 0}
      >
        Previous
      </Button>
      <Button
        variant="secondary"
        size="sm"
        onClick={() => setCurrentPage((p) => Math.min(totalPages - 1, p + 1))}
        disabled={currentPage >= totalPages - 1}
      >
        Next
      </Button>
    </div>
  </div>
)}
```

**Progress Bar in Table Cell**:
```tsx
<td className="px-3 py-2 text-sm font-mono">
  {formatQuantity(executed, 4)} / {formatQuantity(total, 4)}
  {total > 0 && (
    <div className="mt-1 h-1 bg-gray-200 rounded-full overflow-hidden">
      <div
        className="h-full bg-primary"
        style={{ width: `${(executed / total) * 100}%` }}
      />
    </div>
  )}
</td>
```

---

### 10.2 Color Constants for Status and Side

**Location**: `src/shared/utils/constants.ts`

```typescript
/**
 * Status color mapping - use with className
 */
export const STATUS_COLORS: Record<string, string> = {
  // Order statuses
  ACCEPTED: 'bg-info-100 text-info-800',
  PARTIALLY_FILLED: 'bg-warning-100 text-warning-800',
  FILLED: 'bg-success-100 text-success-800',
  CANCELLED: 'bg-gray-100 text-gray-800',
  REJECTED: 'bg-danger-100 text-danger-800',
  EXPIRED: 'bg-gray-100 text-gray-800',
  // Strategy states
  Running: 'bg-success-100 text-success-800',
  Paused: 'bg-warning-100 text-warning-800',
  Stopped: 'bg-gray-100 text-gray-800',
};

/**
 * Side color mapping - use with className
 */
export const SIDE_COLORS: Record<string, string> = {
  BUY: 'text-success',
  SELL: 'text-danger',
  LONG: 'text-success',
  SHORT: 'text-danger',
};
```

**Usage**:
```tsx
import { STATUS_COLORS, SIDE_COLORS } from '@/shared/utils/constants';

// Status badge
<span className={`inline-flex items-center px-2 py-0.5 rounded text-xs font-medium ${STATUS_COLORS[order.status]}`}>
  {order.status}
</span>

// Side indicator
<span className={SIDE_COLORS[order.side]}>{order.side}</span>
```

---

### 10.3 Recharts Integration

**Location**: `src/features/backtest/components/EquityCurveChart.tsx`

**Basic Line Chart Setup**:
```tsx
import {
  LineChart,
  Line,
  XAxis,
  YAxis,
  CartesianGrid,
  Tooltip,
  ResponsiveContainer,
  ReferenceLine,
} from 'recharts';

export function EquityCurveChart({ data, initialCapital }: Props) {
  return (
    <div className="h-64">
      <ResponsiveContainer width="100%" height="100%">
        <LineChart
          data={data}
          margin={{ top: 10, right: 30, left: 10, bottom: 0 }}
        >
          <CartesianGrid strokeDasharray="3 3" stroke="#e5e7eb" />
          <XAxis
            dataKey="date"
            tick={{ fontSize: 12, fill: '#6b7280' }}
            tickLine={{ stroke: '#e5e7eb' }}
            axisLine={{ stroke: '#e5e7eb' }}
          />
          <YAxis
            tickFormatter={(value) => formatCurrency(value)}
            tick={{ fontSize: 12, fill: '#6b7280' }}
            tickLine={{ stroke: '#e5e7eb' }}
            axisLine={{ stroke: '#e5e7eb' }}
            width={80}
          />
          <Tooltip
            formatter={(value) => [formatCurrency(Number(value)), 'Equity']}
            contentStyle={{
              backgroundColor: '#ffffff',
              border: '1px solid #e5e7eb',
              borderRadius: '6px',
              fontSize: '13px',
            }}
          />
          <ReferenceLine
            y={initialCapital}
            stroke="#9ca3af"
            strokeDasharray="5 5"
            label={{ value: 'Initial', position: 'right', fill: '#9ca3af', fontSize: 11 }}
          />
          <Line
            type="monotone"
            dataKey="equity"
            stroke="#2563eb"
            strokeWidth={2}
            dot={false}
            activeDot={{ r: 4, fill: '#2563eb' }}
          />
        </LineChart>
      </ResponsiveContainer>
    </div>
  );
}
```

**Trading Color Scheme for Charts**:
```typescript
const CHART_COLORS = {
  primary: '#2563eb',      // info-600 - main line
  profit: '#059669',       // success - positive areas
  loss: '#dc2626',         // danger - negative areas
  grid: '#e5e7eb',         // border - grid lines
  axis: '#6b7280',         // text-muted - axis text
  reference: '#9ca3af',    // text-light - reference lines
};
```

---

### 10.4 Real-Time Order Book Pattern

**Location**: `src/features/trading/components/OrderBook.tsx`

**Depth Bar with Color**:
```tsx
interface OrderBookRowProps {
  price: number;
  qty: number;
  total: number;
  side: 'bid' | 'ask';
  maxTotal: number;
}

function OrderBookRow({ price, qty, total, side, maxTotal }: OrderBookRowProps) {
  const depthPercent = maxTotal > 0 ? (total / maxTotal) * 100 : 0;
  const bgColor = side === 'bid' ? 'bg-success/10' : 'bg-danger/10';
  const textColor = side === 'bid' ? 'text-success' : 'text-danger';

  return (
    <div className="relative flex items-center py-1 px-2 text-sm font-mono">
      {/* Depth bar - positioned based on side */}
      <div
        className={`absolute inset-y-0 ${side === 'bid' ? 'right-0' : 'left-0'} ${bgColor}`}
        style={{ width: `${depthPercent}%` }}
      />

      {/* Content - always on top */}
      <div className="relative flex w-full items-center justify-between">
        <span className={textColor}>{formatPrice(price, 2)}</span>
        <span className="text-text">{formatQuantity(qty, 4)}</span>
        <span className="text-text-muted">{formatQuantity(total, 4)}</span>
      </div>
    </div>
  );
}
```

**Connection Status Indicator**:
```tsx
const connectionIndicator = (
  <div className="flex items-center gap-2">
    <div
      className={`w-2 h-2 rounded-full ${
        connectionState === 'connected'
          ? 'bg-success'
          : connectionState === 'connecting' || connectionState === 'reconnecting'
            ? 'bg-warning animate-pulse'
            : 'bg-danger'
      }`}
    />
    <span className="text-xs text-text-muted capitalize">{connectionState}</span>
  </div>
);
```

---

### 10.5 Complex Form Patterns

**Location**: `src/features/strategies/components/StrategyConfigModal.tsx`

**React Hook Form + Modal Integration**:
```tsx
import { useEffect } from 'react';
import { useForm } from 'react-hook-form';
import { zodResolver } from '@hookform/resolvers/zod';
import { z } from 'zod';
import { Modal, Button, Input, Select } from '@/shared/components';

const configSchema = z.object({
  name: z.string().min(1, 'Name is required'),
  symbol: z.string().min(1, 'Symbol is required'),
  param1: z.coerce.number().min(0, 'Must be positive'),
  param2: z.coerce.number().min(0, 'Must be positive'),
});

type ConfigFormData = z.infer<typeof configSchema>;

export function ConfigModal({ isOpen, onClose, initialData }: Props) {
  const {
    register,
    handleSubmit,
    reset,
    formState: { errors, isDirty, isSubmitting },
  } = useForm<ConfigFormData>({
    resolver: zodResolver(configSchema),
    defaultValues: initialData,
  });

  // Reset form when modal opens with new data
  useEffect(() => {
    if (isOpen && initialData) {
      reset(initialData);
    }
  }, [isOpen, initialData, reset]);

  const onSubmit = async (data: ConfigFormData) => {
    await saveConfig(data);
    onClose();
  };

  return (
    <Modal
      isOpen={isOpen}
      onClose={onClose}
      title="Configure Strategy"
      size="lg"
      footer={
        <div className="flex justify-end gap-3">
          <Button variant="secondary" onClick={onClose}>
            Cancel
          </Button>
          <Button
            onClick={handleSubmit(onSubmit)}
            disabled={!isDirty}
            isLoading={isSubmitting}
          >
            Save Changes
          </Button>
        </div>
      }
    >
      <form className="space-y-4">
        <Input
          label="Strategy Name"
          {...register('name')}
          error={errors.name?.message}
        />
        <Select
          label="Symbol"
          options={symbolOptions}
          {...register('symbol')}
          error={errors.symbol?.message}
        />
        <div className="grid grid-cols-2 gap-4">
          <Input
            label="Parameter 1"
            type="number"
            {...register('param1')}
            error={errors.param1?.message}
          />
          <Input
            label="Parameter 2"
            type="number"
            {...register('param2')}
            error={errors.param2?.message}
          />
        </div>
      </form>
    </Modal>
  );
}
```

---

### 10.6 Formatting Utilities

**Location**: `src/shared/utils/format.ts`

```typescript
// Price with fixed decimals
export function formatPrice(price: number, decimals = 2): string {
  return price.toLocaleString('en-US', {
    minimumFractionDigits: decimals,
    maximumFractionDigits: decimals,
  });
}

// Currency with symbol
export function formatCurrency(amount: number, currency = 'USD'): string {
  return new Intl.NumberFormat('en-US', {
    style: 'currency',
    currency,
  }).format(amount);
}

// Quantity with variable decimals
export function formatQuantity(qty: number, decimals = 4): string {
  return qty.toLocaleString('en-US', {
    minimumFractionDigits: 0,
    maximumFractionDigits: decimals,
  });
}

// PnL with color class
export function formatPnL(pnl: number): { value: string; className: string } {
  const value = formatCurrency(pnl);
  const className = pnl >= 0 ? 'text-success' : 'text-danger';
  return { value: pnl >= 0 ? `+${value}` : value, className };
}

// Truncate long strings (e.g., order IDs)
export function truncate(str: string, maxLength: number): string {
  if (str.length <= maxLength) return str;
  return `${str.slice(0, maxLength - 3)}...`;
}
```

**Usage in Components**:
```tsx
// Price display
<span className="font-mono">{formatPrice(order.price, 2)}</span>

// PnL with color
const { value, className } = formatPnL(position.pnl);
<span className={`font-mono font-semibold ${className}`}>{value}</span>

// Truncated ID with tooltip
<span className="font-mono text-xs" title={order.id}>
  {truncate(order.id, 12)}
</span>
```

---

### 10.7 Loading and Empty States

**Consistent Loading Pattern**:
```tsx
if (isLoading) {
  return (
    <div className="flex items-center justify-center py-12">
      <Spinner size="lg" />
    </div>
  );
}
```

**Error State Pattern**:
```tsx
if (error) {
  return (
    <div className="text-center py-12">
      <p className="text-danger">Failed to load data</p>
      <p className="text-sm text-text-muted mt-1">
        {error instanceof Error ? error.message : 'Unknown error'}
      </p>
      <Button variant="secondary" size="sm" onClick={refetch} className="mt-4">
        Try Again
      </Button>
    </div>
  );
}
```

**Empty State Pattern**:
```tsx
if (!data || data.length === 0) {
  return (
    <div className="text-center py-12">
      <p className="text-text-muted">No items found</p>
      <p className="text-sm text-text-muted mt-1">
        Create your first item to get started
      </p>
    </div>
  );
}
```

---

### 10.8 Responsive Grid Patterns

**Card Grid**:
```tsx
<div className="grid grid-cols-1 md:grid-cols-2 lg:grid-cols-3 gap-4">
  {items.map((item) => (
    <ItemCard key={item.id} item={item} />
  ))}
</div>
```

**Stats Grid**:
```tsx
<div className="grid grid-cols-2 md:grid-cols-4 gap-4">
  <Card>
    <div className="text-sm text-text-muted">Total</div>
    <div className="text-2xl font-bold">{total}</div>
  </Card>
  {/* More stat cards */}
</div>
```

**Two-Column Layout**:
```tsx
<div className="grid grid-cols-1 lg:grid-cols-3 gap-6">
  <div className="lg:col-span-1">
    {/* Sidebar content */}
  </div>
  <div className="lg:col-span-2">
    {/* Main content */}
  </div>
</div>
```
