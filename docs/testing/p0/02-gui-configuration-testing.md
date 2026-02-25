# GUI Configuration Testing

## 1. Test Strategy

### 1.1 Objectives

- Verify all configuration fields have proper input validation
- Ensure API integration works with all supported exchanges
- Validate error handling and recovery mechanisms
- Confirm configuration persistence across sessions
- Test security of credential storage

### 1.2 Scope

| In Scope | Out of Scope |
|----------|--------------|
| Exchange API configuration | Exchange-specific trading features |
| Strategy parameter configuration | Strategy logic testing |
| Risk limit configuration | Risk calculation accuracy |
| UI/UX validation | Performance optimization |
| Credential storage security | Penetration testing |
| Configuration import/export | Cloud sync |

### 1.3 Supported Exchanges

| Exchange | API Type | Testnet Available |
|----------|----------|-------------------|
| Binance Spot | REST + WebSocket | Yes |
| Binance Futures | REST + WebSocket | Yes |
| OKX Spot | REST + WebSocket | Yes |
| OKX Futures | REST + WebSocket | Yes |
| Bybit Spot | REST + WebSocket | Yes |
| Bybit Futures | REST + WebSocket | Yes |

## 2. Test Environment

### 2.1 Test Data Requirements

| Data Type | Source | Purpose |
|-----------|--------|---------|
| Valid API keys | Testnet accounts | Positive testing |
| Invalid API keys | Generated | Negative testing |
| Expired API keys | Testnet (expired) | Error handling |
| Read-only API keys | Testnet | Permission testing |
| IP-restricted keys | Testnet | Security testing |

### 2.2 Network Conditions

| Condition | Tool | Purpose |
|-----------|------|---------|
| Normal | Direct connection | Baseline |
| High latency | tc/netem (500ms) | Timeout testing |
| Packet loss | tc/netem (10%) | Retry testing |
| Disconnection | Network disable | Reconnection testing |

## 3. Test Cases

### 3.1 Input Validation

#### TC-CFG-001: API Key Format Validation

| Field | Value |
|-------|-------|
| **ID** | TC-CFG-001 |
| **Priority** | P0 |
| **Preconditions** | VeloZ installed, configuration screen open |

**Test Data:**
| Input | Expected Result |
|-------|-----------------|
| Empty string | Error: "API key is required" |
| Spaces only | Error: "API key is required" |
| < 32 characters | Error: "API key too short" |
| > 128 characters | Error: "API key too long" |
| Contains spaces | Error: "API key contains invalid characters" |
| Valid format | Accepted |

**Steps:**
1. Open Exchange Configuration
2. Select Binance exchange
3. Enter each test input in API Key field
4. Click "Validate" or move to next field
5. Observe validation message

**Expected Results:**
- [ ] Real-time validation as user types
- [ ] Clear error messages displayed
- [ ] Error icon shown next to invalid field
- [ ] Submit button disabled until valid
- [ ] No sensitive data in error messages

#### TC-CFG-002: API Secret Format Validation

| Field | Value |
|-------|-------|
| **ID** | TC-CFG-002 |
| **Priority** | P0 |
| **Preconditions** | Configuration screen open |

**Test Data:**
| Input | Expected Result |
|-------|-----------------|
| Empty string | Error: "API secret is required" |
| < 32 characters | Error: "API secret too short" |
| Valid format | Accepted |

**Steps:**
1. Enter each test input in API Secret field
2. Observe validation

**Expected Results:**
- [ ] Secret field masked (password type)
- [ ] Show/hide toggle available
- [ ] Validation without revealing secret
- [ ] Copy-paste works correctly

#### TC-CFG-003: Numeric Field Validation

| Field | Value |
|-------|-------|
| **ID** | TC-CFG-003 |
| **Priority** | P0 |
| **Preconditions** | Risk configuration screen open |

**Test Data for "Max Position Size":**
| Input | Expected Result |
|-------|-----------------|
| Empty | Error: "Value required" |
| "abc" | Error: "Must be a number" |
| "-100" | Error: "Must be positive" |
| "0" | Error: "Must be greater than 0" |
| "999999999" | Error: "Exceeds maximum" |
| "1000.50" | Accepted |

