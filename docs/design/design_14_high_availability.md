# Design 14: High Availability Architecture

**Status**: Draft
**Author**: System Architect
**Date**: 2026-02-23
**Related Tasks**: #4, #7, #17

---

## 1. Overview

This document defines the high availability (HA) architecture for VeloZ production deployment. It covers state replication, failover mechanisms, leader election, and recovery procedures.

### 1.1 Goals

1. **Availability**: 99.9% uptime (< 8.76 hours downtime per year)
2. **Data Durability**: Zero data loss for committed transactions
3. **Recovery Time**: RTO < 30 seconds for automatic failover
4. **Recovery Point**: RPO < 1 second (near-zero data loss)
5. **Consistency**: Strong consistency for order state

### 1.2 Design Principles

1. **Active-Passive**: Primary handles all traffic; standby ready for failover
2. **WAL-Based Replication**: Leverage existing OrderWal infrastructure
3. **Automatic Failover**: Detect failures and failover without manual intervention
4. **Graceful Degradation**: Continue operating with reduced functionality if needed
5. **Kubernetes Native**: Integrate with K8s health checks and StatefulSets

---

## 2. Architecture Overview

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                              Kubernetes Cluster                              │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│  ┌─────────────────────────────────────────────────────────────────────┐    │
│  │                         Load Balancer / Ingress                      │    │
│  └───────────────────────────────┬─────────────────────────────────────┘    │
│                                  │                                           │
│                    ┌─────────────┴─────────────┐                            │
│                    │                           │                            │
│                    ▼                           ▼                            │
│  ┌─────────────────────────────┐ ┌─────────────────────────────┐           │
│  │     Primary Engine (Active) │ │   Standby Engine (Passive)  │           │
│  │                             │ │                             │           │
│  │  ┌─────────────────────┐   │ │  ┌─────────────────────┐   │           │
│  │  │    Engine Core      │   │ │  │    Engine Core      │   │           │
│  │  │  - Order Router     │   │ │  │  - Order Router     │   │           │
│  │  │  - Strategy Manager │   │ │  │  - Strategy Manager │   │           │
│  │  │  - Risk Engine      │   │ │  │  - Risk Engine      │   │           │
│  │  └─────────────────────┘   │ │  └─────────────────────┘   │           │
│  │                             │ │                             │           │
│  │  ┌─────────────────────┐   │ │  ┌─────────────────────┐   │           │
│  │  │    Order WAL        │───┼─┼─▶│    Order WAL        │   │           │
│  │  │  (Write + Replicate)│   │ │  │  (Receive + Apply)  │   │           │
│  │  └─────────────────────┘   │ │  └─────────────────────┘   │           │
│  │                             │ │                             │           │
│  │  ┌─────────────────────┐   │ │  ┌─────────────────────┐   │           │
│  │  │   State Store       │   │ │  │   State Store       │   │           │
│  │  │  - OrderStore       │   │ │  │  - OrderStore       │   │           │
│  │  │  - PositionAggr     │   │ │  │  - PositionAggr     │   │           │
│  │  └─────────────────────┘   │ │  └─────────────────────┘   │           │
│  │                             │ │                             │           │
│  │  PVC: veloz-data-0         │ │  PVC: veloz-data-1         │           │
│  └─────────────────────────────┘ └─────────────────────────────┘           │
│                                                                              │
│  ┌─────────────────────────────────────────────────────────────────────┐    │
│  │                        Leader Election Service                       │    │
│  │                    (Kubernetes Lease / etcd)                         │    │
│  └─────────────────────────────────────────────────────────────────────┘    │
│                                                                              │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

## 3. State Replication

### 3.1 Replication Model

VeloZ uses **synchronous WAL replication** for critical state:

```
┌──────────────┐     1. Write WAL      ┌──────────────┐
│   Primary    │ ──────────────────▶   │   WAL File   │
│   Engine     │                       │   (Local)    │
└──────────────┘                       └──────────────┘
       │                                      │
       │  2. Replicate                        │
       ▼                                      ▼
┌──────────────┐     3. Receive        ┌──────────────┐
│   Standby    │ ◀──────────────────   │   WAL File   │
│   Engine     │                       │   (Remote)   │
└──────────────┘                       └──────────────┘
       │
       │  4. Apply to State
       ▼
┌──────────────┐
│  OrderStore  │
│  (Standby)   │
└──────────────┘
```

