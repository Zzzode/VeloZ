# VeloZ Admin Policy
# Full access to VeloZ secrets for administrative operations

# Full access to all VeloZ secrets
path "secret/data/veloz/*" {
  capabilities = ["create", "read", "update", "delete", "list"]
}

path "secret/metadata/veloz/*" {
  capabilities = ["read", "list", "delete"]
}

# Manage AppRole authentication
path "auth/approle/role/veloz-*" {
  capabilities = ["create", "read", "update", "delete", "list"]
}

path "auth/approle/role/veloz-*/secret-id" {
  capabilities = ["create", "update"]
}

path "auth/approle/role/veloz-*/role-id" {
  capabilities = ["read"]
}

# Manage policies
path "sys/policies/acl/veloz-*" {
  capabilities = ["create", "read", "update", "delete", "list"]
}

# Token management
path "auth/token/create" {
  capabilities = ["create", "update"]
}

path "auth/token/renew-self" {
  capabilities = ["update"]
}

path "auth/token/lookup-self" {
  capabilities = ["read"]
}

# View audit logs
path "sys/audit" {
  capabilities = ["read", "list"]
}

# Health and status
path "sys/health" {
  capabilities = ["read"]
}

path "sys/seal-status" {
  capabilities = ["read"]
}
