# KJ Library Skill for Claude Code

## Description

This skill provides guidance for Claude Code (claude.ai/code) to recognize and use the KJ library from Cap'n Proto when working on the VeloZ codebase. **KJ is the DEFAULT choice over C++ standard library for all applicable use cases.**

## KJ Library Overview

KJ is a utility library from Cap'n Proto that provides:

- **Memory management**: Arena, Own<T>, Maybe<T>, Array<T>, Vector<T>
- **Thread synchronization**: Mutex, MutexGuarded<T>, Lazy<T>
- **Async I/O**: Promises, AsyncIoContext, AsyncIoStream, Network
- **String handling**: String, StringPtr, StringTree
- **Exceptions**: Exception, assertions, system call wrappers
- **Functions**: Function<T> for functors/lambdas
- **Time**: TimePoint, Duration, Timer
- **I/O**: InputStream, OutputStream, AsyncIoStream
- **Networking**: Network, NetworkAddress, ConnectionReceiver, DatagramPort

## MANDATORY: KJ-First Development Rule

When writing C++ code for VeloZ:

### Step 1: Always Check KJ First

Before considering any std library type, check if KJ provides equivalent functionality:

```cpp
// ❌ WRONG - Using std library when KJ available
std::unique_ptr<MyType> ptr = std::make_unique<MyType>(...);
std::optional<int> value;
std::string str = "hello";
std::vector<int> vec;
std::mutex mutex;
std::function<void()> func;

// ✅ CORRECT - Using KJ (default choice)
kj::Own<MyType> ptr = kj::heap<MyType>(...);
kj::Maybe<int> value;
kj::String str = "hello"_kj;
kj::Vector<int> vec;
kj::Mutex mutex;
kj::Function<void()> func;
```

### Step 2: Only Use Std When KJ Unavailable

Use std library types ONLY when:

1. **KJ does not provide equivalent functionality**
   - Example: `std::map`, `std::unordered_map` (KJ has `kj::Table` but not all map features)
   - Example: `std::variant` (KJ has `kj::OneOf` for small types)

2. **External API compatibility requires std types**
   - Example: Third-party library expecting `std::string` or `std::function`

3. **Specific algorithm requirements**
   - Example: `std::sort`, `std::find` when KJ alternatives insufficient

**When using std types, add a comment explaining why:**
```cpp
// Using std::string because external library API expects it
std::string formatForExternalApi(const kj::String& kjStr);
```

## KJ Type Mapping (COMPLETE REFERENCE)

### Memory Management

| Std Type | KJ Equivalent | Notes |
|----------|---------------|-------|
| `new/delete` | `kj::heap<T>()`, `kj::Own<T>` | NEVER use raw new/delete |
| `std::unique_ptr<T>` | `kj::Own<T>` | KJ default for owned objects |
| `std::shared_ptr<T>` | `kj::Own<T>` + manual sharing or `kj::Refcounted` | KJ prefers unique ownership |
| `std::optional<T>` | `kj::Maybe<T>` | Use `KJ_IF_SOME` macro |
| `std::vector<T>` | `kj::Array<T>` (fixed) or `kj::Vector<T>` (dynamic) | KJ arrays don't provide iterator semantics |
| `std::make_shared` | `kj::atomicRefcounted<T>` | For reference-counted objects |
| `std::make_unique` | `kj::heap<T>()` | KJ allocation function |

### Thread Synchronization

| Std Type | KJ Equivalent | Notes |
|----------|---------------|-------|
| `std::mutex` | `kj::Mutex` | KJ mutex with RAII guards |
| `std::unique_lock` | `kj::Mutex::Guard` | RAII lock, auto-release |
| `std::shared_mutex` | `kj::Mutex` (with lockShared) | Use `.lockShared()` for read access |
| `std::condition_variable` | `kj::MutexGuarded<T>` | KJ prefers value-wrapping pattern |
| `std::atomic` | `kj::Atomic` | KJ atomic types |