### 3.2 Replication Protocol

**WAL Entry Replication Flow:**

1. Primary writes WAL entry locally
2. Primary sends entry to standby via gRPC stream
3. Standby acknowledges receipt
4. Primary commits transaction (for sync mode)
5. Standby applies entry to local state

### 3.3 Replication Modes

| Mode | Durability | Latency | Use Case |
|------|------------|---------|----------|
| **Synchronous** | Highest | +2-5ms | Critical orders, position changes |
| **Asynchronous** | High | +0ms | Market data, non-critical state |
| **Checkpoint** | Medium | Periodic | Full state sync, recovery |

### 3.4 WAL Replication Service

Create `libs/core/include/veloz/core/replication.h`:

```cpp
#pragma once

#include "veloz/oms/order_wal.h"

#include <kj/async-io.h>
#include <kj/compat/http.h>

namespace veloz::core {

// Replication mode
enum class ReplicationMode {
    Synchronous,   // Wait for standby ACK before commit
    Asynchronous,  // Fire and forget
    Disabled       // No replication (standalone)
};

// Replication configuration
struct ReplicationConfig {
    ReplicationMode mode{ReplicationMode::Asynchronous};
    kj::String standby_address;           // e.g., "standby-engine:9090"
    kj::Duration ack_timeout{100 * kj::MILLISECONDS};
    kj::Duration reconnect_interval{5 * kj::SECONDS};
    uint32_t max_pending_entries{1000};   // Buffer size for async mode
    bool compress_entries{true};          // Compress WAL entries
};

// Replication statistics
struct ReplicationStats {
    uint64_t entries_sent{0};
    uint64_t entries_acked{0};
    uint64_t entries_failed{0};
    uint64_t bytes_sent{0};
    kj::Duration avg_latency{0 * kj::NANOSECONDS};
    kj::Duration max_latency{0 * kj::NANOSECONDS};
    uint64_t reconnect_count{0};
    bool connected{false};
    uint64_t lag_entries{0};  // Entries pending replication
};

// WAL entry for replication
struct ReplicatedEntry {
    uint64_t sequence;
    oms::WalEntryType type;
    kj::Array<kj::byte> payload;
    int64_t timestamp_ns;
};

// Replication sender (runs on primary)
class ReplicationSender {
public:
    ReplicationSender(kj::AsyncIoContext& io, ReplicationConfig config);

    // Start replication connection
    kj::Promise<void> start();

    // Stop replication
    void stop();

    // Send WAL entry to standby
    kj::Promise<bool> replicate(const ReplicatedEntry& entry);

    // Get replication statistics
    [[nodiscard]] ReplicationStats stats() const;

    // Check if connected to standby
    [[nodiscard]] bool is_connected() const;

    // Get current replication lag
    [[nodiscard]] uint64_t lag() const;

private:
    kj::Promise<void> connect_loop();
    kj::Promise<void> send_loop();
    kj::Promise<bool> send_entry(const ReplicatedEntry& entry);

    kj::AsyncIoContext& io_;
    ReplicationConfig config_;
    kj::MutexGuarded<ReplicationStats> stats_;
    // ... implementation details
};

// Replication receiver (runs on standby)
class ReplicationReceiver {
public:
    using ApplyCallback = kj::Function<void(const ReplicatedEntry&)>;

    ReplicationReceiver(kj::AsyncIoContext& io, uint16_t port);

    // Start listening for replication
    kj::Promise<void> start(ApplyCallback callback);

    // Stop receiving
    void stop();

    // Get last received sequence
    [[nodiscard]] uint64_t last_sequence() const;

    // Get receiver statistics
    [[nodiscard]] ReplicationStats stats() const;

private:
    kj::Promise<void> handle_connection(kj::Own<kj::AsyncIoStream> stream);

    kj::AsyncIoContext& io_;
    uint16_t port_;
    kj::MutexGuarded<ReplicationStats> stats_;
    // ... implementation details
};

} // namespace veloz::core
```

### 3.5 Integrating with Existing OrderWal

Extend the existing `OrderWal` to support replication:

```cpp
// In libs/oms/include/veloz/oms/order_wal.h

class OrderWal final {
public:
    // ... existing methods ...

    // Set replication sender (for primary)
    void set_replication_sender(core::ReplicationSender* sender);

    // Enable replication receiver mode (for standby)
    void enable_receiver_mode(core::ReplicationReceiver* receiver);

private:
    // Modified write_entry to replicate
    uint64_t write_entry(WalEntryType type, kj::ArrayPtr<const kj::byte> payload) {
        // ... existing write logic ...

        // Replicate if sender is configured
        if (replication_sender_ && replication_sender_->is_connected()) {
            ReplicatedEntry entry{sequence, type, payload.clone(), timestamp_ns};
            if (config_.replication_mode == ReplicationMode::Synchronous) {
                // Wait for ACK
                replication_sender_->replicate(entry).wait(io_);
            } else {
                // Fire and forget
                replication_sender_->replicate(entry).detach([](auto&&){});
            }
        }

        return sequence;
    }

    core::ReplicationSender* replication_sender_{nullptr};
    core::ReplicationReceiver* replication_receiver_{nullptr};
};
```

---

## 4. Leader Election

### 4.1 Kubernetes Lease-Based Election

Use Kubernetes Lease objects for leader election:

```yaml
# infra/k8s/ha/leader-election.yaml
apiVersion: coordination.k8s.io/v1
kind: Lease
metadata:
  name: veloz-engine-leader
  namespace: veloz
spec:
  holderIdentity: ""
  leaseDurationSeconds: 15
  acquireTime: null
  renewTime: null
  leaseTransitions: 0
```

### 4.2 Leader Election Service

Create `libs/core/include/veloz/core/leader_election.h`:

```cpp
#pragma once

#include <kj/async.h>
#include <kj/function.h>
#include <kj/string.h>
#include <kj/time.h>

namespace veloz::core {

// Leader election configuration
struct LeaderElectionConfig {
    kj::String lease_name{"veloz-engine-leader"};
    kj::String namespace_{"veloz"};
    kj::String identity;  // This instance's identity (e.g., pod name)
    kj::Duration lease_duration{15 * kj::SECONDS};
    kj::Duration renew_deadline{10 * kj::SECONDS};
    kj::Duration retry_period{2 * kj::SECONDS};
};

// Leader election callbacks
struct LeaderCallbacks {
    kj::Function<void()> on_started_leading;   // Called when we become leader
    kj::Function<void()> on_stopped_leading;   // Called when we lose leadership
    kj::Function<void(kj::StringPtr)> on_new_leader;  // Called when leader changes
};

// Leader election state
enum class LeaderState {
    Unknown,
    Leader,
    Follower,
    Candidate
};

// Leader election service
class LeaderElection {
public:
    LeaderElection(LeaderElectionConfig config, LeaderCallbacks callbacks);

    // Start leader election loop
    kj::Promise<void> run();

    // Stop leader election
    void stop();

    // Check if this instance is the leader
    [[nodiscard]] bool is_leader() const;

    // Get current leader identity
    [[nodiscard]] kj::Maybe<kj::StringPtr> current_leader() const;

    // Get current state
    [[nodiscard]] LeaderState state() const;

    // Voluntarily step down as leader
    void step_down();

private:
    kj::Promise<void> try_acquire_or_renew();
    kj::Promise<bool> acquire_lease();
    kj::Promise<bool> renew_lease();
    void release_lease();

    LeaderElectionConfig config_;
    LeaderCallbacks callbacks_;
    kj::MutexGuarded<LeaderState> state_{LeaderState::Unknown};
    kj::Maybe<kj::String> current_leader_;
    bool should_stop_{false};
};

} // namespace veloz::core
```

### 4.3 Leader Election Flow

```
┌─────────────────────────────────────────────────────────────────┐
│                      Leader Election Flow                        │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│  ┌──────────┐    Acquire Lease    ┌──────────┐                  │
│  │ Candidate│ ─────────────────▶  │  Leader  │                  │
│  └──────────┘                     └──────────┘                  │
│       ▲                                │                         │
│       │                                │ Lease Expires /         │
│       │ Start                          │ Step Down               │
│       │                                ▼                         │
│  ┌──────────┐    Lease Held       ┌──────────┐                  │
│  │ Follower │ ◀─────────────────  │ Follower │                  │
│  └──────────┘    by Another       └──────────┘                  │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

### 4.4 Integration with Engine

```cpp
// In apps/engine/src/engine_app.cpp

