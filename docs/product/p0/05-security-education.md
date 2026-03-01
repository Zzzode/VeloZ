# PRD: Security Education

## Document Information

| Field | Value |
|-------|-------|
| Feature | Security Education |
| Priority | P0 |
| Status | Draft |
| Owner | Product Manager |
| Target Release | v2.0 |
| Last Updated | 2026-02-25 |

## Executive Summary

Create an interactive onboarding flow that educates users about API key security, risk management, and safe trading practices. Require completion of security education and simulated trading before allowing live trading with real funds.

## Problem Statement

### Current State

VeloZ provides security documentation but has no enforcement mechanism to ensure users understand security best practices before trading. This creates significant risks:

- Users may expose API keys
- Users may enable withdrawal permissions
- Users may trade without understanding risks
- Users may lose significant capital due to misconfiguration

### User Pain Points

| Pain Point | Impact | Frequency |
|------------|--------|-----------|
| Security docs are optional | Users skip them | Common |
| No practical training | Theory without practice | Always |
| No guardrails for beginners | Costly mistakes | First-time users |
| Complex security concepts | Confusion, wrong choices | Always |

### Target User Profile

**Persona: James, New Crypto Trader**
- Age: 25, new to automated trading
- Technical skills: Basic computer literacy
- Goal: Start trading safely without making costly mistakes
- Frustration: "I don't know what I don't know about security"

## Goals and Success Metrics

### Goals

1. **Education**: Every user understands key security concepts
2. **Prevention**: Prevent common security mistakes
3. **Progression**: Gradual unlock of capabilities as users demonstrate competence
4. **Confidence**: Users feel confident they're trading safely

### Success Metrics

| Metric | Current | Target | Measurement |
|--------|---------|--------|-------------|
| Security incident rate | Unknown | < 0.1% | Incident tracking |
| Onboarding completion rate | N/A | > 90% | Analytics |
| Paper trading before live | N/A | > 95% | Analytics |
| API key exposure incidents | Unknown | 0 | Security audit |

## User Stories

### US-1: Security Onboarding

**As a** new VeloZ user
**I want to** learn about trading security
**So that** I can protect my funds and API keys

**Acceptance Criteria:**
- [ ] Interactive security tutorial
- [ ] Covers API key safety
- [ ] Covers risk management basics
- [ ] Quiz to verify understanding
- [ ] Cannot skip to live trading

### US-2: API Key Safety Checklist

**As a** user setting up exchange connection
**I want to** verify my API key is configured safely
**So that** I don't accidentally enable dangerous permissions

**Acceptance Criteria:**
- [ ] Checklist before saving API key
- [ ] Warning if withdrawal permission detected
- [ ] Recommendation for IP restriction
- [ ] Explanation of each permission
- [ ] Cannot proceed with unsafe configuration

### US-3: Simulated Trading Requirement

**As a** new user
**I want to** practice with simulated trading first
**So that** I can learn without risking real money

**Acceptance Criteria:**
- [ ] Simulation mode enabled by default
- [ ] Minimum simulation period (e.g., 7 days or 10 trades)
- [ ] Progress tracker toward live trading unlock
- [ ] Clear distinction between sim and live
- [ ] Cannot bypass simulation requirement

### US-4: Progressive Risk Limits

**As a** user progressing to live trading
**I want to** have conservative limits initially
**So that** I can't lose too much while learning

**Acceptance Criteria:**
- [ ] Initial limits: $100 max position, $50 max daily loss
- [ ] Limits increase with trading experience
- [ ] Clear explanation of current limits
- [ ] Request to increase limits (with review)
- [ ] Automatic limit increases based on criteria

### US-5: Warning System

**As a** user about to make a risky action
**I want to** receive clear warnings
**So that** I can reconsider before proceeding

**Acceptance Criteria:**
- [ ] Warning before first live trade
- [ ] Warning when approaching loss limits
- [ ] Warning for large position sizes
- [ ] Warning for unusual market conditions
- [ ] Confirmation required for risky actions

### US-6: Security Dashboard

