# Security Education Testing

## 1. Test Strategy

### 1.1 Objectives

- Verify onboarding workflow completeness and clarity
- Validate all safety mechanism implementations
- Test loss limit enforcement accuracy
- Verify abnormal trading detection accuracy
- Conduct user acceptance testing for educational content
- Perform security audit of protective features

### 1.2 Scope

| In Scope | Out of Scope |
|----------|--------------|
| Onboarding tutorial flow | Advanced trading education |
| Safety mechanism validation | Exchange-specific tutorials |
| Loss limit enforcement | Tax/legal guidance |
| Abnormal trading detection | Investment advice |
| Risk warnings | Third-party integrations |
| Emergency stop features | Customer support system |

### 1.3 Safety Features

| Feature | Purpose | Priority |
|---------|---------|----------|
| Onboarding Tutorial | Educate new users | P0 |
| Daily Loss Limit | Prevent excessive losses | P0 |
| Position Size Limit | Control risk exposure | P0 |
| Emergency Stop | Halt all trading | P0 |
| Abnormal Detection | Identify unusual activity | P0 |
| Risk Warnings | Alert users to dangers | P0 |
| Paper Trading Mode | Practice without risk | P0 |

## 2. Test Environment

### 2.1 User Personas

| Persona | Experience | Risk Tolerance | Test Focus |
|---------|------------|----------------|------------|
| Novice | None | Low | Onboarding, education |
| Beginner | < 1 year | Medium | Safety limits |
| Intermediate | 1-3 years | Medium-High | Advanced features |
| Expert | > 3 years | High | Override capabilities |

### 2.2 Test Accounts

| Account Type | Balance | Limits | Purpose |
|--------------|---------|--------|---------|
| Paper Trading | $100,000 | None | Learning |
| Testnet | $10,000 | Default | Validation |
| Limited | $1,000 | Strict | Limit testing |
| Unlimited | $100,000 | None | Comparison |

## 3. Test Cases

### 3.1 Onboarding Workflow

#### TC-EDU-001: First-time User Onboarding

| Field | Value |
|-------|-------|
| **ID** | TC-EDU-001 |
| **Priority** | P0 |
| **Preconditions** | Fresh installation, no previous user data |

**Steps:**
1. Launch VeloZ for first time
2. Observe welcome screen
3. Progress through onboarding steps
4. Complete all required sections
5. Verify completion status

**Expected Results:**
- [ ] Welcome screen displays automatically
- [ ] Progress indicator shows current step
- [ ] Cannot skip mandatory sections
- [ ] Each section has clear instructions
- [ ] Completion certificate/badge awarded
- [ ] User preferences saved

#### TC-EDU-002: Risk Acknowledgment

| Field | Value |
|-------|-------|
| **ID** | TC-EDU-002 |
| **Priority** | P0 |
| **Preconditions** | Onboarding in progress |

**Steps:**
1. Navigate to risk acknowledgment section
2. Read risk disclosure
3. Answer risk assessment questions
4. Sign acknowledgment
5. Attempt to skip

**Expected Results:**
- [ ] Risk disclosure clearly visible
- [ ] Must scroll through entire disclosure
- [ ] Questions test understanding
- [ ] Cannot proceed without acknowledgment
- [ ] Acknowledgment timestamp recorded
- [ ] Copy provided to user

#### TC-EDU-003: Paper Trading Tutorial

| Field | Value |
|-------|-------|
| **ID** | TC-EDU-003 |
| **Priority** | P0 |
| **Preconditions** | Risk acknowledgment complete |

**Steps:**
1. Enter paper trading mode
2. Follow guided tutorial
3. Place practice orders
4. Experience simulated losses
5. Complete tutorial

**Expected Results:**
- [ ] Clear "PAPER TRADING" indicator
- [ ] Tutorial guides each step
- [ ] Practice orders execute instantly
- [ ] Simulated P&L updates
- [ ] No real money at risk
- [ ] Tutorial completion tracked

#### TC-EDU-004: Safety Feature Introduction

| Field | Value |
|-------|-------|
| **ID** | TC-EDU-004 |
| **Priority** | P0 |
| **Preconditions** | Paper trading complete |

**Steps:**
1. Navigate to safety features section
2. Learn about loss limits
3. Learn about position limits
4. Learn about emergency stop
5. Configure initial limits

