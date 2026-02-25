# P0-5: Security Education System

**Version**: 1.0.0
**Date**: 2026-02-25
**Status**: Design Phase
**Complexity**: Medium
**Estimated Time**: 2-3 weeks

---

## 1. Overview

### 1.1 Goals

- Educate new users about trading risks and security best practices
- Require simulated trading period before live trading
- Implement mandatory loss limits and safety checks
- Detect and alert on abnormal trading patterns
- Provide interactive safety checklists

### 1.2 Non-Goals

- Comprehensive trading education/courses
- Financial advice or recommendations
- Regulatory compliance certification
- Social trading features

---

## 2. Architecture

### 2.1 High-Level Architecture

```
                    Security Education Architecture
                    ================================

+------------------------------------------------------------------+
|                         React UI Layer                            |
+------------------------------------------------------------------+
|  +----------------+  +----------------+  +----------------+       |
|  | Onboarding     |  | Safety         |  | Alert          |       |
|  | Wizard         |  | Dashboard      |  | Center         |       |
|  +----------------+  +----------------+  +----------------+       |
|           |                  |                  |                 |
|           v                  v                  v                 |
|  +----------------+  +----------------+  +----------------+       |
|  | Progress       |  | Checklist      |  | Notification   |       |
|  | Tracker        |  | Manager        |  | Manager        |       |
|  +----------------+  +----------------+  +----------------+       |
+------------------------------------------------------------------+
                               |
                               v (REST API)
+------------------------------------------------------------------+
|                      Python Gateway Layer                         |
+------------------------------------------------------------------+
|  +----------------+  +----------------+  +----------------+       |
|  | User Progress  |  | Trading Mode   |  | Anomaly        |       |
|  | Service        |  | Controller     |  | Detector       |       |
|  +----------------+  +----------------+  +----------------+       |
|           |                  |                  |                 |
|           v                  v                  v                 |
|  +----------------+  +----------------+  +----------------+       |
|  | Progress       |  | Mode State     |  | Alert          |       |
|  | Database       |  | Machine        |  | Engine         |       |
|  +----------------+  +----------------+  +----------------+       |
+------------------------------------------------------------------+
                               |
                               v (stdio)
+------------------------------------------------------------------+
|                       C++ Engine Layer                            |
+------------------------------------------------------------------+
|  +----------------+  +----------------+  +----------------+       |
|  | Simulated      |  | Loss Limit     |  | Circuit        |       |
|  | Trading Engine |  | Enforcer       |  | Breaker        |       |
|  +----------------+  +----------------+  +----------------+       |
+------------------------------------------------------------------+
```

### 2.2 User Journey Flow

```
                        User Safety Journey
                        ====================

New User Registration
         |
         v
+------------------+
| 1. Welcome &     |
|    Risk Warning  |
+------------------+
         |
         v
+------------------+
| 2. Security      |
|    Checklist     |
+------------------+
         |
         v
+------------------+
| 3. Simulated     |
|    Trading Mode  |
|    (Required)    |
+------------------+
         |
         v (After requirements met)
+------------------+
| 4. Paper Trading |
|    Mode          |
|    (Optional)    |
+------------------+
         |
         v (User request + checklist complete)
+------------------+
| 5. Live Trading  |
|    Mode          |
|    (With limits) |
+------------------+
```

---

## 3. Onboarding Wizard

### 3.1 Wizard Steps

