# Core Module Architecture & Design Analysis
**Date**: 2026-02-12
**Version**: VeloZ v1.0
**Module**: libs/core

## Overview

The VeloZ core module provides fundamental infrastructure for the quantitative trading framework, implementing high-performance, type-safe, and production-ready utilities for event handling, logging, JSON processing, memory management, and configuration.

## Core Components

### 1. Event Loop (`event_loop.h/cpp`)

**Purpose**: Asynchronous task execution with priority-based scheduling

**Design Pattern**: Single-threaded event loop with priority queue

**Key Features**:
- 4-level priority system (Low, Normal, High, Critical)
- Event tags for filtering and routing
- Delayed task execution
- Built-in statistics tracking
- Filter predicates for selective event processing
- Custom event routers

**Implementation Details**:

```cpp
// Priority-based task scheduling
struct Task {
    std::function<void()> task;
    EventPriority priority;  // Low/Normal/High/Critical
    std::vector<EventTag> tags;
    std::chrono::steady_clock::time_point enqueue_time;
};

// Filtering and routing
std::unordered_map<uint64_t, std::pair<EventFilter, std::optional<EventPriority>>> filters_;
std::unordered_map<uint64_t, std::regex> tag_filters_;
std::optional<EventRouter> router_;
```

**Design Decisions**:
1. **Single-threaded execution**: All tasks run in a single thread for simplicity and deterministic ordering
2. **Priority-based scheduling**: Uses priority queue to ensure critical events execute first
3. **Tag-based routing**: Flexible filtering allows selective event processing
4. **Statistics-first**: Comprehensive metrics collection for observability

**Thread Safety**:
- `std::mutex mu_` - Protects all shared state
- `std::condition_variable cv_` - Coordinates producer/consumer threads
- `std::atomic<bool>` - For flags and statistics

**Performance Characteristics**:
- O(1) task posting (with lock)
- O(log n) task execution (priority queue)
- O(1) filter application (hash-based lookup)

**Statistics Tracked**:
- Total events processed
- Queue wait times (average/max)
- Processing times (average/max)
- Failed/filtered event counts
- Per-priority breakdown

---

### 2. Logging System (`logger.h/cpp`)

**Purpose**: Thread-safe, multi-destination logging with formatters and rotation

**Design Pattern**: Strategy pattern for formatters, composite pattern for outputs

**Key Features**:
- 6-level severity hierarchy (Trace/Debug/Info/Warn/Error/Critical/Off)
- Multiple output destinations (Console, File, Multi)
- Custom formatters (Text, JSON)
- Log rotation (Size-based, Time-based, Both)
- Source location tracking with `std::source_location`
- Formatted logging with `std::format`

**Implementation Details**:

```cpp
// Formatter interface (Strategy pattern)
class LogFormatter {
    virtual std::string format(const LogEntry& entry) const = 0;
};

// Output destinations (Composite pattern)
class LogOutput {
    virtual void write(const std::string& formatted, const LogEntry& entry) = 0;
    virtual void flush() = 0;
};
```

**Design Decisions**:
1. **Interface-based formatters**: Easy to add new output formats (Syslog, Binary, etc.)
2. **Multi-output support**: Single log can go to console + file simultaneously
3. **Rotation built-in**: Automatic log file management without external tools
4. **Color-coded output**: ANSI color codes for terminal readability
5. **Source location**: Built-in C++23 feature for debugging context

**Thread Safety**:
- Each formatter is stateless (safe to share)
- Multi-output uses `std::mutex` for synchronization
- Logger class is fully thread-safe

**Performance Characteristics**:
- O(1) log call (with formatting)
- O(1) flush operation
- Minimal allocation overhead (string_view usage where possible)

**Log Levels**:
```
Trace (0)  → Most detailed, for deep debugging
Debug (1)   → Development debugging
Info (2)    → Normal runtime information
Warn (3)     → Potential issues
Error (4)    → Error conditions
Critical (5) → Errors preventing system continuation
Off (6)      → Disable all output
```

---

### 3. JSON Processing (`json.h/cpp`)

**Purpose**: High-performance JSON parsing with type-safe API and RAII management

**Design Pattern**: RAII wrapper with zero-copy access where possible

**Key Features**:
- Wrap yyjson library (MIT-licensed, high performance)
- Type-safe parsing (`parse_as<T>()`)
- Compile-time type checking
- JSON builder for fluent API
- Array iteration with type inference
- String view access for zero-copy

**Implementation Details**:

```cpp
// RAII wrapper
class JsonDocument {
    static JsonDocument parse(const std::string& str);
    template<typename T>
    std::optional<T> parse_as() const;
};

// Builder pattern
class JsonBuilder {
    static JsonBuilder object();
    JsonBuilder& put(const std::string& key, const T& value);
    std::string build(bool pretty = false) const;
};
```

