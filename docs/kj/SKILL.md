---
name: "kj-library"
description: "Guides KJ library usage in VeloZ. Invoke when writing or reviewing C++ code and choosing between KJ and std."
---

# KJ Library Skill for Claude Code

## Description

This skill provides guidance for Claude Code (claude.ai/code) to recognize and use the KJ library from Cap'n Proto when working on the VeloZ codebase. **KJ is the DEFAULT choice over C++ standard library for all applicable use cases.**

## KJ Library Overview (Latest Version)

KJ is a utility library from Cap'n Proto that provides:

- **Memory management**: Arena, Own<T>, Maybe<T>, Array<T>, Vector<T>, Refcounted, AtomicRefcounted
- **Thread synchronization**: Mutex, MutexGuarded<T>, Lazy<T>, OneWayLock
- **Async I/O**: Promises, AsyncIoContext, AsyncIoStream, Network, TaskSet
- **String handling**: String, StringPtr, ConstString, StringTree, LiteralStringConst
- **Exceptions**: Exception, assertions, system call wrappers
- **Functions**: Function<T> for functors/lambdas
- **Time**: TimePoint, Duration, Timer, Date
- **I/O**: InputStream, OutputStream, AsyncIoStream, FdInputStream, FdOutputStream
- **Networking**: Network, NetworkAddress, ConnectionReceiver, DatagramPort
- **Async objects**: AsyncObject, AllowAsyncDestructorsScope, DisallowAsyncDestructorsScope
- **Filesystem**: Path, File, Directory, ReadableFile, WritableFile, InMemoryDirectory
- **Source locations**: SourceLocation, NoopSourceLocation
- **Coroutines**: async coroutine support for C++20

**System Requirements**:
- C++20 or later (VeloZ uses C++23)
- Clang 14.0+ or GCC 14.3+
- Requires exceptions enabled

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
| `std::atomic_shared_ptr<T>` | `kj::AtomicRefcounted<T>` | Thread-safe refcounted objects |
| `std::optional<T>` | `kj::Maybe<T>` | Use `KJ_IF_SOME` macro |
| `std::vector<T>` | `kj::Array<T>` (fixed) or `kj::Vector<T>` (dynamic) | KJ arrays don't provide iterator semantics |
| `std::make_shared` | `kj::atomicRefcounted<T>()` | For reference-counted objects |
| `std::make_unique` | `kj::heap<T>()` | KJ allocation function |
| `std::make_shared_for_overwrite` | `kj::refcounted<T>()` | Placement-style construction |

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
| `std::string` | `kj::String` or `kj::ConstString` | KJ strings don't invalidate on move |
| `std::string_view` | `kj::StringPtr` | KJ view is safer, doesn't invalidate |
| `"literal"` | `"literal"_kj` | Use `_kj` suffix for KJ string literals |
| `"literal"` (constexpr) | `"literal"_kjc` | For constexpr `StringPtr` or `LiteralStringConst` |
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
| `std::logic_error` | `kj::Exception` with type | Use `KJ_EXCEPTION(Type::FAILED)` |
| `try/catch` | Same syntax | KJ exceptions are standard C++ exceptions |

### I/O

| Std Type | KJ Equivalent | Notes |
|----------|---------------|-------|
| `std::istream` | `kj::InputStream` | KJ input stream |
| `std::ostream` | `kj::OutputStream` | KJ output stream |
| `std::ifstream` | `kj::FdInputStream` | KJ file descriptor input |
| `std::ofstream` | `kj::FdOutputStream` | KJ file descriptor output |
| `std::filesystem` | `kj::Path`, `kj::Directory`, `kj::File` | KJ filesystem API |

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

### Source Location

| Std Type | KJ Equivalent | Notes |
|----------|---------------|-------|
| `std::source_location` | `kj::SourceLocation` | KJ source location (file, line, column, function) |
| N/A | `kj::NoopSourceLocation` | Noop source location for disabled tracking |

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

