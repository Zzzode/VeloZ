# KJ Migration Documents

This directory contains documentation related to the KJ library migration from C++ standard library.

## Documents

| Document | Description | Status |
|----------|-------------|--------|
| [kj_library_migration.md](kj_library_migration.md) | KJ library integration guide and patterns |
| [kj_migration_status.md](kj_migration_status.md) | Migration status tracking by module |
| [migration_plan.md](migration_plan.md) | Detailed migration strategy and plan |
| [std_inventory.md](std_inventory.md) | Standard library usage inventory |

## Migration Status

**Status**: **COMPLETE** ✅

All migratable C++ standard library types have been successfully migrated to KJ library equivalents across all modules.

### Modules Migrated

| Module | KJ Adoption | Status |
|--------|---------------|--------|
| libs/core | 90% | Complete |
| libs/market | High | Complete |
| libs/exec | High | Complete |
| libs/oms | 95% | Complete |
| libs/risk | 100% | Complete |
| libs/strategy | 100% | Complete |
| libs/backtest | High | Complete |
| apps/engine | High | Complete |

### Type Migrations

| std Type | KJ Equivalent | Status |
|----------|---------------|--------|
| `std::unique_ptr<T>` | `kj::Own<T>` | ✅ Complete |
| `std::optional<T>` | `kj::Maybe<T>` | ✅ Complete |
| `std::vector<T>` | `kj::Vector<T>` / `kj::Array<T>` | ✅ Complete |
| `std::unordered_map<K,V>` | `kj::HashMap<K,V>` | ✅ Complete |
| `std::map<K,V>` | `kj::TreeMap<K,V>` | ✅ Complete |
| `std::function<R(Args...)>` | `kj::Function<R(Args...)>` | ✅ Complete |
| `std::mutex` | `kj::Mutex` / `kj::MutexGuarded<T>` | ✅ Complete |

## Remaining std Usage

The remaining std library usage is limited to justified exceptions:

| std Type | Reason |
|----------|--------|
| `std::atomic<T>` | KJ has no atomic primitives |
| `std::chrono` | Wall clock timestamps (KJ time is async I/O only) |
| `std::string` | External API compatibility (OpenSSL, yyjson, std::filesystem) |
| `std::random` | KJ has no RNG |
| `<cmath>` algorithms | Standard C++ math functions |
| `<algorithm>` | Standard algorithms (std::sort, etc.) |

Each remaining std usage is documented with justification comments in the code.

## Quick Reference

### KJ Pattern Examples

```cpp
// Owned pointer
kj::Own<T> ptr = kj::heap<T>(args...);

// Optional value
kj::Maybe<T> maybe = kj::none;
KJ_IF_SOME(value, maybe) { use(value); }

// Dynamic array
kj::Vector<T> vec;
vec.add(value);
auto arr = vec.releaseAsArray();

// Hash map
kj::HashMap<Key, Value> map;
map.insert(key, value);
KJ_IF_SOME(v, map.find(key)) { use(v); }

// Thread-safe state
kj::MutexGuarded<State> state;
auto lock = state.lockExclusive();
```

For detailed KJ library usage patterns, see [KJ Usage Guide](../../kj/library_usage_guide.md).
