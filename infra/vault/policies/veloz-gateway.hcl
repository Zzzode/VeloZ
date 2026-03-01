# VeloZ Gateway Policy
# Allows the gateway service to read secrets needed for operation

# Read Binance API credentials
path "secret/data/veloz/binance/*" {
  capabilities = ["read", "list"]
}

# Read gateway-specific secrets (JWT secret, admin password)
path "secret/data/veloz/gateway/*" {
  capabilities = ["read", "list"]
}

# Read database credentials
path "secret/data/veloz/database/*" {
  capabilities = ["read", "list"]
}

# Read common secrets
path "secret/data/veloz/common/*" {
  capabilities = ["read", "list"]
}

# Allow token renewal
path "auth/token/renew-self" {
  capabilities = ["update"]
}

# Allow looking up own token info
path "auth/token/lookup-self" {
  capabilities = ["read"]
}
