# Security Best Practices Guide

This guide provides security recommendations for deploying and operating VeloZ in production environments.

## Table of Contents

1. [API Key Management](#api-key-management)
2. [Network Security Configuration](#network-security-configuration)
3. [Access Control Best Practices](#access-control-best-practices)
4. [Audit Log Usage](#audit-log-usage)
5. [Key Rotation Strategies](#key-rotation-strategies)
6. [Security Monitoring](#security-monitoring)
7. [Compliance Requirements](#compliance-requirements)
8. [Security Checklist](#security-checklist)

---

## API Key Management

### Secure Storage

**Never store API keys in plain text or version control:**

```bash
# WRONG: Hardcoded in scripts
export VELOZ_BINANCE_API_KEY="abc123..."  # Never do this!

# CORRECT: Read from secure file
export VELOZ_BINANCE_API_KEY=$(cat /secure/path/binance_key)

# CORRECT: Use environment without shell history
read -s VELOZ_BINANCE_API_SECRET && export VELOZ_BINANCE_API_SECRET
```

**Use HashiCorp Vault for production:**

```bash
export VELOZ_VAULT_ENABLED=true
export VAULT_ADDR=https://vault.internal:8200
export VAULT_AUTH_METHOD=approle
export VAULT_ROLE_ID=<your-role-id>
export VAULT_SECRET_ID=<your-secret-id>
```

### Exchange API Key Permissions

Apply the principle of least privilege:

| Permission | Development | Testnet | Production |
|------------|-------------|---------|------------|
| Read market data | Yes | Yes | Yes |
| Spot trading | No | Yes | Yes |
| Withdrawals | No | No | **Never** |

**Always enable IP restrictions on exchange API keys** by adding your server's public IP to the whitelist.

### Separate Keys by Environment

| Environment | Purpose | Permissions |
|-------------|---------|-------------|
| Development | Local testing | Read-only |
| Testnet | Integration testing | Full trading (testnet) |
| Production | Live trading | Spot trading only |

---

## Network Security Configuration

### TLS Configuration

**Enable TLS for all production deployments:**

```bash
export VELOZ_TLS_ENABLED=true
export VELOZ_TLS_CERT=/etc/veloz/tls/server.crt
export VELOZ_TLS_KEY=/etc/veloz/tls/server.key
```

For internal services, enable mutual TLS (mTLS):

```bash
export VELOZ_TLS_CA=/etc/veloz/tls/ca.crt
export VELOZ_TLS_CLIENT_CERT=true
```

### Firewall Rules

| Port | Protocol | Source | Purpose |
|------|----------|--------|---------|
| 443 | TCP | Load balancer | HTTPS traffic |
| 8080 | TCP | localhost only | Gateway internal |
| 22 | TCP | VPN only | SSH access |

### IP Whitelisting

```bash
# Allow only specific IP ranges
export VELOZ_IP_WHITELIST="10.0.0.0/8,192.168.0.0/16"

# Block known malicious IPs
export VELOZ_IP_BLACKLIST="1.2.3.4,5.6.7.8"

# Disable private IP access for public deployments
export VELOZ_ALLOW_PRIVATE_IPS=false
```

### CORS Configuration

```bash
export VELOZ_PRODUCTION_MODE=true
export VELOZ_CORS_ORIGINS="https://trading.yourdomain.com,https://admin.yourdomain.com"
export VELOZ_CORS_CREDENTIALS=true
```

---

## Access Control Best Practices

### Authentication Configuration

```bash
export VELOZ_AUTH_ENABLED=true
export VELOZ_ADMIN_PASSWORD=$(openssl rand -base64 32)
export VELOZ_JWT_SECRET=$(openssl rand -base64 64)
export VELOZ_TOKEN_EXPIRY=3600  # 1 hour
```

**JWT secret requirements:**

| Requirement | Minimum | Recommended |
|-------------|---------|-------------|
| Length | 32 characters | 64+ characters |
| Entropy | Cryptographically random | Hardware RNG |
| Storage | Environment variable | Vault |

### Role-Based Access Control

| Role | Permissions |
|------|-------------|
| `admin` | Full access to all endpoints |
| `trader` | Place and cancel orders, view positions |
| `viewer` | Read-only access to market data and positions |

### API Key Permissions

Create scoped API keys with minimal permissions:

| Scope | Description |
|-------|-------------|
| `read:market` | View market data |
| `read:orders` | View order history |
| `write:orders` | Place and cancel orders |
| `read:positions` | View positions |
| `admin:users` | Manage users |

---

## Audit Log Usage

### Log Retention Policies

| Log Type | Retention | Purpose |
|----------|-----------|---------|
| `auth` | 90 days | Login/logout events |
| `order` | 365 days | Trading activity |
| `api_key` | 365 days | Key lifecycle |
| `error` | 30 days | System errors |
| `access` | 14 days | API access logs |

**Configure audit logging:**

```bash
export VELOZ_AUDIT_LOG_ENABLED=true
export VELOZ_AUDIT_LOG_FILE=/var/log/veloz/audit/audit.log
```

### Log Analysis

```bash
# Find failed login attempts
grep '"event":"login_failed"' /var/log/veloz/audit/audit.log | jq .

# Find API key creations
grep '"event":"api_key_created"' /var/log/veloz/audit/audit.log | jq .
```

### Incident Investigation

1. Identify the timeframe of suspicious activity
2. Extract relevant logs using timestamps
3. Correlate events across log types
4. Identify affected resources (users, API keys, orders)
5. Document findings for incident report

---

## Key Rotation Strategies

### Rotation Frequency

| Secret Type | Rotation Frequency |
|-------------|-------------------|
| Exchange API keys | Monthly |
| JWT secret | Quarterly |
| Admin password | Quarterly |
| TLS certificates | Before expiry |

### Zero-Downtime Rotation

**Exchange API key rotation procedure:**

1. Create new API key on exchange
2. Store new key in Vault
3. Update VeloZ configuration
4. Verify new key works
5. Revoke old key on exchange
6. Remove old key from Vault

### Emergency Rotation

**When to perform emergency rotation:**

- Suspected credential compromise
- Employee departure with access
- Security audit findings
- Unusual API activity detected

**Emergency actions:**

```bash
# Revoke compromised API key
curl -X DELETE http://127.0.0.1:8080/api/keys/<key_id> \
  -H "Authorization: Bearer <admin_token>"

# Invalidate all user sessions
curl -X POST http://127.0.0.1:8080/api/auth/invalidate-all \
  -H "Authorization: Bearer <admin_token>"
```

---

## Security Monitoring

### Security Metrics

| Metric | Warning | Critical |
|--------|---------|----------|
| Failed logins/hour | > 10 | > 50 |
| Rate limit hits/min | > 100 | > 500 |
| Invalid API keys/hour | > 5 | > 20 |
| Blocked IPs/hour | > 10 | > 50 |

### Alert Configuration

```yaml
# Alertmanager rule example
groups:
  - name: security
    rules:
      - alert: HighAuthFailureRate
        expr: rate(veloz_auth_failures_total[5m]) > 0.1
        for: 5m
        labels:
          severity: warning
```

**Alert routing:**

| Severity | Channel | Response Time |
|----------|---------|---------------|
| Critical | PagerDuty + SMS | Immediate |
| Warning | Slack + Email | 15 minutes |
| Info | Email | 1 hour |

### Incident Response

1. **Detection**: Alert triggered or anomaly identified
2. **Triage**: Assess severity and scope
3. **Containment**: Block suspicious IPs, revoke credentials
4. **Investigation**: Review audit logs, identify root cause
5. **Remediation**: Fix vulnerabilities, rotate credentials
6. **Recovery**: Restore normal operations
7. **Post-mortem**: Document lessons learned

---

## Compliance Requirements

### Data Protection

| Data Type | Protection | Retention |
|-----------|------------|-----------|
| API credentials | Encrypted at rest | Until rotated |
| User passwords | Hashed (bcrypt) | Until changed |
| Trading history | Encrypted | Per regulation |
| Audit logs | Integrity protected | 1+ years |

### Regulatory Requirements

| Regulation | Requirements |
|------------|--------------|
| MiFID II | Transaction reporting, best execution |
| GDPR | Data protection, right to erasure |
| SOC 2 | Access controls, audit logging |

### Security Assessments

| Assessment | Frequency |
|------------|-----------|
| Vulnerability scan | Weekly |
| Penetration test | Annually |
| Code review | Per release |
| Access review | Quarterly |

---

## Security Checklist

### Pre-Deployment

**Secrets Management:**
- [ ] API keys stored in Vault (not environment variables)
- [ ] JWT secret is 64+ characters, cryptographically random
- [ ] Admin password is strong (32+ characters)
- [ ] No secrets in version control

**Network Security:**
- [ ] TLS enabled with valid certificates
- [ ] CORS restricted to specific origins
- [ ] IP whitelist configured (if applicable)
- [ ] Firewall rules in place

**Access Control:**
- [ ] Authentication enabled (`VELOZ_AUTH_ENABLED=true`)
- [ ] Production mode enabled (`VELOZ_PRODUCTION_MODE=true`)
- [ ] Rate limiting configured
- [ ] API key permissions scoped appropriately

**Audit and Monitoring:**
- [ ] Audit logging enabled
- [ ] Security alerts configured
- [ ] Incident response plan documented

**Exchange API Keys:**
- [ ] Withdrawal permissions disabled
- [ ] IP restrictions enabled on exchange
- [ ] Separate keys for each environment

### Ongoing Tasks

**Monthly:**
- [ ] Rotate exchange API keys
- [ ] Review user access permissions

**Quarterly:**
- [ ] Rotate JWT secrets
- [ ] Conduct access review
- [ ] Update incident response plan

---

## Summary

| Security Area | Key Actions |
|---------------|-------------|
| API Keys | Store in Vault, restrict permissions, rotate monthly |
| Network | Enable TLS, configure firewall, use VPN |
| Access Control | Enable auth, use RBAC, scope API keys |
| Audit Logs | Enable logging, configure retention, monitor |
| Key Rotation | Schedule rotations, document procedures |
| Monitoring | Set up alerts, monitor metrics, respond to incidents |
| Compliance | Maintain audit trails, encrypt data, conduct assessments |

## Related Documentation

- [Configuration Guide](configuration.md) - Environment variables and settings
- [Monitoring Guide](monitoring.md) - Metrics and observability
- [Best Practices](best-practices.md) - General operational best practices
- [Security Configuration](../../security/SECURITY_CONFIG.md) - Technical security reference
