# HashiCorp Vault Production Configuration for VeloZ
# This configuration enables TLS, HA storage, and auto-unseal

# Raft storage backend for HA
storage "raft" {
  path    = "/vault/data"
  node_id = "vault-1"

  retry_join {
    leader_api_addr = "https://vault-0.vault-internal:8200"
  }
  retry_join {
    leader_api_addr = "https://vault-1.vault-internal:8200"
  }
  retry_join {
    leader_api_addr = "https://vault-2.vault-internal:8200"
  }
}

# TLS-enabled listener
listener "tcp" {
  address            = "0.0.0.0:8200"
  cluster_address    = "0.0.0.0:8201"
  tls_cert_file      = "/vault/tls/tls.crt"
  tls_key_file       = "/vault/tls/tls.key"
  tls_client_ca_file = "/vault/tls/ca.crt"
  tls_min_version    = "tls12"

  # Enable client certificate authentication (optional)
  tls_require_and_verify_client_cert = false
}

# API address (HTTPS)
api_addr = "https://vault.veloz.example.com:8200"

# Cluster address for HA mode
cluster_addr = "https://vault-0.vault-internal:8201"

# AWS KMS auto-unseal
seal "awskms" {
  region     = "us-east-1"
  kms_key_id = "alias/veloz-vault-unseal"
  # IAM role for KMS access should be configured via IRSA
}

# Disable UI in production (access via CLI/API only)
ui = false

# Disable mlock for containerized environments
disable_mlock = true

# Telemetry for Prometheus metrics
telemetry {
  prometheus_retention_time = "30s"
  disable_hostname          = true
  unauthenticated_metrics_access = false
}

# Audit logging - file backend
# In production, also enable syslog or socket backend for centralized logging
audit {
  type = "file"
  path = "file"
  options = {
    file_path = "/vault/audit/audit.log"
  }
}

# Log level
log_level = "warn"

# Max lease TTL (32 days)
max_lease_ttl = "768h"

# Default lease TTL (32 days)
default_lease_ttl = "768h"

# Service registration for Kubernetes
service_registration "kubernetes" {}
