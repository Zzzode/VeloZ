# PRD: GUI Configuration

## Document Information

| Field | Value |
|-------|-------|
| Feature | GUI Configuration |
| Priority | P0 |
| Status | Draft |
| Owner | Product Manager |
| Target Release | v2.0 |
| Last Updated | 2026-02-25 |

## Executive Summary

Replace VeloZ's current environment variable-based configuration with a visual settings interface that guides users through exchange connection, risk parameters, and system preferences without requiring any command-line interaction.

## Problem Statement

### Current Configuration Process

```
Current User Journey (Technical users only):

1. Read documentation to find required variables
2. Open terminal and set environment variables:
   export VELOZ_BINANCE_API_KEY=xxx
   export VELOZ_BINANCE_API_SECRET=xxx
   export VELOZ_EXECUTION_MODE=binance_testnet_spot
   ...
3. Restart gateway to apply changes
4. Debug configuration errors via logs
5. Repeat for each configuration change
```

### User Pain Points

| Pain Point | Impact | Frequency |
|------------|--------|-----------|
| Environment variable syntax | Configuration errors | Every change |
| No validation feedback | Silent failures | Common |
| Restart required for changes | Workflow disruption | Every change |
| API key exposure in shell history | Security risk | Every setup |
| No guided workflow | User confusion | First-time setup |

### Target User Profile

**Persona: Maria, Part-Time Trader**
- Age: 42, trades crypto evenings and weekends
- Technical skills: Comfortable with desktop apps, avoids terminal
- Goal: Connect to Binance and configure risk limits
- Frustration: "I don't know what half these environment variables mean"

## Goals and Success Metrics

### Goals

1. **Accessibility**: Any user can configure VeloZ through visual interfaces
2. **Safety**: Prevent common configuration mistakes through validation
3. **Security**: Secure storage of sensitive credentials
4. **Guidance**: Contextual help explains every setting

### Success Metrics

| Metric | Current | Target | Measurement |
|--------|---------|--------|-------------|
| Configuration success rate | ~70% | > 95% | Analytics |
| Time to configure exchange | 15+ min | < 5 min | Analytics |
| Configuration-related support tickets | High | < 10% of tickets | Support |
| API key exposure incidents | Unknown | 0 | Security audit |

## User Stories

### US-1: First-Run Setup Wizard

**As a** new VeloZ user
**I want to** be guided through initial setup
**So that** I can start trading quickly without reading documentation

**Acceptance Criteria:**
- [ ] Wizard launches automatically on first run
- [ ] Step-by-step flow: Welcome > Exchange > Risk > Complete
- [ ] Progress indicator shows current step
- [ ] Can skip optional steps
- [ ] Can return to wizard from settings

### US-2: Exchange Connection

**As a** trader
**I want to** connect my exchange account visually
**So that** I can trade without using environment variables

**Acceptance Criteria:**
- [ ] Select exchange from dropdown (Binance, OKX, Bybit, Coinbase)
- [ ] Input fields for API key and secret
- [ ] Password-style masking for secrets
- [ ] "Test Connection" button validates credentials
- [ ] Clear success/failure feedback
- [ ] Secure storage (not plain text)

### US-3: Risk Parameter Configuration

**As a** trader
**I want to** configure risk limits visually
**So that** I can protect my capital without manual calculations

**Acceptance Criteria:**
- [ ] Sliders for percentage-based limits
- [ ] Input fields for absolute values
- [ ] Real-time preview of effective limits
- [ ] Validation prevents dangerous configurations
- [ ] Presets for conservative/moderate/aggressive
- [ ] Tooltips explain each parameter

### US-4: Connection Testing

**As a** trader
**I want to** test my exchange connection before trading
**So that** I know my configuration is correct

**Acceptance Criteria:**
- [ ] "Test Connection" button on exchange settings
- [ ] Tests API key validity
- [ ] Tests API key permissions
- [ ] Shows account balance on success
- [ ] Clear error message on failure
- [ ] Suggests fixes for common errors

### US-5: Settings Persistence

**As a** trader
**I want to** have my settings saved automatically
**So that** I don't have to reconfigure after restart

**Acceptance Criteria:**
- [ ] Settings saved to secure local storage
- [ ] Settings loaded on application start
- [ ] Export settings to file (excluding secrets)
- [ ] Import settings from file
- [ ] Reset to defaults option

### US-6: Advanced Settings

**As an** experienced trader
**I want to** access advanced configuration options
**So that** I can fine-tune VeloZ behavior