```
                        Onboarding Wizard Flow
                        ======================

Step 1: Welcome
+------------------------------------------------------------------+
|                                                                   |
|                    Welcome to VeloZ                               |
|                                                                   |
|  VeloZ is a powerful trading platform. Before you begin,         |
|  please understand the risks involved in trading.                |
|                                                                   |
|  +------------------------------------------------------------+  |
|  |  WARNING: Trading cryptocurrencies involves significant     |  |
|  |  risk of loss. You could lose some or all of your          |  |
|  |  investment. Only trade with money you can afford to lose. |  |
|  +------------------------------------------------------------+  |
|                                                                   |
|  [ ] I understand that trading involves risk of loss             |
|  [ ] I am trading with funds I can afford to lose                |
|  [ ] I am not using borrowed money to trade                      |
|                                                                   |
|                                              [Continue ->]        |
|                                                                   |
+------------------------------------------------------------------+

Step 2: Security Fundamentals
+------------------------------------------------------------------+
|                                                                   |
|                    Security Fundamentals                          |
|                                                                   |
|  1. API Key Security                                              |
|     - Never share your API keys                                   |
|     - Never enable withdrawal permissions                         |
|     - Use IP restrictions when possible                           |
|                                                                   |
|  2. Account Security                                              |
|     - Use a strong, unique password                               |
|     - Enable 2FA on your exchange account                         |
|     - Never share your login credentials                          |
|                                                                   |
|  3. Safe Trading Practices                                        |
|     - Start with small amounts                                    |
|     - Always use stop losses                                      |
|     - Never invest more than you can afford to lose               |
|                                                                   |
|  [ ] I have read and understood these security guidelines        |
|                                                                   |
|  [<- Back]                                    [Continue ->]       |
|                                                                   |
+------------------------------------------------------------------+

Step 3: Loss Limits Setup
+------------------------------------------------------------------+
|                                                                   |
|                    Configure Loss Limits                          |
|                                                                   |
|  Loss limits protect you from excessive losses. These are        |
|  mandatory and cannot be disabled.                               |
|                                                                   |
|  Daily Loss Limit                                                 |
|  [====|==========] 5%                                            |
|  Trading pauses when daily loss exceeds this percentage          |
|                                                                   |
|  Maximum Position Size                                            |
|  [====|==========] 10%                                           |
|  Maximum size of any single position                             |
|                                                                   |
|  Circuit Breaker Threshold                                        |
|  [====|==========] 10%                                           |
|  Emergency stop when total loss exceeds this percentage          |
|                                                                   |
|  +------------------------------------------------------------+  |
|  |  These limits will be enforced automatically. You can       |  |
|  |  adjust them later in Settings, but cannot disable them.   |  |
|  +------------------------------------------------------------+  |
|                                                                   |
|  [<- Back]                                    [Continue ->]       |
|                                                                   |
+------------------------------------------------------------------+

Step 4: Simulated Trading Introduction
+------------------------------------------------------------------+
|                                                                   |
|                    Start with Simulated Trading                   |
|                                                                   |
|  Before trading with real money, you'll practice with            |
|  simulated trading. This uses real market data but virtual       |
|  funds.                                                          |
|                                                                   |
|  Requirements to unlock live trading:                            |
|                                                                   |
|  [ ] Complete at least 10 simulated trades                       |
|  [ ] Trade for at least 7 days                                   |
|  [ ] Complete the security checklist                             |
|  [ ] Demonstrate understanding of loss limits                    |
|                                                                   |
|  Simulated Balance: $10,000 USD                                  |
|                                                                   |
|  +------------------------------------------------------------+  |
|  |  Simulated trading is risk-free. Use this time to learn    |  |
|  |  the platform and test your strategies.                    |  |
|  +------------------------------------------------------------+  |
|                                                                   |
|  [<- Back]                            [Start Simulated Trading]  |
|                                                                   |
+------------------------------------------------------------------+
```

### 3.2 Onboarding Component

```typescript
// src/features/onboarding/OnboardingWizard.tsx
interface OnboardingStep {
  id: string;
  title: string;
  component: React.ComponentType<StepProps>;
  validation: (data: OnboardingData) => boolean;
  canSkip: boolean;
}

const ONBOARDING_STEPS: OnboardingStep[] = [
  {
    id: 'welcome',
    title: 'Welcome',
    component: WelcomeStep,
    validation: (data) =>
      data.acknowledgedRisk &&
      data.acknowledgedAffordability &&
      data.acknowledgedNotBorrowed,
    canSkip: false,
  },
  {
    id: 'security',
    title: 'Security Fundamentals',
    component: SecurityStep,
    validation: (data) => data.acknowledgedSecurity,
    canSkip: false,
  },
  {
    id: 'limits',
    title: 'Loss Limits',
    component: LossLimitsStep,
    validation: (data) =>
      data.dailyLossLimit > 0 &&
      data.maxPositionSize > 0 &&
      data.circuitBreakerThreshold > 0,
    canSkip: false,
  },
  {
    id: 'simulated',
    title: 'Simulated Trading',
    component: SimulatedTradingStep,
    validation: () => true,
    canSkip: false,
  },
];

export function OnboardingWizard() {
  const [currentStep, setCurrentStep] = useState(0);
  const [data, setData] = useState<OnboardingData>(getDefaultOnboardingData());
  const { completeOnboarding } = useOnboardingStore();

  const step = ONBOARDING_STEPS[currentStep];
  const StepComponent = step.component;

  const canProceed = step.validation(data);

  const handleNext = async () => {
    if (currentStep === ONBOARDING_STEPS.length - 1) {
      await completeOnboarding(data);
      // Redirect to dashboard in simulated mode
      navigate('/dashboard');
    } else {
      setCurrentStep(currentStep + 1);
    }
  };

  return (
    <div className="onboarding-wizard">
      <div className="progress">
        {ONBOARDING_STEPS.map((s, i) => (
          <div
            key={s.id}
            className={`step ${i <= currentStep ? 'active' : ''}`}
          >
            {s.title}
          </div>
        ))}
      </div>

      <div className="step-content">
        <StepComponent
          data={data}
          onChange={(updates) => setData({ ...data, ...updates })}
        />
      </div>

      <div className="actions">
        {currentStep > 0 && (
          <Button onClick={() => setCurrentStep(currentStep - 1)}>
            Back
          </Button>
        )}
        <Button
          type="primary"
          onClick={handleNext}
          disabled={!canProceed}
        >
          {currentStep === ONBOARDING_STEPS.length - 1
            ? 'Start Simulated Trading'
            : 'Continue'}
        </Button>
      </div>
    </div>
  );
}
```