class EngineApp {
public:
    void run_ha_mode() {
        // Initialize leader election
        LeaderElectionConfig config{
            .identity = get_pod_name(),
            .lease_duration = 15 * kj::SECONDS
        };

        LeaderCallbacks callbacks{
            .on_started_leading = [this]() {
                become_primary();
            },
            .on_stopped_leading = [this]() {
                become_standby();
            },
            .on_new_leader = [this](kj::StringPtr leader) {
                update_leader_info(leader);
            }
        };

        leader_election_ = kj::heap<LeaderElection>(config, kj::mv(callbacks));
        leader_election_->run().wait(io_.waitScope);
    }

private:
    void become_primary() {
        // Start accepting traffic
        // Enable WAL replication sender
        // Start strategies
        LOG_INFO("Became primary");
    }

    void become_standby() {
        // Stop accepting traffic
        // Enable WAL replication receiver
        // Pause strategies
        LOG_INFO("Became standby");
    }
};
```

---

## 5. Failover Mechanism

### 5.1 Failure Detection

| Failure Type | Detection Method | Detection Time |
|--------------|------------------|----------------|
| Process crash | Kubernetes liveness probe | 30-60s |
| Network partition | Lease renewal failure | 15-30s |
| Hung process | Kubernetes readiness probe | 10-20s |
| Graceful shutdown | Voluntary step-down | Immediate |

### 5.2 Health Probes

```cpp
// Health check endpoints for Kubernetes

// Liveness probe: Is the process alive?
// GET /health/live
bool is_alive() {
    return event_loop_running_ && !fatal_error_;
}

// Readiness probe: Can we accept traffic?
// GET /health/ready
bool is_ready() {
    return is_alive() &&
           leader_election_->is_leader() &&
           exchange_connected_ &&
           replication_caught_up_;
}

// Startup probe: Has initialization completed?
// GET /health/startup
bool is_started() {
    return initialization_complete_ && wal_replay_complete_;
}
```

### 5.3 Kubernetes Probe Configuration

```yaml
# infra/k8s/ha/deployment.yaml
spec:
  template:
    spec:
      containers:
        - name: veloz-engine
          livenessProbe:
            httpGet:
              path: /health/live
              port: 8080
            initialDelaySeconds: 10
            periodSeconds: 10
            failureThreshold: 3

          readinessProbe:
            httpGet:
              path: /health/ready
              port: 8080
            initialDelaySeconds: 5
            periodSeconds: 5
            failureThreshold: 2

          startupProbe:
            httpGet:
              path: /health/startup
              port: 8080
            initialDelaySeconds: 0
            periodSeconds: 5
            failureThreshold: 30  # Allow up to 150s for startup
```

### 5.4 Failover Sequence

```
Time    Primary                 Standby                 K8s/Lease
─────────────────────────────────────────────────────────────────
T+0     Running (Leader)        Running (Follower)      Lease: Primary
        ▼ Crash
T+0     DEAD                    Running (Follower)      Lease: Primary

T+15    -                       Lease expired           Lease: Expired
                                Try acquire lease

T+16    -                       Acquired lease!         Lease: Standby
                                on_started_leading()
                                become_primary()

T+17    -                       Primary (Leader)        Lease: Standby
                                Accepting traffic
                                Start replication sender

T+30    Pod restarting          Primary (Leader)        Lease: Standby

T+45    Running (Follower)      Primary (Leader)        Lease: Standby
        on_new_leader()
        become_standby()
        Start replication receiver