**Acceptance Criteria:**
- [ ] Advanced section collapsed by default
- [ ] Network settings (timeouts, retries)
- [ ] Logging level configuration
- [ ] Performance tuning options
- [ ] Warning when changing advanced settings

## User Flows

### Flow 1: First-Run Setup Wizard

```
+------------------+     +------------------+     +------------------+
|   Welcome        |     |   Exchange       |     |   Risk           |
|   Screen         | --> |   Setup          | --> |   Configuration  |
+------------------+     +--------+---------+     +--------+---------+
                                  |                        |
                                  v                        v
                         +------------------+     +------------------+
                         |   Test           |     |   Review &       |
                         |   Connection     |     |   Complete       |
                         +------------------+     +------------------+
```

### Flow 2: Exchange Connection

```
+------------------+     +------------------+     +------------------+
|   Select         |     |   Enter API      |     |   Test           |
|   Exchange       | --> |   Credentials    | --> |   Connection     |
+------------------+     +------------------+     +--------+---------+
                                                          |
                              +---------------------------+
                              |                           |
                              v                           v
                    +------------------+        +------------------+
                    |   Success:       |        |   Failure:       |
                    |   Show Balance   |        |   Show Error     |
                    +------------------+        +------------------+
```

### Flow 3: Risk Configuration

```
+------------------+     +------------------+     +------------------+
|   Select         |     |   Customize      |     |   Review         |
|   Preset         | --> |   Parameters     | --> |   Effective      |
|   (Optional)     |     |                  |     |   Limits         |
+------------------+     +------------------+     +--------+---------+
                                                          |
                                                          v
                                                +------------------+
                                                |   Save           |
                                                |   Configuration  |
                                                +------------------+
```

## Wireframes

### First-Run Wizard - Welcome

```
+------------------------------------------------------------------+
|  VeloZ Setup                                             [X]      |
+------------------------------------------------------------------+
|                                                                   |
|                         [VeloZ Logo]                              |
|                                                                   |
|                    Welcome to VeloZ                               |
|                                                                   |
|    Let's get you set up for trading in just a few steps.          |
|                                                                   |
|    +----------------------------------------------------------+   |
|    |  [1]  Exchange    [2]  Risk     [3]  Complete            |   |
|    |   *                o             o                        |   |
|    +----------------------------------------------------------+   |
|                                                                   |
|    What we'll configure:                                          |
|    - Connect your exchange account                                |
|    - Set up risk management limits                                |
|    - Verify everything works                                      |
|                                                                   |
|                                        [Skip Setup]  [Get Started]|
+------------------------------------------------------------------+
```

### First-Run Wizard - Exchange Setup

```
+------------------------------------------------------------------+
|  VeloZ Setup                                             [X]      |
+------------------------------------------------------------------+
|                                                                   |
|    Connect Your Exchange                                          |
|    --------------------------------------------------------       |
|                                                                   |
|    +----------------------------------------------------------+   |
|    |  [1]  Exchange    [2]  Risk     [3]  Complete            |   |
|    |       *            o             o                        |   |
|    +----------------------------------------------------------+   |
|                                                                   |
|    Exchange:                                                      |
|    +--------------------------------------------------+           |
|    | Binance                                      [v] |           |
|    +--------------------------------------------------+           |
|                                                                   |
|    API Key:                                                       |
|    +--------------------------------------------------+           |
|    | xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx                 |           |
|    +--------------------------------------------------+           |
|                                                                   |
|    API Secret:                                                    |
|    +--------------------------------------------------+           |
|    | ********************************            [Show]|           |
|    +--------------------------------------------------+           |
|                                                                   |
|    [?] How do I get API keys?                                     |
|                                                                   |
|    [ ] Use testnet (recommended for beginners)                    |
|                                                                   |
|                         [Test Connection]                         |
|                                                                   |
|                                        [< Back]  [Next >]         |
+------------------------------------------------------------------+
```

### Exchange Connection - Test Success