**Expected Results:**
- [ ] Each feature explained clearly
- [ ] Visual demonstrations provided
- [ ] Interactive configuration
- [ ] Recommended defaults suggested
- [ ] User can customize limits
- [ ] Settings saved correctly

#### TC-EDU-005: Onboarding Skip Attempt

| Field | Value |
|-------|-------|
| **ID** | TC-EDU-005 |
| **Priority** | P0 |
| **Preconditions** | Onboarding started |

**Steps:**
1. Attempt to close onboarding
2. Attempt to navigate away
3. Attempt to access trading features
4. Verify restrictions

**Expected Results:**
- [ ] Warning shown on close attempt
- [ ] Navigation restricted during onboarding
- [ ] Trading features locked
- [ ] Must complete critical sections
- [ ] Can pause and resume later
- [ ] Progress saved on pause

### 3.2 Safety Mechanism Validation

#### TC-SAF-001: Daily Loss Limit Enforcement

| Field | Value |
|-------|-------|
| **ID** | TC-SAF-001 |
| **Priority** | P0 |
| **Preconditions** | Daily loss limit set to $100 |

**Steps:**
1. Set daily loss limit to $100
2. Execute trades resulting in $90 loss
3. Attempt trade that would exceed limit
4. Verify limit enforcement
5. Wait for daily reset

**Expected Results:**
- [ ] Warning at 80% of limit ($80 loss)
- [ ] Warning at 90% of limit ($90 loss)
- [ ] Trade blocked at 100% limit
- [ ] Clear message explaining block
- [ ] Override requires confirmation
- [ ] Limit resets at configured time

#### TC-SAF-002: Position Size Limit Enforcement

| Field | Value |
|-------|-------|
| **ID** | TC-SAF-002 |
| **Priority** | P0 |
| **Preconditions** | Position limit set to 1 BTC |

**Steps:**
1. Set max position size to 1 BTC
2. Open position of 0.8 BTC
3. Attempt to add 0.3 BTC (would exceed)
4. Verify limit enforcement

**Expected Results:**
- [ ] Current position displayed
- [ ] Remaining capacity shown
- [ ] Order reduced to fit limit
- [ ] OR order rejected with message
- [ ] User can adjust order size
- [ ] Limit applies per symbol

#### TC-SAF-003: Emergency Stop Functionality

| Field | Value |
|-------|-------|
| **ID** | TC-SAF-003 |
| **Priority** | P0 |
| **Preconditions** | Active positions and orders |

**Steps:**
1. Open several positions
2. Place pending orders
3. Activate emergency stop
4. Verify all activity halted
5. Attempt new trades

**Expected Results:**
- [ ] Emergency stop prominent in UI
- [ ] Confirmation required to activate
- [ ] All open orders cancelled
- [ ] All positions closed (optional)
- [ ] New orders blocked
- [ ] Clear "STOPPED" indicator
- [ ] Requires manual restart

#### TC-SAF-004: Risk Warning Display

| Field | Value |
|-------|-------|
| **ID** | TC-SAF-004 |
| **Priority** | P0 |
| **Preconditions** | Trading interface open |

**Steps:**
1. Attempt high-risk trade (large size)
2. Attempt trade in volatile market
3. Attempt trade with high leverage
4. Observe warnings

**Expected Results:**
- [ ] Warning for large position size
- [ ] Warning for high volatility
- [ ] Warning for leverage usage
- [ ] Warnings are non-dismissible
- [ ] Must acknowledge to proceed
- [ ] Warnings logged for audit

#### TC-SAF-005: Paper Trading Mode Isolation

| Field | Value |
|-------|-------|
| **ID** | TC-SAF-005 |
| **Priority** | P0 |
| **Preconditions** | Paper trading mode active |

**Steps:**
1. Enter paper trading mode
2. Verify visual indicators
3. Attempt to switch to live trading
4. Verify isolation

**Expected Results:**
- [ ] Clear "PAPER" mode indicator
- [ ] Different color scheme
- [ ] Watermark on all screens
- [ ] Switch requires confirmation
- [ ] Paper and live data separate
- [ ] No accidental live trades

### 3.3 Loss Limit Enforcement

#### TC-LOSS-001: Cumulative Loss Tracking