**Steps:**
1. Open Risk Configuration
2. Enter each test input in Max Position Size field
3. Observe validation

**Expected Results:**
- [ ] Non-numeric input rejected
- [ ] Negative values rejected
- [ ] Range validation enforced
- [ ] Decimal precision handled correctly

#### TC-CFG-004: Dropdown Selection Validation

| Field | Value |
|-------|-------|
| **ID** | TC-CFG-004 |
| **Priority** | P1 |
| **Preconditions** | Configuration screen open |

**Steps:**
1. Open Exchange Configuration
2. Verify Exchange dropdown options
3. Select each exchange
4. Verify form fields update accordingly

**Expected Results:**
- [ ] All supported exchanges listed
- [ ] Exchange-specific fields shown/hidden
- [ ] Default values populated correctly
- [ ] Testnet toggle available for supported exchanges

### 3.2 API Integration Testing

#### TC-API-001: Binance API Connection Test

| Field | Value |
|-------|-------|
| **ID** | TC-API-001 |
| **Priority** | P0 |
| **Preconditions** | Valid Binance testnet API keys |

**Steps:**
1. Open Exchange Configuration
2. Select "Binance Spot"
3. Enable "Testnet" toggle
4. Enter valid API key and secret
5. Click "Test Connection"
6. Observe result

**Expected Results:**
- [ ] Connection test initiates with loading indicator
- [ ] Success message: "Connection successful"
- [ ] Account balance displayed (testnet)
- [ ] API permissions shown (read, trade, withdraw)
- [ ] Latency displayed (e.g., "Latency: 45ms")

#### TC-API-002: Binance API Invalid Credentials

| Field | Value |
|-------|-------|
| **ID** | TC-API-002 |
| **Priority** | P0 |
| **Preconditions** | Invalid API keys |

**Steps:**
1. Enter invalid API key/secret
2. Click "Test Connection"
3. Observe error handling

**Expected Results:**
- [ ] Error message: "Invalid API credentials"
- [ ] No stack trace or technical details shown
- [ ] Retry option available
- [ ] Credentials not logged in plain text

#### TC-API-003: API Connection Timeout

| Field | Value |
|-------|-------|
| **ID** | TC-API-003 |
| **Priority** | P1 |
| **Preconditions** | Network latency > 30 seconds |

**Steps:**
1. Enable network latency simulation
2. Enter valid credentials
3. Click "Test Connection"
4. Wait for timeout

**Expected Results:**
- [ ] Loading indicator shown
- [ ] Timeout after 30 seconds
- [ ] Error message: "Connection timed out"
- [ ] Retry option available
- [ ] Cancel option during loading

#### TC-API-004: API Rate Limiting

| Field | Value |
|-------|-------|
| **ID** | TC-API-004 |
| **Priority** | P1 |
| **Preconditions** | Valid credentials |

**Steps:**
1. Click "Test Connection" rapidly 10 times
2. Observe behavior

**Expected Results:**
- [ ] Rate limiting applied (max 1 request/second)
- [ ] Subsequent clicks disabled during request
- [ ] No duplicate requests sent
- [ ] Rate limit error handled gracefully

#### TC-API-005: OKX API Connection Test

| Field | Value |
|-------|-------|
| **ID** | TC-API-005 |
| **Priority** | P0 |
| **Preconditions** | Valid OKX testnet API keys |

**Steps:**
1. Select "OKX Spot"
2. Enable "Testnet"
3. Enter API key, secret, and passphrase
4. Click "Test Connection"

**Expected Results:**
- [ ] Passphrase field shown for OKX
- [ ] Connection successful
- [ ] Account info displayed

#### TC-API-006: Bybit API Connection Test

| Field | Value |
|-------|-------|
| **ID** | TC-API-006 |
| **Priority** | P0 |
| **Preconditions** | Valid Bybit testnet API keys |

**Steps:**
1. Select "Bybit Spot"
2. Enable "Testnet"
3. Enter API key and secret
4. Click "Test Connection"

**Expected Results:**
- [ ] Connection successful
- [ ] Account info displayed

### 3.3 Error Handling and Recovery

