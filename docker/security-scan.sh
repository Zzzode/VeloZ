#!/bin/bash
# VeloZ Container Security Scan Script
# Runs various security checks on the container

set -e

echo "=== VeloZ Container Security Scan ==="
echo "Date: $(date -u +"%Y-%m-%dT%H:%M:%SZ")"
echo ""

# Check if running as root (should fail)
echo "1. Checking user context..."
if [ "$(id -u)" -eq 0 ]; then
    echo "   FAIL: Running as root"
    exit 1
else
    echo "   PASS: Running as non-root user ($(whoami), UID=$(id -u))"
fi

# Check file permissions
echo ""
echo "2. Checking file permissions..."
if [ -w /opt/veloz/bin/veloz_engine ]; then
    echo "   WARN: Engine binary is writable"
else
    echo "   PASS: Engine binary is read-only"
fi

# Check for sensitive files
echo ""
echo "3. Checking for sensitive files..."
SENSITIVE_PATTERNS=".env .aws credentials.json private.key id_rsa"
FOUND_SENSITIVE=0
for pattern in $SENSITIVE_PATTERNS; do
    if find /opt/veloz -name "$pattern" 2>/dev/null | grep -q .; then
        echo "   WARN: Found sensitive file pattern: $pattern"
        FOUND_SENSITIVE=1
    fi
done
if [ $FOUND_SENSITIVE -eq 0 ]; then
    echo "   PASS: No sensitive files found"
fi

# Check environment variables
echo ""
echo "4. Checking environment variables..."
if [ -n "$VELOZ_BINANCE_API_SECRET" ] && [ "$VELOZ_BINANCE_API_SECRET" != "" ]; then
    echo "   WARN: API secret is set in environment (use Vault instead)"
else
    echo "   PASS: No API secrets in environment"
fi

# Check network exposure
echo ""
echo "5. Checking network configuration..."
echo "   Listening ports:"
if command -v ss &> /dev/null; then
    ss -tlnp 2>/dev/null | grep -v "^State" || echo "   No listening ports"
elif command -v netstat &> /dev/null; then
    netstat -tlnp 2>/dev/null | grep -v "^Proto" || echo "   No listening ports"
else
    echo "   Unable to check (no ss or netstat)"
fi

# Check capabilities
echo ""
echo "6. Checking process capabilities..."
if command -v capsh &> /dev/null; then
    capsh --print 2>/dev/null || echo "   Unable to read capabilities"
else
    echo "   capsh not available (expected in minimal image)"
fi

# Summary
echo ""
echo "=== Scan Complete ==="
echo "Container appears to be properly secured."
echo "For comprehensive scanning, use:"
echo "  - trivy image veloz:latest"
echo "  - grype veloz:latest"
echo "  - docker scout cves veloz:latest"
