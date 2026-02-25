# P0 Feature 2: GUI Configuration UI

## Overview

The configuration UI provides a user-friendly interface for setting up VeloZ, managing API keys, configuring risk parameters, and testing connections. It includes both a first-run setup wizard and a comprehensive settings dashboard.

---

## Part A: First-Run Setup Wizard

### User Flow
```
[Welcome] -> [Exchange Selection] -> [API Keys] -> [Risk Settings] -> [Test Connection] -> [Ready]
```

---

### Wizard Screen 1: Welcome

```
+------------------------------------------------------------------+
|                                                                  |
|  +----+  +----+  +----+  +----+  +----+                         |
|  | 1  |--| 2  |--| 3  |--| 4  |--| 5  |                         |
|  +----+  +----+  +----+  +----+  +----+                         |
|  Welcome Exchange  API   Risk   Ready                            |
|                                                                  |
+------------------------------------------------------------------+
|                                                                  |
|                    Welcome to VeloZ Setup                        |
|                                                                  |
|     Let's get your trading platform configured. This wizard      |
|     will guide you through:                                      |
|                                                                  |
|     +------------------------------------------------------+     |
|     |                                                      |     |
|     |  [Exchange Icon]  Connect Your Exchange              |     |
|     |                   Link your Binance, OKX, or other   |     |
|     |                   exchange account                   |     |
|     |                                                      |     |
|     |  [Shield Icon]    Set Risk Parameters                |     |
|     |                   Configure position limits and      |     |
|     |                   safety controls                    |     |
|     |                                                      |     |
|     |  [Test Icon]      Test Connection                    |     |
|     |                   Verify everything works before     |     |
|     |                   you start trading                  |     |
|     |                                                      |     |
|     +------------------------------------------------------+     |
|                                                                  |
|     Estimated time: 5 minutes                                    |
|                                                                  |
|     +------------------------------------------------------+     |
|     | [i] You can skip this wizard and configure later     |     |
|     |     from Settings > Configuration                    |     |
|     +------------------------------------------------------+     |
|                                                                  |
|     [Skip for Now]                        [Start Setup ->]       |
|                                                                  |
+------------------------------------------------------------------+
```

---

### Wizard Screen 2: Exchange Selection

```
+------------------------------------------------------------------+
|                                                                  |
|  +----+  +----+  +----+  +----+  +----+                         |
|  |(1) |--| 2  |--| 3  |--| 4  |--| 5  |                         |
|  +----+  +----+  +----+  +----+  +----+                         |
|  Welcome Exchange  API   Risk   Ready                            |
|                                                                  |
+------------------------------------------------------------------+
|                                                                  |
|                    Select Your Exchange                          |
|                                                                  |
|     Choose the exchange(s) you want to connect to VeloZ.         |
|                                                                  |
|     +------------------------+  +------------------------+       |
|     |                        |  |                        |       |
|     |    [Binance Logo]      |  |     [OKX Logo]         |       |
|     |                        |  |                        |       |
|     |       Binance          |  |        OKX             |       |
|     |                        |  |                        |       |
|     |  Spot & Futures        |  |  Spot & Futures        |       |
|     |  [Select]              |  |  [Select]              |       |
|     |                        |  |                        |       |
|     +------------------------+  +------------------------+       |
|                                                                  |
|     +------------------------+  +------------------------+       |
|     |                        |  |                        |       |
|     |    [Bybit Logo]        |  |    [More Icon]         |       |
|     |                        |  |                        |       |
|     |       Bybit            |  |    More Exchanges      |       |
|     |                        |  |                        |       |
|     |  Spot & Futures        |  |  Coming Soon           |       |
|     |  [Select]              |  |  [Notify Me]           |       |
|     |                        |  |                        |       |
|     +------------------------+  +------------------------+       |
|                                                                  |
|     Selected: Binance                                            |
|                                                                  |
|     [<- Back]                              [Continue ->]         |
|                                                                  |
+------------------------------------------------------------------+
```

### Specifications
- **Exchange Cards**: 200px x 180px, hover shadow effect
- **Selected State**: Primary border (2px), checkmark badge
- **Logo Size**: 64px x 64px
- **Multi-select**: Users can select multiple exchanges

---

### Wizard Screen 3: API Key Configuration

