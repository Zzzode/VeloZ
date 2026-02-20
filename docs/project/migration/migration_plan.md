# KJ Library Migration Plan

**Project**: VeloZ KJ Library Migration
**Created**: 2026-02-15
**Author**: Project Manager
**Status**: Planning Phase

---

## Executive Summary

This document outlines the migration strategy for converting VeloZ from C++ standard library types to KJ library equivalents. The migration follows a bottom-up approach, starting with foundational libraries and progressing to application-layer components.

### User Requirement

> "Complete migration without compromise, do not retain std"

All migratable std types must be replaced with KJ equivalents. Only justified exceptions (where KJ lacks equivalent functionality) are permitted.

---

## 1. Current State Analysis

### std:: Usage by Module

| Module | std:: Count | Priority | Complexity |
|--------|-------------|----------|------------|
| libs/core | 1352 | HIGH | High (foundational) |
| libs/common | 7 | LOW | Low |
| libs/market | 78 | MEDIUM | Medium |
| libs/exec | 139 | MEDIUM | Medium |
| libs/oms | 37 | LOW | Low |
| libs/risk | 54 | LOW | Low |
| libs/strategy | 74 | MEDIUM | Medium |
| libs/backtest | 332 | MEDIUM | Medium |
| apps/engine | 37 | LOW | Low |

### Top std:: Types to Migrate

| Type | Count | KJ Equivalent | Migration Complexity |
|------|-------|---------------|---------------------|
| `std::string` | 536 | `kj::String` | Medium |
| `std::chrono` | 330 | `kj::Duration` (partial) | High - some must remain |
| `std::vector` | 203 | `kj::Vector<T>`, `kj::Array<T>` | Medium |
| `std::format` | 129 | `kj::str()` | Low |
| `std::atomic` | 110 | N/A - must remain | N/A |
| `std::string_view` | 106 | `kj::StringPtr` | Low |
| `std::filesystem` | 72 | N/A - must remain | N/A |
| `std::function` | 64 | `kj::Function<T>` | Medium |
| `std::unique_ptr` | 53 | `kj::Own<T>` | Low |
| `std::mutex` | 37 | `kj::MutexGuarded<T>` | Medium |

---

## 2. Module Dependency Graph

```
libs/common (no deps)
    |
libs/core (depends on: common, kj, kj-async)
    |
    +-- libs/market (depends on: core, common, kj-tls)
    |
    +-- libs/exec (depends on: core, common, kj-tls, OpenSSL)
            |
            +-- libs/oms (depends on: exec, core, common)
            |       |
            |       +-- libs/risk (depends on: oms, exec, core, common)
            |
            +-- libs/strategy (depends on: core, common, market)
                    |
                    +-- libs/backtest (depends on: strategy, market, core, common)

apps/engine (depends on: all libs)
```

---

## 3. Migration Order

Based on dependencies, the migration must proceed in this order:

### Phase 1: Foundation (Tasks #3, #10)
1. **libs/core** (Task #3) - Foundational, all other modules depend on it
2. **Build configuration** (Task #10) - Ensure KJ is properly linked

### Phase 2: Core Libraries (Tasks #4, #5, #6, #7)
3. **libs/market** (Task #4) - Depends only on core
4. **libs/exec** (Task #5) - Depends only on core
5. **libs/oms** (Task #6) - Depends on exec, core
6. **libs/risk** (Task #7) - Depends on oms, exec, core

### Phase 3: Application Libraries (Tasks #8, #9)
7. **libs/strategy** (Task #8) - Depends on core, market
8. **libs/backtest** (Task #9) - Depends on strategy, market, core

### Phase 4: Application Layer (Tasks #13, #11, #12, #14)
9. **apps/engine** (Task #13) - Depends on all libs
10. **Verify test suite** (Task #11) - Full regression testing
11. **Document completion** (Task #12) - Final documentation
12. **Update CLAUDE.md** (Task #14) - Update project rules

---

## 4. Type Mapping Guidelines

### Direct Replacements

```cpp
// Owned pointers
std::unique_ptr<T> ptr = std::make_unique<T>(args...);
// becomes:
kj::Own<T> ptr = kj::heap<T>(args...);

// Optional values
std::optional<T> opt;
// becomes:
kj::Maybe<T> opt;
// Access with KJ_IF_SOME:
KJ_IF_SOME(value, opt) { use(value); }

// Dynamic arrays
std::vector<T> vec;
vec.push_back(item);
// becomes:
kj::Vector<T> vec;
vec.add(item);

// Fixed arrays (when size known)
std::vector<T> arr(size);
// becomes:
kj::Array<T> arr = kj::heapArray<T>(size);

// Strings
std::string str = "hello";
// becomes:
kj::String str = kj::str("hello");
// or for literals:
auto str = "hello"_kj;

// String views
std::string_view sv;
// becomes:
kj::StringPtr sp;

// Functions
std::function<R(Args...)> fn;
// becomes:
kj::Function<R(Args...)> fn;

// Mutex with data
std::mutex mtx;
T data;
// becomes:
kj::MutexGuarded<T> guarded;
auto lock = guarded.lockExclusive();

// Hash maps
std::unordered_map<K, V> map;
// becomes:
kj::HashMap<K, V> map;
map.insert(key, value);
KJ_IF_SOME(v, map.find(key)) { use(v); }

// Ordered maps
std::map<K, V> map;
// becomes:
kj::TreeMap<K, V> map;

// Sets
std::unordered_set<T> set;
// becomes:
kj::HashSet<T> set;

std::set<T> set;
// becomes:
kj::TreeSet<T> set;

// Variants
std::variant<A, B, C> var;
// becomes:
kj::OneOf<A, B, C> var;
if (var.is<A>()) { use(var.get<A>()); }
```

### Justified Exceptions (Must Remain std::)

| std Type | Reason | Required Comment |
|----------|--------|------------------|
| `std::atomic<T>` | KJ has no atomic primitives | `// std::atomic - KJ has no atomic equivalent` |
| `std::chrono::system_clock` | Wall clock time (KJ time is async I/O only) | `// std::chrono for wall clock timestamps` |
| `std::random_device`, `std::mt19937` | KJ has no RNG | `// std::random_device/mt19937 - KJ lacks RNG` |
| `std::filesystem` | KJ lacks filesystem API | `// std::filesystem - KJ lacks filesystem API` |
| `std::thread::sleep_for` | Non-async sleep | `// std::thread::sleep_for for non-async sleep` |
| `std::abs`, `std::ceil`, `std::stod` | Standard C++ math | `// std::abs for math operations` |
| `std::sort`, `std::memcpy` | Standard algorithms | `// std::sort algorithm` |
| `std::condition_variable` | Requires std::mutex | `// std::condition_variable requires std::mutex` |
| `std::string` (external APIs) | OpenSSL, yyjson C APIs | `// std::string for [API] compatibility` |

---

## 5. Testing Strategy

### Per-Module Testing

For each module migration:

1. **Pre-migration baseline**: Run existing tests, record pass count
2. **Migration**: Apply type conversions
3. **Compile check**: Ensure module compiles without errors
4. **Unit tests**: Run module-specific tests
5. **Integration tests**: Run dependent module tests
6. **Regression check**: Compare test results with baseline

### Test Commands

```bash
# Build specific module tests
cmake --preset dev
cmake --build --preset dev-tests -j$(nproc)

# Run specific module tests
./build/dev/libs/core/veloz_core_tests
./build/dev/libs/market/veloz_market_tests
./build/dev/libs/exec/veloz_exec_tests
./build/dev/libs/oms/veloz_oms_tests
./build/dev/libs/risk/veloz_risk_tests
./build/dev/libs/strategy/veloz_strategy_tests
./build/dev/libs/backtest/veloz_backtest_tests

# Run all tests
ctest --preset dev -j$(nproc)
```

### Acceptance Criteria

- All existing tests must pass (or have documented exceptions)
- No new compiler warnings related to KJ usage
- No memory leaks detected by ASan (where KJ supports it)
- Build succeeds for dev, release presets

---

## 6. Risk Assessment

### High Risk Areas

| Area | Risk | Mitigation |
|------|------|------------|
| libs/core | Foundational - breaks everything if wrong | Thorough testing, incremental changes |
| Thread synchronization | Race conditions if mutex patterns wrong | Review all lock patterns, test concurrency |
| String handling | kj::String is move-only, different semantics | Careful review of string ownership |
| Async I/O | KJ async model differs from std | Follow KJ patterns strictly |

### Medium Risk Areas

| Area | Risk | Mitigation |
|------|------|------------|
| Container APIs | kj::HashMap/Vector have different APIs | Document API differences, review all usages |
| External API boundaries | May need std::string for C APIs | Keep std::string at boundaries with comments |

### Low Risk Areas

| Area | Risk | Mitigation |
|------|------|------------|
| libs/common | Minimal std usage | Simple migration |
| apps/engine | Mostly uses lib APIs | Follows lib migrations |

---

## 7. Developer Assignment

### Recommended Team Structure

| Developer | Assigned Tasks | Modules |
|-----------|---------------|---------|
| dev-core | #3 | libs/core (highest complexity) |
| dev-market | #4 | libs/market |
| dev-exec | #5 | libs/exec |
| dev-oms | #6 | libs/oms |
| dev-risk | #7 | libs/risk |
| dev-strategy | #8 | libs/strategy |
| dev-backtest | #9 | libs/backtest |
| dev-engine | #13 | apps/engine |

### Dependencies for Assignment

- **Phase 1**: dev-core starts immediately (Task #3)
- **Phase 2**: dev-market, dev-exec can start after Task #3 completes
- **Phase 2 continued**: dev-oms waits for Task #5, dev-risk waits for Task #6
- **Phase 3**: dev-strategy, dev-backtest wait for Phase 2
- **Phase 4**: dev-engine waits for all lib migrations

---

## 8. Rollback Plan

If migration causes critical issues:

1. **Git revert**: Each module migration should be a separate commit
2. **Feature flags**: Not applicable (type changes are compile-time)
3. **Parallel branches**: Maintain `master` as stable, migrate in feature branch

### Rollback Triggers

- More than 10% test failures after migration
- Critical runtime errors in production paths
- Memory corruption detected by ASan

---

## 9. Success Criteria

- [ ] All modules migrated to KJ types
- [ ] All tests passing (146/146 or documented exceptions)
- [ ] Build succeeds: dev, release presets
- [ ] No unjustified std:: usage remaining
- [ ] All justified std:: usage has comments
- [ ] Documentation updated (CLAUDE.md, migration docs)

---

## 10. References

- **KJ Usage Guide**: `docs/kj/library_usage_guide.md`
- **KJ Skill**: `docs/kj/skill.md`
- **KJ Tour**: `docs/kjdoc/tour.md`
- **Project Standards**: `CLAUDE.md`

---

## Document History

- 2026-02-15: Initial migration plan created
