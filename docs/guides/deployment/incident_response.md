# VeloZ Incident Response Procedures

This document defines the incident response process for VeloZ production issues.

## Incident Severity Levels

| Level | Name | Description | Examples |
|-------|------|-------------|----------|
| P1 | Critical | Complete service outage, data loss, security breach | Gateway down, data corruption, unauthorized access |
| P2 | High | Major feature unavailable, significant degradation | Order processing failures, high latency (>1s) |
| P3 | Medium | Minor feature impact, workaround available | Intermittent errors, slow queries |
| P4 | Low | Minimal impact, cosmetic issues | UI bugs, documentation errors |

## Incident Response Process

### Phase 1: Detection and Triage (0-5 minutes)

1. **Alert received**
   - Acknowledge alert in PagerDuty
   - Note alert time and details

2. **Initial assessment**
   ```bash
   # Quick health check
   curl http://localhost:8080/health

   # Check recent logs
   journalctl -u veloz -n 50 --no-pager

   # Check metrics
   curl http://localhost:8080/metrics | grep -E "(error|latency)"
   ```

3. **Determine severity**
   - Is the service completely down? (P1)
   - Is a major feature broken? (P2)
   - Is there degraded performance? (P3)
   - Is it a minor issue? (P4)

4. **Start incident**
   - Create incident channel: `#incident-YYYYMMDD-brief-description`
   - Post initial assessment
   - Page additional responders if P1/P2

### Phase 2: Diagnosis (5-15 minutes)

1. **Gather information**
   ```bash
   # System status
   systemctl status veloz
   ps aux | grep veloz
   free -h
   df -h

   # Application logs
   journalctl -u veloz --since "15 minutes ago" | grep -i error

   # Metrics
   curl http://localhost:8080/metrics > /tmp/metrics_$(date +%s).txt
   ```

2. **Identify scope**
   - Which components are affected?
   - How many users impacted?
   - What changed recently?

3. **Form hypothesis**
   - What could cause these symptoms?
   - What evidence supports/refutes?

4. **Update stakeholders**
   - Post diagnosis progress to incident channel
   - Update status page if customer-facing

### Phase 3: Mitigation (15-30 minutes)

1. **Choose mitigation strategy**

   | Strategy | When to Use |
   |----------|-------------|
   | Restart | Process crash, memory leak |
   | Rollback | Bad deployment |
   | Scale | Capacity issue |
   | Failover | Infrastructure failure |
   | Config change | Misconfiguration |

2. **Execute mitigation**
   ```bash
   # Restart
   systemctl restart veloz

   # Rollback
   /opt/veloz/scripts/rollback.sh

   # Scale
   kubectl scale deployment veloz-gateway --replicas=5

   # Failover
   /opt/veloz/scripts/failover.sh
   ```

3. **Verify mitigation**
   ```bash
   # Health check
   curl http://localhost:8080/health

   # Check metrics
   curl http://localhost:8080/metrics | grep -E "(error|latency)"

   # Verify functionality
   curl http://localhost:8080/api/orders_state
   ```

### Phase 4: Resolution (30+ minutes)

1. **Confirm service restored**
   - All health checks passing
   - Error rates back to normal
   - Latency within SLA

2. **Root cause analysis**
   - What was the actual cause?
   - Why did it happen?
   - How can we prevent recurrence?

3. **Document findings**
   - Timeline of events
   - Actions taken
   - Root cause
   - Follow-up items

4. **Close incident**
   - Update status page
   - Notify stakeholders
   - Archive incident channel

### Phase 5: Post-Incident (24-48 hours)

1. **Schedule post-mortem**
   - Within 48 hours for P1/P2
   - Within 1 week for P3

2. **Write post-mortem document**
   - See template below

3. **Create follow-up tickets**
   - Prevention measures
   - Monitoring improvements
   - Documentation updates

4. **Share learnings**
   - Team meeting
   - Knowledge base update

## Communication Templates

### Initial Notification

```
INCIDENT: [Brief Description]
Severity: P[1-4]
Time: [YYYY-MM-DD HH:MM UTC]
Impact: [Description of user impact]
Status: Investigating

We are aware of an issue affecting [component/feature].
We are actively investigating and will provide updates.

Next update in: 15 minutes
```

### Status Update

```
INCIDENT UPDATE: [Brief Description]
Time: [YYYY-MM-DD HH:MM UTC]
Status: [Investigating/Identified/Mitigating/Resolved]

Update: [What we've learned/done]
Impact: [Current impact]
ETA: [Estimated resolution time if known]

Next update in: [time]
```

### Resolution Notification

```
RESOLVED: [Brief Description]
Time: [YYYY-MM-DD HH:MM UTC]
Duration: [Total incident duration]

Summary: [Brief summary of what happened]
Impact: [Final impact assessment]
Root Cause: [Brief root cause]

Post-mortem will be shared within 48 hours.
```

## Post-Mortem Template

```markdown
# Post-Mortem: [Incident Title]

**Date:** YYYY-MM-DD
**Duration:** X hours Y minutes
**Severity:** P[1-4]
**Author:** [Name]

## Summary
[1-2 sentence summary of the incident]

## Impact
- Users affected: [number/percentage]
- Revenue impact: [if applicable]
- SLA impact: [if applicable]

## Timeline (UTC)
| Time | Event |
|------|-------|
| HH:MM | [Event description] |
| HH:MM | [Event description] |

## Root Cause
[Detailed explanation of what caused the incident]

## Resolution
[How the incident was resolved]

## What Went Well
- [Item 1]
- [Item 2]

## What Went Poorly
- [Item 1]
- [Item 2]

## Action Items
| Action | Owner | Due Date | Status |
|--------|-------|----------|--------|
| [Action] | [Name] | YYYY-MM-DD | Open |

## Lessons Learned
[Key takeaways from this incident]
```

## Incident Commander Responsibilities

The Incident Commander (IC) is responsible for:

1. **Coordination**
   - Assign roles to responders
   - Ensure communication flow
   - Make decisions on mitigation strategy

2. **Communication**
   - Provide regular updates
   - Manage stakeholder expectations
   - Coordinate external communication

3. **Documentation**
   - Ensure timeline is recorded
   - Capture decisions and rationale
   - Initiate post-mortem process

### IC Handoff Procedure

When handing off IC role:

1. Brief incoming IC on:
   - Current status
   - Actions in progress
   - Open questions
   - Key stakeholders

2. Announce handoff in incident channel
3. Update PagerDuty assignment
4. Remain available for questions

## Runbook Quick Links

- [Operations Runbook](operations_runbook.md)
- [Troubleshooting Guide](troubleshooting.md)
- [DR Runbook](dr_runbook.md)
- [On-Call Handbook](oncall_handbook.md)
- [Monitoring](monitoring.md)

## Emergency Contacts

| Role | Primary | Backup |
|------|---------|--------|
| On-call Engineer | PagerDuty: veloz-oncall | Slack: #veloz-oncall |
| Database Admin | PagerDuty: db-oncall | Slack: #db-oncall |
| Infrastructure | PagerDuty: infra-oncall | Slack: #infra-oncall |
| Engineering Manager | Direct contact | Slack DM |
| VP Engineering | Direct contact | - |
