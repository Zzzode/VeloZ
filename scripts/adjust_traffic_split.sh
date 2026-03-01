#!/usr/bin/env bash
set -euo pipefail

# Adjust traffic split between Python and C++ Gateway
# Usage: ./scripts/adjust_traffic_split.sh [cpp_percentage]

CPP_WEIGHT=${1:-5}
PY_WEIGHT=$((100 - CPP_WEIGHT))

echo "=== Adjusting Traffic Split ==="
echo "C++ Gateway: ${CPP_WEIGHT}%"
echo "Python Gateway: ${PY_WEIGHT}%"

# Check if nginx is available
if command -v nginx >/dev/null 2>&1; then
  NGINX_CONF="/etc/nginx/conf.d/veloz-gateway.conf"

  # Check if we have write access to nginx config
  if [[ -w "${NGINX_CONF}" ]] || [[ -w "/etc/nginx/conf.d" ]]; then
    echo "Updating nginx configuration..."

    # Create nginx configuration
    cat > "${NGINX_CONF}" <<EOF
upstream veloz_gateway {
    # Python Gateway (fallback)
    server 127.0.0.1:8080 weight=${PY_WEIGHT} max_fails=3 fail_timeout=30s;

    # C++ Gateway (canary)
    server 127.0.0.1:8081 weight=${CPP_WEIGHT} max_fails=3 fail_timeout=30s;

    keepalive 100;
}

server {
    listen 80;
    server_name veloz.example.com localhost;

    # Health check endpoint
    location /health {
        proxy_pass http://veloz_gateway/health;
        access_log off;
    }

    # Metrics endpoint
    location /metrics {
        proxy_pass http://veloz_gateway/metrics;
        proxy_http_version 1.1;
    }

    # API endpoints
    location /api/ {
        proxy_pass http://veloz_gateway;
        proxy_http_version 1.1;
        proxy_set_header Connection "";
        proxy_set_header Host \$host;
        proxy_set_header X-Real-IP \$remote_addr;
        proxy_set_header X-Forwarded-For \$proxy_add_x_forwarded_for;
        proxy_set_header X-Forwarded-Proto \$scheme;

        # Timeouts
        proxy_connect_timeout 5s;
        proxy_send_timeout 30s;
        proxy_read_timeout 30s;
    }

    # SSE streaming
    location /api/stream {
        proxy_pass http://veloz_gateway/api/stream;
        proxy_http_version 1.1;
        proxy_set_header Connection "";
        proxy_set_header Host \$host;
        proxy_set_header X-Real-IP \$remote_addr;

        # Disable buffering for SSE
        proxy_buffering off;
        proxy_cache off;
        proxy_read_timeout 3600s;
    }
}
EOF

    # Test nginx configuration
    if nginx -t 2>/dev/null; then
      # Reload nginx
      echo "Reloading nginx..."
      nginx -s reload 2>/dev/null || sudo nginx -s reload 2>/dev/null || {
        echo "Warning: Could not reload nginx (may require sudo)"
        echo "Manual reload required: sudo nginx -s reload"
      }
      echo "Nginx configuration updated successfully"
    else
      echo "ERROR: Nginx configuration test failed"
      exit 1
    fi
  else
    echo "Warning: No write access to ${NGINX_CONF}"
    echo "Manual configuration required:"
    echo ""
    echo "Create/update ${NGINX_CONF} with:"
    echo ""
    echo "upstream veloz_gateway {"
    echo "    server 127.0.0.1:8080 weight=${PY_WEIGHT} max_fails=3 fail_timeout=30s;"
    echo "    server 127.0.0.1:8081 weight=${CPP_WEIGHT} max_fails=3 fail_timeout=30s;"
    echo "    keepalive 100;"
    echo "}"
    echo ""
    echo "Then run: sudo nginx -s reload"
  fi
else
  echo "Nginx not found. Manual load balancer configuration required:"
  echo ""
  echo "Traffic split:"
  echo "  - Python Gateway (port 8080): ${PY_WEIGHT}%"
  echo "  - C++ Gateway (port 8081): ${CPP_WEIGHT}%"
  echo ""
  echo "Example nginx configuration:"
  echo ""
  echo "upstream veloz_gateway {"
  echo "    server 127.0.0.1:8080 weight=${PY_WEIGHT};"
  echo "    server 127.0.0.1:8081 weight=${CPP_WEIGHT};"
  echo "}"
  echo ""
fi

# Verify gateways are running
echo ""
echo "Verifying gateways..."

check_gateway() {
  local url=$1
  local name=$2

  if curl -s -f --max-time 2 "${url}/health" >/dev/null 2>&1; then
    echo "  ✓ ${name} is running"
    return 0
  else
    echo "  ✗ ${name} is not responding"
    return 1
  fi
}

PYTHON_OK=1
CPP_OK=1

if ! check_gateway "http://127.0.0.1:8080" "Python Gateway"; then
  PYTHON_OK=0
fi

if ! check_gateway "http://127.0.0.1:8081" "C++ Gateway"; then
  CPP_OK=0
fi

echo ""

if [[ ${CPP_WEIGHT} -gt 0 ]] && [[ ${CPP_OK} -eq 0 ]]; then
  echo "WARNING: C++ Gateway is not running but traffic weight is ${CPP_WEIGHT}%"
  echo "Start C++ Gateway: ./scripts/deploy_gateway_canary.sh"
fi

if [[ ${PY_WEIGHT} -gt 0 ]] && [[ ${PYTHON_OK} -eq 0 ]]; then
  echo "WARNING: Python Gateway is not running but traffic weight is ${PY_WEIGHT}%"
  echo "Start Python Gateway: ./scripts/run_gateway.sh"
fi

if [[ ${PYTHON_OK} -eq 0 ]] && [[ ${CPP_OK} -eq 0 ]]; then
  echo "ERROR: No gateways are running!"
  exit 1
fi

echo "Traffic split configuration complete"