---

## 4. Safety Checklists

### 4.1 Checklist Categories

| Category | Items | Required for Live Trading |
|----------|-------|---------------------------|
| Account Security | 2FA, strong password, recovery email | Yes |
| API Key Security | No withdrawal, IP restriction, testnet first | Yes |
| Risk Management | Loss limits, position limits, circuit breaker | Yes |
| Trading Knowledge | Complete simulated trades, understand order types | Yes |
| Platform Familiarity | Navigate UI, place orders, view positions | Yes |

### 4.2 Checklist Data Model

```typescript
// src/types/checklist.ts
export interface ChecklistItem {
  id: string;
  category: ChecklistCategory;
  title: string;
  description: string;
  helpUrl?: string;
  autoVerify: boolean;  // Can be verified automatically
  required: boolean;    // Required for live trading
  completed: boolean;
  completedAt?: string;
  verificationMethod?: 'manual' | 'api' | 'action';
}

export interface ChecklistCategory {
  id: string;
  title: string;
  description: string;
  order: number;
  icon: string;
}

export const CHECKLIST_ITEMS: ChecklistItem[] = [
  // Account Security
  {
    id: 'exchange_2fa',
    category: 'account_security',
    title: 'Enable 2FA on Exchange',
    description: 'Enable two-factor authentication on your exchange account',
    helpUrl: '/help/2fa-setup',
    autoVerify: false,
    required: true,
    completed: false,
    verificationMethod: 'manual',
  },
  {
    id: 'strong_password',
    category: 'account_security',
    title: 'Use Strong Password',
    description: 'Ensure your VeloZ password is strong and unique',
    autoVerify: true,
    required: true,
    completed: false,
    verificationMethod: 'api',
  },

  // API Key Security
  {
    id: 'no_withdrawal',
    category: 'api_security',
    title: 'No Withdrawal Permission',
    description: 'Verify your API key does not have withdrawal permissions',
    autoVerify: true,
    required: true,
    completed: false,
    verificationMethod: 'api',
  },
  {
    id: 'ip_restriction',
    category: 'api_security',
    title: 'Enable IP Restriction',
    description: 'Restrict your API key to specific IP addresses',
    helpUrl: '/help/ip-restriction',
    autoVerify: false,
    required: false,
    completed: false,
    verificationMethod: 'manual',
  },

  // Risk Management
  {
    id: 'loss_limits_configured',
    category: 'risk_management',
    title: 'Configure Loss Limits',
    description: 'Set up daily loss limits and circuit breaker',
    autoVerify: true,
    required: true,
    completed: false,
    verificationMethod: 'api',
  },
  {
    id: 'position_limits_configured',
    category: 'risk_management',
    title: 'Configure Position Limits',
    description: 'Set maximum position size and value limits',
    autoVerify: true,
    required: true,
    completed: false,
    verificationMethod: 'api',
  },

  // Trading Knowledge
  {
    id: 'simulated_trades',
    category: 'trading_knowledge',
    title: 'Complete 10 Simulated Trades',
    description: 'Practice trading with simulated funds',
    autoVerify: true,
    required: true,
    completed: false,
    verificationMethod: 'action',
  },
  {
    id: 'trading_days',
    category: 'trading_knowledge',
    title: 'Trade for 7 Days',
    description: 'Use the platform for at least 7 days',
    autoVerify: true,
    required: true,
    completed: false,
    verificationMethod: 'action',
  },
];
```

### 4.3 Checklist UI Component

```typescript
// src/features/safety/SafetyChecklist.tsx
export function SafetyChecklist() {
  const { items, categories, completeItem, progress } = useChecklistStore();

  const groupedItems = useMemo(
    () => groupByCategory(items, categories),
    [items, categories]
  );

  return (
    <div className="safety-checklist">
      <div className="header">
        <h2>Safety Checklist</h2>
        <Progress
          percent={progress.percentage}
          status={progress.allRequired ? 'success' : 'active'}
        />
        <p>
          {progress.completed} of {progress.total} items completed
          {!progress.allRequired && (
            <span className="required-note">
              ({progress.requiredRemaining} required items remaining)
            </span>
          )}
        </p>
      </div>

      {groupedItems.map((group) => (
        <Card key={group.category.id} title={group.category.title}>
          <p className="category-description">{group.category.description}</p>

          <List
            dataSource={group.items}
            renderItem={(item) => (
              <List.Item
                actions={[
                  item.completed ? (
                    <CheckCircleOutlined className="completed-icon" />
                  ) : item.autoVerify ? (
                    <Button
                      size="small"
                      onClick={() => verifyItem(item.id)}
                    >
                      Verify
                    </Button>
                  ) : (
                    <Checkbox
                      checked={item.completed}
                      onChange={() => completeItem(item.id)}
                    >
                      Mark Complete
                    </Checkbox>
                  ),
                ]}
              >
                <List.Item.Meta
                  title={
                    <span>
                      {item.title}
                      {item.required && (
                        <Tag color="red" size="small">Required</Tag>
                      )}
                    </span>
                  }
                  description={item.description}
                />
                {item.helpUrl && (
                  <a href={item.helpUrl} target="_blank">
                    Learn more
                  </a>
                )}
              </List.Item>
            )}
          />
        </Card>
      ))}

      {progress.allRequired && (
        <Alert
          type="success"
          message="All required items completed!"
          description="You can now request to enable live trading."
          action={
            <Button type="primary" onClick={requestLiveTrading}>
              Request Live Trading
            </Button>
          }
        />
      )}
    </div>
  );
}
```

