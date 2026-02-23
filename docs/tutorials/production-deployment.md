# Production Deployment

**Time:** 45 minutes | **Level:** Intermediate

## What You'll Learn

- Build VeloZ for production with release optimizations
- Configure production environment variables and secrets
- Set up authentication with JWT and API keys
- Deploy VeloZ as a systemd service
- Configure Nginx as a reverse proxy with TLS
- Perform blue-green deployment with rollback procedures
- Validate deployment health and monitoring

## Prerequisites

- Completed [Your First Trade](./first-trade.md) tutorial
- Linux server with root access (Ubuntu 22.04 or later recommended)
- Domain name with DNS configured (for TLS)
- Basic familiarity with systemd and Nginx

---

## Step 1: Build Release Binaries

Build VeloZ with release optimizations for production performance.

```bash
# Navigate to VeloZ directory
cd /opt/veloz

# Configure release build
cmake --preset release

# Build all targets with parallel compilation
cmake --build --preset release-all -j$(nproc)
```

**Expected Output:**
```
[100%] Built target veloz_engine
```

Run the test suite to verify the build:

```bash
# Run all tests
ctest --preset release -j$(nproc)
```

**Expected Output:**
```
100% tests passed, 0 tests failed out of 42
```

> **Warning:** Never deploy a build that has failing tests.

---

## Step 2: Create Production Directory Structure

Set up the standard directory structure for production deployment.

```bash
# Create production directories
sudo mkdir -p /opt/veloz/bin
sudo mkdir -p /opt/veloz/config
sudo mkdir -p /var/lib/veloz/wal
sudo mkdir -p /var/log/veloz

# Copy release binary
sudo cp /opt/veloz/build/release/apps/engine/veloz_engine /opt/veloz/bin/

# Set ownership
sudo chown -R veloz:veloz /opt/veloz
sudo chown -R veloz:veloz /var/lib/veloz
sudo chown -R veloz:veloz /var/log/veloz
```

Create a dedicated service user:

```bash
# Create veloz user (no login shell)
sudo useradd -r -s /bin/false veloz
```

---

## Step 3: Configure Environment Variables

Create the production environment configuration file.

```bash
# Create environment file
sudo nano /etc/veloz/veloz.env
```

Add the following configuration:

```bash
# Core Configuration
VELOZ_PORT=8080
VELOZ_LOG_LEVEL=info
VELOZ_WORKER_THREADS=4

# Execution Mode (testnet for initial deployment)
VELOZ_EXECUTION_MODE=binance_testnet_spot

# WAL Configuration
VELOZ_WAL_DIR=/var/lib/veloz/wal
VELOZ_WAL_MAX_SIZE=67108864
VELOZ_WAL_SYNC=true

# Market Data
VELOZ_MARKET_SOURCE=binance_ws
VELOZ_MARKET_SYMBOL=BTCUSDT

# Performance
VELOZ_MEMORY_POOL_SIZE=1073741824
VELOZ_ORDER_RATE_LIMIT=10
```

Set secure permissions:

```bash
# Restrict access to environment file
sudo chmod 600 /etc/veloz/veloz.env
sudo chown veloz:veloz /etc/veloz/veloz.env
```

---

## Step 4: Configure Exchange Credentials

Store exchange API credentials securely. For production, use a secrets manager like HashiCorp Vault.

### Option A: Environment File (Development/Testing)

```bash
# Add credentials to environment file
sudo nano /etc/veloz/veloz.env
```

Append:

```bash
# Binance Credentials (use testnet keys for initial deployment)
VELOZ_BINANCE_API_KEY=your_testnet_api_key
VELOZ_BINANCE_API_SECRET=your_testnet_api_secret
```

### Option B: HashiCorp Vault (Production)

```bash
# Store credentials in Vault
vault kv put secret/veloz/binance \
  api_key=your_api_key \
  api_secret=your_api_secret

# Configure VeloZ to use Vault
echo "VELOZ_VAULT_ADDR=https://vault.example.com:8200" >> /etc/veloz/veloz.env
echo "VELOZ_VAULT_PATH=secret/veloz/binance" >> /etc/veloz/veloz.env
```

> **Warning:** Never commit API credentials to version control.

---

## Step 5: Configure Authentication

Set up JWT authentication for the API.

```bash
# Generate JWT secret (32 bytes, base64 encoded)
JWT_SECRET=$(openssl rand -base64 32)

# Add to environment file
echo "VELOZ_JWT_SECRET=$JWT_SECRET" | sudo tee -a /etc/veloz/veloz.env
echo "VELOZ_JWT_EXPIRY=3600" | sudo tee -a /etc/veloz/veloz.env
```