### Strings

| Std Type | KJ Equivalent | Notes |
|----------|---------------|-------|
| `std::string` | `kj::String` | KJ strings don't invalidate on move |
| `std::string_view` | `kj::StringPtr` | KJ view is safer, doesn't invalidate |
| `"literal"` | `"literal"_kj` | Use `_kj` suffix for KJ string literals |
| `std::to_string` | `kj::str()` | KJ stringification function |
| `std::stringstream` | `kj::str()` | Concatenation in KJ |
| `std::format` | `kj::str()` | KJ str() handles formatting |

### Functions and Callables

| Std Type | KJ Equivalent | Notes |
|----------|---------------|-------|
| `std::function<T>` | `kj::Function<T>` | KJ function wrapper |
| `std::bind` | `kj::mv()` with lambda | KJ prefers lambdas with capture |
| Lambda capture by value | Use `kj::Own<T>` | For captured owned objects |

### Async and Futures

| Std Type | KJ Equivalent | Notes |
|----------|---------------|-------|
| `std::future<T>` | `kj::Promise<T>` | KJ promises chain with `.then()` |
| `std::promise<T>` | `kj::PromiseFulfiller<T>` | KJ promise fulfillment |
| `std::async` | `kj::Promise<T>` with async I/O | KJ has built-in async I/O |
| `std::packaged_task` | `kj::Promise<T>` | KJ promise for task results |

### Time

| Std Type | KJ Equivalent | Notes |
|----------|---------------|-------|
| `std::chrono::duration` | `kj::Duration` | KJ time duration type |
| `std::chrono::time_point` | `kj::TimePoint` | KJ time point type |
| `std::this_thread::sleep_for` | `kj::sleep()` | KJ sleep function |

### Exceptions

| Std Type | KJ Equivalent | Notes |
|----------|---------------|-------|
| `std::exception` | `kj::Exception` | KJ exception base class |
| `std::runtime_error` | `kj::Exception` with type | Use `KJ_EXCEPTION` macro |
| `assert()` | `KJ_ASSERT()` | KJ runtime assertion |
| `std::logic_error` | `KJ_REQUIRE()` | KJ precondition check |
| `try/catch` | Same syntax | KJ exceptions are standard C++ exceptions |

### I/O

| Std Type | KJ Equivalent | Notes |
|----------|---------------|-------|
| `std::istream` | `kj::InputStream` | KJ input stream |
| `std::ostream` | `kj::OutputStream` | KJ output stream |
| `std::ifstream` | `kj::FdInputStream` | KJ file descriptor input |
| `std::ofstream` | `kj::FdOutputStream` | KJ file descriptor output |

### Async I/O

| Std Type | KJ Equivalent | Notes |
|----------|---------------|-------|
| `std::async` | `kj::Promise<T>` | KJ async promises |
| N/A | `kj::AsyncInputStream` | Async input stream |
| N/A | `kj::AsyncOutputStream` | Async output stream |
| N/A | `kj::AsyncIoStream` | Async bidirectional stream |
| N/A | `kj::AsyncIoProvider` | Async I/O factory |
| N/A | `kj::WaitScope` | Synchronous wait scope |

### Networking

| Std Type | KJ Equivalent | Notes |
|----------|---------------|-------|
| N/A | `kj::Network` | Network interface factory |
| N/A | `kj::NetworkAddress` | Network address (connect/listen) |
| N/A | `kj::ConnectionReceiver` | Server socket (accept) |
| N/A | `kj::DatagramPort` | UDP socket |
| N/A | `kj::AsyncIoStream` | TCP connection stream |

### Containers (KJ Has Limited Equivalents)

