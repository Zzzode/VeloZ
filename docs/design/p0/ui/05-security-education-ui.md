# P0 Feature 5: Security Education UI

## Overview

The Security Education system guides users through safe trading practices, provides clear warnings for risky actions, and maintains a visible distinction between simulated and live trading modes. It should educate without being patronizing and protect without being obstructive.

---

## Onboarding Security Wizard

### User Flow

```
[Welcome] -> [API Security] -> [Risk Awareness] -> [Trading Modes] -> [Safety Checklist] -> [Complete]
```

---

### Screen 1: Security Welcome

```
+------------------------------------------------------------------+
|                                                                  |
|  +----+  +----+  +----+  +----+  +----+                         |
|  | 1  |--| 2  |--| 3  |--| 4  |--| 5  |                         |
|  +----+  +----+  +----+  +----+  +----+                         |
|  Welcome  API    Risk   Modes  Checklist                         |
|                                                                  |
+------------------------------------------------------------------+
|                                                                  |
|                    [Shield Icon - Large]                         |
|                                                                  |
|                    Your Security Matters                         |
|                                                                  |
|     Before you start trading, let's make sure you understand     |
|     how to keep your account and funds safe.                     |
|                                                                  |
|     +------------------------------------------------------+     |
|     |                                                      |     |
|     |  This quick guide covers:                            |     |
|     |                                                      |     |
|     |  [Key Icon]     API Key Security                     |     |
|     |                 Protect your exchange credentials    |     |
|     |                                                      |     |
|     |  [Alert Icon]   Risk Awareness                       |     |
|     |                 Understand trading risks             |     |
|     |                                                      |     |
|     |  [TestTube]     Trading Modes                        |     |
|     |                 Simulated vs. Live trading           |     |
|     |                                                      |     |
|     +------------------------------------------------------+     |
|                                                                  |
|     Estimated time: 3 minutes                                    |
|                                                                  |
|     [Skip for Now]                        [Start Guide ->]       |
|                                                                  |
+------------------------------------------------------------------+
```

---

### Screen 2: API Key Security

```
+------------------------------------------------------------------+
|                                                                  |
|  +----+  +----+  +----+  +----+  +----+                         |
|  |(1) |--| 2  |--| 3  |--| 4  |--| 5  |                         |
|  +----+  +----+  +----+  +----+  +----+                         |
|  Welcome  API    Risk   Modes  Checklist                         |
|                                                                  |
+------------------------------------------------------------------+
|                                                                  |
|                    API Key Security                              |
|                                                                  |
|     Your API keys are like passwords to your exchange account.   |
|                                                                  |
|     +------------------------------------------------------+     |
|     | [!] CRITICAL: Never share your API keys              |     |
|     |                                                      |     |
|     | - VeloZ staff will NEVER ask for your keys           |     |
|     | - Do not post keys in forums or support tickets      |     |
|     | - Do not store keys in plain text files              |     |
|     +------------------------------------------------------+     |
|                                                                  |
|     +------------------------------------------------------+     |
|     | Best Practices                                       |     |
|     +------------------------------------------------------+     |
|     |                                                      |     |
|     |  [Check] Enable IP whitelist on your exchange        |     |
|     |          Only allow connections from your IP         |     |
|     |                                                      |     |
|     |  [Check] Disable withdrawal permissions              |     |
|     |          VeloZ never needs to withdraw funds         |     |
|     |                                                      |     |
|     |  [Check] Use separate API keys for VeloZ             |     |
|     |          Don't reuse keys from other services        |     |
|     |                                                      |     |
|     |  [Check] Rotate keys periodically                    |     |
|     |          Change keys every 3-6 months                |     |
|     |                                                      |     |
|     +------------------------------------------------------+     |
|                                                                  |
|     [<- Back]                              [Continue ->]         |
|                                                                  |
+------------------------------------------------------------------+
```

---

### Screen 3: Risk Awareness