#### TC-ERR-001: Network Disconnection During Configuration

| Field | Value |
|-------|-------|
| **ID** | TC-ERR-001 |
| **Priority** | P0 |
| **Preconditions** | Configuration in progress |

**Steps:**
1. Start entering configuration
2. Disconnect network
3. Click "Save Configuration"
4. Reconnect network
5. Retry save

**Expected Results:**
- [ ] Offline status indicator shown
- [ ] Save fails with clear message
- [ ] Data not lost
- [ ] Auto-retry when network restored
- [ ] Success after reconnection

#### TC-ERR-002: Invalid Configuration Recovery

| Field | Value |
|-------|-------|
| **ID** | TC-ERR-002 |
| **Priority** | P1 |
| **Preconditions** | Corrupted config file |

**Steps:**
1. Manually corrupt config file
2. Launch VeloZ
3. Observe recovery behavior

**Expected Results:**
- [ ] Error detected on startup
- [ ] User prompted to reset or restore
- [ ] Backup config offered if available
- [ ] Fresh start option available
- [ ] No crash or data loss

#### TC-ERR-003: Concurrent Configuration Access

| Field | Value |
|-------|-------|
| **ID** | TC-ERR-003 |
| **Priority** | P2 |
| **Preconditions** | VeloZ running |

**Steps:**
1. Open configuration in VeloZ
2. Manually edit config file externally
3. Save changes in VeloZ

**Expected Results:**
- [ ] Conflict detected
- [ ] User prompted to resolve
- [ ] Option to reload external changes
- [ ] Option to overwrite with UI changes

### 3.4 Configuration Persistence

#### TC-PER-001: Configuration Save and Load

| Field | Value |
|-------|-------|
| **ID** | TC-PER-001 |
| **Priority** | P0 |
| **Preconditions** | Fresh VeloZ installation |

**Steps:**
1. Configure all settings:
   - Exchange: Binance Spot
   - API Key: [test key]
   - Max Position: 1000
   - Risk Limit: 5%
2. Save configuration
3. Close VeloZ
4. Reopen VeloZ
5. Verify all settings

**Expected Results:**
- [ ] All settings persisted
- [ ] API key masked but present
- [ ] Numeric values exact
- [ ] Dropdown selections preserved
- [ ] Toggle states preserved

#### TC-PER-002: Configuration Export

| Field | Value |
|-------|-------|
| **ID** | TC-PER-002 |
| **Priority** | P1 |
| **Preconditions** | Configuration complete |

**Steps:**
1. Click "Export Configuration"
2. Select export location
3. Choose export options:
   - Include API keys: No
   - Format: JSON
4. Export file
5. Verify exported content

**Expected Results:**
- [ ] File created successfully
- [ ] API keys NOT included (if unchecked)
- [ ] All other settings included
- [ ] Valid JSON format
- [ ] Human-readable structure

#### TC-PER-003: Configuration Import

| Field | Value |
|-------|-------|
| **ID** | TC-PER-003 |
| **Priority** | P1 |
| **Preconditions** | Exported config file |

**Steps:**
1. Click "Import Configuration"
2. Select config file
3. Preview import
4. Confirm import
5. Verify settings applied

**Expected Results:**
- [ ] Preview shows changes
- [ ] Conflicts highlighted
- [ ] Import successful
- [ ] Settings applied correctly
- [ ] API keys prompt if missing

### 3.5 Edge Cases

#### TC-EDGE-001: Maximum Field Lengths

| Field | Value |
|-------|-------|
| **ID** | TC-EDGE-001 |
| **Priority** | P1 |
| **Preconditions** | Configuration screen open |

**Steps:**
1. Enter maximum length strings in all text fields
2. Save configuration
3. Reload and verify

**Expected Results:**
- [ ] All fields accept max length
- [ ] No truncation on save
- [ ] No UI overflow issues
- [ ] Database/file handles max length

#### TC-EDGE-002: Special Characters in Fields

| Field | Value |
|-------|-------|
| **ID** | TC-EDGE-002 |
| **Priority** | P1 |
| **Preconditions** | Configuration screen open |