| Field | Value |
|-------|-------|
| **ID** | TC-LOSS-001 |
| **Priority** | P0 |
| **Preconditions** | Loss limit configured |

**Steps:**
1. Set daily loss limit to $500
2. Execute multiple losing trades
3. Track cumulative loss
4. Verify accuracy

**Test Trades:**
| Trade | P&L | Cumulative | Expected Action |
|-------|-----|------------|-----------------|
| 1 | -$100 | -$100 | Continue |
| 2 | -$150 | -$250 | Continue |
| 3 | -$100 | -$350 | Continue |
| 4 | -$50 | -$400 | Warning (80%) |
| 5 | -$50 | -$450 | Warning (90%) |
| 6 | -$100 | -$500 | Blocked |

**Expected Results:**
- [ ] Cumulative loss tracked accurately
- [ ] Real-time P&L updates
- [ ] Warnings at thresholds
- [ ] Trading blocked at limit
- [ ] Unrealized losses included

#### TC-LOSS-002: Loss Limit Reset

| Field | Value |
|-------|-------|
| **ID** | TC-LOSS-002 |
| **Priority** | P0 |
| **Preconditions** | Loss limit reached |

**Steps:**
1. Reach daily loss limit
2. Wait for reset time (midnight UTC)
3. Verify limit reset
4. Resume trading

**Expected Results:**
- [ ] Countdown to reset shown
- [ ] Limit resets at configured time
- [ ] Fresh limit available
- [ ] Previous day's losses logged
- [ ] Trading resumes automatically

#### TC-LOSS-003: Loss Limit Override

| Field | Value |
|-------|-------|
| **ID** | TC-LOSS-003 |
| **Priority** | P1 |
| **Preconditions** | Loss limit reached |

**Steps:**
1. Reach loss limit
2. Request override
3. Complete override process
4. Verify override logged

**Expected Results:**
- [ ] Override requires password
- [ ] Cooling-off period (5 min)
- [ ] Additional warning displayed
- [ ] Override logged with timestamp
- [ ] Limited override amount
- [ ] Daily override limit exists

#### TC-LOSS-004: Multi-Currency Loss Tracking

| Field | Value |
|-------|-------|
| **ID** | TC-LOSS-004 |
| **Priority** | P1 |
| **Preconditions** | Trading multiple pairs |

**Steps:**
1. Set loss limit in USD
2. Trade BTC/USDT (loss in USDT)
3. Trade ETH/BTC (loss in BTC)
4. Verify conversion and tracking

**Expected Results:**
- [ ] All losses converted to base currency
- [ ] Real-time exchange rates used
- [ ] Accurate cumulative total
- [ ] Currency shown in UI

### 3.4 Abnormal Trading Detection

#### TC-ABN-001: Unusual Volume Detection

| Field | Value |
|-------|-------|
| **ID** | TC-ABN-001 |
| **Priority** | P0 |
| **Preconditions** | Normal trading history established |

**Steps:**
1. Establish baseline (10 trades/day avg)
2. Execute 50 trades in 1 hour
3. Observe detection
4. Verify alert

**Expected Results:**
- [ ] Unusual activity detected
- [ ] Alert displayed to user
- [ ] Trading paused for review
- [ ] User must acknowledge
- [ ] Activity logged
- [ ] Option to continue or stop

#### TC-ABN-002: Unusual Size Detection

| Field | Value |
|-------|-------|
| **ID** | TC-ABN-002 |
| **Priority** | P0 |
| **Preconditions** | Normal trading history (avg $100 trades) |

**Steps:**
1. Establish baseline ($100 avg trade)
2. Attempt $5,000 trade (50x normal)
3. Observe detection
4. Verify alert

**Expected Results:**
- [ ] Large trade flagged
- [ ] Confirmation required
- [ ] Warning about size
- [ ] Comparison to average shown
- [ ] User can proceed or cancel

#### TC-ABN-003: Rapid Loss Detection

| Field | Value |
|-------|-------|
| **ID** | TC-ABN-003 |
| **Priority** | P0 |
| **Preconditions** | Trading active |

**Steps:**
1. Execute series of losing trades
2. Lose 20% of account in 1 hour
3. Observe detection
4. Verify intervention