**Design Decisions**:
1. **yyjson library**: Chosen for performance and MIT licensing
2. **Type-safe API**: Templates for compile-time type checking
3. **RAII wrapper**: Automatic resource cleanup, exception-safe
4. **Zero-copy access**: `get_string_view()` for performance-critical paths

**Thread Safety**:
- `JsonDocument`: Read-only after parsing (safe to share)
- `JsonBuilder`: Single-threaded construction (build, then use)

**Performance Characteristics**:
- O(n) parsing (where n = input size)
- O(1) value access (direct pointer access)
- O(1) array iteration (callback-based)

**Supported Types**:
- bool, int, int32_t, int64_t, uint32_t, uint64_t, float, double, std::string
- Vectors of all above types

---

### 4. Memory Management (`memory_pool.h/cpp`)

**Purpose**: Advanced memory pool allocation with statistics tracking

**Design Pattern**: Object pool pattern, allocator pattern

**Key Features**:
- Fixed-size memory pools for specific types
- Block allocation to reduce fragmentation
- STL-compatible allocator (`PoolAllocator<T>`)
- Memory usage monitoring (`MemoryMonitor`)
- Allocation site tracking
- RAII-based tracking (`MemoryTracker<T>`)

**Implementation Details**:

```cpp
// Memory pool interface
class MemoryPoolBase {
    virtual void* allocate() = 0;
    virtual void deallocate(void* ptr) = 0;
    // Statistics methods...
};

// Fixed-size pool (template-based)
template <typename T, size_t BlockSize = 64>
class FixedSizeMemoryPool {
    void* allocate() override;
    kj::Own<T, Deleter> create(Args&&... args);
};
```

**Design Decisions**:
1. **Fixed-size blocks**: Reduces fragmentation, improves cache locality
2. **Block-based allocation**: Allocate large blocks, partition into slots
3. **STL allocator**: Seamless integration with `std::vector<T, PoolAllocator<T>>`
4. **Global monitoring**: Single `MemoryMonitor` tracks all allocations
5. **Preallocation**: Optional pre-allocation for known workloads

**Thread Safety**:
- Each pool has `std::mutex mu_`
- Atomic statistics counters
- Thread-safe allocation/deallocation

**Performance Characteristics**:
- O(1) allocation (from free list or new block)
- O(1) deallocation (return to free list)
- Zero fragmentation (fixed-size blocks)
- Cache-friendly (contiguous allocation blocks)

**Memory Statistics**:
- Total/peak allocated bytes
- Allocation/deallocation counts
- Per-site tracking (MemoryAllocationSite)
- Alert thresholds

---

### 5. Configuration Management (`config_manager.h/cpp`)

**Purpose**: Type-safe, hierarchical configuration with validation and hot-reload

**Design Pattern**: Builder pattern, composite pattern (groups), observer pattern (callbacks)

**Key Features**:
- Type-safe config items (`ConfigItem<T>`)
- Hierarchical organization (`ConfigGroup`)
- Custom validators per item
- Change notifications (observer pattern)
- Hot-reload from JSON/YAML files
- Required/default value tracking
- JSON serialization

**Implementation Details**:

```cpp
// Type-safe config item
template <typename T>
class ConfigItem {
    std::optional<T> get() const;
    bool set(const T& value);
    void add_callback(ConfigChangeCallback<T> callback);
};

// Hierarchical groups
class ConfigGroup {
    void add_item(kj::Own<ConfigItemBase> item);
    void add_group(kj::Own<ConfigGroup> group);
};

// Central manager
class ConfigManager {
    ConfigItem<T>* find_item(std::string_view path) const;
    bool validate() const;
    void trigger_hot_reload();
};
```

**Design Decisions**:
1. **Type safety via templates**: Compile-time checking prevents type errors
2. **Builder pattern**: Fluent API for config definition
3. **Hierarchical organization**: Groups can contain sub-groups and items
4. **Path-based access**: `"group.subgroup.item"` notation
5. **Hot-reload**: Watch file and trigger callbacks on change
6. **Validator functions**: Custom validation per config item

**Thread Safety**:
- Each config item has `std::mutex mu_`
- Atomic flags (`std::atomic<bool> is_set_`)
- Safe multi-threaded access

**Supported Types**:
- Scalar: bool, int, int64_t, double, std::string
- Arrays: bool, int, int64_t, double, std::string

---

## Design Patterns Summary