---

## 5. Trading Mode System

### 5.1 Trading Modes

| Mode | Description | Restrictions |
|------|-------------|--------------|
| **Simulated** | Virtual funds, real market data | Cannot lose real money |
| **Paper** | Real API, no real orders | Orders simulated locally |
| **Live** | Real trading with real funds | Subject to loss limits |

### 5.2 Mode State Machine

```
                    Trading Mode State Machine
                    ==========================

+------------------+
| Simulated        |
| (Default)        |
+------------------+
        |
        | Requirements met:
        | - 10+ trades
        | - 7+ days
        | - Checklist complete
        v
+------------------+
| Paper Trading    |
| (Optional)       |
+------------------+
        |
        | User request +
        | Final confirmation
        v
+------------------+
| Live Trading     |
| (With limits)    |
+------------------+
        |
        | Circuit breaker
        | triggered
        v
+------------------+
| Locked           |
| (Cooldown)       |
+------------------+
        |
        | Cooldown expired +
        | User acknowledgment
        v
+------------------+
| Live Trading     |
| (Resumed)        |
+------------------+
```

### 5.3 Mode Controller

```python
# apps/gateway/trading_mode.py
from enum import Enum
from dataclasses import dataclass
from datetime import datetime, timedelta
from typing import Optional

class TradingMode(Enum):
    SIMULATED = 'simulated'
    PAPER = 'paper'
    LIVE = 'live'
    LOCKED = 'locked'

@dataclass
class ModeRequirements:
    min_simulated_trades: int = 10
    min_trading_days: int = 7
    checklist_complete: bool = True

@dataclass
class UserTradingState:
    user_id: str
    current_mode: TradingMode
    simulated_trades: int
    first_trade_date: Optional[datetime]
    checklist_progress: dict
    live_trading_requested: bool
    locked_until: Optional[datetime]
    lock_reason: Optional[str]

class TradingModeController:
    def __init__(self, requirements: ModeRequirements = None):
        self.requirements = requirements or ModeRequirements()

    def can_upgrade_to_paper(self, state: UserTradingState) -> tuple[bool, list[str]]:
        """Check if user can upgrade from simulated to paper trading"""
        blockers = []

        if state.simulated_trades < self.requirements.min_simulated_trades:
            blockers.append(
                f"Complete {self.requirements.min_simulated_trades - state.simulated_trades} "
                f"more simulated trades"
            )

        if state.first_trade_date:
            days_trading = (datetime.utcnow() - state.first_trade_date).days
            if days_trading < self.requirements.min_trading_days:
                blockers.append(
                    f"Trade for {self.requirements.min_trading_days - days_trading} more days"
                )
        else:
            blockers.append(f"Trade for {self.requirements.min_trading_days} days")

        required_items = [k for k, v in state.checklist_progress.items() if v.get('required')]
        completed_required = [k for k in required_items if state.checklist_progress[k].get('completed')]
        if len(completed_required) < len(required_items):
            blockers.append(
                f"Complete {len(required_items) - len(completed_required)} "
                f"required checklist items"
            )

        return len(blockers) == 0, blockers

    def can_upgrade_to_live(self, state: UserTradingState) -> tuple[bool, list[str]]:
        """Check if user can upgrade from paper to live trading"""
        can_paper, blockers = self.can_upgrade_to_paper(state)
        if not can_paper:
            return False, blockers

        if state.current_mode == TradingMode.LOCKED:
            if state.locked_until and datetime.utcnow() < state.locked_until:
                blockers.append(
                    f"Account locked until {state.locked_until.isoformat()}. "
                    f"Reason: {state.lock_reason}"
                )
                return False, blockers

        return True, []

    def request_live_trading(self, state: UserTradingState) -> tuple[bool, str]:
        """Process request to enable live trading"""
        can_upgrade, blockers = self.can_upgrade_to_live(state)

        if not can_upgrade:
            return False, "Cannot enable live trading: " + "; ".join(blockers)

        # Require explicit confirmation
        return True, "live_trading_confirmation_required"

    def lock_account(
        self,
        state: UserTradingState,
        reason: str,
        duration_minutes: int = 60
    ) -> UserTradingState:
        """Lock account due to circuit breaker or anomaly"""
        state.current_mode = TradingMode.LOCKED
        state.locked_until = datetime.utcnow() + timedelta(minutes=duration_minutes)
        state.lock_reason = reason
        return state

    def unlock_account(self, state: UserTradingState) -> tuple[bool, str]:
        """Attempt to unlock account after cooldown"""
        if state.current_mode != TradingMode.LOCKED:
            return False, "Account is not locked"

        if state.locked_until and datetime.utcnow() < state.locked_until:
            remaining = (state.locked_until - datetime.utcnow()).total_seconds() / 60
            return False, f"Account locked for {remaining:.0f} more minutes"

        state.current_mode = TradingMode.LIVE
        state.locked_until = None
        state.lock_reason = None
        return True, "Account unlocked"
```