**Expected Results:**
- [ ] Rapid loss detected
- [ ] Trading automatically paused
- [ ] Cooling-off period enforced
- [ ] User notified
- [ ] Must acknowledge to continue
- [ ] Suggestion to review strategy

#### TC-ABN-004: Pattern Detection - Revenge Trading

| Field | Value |
|-------|-------|
| **ID** | TC-ABN-004 |
| **Priority** | P0 |
| **Preconditions** | Recent significant loss |

**Steps:**
1. Experience large loss
2. Immediately attempt larger trade
3. Observe detection
4. Verify warning

**Expected Results:**
- [ ] Pattern recognized
- [ ] Warning about emotional trading
- [ ] Cooling-off suggested
- [ ] Trade size reduced automatically
- [ ] Educational content shown

#### TC-ABN-005: Off-Hours Trading Alert

| Field | Value |
|-------|-------|
| **ID** | TC-ABN-005 |
| **Priority** | P1 |
| **Preconditions** | Trading schedule configured |

**Steps:**
1. Set trading hours (9 AM - 5 PM)
2. Attempt trade at 3 AM
3. Observe alert
4. Verify handling

**Expected Results:**
- [ ] Off-hours trade flagged
- [ ] Confirmation required
- [ ] Reminder of schedule
- [ ] Option to proceed
- [ ] Activity logged

### 3.5 User Acceptance Testing

#### TC-UAT-001: Novice User Onboarding Experience

| Field | Value |
|-------|-------|
| **ID** | TC-UAT-001 |
| **Priority** | P0 |
| **Preconditions** | Novice user tester |

**Scenario:**
New user with no trading experience completes onboarding.

**Evaluation Criteria:**
| Criterion | Target | Measurement |
|-----------|--------|-------------|
| Completion rate | > 95% | % completing all steps |
| Time to complete | < 30 min | Average duration |
| Comprehension | > 80% | Quiz score |
| Satisfaction | > 4/5 | Survey rating |
| Confidence | > 4/5 | Self-assessment |

**Expected Results:**
- [ ] User completes onboarding
- [ ] User understands risks
- [ ] User can configure limits
- [ ] User feels prepared
- [ ] No confusion points

#### TC-UAT-002: Safety Feature Usability

| Field | Value |
|-------|-------|
| **ID** | TC-UAT-002 |
| **Priority** | P0 |
| **Preconditions** | User completed onboarding |

**Tasks:**
1. Set daily loss limit
2. Find and use emergency stop
3. Enable paper trading mode
4. Review risk warnings

**Evaluation Criteria:**
| Task | Target Time | Success Rate |
|------|-------------|--------------|
| Set loss limit | < 2 min | > 95% |
| Emergency stop | < 30 sec | > 99% |
| Paper trading | < 1 min | > 95% |
| Risk warnings | < 1 min | > 90% |

**Expected Results:**
- [ ] All tasks completed successfully
- [ ] No assistance needed
- [ ] Features easily discoverable
- [ ] Clear feedback provided

#### TC-UAT-003: Educational Content Effectiveness

| Field | Value |
|-------|-------|
| **ID** | TC-UAT-003 |
| **Priority** | P0 |
| **Preconditions** | User completed onboarding |

**Assessment:**
1. Pre-test: Risk awareness quiz
2. Complete educational content
3. Post-test: Same quiz
4. Measure improvement

**Expected Results:**
- [ ] Post-test score > Pre-test score
- [ ] Minimum 20% improvement
- [ ] Key concepts understood
- [ ] Practical application demonstrated

### 3.6 Security Audit

#### TC-SEC-001: Limit Bypass Attempts

| Field | Value |
|-------|-------|
| **ID** | TC-SEC-001 |
| **Priority** | P0 |
| **Preconditions** | Limits configured |

**Attack Vectors:**
1. Direct API calls bypassing UI
2. Multiple simultaneous orders
3. Modifying local storage
4. Time manipulation
5. Account switching

**Expected Results:**
- [ ] API enforces limits (not just UI)
- [ ] Race conditions prevented
- [ ] Local storage tampering detected
- [ ] Server-side time used
- [ ] Limits per account enforced

#### TC-SEC-002: Emergency Stop Reliability

| Field | Value |
|-------|-------|
| **ID** | TC-SEC-002 |
| **Priority** | P0 |
| **Preconditions** | Active trading |