**As a** user
**I want to** see my security status at a glance
**So that** I know if I need to take any action

**Acceptance Criteria:**
- [ ] Security score/rating
- [ ] Checklist of security measures
- [ ] Recommendations for improvement
- [ ] Alert for security issues
- [ ] Link to security settings

## Security Education Content

### Module 1: API Key Safety

| Topic | Content | Duration |
|-------|---------|----------|
| What are API keys | Explanation of API key/secret | 2 min |
| Permission levels | Read, Trade, Withdraw | 3 min |
| Never enable withdrawals | Critical warning | 2 min |
| IP restrictions | How and why to set | 3 min |
| Key storage | Never share, never commit | 2 min |
| **Quiz** | 5 questions | 3 min |

### Module 2: Risk Management

| Topic | Content | Duration |
|-------|---------|----------|
| Position sizing | Never risk more than you can lose | 3 min |
| Stop losses | Automatic loss limiting | 3 min |
| Daily loss limits | Circuit breakers | 2 min |
| Drawdown protection | Maximum portfolio decline | 3 min |
| Diversification | Don't put all eggs in one basket | 2 min |
| **Quiz** | 5 questions | 3 min |

### Module 3: Safe Trading Practices

| Topic | Content | Duration |
|-------|---------|----------|
| Start small | Begin with small amounts | 2 min |
| Paper trading | Practice before live | 2 min |
| Backtest first | Validate strategies | 3 min |
| Monitor actively | Don't set and forget | 2 min |
| Emergency procedures | What to do if something goes wrong | 3 min |
| **Quiz** | 5 questions | 3 min |

### Quiz Questions (Sample)

**API Key Safety:**
1. Should you enable withdrawal permissions on your trading API key?
   - [ ] Yes, for convenience
   - [x] No, never for trading bots
   - [ ] Only for large accounts

2. What should you do if you accidentally share your API key?
   - [ ] Nothing, it's probably fine
   - [ ] Change your password
   - [x] Immediately revoke and create a new key

**Risk Management:**
3. What is a reasonable maximum position size for beginners?
   - [ ] 50% of portfolio
   - [ ] 25% of portfolio
   - [x] 5-10% of portfolio

4. What happens when a daily loss limit is reached?
   - [ ] Nothing, it's just a warning
   - [x] Trading is automatically paused
   - [ ] All positions are liquidated

## User Flows

### Flow 1: New User Onboarding

```
+------------------+     +------------------+     +------------------+
|   Welcome        |     |   Security       |     |   Module 1:      |
|   Screen         | --> |   Overview       | --> |   API Keys       |
+------------------+     +------------------+     +--------+---------+
                                                          |
                                                          v
+------------------+     +------------------+     +------------------+
|   Module 3:      |     |   Module 2:      |     |   Quiz 1         |
|   Safe Trading   | <-- |   Risk Mgmt      | <-- |   (Pass/Fail)    |
+--------+---------+     +------------------+     +------------------+
         |
         v
+------------------+     +------------------+     +------------------+
|   Quiz 3         |     |   Onboarding     |     |   Simulation     |
|   (Pass/Fail)    | --> |   Complete       | --> |   Mode Active    |
+------------------+     +------------------+     +------------------+
```

### Flow 2: API Key Safety Check

```
+------------------+     +------------------+     +------------------+
|   Enter API      |     |   Validate       |     |   Check          |
|   Credentials    | --> |   Connection     | --> |   Permissions    |
+------------------+     +------------------+     +--------+---------+
                                                          |
                              +---------------------------+
                              |                           |
                              v                           v
                    +------------------+        +------------------+
                    |   Safe Config    |        |   Unsafe Config  |
                    |   Proceed        |        |   Warning        |
                    +------------------+        +--------+---------+
                                                         |
                                                         v
                                               +------------------+
                                               |   Must Fix       |
                                               |   Before Proceed |
                                               +------------------+
```

### Flow 3: Simulation to Live Progression

