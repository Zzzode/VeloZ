# VeloZ Accessibility Requirements

## WCAG 2.1 AA Compliance

This document outlines accessibility requirements for the VeloZ trading platform UI, targeting WCAG 2.1 Level AA compliance.

---

## 1. Perceivable

### 1.1 Text Alternatives (1.1.1)

**Requirement**: All non-text content must have text alternatives.

**Implementation**:

```tsx
// Icons must have accessible labels
<Button icon={<PlayCircleOutlined />} aria-label="Start strategy">
  Start
</Button>

// Status indicators need text alternatives
<span className="status-dot connected" aria-label="Connected" />

// Charts must have descriptions
<div role="img" aria-label="Equity curve showing 15% return over 30 days">
  <EquityCurve data={data} />
</div>
```

**Checklist**:
- [ ] All icons have aria-label or accompanying text
- [ ] Status indicators have text alternatives
- [ ] Charts have descriptive labels
- [ ] Images have alt text
- [ ] Decorative elements have aria-hidden="true"

### 1.2 Color Contrast (1.4.3)

**Requirement**: Minimum contrast ratio of 4.5:1 for normal text, 3:1 for large text.

**Color Combinations**:

| Foreground | Background | Ratio | Status |
|------------|------------|-------|--------|
| `#111827` (text) | `#ffffff` (bg) | 16.1:1 | Pass |
| `#6b7280` (muted) | `#ffffff` (bg) | 5.0:1 | Pass |
| `#52c41a` (success) | `#ffffff` (bg) | 3.0:1 | Large text only |
| `#ff4d4f` (error) | `#ffffff` (bg) | 3.9:1 | Large text only |
| `#ffffff` (text) | `#52c41a` (success btn) | 3.0:1 | Large text only |

**Implementation**:

```css
/* Ensure sufficient contrast for trading colors */
.pnl-positive {
  color: #15803d; /* Darker green for better contrast */
}

.pnl-negative {
  color: #dc2626; /* Standard red passes */
}

/* Add underline or icon for color-blind users */
.pnl-positive::before {
  content: "+";
}

.pnl-negative::before {
  content: "-";
}
```

### 1.3 Color Not Sole Indicator (1.4.1)

**Requirement**: Color must not be the only visual means of conveying information.

**Implementation**:

```tsx
// Buy/Sell indicators - use text AND color
<span className={`side ${side === 'BUY' ? 'buy' : 'sell'}`}>
  {side === 'BUY' ? 'BUY' : 'SELL'}
</span>

// PnL indicators - use sign AND color
<span className={pnl >= 0 ? 'pnl-positive' : 'pnl-negative'}>
  {pnl >= 0 ? '+' : ''}{formatCurrency(pnl)}
</span>

// Status badges - use text AND color
<Badge status="running" text="Running" />

// Order book - use position AND color
// Asks above spread, bids below (spatial relationship)
```

### 1.4 Resize Text (1.4.4)

**Requirement**: Text can be resized up to 200% without loss of functionality.

**Implementation**:

```css
/* Use relative units */
body {
  font-size: 100%; /* 16px base */
}

.card-title {
  font-size: 1.125rem; /* 18px */
}

.body-text {
  font-size: 0.875rem; /* 14px */
}

/* Ensure containers can expand */
.order-book {
  min-width: 300px;
  max-width: 100%;
}

/* Avoid fixed heights that clip text */
.table-cell {
  min-height: 2.5rem;
  height: auto;
}
```

---

## 2. Operable

### 2.1 Keyboard Accessible (2.1.1)

**Requirement**: All functionality must be available via keyboard.

**Implementation**:

```tsx
// Ensure all interactive elements are focusable
<button onClick={handleClick}>Submit</button>

// Custom components need tabIndex and key handlers
<div
  role="button"
  tabIndex={0}
  onClick={handleClick}
  onKeyDown={(e) => {
    if (e.key === 'Enter' || e.key === ' ') {
      e.preventDefault();
      handleClick();
    }
  }}
>
  Custom Button
</div>

// Order book price levels should be clickable via keyboard
<div
  role="button"
  tabIndex={0}
  onClick={() => onPriceClick(price)}
  onKeyDown={(e) => {
    if (e.key === 'Enter') onPriceClick(price);
  }}
>
  {formatPrice(price)}
</div>
```