**Test Scenarios:**
1. Network failure during stop
2. Server overload during stop
3. Multiple simultaneous stops
4. Stop during order execution

**Expected Results:**
- [ ] Stop persists through network issues
- [ ] Server queues stop requests
- [ ] Idempotent stop handling
- [ ] In-flight orders cancelled

#### TC-SEC-003: Audit Trail Integrity

| Field | Value |
|-------|-------|
| **ID** | TC-SEC-003 |
| **Priority** | P0 |
| **Preconditions** | Trading activity logged |

**Verification:**
1. All trades logged
2. All limit changes logged
3. All overrides logged
4. Logs tamper-evident

**Expected Results:**
- [ ] Complete audit trail
- [ ] Timestamps accurate
- [ ] User actions attributed
- [ ] Logs cannot be modified
- [ ] Logs backed up

#### TC-SEC-004: Data Protection

| Field | Value |
|-------|-------|
| **ID** | TC-SEC-004 |
| **Priority** | P0 |
| **Preconditions** | User data stored |

**Verification:**
1. Personal data encrypted
2. Trading history protected
3. Limit settings secured
4. Export requires authentication

**Expected Results:**
- [ ] Data encrypted at rest
- [ ] Data encrypted in transit
- [ ] Access controls enforced
- [ ] Data export audited

## 4. Automated Test Scripts

### 4.1 Onboarding Tests (Python)

```python
# File: tests/security_education/test_onboarding.py

import pytest
from veloz.onboarding import OnboardingManager, OnboardingStep

class TestOnboardingFlow:
    """Test onboarding workflow."""

    @pytest.fixture
    def onboarding(self):
        return OnboardingManager()

    def test_new_user_sees_onboarding(self, onboarding):
        """New users must see onboarding."""
        user = create_new_user()
        assert onboarding.is_required(user)
        assert onboarding.get_current_step(user) == OnboardingStep.WELCOME

    def test_cannot_skip_mandatory_steps(self, onboarding):
        """Mandatory steps cannot be skipped."""
        user = create_new_user()

        # Try to skip to trading
        with pytest.raises(OnboardingError):
            onboarding.skip_to_step(user, OnboardingStep.TRADING)

    def test_risk_acknowledgment_required(self, onboarding):
        """Risk acknowledgment must be completed."""
        user = create_new_user()

        # Progress to risk step
        onboarding.complete_step(user, OnboardingStep.WELCOME)
        onboarding.complete_step(user, OnboardingStep.INTRODUCTION)

        # Cannot proceed without acknowledgment
        assert not onboarding.can_proceed(user)

        # Complete acknowledgment
        onboarding.acknowledge_risks(user, signature="User Name")
        assert onboarding.can_proceed(user)

    def test_paper_trading_tutorial(self, onboarding):
        """Paper trading tutorial must be completed."""
        user = create_new_user()

        # Progress to paper trading
        complete_steps_until(onboarding, user, OnboardingStep.PAPER_TRADING)

        # Must complete tutorial trades
        assert onboarding.get_tutorial_progress(user) == 0

        # Complete tutorial
        for i in range(5):
            onboarding.record_tutorial_trade(user)

        assert onboarding.get_tutorial_progress(user) == 5
        assert onboarding.is_tutorial_complete(user)

    def test_onboarding_completion(self, onboarding):
        """Onboarding completion unlocks trading."""
        user = create_new_user()

        # Complete all steps
        complete_all_onboarding(onboarding, user)

        assert onboarding.is_complete(user)
        assert user.can_trade()
        assert user.has_completion_badge()


class TestRiskAcknowledgment:
    """Test risk acknowledgment process."""

    def test_must_scroll_disclosure(self):
        """User must scroll through entire disclosure."""
        disclosure = RiskDisclosure()

        # Not scrolled
        assert not disclosure.is_read()

        # Partial scroll
        disclosure.scroll_to(50)  # 50%
        assert not disclosure.is_read()

        # Full scroll
        disclosure.scroll_to(100)
        assert disclosure.is_read()

    def test_quiz_required(self):
        """Risk quiz must be passed."""
        quiz = RiskQuiz()

        # Wrong answers
        quiz.answer(1, "wrong")
        quiz.answer(2, "wrong")
        assert not quiz.is_passed()

        # Correct answers
        quiz.reset()
        quiz.answer(1, "correct")
        quiz.answer(2, "correct")
        quiz.answer(3, "correct")
        assert quiz.is_passed()

    def test_acknowledgment_recorded(self):
        """Acknowledgment is recorded with timestamp."""
        user = create_new_user()
        ack = RiskAcknowledgment(user)

        ack.sign("User Name")

        assert ack.is_signed()
        assert ack.signature == "User Name"
        assert ack.timestamp is not None
        assert ack.ip_address is not None
```

