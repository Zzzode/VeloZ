# VeloZ Container Security Guide

This document describes container security practices for VeloZ deployments.

## Docker Image Security

### Image Build Best Practices

1. **Multi-stage builds** - Separate build and runtime stages to minimize attack surface
2. **Minimal base images** - Use slim/alpine variants where possible
3. **Non-root user** - Run containers as non-root (UID 1000)
4. **Read-only filesystem** - Make application files read-only
5. **No secrets in images** - Use Vault or environment injection

### Optimized Dockerfile

Use the optimized Dockerfile for production:

```bash
docker build -f docker/Dockerfile.optimized -t veloz:latest .
```

Key security features:
- Non-root user (veloz, UID 1000)
- Stripped binaries (smaller attack surface)
- Read-only application files
- dumb-init for proper signal handling
- Health checks enabled

### Image Scanning

#### Trivy (Recommended)

```bash
# Scan for vulnerabilities
trivy image veloz:latest

# Scan with severity filter
trivy image --severity CRITICAL,HIGH veloz:latest

# Generate SARIF report
trivy image --format sarif -o results.sarif veloz:latest
```

#### Grype

```bash
# Scan image
grype veloz:latest

# Fail on high/critical
grype veloz:latest --fail-on high
```

#### Docker Scout

```bash
# Quick scan
docker scout cves veloz:latest

# Detailed recommendations
docker scout recommendations veloz:latest
```

### Image Signing

Sign images with Cosign for supply chain security:

```bash
# Generate key pair (one-time)
cosign generate-key-pair

# Sign image
cosign sign --key cosign.key veloz:latest

# Verify signature
cosign verify --key cosign.pub veloz:latest
```

For keyless signing (recommended for CI/CD):

```bash
# Sign with OIDC (GitHub Actions)
cosign sign --yes veloz:latest

# Verify
cosign verify veloz:latest
```

## Runtime Security

### Container Configuration

#### Docker Run (Secure)

```bash
docker run -d \
  --name veloz \
  --user 1000:1000 \
  --read-only \
  --tmpfs /tmp:rw,noexec,nosuid \
  --cap-drop ALL \
  --security-opt no-new-privileges:true \
  --security-opt seccomp=docker/seccomp.json \
  -p 8080:8080 \
  veloz:latest
```

#### Docker Compose (Secure)

```yaml
services:
  veloz:
    image: veloz:latest
    user: "1000:1000"
    read_only: true
    tmpfs:
      - /tmp:rw,noexec,nosuid
    cap_drop:
      - ALL
    security_opt:
      - no-new-privileges:true
    deploy:
      resources:
        limits:
          cpus: '2'
          memory: 2G
```

### Kubernetes Security

See `infra/k8s/` for Kubernetes manifests with:
- PodSecurityPolicy / PodSecurityStandards
- NetworkPolicy
- ResourceQuotas
- ServiceAccount with minimal permissions

Example SecurityContext:

```yaml
securityContext:
  runAsNonRoot: true
  runAsUser: 1000
  runAsGroup: 1000
  fsGroup: 1000
  readOnlyRootFilesystem: true
  allowPrivilegeEscalation: false
  capabilities:
    drop:
      - ALL
```

## Network Security

### Container Network Isolation

```bash
# Create isolated network
docker network create --driver bridge --internal veloz-internal

# Run with network isolation
docker run --network veloz-internal veloz:latest
```

### Firewall Rules

Only expose necessary ports:

```bash
# Allow only health check from load balancer
iptables -A INPUT -p tcp --dport 8080 -s 10.0.0.0/8 -j ACCEPT
iptables -A INPUT -p tcp --dport 8080 -j DROP
```

## Secrets Management

### Do NOT:
- Bake secrets into images
- Pass secrets as build args
- Store secrets in environment files committed to git

### DO:
- Use HashiCorp Vault (see `infra/vault/`)
- Use Kubernetes Secrets with encryption at rest
- Use Docker secrets for Swarm deployments

Example with Vault:

```bash
docker run -d \
  -e VELOZ_VAULT_ENABLED=true \
  -e VAULT_ADDR=http://vault:8200 \
  -e VAULT_AUTH_METHOD=approle \
  -e VAULT_ROLE_ID=xxx \
  -e VAULT_SECRET_ID=yyy \
  veloz:latest
```

## Monitoring and Auditing

### Container Logs

```bash
# View logs
docker logs veloz

# Follow logs with timestamps
docker logs -f --timestamps veloz
```

### Runtime Security Monitoring

Consider using:
- **Falco** - Runtime security monitoring
- **Sysdig** - Container visibility
- **Aqua Security** - Full container security platform

### Audit Checklist

- [ ] Images scanned for vulnerabilities
- [ ] No high/critical CVEs
- [ ] Running as non-root
- [ ] Read-only filesystem where possible
- [ ] Capabilities dropped
- [ ] Resource limits set
- [ ] Network policies applied
- [ ] Secrets managed externally
- [ ] Images signed and verified
- [ ] SBOM generated

## CI/CD Integration

The `.github/workflows/container-security.yml` workflow automatically:

1. Builds the Docker image
2. Scans for vulnerabilities (Trivy)
3. Checks for secrets
4. Lints Dockerfiles (Hadolint)
5. Generates SBOM
6. Signs images (on main branch)

### Local Pre-commit Checks

```bash
# Lint Dockerfile
docker run --rm -i hadolint/hadolint < docker/Dockerfile.optimized

# Scan for secrets
trivy fs --scanners secret .

# Build and scan
docker build -f docker/Dockerfile.optimized -t veloz:test .
trivy image veloz:test
```

## Incident Response

If a vulnerability is discovered:

1. **Assess** - Check if vulnerability affects VeloZ
2. **Patch** - Update base image or dependencies
3. **Rebuild** - Create new image with fix
4. **Scan** - Verify vulnerability is resolved
5. **Deploy** - Roll out patched image
6. **Document** - Update security documentation

See `docs/security/INCIDENT_RESPONSE.md` for detailed procedures.