```
+------------------+     +------------------+     +------------------+
|   Complete       |     |   Simulation     |     |   Meet           |
|   Onboarding     | --> |   Trading        | --> |   Requirements   |
+------------------+     +------------------+     +--------+---------+
                                                          |
                                                          v
+------------------+     +------------------+     +------------------+
|   Live Trading   |     |   Final          |     |   Review         |
|   Unlocked       | <-- |   Confirmation   | <-- |   Checklist      |
+------------------+     +------------------+     +------------------+
```

## Wireframes

### Security Onboarding - Welcome

```
+------------------------------------------------------------------+
|  Security Onboarding                                              |
+------------------------------------------------------------------+
|                                                                   |
|                         [Shield Icon]                             |
|                                                                   |
|                    Your Security Matters                          |
|                                                                   |
|    Before you start trading, let's make sure you understand       |
|    how to keep your funds and API keys safe.                      |
|                                                                   |
|    This will take about 15 minutes and covers:                    |
|                                                                   |
|    [Key Icon]     API Key Safety                                  |
|                   How to protect your exchange credentials        |
|                                                                   |
|    [Shield Icon]  Risk Management                                 |
|                   How to limit potential losses                   |
|                                                                   |
|    [Check Icon]   Safe Trading Practices                          |
|                   Best practices for automated trading            |
|                                                                   |
|    +----------------------------------------------------------+   |
|    | [!] You must complete this training before live trading  |   |
|    +----------------------------------------------------------+   |
|                                                                   |
|                                              [Start Training]     |
+------------------------------------------------------------------+
```

### Security Onboarding - Module Content

```
+------------------------------------------------------------------+
|  API Key Safety                                    [1/3]          |
+------------------------------------------------------------------+
|                                                                   |
|  Progress: [====|-----] 40%                                       |
|                                                                   |
|  +----------------------------------------------------------+     |
|  |                                                           |     |
|  |  [Warning Icon]  NEVER Enable Withdrawal Permissions      |     |
|  |                                                           |     |
|  |  When creating an API key for VeloZ, you should:          |     |
|  |                                                           |     |
|  |  [Check] Enable "Read" permission                         |     |
|  |  [Check] Enable "Spot Trading" permission                 |     |
|  |  [X]     NEVER enable "Withdrawal" permission             |     |
|  |                                                           |     |
|  |  +----------------------------------------------------+   |     |
|  |  | Why? If your API key is ever compromised, an       |   |     |
|  |  | attacker could withdraw all your funds. Trading    |   |     |
|  |  | bots never need withdrawal permissions.            |   |     |
|  |  +----------------------------------------------------+   |     |
|  |                                                           |     |
|  |  [Diagram showing safe vs unsafe API key permissions]     |     |
|  |                                                           |     |
|  +----------------------------------------------------------+     |
|                                                                   |
|                                        [< Back]  [Next >]         |
+------------------------------------------------------------------+
```

### Security Onboarding - Quiz

```
+------------------------------------------------------------------+
|  API Key Safety Quiz                               [1/3]          |
+------------------------------------------------------------------+
|                                                                   |
|  Question 2 of 5                                                  |
|                                                                   |
|  +----------------------------------------------------------+     |
|  |                                                           |     |
|  |  Which permissions should you enable for your VeloZ       |     |
|  |  API key?                                                 |     |
|  |                                                           |     |
|  |  ( ) Read, Spot Trading, Withdrawal                       |     |
|  |  (o) Read, Spot Trading only                              |     |
|  |  ( ) All permissions for convenience                      |     |
|  |  ( ) Withdrawal only                                      |     |
|  |                                                           |     |
|  +----------------------------------------------------------+     |
|                                                                   |
|  +----------------------------------------------------------+     |
|  | [Check] Correct!                                          |     |
|  |                                                           |     |
|  | You should only enable Read and Spot Trading. Never       |     |
|  | enable Withdrawal permissions for trading bots.           |     |
|  +----------------------------------------------------------+     |
|                                                                   |
|                                              [Next Question]      |
+------------------------------------------------------------------+
```

### API Key Safety Checklist