| Std Type | KJ Equivalent | Notes |
|----------|---------------|-------|
| `std::array` | `kj::Array` | KJ array type |
| `std::unordered_map` | `kj::HashMap` | KJ hash map |
| `std::map` | `kj::TreeMap` | KJ tree map (B-tree) |
| `std::set` | `kj::TreeSet` | KJ tree set |
| `std::unordered_set` | `kj::HashSet` | KJ hash set |
| `std::queue` | None | Use std::queue if needed |
| `std::stack` | None | Use std::stack if needed |
| `std::deque` | None | Use std::deque if needed |

## When to Use KJ (Decision Tree)

```
Writing C++ code?
  ↓
Need a type/functionality?
  ↓
Does KJ provide it? → YES → Use KJ (default)
  ↓ NO
Is it for external API? → YES → Use std (with comment explaining)
  ↓ NO
Is it for algorithm requiring iterators? → YES → Use std (with comment)
  ↓ NO
Can you implement with KJ types? → YES → Refactor to use KJ
  ↓ NO
Use std (with comment explaining why KJ insufficient)
```

## Event Loops and Async I/O (COMPREHENSIVE GUIDE)

### Event Loop Setup

KJ provides an event loop for managing asynchronous operations:

```cpp
#include <kj/async-io.h>

// Full async I/O setup with networking and timers
kj::AsyncIoContext io = kj::setupAsyncIo();
kj::AsyncIoProvider& ioProvider = *io.provider;
kj::LowLevelAsyncIoProvider& lowLevelProvider = *io.lowLevelProvider;
kj::WaitScope& waitScope = io.waitScope;
kj::Network& network = ioProvider.getNetwork();
kj::Timer& timer = ioProvider.getTimer();

// Simple event loop (no OS I/O)
kj::EventLoop eventLoop;
kj::WaitScope waitScope(eventLoop);
```

### WaitScope - Synchronous Waiting

`WaitScope` allows synchronous waiting on promises at the top level:

```cpp
// Wait for a timer
kj::Timer& timer = ioProvider.getTimer();
kj::Promise<void> promise = timer.afterDelay(5 * kj::SECONDS);
promise.wait(waitScope);  // Blocks until promise completes

// Wait for async I/O
kj::Promise<kj::String> dataPromise = stream->readAllText();
kj::String data = dataPromise.wait(waitScope);

// Run event loop forever (server pattern)
kj::NEVER_DONE.wait(waitScope);
```

**Important**: Synchronous waits cannot be nested. They can only be used at the top level of a thread.

### Promises - Async Operations

#### Basic Promise Usage

```cpp
// Create immediate promises
kj::Promise<int> fulfilled = 42;                    // Already fulfilled
kj::Promise<int> rejected = kj::Exception(...);       // Already rejected
kj::Promise<void> done = kj::READY_NOW;             // Already fulfilled void

// Chain promises with .then()
kj::Promise<kj::String> readPromise = stream->readAllText();
kj::Promise<int> countPromise = readPromise.then([](kj::String text) {
  int lines = 0;
  for (char c : text) {
    if (c == '\n') lines++;
  }
  return lines;
});

// Handle errors
kj::Promise<kj::String> safePromise = readPromise
    .then([](kj::String text) { return text; })
    .catch_([](kj::Exception&& e) {
      return kj::str("error: ", e.getDescription());
    });
```

#### Promise Chaining and Flattening

```cpp
// Promises flatten automatically (no Promise<Promise<T>>)
kj::Promise<kj::String> result = address.connect()
    .then([](kj::Own<kj::AsyncIoStream> stream) {
      // Attach stream to keep it alive during read
      return stream->readAllText().attach(kj::mv(stream));
    });
```

#### Forking Promises

```cpp
// Fork a promise for multiple consumers
kj::ForkedPromise<int> forked = promise.fork();
kj::Promise<int> branch1 = forked.addBranch();
kj::Promise<int> branch2 = forked.addBranch();
```

#### Attachments - Keep Objects Alive

```cpp
// Keep object alive until promise completes
kj::Promise<kj::String> dataPromise = readData();
dataPromise.attach(kj::mv(resourceToKeepAlive));

// Cleanup on completion (like finally block)
kj::Promise<void> promise = doWork()
    .attach(kj::defer([]() {
      // This runs when promise completes or is canceled
      cleanupResources();
    }));
```

