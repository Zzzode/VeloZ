---
paths:
  - "**"
  - "*.cpp"
  - "*.h"
  - "*.hpp"
  - "*.py"
  - "*.md"
---

# Encoding and Style (Code Conventions)

## Overview

This skill provides guidance for encoding standards, naming conventions, and formatting rules in VeloZ.

## Encoding Standards

### English Only (MANDATORY)

**All code, comments, and documentation MUST be in English.**

- **Chinese is NOT allowed** in any code, comments, or documentation
- **No mixed language**: Do not combine English and Chinese
- **Variable names**: Must be English
- **Comments**: Must be in English
- **Documentation**: All documentation files must be in English

**Examples of violations:**

```cpp
// ❌ FORBIDDEN - Chinese comments
// 创建订单对象
Order* order = createOrder();

// ✅ CORRECT - English comments
// Create order object
Order* order = createOrder();
```

### Text Encoding

**All text files must use UTF-8 encoding.**

- **No BOM**: Files should not start with UTF-8 BOM
- **Line endings**: Use LF (`\n`) line endings, not CRLF
- **Special characters**: Use only printable ASCII characters in code (strings can contain Unicode)

## Naming Conventions

### C++ Naming

| Type | Convention | Example |
|--------|--------------|----------|
| **Files** | `snake_case.c++` | `event_loop.cpp` |
| **Classes/Structs** | `TitleCase` | `OrderBook`, `EventLoop` |
| **Functions** | `camelCase` | `submitOrder()`, `updateBid()` |
| **Constants/Enumerants** | `CAPITAL_WITH_UNDERSCORES` | `MAX_RETRY_COUNT`, `EventPriority::CRITICAL` |
| **Private members** | `camelCase_` | `queue_`, `mutex_` |
| **Public members** | `camelCase` | `capacity()`, `size()` |
| **Namespaces** | `lowercase` | `veloz`, `core`, `market` |
| **Macros** | `CAPITAL_WITH_UNDERSCORES` | `KJ_TEST()`, `VELOZ_LOG()` |

### KJ Library Naming

| Type | Convention | Example |
|--------|--------------|----------|
| **KJ classes** | `TitleCase` | `kj::Own`, `kj::Maybe`, `kj::Mutex` |
| **KJ functions** | `camelCase` | `kj::heap()`, `kj::mv()`, `kj::str()` |
| **KJ macros** | `CAPITAL_WITH_UNDERSCORES` | `KJ_ASSERT()`, `KJ_IF_SOME()`, `KJ_EXPECT()` |

### Python Naming

| Type | Convention | Example |
|--------|--------------|----------|
| **Files/Modules** | `snake_case.py` | `strategy_manager.py` |
| **Classes** | `TitleCase` | `OrderBook`, `StrategyManager` |
| **Functions** | `snake_case` | `submit_order()`, `update_bid()` |
| **Constants** | `UPPER_CASE` | `MAX_RETRY_COUNT`, `DEFAULT_TIMEOUT` |
| **Private members** | `_underscore_prefix` | `_private_value` |
| **Public members** | `no_prefix` | `public_value` |

## C++ Formatting

### Indentation

- **2 spaces** (no tabs)
- **Max line length**: 100 characters
- **Brace placement**: Opening brace at end of statement, closing brace on new line

```cpp
// ❌ FORBIDDEN
if (condition)
{
    doSomething();
}

// ✅ CORRECT
if (condition) {
    doSomething();
}
```

### Spacing Rules

- **Space after keyword**: `if (`, `while (`, `for (`, `return `value;`
- **No space before function name**: `functionName(param1, param2)`
- **Space around operators**: `a + b`, `a == b`, `a != b`

### File Organization

```cpp
// Correct header order
#include "kj/common.h>        // KJ first
#include "veloz/core/logger.h"  // Project headers
#include <optional>               // Std library (when needed)
#include <vector>               // Std library (when needed)

// Correct namespace usage
namespace veloz::core {
    // namespace contents
}  // namespace veloz::core

namespace veloz::market {
    // namespace contents
}  // namespace veloz::market
```

## Documentation Comments

### C++ Comment Style

- **Line comments only**: Use `//`, not `/* */`
- **Comment placement**: After function/variable declarations
- **Comment content**: Add useful information not obvious from code
- **TODO comments**: Use format `// TODO(type): description`