**Keyboard Shortcuts**:

| Action | Shortcut | Context |
|--------|----------|---------|
| Submit order | Enter | Order form |
| Cancel order | Escape | Order form |
| Navigate tabs | Arrow keys | Tab navigation |
| Close modal | Escape | Any modal |
| Focus search | Ctrl+K / Cmd+K | Global |
| Toggle theme | Ctrl+Shift+T | Global |

### 2.2 Focus Visible (2.4.7)

**Requirement**: Keyboard focus indicator must be visible.

**Implementation**:

```css
/* Custom focus styles */
:focus-visible {
  outline: 2px solid var(--color-brand-primary);
  outline-offset: 2px;
}

/* Remove default outline only if custom is applied */
:focus:not(:focus-visible) {
  outline: none;
}

/* High contrast focus for dark mode */
[data-theme="dark"] :focus-visible {
  outline-color: #40a9ff;
}

/* Focus within for compound components */
.strategy-card:focus-within {
  box-shadow: 0 0 0 2px var(--color-brand-primary);
}
```

### 2.3 Focus Order (2.4.3)

**Requirement**: Focus order must be logical and intuitive.

**Implementation**:

```tsx
// Modal focus trap
function Modal({ isOpen, onClose, children }) {
  const modalRef = useRef<HTMLDivElement>(null);

  useEffect(() => {
    if (isOpen) {
      // Focus first focusable element
      const firstFocusable = modalRef.current?.querySelector(
        'button, [href], input, select, textarea, [tabindex]:not([tabindex="-1"])'
      );
      (firstFocusable as HTMLElement)?.focus();
    }
  }, [isOpen]);

  // Trap focus within modal
  const handleKeyDown = (e: KeyboardEvent) => {
    if (e.key === 'Tab') {
      // Implement focus trap logic
    }
    if (e.key === 'Escape') {
      onClose();
    }
  };

  return (
    <div
      ref={modalRef}
      role="dialog"
      aria-modal="true"
      onKeyDown={handleKeyDown}
    >
      {children}
    </div>
  );
}
```

### 2.4 Skip Links (2.4.1)

**Requirement**: Provide mechanism to skip repetitive content.

**Implementation**:

```tsx
// Skip link at top of page
function SkipLink() {
  return (
    <a
      href="#main-content"
      className="skip-link"
    >
      Skip to main content
    </a>
  );
}

// CSS
.skip-link {
  position: absolute;
  top: -40px;
  left: 0;
  background: var(--color-brand-primary);
  color: white;
  padding: 8px 16px;
  z-index: 100;
}

.skip-link:focus {
  top: 0;
}
```

### 2.5 Timing Adjustable (2.2.1)

**Requirement**: Users must be able to extend time limits.

**Implementation**:

```tsx
// Session timeout warning
function SessionTimeoutWarning({ expiresAt, onExtend }) {
  const [timeLeft, setTimeLeft] = useState(calculateTimeLeft(expiresAt));

  // Show warning 5 minutes before expiry
  if (timeLeft > 300) return null;

  return (
    <Alert
      type="warning"
      message={`Session expires in ${formatTime(timeLeft)}`}
      action={
        <Button onClick={onExtend}>
          Extend Session
        </Button>
      }
    />
  );
}

// Auto-refresh data with user control
function useAutoRefresh(fetchFn, interval = 5000) {
  const [autoRefresh, setAutoRefresh] = useState(true);

  useEffect(() => {
    if (!autoRefresh) return;
    const timer = setInterval(fetchFn, interval);
    return () => clearInterval(timer);
  }, [autoRefresh, fetchFn, interval]);

  return { autoRefresh, setAutoRefresh };
}
```

---

## 3. Understandable

### 3.1 Language (3.1.1)

**Requirement**: Page language must be programmatically determinable.

**Implementation**:

```html
<!DOCTYPE html>
<html lang="en">
  <head>
    <meta charset="UTF-8">
    <title>VeloZ Trading Platform</title>
  </head>
</html>
```

