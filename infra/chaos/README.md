# VeloZ Chaos Engineering

This directory contains chaos engineering configurations and experiments for testing VeloZ system resilience.

## Overview

Chaos engineering helps us:
- Discover weaknesses before they cause outages
- Build confidence in system resilience
- Validate recovery procedures
- Document failure modes

## Framework

We use **Chaos Mesh** for Kubernetes-based chaos experiments and custom Python scripts for Docker/local testing.

## Quick Start

### Kubernetes (Chaos Mesh)

```bash
# Install Chaos Mesh
kubectl apply -f https://mirrors.chaos-mesh.org/v2.6.2/install.yaml

# Apply VeloZ chaos experiments
kubectl apply -f infra/chaos/experiments/
```

### Docker/Local

```bash
# Run chaos tests
python infra/chaos/chaos_runner.py --scenario network-partition

# Run all scenarios
python infra/chaos/chaos_runner.py --all
```

## Experiments

| Experiment | Description | Target |
|------------|-------------|--------|
| `pod-kill` | Kill gateway/engine pods | Kubernetes |
| `network-partition` | Isolate services | All |
| `network-latency` | Add network delay | All |
| `cpu-stress` | CPU exhaustion | All |
| `memory-stress` | Memory pressure | All |
| `disk-fill` | Fill disk space | All |
| `exchange-failure` | Simulate exchange outage | Gateway |
| `database-failure` | Database connection loss | Gateway |

## Safety

### Blast Radius Control

- All experiments have timeouts
- Experiments target specific namespaces/containers
- Automatic rollback on failure

### Prerequisites

Before running chaos experiments:
1. Ensure monitoring is active
2. Verify alerting is configured
3. Have runbooks ready
4. Notify team members

## Documentation

- `experiments/` - Chaos Mesh experiment definitions
- `scenarios/` - Test scenario configurations
- `docs/FAILURE_MODES.md` - Documented failure modes
- `docs/RUNBOOK.md` - Chaos testing runbook
