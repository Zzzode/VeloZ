# VeloZ On-Call Handbook

This handbook provides guidance for on-call engineers supporting VeloZ in production.

## On-Call Responsibilities

### Primary Responsibilities

1. **Monitor alerts** - Respond to PagerDuty/Opsgenie alerts within SLA
2. **Triage incidents** - Assess severity and escalate as needed
3. **Restore service** - Take immediate action to restore service
4. **Communicate** - Keep stakeholders informed
5. **Document** - Record all actions and findings

### Response Time SLAs

| Severity | Response Time | Resolution Target |
|----------|---------------|-------------------|
| P1 (Critical) | 5 minutes | 30 minutes |
| P2 (High) | 15 minutes | 2 hours |
| P3 (Medium) | 1 hour | 8 hours |
| P4 (Low) | 4 hours | 24 hours |

## Alert Response Guide

### P1 - Critical Alerts

**Indicators:**
- Service completely down
- Data loss or corruption
- Security breach
- Complete trading halt

**Immediate Actions:**
1. Acknowledge alert immediately
2. Start incident bridge/channel
3. Notify engineering manager
4. Begin diagnosis
5. Apply emergency fix or rollback

**Example Alerts:**
- `VelozGatewayDown`
- `VelozWebSocketDisconnected`
- `VelozCriticalDrawdown`
- `VelozVaRBreach`

### P2 - High Severity Alerts

**Indicators:**
- Degraded performance
- Partial service outage
- High error rates
- Risk threshold breaches

**Immediate Actions:**
1. Acknowledge alert within 15 minutes
2. Assess impact scope
3. Begin diagnosis
4. Escalate if needed

**Example Alerts:**
- `VelozHighLatency`
- `VelozHighErrorRate`
- `VelozHighDrawdown`
- `VelozHighLeverage`

### P3 - Medium Severity Alerts

**Indicators:**
- Warning thresholds exceeded
- Non-critical component issues
- Performance degradation

**Actions:**
1. Acknowledge alert within 1 hour
2. Investigate root cause
3. Plan remediation
4. Schedule fix if not urgent

**Example Alerts:**
- `VelozMemoryPressure`
- `VelozHighConcentration`
- `VelozLowWinRate`

### P4 - Low Severity Alerts

**Indicators:**
- Informational alerts
- Minor issues
- Maintenance reminders

**Actions:**
1. Review during business hours
2. Create ticket if needed
3. Plan remediation

## Escalation Procedures

### Escalation Matrix

| Level | Role | Contact Method | When to Escalate |
|-------|------|----------------|------------------|
| L1 | On-call Engineer | PagerDuty | First responder |
| L2 | Senior Engineer | Slack #veloz-oncall | After 15 min without resolution |
| L3 | Tech Lead | Direct call | After 30 min or P1 incident |
| L4 | Engineering Manager | Direct call | After 1 hour or customer impact |
| L5 | VP Engineering | Direct call | Major outage or data loss |

### When to Escalate

**Escalate immediately if:**
- You cannot diagnose the issue within 15 minutes
- The issue requires access you don't have
- Multiple systems are affected
- Customer-facing impact is growing
- You need additional expertise

**How to escalate:**
1. Page the next level via PagerDuty
2. Provide brief summary in Slack
3. Share relevant logs and metrics
4. Stay on the incident until handoff

## Common Scenarios

### Scenario 1: Gateway Not Responding

**Symptoms:**
- Health check failing
- API requests timing out
- Alert: `VelozGatewayDown`

**Diagnosis:**
```bash
# Check service status
systemctl status veloz

# Check process
ps aux | grep veloz

# Check logs
journalctl -u veloz -n 100 --no-pager

# Check port
ss -tlnp | grep 8080
```

**Resolution:**
```bash
# Restart service
systemctl restart veloz

# If restart fails, check logs for root cause
journalctl -u veloz -n 500 --no-pager | grep -i error

# If still failing, restore from backup
/opt/veloz/scripts/backup/restore_wal.sh
systemctl restart veloz
```

### Scenario 2: High Latency

**Symptoms:**
- Slow API responses
- Alert: `VelozHighLatency`

**Diagnosis:**
```bash
# Check latency metrics
curl -s http://localhost:8080/metrics | grep latency

# Check event loop
curl -s http://localhost:8080/metrics | grep event_loop

# Check system resources
top -bn1 | head -20
free -h
```

**Resolution:**
```bash
# If event loop backlog
# Restart to clear queue
systemctl restart veloz

# If resource exhaustion
# Scale horizontally or increase resources
kubectl scale deployment veloz-gateway --replicas=5
```

### Scenario 3: WebSocket Disconnection