### 3.2 Error Identification (3.3.1)

**Requirement**: Input errors must be identified and described.

**Implementation**:

```tsx
// Form field with error
function FormField({ label, error, children, id }) {
  const errorId = `${id}-error`;

  return (
    <div className="form-field">
      <label htmlFor={id}>{label}</label>
      {React.cloneElement(children, {
        id,
        'aria-invalid': !!error,
        'aria-describedby': error ? errorId : undefined,
      })}
      {error && (
        <span id={errorId} className="error-message" role="alert">
          {error}
        </span>
      )}
    </div>
  );
}

// Usage
<FormField label="Quantity" error={errors.quantity} id="quantity">
  <input type="number" value={quantity} onChange={handleChange} />
</FormField>
```

### 3.3 Labels or Instructions (3.3.2)

**Requirement**: Labels or instructions must be provided for user input.

**Implementation**:

```tsx
// Input with label and help text
<Form.Item
  label="Price"
  help="Enter the limit price for your order"
  required
>
  <InputNumber
    placeholder="0.00"
    addonAfter="USDT"
  />
</Form.Item>

// Complex form with instructions
<Form>
  <Alert
    type="info"
    message="Order Form Instructions"
    description="Fill in all required fields. Market orders execute immediately at current price."
  />
  {/* Form fields */}
</Form>
```

### 3.4 Error Suggestion (3.3.3)

**Requirement**: Provide suggestions to correct errors when possible.

**Implementation**:

```tsx
// Validation with suggestions
function validateQuantity(value: string, minQty: number, maxQty: number) {
  const num = parseFloat(value);

  if (isNaN(num)) {
    return 'Please enter a valid number';
  }

  if (num < minQty) {
    return `Minimum quantity is ${minQty}. Please increase your quantity.`;
  }

  if (num > maxQty) {
    return `Maximum quantity is ${maxQty}. Please reduce your quantity.`;
  }

  return null;
}

// API error with suggestions
function handleOrderError(error: ApiError) {
  switch (error.code) {
    case 'INSUFFICIENT_BALANCE':
      return `Insufficient balance. You have ${error.available} available. Reduce quantity or add funds.`;
    case 'MIN_NOTIONAL':
      return `Order value too small. Minimum order value is ${error.minNotional} USDT.`;
    case 'PRICE_FILTER':
      return `Price must be a multiple of ${error.tickSize}. Try ${suggestValidPrice(error)}.`;
    default:
      return error.message;
  }
}
```

### 3.5 Error Prevention (3.3.4)

**Requirement**: For important actions, provide confirmation or ability to reverse.

**Implementation**:

```tsx
// Order confirmation modal
function OrderConfirmation({ order, onConfirm, onCancel }) {
  return (
    <Modal
      title="Confirm Order"
      visible={true}
      onOk={onConfirm}
      onCancel={onCancel}
    >
      <Alert
        type="warning"
        message="Please review your order carefully"
      />
      <Descriptions>
        <Descriptions.Item label="Side">{order.side}</Descriptions.Item>
        <Descriptions.Item label="Symbol">{order.symbol}</Descriptions.Item>
        <Descriptions.Item label="Quantity">{order.quantity}</Descriptions.Item>
        <Descriptions.Item label="Price">{order.price}</Descriptions.Item>
        <Descriptions.Item label="Total">{order.total}</Descriptions.Item>
      </Descriptions>
    </Modal>
  );
}

// Delete confirmation
function DeleteConfirmation({ itemName, onConfirm, onCancel }) {
  return (
    <Modal
      title="Confirm Delete"
      visible={true}
      okText="Delete"
      okButtonProps={{ danger: true }}
      onOk={onConfirm}
      onCancel={onCancel}
    >
      <p>Are you sure you want to delete "{itemName}"?</p>
      <p>This action cannot be undone.</p>
    </Modal>
  );
}
```

---

## 4. Robust

### 4.1 Parsing (4.1.1)

**Requirement**: Content must be parseable by assistive technologies.

**Implementation**:

```tsx
// Valid HTML structure
function App() {
  return (
    <>
      <header role="banner">
        <nav role="navigation" aria-label="Main navigation">
          {/* Navigation items */}
        </nav>
      </header>
      <main id="main-content" role="main">
        {/* Page content */}
      </main>
      <footer role="contentinfo">
        {/* Footer content */}
      </footer>
    </>
  );
}

// Unique IDs
function generateUniqueId(prefix: string) {
  return `${prefix}-${Math.random().toString(36).substr(2, 9)}`;
}
```

### 4.2 Name, Role, Value (4.1.2)

**Requirement**: Custom components must expose name, role, and value.

**Implementation**:

```tsx
// Custom toggle component
function Toggle({ checked, onChange, label }) {
  return (
    <button
      role="switch"
      aria-checked={checked}
      aria-label={label}
      onClick={() => onChange(!checked)}
    >
      <span className="toggle-track">
        <span className="toggle-thumb" />
      </span>
    </button>
  );
}

// Custom select/dropdown
function Dropdown({ value, options, onChange, label }) {
  const [isOpen, setIsOpen] = useState(false);
  const selectedOption = options.find(o => o.value === value);

  return (
    <div className="dropdown">
      <button
        aria-haspopup="listbox"
        aria-expanded={isOpen}
        aria-label={label}
        onClick={() => setIsOpen(!isOpen)}
      >
        {selectedOption?.label || 'Select...'}
      </button>
      {isOpen && (
        <ul role="listbox" aria-label={label}>
          {options.map(option => (
            <li
              key={option.value}
              role="option"
              aria-selected={option.value === value}
              onClick={() => {
                onChange(option.value);
                setIsOpen(false);
              }}
            >
              {option.label}
            </li>
          ))}
        </ul>
      )}
    </div>
  );
}

// Data table with proper semantics
function DataTable({ columns, data, caption }) {
  return (
    <table role="grid" aria-label={caption}>
      <caption className="sr-only">{caption}</caption>
      <thead>
        <tr role="row">
          {columns.map(col => (
            <th
              key={col.key}
              role="columnheader"
              scope="col"
              aria-sort={col.sortDirection}
            >
              {col.title}
            </th>
          ))}
        </tr>
      </thead>
      <tbody>
        {data.map((row, i) => (
          <tr key={i} role="row">
            {columns.map(col => (
              <td key={col.key} role="gridcell">
                {row[col.key]}
              </td>
            ))}
          </tr>
        ))}
      </tbody>
    </table>
  );
}
```

---

## 5. Screen Reader Announcements

### 5.1 Live Regions

**Implementation**:

```tsx
// Order status announcements
function OrderStatusAnnouncer() {
  const [announcement, setAnnouncement] = useState('');

  useEffect(() => {
    // Subscribe to order updates
    const unsubscribe = orderStore.subscribe((state) => {
      const latestUpdate = state.latestUpdate;
      if (latestUpdate) {
        setAnnouncement(
          `Order ${latestUpdate.clientOrderId} ${latestUpdate.status}`
        );
      }
    });
    return unsubscribe;
  }, []);

  return (
    <div
      role="status"
      aria-live="polite"
      aria-atomic="true"
      className="sr-only"
    >
      {announcement}
    </div>
  );
}

// Price update announcements (throttled)
function PriceAnnouncer({ symbol, price }) {
  const [announcement, setAnnouncement] = useState('');
  const lastAnnouncedRef = useRef(0);

  useEffect(() => {
    // Only announce significant changes (>0.1%)
    const change = Math.abs(price - lastAnnouncedRef.current) / lastAnnouncedRef.current;
    if (change > 0.001) {
      setAnnouncement(`${symbol} price: ${formatPrice(price)}`);
      lastAnnouncedRef.current = price;
    }
  }, [symbol, price]);

  return (
    <div
      role="status"
      aria-live="polite"
      aria-atomic="true"
      className="sr-only"
    >
      {announcement}
    </div>
  );
}

// Error announcements
function ErrorAnnouncer({ errors }) {
  return (
    <div
      role="alert"
      aria-live="assertive"
      className="sr-only"
    >
      {errors.map((error, i) => (
        <p key={i}>{error}</p>
      ))}
    </div>
  );
}
```