```
+------------------------------------------------------------------+
|                                                                  |
|  +----+  +----+  +----+  +----+  +----+                         |
|  |(1) |-|(2) |--| 3  |--| 4  |--| 5  |                         |
|  +----+  +----+  +----+  +----+  +----+                         |
|  Welcome Exchange  API   Risk   Ready                            |
|                                                                  |
+------------------------------------------------------------------+
|                                                                  |
|                    Configure Binance API                         |
|                                                                  |
|     +------------------------------------------------------+     |
|     | [!] Security Notice                                  |     |
|     |                                                      |     |
|     | - Never share your API keys with anyone              |     |
|     | - Enable IP restrictions on your exchange            |     |
|     | - Use read-only keys for testing                     |     |
|     +------------------------------------------------------+     |
|                                                                  |
|     API Key *                                                    |
|     +------------------------------------------------------+     |
|     | xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx             |     |
|     +------------------------------------------------------+     |
|                                                                  |
|     API Secret *                                                 |
|     +--------------------------------------------------+ [Eye]   |
|     | ****************************************         |         |
|     +--------------------------------------------------+         |
|                                                                  |
|     Permissions Required:                                        |
|     [Check] Read account information                             |
|     [Check] Spot trading (for spot strategies)                   |
|     [Check] Futures trading (for derivatives)                    |
|     [X]     Withdrawal (NOT required - do not enable)            |
|                                                                  |
|     [?] How do I create API keys?                               |
|                                                                  |
|     [<- Back]                              [Continue ->]         |
|                                                                  |
+------------------------------------------------------------------+
```

### Specifications
- **Security Notice**: warning-light background, warning border
- **API Key Input**: Monospace font, full width
- **Secret Input**: Password type with toggle visibility
- **Permissions Checklist**:
  - Green check: Required and recommended
  - Red X: Not required, security risk if enabled
- **Help Link**: Opens tutorial modal or external guide

---

### Wizard Screen 4: Risk Parameters

```
+------------------------------------------------------------------+
|                                                                  |
|  +----+  +----+  +----+  +----+  +----+                         |
|  |(1) |-|(2) |-|(3) |--| 4  |--| 5  |                         |
|  +----+  +----+  +----+  +----+  +----+                         |
|  Welcome Exchange  API   Risk   Ready                            |
|                                                                  |
+------------------------------------------------------------------+
|                                                                  |
|                    Risk Parameters                               |
|                                                                  |
|     Configure safety limits to protect your capital.             |
|                                                                  |
|     +------------------------------------------------------+     |
|     | [Shield] Recommended Settings for Beginners          |     |
|     |          Start conservative - you can adjust later   |     |
|     +------------------------------------------------------+     |
|                                                                  |
|     Maximum Position Size                                        |
|     +--------------------------------------------------+         |
|     |                                              | % |         |
|     +--------------------------------------------------+         |
|     [Slider: 1% ----[10%]------------------------ 100%]          |
|     Limits each position to 10% of your account balance          |
|                                                                  |
|     Daily Loss Limit                                             |
|     +--------------------------------------------------+         |
|     |                                              | % |         |
|     +--------------------------------------------------+         |
|     [Slider: 1% ------[5%]----------------------- 50%]           |
|     Trading stops if daily losses exceed 5%                      |
|                                                                  |
|     Maximum Open Positions                                       |
|     +--------------------------------------------------+         |
|     |                                              | # |         |
|     +--------------------------------------------------+         |
|     [Slider: 1 --------[3]----------------------- 20]            |
|     Maximum 3 positions open simultaneously                      |
|                                                                  |
|     [Advanced Settings v]                                        |
|                                                                  |
|     [<- Back]                              [Continue ->]         |
|                                                                  |
+------------------------------------------------------------------+
```

### Advanced Settings (Expandable)
```
+------------------------------------------------------+
| Advanced Risk Settings                               |
+------------------------------------------------------+
|                                                      |
| Maximum Leverage                                     |
| [Slider: 1x ------[5x]----------------------- 125x]  |
|                                                      |
| Order Size Limit (USD)                               |
| +------------------------------------------------+   |
| | 1000                                           |   |
| +------------------------------------------------+   |
|                                                      |
| Cooldown After Loss (minutes)                        |
| +------------------------------------------------+   |
| | 30                                             |   |
| +------------------------------------------------+   |
|                                                      |
| [ ] Enable kill switch (emergency stop all)          |
| [ ] Require confirmation for orders > $500           |
|                                                      |
+------------------------------------------------------+
```

