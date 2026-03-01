# VeloZ Tutorial Style Guide

This guide ensures consistency across all VeloZ tutorials. All tutorial writers must follow these conventions.

---

## 1. Standard Template

Every tutorial must follow this exact structure:

```markdown
# [Tutorial Title]

**Time:** X minutes | **Level:** Beginner/Intermediate/Advanced

## What You'll Learn

Brief bullet list of skills/knowledge gained (3-5 items).

## Prerequisites

- Prerequisite 1 (with link if applicable)
- Prerequisite 2
- Required tools/access

---

## Step 1: [Action Verb] [Object]

Brief explanation of what we're doing and why.

\`\`\`bash
# Description of command
command --flag value
\`\`\`

**Expected Output:**
\`\`\`json
{
  "status": "success",
  "data": "..."
}
\`\`\`

> **Tip:** Helpful hint to improve the experience.

---

## Step 2: [Action Verb] [Object]

[Continue pattern...]

---

## Summary

**What you accomplished:**
- Accomplishment 1
- Accomplishment 2
- Accomplishment 3

## Troubleshooting

### Issue: [Common Problem]
**Symptom:** What the user sees
**Solution:** How to fix it

### Issue: [Another Problem]
**Symptom:** ...
**Solution:** ...

## Next Steps

- [Related Tutorial Title](./related-tutorial.md) - Brief description
- [Reference Documentation](../guides/user/relevant-guide.md) - Brief description
```

---

## 2. Tone Guidelines

### Voice
- **Second person:** Use "you" and "your" (not "we" or "the user")
- **Active voice:** "Run the command" (not "The command should be run")
- **Direct and concise:** Get to the point quickly
- **Encouraging:** Acknowledge progress without being patronizing

### Examples

| Correct | Incorrect |
|---------|-----------|
| "Run the health check command." | "We will now run the health check command." |
| "You should see the following output." | "The user will observe the following output." |
| "Your order is now placed." | "The order has been successfully placed by the system." |
| "If this fails, check your API key." | "In the event of failure, verification of the API key is recommended." |

### Technical Level by Audience

| Level | Assumptions | Explanation Depth |
|-------|-------------|-------------------|
| **Beginner** | No VeloZ experience, basic terminal skills | Explain every step, define terms |
| **Intermediate** | Completed beginner tutorials, understands VeloZ basics | Focus on new concepts, brief refreshers |
| **Advanced** | Comfortable with VeloZ, programming experience | Minimal explanation, focus on implementation |

### Tone Don'ts
- No emojis
- No exclamation marks (except in warnings)
- No jokes or humor
- No unnecessary praise ("Great job!")
- No hedging language ("maybe", "might want to", "you could try")

---

## 3. Code Example Conventions

### Bash Commands

```bash
# Always include a comment explaining the command
curl -X POST http://127.0.0.1:8080/api/order \
  -H "Content-Type: application/json" \
  -d '{
    "side": "BUY",
    "symbol": "BTCUSDT",
    "qty": 0.001,
    "price": 50000.0
  }'
```

**Rules:**
- Always include descriptive comment above command
- Use backslash `\` for multi-line commands
- Indent continuation lines by 2 spaces
- Use double quotes for JSON strings
- Use lowercase for JSON keys
- Include realistic but safe values (small quantities, testnet)

### Expected Output Format

**For JSON responses:**
```json
{
  "ok": true,
  "client_order_id": "web-1234567890",
  "venue_order_id": "12345678"
}
```

**For plain text:**
```
VeloZ Engine v1.0.0
Starting in stdio mode...
Ready.
```

**Rules:**
- Always show realistic output (not placeholders like "...")
- Truncate very long outputs with comment: `// ... (truncated)`
- Highlight important fields in explanation after code block

### Environment Variables

```bash
# Set Binance testnet credentials
export VELOZ_BINANCE_API_KEY=your_testnet_api_key
export VELOZ_BINANCE_API_SECRET=your_testnet_api_secret
export VELOZ_EXECUTION_MODE=binance_testnet_spot
```

**Rules:**
- Group related variables together
- Add comment explaining the group
- Use `your_*` prefix for values user must replace
- Never show real API keys or secrets

### Configuration Files (JSON)

```json
{
  "strategy_id": "momentum_1",
  "risk_limits": {
    "max_position_size": 1.0,
    "max_drawdown_pct": 10.0,
    "max_daily_loss": 1000.0
  }
}
```

**Rules:**
- Use 2-space indentation
- Include all required fields
- Use realistic default values
- Add inline comments only if JSON5 is supported (otherwise explain after)

### C++ Code (for Advanced Tutorials)

```cpp
// Strategy header: libs/strategy/include/veloz/strategy/my_strategy.h
#pragma once

#include "veloz/strategy/strategy_base.h"

namespace veloz::strategy {

class MyStrategy final : public StrategyBase {
public:
    explicit MyStrategy(kj::StringPtr name);

    void on_market_data(const MarketData& data) override;
    void on_order_update(const OrderUpdate& update) override;

private:
    double signal_threshold_{0.5};
};

} // namespace veloz::strategy
```

