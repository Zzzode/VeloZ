# VeloZ Engine Policy
# Allows the C++ engine to read trading-related secrets

# Read Binance API credentials for trading
path "secret/data/veloz/binance/*" {
  capabilities = ["read", "list"]
}

# Read engine-specific configuration secrets
path "secret/data/veloz/engine/*" {
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