### Specifications
- **Sliders**: Primary color track, circular thumb
- **Input + Slider Sync**: Value updates both controls
- **Helper Text**: Below each slider, explains impact
- **Advanced Toggle**: Chevron icon, expands smoothly

---

### Wizard Screen 5: Connection Test

```
+------------------------------------------------------------------+
|                                                                  |
|  +----+  +----+  +----+  +----+  +----+                         |
|  |(1) |-|(2) |-|(3) |-|(4) |--| 5  |                           |
|  +----+  +----+  +----+  +----+  +----+                         |
|  Welcome Exchange  API   Risk   Ready                            |
|                                                                  |
+------------------------------------------------------------------+
|                                                                  |
|                    Testing Connection                            |
|                                                                  |
|     Verifying your configuration...                              |
|                                                                  |
|     +------------------------------------------------------+     |
|     |                                                      |     |
|     |  [Check] API Key Valid                               |     |
|     |          Connected to Binance successfully           |     |
|     |                                                      |     |
|     |  [Check] Permissions Verified                        |     |
|     |          Spot trading: Enabled                       |     |
|     |          Futures trading: Enabled                    |     |
|     |                                                      |     |
|     |  [Check] Account Balance Retrieved                   |     |
|     |          Total: $12,450.32 USDT                      |     |
|     |                                                      |     |
|     |  [Check] Market Data Connection                      |     |
|     |          Latency: 45ms                               |     |
|     |                                                      |     |
|     +------------------------------------------------------+     |
|                                                                  |
|     +------------------------------------------------------+     |
|     | [Check] All tests passed! Your setup is complete.    |     |
|     +------------------------------------------------------+     |
|                                                                  |
|     [<- Back]                              [Finish Setup ->]     |
|                                                                  |
+------------------------------------------------------------------+
```

### Test Failure State
```
+------------------------------------------------------+
|                                                      |
|  [Check] API Key Valid                               |
|          Connected to Binance successfully           |
|                                                      |
|  [X] Permissions Verification Failed                 |
|      Spot trading permission not enabled             |
|                                                      |
|      [Fix This ->]                                   |
|                                                      |
+------------------------------------------------------+
```

### Specifications
- **Test Items**: Animated appearance, 500ms delay between each
- **Status Icons**:
  - Pending: Gray spinner
  - Success: Green checkmark
  - Failed: Red X
- **Fix Button**: Links to relevant step or external guide

---

### Wizard Screen 6: Setup Complete

```
+------------------------------------------------------------------+
|                                                                  |
|  +----+  +----+  +----+  +----+  +----+                         |
|  |(1) |-|(2) |-|(3) |-|(4) |-|(5) |                             |
|  +----+  +----+  +----+  +----+  +----+                         |
|  Welcome Exchange  API   Risk   Ready                            |
|                                                                  |
+------------------------------------------------------------------+
|                                                                  |
|                    [Large Green Checkmark]                       |
|                                                                  |
|                    You're All Set!                               |
|                                                                  |
|     Your VeloZ trading platform is ready to use.                 |
|                                                                  |
|     +------------------------------------------------------+     |
|     |                                                      |     |
|     |  [TestTube] Start in Simulated Mode                  |     |
|     |             Practice with paper trading first        |     |
|     |             (Recommended for beginners)              |     |
|     |                                                      |     |
|     |                              [Start Simulated ->]    |     |
|     |                                                      |     |
|     +------------------------------------------------------+     |
|                                                                  |
|     +------------------------------------------------------+     |
|     |                                                      |     |
|     |  [Zap] Start in Live Mode                            |     |
|     |        Trade with real funds immediately             |     |
|     |        (For experienced traders)                     |     |
|     |                                                      |     |
|     |                              [Start Live ->]         |     |
|     |                                                      |     |
|     +------------------------------------------------------+     |
|                                                                  |
|     [Explore Strategies]    [View Documentation]                 |
|                                                                  |
+------------------------------------------------------------------+
```

---

## Part B: Settings Dashboard

### Main Settings Layout