```
+------------------------------------------------------------------+
|  API Key Safety Check                                    [X]      |
+------------------------------------------------------------------+
|                                                                   |
|  Before saving your API key, please verify:                       |
|                                                                   |
|  +----------------------------------------------------------+     |
|  | [Check] Read permission enabled                           |     |
|  +----------------------------------------------------------+     |
|                                                                   |
|  +----------------------------------------------------------+     |
|  | [Check] Spot Trading permission enabled                   |     |
|  +----------------------------------------------------------+     |
|                                                                   |
|  +----------------------------------------------------------+     |
|  | [X] Withdrawal permission DISABLED                        |     |
|  |                                                           |     |
|  | [!] WARNING: Withdrawal permission detected!              |     |
|  |                                                           |     |
|  | Your API key has withdrawal permissions enabled.          |     |
|  | This is a serious security risk.                          |     |
|  |                                                           |     |
|  | Please create a new API key without withdrawal            |     |
|  | permissions before continuing.                            |     |
|  |                                                           |     |
|  | [How to create a safe API key]                            |     |
|  +----------------------------------------------------------+     |
|                                                                   |
|  +----------------------------------------------------------+     |
|  | [ ] IP restriction configured (recommended)               |     |
|  |     Add your IP: 203.0.113.45                             |     |
|  +----------------------------------------------------------+     |
|                                                                   |
|                    [Cannot proceed with unsafe configuration]     |
+------------------------------------------------------------------+
```

### Simulation Progress Tracker

```
+------------------------------------------------------------------+
|  Your Trading Journey                                             |
+------------------------------------------------------------------+
|                                                                   |
|  Current Status: [Simulation Mode]                                |
|                                                                   |
|  +----------------------------------------------------------+     |
|  |                                                           |     |
|  |  Progress to Live Trading                                 |     |
|  |                                                           |     |
|  |  [============================|-------] 75%               |     |
|  |                                                           |     |
|  |  Requirements:                                            |     |
|  |                                                           |     |
|  |  [Check] Complete security onboarding                     |     |
|  |  [Check] Configure exchange connection                    |     |
|  |  [Check] Complete 10 simulated trades (8/10)              |     |
|  |  [ ]     7 days of simulation experience (5/7 days)       |     |
|  |  [ ]     Review and accept risk disclosure                |     |
|  |                                                           |     |
|  +----------------------------------------------------------+     |
|                                                                   |
|  Simulation Stats                                                 |
|  --------------------------------------------------------         |
|  | Trades | Win Rate | Total P&L | Max Drawdown |                 |
|  |   8    |   62%    |  +$245    |    -$80      |                 |
|  --------------------------------------------------------         |
|                                                                   |
|  [!] You're making good progress! Complete 2 more trades          |
|      and wait 2 more days to unlock live trading.                 |
|                                                                   |
+------------------------------------------------------------------+
```

### Live Trading Unlock Confirmation

```
+------------------------------------------------------------------+
|  Ready for Live Trading                                  [X]      |
+------------------------------------------------------------------+
|                                                                   |
|  Congratulations! You've completed all requirements.              |
|                                                                   |
|  Before enabling live trading, please review:                     |
|                                                                   |
|  +----------------------------------------------------------+     |
|  |                                                           |     |
|  |  [!] IMPORTANT: Live Trading Risks                        |     |
|  |                                                           |     |
|  |  - You will be trading with REAL money                    |     |
|  |  - Losses are REAL and cannot be reversed                 |     |
|  |  - Past simulation performance does not guarantee         |     |
|  |    future results                                         |     |
|  |  - Market conditions can change rapidly                   |     |
|  |                                                           |     |
|  +----------------------------------------------------------+     |
|                                                                   |
|  Initial Limits (can be increased later):                         |
|  --------------------------------------------------------         |
|  | Maximum position size:     $100                           |     |
|  | Maximum daily loss:        $50                            |     |
|  | Maximum open positions:    3                              |     |
|  --------------------------------------------------------         |
|                                                                   |
|  [x] I understand the risks of live trading                       |     |
|  [x] I accept that losses are real and irreversible               |     |
|  [x] I will start with small amounts                              |     |
|                                                                   |
|                    [Stay in Simulation]  [Enable Live Trading]    |
+------------------------------------------------------------------+
```