Create the initial admin user:

```bash
# Generate admin API key
ADMIN_API_KEY=$(openssl rand -hex 32)

# Create API keys file
sudo nano /etc/veloz/api_keys.json
```

Add:

```json
{
  "keys": [
    {
      "key": "your_generated_api_key_here",
      "name": "admin",
      "permissions": ["read", "write", "admin"],
      "rate_limit": 100
    }
  ]
}
```

```bash
# Secure the API keys file
sudo chmod 600 /etc/veloz/api_keys.json
sudo chown veloz:veloz /etc/veloz/api_keys.json
```

---

## Step 6: Create Systemd Service

Create a systemd service unit for VeloZ.

```bash
# Create service file
sudo nano /etc/systemd/system/veloz.service
```

Add the following configuration:

```ini
[Unit]
Description=VeloZ Trading Engine
Documentation=https://github.com/veloz/veloz
After=network.target
Wants=network-online.target

[Service]
Type=simple
User=veloz
Group=veloz
EnvironmentFile=/etc/veloz/veloz.env
ExecStart=/opt/veloz/bin/veloz_engine --port ${VELOZ_PORT}
ExecReload=/bin/kill -HUP $MAINPID
Restart=always
RestartSec=5
TimeoutStopSec=30

# Security hardening
NoNewPrivileges=true
ProtectSystem=strict
ProtectHome=true
ReadWritePaths=/var/lib/veloz /var/log/veloz
PrivateTmp=true

# Resource limits
LimitNOFILE=65536
LimitNPROC=4096

[Install]
WantedBy=multi-user.target
```

Enable and start the service:

```bash
# Reload systemd configuration
sudo systemctl daemon-reload

# Enable service to start on boot
sudo systemctl enable veloz

# Start the service
sudo systemctl start veloz

# Check status
sudo systemctl status veloz
```

**Expected Output:**
```
veloz.service - VeloZ Trading Engine
   Loaded: loaded (/etc/systemd/system/veloz.service; enabled)
   Active: active (running) since Mon 2026-02-23 10:00:00 UTC
```

---

## Step 7: Configure Nginx Reverse Proxy

Install and configure Nginx as a reverse proxy with TLS termination.

```bash
# Install Nginx
sudo apt update
sudo apt install -y nginx certbot python3-certbot-nginx
```

Create the Nginx configuration:

```bash
# Create VeloZ site configuration
sudo nano /etc/nginx/sites-available/veloz
```

Add:

```nginx
upstream veloz_backend {
    server 127.0.0.1:8080;
    keepalive 32;
}

server {
    listen 80;
    server_name veloz.example.com;

    # Redirect HTTP to HTTPS
    return 301 https://$server_name$request_uri;
}

server {
    listen 443 ssl http2;
    server_name veloz.example.com;

    # TLS configuration (certificates added by certbot)
    ssl_protocols TLSv1.2 TLSv1.3;
    ssl_ciphers ECDHE-ECDSA-AES128-GCM-SHA256:ECDHE-RSA-AES128-GCM-SHA256;
    ssl_prefer_server_ciphers off;

    # Security headers
    add_header X-Frame-Options DENY;
    add_header X-Content-Type-Options nosniff;
    add_header X-XSS-Protection "1; mode=block";

    # Rate limiting
    limit_req_zone $binary_remote_addr zone=api:10m rate=10r/s;

    location / {
        limit_req zone=api burst=20 nodelay;

        proxy_pass http://veloz_backend;
        proxy_http_version 1.1;
        proxy_set_header Host $host;
        proxy_set_header X-Real-IP $remote_addr;
        proxy_set_header X-Forwarded-For $proxy_add_x_forwarded_for;
        proxy_set_header X-Forwarded-Proto $scheme;

        # WebSocket support for SSE
        proxy_set_header Connection "";
        proxy_buffering off;
        proxy_cache off;
        proxy_read_timeout 3600s;
    }

    location /health {
        proxy_pass http://veloz_backend/health;
        access_log off;
    }
}
```

Enable the site and obtain TLS certificate:

```bash
# Enable site
sudo ln -s /etc/nginx/sites-available/veloz /etc/nginx/sites-enabled/

# Test configuration
sudo nginx -t

# Obtain TLS certificate
sudo certbot --nginx -d veloz.example.com

# Reload Nginx
sudo systemctl reload nginx
```

---

## Step 8: Perform Blue-Green Deployment

