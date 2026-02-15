# Development Guide

This guide helps developers set up their environment for contributing to VeloZ.

## Prerequisites

### Required Tools

| Tool | Version | Purpose |
|-------|---------|---------|
| CMake | >= 3.24 | Build system |
| C++ Compiler | C++23 capable | Compiling code |
| Python | 3.x | Gateway and scripts |
| Git | any | Version control |

### Recommended Tools

| Tool | Purpose |
|-------|---------|
| clang-format | Code formatting |
| clang-tidy | Static analysis |
| VS Code / CLion | IDE |

## Setting Up Development Environment

### macOS

```bash
# Install Homebrew (if not installed)
/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"

# Install CMake
brew install cmake

# Install Clang (includes clang-format and clang-tidy)
brew install llvm

# Install Python
brew install python3

# Install Ninja (optional, faster builds)
brew install ninja
```

### Ubuntu/Debian

```bash
# Install build essentials
sudo apt update
sudo apt install -y cmake ninja-build clang-format clang-tidy python3 python3-pip

# Install Clang compiler (optional, GCC works too)
sudo apt install -y clang
```

### Fedora/CentOS

```bash
# Install build essentials
sudo dnf install -y cmake ninja-build clang-tools-extra python3

# Install Clang compiler
sudo dnf install -y clang
```

## Building the Project

### First-Time Build

```bash
# Clone repository
git clone https://github.com/your-org/VeloZ.git
cd VeloZ

# Configure (uses CMake presets)
cmake --preset dev

# Build all targets
cmake --build --preset dev -j$(nproc)
```

### Incremental Build

After making changes, only rebuild affected modules:

```bash
# Build only engine
cmake --build --preset dev --target veloz_engine -j$(nproc)

# Build specific library
cmake --build --preset dev --target veloz_market -j$(nproc)
```

### Release Build

```bash
cmake --preset release
cmake --build --preset release -j$(nproc)
```

## Project Structure

```
VeloZ/
├── apps/
│   ├── engine/          # C++ engine application
│   ├── gateway/         # Python gateway
│   └── ui/              # Web UI
├── libs/
│   ├── core/            # Infrastructure (logging, event loop, etc.)
│   ├── market/           # Market data module
│   ├── exec/            # Execution module
│   ├── oms/              # Order management system
│   ├── risk/             # Risk management module
│   └── strategy/         # Strategy framework
├── tests/                # Test suites
├── scripts/              # Build and run scripts
└── docs/                 # Documentation
```

## Code Style

### C++ Formatting

Use clang-format for consistent code style:

```bash
# Format all code
./scripts/format.sh

# Format specific file
clang-format -i libs/core/src/*.cpp
```

### Code Style Guidelines

- **Indentation**: 4 spaces (no tabs)
- **Naming**:
  - Classes: `PascalCase` (e.g., `OrderManager`)
  - Functions: `snake_case` (e.g., `place_order`)
  - Variables: `snake_case` (e.g., `client_order_id`)
  - Constants: `UPPER_SNAKE_CASE` (e.g., `MAX_ORDERS`)
- **Brackets**: Allman style (opening on same line, closing on new line)
- **Line Length**: Prefer < 120 characters

### Python Code Style

Follow PEP 8 guidelines. Use `black` for formatting:

```bash
pip install black
black apps/gateway/*.py
```

## Running Tests

### Build Tests

```bash
# Build with tests
cmake --preset dev
cmake --build --preset dev-all -j$(nproc)
```

### Run All Tests

```bash
# Run using CTest
ctest --preset dev -j$(nproc)
```

### Run Specific Test Suite

```bash
# Run market tests
./build/dev/libs/market/veloz_market_tests

# Run execution tests
./build/dev/libs/exec/veloz_exec_tests
```

## Development Workflow

### Making Changes

1. **Create a feature branch**:
   ```bash
   git checkout -b feature/my-feature
   ```

2. **Make your changes**
3. **Format your code**:
   ```bash
   ./scripts/format.sh
   ```

4. **Build and test**:
   ```bash
   cmake --build --preset dev -j$(nproc)
   ctest --preset dev
   ```

5. **Commit your changes**:
   ```bash
   git add .
   git commit -m "feat: add new feature"
   ```

6. **Push and create PR**

### Commit Message Format

Follow conventional commits:

```
<type>(<scope>): <subject>

<body>

<footer>
```

Types:
- `feat`: New feature
- `fix`: Bug fix
- `docs`: Documentation changes
- `refactor`: Code refactoring
- `test`: Test additions/changes
- `chore`: Maintenance tasks

## Debugging

### Debug Build

```bash
# Use debug preset for full symbols
cmake --preset dev
cmake --build --preset dev -j$(nproc)
```

### AddressSanitizer Build

```bash
# Detect memory errors
cmake --preset asan
cmake --build --preset asan -j$(nproc)
./build/asan/apps/engine/veloz_engine
```

### Logging

The engine logs to stdout/stderr. For debugging:

```bash
# Run with verbose output
./scripts/run_gateway.sh dev 2>&1 | tee engine.log
```

## Architecture Overview

### Module Dependencies

```
           ┌─────────────┐
           │    UI      │
           └──────┬──────┘
                  │
           ┌────────▼────────┐
           │   Gateway       │
           └──────┬─────────┘
                  │ stdio
           ┌────────▼────────┐
           │    Engine       │
    ┌────────┴──────┬────────┴────────┐
    │               │                   │
┌───▼───┐   ┌───▼────┐   ┌───▼────────┐
│ Market  │   │ Exec    │   │ OMS        │   ┌───▼───┐
│         │   │         │   │             │   │ Risk   │
└─────────┘   └─────────┘   └─────────────┘   └─────────┘
                  │
           ┌─────▼─────┐
           │  Strategy  │
           └────────────┘
                  │
           ┌─────▼─────┐
           │ Backtest   │
           └────────────┘
```

All modules use KJ library for core functionality.

## Adding a New Feature

### Adding a New Library Module

1. Create directory under `libs/your_module/`
2. Add CMakeLists.txt for the module
3. Update top-level CMakeLists.txt to include the new module
4. Implement header files in `include/veloz/your_module/`
5. Implement source files in `src/`

### Adding API Endpoints

1. Add handler method to `apps/gateway/gateway.py`:
   ```python
   def do_GET_your_endpoint(self):
       # Your implementation
       self._send_json(200, {"result": "data"})
   ```

2. Add route to `Handler.do_GET()`:
   ```python
   if parsed.path == "/api/your_endpoint":
       self.do_GET_your_endpoint()
   ```

3. Update API documentation in `docs/api/http-api.md`

## Contributing

1. **Fork the repository**
2. **Create a feature branch**
3. **Make your changes**
4. **Add tests for new functionality**
5. **Ensure all tests pass**
6. **Update documentation**
7. **Submit a pull request**

## Related

- [Getting Started](getting-started.md) - Quick start guide
- [Configuration](configuration.md) - Configuration options
- [HTTP API Reference](../api/http-api.md) - API endpoints
- [Design Documents](../design/) - Architecture details