### Security Dashboard

```
+------------------------------------------------------------------+
|  Security Status                                                  |
+------------------------------------------------------------------+
|                                                                   |
|  +---------------------------+  +---------------------------+     |
|  | Security Score            |  | Trading Mode              |     |
|  |                           |  |                           |     |
|  |     [85/100]              |  |     [Live Trading]        |     |
|  |     Good                  |  |     Tier 2 Limits         |     |
|  +---------------------------+  +---------------------------+     |
|                                                                   |
|  Security Checklist                                               |
|  --------------------------------------------------------         |
|                                                                   |
|  [Check] Security onboarding completed                            |
|  [Check] API key without withdrawal permissions                   |
|  [Check] IP restriction enabled                                   |
|  [ ]     Two-factor authentication (recommended)                  |
|  [Check] Daily loss limit configured                              |
|  [Check] Circuit breakers enabled                                 |
|                                                                   |
|  Recommendations                                                  |
|  --------------------------------------------------------         |
|                                                                   |
|  [!] Enable two-factor authentication on your exchange            |
|      account for additional security.                             |
|      [Learn how]                                                  |
|                                                                   |
|  Recent Security Events                                           |
|  --------------------------------------------------------         |
|  | Time       | Event                          | Status    |     |
|  | Today 10:45| API key connection test        | Success   |     |
|  | Yesterday  | Daily loss limit reached       | Protected |     |
|  | 3 days ago | New device login               | Verified  |     |
|  --------------------------------------------------------         |
|                                                                   |
+------------------------------------------------------------------+
```

## Progressive Risk Limits

### Tier System

| Tier | Unlock Criteria | Max Position | Max Daily Loss | Max Positions |
|------|-----------------|--------------|----------------|---------------|
| Simulation | Default | Unlimited (sim) | Unlimited (sim) | Unlimited |
| Tier 1 | Complete onboarding | $100 | $50 | 3 |
| Tier 2 | 30 days + $500 volume | $500 | $200 | 5 |
| Tier 3 | 90 days + $5,000 volume | $2,000 | $500 | 10 |
| Tier 4 | 180 days + $25,000 volume | $10,000 | $2,000 | 20 |
| Custom | Manual review | Custom | Custom | Custom |

### Tier Progression

```
Simulation --> Tier 1 --> Tier 2 --> Tier 3 --> Tier 4 --> Custom
    |            |           |           |           |
    v            v           v           v           v
Complete     30 days     90 days    180 days    Request
onboarding   + volume    + volume   + volume    review
```

## Warning System

### Warning Types

| Warning | Trigger | Action |
|---------|---------|--------|
| First Live Trade | First order on live mode | Confirmation required |
| Large Position | > 50% of limit | Warning + confirmation |
| Approaching Loss Limit | > 80% of daily limit | Warning banner |
| Loss Limit Reached | 100% of daily limit | Trading paused |
| Unusual Market | High volatility detected | Information banner |
| Session Timeout | Inactive for 30 min | Re-authentication |

### Warning UI

```
+------------------------------------------------------------------+
|  [!] Warning: Approaching Daily Loss Limit                        |
+------------------------------------------------------------------+
|                                                                   |
|  You have lost $40 today (80% of your $50 daily limit).           |
|                                                                   |
|  If you lose $10 more, trading will be automatically paused       |
|  until tomorrow.                                                  |
|                                                                   |
|  Consider:                                                        |
|  - Reducing position sizes                                        |
|  - Waiting for better market conditions                           |
|  - Reviewing your strategy performance                            |
|                                                                   |
|                              [Dismiss]  [Pause Trading Now]       |
+------------------------------------------------------------------+
```

## Technical Requirements

### Data Storage

| Data | Storage | Encryption |
|------|---------|------------|
| Onboarding progress | Local + Server | Yes |
| Quiz results | Server | Yes |
| Trading tier | Server | Yes |
| Security events | Server | Yes |

