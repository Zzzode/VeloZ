# HashiCorp Vault Configuration for VeloZ
# This configuration is for development/staging environments
# Production should use HA backend (Consul, Raft) and auto-unseal

# Storage backend - file for development, use Consul/Raft for production
storage "file" {
  path = "/vault/data"
}

# Listener configuration
listener "tcp" {
  address     = "0.0.0.0:8200"
  tls_disable = "true"  # Enable TLS in production with proper certificates
}

# API address
api_addr = "http://0.0.0.0:8200"

# Cluster address for HA mode
cluster_addr = "https://0.0.0.0:8201"

# UI enabled for development
ui = true

# Disable mlock for containerized environments
disable_mlock = true

# Telemetry for Prometheus metrics
telemetry {
  prometheus_retention_time = "30s"
  disable_hostname          = true
}

# Log level
log_level = "info"

# Max lease TTL
max_lease_ttl = "768h"

# Default lease TTL
default_lease_ttl = "768h"