```

### 5.5 Split-Brain Prevention

To prevent split-brain scenarios:

1. **Lease-based fencing**: Only the lease holder can accept writes
2. **Epoch numbers**: Each leader election increments an epoch
3. **Exchange connection check**: Verify exchange connectivity before accepting orders
4. **Replication lag check**: Don't become primary if significantly behind

```cpp
bool can_become_primary() {
    // Check replication lag
    if (replication_lag() > MAX_LAG_ENTRIES) {
        LOG_WARN("Cannot become primary: replication lag too high");
        return false;
    }

    // Check exchange connectivity
    if (!exchange_adapter_->is_connected()) {
        LOG_WARN("Cannot become primary: exchange not connected");
        return false;
    }

    // Check WAL integrity
    if (!wal_->is_healthy()) {
        LOG_WARN("Cannot become primary: WAL unhealthy");
        return false;
    }

    return true;
}
```

---

## 6. Recovery Procedures

### 6.1 Startup Recovery

When an engine starts, it must recover state from WAL:

```cpp
void EngineApp::recover_state() {
    LOG_INFO("Starting state recovery from WAL");

    // 1. Replay WAL into OrderStore
    wal_->replay_into(order_store_);

    // 2. Reconcile with exchange
    reconcile_with_exchange();

    // 3. Rebuild derived state
    rebuild_position_aggregator();
    rebuild_risk_state();

    // 4. Mark recovery complete
    recovery_complete_ = true;

    LOG_INFO("State recovery complete",
             "orders", order_store_.count(),
             "sequence", wal_->current_sequence());
}
```

### 6.2 Standby Catch-Up

When a standby falls behind, it must catch up:

```cpp
kj::Promise<void> catch_up_from_primary() {
    // 1. Request checkpoint from primary
    auto checkpoint = co_await request_checkpoint();

    // 2. Apply checkpoint
    wal_->apply_checkpoint(checkpoint);

    // 3. Request entries since checkpoint
    auto entries = co_await request_entries_since(checkpoint.sequence);

    // 4. Apply entries
    for (const auto& entry : entries) {
        wal_->apply_replicated_entry(entry);
    }

    // 5. Switch to streaming replication
    co_await start_streaming_replication();
}
```

### 6.3 Manual Recovery Procedures

**Scenario: Both nodes failed**

```bash
# 1. Identify the node with latest WAL
kubectl exec veloz-engine-0 -- veloz-cli wal-status
kubectl exec veloz-engine-1 -- veloz-cli wal-status

# 2. Start the node with latest WAL as primary
kubectl label pod veloz-engine-0 veloz.io/force-leader=true

# 3. Wait for recovery
kubectl logs -f veloz-engine-0

# 4. Start standby
kubectl label pod veloz-engine-1 veloz.io/force-leader-

# 5. Verify replication
kubectl exec veloz-engine-0 -- veloz-cli replication-status
```

**Scenario: WAL corruption**

```bash
# 1. Stop the affected node
kubectl scale statefulset veloz-engine --replicas=1

# 2. Request full checkpoint from healthy node
kubectl exec veloz-engine-0 -- veloz-cli checkpoint --output=/data/checkpoint.bin

# 3. Copy checkpoint to affected node
kubectl cp veloz-engine-0:/data/checkpoint.bin /tmp/checkpoint.bin
kubectl cp /tmp/checkpoint.bin veloz-engine-1:/data/checkpoint.bin

# 4. Restore from checkpoint
kubectl exec veloz-engine-1 -- veloz-cli restore --input=/data/checkpoint.bin

# 5. Restart cluster
kubectl scale statefulset veloz-engine --replicas=2
```

---

## 7. Kubernetes Deployment

### 7.1 StatefulSet Configuration

```yaml
# infra/k8s/ha/statefulset.yaml
apiVersion: apps/v1
kind: StatefulSet
metadata:
  name: veloz-engine
  namespace: veloz
spec:
  serviceName: veloz-engine-headless
  replicas: 2
  podManagementPolicy: Parallel
  selector:
    matchLabels:
      app: veloz-engine
  template:
    metadata:
      labels:
        app: veloz-engine
    spec:
      terminationGracePeriodSeconds: 30
      serviceAccountName: veloz-engine

      containers:
        - name: veloz-engine
          image: veloz-engine:latest
          args:
            - --ha-mode
            - --replication-port=9090
          ports:
            - name: api
              containerPort: 8080
            - name: replication
              containerPort: 9090
          env:
            - name: POD_NAME
              valueFrom:
                fieldRef:
                  fieldPath: metadata.name
            - name: VELOZ_HA_ENABLED
              value: "true"
            - name: VELOZ_REPLICATION_MODE
              value: "synchronous"
          volumeMounts:
            - name: data
              mountPath: /data
          resources:
            requests:
              memory: "512Mi"
              cpu: "500m"
            limits:
              memory: "1Gi"
              cpu: "1"

  volumeClaimTemplates:
    - metadata:
        name: data
      spec:
        accessModes: ["ReadWriteOnce"]
        storageClassName: fast-ssd
        resources:
          requests:
            storage: 10Gi
---
apiVersion: v1
kind: Service
metadata:
  name: veloz-engine-headless
  namespace: veloz
spec:
  clusterIP: None
  selector:
    app: veloz-engine
  ports:
    - name: api
      port: 8080
    - name: replication
      port: 9090
---
apiVersion: v1
kind: Service
metadata:
  name: veloz-engine
  namespace: veloz