---

## 6. Loss Limits and Circuit Breaker

### 6.1 Loss Limit Enforcement

```
                    Loss Limit Enforcement Flow
                    ===========================

Order Request
      |
      v
+------------------+
| Pre-trade Check  |
| - Position limit |
| - Order value    |
+------------------+
      |
      v
+------------------+
| Execute Order    |
+------------------+
      |
      v
+------------------+
| Post-trade Check |
| - Update P&L     |
| - Check limits   |
+------------------+
      |
      +------------------+------------------+
      |                  |                  |
      v                  v                  v
+----------+      +----------+      +----------+
| OK       |      | Warning  |      | Breach   |
| Continue |      | Alert    |      | Lock     |
+----------+      +----------+      +----------+
```

### 6.2 Loss Limit Engine

```python
# apps/gateway/loss_limits.py
from dataclasses import dataclass
from typing import Optional
from datetime import datetime, date

@dataclass
class LossLimitConfig:
    daily_loss_limit_pct: float  # e.g., 0.05 for 5%
    max_position_size_pct: float  # e.g., 0.10 for 10%
    max_position_value: float  # e.g., 10000 USD
    circuit_breaker_threshold_pct: float  # e.g., 0.10 for 10%
    circuit_breaker_cooldown_minutes: int  # e.g., 60

@dataclass
class TradingState:
    total_equity: float
    daily_pnl: float
    total_pnl: float
    open_position_value: float
    last_reset_date: date

class LossLimitEngine:
    def __init__(self, config: LossLimitConfig):
        self.config = config

    def check_order(
        self,
        state: TradingState,
        order_value: float
    ) -> tuple[bool, Optional[str]]:
        """Check if order can be placed within limits"""

        # Check position size limit
        new_position_value = state.open_position_value + order_value
        max_position = state.total_equity * self.config.max_position_size_pct

        if new_position_value > max_position:
            return False, (
                f"Order would exceed position size limit. "
                f"Current: ${state.open_position_value:.2f}, "
                f"Order: ${order_value:.2f}, "
                f"Limit: ${max_position:.2f}"
            )

        # Check absolute position value limit
        if new_position_value > self.config.max_position_value:
            return False, (
                f"Order would exceed maximum position value. "
                f"Limit: ${self.config.max_position_value:.2f}"
            )

        return True, None

    def check_daily_limit(
        self,
        state: TradingState
    ) -> tuple[bool, Optional[str], float]:
        """Check if daily loss limit has been breached"""

        daily_limit = state.total_equity * self.config.daily_loss_limit_pct
        daily_loss = -state.daily_pnl if state.daily_pnl < 0 else 0

        if daily_loss >= daily_limit:
            return False, (
                f"Daily loss limit reached. "
                f"Loss: ${daily_loss:.2f}, "
                f"Limit: ${daily_limit:.2f}"
            ), 0

        remaining = daily_limit - daily_loss
        usage_pct = daily_loss / daily_limit if daily_limit > 0 else 0

        # Warning at 80%
        if usage_pct >= 0.8:
            return True, (
                f"Warning: Daily loss at {usage_pct*100:.0f}% of limit. "
                f"Remaining: ${remaining:.2f}"
            ), remaining

        return True, None, remaining

    def check_circuit_breaker(
        self,
        state: TradingState
    ) -> tuple[bool, Optional[str]]:
        """Check if circuit breaker should trigger"""

        threshold = state.total_equity * self.config.circuit_breaker_threshold_pct
        total_loss = -state.total_pnl if state.total_pnl < 0 else 0

        if total_loss >= threshold:
            return False, (
                f"Circuit breaker triggered! "
                f"Total loss: ${total_loss:.2f}, "
                f"Threshold: ${threshold:.2f}. "
                f"Trading locked for {self.config.circuit_breaker_cooldown_minutes} minutes."
            )

        return True, None

    def get_status(self, state: TradingState) -> dict:
        """Get current loss limit status for display"""

        daily_limit = state.total_equity * self.config.daily_loss_limit_pct
        daily_loss = -state.daily_pnl if state.daily_pnl < 0 else 0
        daily_usage = daily_loss / daily_limit if daily_limit > 0 else 0

        circuit_threshold = state.total_equity * self.config.circuit_breaker_threshold_pct
        total_loss = -state.total_pnl if state.total_pnl < 0 else 0
        circuit_usage = total_loss / circuit_threshold if circuit_threshold > 0 else 0

        position_limit = state.total_equity * self.config.max_position_size_pct
        position_usage = state.open_position_value / position_limit if position_limit > 0 else 0

        return {
            'daily_loss': {
                'current': daily_loss,
                'limit': daily_limit,
                'usage_pct': daily_usage,
                'status': 'danger' if daily_usage >= 1 else 'warning' if daily_usage >= 0.8 else 'ok',
            },
            'circuit_breaker': {
                'current': total_loss,
                'threshold': circuit_threshold,
                'usage_pct': circuit_usage,
                'status': 'danger' if circuit_usage >= 1 else 'warning' if circuit_usage >= 0.8 else 'ok',
            },
            'position': {
                'current': state.open_position_value,
                'limit': position_limit,
                'usage_pct': position_usage,
                'status': 'danger' if position_usage >= 1 else 'warning' if position_usage >= 0.8 else 'ok',
            },
        }
```