```
+------------------------------------------------------------------+
|  VeloZ Setup                                             [X]      |
+------------------------------------------------------------------+
|                                                                   |
|    Connect Your Exchange                                          |
|    --------------------------------------------------------       |
|                                                                   |
|    +----------------------------------------------------------+   |
|    |  [1]  Exchange    [2]  Risk     [3]  Complete            |   |
|    |       *            o             o                        |   |
|    +----------------------------------------------------------+   |
|                                                                   |
|    +----------------------------------------------------------+   |
|    |  [Check Icon]  Connection Successful!                     |   |
|    |                                                           |   |
|    |  Account: user@example.com                                |   |
|    |  Permissions: Read, Spot Trading                          |   |
|    |                                                           |   |
|    |  Available Balance:                                       |   |
|    |    USDT: 10,000.00                                        |   |
|    |    BTC:  0.5000                                           |   |
|    |    ETH:  5.0000                                           |   |
|    +----------------------------------------------------------+   |
|                                                                   |
|                                        [< Back]  [Next >]         |
+------------------------------------------------------------------+
```

### Exchange Connection - Test Failure

```
+------------------------------------------------------------------+
|  VeloZ Setup                                             [X]      |
+------------------------------------------------------------------+
|                                                                   |
|    Connect Your Exchange                                          |
|    --------------------------------------------------------       |
|                                                                   |
|    +----------------------------------------------------------+   |
|    |  [1]  Exchange    [2]  Risk     [3]  Complete            |   |
|    |       *            o             o                        |   |
|    +----------------------------------------------------------+   |
|                                                                   |
|    +----------------------------------------------------------+   |
|    |  [X Icon]  Connection Failed                              |   |
|    |                                                           |   |
|    |  Error: Invalid API key                                   |   |
|    |                                                           |   |
|    |  Possible causes:                                         |   |
|    |  - API key was copied incorrectly                         |   |
|    |  - API key has been revoked                               |   |
|    |  - Using mainnet key with testnet selected                |   |
|    |                                                           |   |
|    |  [View troubleshooting guide]                             |   |
|    +----------------------------------------------------------+   |
|                                                                   |
|                         [Try Again]                               |
|                                                                   |
|                                        [< Back]  [Skip]           |
+------------------------------------------------------------------+
```

### Risk Configuration

```
+------------------------------------------------------------------+
|  VeloZ Setup                                             [X]      |
+------------------------------------------------------------------+
|                                                                   |
|    Configure Risk Limits                                          |
|    --------------------------------------------------------       |
|                                                                   |
|    +----------------------------------------------------------+   |
|    |  [1]  Exchange    [2]  Risk     [3]  Complete            |   |
|    |       o            *             o                        |   |
|    +----------------------------------------------------------+   |
|                                                                   |
|    Quick Setup:                                                   |
|    +----------------+  +----------------+  +----------------+     |
|    | Conservative   |  | Moderate       |  | Aggressive     |     |
|    | Max 2% risk    |  | Max 5% risk    |  | Max 10% risk   |     |
|    +----------------+  +----------------+  +----------------+     |
|                                                                   |
|    Custom Settings:                                               |
|    --------------------------------------------------------       |
|                                                                   |
|    Maximum Position Size (% of portfolio):                        |
|    [==========|----------] 10%                           [?]      |
|                                                                   |
|    Maximum Daily Loss (% of portfolio):                           |
|    [=====|---------------] 5%                            [?]      |
|                                                                   |
|    Maximum Open Positions:                                        |
|    +--------+                                                     |
|    |   5    |                                            [?]      |
|    +--------+                                                     |
|                                                                   |
|    +----------------------------------------------------------+   |
|    |  Preview: With $10,000 portfolio                          |   |
|    |  - Max position: $1,000                                   |   |
|    |  - Max daily loss: $500                                   |   |
|    |  - Trading will pause if daily loss exceeds $500          |   |
|    +----------------------------------------------------------+   |
|                                                                   |
|                                        [< Back]  [Next >]         |
+------------------------------------------------------------------+
```

### Settings Panel - Exchange Tab

```
+------------------------------------------------------------------+
|  Settings                                                [X]      |
+------------------------------------------------------------------+
|                                                                   |
|  +----------+  +------+  +----------+  +--------+                 |
|  | Exchange |  | Risk |  | Advanced |  | About  |                 |
|  +----------+  +------+  +----------+  +--------+                 |
|                                                                   |
|  Exchange Connections                                             |
|  --------------------------------------------------------         |
|                                                                   |
|  +----------------------------------------------------------+     |
|  |  [Binance Logo]  Binance                                  |     |
|  |  Status: Connected                      [Edit] [Remove]   |     |
|  |  Last tested: 2 minutes ago                               |     |
|  +----------------------------------------------------------+     |
|                                                                   |
|  +----------------------------------------------------------+     |
|  |  [+]  Add Exchange Connection                             |     |
|  +----------------------------------------------------------+     |
|                                                                   |
|  Active Exchange:                                                 |
|  +--------------------------------------------------+             |
|  | Binance                                      [v] |             |
|  +--------------------------------------------------+             |
|                                                                   |
|  Trading Mode:                                                    |
|  ( ) Simulation (no real trades)                                  |
|  (o) Testnet (test with fake money)                               |
|  ( ) Live Trading (real money) [Requires security verification]   |
|                                                                   |
|                                        [Cancel]  [Save]           |
+------------------------------------------------------------------+
```