spec:
  type: ClusterIP
  selector:
    app: veloz-engine
    veloz.io/role: leader  # Only route to leader
  ports:
    - name: api
      port: 8080
```

### 7.2 RBAC for Leader Election

```yaml
# infra/k8s/ha/rbac.yaml
apiVersion: v1
kind: ServiceAccount
metadata:
  name: veloz-engine
  namespace: veloz
---
apiVersion: rbac.authorization.k8s.io/v1
kind: Role
metadata:
  name: veloz-engine-leader-election
  namespace: veloz
rules:
  - apiGroups: ["coordination.k8s.io"]
    resources: ["leases"]
    verbs: ["get", "create", "update"]
  - apiGroups: [""]
    resources: ["pods"]
    verbs: ["get", "list", "patch"]
---
apiVersion: rbac.authorization.k8s.io/v1
kind: RoleBinding
metadata:
  name: veloz-engine-leader-election
  namespace: veloz
subjects:
  - kind: ServiceAccount
    name: veloz-engine
roleRef:
  kind: Role
  name: veloz-engine-leader-election
  apiGroup: rbac.authorization.k8s.io
```

### 7.3 Pod Disruption Budget

```yaml
# infra/k8s/ha/pdb.yaml
apiVersion: policy/v1
kind: PodDisruptionBudget
metadata:
  name: veloz-engine-pdb
  namespace: veloz
spec:
  minAvailable: 1
  selector:
    matchLabels:
      app: veloz-engine
```

---

## 8. Monitoring and Alerting

### 8.1 HA-Specific Metrics

| Metric | Type | Description |
|--------|------|-------------|
| `veloz_ha_leader` | Gauge | 1 if this instance is leader, 0 otherwise |
| `veloz_ha_leader_transitions_total` | Counter | Number of leader transitions |
| `veloz_ha_replication_lag_entries` | Gauge | Entries behind primary |
| `veloz_ha_replication_lag_seconds` | Gauge | Time behind primary |
| `veloz_ha_replication_connected` | Gauge | 1 if connected to peer |
| `veloz_ha_failover_total` | Counter | Number of failovers |
| `veloz_ha_failover_duration_seconds` | Histogram | Time to complete failover |

### 8.2 Alert Rules

```yaml
# infra/config/prometheus-rules-ha.yaml
groups:
  - name: veloz-ha
    rules:
      # Critical: No leader
      - alert: NoLeader
        expr: sum(veloz_ha_leader) == 0
        for: 30s
        labels:
          severity: critical
        annotations:
          summary: "No VeloZ leader elected"
          description: "No engine instance is currently the leader"

      # Critical: Multiple leaders (split-brain)
      - alert: SplitBrain
        expr: sum(veloz_ha_leader) > 1
        for: 10s
        labels:
          severity: critical
        annotations:
          summary: "Split-brain detected"
          description: "Multiple engine instances claim to be leader"

      # High: Replication lag
      - alert: ReplicationLagHigh
        expr: veloz_ha_replication_lag_seconds > 5
        for: 1m
        labels:
          severity: high
        annotations:
          summary: "Replication lag > 5 seconds"
          description: "Standby is {{ $value }}s behind primary"

      # High: Replication disconnected
      - alert: ReplicationDisconnected
        expr: veloz_ha_replication_connected == 0
        for: 1m
        labels:
          severity: high
        annotations:
          summary: "Replication disconnected"
          description: "Standby is not receiving replication from primary"

      # Medium: Frequent failovers
      - alert: FrequentFailovers
        expr: increase(veloz_ha_failover_total[1h]) > 3
        for: 0s
        labels:
          severity: medium
        annotations:
          summary: "Frequent failovers detected"
          description: "{{ $value }} failovers in the last hour"
```

### 8.3 Grafana Dashboard

Key panels for HA dashboard:

1. **Leader Status**: Which instance is leader
2. **Replication Lag**: Time and entry lag
3. **Failover History**: Timeline of failovers
4. **WAL Sequence**: Primary vs standby sequence numbers
5. **Health Status**: Liveness/readiness of both instances

---

## 9. Testing

### 9.1 HA Test Scenarios

| Scenario | Test Method | Expected Outcome |
|----------|-------------|------------------|
| Primary crash | `kubectl delete pod veloz-engine-0` | Standby becomes primary < 30s |
| Network partition | Network policy to block replication | Standby detects, doesn't promote |
| Graceful failover | `veloz-cli step-down` | Clean handoff, no data loss |
| Both nodes restart | `kubectl rollout restart` | One becomes primary, other standby |
| WAL corruption | Corrupt WAL file | Detect and alert, don't promote |
| Replication lag | Slow down standby | Alert when lag > threshold |

### 9.2 Chaos Engineering Tests

```yaml
# Chaos test: Kill primary pod
apiVersion: chaos-mesh.org/v1alpha1
kind: PodChaos
metadata:
  name: kill-primary