```
+------------------------------------------------------------------+
|                                                                  |
|  +----+  +----+  +----+  +----+  +----+                         |
|  |(1) |-|(2) |--| 3  |--| 4  |--| 5  |                         |
|  +----+  +----+  +----+  +----+  +----+                         |
|  Welcome  API    Risk   Modes  Checklist                         |
|                                                                  |
+------------------------------------------------------------------+
|                                                                  |
|                    Understanding Trading Risks                   |
|                                                                  |
|     +------------------------------------------------------+     |
|     | [AlertTriangle] Important Disclaimer                 |     |
|     |                                                      |     |
|     | Cryptocurrency trading involves significant risk.    |     |
|     | You can lose some or all of your investment.         |     |
|     +------------------------------------------------------+     |
|                                                                  |
|     Key Risks to Understand:                                     |
|                                                                  |
|     +------------------------------------------------------+     |
|     |                                                      |     |
|     |  [TrendingDown] Market Risk                          |     |
|     |  Prices can move rapidly against your position       |     |
|     |                                                      |     |
|     |  [Zap] Leverage Risk                                 |     |
|     |  Leverage amplifies both gains AND losses            |     |
|     |                                                      |     |
|     |  [Clock] Liquidation Risk                            |     |
|     |  Positions can be forcibly closed at a loss          |     |
|     |                                                      |     |
|     |  [Wifi] Technical Risk                               |     |
|     |  System failures can prevent order execution         |     |
|     |                                                      |     |
|     +------------------------------------------------------+     |
|                                                                  |
|     +------------------------------------------------------+     |
|     | [i] Only trade with money you can afford to lose.    |     |
|     +------------------------------------------------------+     |
|                                                                  |
|     [<- Back]                              [Continue ->]         |
|                                                                  |
+------------------------------------------------------------------+
```

---

### Screen 4: Trading Modes

```
+------------------------------------------------------------------+
|                                                                  |
|  +----+  +----+  +----+  +----+  +----+                         |
|  |(1) |-|(2) |-|(3) |--| 4  |--| 5  |                         |
|  +----+  +----+  +----+  +----+  +----+                         |
|  Welcome  API    Risk   Modes  Checklist                         |
|                                                                  |
+------------------------------------------------------------------+
|                                                                  |
|                    Trading Modes                                 |
|                                                                  |
|     VeloZ offers two trading modes to help you learn safely.     |
|                                                                  |
|     +------------------------------------------------------+     |
|     |                                                      |     |
|     |  [TestTube] SIMULATED MODE                           |     |
|     |                                                      |     |
|     |  +--------------------------------------------------+|     |
|     |  | [Purple Badge] SIMULATED                         ||     |
|     |  +--------------------------------------------------+|     |
|     |                                                      |     |
|     |  - Practice with virtual funds                       |     |
|     |  - Real market data, fake orders                     |     |
|     |  - No risk to your actual balance                    |     |
|     |  - Perfect for testing strategies                    |     |
|     |                                                      |     |
|     |  Recommended for: Beginners, strategy testing        |     |
|     |                                                      |     |
|     +------------------------------------------------------+     |
|                                                                  |
|     +------------------------------------------------------+     |
|     |                                                      |     |
|     |  [Zap] LIVE MODE                                     |     |
|     |                                                      |     |
|     |  +--------------------------------------------------+|     |
|     |  | [Green Badge] LIVE                               ||     |
|     |  +--------------------------------------------------+|     |
|     |                                                      |     |
|     |  - Trade with real funds                             |     |
|     |  - Real orders on the exchange                       |     |
|     |  - Real profits and losses                           |     |
|     |  - Requires careful risk management                  |     |
|     |                                                      |     |
|     |  Recommended for: Experienced traders                |     |
|     |                                                      |     |
|     +------------------------------------------------------+     |
|                                                                  |
|     [<- Back]                              [Continue ->]         |
|                                                                  |
+------------------------------------------------------------------+
```

---

### Screen 5: Safety Checklist