---

## 7. Anomaly Detection

### 7.1 Anomaly Types

| Anomaly | Description | Action |
|---------|-------------|--------|
| Rapid trading | Many orders in short time | Warning + rate limit |
| Large position | Position exceeds normal size | Block + alert |
| Unusual hours | Trading at unusual times | Notification |
| Loss streak | Multiple consecutive losses | Warning + suggestion |
| Strategy deviation | Behavior differs from strategy | Alert |

### 7.2 Anomaly Detector

```python
# apps/gateway/anomaly_detector.py
from dataclasses import dataclass
from typing import List, Optional
from datetime import datetime, timedelta
from collections import deque

@dataclass
class AnomalyAlert:
    type: str
    severity: str  # 'info', 'warning', 'critical'
    message: str
    timestamp: datetime
    action: Optional[str]  # 'none', 'warn', 'block', 'lock'

class AnomalyDetector:
    def __init__(self):
        self.order_history = deque(maxlen=1000)
        self.trade_history = deque(maxlen=1000)

    def on_order(self, order: dict) -> Optional[AnomalyAlert]:
        """Check for anomalies on new order"""
        alerts = []

        # Check rapid trading
        rapid_alert = self._check_rapid_trading(order)
        if rapid_alert:
            alerts.append(rapid_alert)

        # Check unusual hours
        hours_alert = self._check_unusual_hours(order)
        if hours_alert:
            alerts.append(hours_alert)

        self.order_history.append({
            'timestamp': datetime.utcnow(),
            'order': order,
        })

        return alerts[0] if alerts else None

    def on_trade(self, trade: dict) -> Optional[AnomalyAlert]:
        """Check for anomalies on trade execution"""
        alerts = []

        # Check loss streak
        streak_alert = self._check_loss_streak(trade)
        if streak_alert:
            alerts.append(streak_alert)

        self.trade_history.append({
            'timestamp': datetime.utcnow(),
            'trade': trade,
        })

        return alerts[0] if alerts else None

    def _check_rapid_trading(self, order: dict) -> Optional[AnomalyAlert]:
        """Detect rapid trading pattern"""
        one_minute_ago = datetime.utcnow() - timedelta(minutes=1)
        recent_orders = [
            o for o in self.order_history
            if o['timestamp'] > one_minute_ago
        ]

        if len(recent_orders) >= 10:
            return AnomalyAlert(
                type='rapid_trading',
                severity='warning',
                message=f"Rapid trading detected: {len(recent_orders)} orders in last minute",
                timestamp=datetime.utcnow(),
                action='warn',
            )

        if len(recent_orders) >= 20:
            return AnomalyAlert(
                type='rapid_trading',
                severity='critical',
                message=f"Excessive trading: {len(recent_orders)} orders in last minute. Rate limiting applied.",
                timestamp=datetime.utcnow(),
                action='block',
            )

        return None

    def _check_unusual_hours(self, order: dict) -> Optional[AnomalyAlert]:
        """Detect trading at unusual hours"""
        hour = datetime.utcnow().hour

        # Define unusual hours (e.g., 2-6 AM local time)
        # This should be configurable per user
        if 2 <= hour <= 6:
            return AnomalyAlert(
                type='unusual_hours',
                severity='info',
                message="Trading at unusual hours. Please ensure this is intentional.",
                timestamp=datetime.utcnow(),
                action='none',
            )

        return None

    def _check_loss_streak(self, trade: dict) -> Optional[AnomalyAlert]:
        """Detect consecutive losing trades"""
        recent_trades = list(self.trade_history)[-10:]
        losses = [t for t in recent_trades if t['trade'].get('pnl', 0) < 0]

        if len(losses) >= 5:
            return AnomalyAlert(
                type='loss_streak',
                severity='warning',
                message=f"Loss streak detected: {len(losses)} consecutive losing trades. Consider pausing to review your strategy.",
                timestamp=datetime.utcnow(),
                action='warn',
            )

        return None
```

