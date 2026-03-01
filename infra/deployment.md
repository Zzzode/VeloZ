# VeloZ Deployment Guide

This document provides detailed instructions for deploying the VeloZ Trading Engine in various environments.

## Table of Contents

1. [Development Deployment](#development-deployment)
2. [Docker Deployment](#docker-deployment)
3. [Kubernetes Deployment](#kubernetes-deployment)
4. [Configuration](#configuration)
5. [Monitoring](#monitoring)
6. [Troubleshooting](#troubleshooting)

## Development Deployment

### Prerequisites
- Ubuntu 22.04+ or macOS 12+
- GCC 10+ or Clang 12+
- CMake 3.24+
- Python 3.9+

### Build and Run
```bash
# Build the engine
cmake --preset dev
cmake --build --preset dev -j

# Run the engine directly
./build/dev/apps/engine/veloz_engine

# Or run via the Python gateway
cd apps/gateway
python3 gateway.py
```

## Docker Deployment

### Prerequisites
- Docker 20.10+
- Docker Compose 2.0+

### Quick Start with Docker Compose

```bash
# Create environment file
cp .env.example .env

# Edit .env to set your API keys and configuration
vi .env

# Start services
docker compose up -d

# Check service status
docker compose ps

# View logs
docker compose logs -f veloz-engine
```

### Manual Docker Build

```bash
# Build production image
docker build -t veloz-engine:latest -f infra/docker/Dockerfile .

# Run container
docker run -d \
  --name veloz-engine \
  -p 8080:8080 \
  --env-file .env \
  --volume veloz-logs:/app/logs \
  veloz-engine:latest
```

### Environment Variables

| Variable | Description | Default |
|----------|-------------|---------|
| `VELOZ_PRESET` | Build preset (dev/release) | `dev` |
| `VELOZ_HOST` | API host address | `0.0.0.0` |
| `VELOZ_PORT` | API port | `8080` |
| `VELOZ_MARKET_SOURCE` | Market data source (binance_rest/sim) | `binance_rest` |
| `VELOZ_MARKET_SYMBOL` | Trading symbol | `BTCUSDT` |
| `VELOZ_EXECUTION_MODE` | Execution mode (sim_engine/binance_testnet_spot) | `sim_engine` |
| `VELOZ_BINANCE_BASE_URL` | Binance API endpoint | `https://api.binance.com` |
| `VELOZ_BINANCE_TRADE_BASE_URL` | Binance trade endpoint | `https://testnet.binance.vision` |
| `VELOZ_BINANCE_API_KEY` | Binance API key | `dummy_key` |
| `VELOZ_BINANCE_API_SECRET` | Binance API secret | `dummy_secret` |

## Kubernetes Deployment

### Prerequisites
- Kubernetes 1.24+
- kubectl configured
- Helm 3.10+

### Deploy with Helm (Recommended)

```bash
# Add namespace with resource quotas
kubectl apply -f infra/k8s/namespace.yaml

# Deploy using Helm with environment-specific values
# Development
helm install veloz ./infra/helm/veloz \
  -f ./infra/helm/veloz/environments/values-dev.yaml \
  -n veloz

# Staging
helm install veloz ./infra/helm/veloz \
  -f ./infra/helm/veloz/environments/values-staging.yaml \
  -n veloz

# Production
helm install veloz ./infra/helm/veloz \
  -f ./infra/helm/veloz/environments/values-production.yaml \
  -n veloz-prod

# Check deployment status
helm status veloz -n veloz
kubectl get pods -n veloz

# Upgrade deployment
helm upgrade veloz ./infra/helm/veloz \
  -f ./infra/helm/veloz/environments/values-staging.yaml \
  -n veloz

# Rollback if needed
helm rollback veloz 1 -n veloz
```

### Deploy with kubectl (Basic)

```bash
# Create namespace
kubectl apply -f infra/k8s/namespace.yaml

# Apply configuration
kubectl apply -f infra/k8s/deployment.yaml -n veloz

# Check deployment status
kubectl get deployments -n veloz
kubectl get pods -n veloz

# Check service status
kubectl get services -n veloz

# Forward port to access locally
kubectl port-forward service/veloz-engine 8080:8080 -n veloz
```

### Secrets Management

For production, use HashiCorp Vault with the Vault Secrets Operator:

```bash
# Install Vault Secrets Operator
helm repo add hashicorp https://helm.releases.hashicorp.com
helm install vault-secrets-operator hashicorp/vault-secrets-operator -n vault-secrets-operator-system --create-namespace

# Configure VaultAuth
kubectl apply -f - <<EOF
apiVersion: secrets.hashicorp.com/v1beta1
kind: VaultAuth
metadata:
  name: veloz-vault-auth
  namespace: veloz
spec:
  method: kubernetes
  mount: kubernetes
  kubernetes:
    role: veloz
    serviceAccount: veloz
EOF

# Create VaultStaticSecret
kubectl apply -f - <<EOF
apiVersion: secrets.hashicorp.com/v1beta1
kind: VaultStaticSecret
metadata:
  name: veloz-api-keys
  namespace: veloz
spec:
  type: kv-v2
  mount: secret
  path: veloz/api-keys
  destination:
    name: veloz-api-keys
    create: true
  refreshAfter: 30s
EOF
```

For development/testing, use inline secrets:
```bash
kubectl create secret generic veloz-api-keys \
  --namespace veloz \
  --from-literal=binance-api-key="YOUR_API_KEY" \
  --from-literal=binance-api-secret="YOUR_API_SECRET"
```

### Helm Chart Features

The VeloZ Helm chart includes:

| Feature | Description |
|---------|-------------|
| **Deployment** | Configurable replicas, resources, probes |
| **Service** | ClusterIP, NodePort, or LoadBalancer |
| **Ingress** | NGINX ingress with TLS support |
| **HPA** | Horizontal Pod Autoscaler with CPU/memory metrics |
| **PDB** | Pod Disruption Budget for availability |
| **NetworkPolicy** | Ingress/egress traffic control |
| **ServiceMonitor** | Prometheus Operator integration |
| **PrometheusRule** | Alerting rules for critical metrics |
| **RBAC** | Role and RoleBinding for least privilege |
| **PVC** | Persistent storage for logs and state |

## Configuration

### Engine Configuration

The engine configuration is managed through environment variables and can be customized by:

1. Creating a `.env` file
2. Setting environment variables directly
3. Modifying the Kubernetes secret

### Gateway Configuration

The Python gateway reads configuration from the environment and exposes REST API endpoints:

- `/health` - Health check
- `/api/market` - Current market price
- `/api/orders_state` - Order state
- `/api/order` - Place order
- `/api/cancel` - Cancel order
- `/api/stream` - Server-Sent Events stream

## Monitoring

### Health Check
```bash
curl http://localhost:8080/health
```

### Metrics
The engine exposes basic metrics through the gateway:
```bash
curl http://localhost:8080/metrics
```

### Logs

#### Docker
```bash
docker compose logs -f veloz-engine
```

#### Kubernetes
```bash
kubectl logs -f deployment/veloz-engine -n veloz
```

## Troubleshooting

### Common Issues

1. **Engine failing to start**:
   - Check if all dependencies are installed
   - Verify the engine binary exists at `/app/veloz_engine`
   - Check logs for detailed error information

2. **API not responding**:
   - Verify the container/pod is running
   - Check port binding configuration
   - Verify firewall/security group settings

3. **Market data not updating**:
   - Check internet connectivity from the container
   - Verify Binance API endpoint accessibility
   - Check API key permissions

4. **Order execution failing**:
   - Verify API key has trading permissions
   - Check account balance and order limits
   - Verify execution mode configuration

### Debugging

#### Enable Debug Logging
```bash
# For development
VELOZ_LOG_LEVEL=DEBUG python3 gateway.py

# For Docker
docker run -e VELOZ_LOG_LEVEL=DEBUG ...
```

## Upgrading

### Docker
```bash
# Pull latest image
docker pull veloz-engine:latest

# Recreate container
docker stop veloz-engine
docker rm veloz-engine
docker run -d --name veloz-engine --env-file .env -p 8080:8080 veloz-engine:latest
```

### Kubernetes
```bash
# Update image
kubectl set image deployment/veloz-engine veloz-engine=veloz-engine:latest -n veloz

# Check status
kubectl rollout status deployment/veloz-engine -n veloz
```

## Security

### Best Practices
- Always use separate API keys for different environments
- Restrict API key permissions to necessary operations
- Use Kubernetes secrets for sensitive information
- Enable HTTPS in production
- Implement IP whitelisting
- Rotate API keys regularly

### API Key Permissions
For Binance API keys, the following permissions are recommended:
- Read market data
- Read account information
- Place orders
- Cancel orders

## Backup and Recovery

### Data Persistence
- Logs are stored in `/app/logs` (mount a volume for persistence)
- Order and position state is stored in memory (not persistent)
- For persistent storage, implement database integration

### Disaster Recovery
1. Monitor engine health
2. Implement auto-scaling and load balancing
3. Use rolling updates for zero-downtime deployments
4. Test recovery procedures regularly

## Performance Tuning

### Engine Optimization
- Adjust worker and thread counts based on workload
- Optimize memory and CPU limits
- Use production build preset for best performance
- Monitor resource usage and adjust as needed

### Network Optimization
- Enable HTTP/2 for better performance
- Implement connection pooling
- Optimize DNS resolution
- Use CDN for static content

## Advanced Configuration

### Custom Strategies
- Implement custom strategy plugins
- Configure strategy parameters via REST API
- Enable strategy backtesting
- Monitor strategy performance metrics

### Trading Pairs
- Configure multiple trading pairs
- Implement spread and arbitrage strategies
- Monitor correlation between pairs

### Risk Management
- Adjust risk limits per strategy
- Implement stop-loss and take-profit mechanisms
- Monitor portfolio risk exposure
- Enable circuit breakers