spec:
  action: pod-kill
  mode: one
  selector:
    labelSelectors:
      app: veloz-engine
      veloz.io/role: leader
  scheduler:
    cron: "@every 1h"
```

### 9.3 Integration Tests

```cpp
// tests/integration/test_ha_failover.cpp

KJ_TEST("HA failover completes within 30 seconds") {
    // Start primary and standby
    auto primary = start_engine(Role::Primary);
    auto standby = start_engine(Role::Standby);

    // Wait for replication to sync
    wait_for_replication_sync(primary, standby);

    // Place some orders on primary
    for (int i = 0; i < 100; i++) {
        primary->place_order(create_test_order());
    }

    // Kill primary
    auto kill_time = kj::systemPreciseMonotonicClock().now();
    primary->kill();

    // Wait for standby to become primary
    wait_for_leader(standby, 30 * kj::SECONDS);

    // Verify failover time
    auto failover_time = kj::systemPreciseMonotonicClock().now() - kill_time;
    KJ_EXPECT(failover_time < 30 * kj::SECONDS);

    // Verify all orders are present
    auto orders = standby->list_orders();
    KJ_EXPECT(orders.size() == 100);
}

KJ_TEST("No data loss during failover") {
    // ... similar test verifying data integrity
}

KJ_TEST("Split-brain prevention works") {
    // ... test that only one leader can exist
}
```

---

## 10. Implementation Plan

### 10.1 Phase 1: WAL Replication (Task #4 prerequisite)

1. Create `ReplicationSender` and `ReplicationReceiver` classes
2. Integrate with existing `OrderWal`
3. Add replication metrics
4. Test replication latency and throughput

### 10.2 Phase 2: Leader Election (Task #7 core)

1. Implement Kubernetes lease-based leader election
2. Create health check endpoints
3. Integrate with engine startup/shutdown
4. Test failover scenarios

### 10.3 Phase 3: Kubernetes Deployment (Task #7 + #9)

1. Create StatefulSet configuration
2. Configure RBAC for leader election
3. Set up headless service for replication
4. Configure PodDisruptionBudget

### 10.4 Phase 4: Testing and Validation (Task #17)

1. Create integration tests for failover
2. Set up chaos engineering tests
3. Validate RTO and RPO targets
4. Document runbooks

---

## 11. References

- [Kubernetes Leader Election](https://kubernetes.io/docs/concepts/architecture/leases/)
- [PostgreSQL Streaming Replication](https://www.postgresql.org/docs/current/warm-standby.html) (inspiration)
- [Raft Consensus Algorithm](https://raft.github.io/) (alternative approach)
- [VeloZ Order WAL](../libs/oms/include/veloz/oms/order_wal.h)

---

## Appendix A: Configuration Reference

```yaml
# veloz-ha-config.yaml
ha:
  enabled: true
  mode: active-passive

replication:
  mode: synchronous  # synchronous | asynchronous
  port: 9090
  ack_timeout_ms: 100
  reconnect_interval_s: 5
  max_pending_entries: 1000
  compress: true

leader_election:
  lease_name: veloz-engine-leader
  namespace: veloz
  lease_duration_s: 15
  renew_deadline_s: 10
  retry_period_s: 2

failover:
  max_replication_lag_entries: 100
  max_replication_lag_s: 5
  require_exchange_connection: true
  require_healthy_wal: true

recovery:
  wal_replay_on_startup: true
  reconcile_with_exchange: true
  checkpoint_on_promotion: true
```

## Appendix B: CLI Commands

```bash
# Check HA status
veloz-cli ha-status

# Force leadership (emergency)
veloz-cli force-leader --confirm

# Step down as leader
veloz-cli step-down

# Check replication status
veloz-cli replication-status

# Create checkpoint
veloz-cli checkpoint --output=/data/checkpoint.bin

# Restore from checkpoint
veloz-cli restore --input=/data/checkpoint.bin

# Check WAL status
veloz-cli wal-status
```