## String Handling with kj::str (COMPREHENSIVE GUIDE)

### String Literals

KJ provides two user-defined literal suffixes for creating string literals:

```cpp
// KJ StringPtr literal (runtime, NUL-safe)
kj::StringPtr str1 = "hello"_kj;

// KJ ConstString literal (constexpr, NUL-safe)
kj::ConstString str2 = "world"_kjc;
```

**Important notes:**
- `_kj` suffix creates a `kj::StringPtr` for runtime use
- `_kjc` suffix creates a `kj::LiteralStringConst` for constexpr
- KJ literals are NUL-safe: they preserve NUL bytes in the middle of the string
- Use `_kj` when you need a constexpr `StringPtr` or to avoid global constructor code

### kj::str() - Template String Function

`kj::str()` is KJ's powerful string template function that concatenates and converts multiple values into a single `kj::String`:

```cpp
#include <kj/string.h>

// Basic concatenation
kj::String greeting = kj::str("Hello, ", name, "!");  // "Hello, Bob!"

// Mixing types
kj::String msg = kj::str("Order #", orderId, " total: ", price);

// With numbers (automatic conversion)
kj::String status = kj::str("Progress: ", percent, "%");

// Nested str() calls work
kj::String formatted = kj::str("File: ", kj::str(path, " exists"));
```

### Stringification Rules

`kj::str()` automatically stringifies any type that has a defined "stringifier". Built-in types supported:

```cpp
// Numbers (all built-in integer and float types)
kj::String s1 = kj::str(42);           // "42"
kj::String s2 = kj::str(3.14159);       // "3.14159"
kj::String s3 = kj::str(-123);         // "-123"

// Booleans
kj::String s4 = kj::str(true);          // "true"
kj::String s5 = kj::str(false);         // "false"

// Pointers
kj::String s6 = kj::str((void*)0x1234); // "0x1234"

// Characters and bytes
kj::String s7 = kj::str('A');           // "A"
kj::String s8 = kj::str((unsigned char)255); // "ff"

// KJ strings
kj::String s9 = kj::str("hello"_kj);
kj::StringPtr ptr = "world"_kj;
kj::String s10 = kj::str(ptr); // "world"

// Arrays (elements delimited by ", ")
kj::Vector<int> nums = {1, 2, 3};
kj::String s11 = kj::str(nums); // "1, 2, 3"

// Custom stringifiable types (define KJ_STRINGIFY)
// See section below for custom type stringification
```

### String Methods and Operations

```cpp
kj::String text = "Hello, World!"_kj;

// Comparison operators
if (text == "Hello"_kj) { /* ... */ }
if (text != "Goodbye"_kj) { /* ... */ }
if (text < "Apple"_kj) { /* ... */ }

// Searching
if (text.startsWith("Hello"_kj)) { /* ... */ }
if (text.endsWith("World!"_kj)) { /* ... */ }
bool has = text.contains("lo"_kj);  // true

// Find character
KJ_IF_SOME(pos, text.findFirst('o')) {
  // Found at position pos
}

// Find string
KJ_IF_SOME(pos, text.find("World"_kj)) {
  // Found at position pos
}

// Slicing
kj::StringPtr prefix = text.slice(0, 5);  // "Hello"
kj::StringPtr suffix = text.slice(7);     // "World!"
kj::StringPtr first4 = text.first(4);      // "Hell"

// Get raw C string
const char* cstr = text.cStr();
size_t len = text.size();  // length without NUL terminator
```

### Array Stringification

```cpp
// Arrays can be stringified with ", " delimiter
kj::Array<int> arr = {1, 2, 3};
kj::String joined = kj::str(arr);  // "1, 2, 3"

// Use kj::delimited() for custom delimiter
kj::Vector<kj::String> parts = {"a", "b", "c"};
kj::String custom = kj::str(kj::delimited(parts, " | ")); // "a | b | c"
```

### Custom Type Stringification

Define stringifier for custom types using `KJ_STRINGIFY` macro:

```cpp
class Order {
  kj::String id;
  double total;

  // Define stringifier (must be in global or same namespace as type)
  inline kj::StringPtr KJ_STRINGIFY(const Order& order) {
    return order.id;  // or return str(order.id, ":", order.total);
  }
};

// Now can use with kj::str()
Order order;
kj::String msg = kj::str("Order: ", order);
```

### Maybe Stringification

```cpp
kj::Maybe<int> maybeValue = kj::maybe(42);
kj::Maybe<kj::String> maybeStr = "hello"_kj;

// KJ_IF_SOME handles stringification automatically
KJ_IF_SOME(val, maybeValue) {
  // val is int, not Maybe - kj::str(val) would work
}

// kj::str() handles Maybe automatically
kj::String result = kj::str("Value: ", maybeValue);
// If value is present: "Value: 42"
// If value is none: "Value: (none)"
```

### Preallocated String Building

For signal-safe or performance-critical code:

```cpp
char buffer[256];
kj::StringPtr text = kj::strPreallocated(buffer, "Critical: ", error);
// If buffer too small, text is truncated but always NUL-terminated
```

## Exception Handling (COMPREHENSIVE GUIDE)

### kj::Exception - KJ Exception Base Class

`kj::Exception` is KJ's exception class with rich context information:

```cpp
#include <kj/exception.h>

// Exception types
kj::Exception::Type type = kj::Exception::Type::FAILED;      // General failure
kj::Exception::Type type = kj::Exception::Type::OVERLOADED;  // Resource/temporary lack
kj::Exception::Type type = kj::Exception::Type::DISCONNECTED; // Connection lost
kj::Exception::Type type = kj::Exception::Type::UNIMPLEMENTED; // Method not implemented

// Creating exceptions
kj::Exception ex(kj::Exception::Type::FAILED,
                 __FILE__, __LINE__,
                 kj::str("Something went wrong"));
```

### Exception Macros

KJ provides macros for common exception scenarios:

```cpp
// Assertions (runtime checks that can be disabled in release)
KJ_ASSERT(condition, "context: ", value);       // Fatal if false in debug
KJ_ASSERT_NONNULL(ptr);                        // Fatal if ptr is nullptr

// Preconditions (always checked)
KJ_REQUIRE(condition, "invalid parameter: ", param);  // Throws FAILED if false
KJ_REQUIRE_NONNULL(ptr);                         // Throws FAILED if nullptr

// Throw exception directly
KJ_THROW(kj::Exception(kj::Exception::Type::FAILED,
                       __FILE__, __LINE__,
                       kj::str("Operation failed")));
```

### Exception Context and Stack Traces

```cpp
// Get exception information
try {
  // code that may throw
} catch (const kj::Exception& e) {
  // Get exception details
  const char* file = e.getFile();
  int line = e.getLine();
  kj::StringPtr desc = e.getDescription();
  kj::Exception::Type type = e.getType();

  // Get stack trace
  auto trace = e.getStackTrace();

  KJ_LOG(ERROR, "Exception: ", desc,
            " at ", file, ":", line);
}

// Add context to current exception
kj::Exception::addTraceHere();

// Get exception type as string
kj::StringPtr typeStr = kj::getCaughtExceptionType();
```

### Exception Callbacks

For custom exception handling:

```cpp
// Set exception callback
class MyCallback : public kj::ExceptionCallback {
  void onFatalException(kj::Exception&& exception) override {
    // Custom fatal exception handling
  }
  void onRecoverableException(kj::Exception&& exception) override {
    // Custom recoverable exception handling
  }
};

// Register callback (only at program startup)
// Note: This is advanced usage; most code should use try/catch
```

### Logging with Exceptions

```cpp
// Log levels (see Log Severity section)
enum class LogSeverity {
  INFO,    // Information
  WARNING, // Problem detected but execution can continue
  ERROR,   // Something wrong but execution can continue with garbage output
  FATAL,   // Cannot continue
};

// Log messages with exceptions
KJ_LOG(ERROR, "Failed to process order: ", orderId);

// The log severity can be configured
// See kj/exception.h for stack trace mode settings
```