### Settings Panel - Risk Tab

```
+------------------------------------------------------------------+
|  Settings                                                [X]      |
+------------------------------------------------------------------+
|                                                                   |
|  +----------+  +------+  +----------+  +--------+                 |
|  | Exchange |  | Risk |  | Advanced |  | About  |                 |
|  +----------+  +------+  +----------+  +--------+                 |
|                                                                   |
|  Risk Management                                                  |
|  --------------------------------------------------------         |
|                                                                   |
|  Position Limits                                                  |
|  --------------------------------------------------------         |
|                                                                   |
|  Maximum Position Size:                                           |
|  [==========|----------] 10%  ($1,000)                   [?]      |
|                                                                   |
|  Maximum Notional Value:                                          |
|  +------------------+                                             |
|  | $5,000           |                                    [?]      |
|  +------------------+                                             |
|                                                                   |
|  Loss Limits                                                      |
|  --------------------------------------------------------         |
|                                                                   |
|  Maximum Daily Loss:                                              |
|  [=====|---------------] 5%  ($500)                      [?]      |
|                                                                   |
|  Maximum Drawdown:                                                |
|  [=======|-------------] 15%  ($1,500)                   [?]      |
|                                                                   |
|  Circuit Breakers                                                 |
|  --------------------------------------------------------         |
|                                                                   |
|  [x] Pause trading on daily loss limit                            |
|  [x] Pause trading on drawdown limit                              |
|  [ ] Pause trading on consecutive losses (5)                      |
|                                                                   |
|                                        [Cancel]  [Save]           |
+------------------------------------------------------------------+
```

## Field Validation Rules

### Exchange Credentials

| Field | Validation | Error Message |
|-------|------------|---------------|
| API Key | Required, 64 chars | "API key is required" / "API key should be 64 characters" |
| API Secret | Required, 64 chars | "API secret is required" / "API secret should be 64 characters" |
| OKX Passphrase | Required for OKX | "Passphrase is required for OKX" |

### Risk Parameters

| Field | Validation | Error Message |
|-------|------------|---------------|
| Max Position Size | 1-100%, default 10% | "Position size must be between 1% and 100%" |
| Max Daily Loss | 1-50%, default 5% | "Daily loss limit must be between 1% and 50%" |
| Max Drawdown | 5-100%, default 20% | "Drawdown limit must be between 5% and 100%" |
| Max Open Positions | 1-100, default 10 | "Must have at least 1 position allowed" |

### Validation Warnings

| Condition | Warning Message |
|-----------|-----------------|
| Max position > 25% | "High position size increases risk. Consider reducing." |
| Max daily loss > 10% | "High daily loss limit. Consider a more conservative setting." |
| Circuit breakers disabled | "Disabling circuit breakers removes important safety nets." |

## Technical Requirements

### Secure Storage

| Platform | Storage Method | Encryption |
|----------|----------------|------------|
| Windows | Windows Credential Manager | DPAPI |
| macOS | Keychain | AES-256 |
| Linux | libsecret / KWallet | AES-256 |

### Configuration File Format

```yaml
# ~/.veloz/config.yaml (non-sensitive settings)
version: 1
exchanges:
  active: binance
  connections:
    - id: binance-main
      exchange: binance
      mode: testnet
      # credentials stored in secure storage
risk:
  max_position_pct: 10
  max_daily_loss_pct: 5
  max_drawdown_pct: 20
  max_open_positions: 10
  circuit_breakers:
    daily_loss: true
    drawdown: true
    consecutive_losses: false
advanced:
  log_level: info
  network_timeout_ms: 30000
```

### API Integration

| Endpoint | Purpose | Method |
|----------|---------|--------|
| `/api/config` | Get current config | GET |
| `/api/config` | Update config | PUT |
| `/api/config/test-exchange` | Test exchange connection | POST |
| `/api/config/validate` | Validate config | POST |

## Error Messages

### Exchange Connection Errors

