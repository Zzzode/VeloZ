# KJ Library Migration Plan for VeloZ

## Overview

Copy KJ library source code directly into `libs/core/kj/` instead of using CMake FetchContent.
This approach:
- Eliminates external dependency management complexity
- Allows direct customization if needed
- Ensures KJ version is fixed and controlled
- No network access required during build

## KJ Library Components

Based on https://github.com/capnproto/capnproto/tree/v2/c%2B%2B/src/kj, KJ provides:

| Module | Purpose | Header Files | Source Files |
|---------|---------|-------------|--------------|
| Async I/O | Promise/Future, async operations | async.h | async.c++ |
| Event Loop | Platform-specific event loops | async-unix.h | async-unix.c++ |
| Networking | Async TCP/UDP sockets | async-io.h | async-io.c++ |
| Threading | Mutex, thread utilities | thread.h, mutex.h | thread.c++ |
| Time | Timer, time utilities | time.h | - |
| Memory | Memory allocation, pools | memory.h | memory.c++ |
| Exception | Exception handling | exception.h | exception.c++ |
| String | String utilities | string.h | string.c++ |
| I/O | File, pipe I/O | io.h | io.c++ |
| Vector | Dynamic arrays | vector.h | - |
| Function | Callable utilities | function.h | - |
| Common | Utilities and types | common.h | common.c++ |
| Debug | Debug utilities | debug.h | debug.c++ |

## Directory Structure

After migration, the structure will be:

```
libs/core/
├── include/veloz/core/
│   ├── event_loop.h
│   ├── logger.h
│   ├── memory.h
│   └── ...
├── src/
│   ├── event_loop.cpp
│   ├── logger.cpp
│   ├── memory.cpp
│   └── ...
├── kj/                          # KJ library source code (copied)
│   ├── kj/                      # KJ namespace headers
│   │   ├── async.h
│   │   ├── async-unix.h
│   │   ├── async-io.h
│   ├── async.c++
│   ├── async-unix.c++
│   ├── async-io.c++
│   ├── common.h
│   ├── common.c++
│   ├── debug.h
│   ├── debug.c++
│   ├── exception.h
│   ├── exception.c++
│   ├── function.h
│   ├── io.h
│   ├── io.c++
│   ├── memory.h
│   ├── memory.c++
│   ├── mutex.h
│   ├── string.h
│   ├── string.c++
│   ├── thread.h
│   ├── thread.c++
│   ├── time.h
│   └── vector.h
└── CMakeLists.txt
```

## Migration Mapping

### 1. Event Loop (Priority: CRITICAL)

**Current Implementation**: `libs/core/src/event_loop.cpp` + `libs/core/include/veloz/core/event_loop.h`

**KJ Replacement**: `kj::EventLoop` from `kj/async.h` and `kj/async-unix.h`

**Migration Steps**:
1. Create wrapper class `veloz::core::EventLoop` that wraps `kj::EventLoop`
2. Replace task queue with `kj::Promise` and `kj::TaskSet`
3. Replace `std::priority_queue` with KJ's priority scheduling
4. Replace `std::condition_variable` with KJ's async primitives
5. Adapt existing event loop API to KJ's model

**Benefits**:
- More efficient cross-platform event loop
- Better timer management
- Integrated with KJ's async ecosystem
- Less code to maintain

### 2. Async I/O & Networking (Priority: CRITICAL)

**Current Implementation**: Custom WebSocket using KJ async I/O, CURL for REST API

**KJ Replacement**: `kj::AsyncIoStream`, `kj::AsyncInputStream`, `kj::AsyncOutputStream`

**Migration Steps**:
1. ~~Replace WebSocket with KJ's async networking~~ (COMPLETED - Custom WebSocket implementation using KJ async I/O)
2. Replace CURL HTTP client with KJ's async HTTP
3. Use `kj::Promise` for async operations
4. Implement timeout handling with KJ timers

**Files to Modify**:
- `libs/market/src/binance_websocket.cpp`
- `libs/exec/src/binance_adapter.cpp`

### 3. Threading & Synchronization (Priority: HIGH)

**Current Implementation**: `std::mutex`, `std::condition_variable`, `std::scoped_lock`

**KJ Replacement**: `kj::Mutex`, `kj::MutexGuarded`, `kj::Thread`

**Migration Steps**:
1. Replace all `std::mutex` with `kj::Mutex`
2. Replace `std::scoped_lock` with `kj::MutexGuarded<T>` or `kj::LockGuard`
3. Replace `std::thread` with `kj::Thread`
4. Replace `std::condition_variable` with KJ's async primitives

