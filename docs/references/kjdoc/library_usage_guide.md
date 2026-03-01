# KJ Library Usage Guide for VeloZ

## Overview

This guide provides practical examples and usage patterns for the KJ library in VeloZ. KJ is the **default** choice over the C++ standard library. Use KJ types and patterns whenever possible.

## Common Headers

```cpp
#include <kj/common.h>     // Core utilities
#include <kj/memory.h>     // Memory management
#include <kj/array.h>      // Arrays and vectors
#include <kj/string.h>     // String types
#include <kj/mutex.h>      // Thread synchronization
#include <kj/async.h>      // Async/promises
#include <kj/async-io.h>   // Async I/O
#include <kj/exception.h>  // Exception handling
#include <kj/time.h>       // Time utilities
#include <kj/filesystem.h> // Filesystem
#include <kj/source-location.h> // Source location
```

## Memory Management

### Owned Objects

Use `kj::Own<T>` with `kj::heap<T>()` for owned objects:

```cpp
class MyClass {
public:
  kj::Own<Dependency> dep_;

  MyClass() : dep_(kj::heap<Dependency>()) {}
};
```

### Nullable Values (Maybe)

Use `kj::Maybe<T>` instead of `std::optional<T>`:

```cpp
kj::Maybe<int> findValue();

KJ_IF_SOME(value, findValue()) {
  KJ_LOG(INFO, "Found value: ", value);
}
```

### Arrays and Vectors

Fixed-size arrays use `kj::Array<T>`:

```cpp
kj::Array<int> arr = kj::heapArray<int>(10);
```

Dynamic arrays use `kj::Vector<T>`:

```cpp
kj::Vector<int> vec;
vec.add(1);
vec.add(2);
```

## String Handling

### String Literals

Use KJ string literals:

```cpp
kj::StringPtr name = "Alice"_kj;
kj::ConstString constName = "Bob"_kjc;
```

### String Construction

Use `kj::str()` for concatenation and formatting:

```cpp
kj::String msg = kj::str("Hello, ", name, "!");
```

### String Views

Use `kj::StringPtr` for non-owning string references:

```cpp
void process(kj::StringPtr input);
```

## Thread Synchronization

### MutexGuarded

Use `kj::MutexGuarded<T>` for thread-safe state:

```cpp
class ThreadSafeCounter {
  kj::MutexGuarded<uint64_t> count_;
public:
  uint64_t incrementAndGet() {
    auto lock = count_.lockExclusive();
    return ++(*lock);
  }
};
```

### Shared Locks

KJ supports shared locks:

```cpp
kj::MutexGuarded<int> value;

// Exclusive lock (write)
auto writeLock = value.lockExclusive();

// Shared lock (read)
auto readLock = value.lockShared();
```

## Async and Promises

### Basic Promises

```cpp
kj::Promise<int> computeAsync() {
  return kj::evalLater([]() { return 42; });
}

// Use .then() to chain
computeAsync().then([](int result) {
  KJ_LOG(INFO, "Result: ", result);
});
```

### WaitScope

Wait for promises at top-level:

```cpp
kj::AsyncIoContext io = kj::setupAsyncIo();
kj::WaitScope& waitScope = io.waitScope;

auto promise = computeAsync();
int result = promise.wait(waitScope);
```

### TaskSet

Manage multiple background tasks:

```cpp
kj::TaskSet tasks([](kj::Exception&& e) {
  KJ_LOG(ERROR, "Task failed: ", e);
});

tasks.add(doTask1());
tasks.add(doTask2());
```

## Exception Handling

Use KJ exceptions and macros:

```cpp
KJ_REQUIRE(condition, "Invalid state");
KJ_ASSERT(value > 0, "Value must be positive");

KJ_TRY {
  // risky code
} KJ_CATCH(e) {
  KJ_LOG(ERROR, "Caught: ", e.getDescription());
}
```

## Filesystem Operations

### Read File

```cpp
auto fs = kj::newDiskFilesystem();
auto file = fs->getRoot().openFile(kj::Path{"data.txt"});
kj::String content = file->readAllText();
```

### Write File

```cpp
auto fs = kj::newDiskFilesystem();
auto dir = fs->getRoot().openSubdir(kj::Path{"output"},
    kj::WriteMode::CREATE | kj::WriteMode::CREATE_PARENT);

auto file = dir->openFile(kj::Path{"log.txt"},
    kj::WriteMode::CREATE | kj::WriteMode::MODIFY);
file->writeAll("Hello"_kj);
```

### Directory Listing

```cpp
auto fs = kj::newDiskFilesystem();
auto dir = fs->getRoot().openSubdir(kj::Path{"data"});

for (auto& entry : dir->listEntries()) {
  KJ_LOG(INFO, "Entry: ", entry.name);
}
```

## Source Location

Use `kj::SourceLocation` for file/line info:

```cpp
kj::SourceLocation loc;
KJ_LOG(INFO, "Here: ", loc.fileName, ":", loc.lineNumber);
```

## Common Patterns

### Resource Cleanup

Use `kj::defer()` or `KJ_DEFER`:

```cpp
auto cleanup = kj::defer([]() {
  // cleanup code
});

KJ_DEFER({
  // cleanup code
});
```

### Move Semantics

Use `kj::mv()` for moves:

```cpp
kj::Own<MyType> ptr = kj::heap<MyType>();
auto moved = kj::mv(ptr);
```

### Scope Guards

Use `kj::defer()` or `KJ_DEFER` instead of manual try/finally.

## Common Pitfalls

- Forgetting `_kj` suffix for string literals
- Using `std::string` or `std::vector` out of habit
- Using `std::optional` instead of `kj::Maybe`
- Using `std::unique_ptr` instead of `kj::Own`
- Using `std::mutex` instead of `kj::Mutex`

## Summary

Always choose KJ first. Use std only when KJ does not provide equivalent functionality or external API requires std types.
