#!/bin/sh
# VeloZ Vault Initialization Script
# This script initializes Vault with policies, AppRole auth, and seed secrets

set -e

echo "=== VeloZ Vault Initialization ==="
echo "Waiting for Vault to be ready..."

# Wait for Vault to be fully ready
until vault status > /dev/null 2>&1; do
    echo "Waiting for Vault..."
    sleep 2
done

echo "Vault is ready. Starting initialization..."

# =============================================================================
# Enable Secret Engines
# =============================================================================
echo "Enabling KV v2 secrets engine..."
vault secrets enable -path=secret -version=2 kv 2>/dev/null || echo "KV engine already enabled"

# =============================================================================
# Create Policies
# =============================================================================
echo "Creating policies..."

vault policy write veloz-gateway /vault/policies/veloz-gateway.hcl
vault policy write veloz-engine /vault/policies/veloz-engine.hcl
vault policy write veloz-admin /vault/policies/veloz-admin.hcl

echo "Policies created successfully"

# =============================================================================
# Enable AppRole Authentication
# =============================================================================
echo "Enabling AppRole authentication..."
vault auth enable approle 2>/dev/null || echo "AppRole already enabled"

# Create AppRole for Gateway
echo "Creating AppRole for gateway..."
vault write auth/approle/role/veloz-gateway \
    token_policies="veloz-gateway" \
    token_ttl=1h \
    token_max_ttl=4h \
    secret_id_ttl=720h \
    secret_id_num_uses=0

# Create AppRole for Engine
echo "Creating AppRole for engine..."
vault write auth/approle/role/veloz-engine \
    token_policies="veloz-engine" \
    token_ttl=1h \
    token_max_ttl=4h \
    secret_id_ttl=720h \
    secret_id_num_uses=0

# =============================================================================
# Seed Initial Secrets (Development/Staging only)
# =============================================================================
echo "Seeding initial secrets..."

# Binance API credentials (placeholder - replace with actual values)
vault kv put secret/veloz/binance/credentials \
    api_key="${VELOZ_BINANCE_API_KEY:-}" \
    api_secret="${VELOZ_BINANCE_API_SECRET:-}" \
    testnet="true"

# Gateway secrets
vault kv put secret/veloz/gateway/auth \
    jwt_secret="${VELOZ_JWT_SECRET:-$(openssl rand -hex 32)}" \
    admin_password="${VELOZ_ADMIN_PASSWORD:-}"

# Database credentials (for future use)
vault kv put secret/veloz/database/postgres \
    host="postgres" \
    port="5432" \
    database="veloz" \
    username="veloz" \
    password="${POSTGRES_PASSWORD:-veloz_dev_password}"

# Common secrets
vault kv put secret/veloz/common/encryption \
    key="$(openssl rand -hex 32)"

# =============================================================================
# Output AppRole Credentials for Services
# =============================================================================
echo ""
echo "=== AppRole Credentials ==="
echo ""
echo "Gateway AppRole:"
GATEWAY_ROLE_ID=$(vault read -field=role_id auth/approle/role/veloz-gateway/role-id)
GATEWAY_SECRET_ID=$(vault write -f -field=secret_id auth/approle/role/veloz-gateway/secret-id)
echo "  VAULT_ROLE_ID=${GATEWAY_ROLE_ID}"
echo "  VAULT_SECRET_ID=${GATEWAY_SECRET_ID}"
echo ""
echo "Engine AppRole:"
ENGINE_ROLE_ID=$(vault read -field=role_id auth/approle/role/veloz-engine/role-id)
ENGINE_SECRET_ID=$(vault write -f -field=secret_id auth/approle/role/veloz-engine/secret-id)
echo "  VAULT_ROLE_ID=${ENGINE_ROLE_ID}"
echo "  VAULT_SECRET_ID=${ENGINE_SECRET_ID}"
echo ""

# =============================================================================
# Verification
# =============================================================================
echo "=== Verification ==="
echo "Listing secrets..."
vault kv list secret/veloz/

echo ""
echo "=== Vault Initialization Complete ==="
echo "Vault is ready for use with VeloZ services"