**Test Data:**
| Characters | Field | Expected |
|------------|-------|----------|
| `<script>` | Notes | Escaped/rejected |
| `'; DROP TABLE` | Notes | Escaped |
| Unicode emoji | Notes | Accepted |
| Null byte | API Key | Rejected |

**Expected Results:**
- [ ] XSS attempts blocked
- [ ] SQL injection attempts blocked
- [ ] Valid special characters accepted
- [ ] Proper encoding on save/load

#### TC-EDGE-003: Rapid Configuration Changes

| Field | Value |
|-------|-------|
| **ID** | TC-EDGE-003 |
| **Priority** | P2 |
| **Preconditions** | Configuration screen open |

**Steps:**
1. Rapidly change dropdown selections
2. Rapidly toggle checkboxes
3. Save configuration
4. Verify final state

**Expected Results:**
- [ ] UI remains responsive
- [ ] Final state saved correctly
- [ ] No race conditions
- [ ] No duplicate saves

### 3.6 Security Testing

#### TC-SEC-001: Credential Storage Encryption

| Field | Value |
|-------|-------|
| **ID** | TC-SEC-001 |
| **Priority** | P0 |
| **Preconditions** | API keys configured |

**Steps:**
1. Configure API keys
2. Save configuration
3. Locate config file on disk
4. Examine file contents

**Expected Results:**
- [ ] API keys NOT in plain text
- [ ] Encryption algorithm documented
- [ ] Key derivation uses secure method
- [ ] Different encryption per installation

#### TC-SEC-002: Credential Memory Protection

| Field | Value |
|-------|-------|
| **ID** | TC-SEC-002 |
| **Priority** | P0 |
| **Preconditions** | API keys configured |

**Steps:**
1. Configure API keys
2. Create memory dump of process
3. Search for API key in dump

**Expected Results:**
- [ ] API keys not in plain text in memory
- [ ] Secure string handling used
- [ ] Memory cleared after use

#### TC-SEC-003: Credential Transmission Security

| Field | Value |
|-------|-------|
| **ID** | TC-SEC-003 |
| **Priority** | P0 |
| **Preconditions** | Network capture enabled |

**Steps:**
1. Configure API keys
2. Test connection to exchange
3. Capture network traffic
4. Analyze captured packets

**Expected Results:**
- [ ] All traffic over HTTPS/WSS
- [ ] No plain text credentials in packets
- [ ] Certificate validation enabled
- [ ] TLS 1.2+ used

#### TC-SEC-004: Session Timeout

| Field | Value |
|-------|-------|
| **ID** | TC-SEC-004 |
| **Priority** | P1 |
| **Preconditions** | VeloZ running with credentials |

**Steps:**
1. Leave VeloZ idle for 30 minutes
2. Attempt to perform action
3. Observe behavior

**Expected Results:**
- [ ] Session timeout warning shown
- [ ] Re-authentication required for sensitive actions
- [ ] Credentials not exposed after timeout

## 4. Automated Test Scripts

### 4.1 Configuration Validation Tests (Python)