---

## 8. Alert Center

### 8.1 Alert UI

```
+------------------------------------------------------------------+
|  Alert Center                                    [Mark All Read]  |
+------------------------------------------------------------------+
|                                                                   |
|  Today                                                            |
|  +--------------------------------------------------------------+ |
|  | [!] Daily Loss Warning                           2 hours ago | |
|  |     Daily loss at 80% of limit. Remaining: $500.00           | |
|  |     [View Details] [Adjust Limits]                           | |
|  +--------------------------------------------------------------+ |
|  | [i] Unusual Trading Hours                        4 hours ago | |
|  |     Trading at unusual hours. Please ensure intentional.     | |
|  |     [Dismiss]                                                | |
|  +--------------------------------------------------------------+ |
|                                                                   |
|  Yesterday                                                        |
|  +--------------------------------------------------------------+ |
|  | [!] Loss Streak Detected                        18 hours ago | |
|  |     5 consecutive losing trades. Consider reviewing strategy.| |
|  |     [View Trades] [Pause Trading]                            | |
|  +--------------------------------------------------------------+ |
|                                                                   |
+------------------------------------------------------------------+
```

### 8.2 Alert Component

```typescript
// src/features/safety/AlertCenter.tsx
interface Alert {
  id: string;
  type: string;
  severity: 'info' | 'warning' | 'critical';
  message: string;
  timestamp: string;
  read: boolean;
  actions: AlertAction[];
}

interface AlertAction {
  label: string;
  action: string;
  primary?: boolean;
}

export function AlertCenter() {
  const { alerts, markRead, markAllRead } = useAlertStore();
  const unreadCount = alerts.filter((a) => !a.read).length;

  const groupedAlerts = useMemo(
    () => groupByDate(alerts),
    [alerts]
  );

  return (
    <div className="alert-center">
      <div className="header">
        <h2>
          Alert Center
          {unreadCount > 0 && (
            <Badge count={unreadCount} />
          )}
        </h2>
        <Button onClick={markAllRead}>Mark All Read</Button>
      </div>

      {groupedAlerts.map((group) => (
        <div key={group.date} className="alert-group">
          <h3>{group.label}</h3>

          {group.alerts.map((alert) => (
            <Card
              key={alert.id}
              className={`alert-card ${alert.severity} ${alert.read ? 'read' : ''}`}
              onClick={() => markRead(alert.id)}
            >
              <div className="alert-icon">
                {alert.severity === 'critical' && <ExclamationCircleOutlined />}
                {alert.severity === 'warning' && <WarningOutlined />}
                {alert.severity === 'info' && <InfoCircleOutlined />}
              </div>

              <div className="alert-content">
                <div className="alert-title">{getAlertTitle(alert.type)}</div>
                <div className="alert-message">{alert.message}</div>
                <div className="alert-time">
                  {formatRelativeTime(alert.timestamp)}
                </div>
              </div>

              <div className="alert-actions">
                {alert.actions.map((action) => (
                  <Button
                    key={action.action}
                    type={action.primary ? 'primary' : 'default'}
                    size="small"
                    onClick={(e) => {
                      e.stopPropagation();
                      handleAlertAction(alert, action);
                    }}
                  >
                    {action.label}
                  </Button>
                ))}
              </div>
            </Card>
          ))}
        </div>
      ))}
    </div>
  );
}
```

---

## 9. API Contracts

### 9.1 Onboarding API

```
GET /api/onboarding/status
Response: {
  completed: boolean,
  currentStep: string,
  data: OnboardingData
}

POST /api/onboarding/complete
Body: { data: OnboardingData }
Response: { success: boolean }

POST /api/onboarding/reset
Response: { success: boolean }
```

### 9.2 Checklist API

```
GET /api/checklist
Response: { items: ChecklistItem[], progress: ChecklistProgress }

POST /api/checklist/:itemId/complete
Response: { success: boolean, item: ChecklistItem }

POST /api/checklist/:itemId/verify
Response: { success: boolean, verified: boolean, item: ChecklistItem }
```

### 9.3 Trading Mode API

```
GET /api/trading-mode
Response: {
  mode: TradingMode,
  canUpgrade: boolean,
  blockers: string[],
  lockedUntil?: string,
  lockReason?: string
}

POST /api/trading-mode/request-live
Response: { success: boolean, message: string }

POST /api/trading-mode/confirm-live
Body: { acknowledged: boolean }
Response: { success: boolean, mode: TradingMode }

POST /api/trading-mode/unlock
Response: { success: boolean, message: string }
```

### 9.4 Loss Limits API

