# VeloZ Helm Chart

This directory contains the Helm chart for deploying VeloZ to Kubernetes.

## Prerequisites

- Kubernetes 1.24+
- Helm 3.10+
- kubectl configured to access your cluster

## Quick Start

### Install

```bash
# Add dependencies (if using PostgreSQL/Redis subcharts)
helm dependency update ./veloz

# Install with default values (development)
helm install veloz ./veloz -n veloz --create-namespace

# Install for staging
helm install veloz ./veloz -n veloz-staging --create-namespace -f ./veloz/values-staging.yaml

# Install for production
helm install veloz ./veloz -n veloz-production --create-namespace -f ./veloz/values-production.yaml
```

### Upgrade

```bash
helm upgrade veloz ./veloz -n veloz -f ./veloz/values-production.yaml
```

### Uninstall

```bash
helm uninstall veloz -n veloz
```

## Configuration

See `values.yaml` for all configurable options. Key configurations:

### Engine

| Parameter | Description | Default |
|-----------|-------------|---------|
| `engine.replicaCount` | Number of engine replicas | `1` |
| `engine.image.repository` | Engine image repository | `veloz-engine` |
| `engine.image.tag` | Engine image tag | `latest` |
| `engine.resources.requests.memory` | Memory request | `256Mi` |
| `engine.resources.limits.memory` | Memory limit | `1Gi` |

### Secrets

| Parameter | Description | Default |
|-----------|-------------|---------|
| `secrets.useExternal` | Use external secrets (Vault) | `false` |
| `secrets.externalProvider` | External provider (vault, aws-secrets-manager) | `vault` |
| `secrets.vault.address` | Vault server address | `http://vault:8200` |
| `secrets.vault.path` | Vault secrets path | `secret/data/veloz` |

### Autoscaling

| Parameter | Description | Default |
|-----------|-------------|---------|
| `autoscaling.enabled` | Enable HPA | `false` |
| `autoscaling.minReplicas` | Minimum replicas | `1` |
| `autoscaling.maxReplicas` | Maximum replicas | `3` |
| `autoscaling.targetCPUUtilizationPercentage` | Target CPU utilization | `80` |

### Monitoring

| Parameter | Description | Default |
|-----------|-------------|---------|
| `monitoring.enabled` | Enable monitoring | `true` |
| `monitoring.serviceMonitor.enabled` | Create ServiceMonitor | `true` |
| `monitoring.prometheusRule.enabled` | Create PrometheusRule | `false` |

## Environment-Specific Values

### Development (default)
- Single replica
- No autoscaling
- Inline secrets
- No persistence

### Staging (`values-staging.yaml`)
- Single replica
- External secrets (Vault)
- Persistence enabled
- Network policies enabled

### Production (`values-production.yaml`)
- Multiple replicas with anti-affinity
- Autoscaling enabled (2-10 replicas)
- External secrets (Vault)
- Network policies enabled
- PodDisruptionBudget enabled
- Production-grade resources

## Templates

The chart includes the following templates:

| Template | Description |
|----------|-------------|
| `deployment.yaml` | Main engine deployment |
| `service.yaml` | ClusterIP service |
| `ingress.yaml` | Ingress resource (optional) |
| `configmap.yaml` | Configuration |
| `secret.yaml` | Secrets (if not using external) |
| `serviceaccount.yaml` | Service account |
| `rbac.yaml` | Role and RoleBinding |
| `hpa.yaml` | HorizontalPodAutoscaler |
| `pdb.yaml` | PodDisruptionBudget |
| `networkpolicy.yaml` | NetworkPolicy |
| `servicemonitor.yaml` | Prometheus ServiceMonitor |
| `prometheusrule.yaml` | Prometheus alerting rules |
| `pvc.yaml` | PersistentVolumeClaim |

## Alerts

When `monitoring.prometheusRule.enabled` is true, the following alerts are created:

| Alert | Severity | Description |
|-------|----------|-------------|
| VelozEngineDown | critical | Engine is down for > 1 minute |
| VelozHighLatency | warning | 99th percentile latency > 1 second |
| VelozHighErrorRate | warning | Error rate > 0.1/second |
| VelozHighMemoryUsage | warning | Memory usage > 90% |
| VelozPodRestarting | warning | Pod restarted > 3 times in 1 hour |

## Troubleshooting

### Check pod status
```bash
kubectl get pods -n veloz -l app.kubernetes.io/name=veloz
```

### View logs
```bash
kubectl logs -n veloz -l app.kubernetes.io/name=veloz -f
```

### Check events
```bash
kubectl get events -n veloz --sort-by='.lastTimestamp'
```

### Debug deployment
```bash
kubectl describe deployment veloz -n veloz
```