```python
# File: tests/gui/test_configuration.py

import pytest
from veloz.config import ConfigManager, ConfigValidator

class TestAPIKeyValidation:
    """Test API key format validation."""

    @pytest.fixture
    def validator(self):
        return ConfigValidator()

    def test_empty_api_key_rejected(self, validator):
        result = validator.validate_api_key("")
        assert not result.is_valid
        assert "required" in result.error.lower()

    def test_short_api_key_rejected(self, validator):
        result = validator.validate_api_key("abc123")
        assert not result.is_valid
        assert "too short" in result.error.lower()

    def test_api_key_with_spaces_rejected(self, validator):
        result = validator.validate_api_key("abc 123 def 456")
        assert not result.is_valid
        assert "invalid characters" in result.error.lower()

    def test_valid_api_key_accepted(self, validator):
        valid_key = "a" * 64  # 64 character key
        result = validator.validate_api_key(valid_key)
        assert result.is_valid

    @pytest.mark.parametrize("value,expected_valid", [
        ("", False),
        ("-100", False),
        ("0", False),
        ("abc", False),
        ("100.50", True),
        ("1000", True),
    ])
    def test_numeric_validation(self, validator, value, expected_valid):
        result = validator.validate_numeric(value, min_value=0.01)
        assert result.is_valid == expected_valid


class TestConfigPersistence:
    """Test configuration save/load."""

    @pytest.fixture
    def config_manager(self, tmp_path):
        return ConfigManager(config_dir=tmp_path)

    def test_save_and_load_config(self, config_manager):
        # Save config
        config = {
            "exchange": "binance",
            "testnet": True,
            "max_position": 1000.0,
            "risk_limit": 0.05,
        }
        config_manager.save(config)

        # Load config
        loaded = config_manager.load()
        assert loaded["exchange"] == "binance"
        assert loaded["testnet"] is True
        assert loaded["max_position"] == 1000.0
        assert loaded["risk_limit"] == 0.05

    def test_api_keys_encrypted(self, config_manager, tmp_path):
        config = {
            "api_key": "secret_api_key_12345",
            "api_secret": "secret_api_secret_67890",
        }
        config_manager.save(config)

        # Read raw file
        config_file = tmp_path / "config.json"
        raw_content = config_file.read_text()

        # Keys should not be in plain text
        assert "secret_api_key_12345" not in raw_content
        assert "secret_api_secret_67890" not in raw_content

    def test_export_without_credentials(self, config_manager, tmp_path):
        config = {
            "exchange": "binance",
            "api_key": "secret_key",
            "api_secret": "secret_secret",
        }
        config_manager.save(config)

        # Export without credentials
        export_path = tmp_path / "export.json"
        config_manager.export(export_path, include_credentials=False)

        # Verify export
        import json
        with open(export_path) as f:
            exported = json.load(f)

        assert "api_key" not in exported
        assert "api_secret" not in exported
        assert exported["exchange"] == "binance"


class TestAPIConnection:
    """Test exchange API connection."""

    @pytest.fixture
    def api_client(self):
        from veloz.exchange import ExchangeClient
        return ExchangeClient()

    @pytest.mark.integration
    def test_binance_testnet_connection(self, api_client):
        result = api_client.test_connection(
            exchange="binance",
            testnet=True,
            api_key="valid_testnet_key",
            api_secret="valid_testnet_secret",
        )
        assert result.success
        assert result.latency_ms < 5000

    def test_invalid_credentials_error(self, api_client):
        result = api_client.test_connection(
            exchange="binance",
            testnet=True,
            api_key="invalid_key",
            api_secret="invalid_secret",
        )
        assert not result.success
        assert "invalid" in result.error.lower()
        # Ensure no sensitive data in error
        assert "invalid_key" not in result.error
        assert "invalid_secret" not in result.error
```

### 4.2 UI Automation Tests (Playwright)

```typescript
// File: tests/gui/configuration.spec.ts

import { test, expect } from '@playwright/test';

test.describe('Configuration GUI', () => {
  test.beforeEach(async ({ page }) => {
    await page.goto('/config');
  });

  test('should validate API key format', async ({ page }) => {
    // Empty key
    await page.fill('#api-key', '');
    await page.click('#validate-btn');
    await expect(page.locator('.error-message')).toContainText('required');

    // Short key
    await page.fill('#api-key', 'abc');
    await expect(page.locator('.error-message')).toContainText('too short');

    // Valid key
    await page.fill('#api-key', 'a'.repeat(64));
    await expect(page.locator('.error-message')).not.toBeVisible();
  });

  test('should mask API secret field', async ({ page }) => {
    const secretField = page.locator('#api-secret');
    await expect(secretField).toHaveAttribute('type', 'password');

    // Toggle visibility
    await page.click('#toggle-secret-visibility');
    await expect(secretField).toHaveAttribute('type', 'text');

    await page.click('#toggle-secret-visibility');
    await expect(secretField).toHaveAttribute('type', 'password');
  });

  test('should show exchange-specific fields', async ({ page }) => {
    // Binance - no passphrase
    await page.selectOption('#exchange', 'binance');
    await expect(page.locator('#passphrase')).not.toBeVisible();

    // OKX - has passphrase
    await page.selectOption('#exchange', 'okx');
    await expect(page.locator('#passphrase')).toBeVisible();
  });

  test('should test connection successfully', async ({ page }) => {
    await page.selectOption('#exchange', 'binance');
    await page.check('#testnet');
    await page.fill('#api-key', process.env.BINANCE_TESTNET_KEY!);
    await page.fill('#api-secret', process.env.BINANCE_TESTNET_SECRET!);

    await page.click('#test-connection');

    // Wait for connection test
    await expect(page.locator('.connection-status')).toContainText('successful', {
      timeout: 30000,
    });
  });

  test('should save and persist configuration', async ({ page }) => {
    // Fill configuration
    await page.selectOption('#exchange', 'binance');
    await page.check('#testnet');
    await page.fill('#max-position', '1000');
    await page.fill('#risk-limit', '5');

    // Save
    await page.click('#save-config');
    await expect(page.locator('.save-status')).toContainText('saved');

    // Reload page
    await page.reload();

    // Verify persistence
    await expect(page.locator('#exchange')).toHaveValue('binance');
    await expect(page.locator('#testnet')).toBeChecked();
    await expect(page.locator('#max-position')).toHaveValue('1000');
    await expect(page.locator('#risk-limit')).toHaveValue('5');
  });

  test('should handle network error gracefully', async ({ page }) => {
    // Simulate offline
    await page.route('**/api/**', (route) => route.abort());

    await page.click('#test-connection');

    await expect(page.locator('.error-message')).toContainText('network');
    await expect(page.locator('#retry-btn')).toBeVisible();
  });
});
```

