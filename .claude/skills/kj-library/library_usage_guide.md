# KJ Library Usage Guide for VeloZ

Complete reference for KJ library capabilities with practical examples. KJ is the **default choice** over C++ standard library for VeloZ.

## Table of Contents

1. [Memory Management](#memory-management)
2. [Async & Promises](#async--promises)
3. [I/O Operations](#io-operations)
4. [Network Operations](#network-operations)
5. [Filesystem Operations](#filesystem-operations)
6. [Thread Synchronization](#thread-synchronization)
7. [String Handling](#string-handling)
8. [Time & Timers](#time--timers)
9. [Exception Handling](#exception-handling)
10. [Data Structures](#data-structures)
11. [Advanced Patterns](#advanced-patterns)

---

## Memory Management

### Owned Objects (`kj::Own<T>`)

KJ's unique pointer equivalent with custom disposal.

```cpp
#include <kj/memory.h>

// Basic ownership
kj::Own<MyClass> obj = kj::heap<MyClass>(arg1, arg2);

// Class member
class Container {
  kj::Own<Dependency> dep_;
public:
  Container() : dep_(kj::heap<Dependency>()) {}
};

// Move semantics
kj::Own<MyClass> ptr = kj::heap<MyClass>();
auto moved = kj::mv(ptr);  // ptr is now null
```

**Key Points:**
- Use `kj::heap<T>()` to create owned objects
- Movable, NOT copyable (like `std::unique_ptr`)
- No `.release()` method (KJ limitation)
- No custom deleters (use `std::unique_ptr` for that case)

### Nullable Values (`kj::Maybe<T>`)

KJ's optional type with pattern matching.

```cpp
#include <kj/memory.h>

// Function returning optional
kj::Maybe<int> findValue(const kj::String& key);

// Pattern matching with KJ_IF_SOME
KJ_IF_SOME(value, findValue("key")) {
  KJ_LOG(INFO, "Found: ", value);
} else {
  KJ_LOG(INFO, "Not found");
}

// Chaining matches
KJ_IF_SOME(val1, getA()) {
  KJ_IF_SOME(val2, getB()) {
    // Both present
  } else {
    // Only val1 present
  }
} else {
  // Neither present
}

// Checking presence
kj::Maybe<int> opt = 42;
if (opt == kj::none) { /* empty */ }
```

**Key Points:**
- Use `kj::none` for empty value
- `KJ_IF_SOME(variable, maybe)` extracts value if present
- Movable, NOT copyable
- Unlike `std::optional`, provides pattern matching

### Arena Allocation

Bulk allocation that frees all objects at once. Great for request/response processing.

```cpp
#include <kj/arena.h>

kj::Arena arena;

// Allocate objects
auto& obj1 = arena.allocate<MyType>(arg1, arg2);
auto& obj2 = arena.allocate<OtherType>();

// Allocate arrays
kj::ArrayPtr<int> arr = arena.allocateArray<int>(100);

// Allocate owned objects (destruct when Own<T> goes out of scope)
kj::Own<MyType> owned = arena.allocateOwn<MyType>(args...);

// Copy strings/values into arena
kj::StringPtr strCopy = arena.copyString("hello");
auto& intCopy = arena.copy(42);

// All objects freed when arena destroyed
// Arena is NOT thread-safe - use thread-local arenas if needed
```

**Key Points:**
- Allocates from contiguous chunks
- Single `free` for all objects
- Non-trivial destructors called in reverse allocation order
- Not thread-safe (use per-thread arenas)

### Refcounted Objects

For shared ownership without atomic refcounting overhead.

```cpp
#include <kj/memory.h>

class SharedResource : public kj::Refcounted {
  // Use kj::Refcounted for single-threaded sharing
public:
  void doWork();
};

// Refcounted pointer is automatically managed
kj::Own<SharedResource> create() {
  return kj::refcounted<SharedResource>();
}

// Atomic refcounting for cross-thread sharing
class ThreadSafeResource : public kj::AtomicRefcounted {
public:
  void doWork();
};

kj::Own<ThreadSafeResource> createThreadSafe() {
  return kj::atomicRefcounted<ThreadSafeResource>();
}
```

**Key Points:**
- `kj::Refcounted` - single-threaded, faster
- `kj::AtomicRefcounted` - thread-safe with atomics
- Use `kj::refcounted<T>()` / `kj::atomicRefcounted<T>()` factories
- Refcounted with Own<T> gives shared semantics

---

## Async & Promises

### Basic Promise Usage

```cpp
#include <kj/async.h>

// Create a promise
kj::Promise<int> computeAsync() {
  return kj::evalLater([]() { return 42; });
}

// Chain with .then()
computeAsync().then([](int result) {
  KJ_LOG(INFO, "Result: ", result);
  return result * 2;
}).then([](int doubled) {
  KJ_LOG(INFO, "Doubled: ", doubled);
});
```

### Promise Chaining

```cpp
// Promise returning another promise (auto-flattens)
kj::Promise<kj::String> readFileAsync(kj::StringPtr path) {
  return openFileAsync(path).then([](kj::AsyncInputStream& stream) {
    return stream.readAllText();
  });
}

// Error handling
promise.then(
  [](Result result) { return process(result); },  // Success
  [](kj::Exception&& e) {                // Error
    KJ_LOG(ERROR, "Failed: ", e.getDescription());
    return defaultValue;
  }
);

// catch_() shorthand for error-only handler
promise.catch_([](kj::Exception&& e) {
  KJ_LOG(ERROR, "Caught: ", e);
});
```

### WaitScope

For waiting on promises at the top level (e.g., main()).

```cpp
#include <kj/async-io.h>

kj::AsyncIoContext io = kj::setupAsyncIo();
kj::WaitScope& waitScope = io.waitScope;

kj::Promise<int> promise = computeAsync();

// This runs the event loop until promise resolves
int result = promise.wait(waitScope);

// Multiple promises can be waited sequentially
int r1 = promise1.wait(waitScope);
int r2 = promise2.wait(waitScope);
```

**Key Points:**
- `waitScope` proves caller is in a wait-allowed context
- Cannot wait during an event callback (recursive waits disallowed)
- Server code typically cannot use `wait()` - must chain promises

### TaskSet

Manage multiple concurrent background tasks.

```cpp
kj::TaskSet tasks([](kj::Exception&& e) {
  KJ_LOG(ERROR, "Background task failed: ", e);
});

// Add tasks - they run concurrently
tasks.add(fetchData1());
tasks.add(fetchData2());
tasks.add(fetchData3());

// Tasks continue until destructor or explicit cancellation
// Failures reported to callback above
```

### Advanced Promise Operations

```cpp
// Fork promise for multiple consumers
auto forked = promise.fork();
auto consumer1 = forked.addBranch();
auto consumer2 = forked.addBranch();

// Eager evaluation
promise.eagerlyEvaluate([](Result result) {
  // Called as soon as promise ready
});

// Separate error paths
kj::Promise<Result> promise = maybeFail()
  .then([](Result r) { return r; }, [](kj::Exception&& e) {
    // Convert error to value
    return handleError(e);
  });

// Timeout
timer.timeoutAfter(5 * kj::SECONDS, promise)
  .then([](Result r) { /* success */ })
  .catch_([](kj::Exception&& e) {
    if (e.getType() == kj::Exception::Type::OVERLOADED) {
      KJ_LOG(ERROR, "Timed out");
    }
  });
```

---

## I/O Operations

### Synchronous I/O

```cpp
#include <kj/io.h>

// Input streams
class MyInputStream : public kj::InputStream {
public:
  size_t tryRead(kj::ArrayPtr<byte> buffer, size_t minBytes) override {
    // Read at least minBytes, up to buffer.size()
    // Returns actual bytes read (< minBytes on EOF)
    return actualRead;
  }
};

// Output streams
class MyOutputStream : public kj::OutputStream {
public:
  void write(kj::ArrayPtr<const byte> data) override {
    // Always writes all data
  }
};

// Buffered I/O (reduces syscalls)
kj::BufferedInputStreamWrapper bufferedInput(input, bufferSpace);
kj::ArrayPtr<const byte> buf = bufferedInput.tryGetReadBuffer();
```

### Async I/O

```cpp
#include <kj/async-io.h>

// Async input stream
kj::Promise<size_t> readAsync(kj::AsyncInputStream& stream) {
  auto buffer = kj::heapArray<byte>(4096);
  return stream.tryRead(buffer, 0, 4096)
    .then([buffer = kj::mv(buffer)](size_t bytesRead) mutable {
      // Process bytesRead from buffer
      return bytesRead;
    });
}

// Async output stream
kj::Promise<void> writeAsync(kj::AsyncOutputStream& stream, kj::StringPtr data) {
  return stream.write(data.asBytes());
}

// Read all data
kj::Promise<kj::String> readAll(kj::AsyncInputStream& stream) {
  return stream.readAllText(1024 * 1024);  // 1MB limit
}

// Pump from input to output
kj::Promise<uint64_t> copyStream(kj::AsyncInputStream& in, kj::AsyncOutputStream& out) {
  return in.pumpTo(out);
}
```

### File Descriptors

```cpp
#include <kj/async-io.h>

// Read from file descriptor
kj::AutoCloseFd fd = open("data.txt", O_RDONLY);
kj::FdInputStream inputStream(fd);

kj::Array<byte> data = inputStream.readAllBytes();

// Write to file descriptor
kj::AutoCloseFd outFd = open("out.txt", O_WRONLY | O_CREAT);
kj::FdOutputStream outputStream(outFd);
outputStream.write("Hello"_kj.asBytes());
```

---

## Network Operations

### Setting Up Network

```cpp
#include <kj/async-io.h>

kj::AsyncIoContext io = kj::setupAsyncIo();
kj::Network& network = io.provider->getNetwork();

kj::WaitScope& waitScope = io.waitScope;
```

### TCP Client

```cpp
// Connect to server
kj::Promise<kj::Own<kj::AsyncIoStream>> connect() {
  kj::NetworkAddress addr = network.parseAddress("127.0.0.1:8080").wait(waitScope);
  return network.connect(addr);
}

connect().then([](kj::Own<kj::AsyncIoStream> connection) {
  // Send request
  return connection->write("GET / HTTP/1.1\r\n\r\n"_kj.asBytes());
}).then([connection = kj::mv(connection)]() mutable {
  // Read response
  return connection->readAllText(1024 * 1024);
}).then([](kj::String response) {
  KJ_LOG(INFO, "Response: ", response);
});
```

### TCP Server

```cpp
class EchoServer {
  kj::Network& network;
  kj::Own<kj::ConnectionReceiver> listener;
  kj::TaskSet tasks;

public:
  EchoServer(kj::Network& network) : network(network) {
    auto port = network.parseAddress("127.0.0.1:8080").wait(waitScope);
    listener = port->listen();

    // Accept connections
    tasks.add(listener->accept().then([this](kj::Own<kj::AsyncIoStream> connection) {
      handleConnection(kj::mv(connection));
      // Accept next
      handleAccept();
    }));
  }

private:
  void handleConnection(kj::Own<kj::AsyncIoStream> connection) {
    auto connPtr = connection.get();
    tasks.add(connPtr->readAllText(1024 * 1024)
      .then([connPtr](kj::String data) {
        return connPtr->write(data.asBytes());
      }).catch_([this](kj::Exception&& e) {
        KJ_LOG(INFO, "Client disconnected: ", e);
      }));
  }
};
```

### UDP (Datagrams)

```cpp
// Bind UDP socket
kj::Own<kj::DatagramPort> port = network.bindDatagramPort(
  network.parseAddress("0.0.0.0:0").wait(waitScope)
).wait(waitScope);

// Send datagram
kj::NetworkAddress dest = network.parseAddress("127.0.0.1:9000").wait(waitScope);
port->send(dest, "Hello"_kj.asBytes()).wait(waitScope);

// Receive datagram
port->receive().then([](kj::DatagramPort::Datagram datagram) {
  kj::StringPtr data(datagram.data);
  KJ_LOG(INFO, "Received from ", datagram.src.toString(true), ": ", data);
});
```

---

## Filesystem Operations

### Path Operations

KJ's `Path` class prevents path injection vulnerabilities.

```cpp
#include <kj/filesystem.h>

// Create path from components (SAFE - no injection)
kj::Path path = {"data", "subdir", "file.txt"};

// Append components
path = kj::mv(path).append("subdir2").append("file2.txt");

// Get parent/basename
kj::PathPtr parent = path.parent();
kj::PathPtr base = path.basename();

// Convert to string (for syscalls)
kj::String pathStr = path.toString();

// DANGEROUS: eval() allows ".." and "/" traversal
// Prefer append() for relative paths
kj::Path dangerous = basePath.eval("../../../etc/passwd");
// INSTEAD USE:
kj::Path safe = basePath.append(Path::parse("../../../etc/passwd"));
// This canonicalizes ".." before checking filesystem
```

**Key Points:**
- Path is immutable (operations return new path)
- `append()` is safe (no path traversal)
- `eval()` is dangerous (allows traversal)
- Components cannot contain `/`, `\0`, `.`, `..`

### File Operations

```cpp
#include <kj/filesystem.h>

kj::Filesystem& fs = kj::newDiskFilesystem();

// Open file for reading
kj::Own<kj::ReadableFile> file = fs->getRoot().openFile(
  kj::Path{"data.txt"}
);

kj::String content = file->readAllText();

// Open file for writing
kj::Own<kj::File> outFile = fs->getRoot().openFile(
  kj::Path{"output.txt"},
  kj::WriteMode::CREATE | kj::WriteMode::MODIFY
);

outFile->writeAll("Hello, KJ!"_kj);

// Open directory
kj::Own<kj::Directory> dir = fs->getRoot().openSubdir(
  kj::Path{"logs"},
  kj::WriteMode::CREATE | kj::WriteMode::CREATE_PARENT
);

// List directory entries
for (auto& entry : dir->listEntries()) {
  KJ_LOG(INFO, "Found: ", entry.name);
}

// Check existence
bool exists = fs->getRoot().tryOpenFile(kj::Path{"test.txt"}) != nullptr;
```

### In-Memory Filesystem

```cpp
// Create in-memory filesystem (great for testing)
kj::Own<kj::Filesystem> memFs = kj::newInMemoryFilesystem();

auto& root = memFs->getRoot();

// Write files
auto file = root.openFile(kj::Path{"test.txt"},
  kj::WriteMode::CREATE | kj::WriteMode::MODIFY);
file->writeAll("content"_kj);

// Read back
auto readFile = root.openFile(kj::Path{"test.txt"});
kj::String data = readFile->readAllText();
// data == "content"
```

---

## Thread Synchronization

### MutexGuarded

Thread-safe state with read/write locks.

```cpp
#include <kj/mutex.h>

class ThreadSafeCounter {
  kj::MutexGuarded<uint64_t> count_;

public:
  uint64_t incrementAndGet() {
    auto lock = count_.lockExclusive();
    return ++(*lock);
  }

  uint64_t getCount() const {
    auto lock = count_.lockShared();
    return *lock;
  }
};

// Usage
ThreadSafeCounter counter;
uint64_t val = counter.incrementAndGet();
```

### OneWayLock

Locks that can only be acquired once (for one-time initialization).

```cpp
kj::OneWayLock lock;

if (lock.tryLock()) {
  // First thread to succeed does initialization
  initialize();
  // Lock is now permanently locked
  // Other threads' tryLock() returns false
}
```

### Lazy<T>

Lazy initialization with thread safety.

```cpp
kj::Lazy<ExpensiveObject> lazyObj;

// Access initializes on first use
auto& obj = lazyObj.get();

// Subsequent accesses return same object
auto& obj2 = lazyObj.get();  // Same as obj
```

---

## String Handling

### String Types

```cpp
#include <kj/string.h>

// Non-owning view (StringPtr)
kj::StringPtr view = "text"_kj;
void process(kj::StringPtr input);  // Efficient parameter type

// Owning string (String)
kj::String owned = kj::str("Hello ", "world", "!");

// Compile-time string (ConstString)
static constexpr kj::ConstString constStr = "constant"_kjc;

// Concatenation
kj::String result = kj::str("Name: ", name, ", Age: ", age);
```

### String Literals

```cpp
// Use _kj suffix for KJ strings
kj::StringPtr view = "text"_kj;

// Use _kjc for constexpr
constexpr kj::ConstString constName = "name"_kjc;
```

### String Operations

```cpp
kj::StringPtr text = "hello world"_kj;

// Find
auto idx = text.findFirst('e');  // Maybe<size_t>
bool has = text.contains("world"_kj);
bool starts = text.startsWith("hello"_kj);

// Slice
kj::StringPtr sub = text.slice(0, 5);  // "hello"
kj::StringPtr suffix = text.slice(6);    // "world"

// Parse
int num = text.parseAs<int>();  // Throws on failure
kj::Maybe<int> maybeNum = text.tryParseAs<int>();
```

**Key Points:**
- `kj::str()` does NOT support width/precision specifiers
- Use `std::format` for `{:04d}`, `{:.2f}` etc.
- NUL-terminated, but NUL bytes allowed before end
- Non-copyable (use `std::string` for copyable structs)

---

## Time & Timers

### Time Types

```cpp
#include <kj/time.h>

// Current time
kj::TimePoint now = kj::systemPreciseMonotonicClock().now();

// Time from system
kj::Date date = kj::systemClock().now();

// Duration
kj::Duration oneSecond = 1 * kj::SECONDS;
kj::Duration fiveMinutes = 5 * kj::MINUTES;
kj::Duration twoHours = 2 * kj::HOURS;

// Arithmetic
kj::TimePoint future = now + oneSecond;
kj::Duration diff = future - now;
```

### Timer Operations

```cpp
#include <kj/timer.h>
#include <kj/async-io.h>

kj::AsyncIoContext io = kj::setupAsyncIo();
kj::Timer& timer = io.provider->getTimer();
kj::WaitScope& waitScope = io.waitScope;

// Wait until specific time
kj::Promise<void> waitUntil = timer.atTime(futureTime);
waitUntil.wait(waitScope);

// Wait for delay
kj::Promise<void> waitFiveSeconds = timer.afterDelay(5 * kj::SECONDS);
waitFiveSeconds.wait(waitScope);

// Timeout a promise
kj::Promise<int> result = computeAsync();
kj::Promise<int> withTimeout = timer.timeoutAfter(
  1 * kj::SECONDS, result
);

withTimeout.then([](int v) { KJ_LOG(INFO, "Result: ", v); })
  .catch_([](kj::Exception&& e) {
    if (e.getType() == kj::Exception::Type::OVERLOADED) {
      KJ_LOG(INFO, "Timed out!");
    }
  });
```

---

## Exception Handling

### KJ Exception Types

```cpp
#include <kj/exception.h>

// Exception types
enum class Type {
  FAILED,        // General error (KJ_ASSERT, KJ_REQUIRE)
  OVERLOADED,    // Resource/temporary issue (timeout, OOM)
  DISCONNECTED,   // Connection lost
  UNIMPLEMENTED   // Feature not available
};
```

### Assertions and Requirements

```cpp
#include <kj/debug.h>

// Debug assertion (bugs in your code)
KJ_ASSERT(value > 0, "Value must be positive", value);

// Precondition check (caller's responsibility)
KJ_REQUIRE(key != nullptr, "Key cannot be null");

// Assume (optimization hint - UB if false)
KJ_ASSUME(ptr != nullptr);

// Logging
KJ_LOG(INFO, "Processing item ", id);
KJ_LOG(WARNING, "Deprecated API used");
KJ_LOG(ERROR, "Failed to connect");
KJ_LOG(FATAL, "Critical error");  // Terminates

// Debug-only logging (delete before commit)
KJ_DBG("Checking variable ", x);
```

### Exception Context

```cpp
#include <kj/exception.h>

KJ_REQUIRE(condition, "Message") {
  // This block runs if condition fails
  // Can add recovery code
  fixState();
  return errorValue;
}

// Wrap exceptions with context
KJ_TRY {
  riskyOperation();
} KJ_CATCH(e) {
  e.wrapContext(__FILE__, __LINE__, "In processData");
  throw;
}
```

### System Calls

```cpp
#include <kj/debug.h>

// System call wrapper (handles EINTR)
int fd;
KJ_SYSCALL(fd = open("file.txt", O_RDONLY), "file.txt");

// Non-blocking syscall (handles EAGAIN/EWOULDBLOCK)
int bytes = KJ_NONBLOCKING_SYSCALL(read(fd, buf, size), fd);

// File descriptor wrapper
kj::AutoCloseFd fd = KJ_SYSCALL_FD(open("file.txt", O_RDONLY), "file.txt");
```

---

## Data Structures

### Vector<T>

```cpp
#include <kj/vector.h>

kj::Vector<int> vec;
vec.add(1);
vec.add(2);
vec.add(3);

// Access
int first = vec[0];
int last = vec.back();

// Convert to array
kj::Array<int> arr = vec.releaseAsArray();

// Reserve capacity
vec.reserve(100);

// Add all from another container
kj::Vector<int> other;
vec.addAll(other);
```

### Array<T>

```cpp
#include <kj/array.h>

// Fixed-size array
kj::Array<int> arr = kj::heapArray<int>(10);
arr[0] = 42;

// ArrayPtr (non-owning view)
kj::ArrayPtr<int> view = arr.slice(0, 5);

// Helpers
kj::ArrayPtr<const byte> bytes = "hello"_kj.asBytes();
```

### Map<T, U>

```cpp
#include <kj/map.h>

kj::Map<kj::StringPtr, int> map;

map.insert("key1"_kj, 1);
map.insert("key2"_kj, 2);

// Lookup
kj::Maybe<int&> val = map.find("key1"_kj);
KJ_IF_SOME(v, val) {
  KJ_LOG(INFO, "Found: ", *v);
}

// Iterate
for (auto& entry : map) {
  KJ_LOG(INFO, entry.key, " = ", entry.value);
}
```

### StringTree

Efficient string concatenation for building complex output.

```cpp
#include <kj/string-tree.h>

kj::StringTree tree = kj::strTree(
  "Name: ", name, "\n",
  "Age: ", age, "\n",
  "Items: ", kj::StringTree(array), "\n"
);

kj::String result = tree.flatten();
```

---

## Advanced Patterns

### Event Loop Patterns

```cpp
// Main event loop
kj::AsyncIoContext io = kj::setupAsyncIo();
kj::WaitScope waitScope(io.waitScope);

// Background tasks
kj::TaskSet backgroundTasks([](kj::Exception&& e) {
  KJ_LOG(ERROR, "Task failed: ", e);
});

// Add tasks that run forever
backgroundTasks.add(monitorResources());
backgroundTasks.add(processQueue());

// Block forever (or until explicitly stopped)
kj::NEVER_DONE.wait(waitScope);
```

### Async Coroutine Support (C++20)

```cpp
#include <kj/async-coroutine-alloc.h>

// Use co_await with KJ promises
kj::Promise<kj::String> fetchWithCoroutine() {
  kj::String result = co_await readFileAsync("data.txt");
  co_return kj::str("Fetched: ", result);
}
```

### Defer / Scope Guards

```cpp
#include <kj/common.h>

void process() {
  auto cleanup = kj::defer([]() {
    // Runs when cleanup goes out of scope
    KJ_LOG(INFO, "Cleaning up");
  });

  // Do work
  // cleanup runs automatically

  // Or use KJ_DEFER macro
  KJ_DEFER({
    KJ_LOG(INFO, "Macro-style cleanup");
  });
}
```

### Move Semantics

```cpp
#include <kj/common.h>

kj::Own<MyType> ptr = kj::heap<MyType>();

// Move to another variable
auto moved = kj::mv(ptr);

// Pass to function
void accept(kj::Own<MyType> p);
accept(kj::mv(ptr));  // ptr is now null

// Return owned
kj::Own<MyType> create() {
  return kj::heap<MyType>();
}
```

---

## Self-Learning from KJ Tests

### Why Learn from Tests?

The KJ test files are the authoritative source for:
- Real-world usage patterns
- Edge cases and error handling
- API idioms and best practices
- Performance patterns

### Test File Location

All KJ tests are in: `libs/core/kj/*-test.c++`

### Learning Workflow

When you encounter an unfamiliar KJ pattern or need to understand an API:

1. **Identify the feature** you're working with (e.g., `kj::Promise::fork()`)
2. **Find the test file** (e.g., `async-test.c++` for promise operations)
3. **Search for usage patterns:**
   ```bash
   # Find fork() usage in async tests
   grep -B 5 -A 20 "\.fork()" libs/core/kj/async-test.c++
   ```
4. **Read test cases** to understand:
   - How the API is used correctly
   - What edge cases exist
   - How errors are handled
   - What patterns are idiomatic
5. **Extract and apply** the patterns to your code
6. **Update this skill** if you discover important new patterns

### Test File Reference by Feature

| Feature | Test File | What You'll Learn |
|---------|-----------|------------------|
| `kj::Own<T>`, `kj::Maybe<T>` | `memory-test.c++` | Ownership, null checking, move semantics |
| `kj::Promise<T>` | `async-test.c++` | Promise chains, error handling, forking, eager evaluation |
| `kj::TaskSet` | `async-test.c++` | Background task management, error handling |
| `kj::AsyncIoContext` | `async-io-test.c++` | Event loops, async I/O setup |
| Network (TCP/UDP) | `async-io-test.c++` | Client/server patterns, connection handling |
| `kj::Path`, filesystem | `filesystem-test.c++`, `filesystem-disk-test.c++` | Path safety, file operations, directory traversal |
| `kj::MutexGuarded<T>` | `mutex-test.c++` | Exclusive/shared locks, wait conditions |
| `kj::Arena` | `arena-test.c++` | Bulk allocation, object lifetime |
| `kj::String`, `kj::StringPtr` | `string-test.c++` | String construction, slicing, parsing |
| `kj::Timer`, timeouts | `timer-test.c++` | Delays, timeouts, time-based operations |
| `kj::Function<T>` | `function-test.c++` | Functors, method binding with `KJ_BIND_METHOD` |
| `kj::Array<T>`, `kj::Vector<T>` | `array-test.c++` | Fixed/dynamic arrays, array builders |
| `kj::Map<K,V>` | `map-test.c++` | Map operations, iteration |
| `kj::Exception`, assertions | `exception-test.c++`, `debug-test.c++` | Error handling, assertions, system calls |
| `kj::Refcounted` | `refcount-test.c++` | Shared ownership patterns |

### Example: Learning Promise Forking

Suppose you need to understand how to use `kj::Promise::fork()`:

```bash
# Search for fork usage in async tests
grep -B 5 -A 15 "\.fork()" libs/core/kj/async-test.c++
```

You might find:
```cpp
TEST(Async, Fork) {
  Promise<int> promise = evalLater([]() { return 123; });
  auto fork = promise.fork();

  auto branch1 = fork.addBranch();
  auto branch2 = fork.addBranch();
  auto branch3 = fork.addBranch();

  EXPECT_EQ(123, branch1.wait(waitScope));
  EXPECT_EQ(123, branch2.wait(waitScope));
  EXPECT_EQ(123, branch3.wait(waitScope));
}
```

From this test you learn:
- Call `.fork()` on a promise to create a `ForkedPromise`
- Use `.addBranch()` to create multiple consumers
- Each branch gets the same value
- Original promise can only be forked once

### Example: Learning Arena Allocation

```bash
# Search for arena usage
grep -B 3 -A 10 "arena\.allocate" libs/core/kj/arena-test.c++
```

You find:
```cpp
TEST(Arena, Basic) {
  Arena arena;

  int& i = arena.allocate<int>();
  i = 123;

  String& s = arena.copyString("foo");

  // Objects destroyed when arena destroyed
}
```

Learning:
- Use `arena.allocate<T>(args...)` for objects
- Use `arena.copyString()` to copy strings into arena
- All objects freed when arena goes out of scope

### Example: Learning MutexGuarded Wait Conditions

```bash
# Search for wait() usage with mutex
grep -B 5 -A 15 "wait\(.*predicate" libs/core/kj/mutex-test.c++
```

You discover:
```cpp
TEST(Mutex, Wait) {
  MutexGuarded<int> value(0);

  auto lock = value.lockExclusive();

  // Wait until condition met
  lock.wait([](int v) { return v > 0; });

  // lock is still held after wait
}
```

Learning:
- Use `lock.wait(predicate)` to wait for condition
- Lock is automatically released during wait, re-acquired after
- Predicate checks value while lock is held

### Updating This Skill

If you discover important patterns or best practices from tests that aren't documented here:

1. **Add the pattern** to the appropriate section in this guide
2. **Include a code example** showing the pattern
3. **Note the test file** where you found it (for future reference)
4. **Keep it concise** - this file should remain scannable

---

## Common Pitfalls

### 1. Forgetting `_kj` Suffix

```cpp
// WRONG
kj::StringPtr str = "text";  // Works but inefficient

// RIGHT
kj::StringPtr str = "text"_kj;  // Compile-time known size
```

### 2. Using std Types Out of Habit

```cpp
// WRONG
std::vector<int> vec;
std::optional<int> opt;

// RIGHT
kj::Vector<int> vec;
kj::Maybe<int> opt;
```

### 3. KJ String Limitations

```cpp
// WRONG - kj::str() doesn't support format specifiers
kj::String s = kj::str("Value: ", value, ", padded: ", kj::padLeft(num, ' ', 4));

// RIGHT - use std::format for complex formatting
#include <format>
std::string s = std::format("Value: {}, padded: {:04d}", value, num);
```

### 4. Copyable Structs with KJ Strings

```cpp
// WRONG - kj::String is not copyable
struct Config {
  kj::String name;  // Makes struct non-copyable
};

// RIGHT - use std::string for copyable structs
struct Config {
  std::string name;
};
```

### 5. Recursive Lambdas with kj::Function

```cpp
// WRONG - kj::Function is not copyable
kj::Function<void()> recurse = [&]() {
  // Error: cannot capture recursive non-copyable lambda
};

// RIGHT - use std::function
std::function<void()> recurse = [&]() {
  // This works
};
```

### 6. Custom Deleters

```cpp
// WRONG - kj::Own doesn't support custom deleters
auto ptr = kj::Own<FILE>(fopen("file.txt", "r"), customDeleter);

// RIGHT - use std::unique_ptr
std::unique_ptr<FILE, decltype(&fclose)> ptr(fopen("file.txt", "r"), fclose);
```

---

## Migration Guide: std → KJ

| std Pattern | KJ Replacement | Notes |
|-----------|---------------|-------|
| `std::unique_ptr<T>` | `kj::Own<T>` + `kj::heap<T>()` | No custom deleters |
| `std::optional<T>` | `kj::Maybe<T>` | Use `KJ_IF_SOME` for pattern matching |
| `std::string` | `kj::String` / `kj::StringPtr` | Use `_kj` literals |
| `std::vector<T>` | `kj::Vector<T>` | No iterator-based algorithms |
| `std::map<K,V>` | `kj::Map<K,V>` | No ordered iteration |
| `std::function<T>` | `kj::Function<T>` | Non-copyable |
| `std::mutex` | `kj::MutexGuarded<T>` | Supports read/write locks |
| `std::future<T>` | `kj::Promise<T>` | Use `.then()` chaining |
| `std::shared_ptr<T>` | `kj::Own<Refcounted<T>>` | Or `kj::Refcounted`/`kj::AtomicRefcounted` |

---

## Summary

**Always prefer KJ first.** Use std only when:
1. KJ does not provide equivalent functionality
2. External API compatibility requires std types
3. Third-party library integration requires std types

For quick reference, see the `SKILL.md` decision flowchart and type mapping table.