| Error Code | User Message | Suggested Action |
|------------|--------------|------------------|
| `invalid_api_key` | "API key is invalid or has been revoked" | "Check your API key on the exchange website" |
| `invalid_signature` | "API secret is incorrect" | "Re-enter your API secret" |
| `ip_restricted` | "Your IP is not whitelisted on the exchange" | "Add your IP to the exchange API whitelist" |
| `insufficient_permissions` | "API key lacks required permissions" | "Enable 'Spot Trading' permission on exchange" |
| `rate_limited` | "Too many requests. Please wait and try again." | "Wait 60 seconds before testing again" |
| `network_error` | "Could not connect to exchange" | "Check your internet connection" |

### Validation Errors

| Error Code | User Message |
|------------|--------------|
| `required_field` | "{field} is required" |
| `invalid_format` | "{field} format is invalid" |
| `out_of_range` | "{field} must be between {min} and {max}" |
| `dangerous_config` | "This configuration may result in significant losses" |

## Non-Functional Requirements

### Performance

| Requirement | Target |
|-------------|--------|
| Settings panel load time | < 500ms |
| Connection test response | < 5 seconds |
| Settings save time | < 1 second |
| Config validation | < 100ms |

### Security

| Requirement | Implementation |
|-------------|----------------|
| Credential storage | Platform secure storage (Keychain, Credential Manager) |
| Memory handling | Clear secrets from memory after use |
| Logging | Never log credentials |
| Export | Exclude credentials from config export |

### Accessibility

| Requirement | Implementation |
|-------------|----------------|
| Screen reader | All form fields labeled |
| Keyboard | Tab navigation, Enter to submit |
| Error announcement | Errors announced to screen readers |
| Color contrast | WCAG AA compliance |

## Sprint Planning

### Sprint 1-2: Core Settings Infrastructure (2 weeks)

| Task | Estimate | Owner |
|------|----------|-------|
| Settings storage abstraction | 2 days | Backend |
| Secure credential storage | 3 days | Backend |
| Settings API endpoints | 2 days | Backend |
| Settings panel shell | 2 days | Frontend |
| Unit tests | 1 day | QA |

**Deliverable:** Settings infrastructure with secure storage

### Sprint 3-4: Exchange Configuration (2 weeks)

| Task | Estimate | Owner |
|------|----------|-------|
| Exchange selection UI | 2 days | Frontend |
| Credential input forms | 2 days | Frontend |
| Connection testing | 3 days | Backend |
| Error handling | 2 days | Frontend |
| Integration tests | 1 day | QA |

**Deliverable:** Working exchange configuration

### Sprint 5-6: Risk Configuration (2 weeks)

| Task | Estimate | Owner |
|------|----------|-------|
| Risk parameter UI | 3 days | Frontend |
| Slider components | 2 days | Frontend |
| Validation logic | 2 days | Backend |
| Preview calculations | 1 day | Frontend |
| User testing | 2 days | QA |

**Deliverable:** Working risk configuration

### Sprint 7-8: First-Run Wizard (2 weeks)

| Task | Estimate | Owner |
|------|----------|-------|
| Wizard flow implementation | 3 days | Frontend |
| Step navigation | 2 days | Frontend |
| Progress persistence | 1 day | Backend |
| Help content | 2 days | Content |
| End-to-end testing | 2 days | QA |

**Deliverable:** Complete first-run wizard

## Dependencies

### External Dependencies

| Dependency | Type | Risk |
|------------|------|------|
| Platform secure storage APIs | Technical | Low |
| Exchange API documentation | Documentation | Low |

### Internal Dependencies

| Dependency | Feature | Impact |
|------------|---------|--------|
| One-Click Installer | First-run trigger | Blocking |
| Security Education | Live trading unlock | Blocking |

## Risks and Mitigations

| Risk | Probability | Impact | Mitigation |
|------|-------------|--------|------------|
| Platform storage differences | Medium | Medium | Abstract storage layer |
| Exchange API changes | Low | Medium | Adapter pattern |
| User enters wrong credentials | High | Low | Clear error messages |
| Credential migration from env vars | Medium | Low | Import wizard |

## Success Criteria

### MVP (Month 2)

- [ ] Exchange connection via GUI
- [ ] Basic risk parameter configuration
- [ ] Connection testing
- [ ] Settings persistence

### Full Release (Month 4)

- [ ] First-run setup wizard
- [ ] All exchange adapters
- [ ] Advanced settings
- [ ] Import/export functionality
- [ ] Configuration success rate > 95%

---

*Document Version: 1.0*
*Last Updated: 2026-02-25*