```
GET /api/loss-limits/status
Response: {
  daily_loss: { current, limit, usage_pct, status },
  circuit_breaker: { current, threshold, usage_pct, status },
  position: { current, limit, usage_pct, status }
}

PUT /api/loss-limits/config
Body: { dailyLossLimit, maxPositionSize, circuitBreakerThreshold }
Response: { success: boolean, config: LossLimitConfig }
```

### 9.5 Alerts API

```
GET /api/alerts
Query: { unreadOnly?: boolean, limit?: number }
Response: { alerts: Alert[] }

POST /api/alerts/:id/read
Response: { success: boolean }

POST /api/alerts/read-all
Response: { success: boolean }
```

---

## 10. Implementation Plan

### 10.1 Phase Breakdown

| Phase | Duration | Deliverables |
|-------|----------|--------------|
| **Phase 1: Onboarding** | 0.5 week | Wizard UI, progress tracking |
| **Phase 2: Checklist** | 0.5 week | Checklist system, verification |
| **Phase 3: Trading Modes** | 0.5 week | Mode controller, state machine |
| **Phase 4: Loss Limits** | 0.5 week | Limit engine, UI display |
| **Phase 5: Anomaly Detection** | 0.5 week | Detector, alert generation |
| **Phase 6: Alert Center** | 0.5 week | Alert UI, notifications |

### 10.2 Dependencies

```
                    Implementation Dependencies
                    ===========================

+------------------+
| Onboarding       |
| Wizard           |
+------------------+
        |
        v
+------------------+     +------------------+
| Checklist        |---->| Trading Mode     |
| System           |     | Controller       |
+------------------+     +------------------+
        |                        |
        +------------------------+
                    |
                    v
            +------------------+
            | Loss Limit       |
            | Engine           |
            +------------------+
                    |
                    v
            +------------------+
            | Anomaly          |
            | Detector         |
            +------------------+
                    |
                    v
            +------------------+
            | Alert Center     |
            +------------------+
```

---

## 11. Testing Strategy

### 11.1 Unit Tests

```typescript
// tests/loss-limits.test.ts
describe('LossLimitEngine', () => {
  const config: LossLimitConfig = {
    dailyLossLimitPct: 0.05,
    maxPositionSizePct: 0.10,
    maxPositionValue: 10000,
    circuitBreakerThresholdPct: 0.10,
    circuitBreakerCooldownMinutes: 60,
  };

  test('blocks order exceeding position limit', () => {
    const engine = new LossLimitEngine(config);
    const state: TradingState = {
      totalEquity: 10000,
      dailyPnl: 0,
      totalPnl: 0,
      openPositionValue: 900,
    };

    const [allowed, message] = engine.checkOrder(state, 200);
    expect(allowed).toBe(false);
    expect(message).toContain('position size limit');
  });

  test('triggers circuit breaker at threshold', () => {
    const engine = new LossLimitEngine(config);
    const state: TradingState = {
      totalEquity: 10000,
      dailyPnl: -500,
      totalPnl: -1000,
      openPositionValue: 0,
    };

    const [ok, message] = engine.checkCircuitBreaker(state);
    expect(ok).toBe(false);
    expect(message).toContain('Circuit breaker triggered');
  });
});
```

### 11.2 E2E Tests

```typescript
// tests/e2e/onboarding.spec.ts
test('completes onboarding flow', async ({ page }) => {
  await page.goto('/onboarding');

  // Step 1: Welcome
  await page.getByRole('checkbox', { name: /risk/i }).check();
  await page.getByRole('checkbox', { name: /afford/i }).check();
  await page.getByRole('checkbox', { name: /borrowed/i }).check();
  await page.getByRole('button', { name: /continue/i }).click();

  // Step 2: Security
  await page.getByRole('checkbox', { name: /security/i }).check();
  await page.getByRole('button', { name: /continue/i }).click();

  // Step 3: Loss Limits
  await page.getByRole('slider', { name: /daily/i }).fill('5');
  await page.getByRole('button', { name: /continue/i }).click();

  // Step 4: Start Simulated
  await page.getByRole('button', { name: /start simulated/i }).click();

  // Verify redirect to dashboard in simulated mode
  await expect(page).toHaveURL('/dashboard');
  await expect(page.getByText(/simulated/i)).toBeVisible();
});
```

---

## 12. Success Metrics

| Metric | Target | Measurement |
|--------|--------|-------------|
| Onboarding completion | > 90% | Analytics |
| Checklist completion | > 80% | Analytics |
| Circuit breaker triggers | < 5% of users | Telemetry |
| User loss incidents | < 1% severe | Support data |
| Security incident rate | 0 | Security monitoring |

---

## 13. Related Documentation

- [GUI Configuration System](02-gui-configuration.md)
- [Security Best Practices](../../guides/user/security-best-practices.md)
- [Risk Management Guide](../../guides/user/risk-management.md)
- [Trading Guide](../../guides/user/trading-guide.md)