### KJ_TRY / KJ_CATCH Macros

KJ provides safer exception handling macros:

```cpp
// KJ_TRY / KJ_CATCH automatically coerce exceptions to kj::Exception
KJ_TRY {
  // code that may throw
} KJ_CATCH(e) {
  // e is kj::Exception&
  KJ_LOG(ERROR, "Caught: ", e.getDescription());
}

// You can use KJ_CATCH with any variable name
// Use _ to suppress unused variable warning
KJ_TRY {
  riskyOperation();
} KJ_CATCH(_) {
  // Ignoring exception
}
```

## File System Operations (COMPREHENSIVE GUIDE)

### kj::Path - Type-Safe Path Handling

`kj::Path` represents filesystem paths as arrays of components, preventing path injection bugs:

```cpp
#include <kj/filesystem.h>

// Creating paths from components
kj::Path path1{"foo", "bar", "baz"};  // foo/bar/baz

// Single component path
kj::Path path2 = kj::Path("filename.txt");

// Parse traditional path (with safety checks)
kj::Path path3 = kj::Path::parse("data/config.json");
// Note: Path::parse() canonicalizes "." and ".." components

// Path operations
kj::Path parent = path.parent();                    // Get parent
kj::Path base = path.basename();                    // Get filename
kj::Path appended = path.append("subdir");           // Append component
kj::Path evaluated = path.eval("../config");        // Evaluate relative path (BE CAREFUL!)

// ⚠️ WARNING: eval() is dangerous!
// Use append() instead when possible. eval() can escape current directory.

// Convert to string (for display or system calls)
kj::String strPath = path.toString();          // Relative path string
kj::String absPath = path.toString(true);     // Absolute path string

// Platform-specific strings
kj::String nativePath = path.toNativeString();  // Native format for current platform
```

### Reading Files

```cpp
#include <kj/filesystem.h>

// Get filesystem
kj::Own<kj::Filesystem> fs = kj::newDiskFilesystem();
const kj::Directory& root = fs->getRoot();

// Open a file for reading
kj::Own<const kj::File> file = root.openFile(kj::Path{"data", "input.txt"});

// Read all text
kj::String content = file->readAllText();

// Read all bytes
kj::Array<byte> data = file->readAllBytes();

// Read at offset with buffer
char buffer[1024];
size_t bytesRead = file->read(0, kj::arrayPtr(buffer));

// Memory map file (read-only, efficient for large files)
kj::Array<const byte> mapped = file->mmap(0, fileSize);
```

### Writing Files

```cpp
// Open file for writing
auto dir = fs->getRoot().openSubdir(kj::Path{"data"});

// Write modes (bitflags)
kj::WriteMode mode = kj::WriteMode::CREATE | kj::WriteMode::MODIFY;

// Create/overwrite file
kj::Own<const kj::File> file = dir->openFile(kj::Path{"output.txt"}, mode);

// Write all content
file->writeAll(kj::arrayPtr(data));
file->writeAll("Hello, World!"_kj);

// Write at offset
file->write(100, kj::arrayPtr(someData));

// Truncate file
file->truncate(2048);

// Zero a range (create sparse file)
file->zero(1024, 4096);
```

### Directory Operations