### 5.2 Screen Reader Only Text

```css
/* Visually hidden but accessible */
.sr-only {
  position: absolute;
  width: 1px;
  height: 1px;
  padding: 0;
  margin: -1px;
  overflow: hidden;
  clip: rect(0, 0, 0, 0);
  white-space: nowrap;
  border: 0;
}

/* Show on focus (for skip links) */
.sr-only-focusable:focus {
  position: static;
  width: auto;
  height: auto;
  padding: inherit;
  margin: inherit;
  overflow: visible;
  clip: auto;
  white-space: normal;
}
```

---

## 6. Motion and Animation

### 6.1 Reduced Motion

**Requirement**: Respect user's motion preferences.

**Implementation**:

```css
/* Reduce motion when requested */
@media (prefers-reduced-motion: reduce) {
  *,
  *::before,
  *::after {
    animation-duration: 0.01ms !important;
    animation-iteration-count: 1 !important;
    transition-duration: 0.01ms !important;
    scroll-behavior: auto !important;
  }
}

/* Specific component adjustments */
@media (prefers-reduced-motion: reduce) {
  .orderbook-row {
    transition: none;
  }

  .level-new,
  .level-updated {
    animation: none;
  }

  .pulse {
    animation: none;
  }
}
```

```tsx
// React hook for motion preference
function usePrefersReducedMotion() {
  const [prefersReducedMotion, setPrefersReducedMotion] = useState(
    window.matchMedia('(prefers-reduced-motion: reduce)').matches
  );

  useEffect(() => {
    const mediaQuery = window.matchMedia('(prefers-reduced-motion: reduce)');
    const handler = (e: MediaQueryListEvent) => {
      setPrefersReducedMotion(e.matches);
    };
    mediaQuery.addEventListener('change', handler);
    return () => mediaQuery.removeEventListener('change', handler);
  }, []);

  return prefersReducedMotion;
}

// Usage
function AnimatedComponent() {
  const prefersReducedMotion = usePrefersReducedMotion();

  return (
    <div
      className={prefersReducedMotion ? 'no-animation' : 'with-animation'}
    >
      {/* Content */}
    </div>
  );
}
```

---

## 7. Testing Checklist

### 7.1 Automated Testing

- [ ] Run axe-core on all pages
- [ ] Run Lighthouse accessibility audit
- [ ] Validate HTML with W3C validator
- [ ] Check color contrast with WebAIM tool

### 7.2 Manual Testing

- [ ] Navigate entire app with keyboard only
- [ ] Test with screen reader (NVDA, VoiceOver)
- [ ] Test at 200% zoom level
- [ ] Test with high contrast mode
- [ ] Test with reduced motion preference
- [ ] Test focus visibility
- [ ] Test form error handling
- [ ] Test modal focus trapping

### 7.3 Screen Reader Testing Script

```
1. Navigate to login page
   - Verify form fields are announced
   - Verify error messages are announced

2. Navigate to trading page
   - Verify order book data is accessible
   - Verify price updates are announced (throttled)
   - Verify order form labels are clear

3. Navigate to strategies page
   - Verify strategy cards are navigable
   - Verify status badges are announced
   - Verify action buttons are labeled

4. Navigate to backtest page
   - Verify results table is navigable
   - Verify chart has description

5. Navigate to audit page (admin)
   - Verify table is navigable
   - Verify filters are accessible
```

---

## 8. Implementation Priority

### Phase 1 (Critical)
- [ ] Keyboard navigation for all interactive elements
- [ ] Focus indicators
- [ ] Form labels and error messages
- [ ] Color contrast compliance
- [ ] Skip links

### Phase 2 (High)
- [ ] Screen reader announcements for updates
- [ ] Modal focus trapping
- [ ] ARIA attributes for custom components
- [ ] Reduced motion support

### Phase 3 (Medium)
- [ ] Comprehensive screen reader testing
- [ ] High contrast mode support
- [ ] Touch target sizes (mobile)
- [ ] Automated accessibility testing in CI
