# KJ Library Usage Guide for VeloZ

## Overview

This document provides guidance on how to use the KJ library (from Cap'n Proto) in the VeloZ codebase. KJ is used for:

- **Memory management** (Arena, Own, Maybe)
- **Thread synchronization** (Mutex, MutexGuarded)
- **Asynchronous I/O** (Promises, async streams)
- **Exceptions and logging** (Exception, assertions)
- **String handling** (String, StringPtr, StringTree)

## Table of Contents

1. [Quick Reference](#quick-reference)
2. [Memory Management](#memory-management)
3. [Thread Synchronization](#thread-synchronization)
4. [Async/Promises](#asyncpromises)
5. [Exceptions](#exceptions)
6. [String Handling](#string-handling)

---

## Quick Reference

### Common Headers to Include

```cpp
#include <kj/common.h>   // Core utilities
#include <kj/memory.h>   // Memory management
#include <kj/mutex.h>    // Thread synchronization
#include <kj/async.h>      // Async/promises
#include <kj/exception.h> // Exception handling
#include <kj/string.h>     // String handling
#include <kj/io.h>        // I/O streams
```

### Basic Type Definitions

```cpp
// Owned pointer - use when you need unique ownership
kj::Own<MyType> ptr;

// Nullable value - use when a value may be null
kj::Maybe<MyType> maybeValue;

// Array - owned array
kj::Array<MyType> arr;

// String pointer - non-owned reference to string
kj::StringPtr str;

// Thread-safe wrapper - protects value with mutex
kj::MutexGuarded<MyType> guardedValue;
```

---

## Memory Management

### Own<T> - Unique Ownership

`kj::Own<T>` represents owned pointer to a heap-allocated object. When the `Own` goes out of scope, the object is automatically destroyed (via a `kj::Disposer`).

```cpp
// Allocate and return owned object
kj::Own<MyObject> obj = kj::heap<MyObject>(arg1, arg2, ...);

// Return from function (ownership transfer)
kj::Own<MyObject> createMyObject() {
    return kj::heap<MyObject>(...);
}

// Move ownership
kj::Own<MyObject> moved = kj::mv(original);  // Original is now null
```

**When to use**: When you need a value that will live independently and has clear ownership semantics.

**Key Points**:
- Never use `new`/`delete` directly
- Always use `kj::heap` for allocation
- Ownership is transferred when passed to function or returned

### Maybe<T> - Nullable Values

`kj::Maybe<T>` is either a value of type `T` or null. Always use this instead of raw pointers when a value may be missing.

```cpp
// Function that may fail
kj::Maybe<MyObject*> findMyObject() {
    if (notFound) {
        return nullptr;
    }
    return kj::heap<MyObject>(...);
}

// Usage with KJ_IF_SOME macro
KJ_IF_SOME(maybeObj, [&]() {
    maybeObj->doSomething();
});
```

**When to use**:
- Optional return values from functions
- Configuration values that may be absent
- Any pointer that might be null should be `kj::Maybe<T&>` instead of `T*`

### Array<T> - Dynamic Arrays

`kj::Array<T>` is a growable array similar to `std::vector<T>` but with different memory characteristics.

```cpp
// Create array using builder
kj::Array<kj::String> strings = kj::heapArray<kj::String>(size);
auto builder = kj::heapArrayBuilder<kj::String>();
builder.add("hello");
builder.add("world");
strings = builder.finish();

// Access elements
for (auto s : strings) {
    KJ_LOG(INFO, "string: ", s);
}
```

**When to use**: When you need a dynamic array that will be modified.

### Arena<T> - Fast Temporary Allocation

`kj::Arena` provides fast allocation with bulk deallocation. Useful for temporary objects or per-request allocations.

```cpp
// Create arena and allocate from it
kj::Arena arena;
void processRequest() {
    // All allocations during this function are freed when arena goes out of scope
    kj::Own<MyType> obj = arena.construct<MyType>(...);
    // Use obj...
}

// Reset arena (free all allocations)
arena.reset();
```

**When to use**:
- When many small objects are allocated and freed together
- For request/response processing
- Avoids memory fragmentation

---

## Thread Synchronization

### Mutex - Basic Locking

`kj::Mutex` is a simple mutex. For most cases in VeloZ, use `kj::MutexGuarded<T>` instead which provides better ergonomics.

```cpp
// Simple mutex
kj::Mutex myMutex;
kj::Mutex::Guard lock1(myMutex);
// Lock acquired, released when lock1 goes out of scope

// MutexGuarded<T> - Thread-safe value wrapper
kj::MutexGuarded<MyType> guardedValue;
void accessValue() {
    auto lock = guardedValue.lockExclusive();  // Returns kj::Locked<T>
    lock->doSomething();
    // Lock automatically released
}
```

**When to use**:
- Protecting a single value from concurrent access
- Prefer `MutexGuarded<T>` when you have a single value that needs protection

### MutexGuarded<T> - Thread-Safe Values

`kj::MutexGuarded<T>` combines a mutex with a value. It enforces that the value can only be accessed after obtaining a lock.

```cpp
// State class with thread-safe values
class MyState {
public:
    kj::MutexGuarded<StateData> guarded_;

    kj::String getFileName() {
        auto lock = guarded_.lockExclusive();
        return lock->filename;
    }
};

// Constructor
MyState::MyState() : guarded_(kj::atomicRefcounted<StateData>(...)) {}
```

**When to use**:
- Replace `std::mutex` + `T` combinations with `kj::MutexGuarded<T>`
- All access methods must call `.lockExclusive()` first
- Lock is automatically released when the lock object goes out of scope

### Lazy<T> - Lazy Initialization

`kj::Lazy<T>` delays construction of a value until first access. Useful for expensive-to-construct objects.

```cpp
// Lazy value
kj::Lazy<MyExpensiveObject> lazyObj;

// First access - object constructed
lazyObj->doSomething();

// Or eager evaluation
kj::Own<MyExpensiveObject> obj = lazyObj.eagerlyEvaluate();
```

**When to use**:
- When object construction is expensive and may not be needed
- Defers initialization until first use

---

## Async/Promises

### Promise<T> - Asynchronous Operations

`kj::Promise<T>` represents an asynchronous operation that will complete with a value of type `T` or reject with an exception.

```cpp
// Simple immediate promise
kj::Promise<int> createImmediatePromise() {
    return 123;  // Resolved immediately
}

// Async operation
kj::Promise<kj::String> fetchData() {
    // Returns immediately, completes when async operation finishes
    return networkAddress.connect();
}

// Chain promises with .then()
kj::Promise<kj::String> textPromise = fetchData();
kj::Promise<int> lineCountPromise = textPromise.then([](kj::String text) {
    int lineCount = 0;
    for (char c : text) {
        if (c == '\n') ++lineCount;
    }
    return lineCount;
});

// Wait for promise (synchronous)
kj::WaitScope waitScope(io.waitScope);
kj::Promise<void> result = someAsyncFunc();
result.wait(waitScope);  // Blocks until complete
```

**When to use**:
- All async functions return a `Promise<T>` immediately (non-blocking)
- Use `.then()` to register callbacks for completion
- Use `.wait()` with a `kj::WaitScope` only when you must block

### Async Streams - I/O Operations

```cpp
// Read from async stream
kj::Promise<kj::String> textPromise = stream.readAllText();

// Write to async stream
kj::Promise<void> writePromise = stream.write(text);
stream.write(text).attach(kj::mv(stream));

// WaitScope - required for async operations
kj::AsyncIoContext io = kj::setupAsyncIo();
kj::WaitScope waitScope(io.waitScope);
```

**When to use**:
- All async I/O requires an `AsyncIoContext` and `WaitScope`
- Use `.attach()` to keep objects alive during async operations

---

## Exceptions

### Exception Handling

`kj::Exception` is KJ's exception type. All KJ exceptions inherit from `kj::Exception`.

```cpp
// Throw a KJ exception
KJ_THROW(KJ_EXCEPTION(FAILED, "operation failed"), errorCode);

// Recoverable vs fatal exceptions
// Fatal exceptions terminate the program
// Recoverable exceptions can be caught and handled
```

### Assertions

Use `KJ_ASSERT` for runtime checks that should never fail.

```cpp
KJ_ASSERT(condition, "description", context...);
KJ_REQUIRE(condition, "description");  // For preconditions

// Example
KJ_ASSERT(count > 0, "count must be positive");
KJ_REQUIRE(ptr != nullptr, "pointer must not be null");
```

### System Call Wrappers

```cpp
// Unix file descriptor operations
kj::OwnFd fd = KJ_SYSCALL_FD(
    open(path.cStr(), O_RDONLY),
    "couldn't open file",
    path);

// Generic system calls
ssize_t n = KJ_SYSCALL(n = read(fd, buffer, sizeof(buffer)));
```

---

## String Handling

### String<T> - Owned Strings

`kj::String` is similar to `std::string` but is not copy-optimized and maintains stable memory locations.

```cpp
// String literals (auto kj::StringPtr)
auto greeting = "Hello, world!"_kj;  // Creates kj::StringPtr

// String concatenation
kj::String message = kj::str("Hello, ", name, "!");
auto fullMsg = kj::str(message, " The time is: ", timestamp);

// Get C string (for legacy APIs)
const char* cStr = myString.cStr();
```

**When to use**:
- Use `kj::str()` for stringification
- Use `"_kj` suffix for string literals
- Use `.cStr()` when passing to C APIs expecting NUL-terminated strings

### StringPtr<T> - Non-Owned String References

`kj::StringPtr` points to a `kj::String` but doesn't own it. Moving the original string doesn't invalidate the pointer.

```cpp
// StringPtr doesn't invalidate when original moves
kj::StringPtr ptr1 = originalString;
kj::StringPtr ptr2 = kj::mv(ptr1);  // ptr1 is still valid
```

**When to use**:
- Use `kj::StringPtr` when you need to reference a string without ownership
- Safer than `std::string_view` because invalidation can't happen by moving original

---

## Common Patterns

### Moving Objects

Always use `kj::mv()` to transfer ownership when passing an `kj::Own` to another function.

```cpp
kj::Own<Foo> createFoo() {
    return kj::heap<Foo>(...);
}

void process(kj::Own<Foo> foo) {
    // Ownership transferred to this function
    // foo will be destroyed when process() returns
}
```

### Scope-Based Cleanup

Use `kj::defer()` to execute code when exiting scope.

```cpp
void closeFile(int fd) {
    auto closeOnExit = kj::defer([fd]() { close(fd); });
    // ... use fd ...
    // closeOnExit destroyed after this function returns
}
```

### Constness

KJ enforces const-correctness through `kj::Maybe<T&>` and requires thinking about mutability.

```cpp
// Const reference for read-only access
void readConfig(const kj::Maybe<kj::StringPtr>& configPtr) {
    KJ_IF_SOME(configPtr, [&]() {
        auto lock = configPtr->lockExclusive();
        const auto& config = *lock;
        // Config is now guaranteed not to be modified while we're reading it
        processConfig(config);
    });
}
```

---

## Migration Guidelines for VeloZ

### Replacing std::mutex

```cpp
// Before (C++ standard library)
std::mutex myMutex;
std::unique_lock<std::mutex> lock(myMutex);

// After (KJ library)
kj::Mutex myMutex;

// For simple cases
class ThreadSafeCounter {
    kj::MutexGuarded<uint64_t> count_;
public:
    uint64_t incrementAndGet() {
        auto lock = count_.lockExclusive();
        return ++(*lock);
    }
};
```

### Replacing std::unique_ptr

```cpp
// Before
std::unique_ptr<MyObject> ptr = std::make_unique<MyObject>(...);

// After (KJ library)
kj::Own<MyObject> ptr = kj::heap<MyObject>(...);
// Ownership is automatic with kj::Own, no manual delete needed
```

### Replacing std::optional

```cpp
// Before
std::optional<MyType> opt;

// After (KJ library)
kj::Maybe<MyType> opt;  // Always use Maybe for nullable values
```

### Using KJ with VeloZ Event Loop

When integrating KJ async I/O with the existing VeloZ event loop, you may need to integrate them:

```cpp
// The VeloZ event loop drives KJ's event loop
class VeloZEventLoop {
private:
    kj::AsyncIoContext& io_;
    kj::WaitScope& waitScope_;

public:
    void run() {
        // Set up KJ async I/O
        io_ = kj::setupAsyncIo();
        waitScope_ = kj::WaitScope(io_);

        // Run forever
        kj::NEVER_DONE.wait(waitScope_);
    }
};
```

---

## Common Pitfalls

### Memory Leaks

Never create a `kj::Own` without consuming it or passing it somewhere that will take ownership.

```cpp
// BAD: Creating own object but not using it
kj::Own<Foo> obj = kj::heap<Foo>(...);  // LEAK! obj never destroyed

// GOOD: Return it or attach it
kj::Own<Foo> createFoo() {
    return kj::heap<Foo>(...);  // Caller takes ownership
}
```

### Race Conditions

Always think carefully about ordering of operations in async code.

```cpp
// Potential race: modifying value before callback
kj::Maybe<kj::Own<Data>> data;
data->process();  // What if data is modified in callback?
```

### Forgetting WaitScope

All async operations that use `.wait()` require a `kj::WaitScope` to be passed.

```cpp
// GOOD: WaitScope passed as parameter
kj::Promise<int> myFunc(kj::WaitScope& waitScope) {
    // ... use waitScope in async operations ...
    return promise.wait(waitScope);
}

// BAD: Creating WaitScope on stack when not needed
kj::Promise<int> myFunc() {
    kj::WaitScope waitScope;  // Undefined behavior if not passed to wait()
    return promise.wait(waitScope);
}
```

---

## References

- KJ Tour: `/Users/bytedance/Develop/VeloZ/docs/kjdoc/tour.md`
- KJ Style Guide: `/Users/bytedance/Develop/VeloZ/docs/kjdoc/style-guide.md`
- KJ Library: Part of Cap'n Proto: https://capnproto.org/