```cpp
// Open directory
kj::Own<const kj::Directory> dir = fs->getRoot().openSubdir(kj::Path{"data"});

// List directory contents (names only)
kj::Array<kj::String> names = dir->listNames();

// List with metadata
kj::Array<kj::ReadableDirectory::Entry> entries = dir->listEntries();
for (auto& entry : entries) {
  kj::StringPtr name = entry.name;
  kj::FsNode::Type type = entry.type;
  // FILE, DIRECTORY, SYMLINK, BLOCK_DEVICE, etc.
}

// Check if file exists
bool exists = dir->exists(kj::Path{"file.txt"});

// Get metadata
KJ_IF_SOME(meta, dir->tryLstat(kj::Path{"file.txt"})) {
  uint64_t size = meta.size;
  kj::Date modified = meta.lastModified;
  uint linkCount = meta.linkCount;
}

// Create subdirectory
auto subdir = dir->openSubdir(kj::Path{"newdir"},
                       kj::WriteMode::CREATE | kj::WriteMode::CREATE_PARENT);

// Remove file/directory (recursive)
dir->remove(kj::Path{"old_data"});

// Transfer (move/copy/link)
dir->transfer(kj::Path{"dest"},
             kj::WriteMode::CREATE | kj::WriteMode::CREATE_PARENT,
             kj::Path{"src"},
             kj::TransferMode::MOVE);
```

### Atomic File Replacement

```cpp
// Replace file atomically (safe for crash/power failure)
auto replacer = dir->replaceFile(kj::Path{"important.txt"},
                                    kj::WriteMode::CREATE | kj::WriteMode::MODIFY);

// Write to temporary file
replacer->get().writeAll("new content");

// Commit atomically (temporary moved to final location)
replacer->commit();
// If destroy without commit(), temporary is deleted
```

### Appendable Files

```cpp
// Open file for appending (good for logs)
kj::Own<kj::AppendableFile> logFile = dir->appendFile(
    kj::Path{"app.log"},
    kj::WriteMode::CREATE | kj::WriteMode::CREATE_PARENT);

// Append data (inherits OutputStream interface)
kj::String entry = kj::str("[", timestamp, "] ", message, "\n");
logFile->write(entry.asBytes());
```

### In-Memory Filesystem

For testing or sandboxed operations:

```cpp
// Create in-memory filesystem
kj::Own<kj::Directory> memDir = kj::newInMemoryDirectory(kj::systemClock());

// Works like real filesystem
kj::Own<const kj::File> memFile = memDir->openFile(
    kj::Path{"test.txt"}, kj::WriteMode::CREATE);

memFile->writeAll("test content"_kj);
kj::String readBack = memFile->readAllText();  // "test content"

// Linux-specific: memfd files (real memory files with FDs)
#if __linux__
kj::Own<kj::File> memfd = kj::newMemfdFile();
#endif
```

### Symlinks

```cpp
// Create symlink
dir->symlink(kj::Path{"link"},
             "target_file"_kj,
             kj::WriteMode::CREATE);

// Read symlink target
kj::String target = dir->readlink(kj::Path{"link"});

// Use tryReadlink for safe handling
KJ_IF_SOME(t, dir->tryReadlink(kj::Path{"link"})) {
  // Target is t
}
```

## Source Location Tracking (COMPREHENSIVE GUIDE)

### kj::SourceLocation - Compile-Time Source Information

`kj::SourceLocation` provides compile-time capture of source code location:

```cpp
#include <kj/source-location.h>

// Get current source location automatically
class MyClass {
public:
  void myMethod() {
    kj::SourceLocation loc;  // Captured at point of declaration

    // Access location info
    const char* file = loc.fileName;
    const char* func = loc.function;
    uint line = loc.lineNumber;
    uint col = loc.columnNumber;
  }
};
```

**Important Notes:**
- `kj::SourceLocation` uses compiler built-ins (`__builtin_FILE`, `__builtin_FUNCTION`, `__builtin_LINE`, `__builtin_COLUMN`)
- Column number may be 0 on compilers that don't support `__builtin_COLUMN()`
- Location is captured at declaration point, not at runtime call site

### Comparing Source Locations

```cpp
kj::SourceLocation loc1;
kj::SourceLocation loc2;

if (loc1 == loc2) {
  // Same location
}
```

### Stringifying Source Locations

```cpp
kj::SourceLocation loc;
kj::String locStr = KJ_STRINGIFY(loc);
// Produces format like: "file.cpp:123" (line)
```

### NoopSourceLocation

For code that needs to optionally track locations:

```cpp
kj::NoopSourceLocation noLoc;  // Has minimal/no operations

// Use for zero-overhead when location tracking is disabled
```

### Source Location with Exceptions

```cpp
// Include location in exception
throw kj::Exception(kj::Exception::Type::FAILED,
                       __FILE__, __LINE__,
                       "description"_kj);

// Or use macros that include location automatically
KJ_REQUIRE(condition, "Bad value: ", value);  // Includes file/line
KJ_ASSERT(condition, "Invariant failed");        // Includes file/line in debug
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

#### Async Object Lifecycle Management

```cpp
// Mark async objects for debug validation
class MyAsyncObject : public kj::AsyncObject {
  // Derive from AsyncObject to enable debug checks for
  // objects tied to specific event loops/threads
};

// Prevent async destruction during critical sections
{
  kj::DisallowAsyncDestructorsScope scope("Critical section");
  // If any AsyncObject is destroyed here, process terminates
}  // Scope end

// Override disallow scope when needed
{
  kj::DisallowAsyncDestructorsScope outer("Outer");
  {
    kj::AllowAsyncDestructorsScope inner;
    // Async destruction is OK in this scope
  }
}
```

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

#### Coroutines (C++20)

```cpp
// KJ supports co_await via coroutines
kj::Promise<kj::String> fetchAsync() {
  auto addr = co_await network.parseAddress("example.com:80"_kj);
  auto stream = co_await addr.connect();
  auto response = co_await stream.readAllText().attach(kj::mv(stream));
  co_return response;
}

// Use coroutine-based allocation for better performance
kj::CoroutineAllocator allocator;
kj::Promise<void> task = coMyTask(allocator);
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

### File Operations

```cpp
// ✅ GOOD: KJ filesystem
auto fs = kj::newDiskFilesystem();
auto file = fs->getRoot().openFile(kj::Path{"data.txt"}, kj::WriteMode::CREATE);
kj::String content = file->readAllText();

// ❌ BAD: std::filesystem
std::ifstream file("data.txt");
std::string content((std::istreambuf_iterator<char>(file), {});
```

### Source Location

```cpp
// ✅ GOOD: KJ SourceLocation
kj::SourceLocation loc;
const char* file = loc.fileName;
int line = loc.lineNumber;

// ❌ BAD: std::source_location
auto loc = std::source_location::current();
```

### Exception Handling

```cpp
// ✅ GOOD: KJ exceptions
KJ_REQUIRE(condition, "Invalid state");
KJ_ASSERT(value > 0, "Value must be positive");
KJ_THROW(kj::Exception(kj::Exception::Type::FAILED,
                       __FILE__, __LINE__,
                       "Operation failed"));

// ❌ BAD: std exceptions
throw std::runtime_error("Operation failed");
assert(condition);  // Use KJ_ASSERT instead
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

// std::filesystem → kj::Path/File/Directory
// Before: filesystem path p("file.txt");
// After:  kj::Path p = kj::Path{"file.txt"};

// std::source_location → kj::SourceLocation
// Before: auto loc = std::source_location::current();
// After:  kj::SourceLocation loc;

// std::exception → kj::Exception
// Before: throw std::runtime_error("msg");
// After:  KJ_THROW(kj::Exception(kj::Exception::Type::FAILED, ...));

// std::stringstream/std::format → kj::str()
// Before: std::stringstream ss; ss << a << b; std::string s = ss.str();
// After:  kj::String s = kj::str(a, b);
```

## Quick Reference Card

### Memory
- `kj::heap<T>(...)` - Allocate single object
- `kj::Own<T>` - Owned pointer with auto cleanup
- `kj::Arena` - Fast temporary allocation
- `kj::Maybe<T>` - Nullable value wrapper
- `kj::Array<T>` - Fixed-size owned array
- `kj::Vector<T>` - Dynamic array
- `kj::Refcounted<T>` - Reference-counted base class
- `kj::AtomicRefcounted<T>` - Thread-safe refcounted objects