### 4.2 Safety Mechanism Tests (Python)

```python
# File: tests/security_education/test_safety_mechanisms.py

import pytest
from veloz.safety import LossLimiter, PositionLimiter, EmergencyStop
from veloz.trading import Order, Position

class TestLossLimiter:
    """Test loss limit enforcement."""

    @pytest.fixture
    def limiter(self):
        return LossLimiter(daily_limit=500.0)

    def test_tracks_cumulative_loss(self, limiter):
        """Cumulative loss is tracked accurately."""
        limiter.record_loss(100.0)
        assert limiter.cumulative_loss == 100.0

        limiter.record_loss(150.0)
        assert limiter.cumulative_loss == 250.0

    def test_warning_at_threshold(self, limiter):
        """Warning issued at 80% threshold."""
        limiter.record_loss(400.0)  # 80%

        assert limiter.is_warning_threshold()
        assert limiter.get_warning_message() is not None

    def test_blocks_at_limit(self, limiter):
        """Trading blocked at 100% limit."""
        limiter.record_loss(500.0)

        assert limiter.is_limit_reached()
        assert not limiter.can_trade()

    def test_blocks_order_exceeding_limit(self, limiter):
        """Order that would exceed limit is blocked."""
        limiter.record_loss(450.0)

        # Order with potential $100 loss
        order = Order(symbol="BTCUSDT", size=0.01, stop_loss=45000)
        order.potential_loss = 100.0

        result = limiter.check_order(order)
        assert not result.allowed
        assert "exceed" in result.reason.lower()

    def test_resets_daily(self, limiter):
        """Limit resets at configured time."""
        limiter.record_loss(500.0)
        assert limiter.is_limit_reached()

        # Simulate time passing to next day
        limiter.reset()

        assert limiter.cumulative_loss == 0.0
        assert limiter.can_trade()


class TestPositionLimiter:
    """Test position size limit enforcement."""

    @pytest.fixture
    def limiter(self):
        return PositionLimiter(max_position={"BTCUSDT": 1.0})

    def test_allows_within_limit(self, limiter):
        """Orders within limit are allowed."""
        order = Order(symbol="BTCUSDT", size=0.5)
        result = limiter.check_order(order)
        assert result.allowed

    def test_blocks_exceeding_limit(self, limiter):
        """Orders exceeding limit are blocked."""
        order = Order(symbol="BTCUSDT", size=1.5)
        result = limiter.check_order(order)
        assert not result.allowed

    def test_considers_existing_position(self, limiter):
        """Existing position is considered."""
        # Existing position
        limiter.record_position("BTCUSDT", 0.8)

        # New order would exceed
        order = Order(symbol="BTCUSDT", size=0.3)
        result = limiter.check_order(order)
        assert not result.allowed

    def test_suggests_reduced_size(self, limiter):
        """Suggests reduced order size."""
        limiter.record_position("BTCUSDT", 0.8)

        order = Order(symbol="BTCUSDT", size=0.5)
        result = limiter.check_order(order)

        assert result.suggested_size == 0.2  # Max remaining


class TestEmergencyStop:
    """Test emergency stop functionality."""

    @pytest.fixture
    def emergency_stop(self):
        return EmergencyStop()

    def test_stops_all_trading(self, emergency_stop):
        """Emergency stop halts all trading."""
        emergency_stop.activate()

        assert emergency_stop.is_active()
        assert not emergency_stop.can_trade()

    def test_cancels_open_orders(self, emergency_stop, mock_oms):
        """Open orders are cancelled."""
        # Create open orders
        mock_oms.create_order(Order(symbol="BTCUSDT", size=0.1))
        mock_oms.create_order(Order(symbol="ETHUSDT", size=1.0))

        emergency_stop.activate(oms=mock_oms)

        assert mock_oms.open_order_count() == 0

    def test_requires_confirmation(self, emergency_stop):
        """Activation requires confirmation."""
        # Without confirmation
        with pytest.raises(ConfirmationRequired):
            emergency_stop.activate(confirmed=False)

        # With confirmation
        emergency_stop.activate(confirmed=True)
        assert emergency_stop.is_active()

    def test_requires_manual_restart(self, emergency_stop):
        """Manual restart required after stop."""
        emergency_stop.activate(confirmed=True)

        # Cannot auto-restart
        with pytest.raises(ManualRestartRequired):
            emergency_stop.auto_restart()

        # Manual restart works
        emergency_stop.manual_restart(password="user_password")
        assert not emergency_stop.is_active()
```

