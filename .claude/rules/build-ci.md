---
paths:
  - "CMakeLists.txt"
  - "CMakePresets.json"
  - "scripts/build.sh"
  - "scripts/format.sh"
  - ".github/workflows/*.yml"
  - "libs/*/CMakeLists.txt"
---

# Build and CI (Build System Rules)

## Overview

This skill provides guidance for build system configuration, CMake presets, formatting, and CI pipeline management in VeloZ.

## CMake Presets (MANDATORY)

### Available Presets

The repository provides three CMake presets in `CMakePresets.json`:

| Preset | Configuration | Build Type | Use Case |
|---------|--------------|------------|----------|
| `dev` | Debug + generate compile_commands.json | Development builds, debugging |
| `release` | Release + generate compile_commands.json | Production builds, optimized |
| `asan` | Clang + ASan/UBSan (local self-check) | Memory error detection, debugging |

### Build Commands

```bash
# Configure and build all targets (recommended)
cmake --preset <dev|release|asan>
cmake --build --preset <dev|release|asan>-all -j$(nproc)

# Build specific targets
cmake --build --preset dev-engine -j$(nproc)  # Build engine only
cmake --build --preset dev-libs -j$(nproc)    # Build libraries only
cmake --build --preset dev-tests -j$(nproc)   # Build tests only
```

### Build Output Location

Build outputs are located at:
- `build/<preset>/` - Root build directory
- Engine executable: `build/<preset>/apps/engine/veloz_engine`
- Libraries: `build/<preset>/libs/veloz_*.a`
- Tests: `build/<preset>/libs/*/veloz_*_tests`

## CMake Configuration Rules

### Root CMakeLists.txt

**Key Configuration:**

```cmake
cmake_minimum_required(VERSION 3.24)
project(VeloZ LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
```

### Build Options

```cmake
option(VELOZ_ENABLE_WERROR "Treat warnings as errors" OFF)
option(VELOZ_BUILD_TESTS "Build tests" ON)
option(VELOZ_BUILD_BACKTEST "Build backtest module" ON)
option(VELOZ_BUILD_WEBSOCKET "Build WebSocket market data module" ON)
```

### Library Dependencies

**KJ Library Integration:**

```cmake
# Set VERSION variable required by KJ's CMakeLists.txt
set(VERSION "2.0.0")

# Build KJ library from source
add_subdirectory(kj)

# Link against KJ library
target_link_libraries(veloz_core PUBLIC kj kj-async)
```

### Module Subdirectories

```cmake
add_subdirectory(libs/common)
add_subdirectory(libs/core)
add_subdirectory(libs/market)
add_subdirectory(libs/exec)
add_subdirectory(libs/oms)
add_subdirectory(libs/risk)
add_subdirectory(libs/strategy)
if(VELOZ_BUILD_BACKTEST)
    add_subdirectory(libs/backtest)
endif()
add_subdirectory(apps/engine)
```

### Test Configuration

```cmake
# Enable testing early so BUILD_TESTING is set for all subdirectories
if(VELOZ_BUILD_TESTS)
    enable_testing()
endif()

# Tests use KJ Test (kj::TestRunner) instead of GoogleTest
target_link_libraries(veloz_core_tests
    PRIVATE
      veloz_core
      kj-test
    )
```

## Code Formatting (MANDATORY)

### Format Script

Use `scripts/format.sh` to format C++ code:

```bash
./scripts/format.sh
```

**Behavior:**
- Scans `*.h/*.hpp/*.cc/*.cpp/*.cxx` under `apps/` and `libs/`
- Formats in place (`clang-format -i`)
- Supports `--check` option to verify formatting without changes

### Formatting Configuration

The project uses clang-format. Key requirements:
- **2-space indentation** (no tabs)
- **Line length**: Maximum 100 characters
- **Brace placement**: Opening brace at end of statement, closing brace on new line

### CI Format Check

The CI pipeline runs a format check job (`format-check`):

```yaml
format-check:
  - name: Check C++ format
  run: |
    ./scripts/format.sh --check 2>&1 || {
      echo "Code formatting check failed. Please run ./scripts/format.sh to fix."
      exit 1
    }
```

## CI Pipeline (MANDATORY)

### Workflow Triggers

CI runs on:
- **Push** to `main` or `master` branches
- **Pull requests** targeting `main` or `master` branches
- **Workflow dispatch** for manual triggers

### CI Jobs

#### Development Build (`dev-build`)

```yaml
dev-build:
  steps:
    - name: Set up CMake
      uses: lukka/get-cmake@v3
      with:
        cmakeVersion: 3.27.7

    - name: Configure (dev)
      run: cmake --preset dev

    - name: Build (dev-all)
      run: cmake --build --preset dev-all -j$(nproc)

    - name: Run all tests
      run: ctest --preset dev -j$(nproc)

    - name: Smoke run engine
      run: timeout 3s ./build/dev/apps/engine/veloz_engine || true
```