**Benefits**:
- KJ's `MutexGuarded<T>` provides thread-safe value wrappers
- Better debugging and error handling
- Consistent with KJ's async model

### 4. Memory Management (Priority: HIGH)

**Current Implementation**: `libs/core/src/memory.cpp` + `libs/core/include/veloz/core/memory.h`

**KJ Replacement**: `kj::Memory` utilities

**Migration Steps**:
1. Replace `aligned_alloc`/`aligned_free` with `kj::heapArray` / `kj::heap`
2. Replace `ObjectPool<T>` with `kj::Arena`
3. Replace `MemoryStats` with KJ's built-in memory tracking
4. Use `kj::Own<T>` for unique ownership

**Benefits**:
- KJ Arena provides fast allocation and bulk deallocation
- Built-in memory leak detection (in debug mode)
- More efficient for temporary objects

### 5. Exception Handling (Priority: MEDIUM)

**Current Implementation**: `libs/core/include/veloz/core/error.h`

**KJ Replacement**: `kj::Exception` from `kj/exception.h`

**Migration Steps**:
1. Create `veloz::core::VeloZException` inheriting from `kj::Exception`
2. Add file/line/function information using `kj::Exception::Context`
3. Preserve existing error types as KJ exception subclasses

**Benefits**:
- Consistent exception handling across KJ ecosystem
- Better stack trace support
- Structured error context

### 6. String & Formatting (Priority: LOW)

**Current Implementation**: `std::string`, `std::format`

**KJ Replacement**: `kj::String`, `kj::StringPtr`

**Migration Steps**:
1. Evaluate where `kj::String` can replace `std::string`
2. Use `kj::Stringify` for type conversions
3. Keep `std::format` for C++23 compatibility

**Note**: Partial migration only - keep std::string where needed for external API compatibility

### 7. Time & Timers (Priority: MEDIUM)

**Current Implementation**: `libs/core/include/veloz/core/time.h`

**KJ Replacement**: `kj::TimePoint`, `kj::Duration` from `kj/time.h`

**Migration Steps**:
1. Replace custom time types with KJ types
2. Use KJ timers in event loop
3. Convert timestamp utilities

### 8. Keep Unchanged (No KJ replacement needed)

The following components are well-designed and should be kept as-is:

| Component | Status | Reason |
|-----------|--------|--------|
| Configuration | Keep | KJ doesn't have config system |
| JSON | Keep | KJ doesn't have JSON parsing |
| Logger | Keep | Current logger has rotation support |
| Metrics | Keep | Custom metrics are domain-specific |

## Integration Steps

### Step 1: Copy KJ Source

```bash
# Clone Cap'n Proto repository
git clone --depth 1 --branch v2.0.1.1 https://github.com/capnproto/capnproto.git /tmp/capnproto

# Copy KJ source to libs/core/kj/
mkdir -p libs/core/kj
cp -r /tmp/capnproto/c++/src/kj/* libs/core/kj/
cp -r /tmp/capnproto/c++/src/kj/kj libs/core/kj/

# Cleanup
rm -rf /tmp/capnproto
```

### Step 2: Update CMakeLists.txt

Add KJ source files to build:

```cmake
# KJ library sources
set(KJ_SOURCES
    libs/core/kj/common.c++
    libs/core/kj/debug.c++
    libs/core/kj/exception.c++
    libs/core/kj/io.c++
    libs/core/kj/memory.c++
    libs/core/kj/async.c++
    libs/core/kj/async-io.c++
    libs/core/kj/async-unix.c++
    libs/core/kj/string.c++
    libs/core/kj/thread.c++
)

# KJ include directory
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/libs/core/kj
)

# Add KJ sources to veloz_core
target_sources(veloz_core PRIVATE ${KJ_SOURCES})
```

### Step 3: Phase 1 - Event Loop & Synchronization (Week 1)

1. Copy KJ source to `libs/core/kj/`
2. Update CMakeLists.txt
3. Create `veloz::core::EventLoop` wrapper around `kj::EventLoop`
4. Replace all `std::mutex` with `kj::Mutex`
5. Replace lock guards with `kj::LockGuard`

### Step 4: Phase 2 - Memory & Exceptions (Week 2)

1. Integrate KJ memory utilities
2. Replace custom pools with `kj::Arena`
3. Convert exceptions to inherit from `kj::Exception`
4. Update tests

### Step 5: Phase 3 - Networking (Week 3-4)