For zero-downtime updates, use blue-green deployment.

### Prepare the New Version

```bash
# Create backup of current binary
sudo cp /opt/veloz/bin/veloz_engine \
  /opt/veloz/bin/veloz_engine.$(date +%Y%m%d)

# Pull and build new version
cd /opt/veloz/src
git pull origin main
cmake --preset release
cmake --build --preset release-all -j$(nproc)

# Run tests
ctest --preset release -j$(nproc)
```

### Deploy with Health Check

```bash
# Copy new binary to staging location
sudo cp /opt/veloz/build/release/apps/engine/veloz_engine \
  /opt/veloz/bin/veloz_engine.new

# Stop current service
sudo systemctl stop veloz

# Swap binaries
sudo mv /opt/veloz/bin/veloz_engine /opt/veloz/bin/veloz_engine.old
sudo mv /opt/veloz/bin/veloz_engine.new /opt/veloz/bin/veloz_engine

# Start new version
sudo systemctl start veloz

# Wait for startup
sleep 5

# Verify health
curl -f http://127.0.0.1:8080/health
```

**Expected Output:**
```json
{
  "status": "healthy",
  "version": "1.1.0"
}
```

---

## Step 9: Configure Rollback Procedure

If the new version fails health checks, rollback immediately.

```bash
# Rollback script: /opt/veloz/scripts/rollback.sh
#!/bin/bash
set -e

echo "Rolling back VeloZ..."

# Stop current service
sudo systemctl stop veloz

# Restore previous binary
sudo mv /opt/veloz/bin/veloz_engine.old /opt/veloz/bin/veloz_engine

# Start service
sudo systemctl start veloz

# Verify health
sleep 5
if curl -f http://127.0.0.1:8080/health; then
    echo "Rollback successful"
else
    echo "Rollback failed - manual intervention required"
    exit 1
fi
```

Make the script executable:

```bash
chmod +x /opt/veloz/scripts/rollback.sh
```

---

## Step 10: Validate Deployment

Run the post-deployment verification checklist.

```bash
# Check service status
sudo systemctl status veloz
```

```bash
# Verify health endpoint
curl https://veloz.example.com/health
```

**Expected Output:**
```json
{
  "status": "healthy",
  "engine": "running",
  "uptime_seconds": 120,
  "version": "1.0.0"
}
```

```bash
# Verify metrics endpoint
curl https://veloz.example.com/metrics | head -20
```

**Expected Output:**
```
# HELP veloz_orders_total Total orders processed
# TYPE veloz_orders_total counter
veloz_orders_total{status="filled"} 0
veloz_orders_total{status="canceled"} 0
```

```bash
# Test order placement (simulation mode)
curl -X POST https://veloz.example.com/api/order \
  -H "Content-Type: application/json" \
  -H "Authorization: Bearer your_api_key" \
  -d '{
    "side": "BUY",
    "symbol": "BTCUSDT",
    "qty": 0.001,
    "price": 50000.0
  }'
```

**Expected Output:**
```json
{
  "ok": true,
  "client_order_id": "web-1708704000000"
}
```

---

## Summary

**What you accomplished:**
- Built VeloZ with release optimizations
- Configured production environment variables and secrets
- Set up JWT authentication and API keys
- Deployed VeloZ as a systemd service with security hardening
- Configured Nginx reverse proxy with TLS
- Performed blue-green deployment
- Validated deployment health

## Troubleshooting

### Issue: Service fails to start
**Symptom:** `systemctl status veloz` shows "failed"
**Solution:** Check logs for details:
```bash
journalctl -u veloz -n 50
```

### Issue: Permission denied on WAL directory
**Symptom:** Error "cannot create WAL file"
**Solution:** Fix ownership:
```bash
sudo chown -R veloz:veloz /var/lib/veloz
```

### Issue: Nginx returns 502 Bad Gateway
**Symptom:** Browser shows "502 Bad Gateway"
**Solution:** Verify VeloZ is running and listening:
```bash
sudo systemctl status veloz
curl http://127.0.0.1:8080/health
```

### Issue: TLS certificate renewal fails
**Symptom:** Certificate expired warning
**Solution:** Renew manually:
```bash
sudo certbot renew --nginx
```

## Next Steps

- [Custom Strategy Development](./custom-strategy-development.md) - Build and deploy custom trading strategies
- [Monitoring Guide](../guides/deployment/monitoring.md) - Set up Prometheus and Grafana
- [Operations Runbook](../guides/deployment/operations_runbook.md) - Day-to-day operational procedures
