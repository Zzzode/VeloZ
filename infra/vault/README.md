# VeloZ Vault Integration

HashiCorp Vault integration for secure secrets management in VeloZ.

## Quick Start (Development)

### 1. Start Vault

```bash
# Create the network if it doesn't exist
docker network create veloz-network 2>/dev/null || true

# Start Vault in dev mode
cd infra/vault
docker-compose up -d
```

### 2. Initialize Secrets

The `vault-init` container automatically:
- Enables KV v2 secrets engine
- Creates policies for gateway and engine
- Sets up AppRole authentication
- Seeds initial secrets from environment variables

### 3. Configure Gateway

Set these environment variables to enable Vault:

```bash
export VELOZ_VAULT_ENABLED=true
export VAULT_ADDR=http://127.0.0.1:8200
export VAULT_AUTH_METHOD=token
export VAULT_TOKEN=veloz-dev-token  # Dev mode token
```

For production with AppRole:

```bash
export VELOZ_VAULT_ENABLED=true
export VAULT_ADDR=http://vault:8200
export VAULT_AUTH_METHOD=approle
export VAULT_ROLE_ID=<role-id-from-init>
export VAULT_SECRET_ID=<secret-id-from-init>
```

## Secrets Structure

```
secret/veloz/
├── binance/
│   └── credentials     # api_key, api_secret, testnet
├── gateway/
│   └── auth            # jwt_secret, admin_password
├── database/
│   └── postgres        # host, port, database, username, password
└── common/
    └── encryption      # key
```

## Policies

| Policy | Description |
|--------|-------------|
| `veloz-gateway` | Read access to binance, gateway, database, common secrets |
| `veloz-engine` | Read access to binance, engine, common secrets |
| `veloz-admin` | Full access to all VeloZ secrets and AppRole management |

## AppRole Authentication

Services authenticate using AppRole for secure, automated authentication:

```bash
# Get role-id (static, can be stored in config)
vault read auth/approle/role/veloz-gateway/role-id

# Generate secret-id (rotate periodically)
vault write -f auth/approle/role/veloz-gateway/secret-id
```

## Secret Rotation

### Manual Rotation

```bash
# Rotate Binance credentials
vault kv put secret/veloz/binance/credentials \
    api_key="new-key" \
    api_secret="new-secret" \
    testnet="true"

# Rotate JWT secret (requires gateway restart)
vault kv put secret/veloz/gateway/auth \
    jwt_secret="$(openssl rand -hex 32)" \
    admin_password="new-password"
```

### Automated Rotation

For production, configure Vault's dynamic secrets or use external rotation tools.

## Environment Variable Fallback

When Vault is unavailable, the gateway falls back to environment variables:

| Vault Path | Environment Variable |
|------------|---------------------|
| `veloz/binance/credentials.api_key` | `VELOZ_BINANCE_API_KEY` |
| `veloz/binance/credentials.api_secret` | `VELOZ_BINANCE_API_SECRET` |
| `veloz/gateway/auth.jwt_secret` | `VELOZ_JWT_SECRET` |
| `veloz/gateway/auth.admin_password` | `VELOZ_ADMIN_PASSWORD` |
| `veloz/database/postgres.password` | `POSTGRES_PASSWORD` |

## Production Considerations

1. **TLS**: Enable TLS in production by providing certificates
2. **Storage**: Use Consul or Raft backend instead of file storage
3. **Auto-unseal**: Configure auto-unseal with AWS KMS, GCP KMS, or Azure Key Vault
4. **Audit**: Enable audit logging for compliance
5. **HA**: Deploy multiple Vault nodes for high availability

## Troubleshooting

### Check Vault Status

```bash
docker exec veloz-vault vault status
```

### View Logs

```bash
docker logs veloz-vault
docker logs veloz-vault-init
```

### Manual Secret Operations

```bash
# Read a secret
docker exec veloz-vault vault kv get secret/veloz/binance/credentials

# List secrets
docker exec veloz-vault vault kv list secret/veloz/
```