```
+------------------------------------------------------------------+
|  Settings                                                        |
+------------------------------------------------------------------+
|                                                                  |
|  +----------------+  +---------------------------------------+   |
|  |                |  |                                       |   |
|  |  [User] General|  |  Exchange Connections                 |   |
|  |                |  |                                       |   |
|  |  [Key] API Keys|  |  Manage your exchange API keys and    |   |
|  |                |  |  connection settings.                 |   |
|  |  [Link] Connect|  |                                       |   |
|  |                |  |  +----------------------------------+  |   |
|  |  [Shield] Risk |  |  |  [Binance Logo]                  |  |   |
|  |                |  |  |                                  |  |   |
|  |  [Bell] Alerts |  |  |  Binance                         |  |   |
|  |                |  |  |  Status: Connected               |  |   |
|  |  [Palette] UI  |  |  |  Last sync: 2 min ago            |  |   |
|  |                |  |  |                                  |  |   |
|  |  [Database]    |  |  |  [Test] [Edit] [Disconnect]      |  |   |
|  |  Data          |  |  +----------------------------------+  |   |
|  |                |  |                                       |   |
|  |  [Info] About  |  |  +----------------------------------+  |   |
|  |                |  |  |  [+] Add Exchange Connection     |  |   |
|  |                |  |  +----------------------------------+  |   |
|  |                |  |                                       |   |
|  +----------------+  +---------------------------------------+   |
|                                                                  |
+------------------------------------------------------------------+
```

### Specifications
- **Sidebar**: 200px width, fixed position
- **Content Area**: Flexible width, max-width 800px
- **Section Cards**: Full width, rounded-lg, shadow-sm

---

### API Key Management Panel

```
+------------------------------------------------------------------+
|  API Key Management                                              |
+------------------------------------------------------------------+
|                                                                  |
|  +--------------------------------------------------------------+|
|  | Binance API Key                                    [Edit]    ||
|  +--------------------------------------------------------------+|
|  |                                                              ||
|  |  API Key:     xxxxxxxx...xxxxxxxx          [Copy]            ||
|  |  Created:     Feb 15, 2026                                   ||
|  |  Last Used:   2 minutes ago                                  ||
|  |                                                              ||
|  |  Permissions:                                                ||
|  |  [Check] Read    [Check] Spot    [Check] Futures   [X] Withdraw||
|  |                                                              ||
|  |  IP Whitelist:   192.168.1.100, 10.0.0.1                    ||
|  |                                                              ||
|  +--------------------------------------------------------------+|
|                                                                  |
|  +--------------------------------------------------------------+|
|  | Security Recommendations                                     ||
|  +--------------------------------------------------------------+|
|  |                                                              ||
|  |  [!] Enable IP whitelist on Binance for added security       ||
|  |      [Learn How ->]                                          ||
|  |                                                              ||
|  |  [Check] API key does not have withdrawal permission         ||
|  |                                                              ||
|  +--------------------------------------------------------------+|
|                                                                  |
+------------------------------------------------------------------+
```

### Edit API Key Modal

```
+--------------------------------------------------+
|  Edit Binance API Key                    [X]     |
+--------------------------------------------------+
|                                                  |
|  API Key                                         |
|  +--------------------------------------------+  |
|  | xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx   |  |
|  +--------------------------------------------+  |
|                                                  |
|  API Secret                                      |
|  +----------------------------------------+ [Eye]|
|  | **************************************** |    |
|  +----------------------------------------+      |
|                                                  |
|  +--------------------------------------------+  |
|  | [!] Changing your API key will require    |  |
|  |     re-verification of permissions.       |  |
|  +--------------------------------------------+  |
|                                                  |
|  [Delete Key]              [Cancel] [Save]       |
|                                                  |
+--------------------------------------------------+
```

---

### Risk Settings Panel