### Synchronization
- `kj::Mutex` - Simple mutex with read/write locks
- `kj::MutexGuarded<T>` - Mutex-protected value wrapper
- `kj::Lazy<T>` - Lazy initialization
- `kj::OneWayLock` - Simple one-way lock
- `.lockExclusive()` - Acquire exclusive lock
- `.lockShared()` - Acquire shared read-only lock

### Async
- `kj::Promise<T>` - Async operation
- `.then(continuation)` - Chain callback
- `.wait(waitScope)` - Block until complete
- `.attach(object)` - Keep object alive during async
- `.eagerlyEvaluate(errorHandler)` - Force eager evaluation
- `kj::TaskSet` - Manage multiple async tasks
- `co_await` - Coroutine support (C++20)
- `kj::CoroutineAllocator` - Coroutine allocation optimization

### Event Loop
- `kj::setupAsyncIo()` - Setup full async I/O
- `kj::EventLoop` - Simple event loop
- `kj::WaitScope` - Synchronous wait context
- `kj::NEVER_DONE` - Promise that never completes
- `kj::evalNow()` - Execute immediately as async
- `kj::evalLater()` - Execute on next turn
- `kj::evalLast()` - Execute after all queued work

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
- `kj::Date` - Calendar time type

### Strings
- `kj::String` - Owned string
- `kj::StringPtr` - Non-owned reference
- `kj::ConstString` - Constant string (optimization)
- `"_kj` suffix - String literal for runtime
- `"_kjc` suffix - Constexpr string literal
- `kj::LiteralStringConst` - Constexpr string wrapper
- `kj::str(...)` - String concatenation/formatting
- `kj::strPreallocated(buffer, ...)` - Preallocated string building

### Functions
- `kj::Function<T>` - Store callable objects

### Exceptions
- `kj::Exception` - KJ exception class with rich context
- `kj::Exception::Type` - Exception types (FAILED, OVERLOADED, DISCONNECTED, UNIMPLEMENTED)
- `KJ_EXCEPTION(type, description)` - Create KJ exception
- `KJ_ASSERT(condition, ...)` - Runtime assertion
- `KJ_REQUIRE(condition, ...)` - Precondition check
- `KJ_THROW(exception)` - Throw KJ exception
- `KJ_DEFER(...)` - Execute on scope exit
- `KJ_TRY / KJ_CATCH` - Safe exception handling macros

### Filesystem
- `kj::Path` - Type-safe path handling (prevents injection)
- `kj::Path::parse()` - Parse traditional path (canonicalizes)
- `kj::File` - File interface (read/write)
- `kj::ReadableFile` - Read-only file interface
- `kj::AppendableFile` - Append-only file interface
- `kj::Directory` - Directory interface
- `kj::WriteMode` - Write mode flags (CREATE, MODIFY, CREATE_PARENT, etc.)
- `kj::newDiskFilesystem()` - Get real filesystem
- `kj::newInMemoryDirectory()` - Create in-memory filesystem

### Source Location
- `kj::SourceLocation` - Compile-time source location (file, line, column, function)
- `kj::NoopSourceLocation` - Noop source location for disabled tracking

### I/O
- `kj::InputStream` - Input stream interface
- `kj::OutputStream` - Output stream interface
- `kj::FdInputStream` - File descriptor input
- `kj::FdOutputStream` - File descriptor output
- `kj::OneWayPipe` - One-way pipe
- `kj::TwoWayPipe` - Two-way pipe
- `kj::CapabilityPipe` - Unix capability pipe

### Async Lifecycle
- `kj::AsyncObject` - Base class for async objects
- `kj::DisallowAsyncDestructorsScope` - Prevent async destruction
- `kj::AllowAsyncDestructorsScope` - Allow async destruction

## Files to Check

- `/Users/bytedance/Develop/VeloZ/libs/core/kj/` - KJ library source code (headers)
- `/Users/bytedance/Develop/VeloZ/docs/kjdoc/tour.md` - KJ tour and concepts
- `/Users/bytedance/Develop/VeloZ/docs/kjdoc/style-guide.md` - KJ coding style guide