```cpp
// ❌ FORBIDDEN
Order* createOrder(); // Creates order object

// ✅ CORRECT
// Creates order object and configures parameters
Order* createOrder();

// ❌ FORBIDDEN
/* This function parses the command */

// ✅ CORRECT
// Parses the command string
void parseCommand();

// TODO example
// TODO(perf): Optimize this function for better performance
// TODO(security): Add input validation
```

### KJ String Literals

Use KJ string literals for compile-time strings:

```cpp
// Use _kj suffix for runtime StringPtr
kj::StringPtr str = "hello"_kj;

// Use _kjc suffix for constexpr ConstString
constexpr kj::ConstString constStr = "world"_kjc;

// Use kj::str() for stringification
kj::String message = kj::str("Value: ", value);
```

## Python Formatting

### PEP 8 Compliance

Use `flake8` for Python code quality:

```bash
flake8 python/ --count --select=E9,F63,F7,F82 --show-source --statistics
```

### Python Style Guide

| Rule | Description |
|--------|-------------|
| **Line length** | Max 127 characters |
| **Indentation** | 4 spaces |
| **Import order** | stdlib, third-party, local |
| **Whitespace** | 2 blank lines between functions |
| **Naming** | snake_case for functions/variables, UPPER_CASE for constants |

```python
# ❌ FORBIDDEN
import os, sys
def myFunction():
    pass

# ✅ CORRECT
import os
import sys


def my_function():
    pass
```

## JSON Format Standards

### NDJSON (Newline-Delimited JSON)

**For stdout events (engine → gateway):**

```json
{"type":"market","data":{...}}
{"type":"order_update","data":{...}}
```

**Rules:**
- One JSON object per line
- `type` field identifies event type
- Field names use `snake_case`
- Timestamps in ISO 8601 format: `2025-01-15T10:30:00.123Z`

### Event Type Names

| Event Type | Type Value | Example Fields |
|------------|-------------|----------------|
| `market` | object | `symbol`, `bid`, `ask`, `timestamp` |
| `order_update` | object | `order_id`, `side`, `price`, `qty`, `status` |
| `fill` | object | `order_id`, `price`, `qty`, `commission` |
| `order_state` | object | `order_id`, `status`, `filled_qty`, `remaining_qty` |
| `account` | object | `balance`, `available`, `locked` |
| `error` | object | `code`, `message`, `context` |

## File Templates

### C++ Header Template

```cpp
// Project Name - Module brief description
// Copyright (c) 2025 Primary Author and contributors
//
// Licensed under the MIT License

#pragma once
// Documentation for file.
//
// #include <kj/common.h>  // KJ first
// #include "veloz/core/logger.h"  // Project headers

namespace veloz {
    namespace module {
        // declarations
    }  // namespace module
}  // namespace veloz
```

### C++ Source Template

```cpp
// Project Name - Module brief description
// Copyright (c) 2025 Primary Author and contributors
//
// Licensed under the MIT License

#include "module.h"
#include <kj/common.h>
#include "veloz/core/logger.h"

namespace veloz {
    namespace module {

        void publicFunction();

    }  // namespace module
}
```

### Python Module Template

```python
"""
Project Name - Module brief description
Copyright (c) 2025 Primary Author and contributors

Licensed under the MIT License
"""

import sys
from typing import Optional


class MyClass:
    """Documentation for class."""
    pass


def public_function():
    """Documentation for function."""
    pass
```

## Documentation Style

### Markdown Documentation

- **Headers**: Use ATX style (`#`, `##`, `###`)
- **Lists**: Use bullet points with `-` or numbered lists
- **Code blocks**: Use triple backticks for inline code, fenced blocks for blocks
- **Emphasis**: Use asterisks (`*text*`) or bold (`**text**`)

```markdown
# Overview

This document describes the architecture of VeloZ.

## Key Features

- Feature 1 - Description
- Feature 2 - Description

```

## Files to Check

- `/Users/bytedance/Develop/VeloZ/.claude/rules/cpp-engine.md` - C++ engine rules
- `/Users/bytedance/Develop/VeloZ/.claude/rules/build-ci.md` - Build and CI rules
- `/Users/bytedance/Develop/VeloZ/.claude/rules/architecture.md` - Architecture rules
- `/Users/bytedance/Develop/VeloZ/.claude/rules/testing.md` - Testing rules