#### Background Tasks

```cpp
// Background task that runs concurrently
kj::Promise<void> task = timer.afterDelay(5 * kj::SECONDS)
    .then([]() {
      KJ_LOG(INFO, "5 seconds elapsed");
    }).eagerlyEvaluate(nullptr);  // Error handler required

// Manage multiple background tasks
kj::TaskSet tasks([](kj::Exception&& e) {
  KJ_LOG(ERROR, "Background task failed", e);
});
tasks.add(doWork1());
tasks.add(doWork2());
```

### Timers and Timing

#### Time Types

```cpp
#include <kj/time.h>

// Duration types
kj::Duration d1 = 5 * kj::SECONDS;
kj::Duration d2 = 100 * kj::MILLISECONDS;
kj::Duration d3 = 1000 * kj::MICROSECONDS;

// Time points
kj::TimePoint now = timer.now();
kj::TimePoint future = now + d1;

// Date (calendar time)
kj::Date today = clock.now();
```

#### Timer Operations

```cpp
// Delay execution
kj::Promise<void> promise = timer.afterDelay(5 * kj::SECONDS);

// Execute at specific time
kj::Promise<void> promise = timer.atTime(targetTime);

// Timeout a promise
kj::Promise<kj::String> result = fetchData()
    .timeoutAfter(5 * kj::SECONDS, timer);
```

### Async I/O Streams

#### AsyncInputStream

```cpp
// Read data asynchronously
kj::Promise<size_t> readPromise = stream->read(buffer, minBytes, maxBytes);
kj::Promise<size_t> tryReadPromise = stream->tryRead(buffer, minBytes, maxBytes);

// Read all data
kj::Promise<kj::Array<byte>> allBytes = stream->readAllBytes(limit);
kj::Promise<kj::String> allText = stream->readAllText(limit);

// Pump data to output stream
kj::Promise<uint64_t> pumpPromise = inputStream->pumpTo(outputStream, amount);
```

#### AsyncOutputStream

```cpp
// Write data asynchronously
kj::Promise<void> writePromise = outputStream->write(buffer, size);

// Write multiple buffers
kj::Promise<kj::ArrayPtr<const kj::ArrayPtr<const byte>>> pieces = ...;
kj::Promise<void> writePromise = outputStream->write(pieces);

// Wait for disconnect
kj::Promise<void> disconnectPromise = outputStream->whenWriteDisconnected();
```

#### AsyncIoStream (Bidirectional)

```cpp
// Read and write on same stream
kj::Promise<kj::String> readPromise = ioStream->readAllText();
kj::Promise<void> writePromise = ioStream->write("hello"_kj.asBytes());

// Shutdown write end
ioStream->shutdownWrite();

// Get socket options
int value;
uint length = sizeof(value);
ioStream->getsockopt(SOL_SOCKET, SO_RCVBUF, &value, &length);
```

### Networking

#### Network Factory

```cpp
// Get network interface
kj::Network& network = ioProvider.getNetwork();

// Parse address from string
kj::Promise<Own<kj::NetworkAddress>> addrPromise =
    network.parseAddress("example.com:80");
kj::Own<kj::NetworkAddress> addr = addrPromise.wait(waitScope);

// Get local addresses
kj::Own<kj::NetworkAddress> anyAddr =
    network.getSockaddr(sockaddr_ptr, sockaddr_len);
```

#### TCP Client

```cpp
// Connect to server
kj::Promise<Own<kj::AsyncIoStream>> connectPromise = addr->connect();
kj::Own<kj::AsyncIoStream> stream = connectPromise.wait(waitScope);

// Connect with peer info
kj::Promise<kj::AuthenticatedStream> authPromise =
    addr->connectAuthenticated();
kj::AuthenticatedStream auth = authPromise.wait(waitScope);
// auth.stream, auth.peerIdentity

// Use the stream
kj::String response = stream->readAllText().wait(waitScope);
```