| Pattern | Components | Purpose |
|---------|--------------|-----------|
| RAII | JsonDocument, MemoryPool | Automatic resource cleanup |
| Strategy | LogFormatter, ConfigValidator | Pluggable algorithms/formatters |
| Composite | MultiOutput, ConfigGroup | Hierarchical organization |
| Builder | JsonBuilder, ConfigItem::Builder | Fluent construction API |
| Observer | ConfigManager callbacks | Change notifications |
| Singleton | Logger, MemoryMonitor | Global accessors |

## Thread Safety Strategy

1. **Fine-grained locking**: Each component has its own mutex
2. **Atomic counters**: For statistics (lock-free reads)
3. **RAII-based**: Lock guards (`std::lock_guard`, `std::scoped_lock`)
4. **Copy-prohibited**: Disable copy constructors to prevent accidental sharing

## Performance Considerations

1. **Zero-copy where possible**: Use `string_view`, avoid allocations
2. **Fixed-size allocations**: Reduce fragmentation (memory pool)
3. **Compile-time checks**: Type safety without runtime overhead
4. **Lock-free reads**: Atomic counters for statistics
5. **Efficient data structures**: `std::unordered_map` for O(1) lookups

## Error Handling Strategy

1. **Exceptions**: Use C++ exceptions for error propagation
2. **Optional returns**: `std::optional<T>` for operations that may fail
3. **Validation functions**: Pre-check before operations
4. **RAII cleanup**: Exception-safe resource management

## Modern C++23 Features Utilized

1. **`std::format`**: Type-safe string formatting
2. **`std::source_location`**: Built-in source tracking
3. **`std::optional`**: Null-safety for operations
4. **`std::variant`**: Type-safe union for config values
5. **`std::atomic`**: Lock-free counters and flags
6. **`std::string_view`**: Zero-copy string access
7. **`std::function`**: Callable objects for callbacks/handlers
8. **`std::unique_ptr`**: RAII ownership management

## Dependencies

**External Libraries**:
- `yyjson` - MIT licensed JSON parser

**Standard Library Usage**:
- `<thread>` - Threading primitives
- `<mutex>` - Synchronization primitives
- `<atomic>` - Lock-free operations
- `<filesystem>` - File operations
- `<format>` - String formatting
- `<variant>` - Type-safe unions

## Integration Points

The core module provides integration points for other modules:

### Logger Integration
```cpp
// Global logger accessor
veloz::core::Logger& logger = veloz::core::global_logger();

// Convenience functions
veloz::core::info("Application started");
veloz::core::error("Failed to connect");
```

### Memory Pool Integration
```cpp
// Create a pool
veloz::core::FixedSizeMemoryPool<Order, 64> pool(100, 1000);

// Use pool
auto order = pool.create( /* args */ );
```

### JSON Integration
```cpp
// Parse JSON
auto doc = veloz::core::JsonDocument::parse(json_string);
double price = doc.parse_as<double>();

// Build JSON
auto builder = veloz::core::JsonBuilder::object();
builder.put("price", 50000.0);
std::string json = builder.build();
```

### Config Integration
```cpp
// Load from file
veloz::core::ConfigManager manager("app");
manager.load_from_json("config.json");

// Access config
auto port = manager.find_item<int>("server.port");
int p = port->get_or(8080);

// Set with validation
port->set(8080);  // Validates range
```

## Future Enhancements

### High Priority
1. **Event loop worker threads**: Currently single-threaded, consider thread pool for CPU-bound tasks
2. **Structured logging**: Integration with `spdlog` or `glog` for production
3. **Memory pool growth**: Dynamic pool sizing based on workload patterns
4. **Config encryption**: Secure config file encryption for sensitive data

### Medium Priority
1. **Log aggregation**: Centralized log collection across processes
2. **Memory profiling**: Integration with sanitizers and profilers
3. **Config validation**: Richer validation with dependencies between items
4. **JSON streaming**: Incremental parsing for large files

## Testing Strategy

The core module should be tested for:

1. **Thread safety**: Multiple threads accessing shared resources
2. **Exception safety**: Verify RAII cleanup on exceptions
3. **Performance benchmarks**: Measure allocation overhead, parse times
4. **Edge cases**: Empty input, invalid types, buffer overflows
5. **Stress testing**: Long-running event loops, rapid config changes

## Conclusion

The VeloZ core module demonstrates modern C++23 best practices with:
- **Type safety**: Compile-time checks prevent runtime errors
- **Performance**: Zero-copy operations, fixed-size allocations
- **Observability**: Comprehensive statistics and logging
- **Maintainability**: Clear interfaces, consistent patterns
- **Thread safety**: Explicit synchronization primitives
- **RAII**: Automatic resource management

This architecture provides a solid foundation for the quantitative trading framework, balancing performance, safety, and developer experience.