### 4.3 Abnormal Detection Tests (Python)

```python
# File: tests/security_education/test_abnormal_detection.py

import pytest
from veloz.safety import AbnormalDetector
from veloz.trading import Trade

class TestAbnormalDetector:
    """Test abnormal trading detection."""

    @pytest.fixture
    def detector(self):
        return AbnormalDetector()

    def test_detects_unusual_volume(self, detector):
        """Detects unusual trading volume."""
        # Establish baseline (10 trades/day)
        for _ in range(30):  # 30 days
            for _ in range(10):
                detector.record_trade(Trade(size=0.01))
            detector.advance_day()

        # Unusual volume (50 trades in 1 hour)
        for _ in range(50):
            detector.record_trade(Trade(size=0.01))

        assert detector.is_unusual_volume()
        assert detector.get_alert() is not None

    def test_detects_unusual_size(self, detector):
        """Detects unusual trade size."""
        # Establish baseline ($100 avg)
        for _ in range(100):
            detector.record_trade(Trade(value=100))

        # Unusual size ($5000)
        result = detector.check_trade(Trade(value=5000))

        assert result.is_unusual
        assert "size" in result.reason.lower()

    def test_detects_rapid_loss(self, detector):
        """Detects rapid loss pattern."""
        initial_balance = 10000

        # Lose 20% in 1 hour
        for _ in range(20):
            detector.record_loss(100)  # 1% each

        assert detector.is_rapid_loss(initial_balance)
        assert detector.should_pause_trading()

    def test_detects_revenge_trading(self, detector):
        """Detects revenge trading pattern."""
        # Large loss
        detector.record_loss(500)

        # Immediate larger trade
        trade = Trade(value=1000)
        result = detector.check_trade(trade)

        assert result.is_revenge_trading
        assert result.cooling_off_suggested

    def test_detects_off_hours_trading(self, detector):
        """Detects off-hours trading."""
        detector.set_trading_hours(start=9, end=17)  # 9 AM - 5 PM

        # Trade at 3 AM
        from datetime import datetime
        trade = Trade(timestamp=datetime(2024, 1, 1, 3, 0, 0))

        result = detector.check_trade(trade)
        assert result.is_off_hours
```

### 4.4 UI Tests (Playwright)

