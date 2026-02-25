# VeloZ Productization (Internal Platform) Implementation Plan

Date: 2026-02-25

## Overview

This plan tracks the remaining productization tasks focused on operational dashboards, stability KPIs, and a one-command run path. It builds on the existing engine-gateway-UI stack and aligns health, metrics, and run scripts for day-to-day operations.

## Task 1: OMS WAL recovery and replay

**Goal:** Ensure order WAL replay restores OMS state after restart and survives corruption gaps.

**Files:**
- Modify: `libs/oms/src/order_wal.cpp`
- Modify: `libs/oms/include/veloz/oms/order_wal.h`
- Test: `libs/oms/tests/test_order_wal.cpp`

**Step 1: Write the failing test**

```cpp
KJ_TEST("OrderWal: crash recovery replay into store") {
  // Create WAL, write entries, then replay into a fresh OrderStore
}
```

**Step 2: Run test to verify it fails**

Run: `cmake --build --preset dev-tests -j$(nproc) && ./build/dev/libs/oms/veloz_oms_tests`
Expected: FAIL (replay path incomplete or inconsistent)

**Step 3: Write minimal implementation**

```cpp
wal.replay_into(order_store);
```

**Step 4: Run test to verify it passes**

Run: `cmake --build --preset dev-tests -j$(nproc) && ./build/dev/libs/oms/veloz_oms_tests`
Expected: PASS

## Task 2: Order reconciliation pipeline

**Goal:** Reconcile local OMS state with venue open orders to detect orphaned orders.

**Files:**
- Modify: `libs/exec/src/reconciliation.cpp`
- Modify: `libs/exec/include/veloz/exec/reconciliation.h`
- Test: `libs/exec/tests/test_reconciliation.cpp`

**Step 1: Write the failing test**

```cpp
KJ_TEST("AccountReconciler: detects orphaned order") {
  // Given venue has open order not in OrderStore, mark mismatch
}
```

**Step 2: Run test to verify it fails**

Run: `cmake --build --preset dev-tests -j$(nproc) && ./build/dev/libs/exec/veloz_exec_tests`
Expected: FAIL (mismatch not recorded)

**Step 3: Write minimal implementation**

```cpp
reconciler.reconcile_now();
```

**Step 4: Run test to verify it passes**

Run: `cmake --build --preset dev-tests -j$(nproc) && ./build/dev/libs/exec/veloz_exec_tests`
Expected: PASS

## Task 3: Gateway reconnect and health endpoints

**Goal:** Ensure gateway reconnect state and health endpoints are stable and consumable by UI.

**Steps:**
1. Confirm `/api/health` returns engine connectivity.
2. Ensure gateway endpoints reflect reconnect state where applicable.
3. Verify integration tests cover health behavior.

## Task 4: Operational UI dashboard for core KPIs

**Files:**
- Modify: `apps/ui/src/features/dashboard/index.tsx`
- Modify: `apps/ui/src/shared/api/hooks/useEngine.ts`
- Modify: `apps/ui/src/shared/api/hooks/useEngine.test.tsx`

**Step 1: Write the failing test**

```tsx
render(<Dashboard />);
expect(await screen.findByText(/Engine Connected/)).toBeInTheDocument();
```

**Step 2: Run test to verify it fails**

Run: `cd apps/ui && npm test -- --runInBand`
Expected: FAIL (missing fields in UI)

**Step 3: Write minimal implementation**

```tsx
<Card title="Engine Connected">{engine.connected ? "Yes" : "No"}</Card>
```

**Step 4: Run test to verify it passes**

Run: `cd apps/ui && npm test -- --runInBand`
Expected: PASS

## Task 5: Metrics pipeline for stability KPIs

**Files:**
- Modify: `libs/core/src/metrics.cpp`
- Modify: `apps/engine/src/event_emitter.cpp`
- Modify: `apps/gateway/metrics.py`
- Test: `libs/core/tests/test_metrics.cpp`

**Step 1: Write the failing test**

```cpp
TEST_CASE("Metrics: exposes reconnect_count") {
  auto metrics = veloz::core::Metrics::snapshot();
  REQUIRE(metrics.contains("reconnect_count"));
}
```

**Step 2: Run test to verify it fails**

Run: `cmake --build --preset dev-tests -j$(nproc) && ./build/dev/libs/core/veloz_core_tests`
Expected: FAIL (missing metric)

**Step 3: Write minimal implementation**

```cpp
metrics["reconnect_count"] = reconnectCount.load();
```

**Step 4: Run test to verify it passes**

Run: `cmake --build --preset dev-tests -j$(nproc) && ./build/dev/libs/core/veloz_core_tests`
Expected: PASS

## Task 6: One-command operational run

**Files:**
- Modify: `scripts/run_gateway.sh`
- Modify: `scripts/run_engine.sh`
- Test: `docs/guides/build_and_run.md`

**Step 1: Write the failing test**

```bash
./scripts/run_gateway.sh dev --dry-run | grep "API Base"
```

**Step 2: Run test to verify it fails**

Run: `./scripts/run_gateway.sh dev --dry-run`
Expected: FAIL (no dry-run mode)

**Step 3: Write minimal implementation**

```bash
if [[ "$1" == "--dry-run" ]]; then
  echo "API Base: http://127.0.0.1:8080"
  exit 0
fi
```

**Step 4: Run test to verify it passes**

Run: `./scripts/run_gateway.sh dev --dry-run`
Expected: PASS