#### Release Build (`release-build`)

```yaml
release-build:
  steps:
    - name: Configure (release)
      run: cmake --preset release

    - name: Build (release-all)
      run: cmake --build --preset release-all -j$(nproc)

    - name: Smoke run engine (release)
      run: timeout 3s ./build/release/apps/engine/veloz_engine || true
```

#### ASan Build (`asan-build`)

```yaml
asan-build:
  steps:
    - name: Install Clang
      run: sudo apt-get update && sudo apt-get install -y clang-16

    - name: Configure (asan)
      run: cmake --preset asan

    - name: Build (asan-all)
      run: cmake --build --preset asan-all -j$(nproc)

    - name: Run all tests with ASan
      run: ctest --test-dir build/asan -j$(nproc)
```

#### Python Code Check (`python-check`)

```yaml
python-check:
  steps:
    - name: Install dependencies
      run: |
        if [ -f "python/requirements.txt" ]; then
          pip install -r python/requirements.txt
        fi
        if [ -f "apps/gateway/requirements.txt" ]; then
          pip install -r apps/gateway/requirements.txt
        fi

    - name: Lint Python code
      run: |
        flake8 python/ --count --select=E9,F63,F7,F82 --show-source --statistics
        flake8 apps/gateway/ --count --exit-zero --max-complexity=10 --max-line-length=127 --statistics
```

#### Documentation Check (`docs-check`)

```yaml
docs-check:
  steps:
    - name: Check for broken links in documentation
      run: |
        test -f "README.md" || exit 1
        test -f "CLAUDE.md" || exit 1
        test -d "docs" || exit 1
```

## KJ Library in Build

### KJ Source Integration

The KJ library source is included in `libs/core/kj/`:

**CMake Configuration:**

```cmake
# libs/core/CMakeLists.txt
set(VERSION "2.0.0")  # Required by KJ's CMakeLists.txt
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)  # Suppress KJ deprecation warnings

add_subdirectory(kj)  # Build KJ from source

target_link_libraries(veloz_core PUBLIC kj kj-async)
```

### KJ Policy Settings

Set CMAKE policy to suppress KJ deprecation warnings:

```cmake
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
```

## Testing with CTest

### Running Tests

```bash
# Run all tests with CTest preset
ctest --preset dev -j$(nproc)

# Run tests from specific build directory
ctest --test-dir build/asan -j$(nproc)
```

### Test Executables

Each library module builds a test executable following the pattern:

```
veloz_core_tests    # Core library tests
veloz_market_tests   # Market library tests
veloz_exec_tests     # Execution library tests
veloz_oms_tests       # OMS library tests
veloz_risk_tests      # Risk library tests
veloz_strategy_tests # Strategy library tests
```

### KJ Test Framework

**CRITICAL: All tests use KJ Test framework (`kj::TestRunner`), NOT GoogleTest.**

Test files use `KJ_TEST` macros for assertions:

```cpp
KJ_TEST("test name") {
    // test code
}
```

## External Dependencies

### Optional Dependencies

| Dependency | Purpose | Required By |
|------------|----------|--------------|
| `yyjson` | JSON parsing | Core library |
| `CURL` | HTTP client | Core library (optional) |
| `OpenSSL` | TLS support | WebSocket module |

### Dependency Management

Use CMake's `find_package()` for optional dependencies:

```cmake
find_package(OpenSSL 1.1.0 REQUIRED)

if(OpenSSL_FOUND)
    set(WITH_OPENSSL ON)
    target_link_libraries(veloz_core PUBLIC OpenSSL::Crypto)
endif()
```

## Build Scripts

### Build Wrapper (`scripts/build.sh`)

```bash
./scripts/build.sh <preset>
```

**Behavior:**
- Calls `cmake --preset <preset>`
- Calls `cmake --build --preset <preset>-all -j$(nproc)`

### Run Scripts

| Script | Purpose | Usage |
|--------|-----------|-------|
| `scripts/run_engine.sh` | Build and run engine with smoke test |
| `scripts/run_gateway.sh` | Build and run gateway with UI |

## Build Errors

### Common Issues

1. **cmake --preset not found**: Ensure CMake >= 3.24
2. **clang-format not found**: Install clang-format or skip formatting
3. **KJ version mismatch**: Update VERSION variable if KJ library changes
4. **OpenSSL not found**: Install OpenSSL development headers (libssl-dev)

## Files to Check

- `/Users/bytedance/Develop/VeloZ/CMakeLists.txt` - Root CMake configuration
- `/Users/bytedance/Develop/VeloZ/CMakePresets.json` - Build presets
- `/Users/bytedance/Develop/VeloZ/libs/core/kj/CMakeLists.txt` - KJ library build
- `/Users/bytedance/Develop/VeloZ/.github/workflows/ci.yml` - CI pipeline
- `/Users/bytedance/Develop/VeloZ/scripts/format.sh` - Formatting script
