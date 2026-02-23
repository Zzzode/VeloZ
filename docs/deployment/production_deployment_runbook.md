# VeloZ Production Deployment Runbook

This document provides comprehensive procedures for production deployment, validation, and rollback.

## Table of Contents

1. [Pre-Deployment Checklist](#1-pre-deployment-checklist)
2. [Blue-Green Deployment Steps](#2-blue-green-deployment-steps)
3. [Post-Deployment Validation](#3-post-deployment-validation)
4. [Rollback Procedure](#4-rollback-procedure)
5. [Emergency Contacts](#5-emergency-contacts)

---

## 1. Pre-Deployment Checklist

### 1.1 Release Readiness

Complete all items before proceeding with deployment:

#### Code Quality
- [ ] All CI tests passing on release branch
- [ ] Code review approved by at least 2 reviewers
- [ ] Security scan completed with no critical/high vulnerabilities
- [ ] Performance benchmarks meet SLO targets
- [ ] Release notes documented

#### Infrastructure
- [ ] Kubernetes cluster healthy (`kubectl get nodes`)
- [ ] Sufficient cluster resources available
- [ ] Database connections verified
- [ ] Redis cluster healthy
- [ ] Network policies in place

#### Backup & Recovery
- [ ] Fresh database backup taken (< 1 hour old)
- [ ] WAL backup current (< 5 minutes old)
- [ ] Backup verification passed
- [ ] DR runbook accessible

#### Monitoring
- [ ] Alerting rules configured
- [ ] Dashboards accessible
- [ ] On-call engineer notified
- [ ] Status page updated (maintenance window)

### 1.2 Pre-Deployment Commands

```bash
# 1. Verify cluster health
kubectl get nodes
kubectl get pods -n veloz -o wide

# 2. Check current deployment
kubectl get deployment veloz-engine -n veloz -o yaml | grep image
helm list -n veloz

# 3. Verify database backup
/opt/veloz/scripts/backup/verify_backup.sh

# 4. Check PostgreSQL replication
kubectl exec -n veloz veloz-postgres-0 -- psql -U veloz -c \
  "SELECT client_addr, state, sync_state FROM pg_stat_replication;"

# 5. Check Redis Sentinel
kubectl exec -n veloz veloz-redis-sentinel-0 -- redis-cli -p 26379 \
  sentinel master veloz-master

# 6. Take pre-deployment snapshot
/opt/veloz/scripts/backup/backup_postgres.sh -t full
/opt/veloz/scripts/backup/backup_wal.sh
```

### 1.3 Deployment Window

| Item | Requirement |
|------|-------------|
| Preferred Time | Weekdays 10:00-14:00 UTC (low trading volume) |
| Maintenance Window | 30 minutes |
| Change Freeze | Avoid Fridays, month-end, major market events |
| Notification | 24 hours advance notice to stakeholders |

---

## 2. Blue-Green Deployment Steps

### 2.1 Deployment Overview

```
                    ┌─────────────────┐
                    │   Load Balancer │
                    │    (Ingress)    │
                    └────────┬────────┘
                             │
              ┌──────────────┴──────────────┐
              │                             │
       ┌──────▼──────┐               ┌──────▼──────┐
       │    BLUE     │               │    GREEN    │
       │  (Current)  │               │    (New)    │
       │   v1.0.0    │               │   v1.1.0    │
       └─────────────┘               └─────────────┘
              │                             │
              └──────────────┬──────────────┘
                             │
                    ┌────────▼────────┐
                    │    Database     │
                    │  (Shared State) │
                    └─────────────────┘
```

### 2.2 Step-by-Step Deployment

#### Step 1: Prepare Green Environment

```bash
# Set deployment variables
export RELEASE_VERSION="1.1.0"
export NAMESPACE="veloz"
export HELM_RELEASE="veloz"

# Verify new image exists
docker pull veloz/veloz-engine:${RELEASE_VERSION}

# Update values file with new version
cat > /tmp/upgrade-values.yaml << EOF
engine:
  image:
    tag: "${RELEASE_VERSION}"
EOF
```

#### Step 2: Deploy Green (Canary)

```bash
# Deploy with canary strategy (1 replica first)
helm upgrade ${HELM_RELEASE} ./infra/helm/veloz \
  -n ${NAMESPACE} \
  -f infra/helm/veloz/values-production.yaml \
  -f /tmp/upgrade-values.yaml \
  --set engine.replicaCount=1 \
  --set autoscaling.enabled=false \
  --wait \
  --timeout 5m

# Verify green pod is running
kubectl get pods -n ${NAMESPACE} -l app.kubernetes.io/name=veloz \
  --sort-by=.metadata.creationTimestamp

# Check green pod logs
kubectl logs -n ${NAMESPACE} -l app.kubernetes.io/name=veloz \
  --tail=50 -f
```

#### Step 3: Validate Green Environment

```bash
# Get green pod name
GREEN_POD=$(kubectl get pods -n ${NAMESPACE} -l app.kubernetes.io/name=veloz \
  --sort-by=.metadata.creationTimestamp -o jsonpath='{.items[-1].metadata.name}')

# Port-forward to green pod for testing
kubectl port-forward -n ${NAMESPACE} ${GREEN_POD} 8081:8080 &
PF_PID=$!

# Run health checks against green
curl -s http://localhost:8081/health | jq .
curl -s http://localhost:8081/health/ready | jq .
curl -s http://localhost:8081/health/startup | jq .

# Run smoke tests
curl -s http://localhost:8081/api/market | jq .
curl -s http://localhost:8081/api/orders_state | jq .

# Stop port-forward
kill $PF_PID
```

#### Step 4: Scale Up Green, Scale Down Blue

```bash
# If validation passed, scale to full replicas
helm upgrade ${HELM_RELEASE} ./infra/helm/veloz \
  -n ${NAMESPACE} \
  -f infra/helm/veloz/values-production.yaml \
  -f /tmp/upgrade-values.yaml \
  --set engine.replicaCount=2 \
  --set autoscaling.enabled=true \
  --wait \
  --timeout 5m

# Verify all pods are running new version
kubectl get pods -n ${NAMESPACE} -l app.kubernetes.io/name=veloz -o wide

# Check rollout status
kubectl rollout status deployment/veloz-engine -n ${NAMESPACE}
```

#### Step 5: Finalize Deployment

```bash
# Verify all pods running new version
kubectl get pods -n ${NAMESPACE} -o jsonpath='{range .items[*]}{.metadata.name}{"\t"}{.spec.containers[0].image}{"\n"}{end}'

# Run full validation suite
./scripts/validate_deployment.sh production

# Update deployment record
echo "$(date -u +%Y-%m-%dT%H:%M:%SZ) - Deployed v${RELEASE_VERSION} to production" >> /var/log/veloz/deployments.log
```

### 2.3 Kubernetes Deployment Commands (Alternative)

```bash
# Using kubectl directly (if not using Helm)
kubectl set image deployment/veloz-engine \
  veloz-engine=veloz/veloz-engine:${RELEASE_VERSION} \
  -n ${NAMESPACE}

# Watch rollout
kubectl rollout status deployment/veloz-engine -n ${NAMESPACE} --watch

# Check rollout history
kubectl rollout history deployment/veloz-engine -n ${NAMESPACE}
```

---

## 3. Post-Deployment Validation

### 3.1 Immediate Validation (0-5 minutes)

```bash
# Health check endpoints
curl -f https://veloz.example.com/health
curl -f https://veloz.example.com/health/ready
curl -f https://veloz.example.com/health/startup

# API functionality
curl -s https://veloz.example.com/api/market | jq '.status'
curl -s https://veloz.example.com/api/orders_state | jq '.count'

# Metrics endpoint
curl -s https://veloz.example.com/metrics | grep veloz_

# Check pod status
kubectl get pods -n veloz -l app.kubernetes.io/name=veloz
```

### 3.2 Functional Validation (5-15 minutes)

```bash
# Test order placement (use test account)
curl -X POST https://veloz.example.com/api/order \
  -H "Content-Type: application/json" \
  -H "Authorization: Bearer ${TEST_TOKEN}" \
  -d '{
    "side": "BUY",
    "symbol": "BTCUSDT",
    "qty": 0.0001,
    "price": 50000.0
  }'

# Verify order appears in state
curl -s https://veloz.example.com/api/orders_state | jq '.orders | length'

# Test SSE stream
timeout 30 curl -N https://veloz.example.com/api/stream

# Test WebSocket connection
wscat -c wss://veloz.example.com/ws/market
```

### 3.3 Monitoring Validation (15-30 minutes)

| Metric | Expected | Alert Threshold |
|--------|----------|-----------------|
| Error rate | < 0.1% | > 1% |
| P99 latency | < 100ms | > 500ms |
| CPU usage | < 50% | > 80% |
| Memory usage | < 60% | > 85% |
| Active connections | Stable | > 1000 |

```bash
# Check Grafana dashboards
open https://grafana.example.com/d/veloz-overview

# Query Prometheus metrics
curl -s "http://prometheus:9090/api/v1/query?query=rate(veloz_http_requests_total[5m])"

# Check for errors in logs
kubectl logs -n veloz -l app.kubernetes.io/name=veloz --since=15m | grep -i error
```

### 3.4 Validation Checklist

- [ ] All health endpoints returning 200
- [ ] API endpoints functional
- [ ] SSE stream delivering events
- [ ] WebSocket connections working
- [ ] Metrics being collected
- [ ] No error spikes in logs
- [ ] Latency within SLO
- [ ] Database connections stable
- [ ] Redis operations working
- [ ] Replication lag < 1MB

---

## 4. Rollback Procedure

### 4.1 Rollback Decision Criteria

Initiate rollback if ANY of the following occur:

| Condition | Threshold | Action |
|-----------|-----------|--------|
| Health check failures | > 3 consecutive | Immediate rollback |
| Error rate | > 5% for 5 minutes | Immediate rollback |
| P99 latency | > 1s for 5 minutes | Evaluate, likely rollback |
| Data inconsistency | Any detected | Immediate rollback |
| Critical bug | User-impacting | Immediate rollback |

### 4.2 Immediate Rollback (< 2 minutes)

```bash
# Option 1: Helm rollback (preferred)
helm rollback ${HELM_RELEASE} -n ${NAMESPACE}

# Option 2: Kubectl rollback
kubectl rollout undo deployment/veloz-engine -n ${NAMESPACE}

# Verify rollback
kubectl rollout status deployment/veloz-engine -n ${NAMESPACE}
kubectl get pods -n ${NAMESPACE} -l app.kubernetes.io/name=veloz
```

### 4.3 Rollback to Specific Version

```bash
# List available revisions
helm history ${HELM_RELEASE} -n ${NAMESPACE}

# Rollback to specific revision
helm rollback ${HELM_RELEASE} 5 -n ${NAMESPACE}

# Or with kubectl
kubectl rollout history deployment/veloz-engine -n ${NAMESPACE}
kubectl rollout undo deployment/veloz-engine -n ${NAMESPACE} --to-revision=3
```

### 4.4 Database Rollback (if needed)

Only if deployment included database migrations that need reverting:

```bash
# 1. Stop application
kubectl scale deployment veloz-engine -n ${NAMESPACE} --replicas=0

# 2. Point-in-time recovery to before deployment
RECOVERY_TIME="2026-02-23T11:55:00Z"  # Time before deployment
/opt/veloz/scripts/backup/restore_postgres.sh -t pitr -T "${RECOVERY_TIME}"

# 3. Restore WAL
/opt/veloz/scripts/backup/restore_wal.sh

# 4. Deploy previous version
helm rollback ${HELM_RELEASE} -n ${NAMESPACE}

# 5. Verify recovery
kubectl scale deployment veloz-engine -n ${NAMESPACE} --replicas=2
curl -f https://veloz.example.com/health
```

### 4.5 Post-Rollback Actions

```bash
# 1. Verify service health
curl -f https://veloz.example.com/health
curl -f https://veloz.example.com/api/orders_state

# 2. Check for data consistency
kubectl exec -n veloz veloz-postgres-0 -- psql -U veloz -c \
  "SELECT count(*) FROM orders WHERE created_at > NOW() - INTERVAL '1 hour';"

# 3. Notify stakeholders
# Send notification to #veloz-ops channel

# 4. Document rollback
echo "$(date -u +%Y-%m-%dT%H:%M:%SZ) - ROLLBACK from v${RELEASE_VERSION}" >> /var/log/veloz/deployments.log

# 5. Create incident ticket
# Link to post-mortem template
```

---

## 5. Emergency Contacts

### 5.1 Escalation Matrix

| Level | Role | Contact | Response Time | Scope |
|-------|------|---------|---------------|-------|
| L1 | On-call Engineer | PagerDuty | 5 minutes | Service restart, basic troubleshooting |
| L2 | Platform Engineer | #veloz-platform | 15 minutes | Deployment issues, infrastructure |
| L3 | Database Admin | #db-oncall | 15 minutes | Database recovery, replication |
| L4 | Engineering Manager | Direct call | 30 minutes | Business impact, resource allocation |
| L5 | VP Engineering | Direct call | 1 hour | Critical incidents, external comms |

### 5.2 Contact Information

| Team | Primary Contact | Backup Contact | Channel |
|------|-----------------|----------------|---------|
| Platform | PagerDuty rotation | @platform-lead | #veloz-platform |
| Database | @dba-oncall | @dba-backup | #db-oncall |
| Security | @security-oncall | @security-lead | #security-ops |
| Trading Ops | @trading-ops | @trading-lead | #trading-ops |

### 5.3 External Contacts

| Service | Support Portal | Emergency Line |
|---------|----------------|----------------|
| AWS | aws.amazon.com/support | Premium Support |
| Binance | binance.com/support | API Support |
| PagerDuty | support.pagerduty.com | N/A |
| Vault | HashiCorp Support | Enterprise Support |

### 5.4 Communication Templates

#### Deployment Start Notification

```
DEPLOYMENT STARTED
Service: VeloZ Production
Version: v1.1.0
Time: 2026-02-23 12:00 UTC
Duration: ~30 minutes
Impact: Minimal (rolling deployment)
Contact: @deployer-name
```

#### Deployment Complete Notification

```
DEPLOYMENT COMPLETE
Service: VeloZ Production
Version: v1.1.0
Time: 2026-02-23 12:25 UTC
Status: SUCCESS
Validation: All checks passed
Release Notes: [link]
```

#### Rollback Notification

```
ROLLBACK INITIATED
Service: VeloZ Production
From Version: v1.1.0
To Version: v1.0.0
Reason: [brief description]
Time: 2026-02-23 12:15 UTC
Status: IN PROGRESS
Incident: [ticket link]
```

---

## Appendix A: Quick Reference Commands

### Deployment

```bash
# Deploy new version
helm upgrade veloz ./infra/helm/veloz -n veloz \
  -f infra/helm/veloz/values-production.yaml \
  --set engine.image.tag=1.1.0

# Check status
kubectl rollout status deployment/veloz-engine -n veloz

# Rollback
helm rollback veloz -n veloz
```

### Health Checks

```bash
# All health endpoints
curl https://veloz.example.com/health
curl https://veloz.example.com/health/ready
curl https://veloz.example.com/health/startup
```

### Logs

```bash
# Recent logs
kubectl logs -n veloz -l app.kubernetes.io/name=veloz --tail=100

# Follow logs
kubectl logs -n veloz -l app.kubernetes.io/name=veloz -f

# Errors only
kubectl logs -n veloz -l app.kubernetes.io/name=veloz | grep -i error
```

### Database

```bash
# Check replication
kubectl exec -n veloz veloz-postgres-0 -- psql -U veloz -c \
  "SELECT * FROM pg_stat_replication;"

# Check connections
kubectl exec -n veloz veloz-postgres-0 -- psql -U veloz -c \
  "SELECT count(*) FROM pg_stat_activity;"
```

---

## Appendix B: Related Documents

- [DR Runbook](../guides/deployment/dr_runbook.md) - Disaster recovery procedures
- [High Availability Guide](../guides/deployment/high_availability.md) - HA architecture
- [Operations Runbook](../guides/deployment/operations_runbook.md) - Day-to-day operations
- [CI/CD Pipeline](../guides/deployment/ci_cd.md) - Automated deployment
- [Monitoring Guide](../guides/deployment/monitoring.md) - Observability setup
- [Troubleshooting](../guides/deployment/troubleshooting.md) - Common issues

---

## Document Information

| Field | Value |
|-------|-------|
| Version | 1.0 |
| Last Updated | 2026-02-23 |
| Owner | Reliability Team |
| Review Cycle | Quarterly |
| Next Review | 2026-05-23 |