```
+------------------------------------------------------------------+
|                                                                  |
|  +----+  +----+  +----+  +----+  +----+                         |
|  |(1) |-|(2) |-|(3) |-|(4) |--| 5  |                           |
|  +----+  +----+  +----+  +----+  +----+                         |
|  Welcome  API    Risk   Modes  Checklist                         |
|                                                                  |
+------------------------------------------------------------------+
|                                                                  |
|                    Safety Checklist                              |
|                                                                  |
|     Confirm you understand these important points:               |
|                                                                  |
|     +------------------------------------------------------+     |
|     |                                                      |     |
|     |  [ ] I understand that trading involves risk and     |     |
|     |      I may lose money                                |     |
|     |                                                      |     |
|     |  [ ] I will not enable withdrawal permissions on     |     |
|     |      my API keys                                     |     |
|     |                                                      |     |
|     |  [ ] I will start with simulated trading before      |     |
|     |      using real funds                                |     |
|     |                                                      |     |
|     |  [ ] I will set appropriate risk limits before       |     |
|     |      trading live                                    |     |
|     |                                                      |     |
|     |  [ ] I will only trade with money I can afford       |     |
|     |      to lose                                         |     |
|     |                                                      |     |
|     +------------------------------------------------------+     |
|                                                                  |
|     +------------------------------------------------------+     |
|     | [i] You must check all boxes to continue             |     |
|     +------------------------------------------------------+     |
|                                                                  |
|     [<- Back]                              [Complete ->]         |
|                                                                  |
+------------------------------------------------------------------+
```

### Specifications
- **Checkboxes**: All must be checked to enable Continue button
- **Continue Button**: Disabled state until all checked
- **Progress**: Saved so users can resume if they leave

---

## Trading Mode Indicator

### Global Mode Badge

The trading mode is always visible in the header:

```
+------------------------------------------------------------------+
|  [VeloZ Logo]  Dashboard  Strategies  Charts  |  [SIMULATED]  [User]|
+------------------------------------------------------------------+
```

### Simulated Mode Badge

```
+---------------------------+
| [TestTube] SIMULATED      |
+---------------------------+
```

- **Background**: `#ede9fe` (simulated-light)
- **Text**: `#7c3aed` (simulated)
- **Icon**: TestTube from Lucide
- **Position**: Header, always visible

### Live Mode Badge

```
+---------------------------+
| [Zap] LIVE                |
+---------------------------+
```

- **Background**: `#d1fae5` (success-light)
- **Text**: `#059669` (success)
- **Icon**: Zap from Lucide
- **Position**: Header, always visible

### Mode Switch Confirmation

When switching from Simulated to Live:

```
+--------------------------------------------------+
|  [AlertTriangle] Switch to Live Trading?  [X]    |
+--------------------------------------------------+
|                                                  |
|  You are about to enable LIVE trading mode.      |
|                                                  |
|  +--------------------------------------------+  |
|  | [!] WARNING                                |  |
|  |                                            |  |
|  | - All orders will use REAL funds           |  |
|  | - Losses will affect your actual balance   |  |
|  | - Make sure your risk limits are set       |  |
|  +--------------------------------------------+  |
|                                                  |
|  Current Risk Settings:                          |
|  - Max Position: 10% of balance                  |
|  - Daily Loss Limit: 5%                          |
|  - Max Leverage: 5x                              |
|                                                  |
|  [ ] I understand and accept the risks           |
|                                                  |
|  [Stay in Simulated]    [Switch to Live]         |
|                                                  |
+--------------------------------------------------+
```

---

## Warning Dialogs

### High-Risk Action Warning

For actions like high leverage or large position sizes:

```
+--------------------------------------------------+
|  [AlertTriangle] High Risk Action         [X]    |
+--------------------------------------------------+
|                                                  |
|  This action has elevated risk:                  |
|                                                  |
|  +--------------------------------------------+  |
|  |                                            |  |
|  |  Leverage: 20x                             |  |
|  |  Position Size: $5,000 (25% of balance)    |  |
|  |  Potential Loss: Up to $5,000              |  |
|  |                                            |  |
|  +--------------------------------------------+  |
|                                                  |
|  +--------------------------------------------+  |
|  | [i] Consider reducing leverage or position |  |
|  |     size to limit potential losses.        |  |
|  +--------------------------------------------+  |
|                                                  |
|  [ ] I understand the risks                      |
|                                                  |
|  [Modify Order]              [Proceed Anyway]    |
|                                                  |
+--------------------------------------------------+
```

