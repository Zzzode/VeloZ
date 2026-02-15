# Production Architecture

This document describes the production deployment architecture for VeloZ.

## System Overview

```
                                    ┌─────────────────────────────────────────┐
                                    │              Load Balancer               │
                                    │           (nginx / HAProxy)              │
                                    └─────────────────┬───────────────────────┘
                                                      │
                    ┌─────────────────────────────────┼─────────────────────────────────┐
                    │                                 │                                 │
                    ▼                                 ▼                                 ▼
          ┌─────────────────┐             ┌─────────────────┐             ┌─────────────────┐
          │  Gateway Pod 1  │             │  Gateway Pod 2  │             │  Gateway Pod N  │
          │  (Python HTTP)  │             │  (Python HTTP)  │             │  (Python HTTP)  │
          └────────┬────────┘             └────────┬────────┘             └────────┬────────┘
                   │                               │                               │
                   ▼                               ▼                               ▼
          ┌─────────────────┐             ┌─────────────────┐             ┌─────────────────┐
          │   Engine Pod 1  │             │   Engine Pod 2  │             │   Engine Pod N  │
          │   (C++ Core)    │             │   (C++ Core)    │             │   (C++ Core)    │
          └────────┬────────┘             └────────┬────────┘             └────────┬────────┘
                   │                               │                               │
                   └───────────────────────────────┼───────────────────────────────┘
                                                   │
                                                   ▼
                              ┌─────────────────────────────────────────┐
                              │           Shared Services               │
                              │  ┌─────────┐  ┌─────────┐  ┌─────────┐ │
                              │  │  Redis  │  │Postgres │  │ Metrics │ │
                              │  │ (Cache) │  │  (WAL)  │  │(Prometheus)│
                              │  └─────────┘  └─────────┘  └─────────┘ │
                              └─────────────────────────────────────────┘
```

## Component Architecture

### Gateway Layer

The Python HTTP gateway handles:
- REST API requests
- SSE event streaming
- Request routing to engine
- Authentication and rate limiting

**Scaling:** Horizontal scaling with load balancer

### Engine Layer

The C++ engine handles:
- Order processing
- Market data handling
- Strategy execution
- Risk management

**Scaling:** One engine per gateway (stdio bridge)

### Data Layer

| Service | Purpose | Scaling |
|---------|---------|---------|
| Redis | Session cache, rate limiting | Cluster mode |
| PostgreSQL | Order WAL, configuration | Primary-replica |
| Prometheus | Metrics collection | Federation |

## Deployment Topologies

### Single Node (Development)

```yaml
# docker-compose.yml
services:
  veloz:
    build: .
    ports:
      - "8080:8080"
    environment:
      - VELOZ_PRESET=release
```

### Multi-Node (Production)

```yaml
# docker-compose.prod.yml
services:
  gateway:
    build: .
    deploy:
      replicas: 3
    ports:
      - "8080:8080"
    depends_on:
      - redis
      - postgres

  redis:
    image: redis:7-alpine
    command: redis-server --appendonly yes

  postgres:
    image: postgres:15-alpine
    environment:
      POSTGRES_DB: veloz
      POSTGRES_USER: veloz
      POSTGRES_PASSWORD: ${POSTGRES_PASSWORD}
    volumes:
      - postgres_data:/var/lib/postgresql/data

  prometheus:
    image: prom/prometheus:latest
    volumes:
      - ./prometheus.yml:/etc/prometheus/prometheus.yml

  grafana:
    image: grafana/grafana:latest
    ports:
      - "3000:3000"
    depends_on:
      - prometheus

volumes:
  postgres_data:
```

### Kubernetes (Enterprise)

```yaml
# k8s/deployment.yaml
apiVersion: apps/v1
kind: Deployment
metadata:
  name: veloz-gateway
spec:
  replicas: 3
  selector:
    matchLabels:
      app: veloz-gateway
  template:
    metadata:
      labels:
        app: veloz-gateway
    spec:
      containers:
      - name: gateway
        image: veloz:latest
        ports:
        - containerPort: 8080
        resources:
          requests:
            memory: "512Mi"
            cpu: "500m"
          limits:
            memory: "1Gi"
            cpu: "1000m"
        env:
        - name: VELOZ_PRESET
          value: "release"
        - name: VELOZ_EXECUTION_MODE
          valueFrom:
            secretKeyRef:
              name: veloz-secrets
              key: execution-mode
        livenessProbe:
          httpGet:
            path: /health
            port: 8080
          initialDelaySeconds: 10
          periodSeconds: 5
        readinessProbe:
          httpGet:
            path: /health
            port: 8080
          initialDelaySeconds: 5
          periodSeconds: 3
---
apiVersion: v1
kind: Service
metadata:
  name: veloz-gateway
spec:
  selector:
    app: veloz-gateway
  ports:
  - port: 80
    targetPort: 8080
  type: LoadBalancer
```

## Resource Requirements

### Minimum Production Requirements