```typescript
// File: tests/security_education/onboarding.spec.ts

import { test, expect } from '@playwright/test';

test.describe('Onboarding Flow', () => {
  test('new user sees onboarding', async ({ page }) => {
    // Clear any existing user data
    await page.evaluate(() => localStorage.clear());

    await page.goto('/');

    // Should see welcome screen
    await expect(page.locator('.onboarding-welcome')).toBeVisible();
    await expect(page.locator('.trading-interface')).not.toBeVisible();
  });

  test('cannot skip mandatory steps', async ({ page }) => {
    await page.goto('/onboarding');

    // Try to navigate to trading
    await page.goto('/trading');

    // Should be redirected back
    await expect(page).toHaveURL(/onboarding/);
    await expect(page.locator('.onboarding-required-message')).toBeVisible();
  });

  test('risk acknowledgment flow', async ({ page }) => {
    await page.goto('/onboarding/risk');

    // Must scroll through disclosure
    const disclosure = page.locator('.risk-disclosure');
    await disclosure.evaluate(el => el.scrollTop = el.scrollHeight);

    // Must answer quiz correctly
    await page.click('[data-answer="correct-1"]');
    await page.click('[data-answer="correct-2"]');
    await page.click('[data-answer="correct-3"]');

    // Must sign acknowledgment
    await page.fill('#signature', 'Test User');
    await page.click('#acknowledge-btn');

    // Should proceed to next step
    await expect(page).toHaveURL(/onboarding\/paper-trading/);
  });

  test('paper trading tutorial', async ({ page }) => {
    await page.goto('/onboarding/paper-trading');

    // Should see paper trading indicator
    await expect(page.locator('.paper-trading-badge')).toBeVisible();
    await expect(page.locator('.paper-trading-badge')).toContainText('PAPER');

    // Complete tutorial trades
    for (let i = 0; i < 5; i++) {
      await page.click('#tutorial-trade-btn');
      await page.waitForSelector('.trade-complete');
    }

    // Tutorial should be complete
    await expect(page.locator('.tutorial-complete')).toBeVisible();
  });
});

test.describe('Safety Features', () => {
  test('loss limit warning', async ({ page }) => {
    await page.goto('/trading');

    // Set loss limit
    await page.click('#settings-btn');
    await page.fill('#daily-loss-limit', '100');
    await page.click('#save-settings');

    // Simulate losses to 80%
    await page.evaluate(() => {
      (window as any).simulateLoss(80);
    });

    // Warning should appear
    await expect(page.locator('.loss-warning')).toBeVisible();
    await expect(page.locator('.loss-warning')).toContainText('80%');
  });

  test('emergency stop', async ({ page }) => {
    await page.goto('/trading');

    // Click emergency stop
    await page.click('#emergency-stop-btn');

    // Confirmation dialog
    await expect(page.locator('.confirm-dialog')).toBeVisible();
    await page.click('#confirm-stop');

    // Trading should be stopped
    await expect(page.locator('.stopped-indicator')).toBeVisible();
    await expect(page.locator('#place-order-btn')).toBeDisabled();
  });

  test('abnormal trading alert', async ({ page }) => {
    await page.goto('/trading');

    // Simulate rapid trading
    for (let i = 0; i < 50; i++) {
      await page.evaluate(() => {
        (window as any).simulateTrade();
      });
    }

    // Alert should appear
    await expect(page.locator('.abnormal-alert')).toBeVisible();
    await expect(page.locator('.abnormal-alert')).toContainText('unusual');
  });
});
```

## 5. Performance Benchmarks

### 5.1 Onboarding Performance

| Metric | Target | Maximum |
|--------|--------|---------|
| Page load time | < 2s | 5s |
| Step transition | < 500ms | 1s |
| Quiz submission | < 1s | 3s |
| Tutorial trade | < 500ms | 2s |

### 5.2 Safety Feature Performance

| Feature | Response Time | Maximum |
|---------|---------------|---------|
| Loss limit check | < 10ms | 50ms |
| Position limit check | < 10ms | 50ms |
| Emergency stop | < 100ms | 500ms |
| Abnormal detection | < 50ms | 200ms |

## 6. Security Testing Checklist

### 6.1 Limit Enforcement

- [ ] Limits enforced server-side (not just UI)
- [ ] API calls respect limits
- [ ] Race conditions prevented
- [ ] Limits cannot be bypassed via timing
- [ ] Multi-account limits enforced

### 6.2 Data Protection

- [ ] User progress encrypted
- [ ] Acknowledgments tamper-proof
- [ ] Audit logs immutable
- [ ] Personal data protected

### 6.3 Access Control

- [ ] Onboarding cannot be bypassed
- [ ] Trading locked until complete
- [ ] Override requires authentication
- [ ] Admin actions logged

## 7. UAT Plan

### 7.1 UAT Participants

- 10 novice users (no trading experience)
- 5 beginner users (< 1 year experience)
- 3 intermediate users (1-3 years experience)

### 7.2 UAT Success Criteria

| Criterion | Target |
|-----------|--------|
| Onboarding completion rate | > 95% |
| Risk quiz pass rate | > 90% |
| Safety feature usability | > 4/5 |
| Educational content clarity | > 4/5 |
| Overall satisfaction | > 4/5 |

## 8. Bug Tracking

### 8.1 Common Issues

| ID | Description | Severity | Status |
|----|-------------|----------|--------|
| EDU-001 | Quiz answers not saved | S1 | Fixed |
| EDU-002 | Loss limit bypass via API | S0 | Fixed |
| EDU-003 | Emergency stop delay | S1 | In Progress |