### Loss Limit Warning

When approaching or hitting loss limits:

```
+--------------------------------------------------+
|  [AlertCircle] Daily Loss Limit Warning   [X]    |
+--------------------------------------------------+
|                                                  |
|  You are approaching your daily loss limit.      |
|                                                  |
|  +--------------------------------------------+  |
|  |                                            |  |
|  |  Daily Loss Limit:     5%                  |  |
|  |  Current Daily Loss:   4.2%                |  |
|  |  Remaining:            0.8%                |  |
|  |                                            |  |
|  |  [========================================]|  |
|  |                                       84%  |  |
|  |                                            |  |
|  +--------------------------------------------+  |
|                                                  |
|  Options:                                        |
|  - Continue trading (limit will auto-stop)       |
|  - Pause trading for today                       |
|  - Adjust your loss limit in settings            |
|                                                  |
|  [Pause Trading]    [Continue]    [Settings]     |
|                                                  |
+--------------------------------------------------+
```

### Kill Switch Activated

```
+--------------------------------------------------+
|  [XCircle] Trading Stopped                [X]    |
+--------------------------------------------------+
|                                                  |
|  +--------------------------------------------+  |
|  | [!] KILL SWITCH ACTIVATED                  |  |
|  +--------------------------------------------+  |
|                                                  |
|  All trading has been automatically stopped      |
|  because you reached your daily loss limit.      |
|                                                  |
|  Daily Loss: -5.2% ($520.00)                     |
|                                                  |
|  All open orders have been cancelled.            |
|  Positions remain open but no new orders         |
|  can be placed until tomorrow.                   |
|                                                  |
|  +--------------------------------------------+  |
|  | [i] This is a safety feature to protect    |  |
|  |     you from excessive losses.             |  |
|  +--------------------------------------------+  |
|                                                  |
|  [View Positions]    [Adjust Settings]    [OK]   |
|                                                  |
+--------------------------------------------------+
```

---

## Risk Settings Dashboard

### Risk Overview Panel

```
+------------------------------------------------------------------+
|  Risk Dashboard                                                  |
+------------------------------------------------------------------+
|                                                                  |
|  Current Mode: [LIVE]                                            |
|                                                                  |
|  +---------------------------+  +-------------------------------+|
|  | Daily Loss Status         |  | Position Exposure             ||
|  +---------------------------+  +-------------------------------+|
|  |                           |  |                               ||
|  |  Limit: 5%                |  |  Max Allowed: 10%             ||
|  |  Current: -2.1%           |  |  Current: 6.5%                ||
|  |                           |  |                               ||
|  |  [=========>         ]    |  |  [======>              ]      ||
|  |              42%          |  |              65%              ||
|  |                           |  |                               ||
|  |  Remaining: $290          |  |  Available: $350              ||
|  |                           |  |                               ||
|  +---------------------------+  +-------------------------------+|
|                                                                  |
|  +--------------------------------------------------------------+|
|  | Active Protections                                           ||
|  +--------------------------------------------------------------+|
|  |                                                              ||
|  |  [Check] Daily loss limit (5%)                    Active     ||
|  |  [Check] Maximum position size (10%)              Active     ||
|  |  [Check] Maximum leverage (5x)                    Active     ||
|  |  [Check] Order confirmation > $500                Active     ||
|  |  [X]     Kill switch                              Standby    ||
|  |                                                              ||
|  +--------------------------------------------------------------+|
|                                                                  |
|  [Emergency Stop All]                         [Edit Settings]    |
|                                                                  |
+------------------------------------------------------------------+
```

### Emergency Stop Button