## Notes for Claude Code

When writing C++ code for VeloZ:

1. **ALWAYS check KJ first** before considering std library types
2. **Use the decision tree** above to determine whether KJ or std is appropriate
3. **Follow the type mapping** to choose correct KJ types
4. **ONLY use std types** when KJ doesn't provide equivalent functionality or external API requires it
5. **Add comments** when using std types to explain why KJ was not used
6. **Always include `#include <kj/common.h>`** when using KJ types

### PROHIBITED: Never Use These std Types When KJ Provides Equivalent

The following std library types are **PROHIBITED** in VeloZ code unless KJ has no equivalent:

**Memory:**
- ❌ `std::unique_ptr<T>` → Use `kj::Own<T>` with `kj::heap<T>()`
- ❌ `std::shared_ptr<T>` → Use `kj::Own<T>` or `kj::Refcounted<T>`
- ❌ `std::optional<T>` → Use `kj::Maybe<T>` with `KJ_IF_SOME`
- ❌ `std::vector<T>` → Use `kj::Array<T>` or `kj::Vector<T>`
- ❌ `std::make_unique` → Use `kj::heap<T>()`
- ❌ `std::make_shared` → Use `kj::atomicRefcounted<T>()`

**Strings:**
- ❌ `std::string` → Use `kj::String` with `_kj` suffix
- ❌ `std::string_view` → Use `kj::StringPtr`
- ❌ `std::stringstream` → Use `kj::str(...)` for concatenation
- ❌ `std::format` → Use `kj::str(...)` for formatting
- ❌ `std::to_string` → Use `kj::str(...)` for number conversion

**Threading:**
- ❌ `std::mutex` → Use `kj::Mutex`
- ❌ `std::unique_lock` → Use `kj::Mutex::Guard`
- ❌ `std::shared_mutex` → Use `kj::Mutex` with `lockShared()`
- ❌ `std::condition_variable` → Use `kj::MutexGuarded<T>`

**Exceptions:**
- ❌ `std::exception` / `std::runtime_error` → Use `kj::Exception`
- ❌ `std::logic_error` → Use `kj::Exception` with appropriate Type
- ❌ `assert()` → Use `KJ_ASSERT()` macro
- ❌ `std::logic_error` for preconditions → Use `KJ_REQUIRE()` macro
- ❌ `throw std::runtime_error(...)` → Use `KJ_THROW()` or `KJ_EXCEPTION()` macro

**Async:**
- ❌ `std::future<T>` → Use `kj::Promise<T>` with `.then()`
- ❌ `std::promise<T>` → Use `kj::PromiseFulfillerPair<T>`
- ❌ `std::async` → Use `kj::Promise<T>` with `kj::evalLater()`

**Time:**
- ❌ `std::chrono::duration` → Use `kj::Duration`
- ❌ `std::chrono::time_point` → Use `kj::TimePoint`
- ❌ `std::this_thread::sleep_for` → Use `kj::sleep()`

**Filesystem:**
- ❌ std filesystem path → Use `kj::Path`
- ❌ `std::filesystem` functions → Use `kj::File`, `kj::Directory`
- ❌ `std::fstream`, `std::ifstream`, `std::ofstream` → Use `kj::File` methods

**Source Location:**
- ❌ `std::source_location` → Use `kj::SourceLocation`

**Functions:**
- ❌ `std::function<T>` → Use `kj::Function<T>`

**I/O:**
- ❌ `std::istream`, `std::ostream` → Use `kj::InputStream`, `kj::OutputStream`
- ❌ `std::iostream`, `std::stringstream` → Use `kj::str(...)` for formatting

**Networking:**
- ❌ ANY std networking → Use KJ async networking (`kj::Network`, `kj::Promise<T>`)

Remember: **KJ is DEFAULT choice** for VeloZ C++ code. Only use std when KJ cannot satisfy the requirement.