#### TCP Server

```cpp
// Listen on address
kj::Own<kj::ConnectionReceiver> receiver = addr->listen();

// Accept connections
kj::Promise<Own<kj::AsyncIoStream>> acceptPromise = receiver->accept();
kj::Own<kj::AsyncIoStream> client = acceptPromise.wait(waitScope);

// Accept with peer info
kj::Promise<kj::AuthenticatedStream> authPromise =
    receiver->acceptAuthenticated();
kj::AuthenticatedStream auth = authPromise.wait(waitScope);

// Get port number
uint port = receiver->getPort();
```

#### UDP / Datagram

```cpp
// Bind UDP socket
kj::Own<kj::DatagramPort> udp = addr->bindDatagramPort();

// Send datagram
kj::Promise<size_t> sendPromise =
    udp->send(buffer, size, destination);

// Receive datagrams
kj::Own<kj::DatagramReceiver> receiver = udp->makeReceiver(capacity);
receiver->receive().then([]() {
  auto content = receiver->getContent();
  auto source = receiver->getSource();
  // Process datagram
}).wait(waitScope);
```

#### Network Restrictions

```cpp
// Restrict network access
kj::Own<kj::Network> restricted = network.restrictPeers(
    {"public"},  // Allow public addresses
    {"192.168.0.0/16"}  // Deny private network
);

// Use restricted network for safety
kj::Promise<Own<kj::NetworkAddress>> addrPromise =
    restricted->parseAddress("example.com:80");
```

### Pipes and Inter-Process Communication

#### One-Way Pipe

```cpp
// Create one-way pipe
kj::OneWayPipe pipe = kj::newOneWayPipe();

// Write to output, read from input
kj::Promise<void> writePromise = pipe.out->write(data, size);
kj::Promise<kj::Array<byte>> readPromise = pipe.in->readAllBytes();
```

#### Two-Way Pipe

```cpp
// Create two-way pipe (socketpair)
kj::TwoWayPipe pipe = kj::newTwoWayPipe();

// Both ends can read and write
kj::Promise<void> write1 = pipe.ends[0]->write("hello"_kj.asBytes());
kj::Promise<kj::String> read1 = pipe.ends[1]->readAllText();
```

#### Capability Pipe (Unix Only)

```cpp
// Create pipe for passing file descriptors
kj::CapabilityPipe pipe = kj::newCapabilityPipe();

// Send/receive file descriptors
kj::Promise<AutoCloseFd> recvFd = pipe.ends[0]->receiveFd();
pipe.ends[1]->sendFd(fd);
```

### Advanced Async Patterns

#### evalNow, evalLater, evalLast

```cpp
// Execute immediately (catch exceptions as async)
kj::Promise<int> p1 = kj::evalNow([]() { return compute(); });

// Execute on next event loop turn
kj::Promise<int> p2 = kj::evalLater([]() { return compute(); });

// Execute after all other queued work (good for batching)
kj::Promise<int> p3 = kj::evalLast([]() { return compute(); });
```

#### Promise-Fulfiller Pairs

```cpp
// Create producer-consumer promise
kj::PromiseFulfillerPair<int> paf = kj::newPromiseAndFulfiller<int>();

// Consumer waits
kj::Promise<int> consumerPromise = paf.promise.then([](int value) {
  return value * 2;
});

// Producer fulfills later
paf.fulfiller->fulfill(42);
```

#### Exclusive Join

```cpp
// Wait for first of two promises to complete
kj::Promise<T> result = promise1.exclusiveJoin(promise2);
// The loser is automatically canceled
```

## Code Patterns

### Memory Allocation