**Rules:**
- Include file path as first comment
- Follow VeloZ coding style (see CLAUDE.md)
- Use KJ types as default (kj::String, kj::Own, etc.)
- Include namespace
- Show complete, compilable examples

---

## 4. Section Naming Patterns

### Step Titles

Format: `## Step N: [Action Verb] [Object]`

| Correct | Incorrect |
|---------|-----------|
| `## Step 1: Start the Gateway` | `## Step 1: Starting the Gateway` |
| `## Step 2: Verify System Health` | `## Step 2: Health Check` |
| `## Step 3: Place Your First Order` | `## Step 3: Order Placement` |
| `## Step 4: Monitor Order Status` | `## Step 4: Monitoring` |

**Action Verbs to Use:**
- Start, Stop, Restart
- Configure, Set Up, Enable, Disable
- Create, Build, Deploy, Install
- Verify, Check, Test, Validate
- Monitor, View, Inspect
- Place, Cancel, Modify (for orders)

### Subsection Titles

Use `###` for subsections within steps:

```markdown
## Step 3: Configure Risk Limits

### Position Limits

[Content...]

### Drawdown Protection

[Content...]
```

### Troubleshooting Section

Format: `### Issue: [Brief Problem Description]`

```markdown
## Troubleshooting

### Issue: Gateway fails to start
**Symptom:** Error message "Address already in use"
**Solution:** Check if port 8080 is in use...

### Issue: Order rejected
**Symptom:** API returns {"ok": false, "error": "insufficient_balance"}
**Solution:** Verify account balance...
```

---

## 5. Cross-Reference Style

### Links to Other Tutorials

```markdown
See [Your First Trade](./first-trade.md) for basic order placement.
```

Format: `[Tutorial Title](./filename.md)`

### Links to User Guides

```markdown
For complete configuration options, see the [Configuration Guide](../guides/user/configuration.md).
```

Format: `[Guide Title](../guides/user/filename.md)`

### Links to API Documentation

```markdown
See [HTTP API Reference](../api/http_api.md#place-order) for all order parameters.
```

Format: `[API Reference](../api/filename.md#section-anchor)`

### Links to External Resources

```markdown
Get testnet API keys from [Binance Testnet](https://testnet.binance.vision/).
```

Format: `[Description](https://url)` - Always include trailing period outside link.

### Prerequisites Links

```markdown
## Prerequisites

- Completed [Your First Trade](./first-trade.md) tutorial
- VeloZ gateway running (see [Getting Started](../guides/user/getting-started.md))
- Binance testnet API keys (see [Binance Integration](../guides/user/binance.md#getting-api-keys))
```

---

## 6. Callout Boxes

### Tips (Helpful Information)

```markdown
> **Tip:** Use `client_order_id` to track orders across your system.
```

### Warnings (Important Cautions)

```markdown
> **Warning:** Never use production API keys for testing. Always use testnet credentials.
```

### Notes (Additional Context)

```markdown
> **Note:** This feature requires VeloZ v1.0 or later.
```

**Rules:**
- One sentence per callout (two maximum)
- Place immediately after relevant content
- Use sparingly (max 2-3 per tutorial)
- Warnings take priority over tips

---

## 7. File Naming

| Pattern | Example |
|---------|---------|
| Lowercase | `first-trade.md` |
| Hyphens for spaces | `multi-exchange-arbitrage.md` |
| Descriptive | `custom-strategy-development.md` |
| No version numbers | `production-deployment.md` (not `production-deployment-v1.md`) |

---

## 8. Quality Checklist

Before submitting a tutorial, verify:

- [ ] Follows standard template structure
- [ ] Time estimate is realistic (test it yourself)
- [ ] Level is appropriate for content
- [ ] All commands tested and working
- [ ] All expected outputs are accurate
- [ ] No placeholder values (replace "..." with real examples)
- [ ] Prerequisites are complete and linked
- [ ] Troubleshooting covers common issues
- [ ] Cross-references use correct relative paths
- [ ] No emojis or exclamation marks
- [ ] Code follows VeloZ conventions (KJ types, etc.)
- [ ] Spelling and grammar checked

---

## 9. Example: Complete Step

Here's a fully-formatted example step:

```markdown
## Step 3: Place Your First Order

Now that the gateway is running and healthy, place a limit buy order for BTCUSDT.

\`\`\`bash
# Place a limit buy order
curl -X POST http://127.0.0.1:8080/api/order \
  -H "Content-Type: application/json" \
  -d '{
    "side": "BUY",
    "symbol": "BTCUSDT",
    "qty": 0.001,
    "price": 50000.0
  }'
\`\`\`

**Expected Output:**
\`\`\`json
{
  "ok": true,
  "client_order_id": "web-1708704000000",
  "venue_order_id": "12345678"
}
\`\`\`

The `client_order_id` is automatically generated. Save this value to track your order.

> **Tip:** Use a custom `client_order_id` by adding it to the request body for easier tracking.
```

---

## Questions?

Contact the architect (pm) for clarification on technical accuracy or style questions.