| Component | CPU | Memory | Disk |
|-----------|-----|--------|------|
| Gateway | 0.5 core | 512 MB | 100 MB |
| Engine | 1 core | 1 GB | 500 MB |
| Redis | 0.5 core | 256 MB | 1 GB |
| PostgreSQL | 1 core | 1 GB | 10 GB |
| Prometheus | 0.5 core | 512 MB | 10 GB |

### Recommended Production Requirements

| Component | CPU | Memory | Disk |
|-----------|-----|--------|------|
| Gateway | 2 cores | 2 GB | 500 MB |
| Engine | 4 cores | 4 GB | 2 GB |
| Redis | 2 cores | 2 GB | 10 GB |
| PostgreSQL | 4 cores | 8 GB | 100 GB |
| Prometheus | 2 cores | 4 GB | 100 GB |

## Network Configuration

### Ports

| Service | Port | Protocol | Description |
|---------|------|----------|-------------|
| Gateway HTTP | 8080 | TCP | REST API |
| Gateway SSE | 8080 | TCP | Event stream |
| Prometheus | 9090 | TCP | Metrics |
| Grafana | 3000 | TCP | Dashboards |
| Redis | 6379 | TCP | Cache |
| PostgreSQL | 5432 | TCP | Database |

### Firewall Rules

```bash
# Allow HTTP traffic
ufw allow 8080/tcp

# Allow metrics (internal only)
ufw allow from 10.0.0.0/8 to any port 9090

# Deny direct database access from outside
ufw deny 5432/tcp
ufw deny 6379/tcp
```

## Security Configuration

### TLS/SSL

```nginx
# nginx.conf
server {
    listen 443 ssl http2;
    server_name veloz.example.com;

    ssl_certificate /etc/ssl/certs/veloz.crt;
    ssl_certificate_key /etc/ssl/private/veloz.key;
    ssl_protocols TLSv1.2 TLSv1.3;
    ssl_ciphers ECDHE-ECDSA-AES128-GCM-SHA256:ECDHE-RSA-AES128-GCM-SHA256;

    location / {
        proxy_pass http://veloz-gateway:8080;
        proxy_http_version 1.1;
        proxy_set_header Upgrade $http_upgrade;
        proxy_set_header Connection "upgrade";
        proxy_set_header Host $host;
        proxy_set_header X-Real-IP $remote_addr;
    }
}
```

### API Key Management

```bash
# Generate API keys
openssl rand -hex 32 > /etc/veloz/api_key

# Set environment variable
export VELOZ_API_KEY=$(cat /etc/veloz/api_key)
```

### Secrets Management

```yaml
# k8s/secrets.yaml
apiVersion: v1
kind: Secret
metadata:
  name: veloz-secrets
type: Opaque
stringData:
  binance-api-key: "your-api-key"
  binance-api-secret: "your-api-secret"
  postgres-password: "secure-password"
```

## High Availability

### Load Balancing

```nginx
# nginx.conf
upstream veloz_backend {
    least_conn;
    server gateway1:8080 weight=1;
    server gateway2:8080 weight=1;
    server gateway3:8080 weight=1;
    keepalive 32;
}
```

### Health Checks

```yaml
# docker-compose.yml
services:
  gateway:
    healthcheck:
      test: ["CMD", "curl", "-f", "http://localhost:8080/health"]
      interval: 10s
      timeout: 5s
      retries: 3
      start_period: 30s
```

### Failover

1. **Gateway Failover**: Load balancer removes unhealthy instances
2. **Database Failover**: PostgreSQL streaming replication
3. **Cache Failover**: Redis Sentinel or Cluster mode

## Performance Tuning

### Engine Optimization

```bash
# Set CPU affinity for engine process
taskset -c 0-3 ./veloz_engine

# Increase file descriptor limits
ulimit -n 65535

# Enable huge pages (Linux)
echo 1024 > /proc/sys/vm/nr_hugepages
```

### Network Optimization

```bash
# Increase TCP buffer sizes
sysctl -w net.core.rmem_max=16777216
sysctl -w net.core.wmem_max=16777216
sysctl -w net.ipv4.tcp_rmem="4096 87380 16777216"
sysctl -w net.ipv4.tcp_wmem="4096 65536 16777216"

# Enable TCP fast open
sysctl -w net.ipv4.tcp_fastopen=3
```

## Deployment Checklist

### Pre-Deployment

- [ ] Build release binaries: `cmake --preset release && cmake --build --preset release`
- [ ] Run all tests: `ctest --preset release`
- [ ] Review configuration
- [ ] Set up secrets management
- [ ] Configure TLS certificates

### Deployment

- [ ] Deploy database and cache
- [ ] Deploy gateway instances
- [ ] Configure load balancer
- [ ] Verify health checks
- [ ] Test API endpoints

### Post-Deployment

- [ ] Verify metrics collection
- [ ] Set up alerting
- [ ] Document runbooks
- [ ] Schedule backup verification
- [ ] Conduct load testing

## Related Documents

- [Monitoring](monitoring.md) - Observability setup
- [Backup & Recovery](backup_recovery.md) - Disaster recovery
- [CI/CD Pipeline](ci_cd.md) - Deployment automation
- [Troubleshooting](troubleshooting.md) - Common issues