1. ~~Implement async HTTP client using KJ~~
2. ~~Implement async WebSocket using KJ~~ (COMPLETED - Custom WebSocket using KJ async I/O)
3. Remove CURL and websocketpp dependencies (websocketpp removed, CURL still in use for REST)
4. Update market and exec modules

## File-by-File Migration Checklist

### Core Library

| File | Status | KJ Component | Notes |
|-------|----------|---------------|-------|
| `libs/core/kj/` | To Add | All KJ source | Copy from Cap'n Proto |
| `libs/core/src/event_loop.cpp` | Pending | `kj::EventLoop` | Major rewrite |
| `libs/core/include/veloz/core/event_loop.h` | Pending | `kj::EventLoop` | API compatibility |
| `libs/core/src/memory.cpp` | Pending | `kj::Arena`, `kj::Own` | Replace pools |
| `libs/core/include/veloz/core/memory.h` | Pending | `kj::Memory` | Simplify |
| `libs/core/include/veloz/core/error.h` | Pending | `kj::Exception` | Inheritance |
| `libs/core/include/veloz/core/time.h` | Pending | `kj::TimePoint` | Type conversion |
| `libs/core/CMakeLists.txt` | Pending | KJ sources | Add KJ build |

### Market Library

| File | Status | KJ Component | Notes |
|-------|----------|---------------|-------|
| `libs/market/src/binance_websocket.cpp` | Completed | `kj::AsyncIoStream` | Custom WebSocket implementation using KJ async I/O |
| `libs/market/include/veloz/market/binance_websocket.h` | Completed | KJ async types | RFC 6455 WebSocket protocol implementation |

### Exec Library

| File | Status | KJ Component | Notes |
|-------|----------|---------------|-------|
| `libs/exec/src/binance_adapter.cpp` | Pending | `kj::AsyncIoStream` | Async HTTP |
| `libs/exec/include/veloz/exec/binance_adapter.h` | Pending | KJ async types | Interface changes |

## CMake Integration

```cmake
# libs/core/CMakeLists.txt

# KJ library source files (copied from Cap'n Proto v2.0.1.1)
set(KJ_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/kj/common.c++
    ${CMAKE_CURRENT_SOURCE_DIR}/kj/debug.c++
    ${CMAKE_CURRENT_SOURCE_DIR}/kj/exception.c++
    ${CMAKE_CURRENT_SOURCE_DIR}/kj/io.c++
    ${CMAKE_CURRENT_SOURCE_DIR}/kj/memory.c++
    ${CMAKE_CURRENT_SOURCE_DIR}/kj/async.c++
    ${CMAKE_CURRENT_SOURCE_DIR}/kj/async-io.c++
    ${CMAKE_CURRENT_SOURCE_DIR}/kj/async-unix.c++
    ${CMAKE_CURRENT_SOURCE_DIR}/kj/string.c++
    ${CMAKE_CURRENT_SOURCE_DIR}/kj/thread.c++
)

# KJ include path
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/kj)

# Add KJ sources to veloz_core library
target_sources(veloz_core PRIVATE ${KJ_SOURCES})

# Platform-specific KJ files
if(UNIX)
    target_sources(veloz_core PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/kj/async-unix.c++)
elseif(WIN32)
    # async-win32.c++ would be added if Windows support is needed
endif()
```

## Backward Compatibility

During migration, maintain compatibility by:

1. **Wrapper Classes**: Keep VeloZ class names wrapping KJ implementations
2. **Adapter Layer**: Create adapter functions for common operations
3. **Gradual Migration**: Convert module by module, test after each
4. **Feature Flags**: Use CMake options to enable/disable KJ features

## Risks & Mitigations

| Risk | Impact | Mitigation |
|-------|----------|-------------|
| KJ API unfamiliar to team | Medium | Training sessions, code review |
| Breaking changes in interfaces | High | Maintain wrapper APIs |
| Performance regression | Medium | Benchmark before/after |
| Integration issues | Medium | Incremental migration with tests |
| KJ version updates | Low | Version pinned by copy |
| License compatibility | Low | KJ is MIT (compatible) |

## Success Criteria

1. All tests pass with KJ implementation
2. No memory leaks (valgrind/ASan clean)
3. Performance maintained or improved
4. Code complexity reduced
5. External dependencies reduced (CURL still in use for REST API, websocketpp removed)
6. Documentation updated

## References

- KJ Library: https://github.com/capnproto/capnproto/tree/v2/c%2B%2B/src/kj
- Cap'n Proto Docs: https://capnproto.org/
- KJ Async Tutorial: https://capnproto.org/encoding.html#async
- KJ License: MIT (https://github.com/capnproto/capnproto/blob/master/LICENSE)