```
+------------------------------------------------------------------+
|  Risk Management Settings                                        |
+------------------------------------------------------------------+
|                                                                  |
|  Current Risk Profile: [Conservative v]                          |
|                                                                  |
|  +--------------------------------------------------------------+|
|  | Position Limits                                              ||
|  +--------------------------------------------------------------+|
|  |                                                              ||
|  |  Maximum Position Size         Maximum Open Positions        ||
|  |  +------------------------+    +------------------------+    ||
|  |  | 10                 | % |    | 3                  | # |    ||
|  |  +------------------------+    +------------------------+    ||
|  |                                                              ||
|  |  Maximum Leverage              Order Size Limit              ||
|  |  +------------------------+    +------------------------+    ||
|  |  | 5                  | x |    | 1000               | $ |    ||
|  |  +------------------------+    +------------------------+    ||
|  |                                                              ||
|  +--------------------------------------------------------------+|
|                                                                  |
|  +--------------------------------------------------------------+|
|  | Loss Limits                                                  ||
|  +--------------------------------------------------------------+|
|  |                                                              ||
|  |  Daily Loss Limit              Weekly Loss Limit             ||
|  |  +------------------------+    +------------------------+    ||
|  |  | 5                  | % |    | 15                 | % |    ||
|  |  +------------------------+    +------------------------+    ||
|  |                                                              ||
|  |  [x] Enable kill switch when daily limit reached             ||
|  |  [x] Send email alert at 80% of limit                        ||
|  |                                                              ||
|  +--------------------------------------------------------------+|
|                                                                  |
|  +--------------------------------------------------------------+|
|  | Order Confirmations                                          ||
|  +--------------------------------------------------------------+|
|  |                                                              ||
|  |  [x] Require confirmation for orders > $ [500]               ||
|  |  [x] Require confirmation for market orders                  ||
|  |  [ ] Require confirmation for all orders                     ||
|  |                                                              ||
|  +--------------------------------------------------------------+|
|                                                                  |
|                                    [Reset to Defaults] [Save]    |
|                                                                  |
+------------------------------------------------------------------+
```

---

## Connection Testing UI

### Test Connection Modal

```
+--------------------------------------------------+
|  Test Exchange Connection                [X]     |
+--------------------------------------------------+
|                                                  |
|  Testing Binance connection...                   |
|                                                  |
|  +--------------------------------------------+  |
|  |                                            |  |
|  |  [Check] Authentication                    |  |
|  |          API key validated                 |  |
|  |                                            |  |
|  |  [Spin]  Permissions                       |  |
|  |          Checking trading permissions...   |  |
|  |                                            |  |
|  |  [ ]     Market Data                       |  |
|  |          Pending...                        |  |
|  |                                            |  |
|  |  [ ]     Order Placement                   |  |
|  |          Pending...                        |  |
|  |                                            |  |
|  +--------------------------------------------+  |
|                                                  |
|                                       [Cancel]   |
|                                                  |
+--------------------------------------------------+
```

### Test Results

```
+--------------------------------------------------+
|  Connection Test Results                 [X]     |
+--------------------------------------------------+
|                                                  |
|  +--------------------------------------------+  |
|  | [Check] All Tests Passed                   |  |
|  +--------------------------------------------+  |
|                                                  |
|  +--------------------------------------------+  |
|  |                                            |  |
|  |  Authentication     [Check] Passed         |  |
|  |  Permissions        [Check] Passed         |  |
|  |  Market Data        [Check] Passed  45ms   |  |
|  |  Order Placement    [Check] Passed  120ms  |  |
|  |                                            |  |
|  +--------------------------------------------+  |
|                                                  |
|  Connection Quality: Excellent                   |
|  Average Latency: 82ms                           |
|                                                  |
|                                        [Close]   |
|                                                  |
+--------------------------------------------------+
```

---

## Responsive Layouts

### Desktop (>= 1024px)
- Full sidebar navigation
- Two-column layout for settings panels
- Inline form validation

### Tablet (768px - 1023px)
- Collapsible sidebar (hamburger menu)
- Single-column settings panels
- Full-width forms

### Mobile (< 768px)
- Bottom navigation or hamburger menu
- Stacked form fields
- Full-screen modals
- Larger touch targets

---

## Accessibility

### Form Accessibility
- All inputs have associated labels
- Error messages linked via aria-describedby
- Required fields marked with asterisk and aria-required
- Form validation announced to screen readers

### Navigation
- Sidebar items have aria-current for active state
- Skip link to main content
- Logical tab order

### Visual
- Minimum 4.5:1 contrast for all text
- Focus indicators on all interactive elements
- Icons have aria-hidden with text labels

---

## User Testing Recommendations

### Test Scenarios
1. **First-Time Setup**: Complete wizard with valid credentials
2. **Invalid API Key**: Enter incorrect credentials and verify error handling
3. **Permission Mismatch**: Test with API key missing required permissions
4. **Risk Parameter Adjustment**: Modify risk settings and verify persistence
5. **Multi-Exchange**: Configure multiple exchanges
6. **Connection Recovery**: Test behavior when connection drops

### Metrics to Track
- Wizard completion rate
- Time to complete setup
- API key validation error rate
- Settings change frequency
- Support ticket volume for configuration issues

### A/B Testing Ideas
- Wizard step count (5 steps vs. 3 consolidated steps)
- Risk parameter presets vs. manual entry
- Inline help vs. external documentation links