## 5. Performance Benchmarks

### 5.1 Response Time Targets

| Action | Target | Maximum |
|--------|--------|---------|
| Form validation | < 50ms | 100ms |
| Save configuration | < 200ms | 500ms |
| Load configuration | < 100ms | 300ms |
| API connection test | < 5s | 30s |
| Export configuration | < 500ms | 2s |
| Import configuration | < 500ms | 2s |

### 5.2 UI Responsiveness

| Metric | Target |
|--------|--------|
| First Contentful Paint | < 1s |
| Time to Interactive | < 2s |
| Input latency | < 50ms |
| Animation frame rate | 60 FPS |

## 6. Security Testing Checklist

### 6.1 Credential Security

- [ ] API keys encrypted at rest (AES-256 or better)
- [ ] API secrets never logged
- [ ] Credentials cleared from memory after use
- [ ] No credentials in URL parameters
- [ ] No credentials in browser local storage (plain text)
- [ ] Secure key derivation (PBKDF2, Argon2)

### 6.2 Input Security

- [ ] XSS prevention (input sanitization)
- [ ] SQL injection prevention (parameterized queries)
- [ ] Path traversal prevention
- [ ] Command injection prevention
- [ ] CSRF protection on forms

### 6.3 Transport Security

- [ ] HTTPS only for API calls
- [ ] Certificate pinning (optional)
- [ ] TLS 1.2+ required
- [ ] No mixed content

### 6.4 Access Control

- [ ] Session management secure
- [ ] Session timeout implemented
- [ ] Re-authentication for sensitive changes
- [ ] Audit logging for configuration changes

## 7. UAT Plan

### 7.1 UAT Scenarios

1. **New User Configuration**
   - First-time setup wizard
   - Exchange selection
   - API key entry
   - Connection test
   - Risk limit configuration

2. **Experienced User Configuration**
   - Multiple exchange setup
   - Advanced risk parameters
   - Strategy parameter tuning
   - Configuration backup/restore

3. **Error Recovery**
   - Invalid API key handling
   - Network failure recovery
   - Configuration corruption recovery

### 7.2 UAT Success Criteria

- Configuration completion rate > 95%
- Average configuration time < 10 minutes
- Error message clarity rating > 4/5
- No data loss during configuration
- All exchanges connect successfully on testnet

## 8. Bug Tracking

### 8.1 Common Issues

| ID | Description | Severity | Resolution |
|----|-------------|----------|------------|
| CFG-001 | API key validation too strict | S2 | Relaxed regex |
| CFG-002 | Passphrase field not shown for OKX | S1 | Added exchange-specific fields |
| CFG-003 | Config not saved on browser close | S1 | Added auto-save |