### API Endpoints

| Endpoint | Method | Purpose |
|----------|--------|---------|
| `/api/onboarding/status` | GET | Get onboarding progress |
| `/api/onboarding/complete` | POST | Mark module complete |
| `/api/onboarding/quiz` | POST | Submit quiz answers |
| `/api/security/tier` | GET | Get current tier |
| `/api/security/upgrade` | POST | Request tier upgrade |
| `/api/security/events` | GET | Get security events |

### Permission Detection

| Exchange | Method | Permissions Detected |
|----------|--------|----------------------|
| Binance | API call | Read, Trade, Withdraw |
| OKX | API call | Read, Trade, Withdraw |
| Bybit | API call | Read, Trade, Withdraw |
| Coinbase | API call | Read, Trade, Withdraw |

## Non-Functional Requirements

### Performance

| Requirement | Target |
|-------------|--------|
| Onboarding load time | < 2 seconds |
| Quiz response time | < 500ms |
| Permission check | < 3 seconds |

### Accessibility

| Requirement | Implementation |
|-------------|----------------|
| Screen reader | Full support |
| Keyboard navigation | Tab through all elements |
| Color contrast | WCAG AA |
| Text alternatives | All images have alt text |

### Localization

| Language | Priority |
|----------|----------|
| English | P0 |
| Chinese (Simplified) | P1 |
| Spanish | P2 |
| Japanese | P2 |

## Sprint Planning

### Sprint 1-2: Onboarding Flow (2 weeks)

| Task | Estimate | Owner |
|------|----------|-------|
| Onboarding content creation | 3 days | Content |
| Onboarding UI | 4 days | Frontend |
| Progress tracking | 2 days | Backend |
| Quiz system | 2 days | Backend |

**Deliverable:** Complete onboarding flow

### Sprint 3-4: API Key Safety (2 weeks)

| Task | Estimate | Owner |
|------|----------|-------|
| Permission detection | 3 days | Backend |
| Safety checklist UI | 2 days | Frontend |
| Warning system | 3 days | Frontend |
| Integration testing | 2 days | QA |

**Deliverable:** API key safety checks

### Sprint 5-6: Simulation Requirement (2 weeks)

| Task | Estimate | Owner |
|------|----------|-------|
| Simulation mode enforcement | 3 days | Backend |
| Progress tracker | 2 days | Frontend |
| Unlock flow | 2 days | Frontend |
| Testing | 3 days | QA |

**Deliverable:** Simulation requirement system

### Sprint 7-8: Progressive Limits (2 weeks)

| Task | Estimate | Owner |
|------|----------|-------|
| Tier system | 3 days | Backend |
| Limit enforcement | 3 days | Backend |
| Security dashboard | 2 days | Frontend |
| End-to-end testing | 2 days | QA |

**Deliverable:** Progressive limit system

## Dependencies

### External Dependencies

| Dependency | Type | Risk |
|------------|------|------|
| Exchange permission APIs | Technical | Low |
| Content creation | Resources | Medium |

### Internal Dependencies

| Dependency | Feature | Impact |
|------------|---------|--------|
| GUI Configuration | API key entry | Blocking |
| Strategy Marketplace | Deployment gating | Blocking |

## Risks and Mitigations

| Risk | Probability | Impact | Mitigation |
|------|-------------|--------|------------|
| Users find workarounds | Medium | High | Server-side enforcement |
| Onboarding too long | Medium | Medium | Keep under 15 minutes |
| False permission detection | Low | High | Manual override option |
| Users frustrated by limits | Medium | Medium | Clear progression path |

## Success Criteria

### MVP (Month 2)

- [ ] Security onboarding flow
- [ ] API key safety checklist
- [ ] Basic simulation requirement

### Full Release (Month 4)

- [ ] Complete tier system
- [ ] Progressive limits
- [ ] Security dashboard
- [ ] Security incident rate < 0.1%
- [ ] Onboarding completion > 90%

---

*Document Version: 1.0*
*Last Updated: 2026-02-25*