```
+------------------------------------------+
|  [!] Emergency Stop                      |
+------------------------------------------+
|                                          |
|  This will immediately:                  |
|                                          |
|  - Cancel ALL open orders                |
|  - Disable ALL running strategies        |
|  - Prevent any new orders                |
|                                          |
|  Your open positions will NOT be closed. |
|                                          |
|  Type "STOP" to confirm:                 |
|  +--------------------------------------+|
|  |                                      ||
|  +--------------------------------------+|
|                                          |
|  [Cancel]              [Emergency Stop]  |
|                                          |
+------------------------------------------+
```

---

## Contextual Education

### Tooltip Education

```
+------------------------------------------+
|  Leverage: [20x v]                       |
|            +----------------------------+|
|            | [i] What is leverage?      ||
|            |                            ||
|            | Leverage multiplies your   ||
|            | buying power but also      ||
|            | your risk.                 ||
|            |                            ||
|            | 20x means:                 ||
|            | - $100 controls $2,000     ||
|            | - 5% move = 100% gain/loss ||
|            |                            ||
|            | [Learn More ->]            ||
|            +----------------------------+|
+------------------------------------------+
```

### First-Time Feature Hints

```
+------------------------------------------------------------------+
|                                                                  |
|  +--------------------------------------------------------------+|
|  |  [Lightbulb] New Feature: Chart Trading                      ||
|  |                                                              ||
|  |  Click anywhere on the chart to place orders at that price.  ||
|  |                                                              ||
|  |  [Got it]                              [Show me how]         ||
|  +--------------------------------------------------------------+|
|                                                                  |
+------------------------------------------------------------------+
```

---

## Responsive Layouts

### Desktop (>= 1024px)
- Full risk dashboard with all metrics
- Side-by-side warning dialogs
- Persistent mode indicator in header

### Tablet (768px - 1023px)
- Stacked risk metrics
- Full-width warning dialogs
- Mode indicator in header

### Mobile (< 768px)
- Simplified risk overview
- Full-screen warning dialogs
- Floating mode indicator badge
- Larger touch targets for confirmations

### Mobile Mode Indicator

```
+----------------------------------+
|  [VeloZ]              [SIMULATED]|
+----------------------------------+
```

---

## Accessibility

### Warning Dialogs
- Focus trapped within dialog
- Escape key closes (if safe)
- Screen reader announces dialog title
- Confirmation checkbox required for risky actions

### Color Independence
- Icons accompany all color-coded status
- Text labels for all status indicators
- Pattern fills available for charts

### Keyboard Navigation
- Tab through all interactive elements
- Enter to confirm, Escape to cancel
- Spacebar to toggle checkboxes

---

## User Testing Recommendations

### Test Scenarios
1. **Complete Onboarding**: Go through security wizard
2. **Mode Switch**: Switch from Simulated to Live
3. **Warning Response**: Encounter and respond to risk warning
4. **Kill Switch**: Trigger and recover from kill switch
5. **Emergency Stop**: Use emergency stop feature
6. **Mobile Warnings**: Handle warnings on mobile device

### Metrics to Track
- Onboarding completion rate
- Time spent on each security screen
- Mode switch frequency
- Warning acknowledgment rate
- Kill switch activation frequency
- Support tickets related to security

### A/B Testing Ideas
- Onboarding length (5 screens vs. 3 condensed)
- Warning dialog urgency levels
- Mode indicator visibility/prominence
- Checkbox vs. type-to-confirm for critical actions

---

## Security Education Content

### Help Center Topics

1. **API Key Security**
   - How to create secure API keys
   - IP whitelisting guide
   - Permission settings explained
   - Key rotation best practices

2. **Risk Management**
   - Understanding leverage
   - Position sizing strategies
   - Setting appropriate limits
   - Recovery from losses

3. **Trading Modes**
   - When to use simulated mode
   - Transitioning to live trading
   - Interpreting simulated results

4. **Emergency Procedures**
   - Using the kill switch
   - Manual position closing
   - Contacting support
   - Exchange-side controls