**Symptoms:**
- No market data updates
- Alert: `VelozWebSocketDisconnected`

**Diagnosis:**
```bash
# Check WebSocket metrics
curl -s http://localhost:8080/metrics | grep websocket

# Check network connectivity
ping api.binance.com
curl https://api.binance.com/api/v3/ping

# Check logs for reconnection attempts
journalctl -u veloz | grep -i websocket
```

**Resolution:**
```bash
# Usually auto-reconnects
# If not, restart service
systemctl restart veloz

# If network issue, check firewall
sudo ufw status
```

### Scenario 4: High Drawdown Alert

**Symptoms:**
- Portfolio losses exceeding threshold
- Alert: `VelozHighDrawdown` or `VelozCriticalDrawdown`

**Diagnosis:**
```bash
# Check current drawdown
curl -s http://localhost:8080/metrics | grep drawdown

# Check positions
curl http://localhost:8080/api/positions

# Check recent trades
curl http://localhost:8080/api/trades?limit=20
```

**Resolution:**
1. **For warning (>10%):**
   - Monitor closely
   - Review market conditions
   - Notify trading team

2. **For critical (>20%):**
   - Consider halting new orders
   - Notify management immediately
   - Review risk limits
   ```bash
   # Put in read-only mode if supported
   curl -X POST http://localhost:8080/api/admin/readonly
   ```

### Scenario 5: Database Connection Issues

**Symptoms:**
- Database errors in logs
- Order state not persisting

**Diagnosis:**
```bash
# Check PostgreSQL status
systemctl status postgresql
pg_isready

# Check connections
psql -U veloz -c "SELECT count(*) FROM pg_stat_activity;"

# Check logs
tail -100 /var/log/postgresql/postgresql-*.log
```

**Resolution:**
```bash
# Restart PostgreSQL
systemctl restart postgresql

# If connection exhaustion
# Kill idle connections
psql -U postgres -c "SELECT pg_terminate_backend(pid)
                     FROM pg_stat_activity
                     WHERE state = 'idle'
                     AND query_start < now() - interval '1 hour';"
```

## Useful Commands Reference

### Service Management

```bash
# Start/stop/restart
systemctl start|stop|restart veloz

# Check status
systemctl status veloz

# View logs
journalctl -u veloz -f
journalctl -u veloz -n 100 --no-pager
journalctl -u veloz --since "1 hour ago"
```

### Health Checks

```bash
# Basic health
curl http://localhost:8080/health

# Detailed health
curl http://localhost:8080/health/ready

# Metrics
curl http://localhost:8080/metrics
```

### Diagnostics

```bash
# Check processes
ps aux | grep veloz
top -p $(pgrep veloz_engine)

# Check network
ss -tlnp | grep 8080
netstat -an | grep 8080

# Check resources
free -h
df -h
iostat -x 1 5
```

### Log Analysis

```bash
# Recent errors
journalctl -u veloz | grep -i error | tail -50

# WebSocket issues
journalctl -u veloz | grep -i websocket | tail -50

# Order issues
journalctl -u veloz | grep -i order | tail -50
```

## On-Call Checklist

### Start of Shift

- [ ] Check PagerDuty for active incidents
- [ ] Review recent alerts (last 24 hours)
- [ ] Check service health dashboards
- [ ] Review handoff notes from previous shift
- [ ] Verify access to all systems

### During Shift

- [ ] Respond to alerts within SLA
- [ ] Document all incidents
- [ ] Update status page as needed
- [ ] Communicate with stakeholders

### End of Shift

- [ ] Hand off active incidents
- [ ] Document any ongoing issues
- [ ] Update runbooks if needed
- [ ] Notify next on-call of any concerns

## Contact Information

### Internal Contacts

| Role | Slack Channel | PagerDuty |
|------|---------------|-----------|
| On-call | #veloz-oncall | veloz-oncall |
| Database | #db-oncall | db-oncall |
| Infrastructure | #infra-oncall | infra-oncall |
| Security | #security-oncall | security-oncall |

### External Contacts

| Service | Support URL | Status Page |
|---------|-------------|-------------|
| Binance | support.binance.com | status.binance.com |
| AWS | aws.amazon.com/support | status.aws.amazon.com |
| PagerDuty | support.pagerduty.com | status.pagerduty.com |

## Related Documents

- [Operations Runbook](operations_runbook.md)
- [Troubleshooting Guide](troubleshooting.md)
- [DR Runbook](dr_runbook.md)
- [Monitoring](monitoring.md)
- [Incident Response](incident_response.md)
- [User Troubleshooting Guide](../user/troubleshooting.md) - User-facing troubleshooting
- [Glossary](../user/glossary.md) - Technical term definitions
