# Deployment Documentation

This directory contains documentation for deploying VeloZ in production environments.

## Documents

| Document | Description |
|----------|-------------|
| [Production Architecture](production_architecture.md) | System architecture and deployment topology |
| [Monitoring](monitoring.md) | Observability, metrics, and alerting setup |
| [Backup & Recovery](backup_recovery.md) | Disaster recovery procedures |
| [CI/CD Pipeline](ci_cd.md) | Continuous integration and deployment |
| [Troubleshooting](troubleshooting.md) | Common issues and solutions |

## Quick Links

- [Installation Guide](../user/installation.md) - Initial setup
- [Configuration Guide](../user/configuration.md) - Environment variables
- [Docker Setup](#docker-deployment) - Container deployment

## Deployment Options

### 1. Local Development

```bash
./scripts/run_gateway.sh dev
```

### 2. Docker Deployment

```bash
docker-compose up -d
```

### 3. Kubernetes Deployment

See [Production Architecture](production_architecture.md) for Kubernetes manifests.

## Production Checklist

Before deploying to production:

- [ ] Review [Production Architecture](production_architecture.md)
- [ ] Configure environment variables
- [ ] Set up monitoring and alerting
- [ ] Configure backup procedures
- [ ] Test disaster recovery
- [ ] Review security settings
- [ ] Load test the system