```cpp
// ✅ GOOD: KJ memory management
kj::Own<MyType> ptr = kj::heap<MyType>(args...);
kj::Maybe<MyType> maybeValue;
kj::Array<int> arr = kj::heapArray<int>(10);

// ❌ BAD: Standard library equivalents
auto ptr = std::make_unique<MyType>(...);
std::optional<MyType> maybeValue;
std::vector<int> arr(10);
```

### String Handling

```cpp
// ✅ GOOD: KJ strings
kj::String greeting = "Hello, world!"_kj;
kj::StringPtr view = greeting;
kj::String message = kj::str("Hello, ", name, "!");

// ❌ BAD: Standard library strings
std::string greeting = "Hello, world!";
std::string_view view = greeting;
std::string message = "Hello, " + name + "!";
```

### Thread Synchronization

```cpp
// ✅ GOOD: KJ mutex
class ThreadSafeCounter {
    kj::MutexGuarded<uint64_t> count_;
public:
    uint64_t incrementAndGet() {
        auto lock = count_.lockExclusive();
        return ++(*lock);
    }
};

// ❌ BAD: Standard library mutex
class ThreadSafeCounter {
    mutable std::mutex mutex_;
    uint64_t count_;
public:
    uint64_t incrementAndGet() {
        std::unique_lock<std::mutex> lock(mutex_);
        return ++count_;
    }
};
```

### Nullable Values

```cpp
// ✅ GOOD: KJ Maybe
kj::Maybe<MyType> findObject();
// Usage:
KJ_IF_SOME(obj, findObject()) {
    obj->doSomething();
}

// ❌ BAD: Raw pointer or std::optional
MyType* findObject();  // Or std::optional<MyType>
// Usage:
auto obj = findObject();
if (obj) {
    obj->doSomething();
}
```

### Async Operations

```cpp
// ✅ GOOD: KJ Promise
kj::Promise<Result> fetchData() {
    return networkRequest().then([](kj::String data) {
        return parse(data);
    });
}

// ❌ BAD: std::future
std::future<Result> fetchData() {
    return std::async([&]() {
        return parse(networkRequest());
    });
}
```

### Network Client

```cpp
// ✅ GOOD: KJ networking
kj::Promise<kj::String> fetchHttp(kj::StringPtr host, uint16_t port) {
  auto addr = ioProvider.getNetwork().parseAddress(host, port);
  return addr.then([](kj::Own<kj::NetworkAddress> addr) {
    return addr.connect();
  }).then([](kj::Own<kj::AsyncIoStream> stream) {
    auto request = kj::str("GET / HTTP/1.0\r\n\r\n");
    return stream->write(request.asBytes())
        .attach(kj::mv(stream))
        .then([request](kj::Own<kj::AsyncIoStream> stream) {
      return stream->readAllText().attach(kj::mv(stream));
    });
  });
}
```

## Migration Quick Reference

### Common std → KJ Migrations

```cpp
// std::unique_ptr → kj::Own<T>
// Before: auto ptr = std::make_unique<T>(...);
// After:  kj::Own<T> ptr = kj::heap<T>(...);

// std::optional → kj::Maybe<T>
// Before: std::optional<int> opt;
// After:  kj::Maybe<int> opt;

// std::string → kj::String
// Before: std::string s = "hello";
// After:  kj::String s = "hello"_kj;

// std::vector → kj::Array or kj::Vector
// Before: std::vector<int> v(10);
// After:  kj::Array<int> v = kj::heapArray<int>(10);
// Or:     kj::Vector<int> v;

// std::mutex → kj::Mutex
// Before: std::mutex m; std::unique_lock<std::mutex> lock(m);
// After:  kj::Mutex m; kj::Mutex::Guard lock(m);

// std::function → kj::Function
// Before: std::function<void(int)> f;
// After:  kj::Function<void(int)> f;

// std::chrono::milliseconds → kj::Duration
// Before: std::chrono::milliseconds(100);
// After:  100 * kj::MILLISECONDS;

// std::async → kj::Promise
// Before: std::async([&]() { return work(); });
// After:  kj::Promise<Result> p = kj::evalLater([]() { return work(); });

// std::future → kj::Promise
// Before: std::future<int> f = ...; int r = f.get();
// After:  kj::Promise<int> p = ...; int r = p.wait(waitScope);
```

