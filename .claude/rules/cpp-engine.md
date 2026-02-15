---
paths:
  - "apps/engine/**"
  - "libs/**"
  - "CMakeLists.txt"
  - "CMakePresets.json"
  - "scripts/build.sh"
  - "scripts/run_engine.sh"
---

# C++ engine / libs (module notes)

- The gateway runs the engine in **`--stdio` mode**. Keep `--stdio` behavior and the stdio wire protocol compatible when changing engine code.
- **Inbound commands (stdin)** are plain text:
  - `ORDER <BUY|SELL> <symbol> <qty> <price> <client_order_id>`
  - `CANCEL <client_order_id>`
- **Outbound events (stdout)** are **NDJSON** (one JSON object per line). The gateway/UI depend on stable `type` and field names.
  - Common `type`: `market`, `order_update`, `fill`, `order_state`, `account`, `error`.
- Key entrypoints:
  - `apps/engine/src/main.cpp` (flag parsing)
  - `apps/engine/src/engine_app.cpp` (mode switch)
  - `apps/engine/src/stdio_engine.cpp` (stdio loop)
  - `apps/engine/src/command_parser.cpp` (ORDER/CANCEL parsing)
  - `apps/engine/src/event_emitter.cpp` (event JSON emission)
- Build outputs follow CMake presets: `build/<preset>/apps/engine/veloz_engine`.
  - `./scripts/build.sh <preset>` is the canonical wrapper used by other scripts.
  - `asan` preset is available for ASan/UBSan self-check builds.

# C++ Coding Rules (CRITICAL)

## NO RAW POINTERS

**Entire project prohibits raw pointers (T*)**. All nullable pointers must use `kj::Maybe<T>` or KJ-owned types:

| Raw Pointer (FORBIDDEN) | KJ Alternative (REQUIRED) |
|-------------------------|----------------------------|
| `T*` (nullable) | `kj::Maybe<T>` or `kj::Own<T>` |
| `T*` (non-nullable) | `T&` or `kj::Own<T>` |
| `T*` (function return, nullable) | `kj::Maybe<T>` |
| `T*` (function return, non-null) | `kj::Own<T>` or `T&` |
| `nullptr` check | `KJ_IF_SOME(value, ...) { ... }` |

### Examples

```cpp
// ❌ FORBIDDEN - Raw pointer (nullable)
T* findObject();
if (obj != nullptr) {
    obj->doSomething();
}

// ✅ REQUIRED - KJ Maybe (optional value)
kj::Maybe<T> findObject();
KJ_IF_SOME(obj, findObject()) {
    obj->doSomething();  // obj 是 T& 类型
}

// ❌ FORBIDDEN - Raw pointer ownership
T* createObject();
void destroyObject(T* obj);

// ✅ REQUIRED - KJ Own
kj::Own<T> createObject();
// No destroyObject needed - kj::Own<T> auto-deletes

// ❌ FORBIDDEN - Raw pointer parameter
void process(T* obj);

// ✅ REQUIRED - Reference or Own<T>
void process(T& obj);           // Non-null, non-owning
void process(kj::Own<T> obj);  // Transfer ownership

// ❌ FORBIDDEN - Raw pointer return (non-null guaranteed)
T* getReference();

// ✅ REQUIRED - Reference or Own return
T& getReference();        // Non-null reference
kj::Own<T> getOwned();  // Transfer ownership
```

### Allowed Exceptions (document required)

Raw pointers are ONLY allowed in these cases, and MUST be documented with a comment:

1. **External C API** - When interfacing with C libraries
   ```cpp
   // Using raw pointer for external C API: c_library_func()
   extern "C" void c_library_func(void* ctx);
   ```

2. **Opaque types** - When working with platform-specific opaque handles
   ```cpp
   // Using raw pointer for platform handle: socket_t
   using socket_t = void*;  // Platform-specific opaque type
   ```

3. **KJ internal** - Within kj::Own<T>, kj::Maybe<T> implementations
   ```cpp
   // KJ internal implementation detail
   T* ptr;  // Inside Own<T> implementation only
   ```

**When using raw pointers, ALWAYS add a comment explaining why.**
