# Installation Guide

This guide covers installing VeloZ for development and production environments.

## System Requirements

### Minimum Requirements

| Component | Requirement |
|-----------|-------------|
| OS | Linux (Ubuntu 20.04+), macOS 12+, Windows 10+ |
| CPU | x86_64 or ARM64 |
| RAM | 4 GB minimum, 8 GB recommended |
| Disk | 2 GB for build, 500 MB for runtime |

### Software Dependencies

| Dependency | Version | Required | Notes |
|------------|---------|----------|-------|
| CMake | >= 3.24 | Yes | Build system |
| C++ Compiler | C++23 capable | Yes | Clang 16+ or GCC 13+ |
| Python | >= 3.8 | Yes | Gateway runtime |
| OpenSSL | >= 1.1.1 | Recommended | TLS support for WebSocket |
| Ninja | >= 1.10 | Recommended | Faster builds |

## Quick Installation

### Ubuntu/Debian

```bash
# Install system dependencies
sudo apt update
sudo apt install -y \
    cmake \
    ninja-build \
    build-essential \
    clang-16 \
    libssl-dev \
    python3 \
    python3-pip \
    git

# Clone repository
git clone https://github.com/your-org/VeloZ.git
cd VeloZ

# Build
cmake --preset dev
cmake --build --preset dev -j$(nproc)

# Verify installation
./build/dev/apps/engine/veloz_engine --version
```

### macOS

```bash
# Install Xcode Command Line Tools
xcode-select --install

# Install Homebrew dependencies
brew install cmake ninja openssl@3

# Clone and build
git clone https://github.com/your-org/VeloZ.git
cd VeloZ

cmake --preset dev
cmake --build --preset dev -j$(sysctl -n hw.ncpu)
```

### Windows (MSVC)

```powershell
# Install Visual Studio 2022 with C++ workload
# Install CMake from https://cmake.org/download/

# Clone repository
git clone https://github.com/your-org/VeloZ.git
cd VeloZ

# Build with MSVC
cmake --preset dev
cmake --build --preset dev -j
```

## KJ Library Dependencies

VeloZ uses the KJ library (v1.3.0) from Cap'n Proto as its core C++ foundation. KJ is automatically fetched and built via CMake FetchContent.

### Automatically Fetched Dependencies

| Library | Version | Purpose |
|---------|---------|---------|
| KJ | 1.3.0 | Core utilities, async I/O, memory management |
| GoogleTest | Latest | Testing framework |
| yyjson | Latest | JSON parsing |

### KJ Features Used

- `kj::MutexGuarded<T>` - Thread-safe state management
- `kj::Own<T>` - Owned pointers (replaces `std::unique_ptr`)
- `kj::Maybe<T>` - Nullable values (replaces `std::optional`)
- `kj::String` / `kj::StringPtr` - String handling
- `kj::Array<T>` / `kj::Vector<T>` - Dynamic arrays
- `kj::Promise<T>` - Async operations
- `kj::AsyncIoContext` - Async I/O (WebSocket, HTTP)

### Manual KJ Installation (Optional)

If you prefer to use a system-installed KJ library:

```bash
# Clone Cap'n Proto
git clone https://github.com/capnproto/capnproto.git
cd capnproto/c++

# Build and install
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
sudo cmake --install build

# Configure VeloZ to use system KJ
cmake --preset dev -DUSE_SYSTEM_KJ=ON
```

## Build Presets

VeloZ provides CMake presets for different build configurations:

| Preset | Description | Use Case |
|--------|-------------|----------|
| `dev` | Debug build with symbols | Development |
| `release` | Optimized release build | Production |
| `asan` | AddressSanitizer enabled | Memory debugging |

### Development Build

```bash
cmake --preset dev
cmake --build --preset dev -j$(nproc)
```

Output: `build/dev/apps/engine/veloz_engine`

### Release Build

```bash
cmake --preset release
cmake --build --preset release -j$(nproc)
```

Output: `build/release/apps/engine/veloz_engine`

### ASan Build (Memory Debugging)

```bash
cmake --preset asan
cmake --build --preset asan -j$(nproc)
```

## Build Targets

Build specific components instead of everything:

```bash
# Build only engine
cmake --build --preset dev-engine -j$(nproc)

# Build only libraries
cmake --build --preset dev-libs -j$(nproc)

# Build only tests
cmake --build --preset dev-tests -j$(nproc)
```

## Running Tests

```bash
# Run all tests
ctest --preset dev -j$(nproc)

# Run specific test suite
./build/dev/libs/core/veloz_core_tests
./build/dev/libs/market/veloz_market_tests
./build/dev/libs/backtest/veloz_backtest_tests
```

## Environment Variables

Configure VeloZ behavior via environment variables:

### Gateway Configuration

| Variable | Default | Description |
|----------|---------|-------------|
| `VELOZ_HOST` | `0.0.0.0` | HTTP server host |
| `VELOZ_PORT` | `8080` | HTTP server port |
| `VELOZ_PRESET` | `dev` | Build preset to use |

### Market Data Configuration

| Variable | Default | Description |
|----------|---------|-------------|
| `VELOZ_MARKET_SOURCE` | `sim` | Market source: `sim` or `binance_rest` |
| `VELOZ_MARKET_SYMBOL` | `BTCUSDT` | Trading symbol |
| `VELOZ_BINANCE_BASE_URL` | `https://api.binance.com` | Binance REST API URL |

### Execution Configuration

| Variable | Default | Description |
|----------|---------|-------------|
| `VELOZ_EXECUTION_MODE` | `sim_engine` | Execution mode |
| `VELOZ_BINANCE_TRADE_BASE_URL` | `https://testnet.binance.vision` | Binance trade API URL |
| `VELOZ_BINANCE_API_KEY` | (empty) | Binance API key |
| `VELOZ_BINANCE_API_SECRET` | (empty) | Binance API secret |
| `VELOZ_BINANCE_WS_BASE_URL` | `wss://testnet.binance.vision/ws` | Binance WebSocket URL |

## Verification

After installation, verify everything works:

```bash
# Check engine starts
./build/dev/apps/engine/veloz_engine --help

# Run smoke test
./scripts/run_engine.sh dev

# Start gateway and UI
./scripts/run_gateway.sh dev
# Open http://127.0.0.1:8080 in browser

# Run test suite
ctest --preset dev -j$(nproc)
```

## Troubleshooting

### CMake version too old

```bash
# Check version
cmake --version

# Ubuntu: Install newer CMake
sudo apt remove cmake
pip3 install cmake
```

### Compiler doesn't support C++23

```bash
# Ubuntu: Install Clang 16
sudo apt install clang-16
export CC=clang-16
export CXX=clang++-16
cmake --preset dev
```

### OpenSSL not found

```bash
# Ubuntu
sudo apt install libssl-dev

# macOS
brew install openssl@3
export OPENSSL_ROOT_DIR=$(brew --prefix openssl@3)
```

### Build fails with KJ errors

```bash
# Clean and rebuild
rm -rf build/
cmake --preset dev
cmake --build --preset dev -j$(nproc)
```

### Tests fail

```bash
# Run with verbose output
ctest --preset dev --output-on-failure

# Run specific failing test
./build/dev/libs/core/veloz_core_tests --gtest_filter=*TestName*
```

## Next Steps

- [Getting Started Guide](getting-started.md) - Quick start tutorial
- [Configuration Guide](configuration.md) - Detailed configuration options
- [Production Deployment](../deployment/production_architecture.md) - Deploy to production
- [Development Guide](development.md) - Contributing to VeloZ