## Quick Reference Card

### Memory
- `kj::heap<T>(...)` - Allocate single object
- `kj::Own<T>` - Owned pointer with auto cleanup
- `kj::Arena` - Fast temporary allocation
- `kj::Maybe<T>` - Nullable value wrapper
- `kj::Array<T>` - Fixed-size owned array
- `kj::Vector<T>` - Dynamic array

### Synchronization
- `kj::Mutex` - Simple mutex
- `kj::MutexGuarded<T>` - Mutex-protected value wrapper
- `kj::Lazy<T>` - Lazy initialization
- `.lockExclusive()` - Acquire exclusive lock
- `.lockShared()` - Acquire shared read-only lock

### Async
- `kj::Promise<T>` - Async operation
- `.then(continuation)` - Chain callback
- `.wait(waitScope)` - Block until complete
- `.attach(object)` - Keep object alive during async
- `.eagerlyEvaluate(errorHandler)` - Force eager evaluation

### Event Loop
- `kj::setupAsyncIo()` - Setup full async I/O
- `kj::EventLoop` - Simple event loop
- `kj::WaitScope` - Synchronous wait context
- `kj::NEVER_DONE` - Promise that never completes

### Networking
- `kj::Network` - Network factory
- `kj::NetworkAddress` - Network address
- `.connect()` - TCP connect
- `.listen()` - TCP listen
- `.bindDatagramPort()` - UDP bind

### Streams
- `kj::AsyncInputStream` - Async input
- `kj::AsyncOutputStream` - Async output
- `kj::AsyncIoStream` - Async bidirectional
- `.read()` / `.write()` - Async read/write
- `.pumpTo()` - Stream transfer

### Timers
- `kj::Timer` - Timer interface
- `.afterDelay(duration)` - Delay promise
- `.atTime(time)` - Wait until time
- `.timeoutAfter(promise, duration)` - Timeout promise

### Strings
- `kj::String` - Owned string
- `kj::StringPtr` - Non-owned reference
- `"_kj` suffix for string literals
- `kj::str(...)` - String concatenation/formatting

### Functions
- `kj::Function<T>` - Store callable objects

### Exceptions
- `KJ_EXCEPTION(type, description)` - Create KJ exception
- `KJ_ASSERT(condition)` - Runtime assertion
- `KJ_REQUIRE(condition)` - Precondition check
- `KJ_THROW(exception)` - Throw KJ exception
- `KJ_DEFER(...)` - Execute on scope exit

### I/O
- `kj::InputStream` - Input stream interface
- `kj::OutputStream` - Output stream interface
- `kj::FdInputStream` - File descriptor input
- `kj::FdOutputStream` - File descriptor output

## Files to Check

- `/Users/bytedance/Develop/VeloZ/libs/core/kj/` - KJ library source code
- `/Users/bytedance/Develop/VeloZ/docs/kjdoc/tour.md` - KJ tour and concepts
- `/Users/bytedance/Develop/VeloZ/docs/kjdoc/style-guide.md` - KJ coding style guide
- `/Users/bytedance/Develop/VeloZ/docs/kj/library_usage_guide.md` - Detailed usage guide

## Notes for Claude Code

When writing C++ code for VeloZ:

1. **ALWAYS check KJ first** before considering std library types
2. **Use the decision tree** above to determine whether KJ or std is appropriate
3. **Follow the type mapping** to choose correct KJ types
4. **ONLY use std types** when KJ doesn't provide equivalent functionality or external API requires it
5. **Add comments** when using std types to explain why KJ was not used
6. **Always include `#include <kj/common.h>`** when using KJ types

Remember: **KJ is the DEFAULT choice** for VeloZ C++ code. Only use std when KJ cannot satisfy the requirement.
