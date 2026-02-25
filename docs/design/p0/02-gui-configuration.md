# P0-2: GUI Configuration System

**Version**: 1.0.0
**Date**: 2026-02-25
**Status**: Design Phase
**Complexity**: Medium
**Estimated Time**: 3-4 weeks

---

## 1. Overview

### 1.1 Goals

- Provide intuitive GUI for all configuration tasks
- Secure API key management with OS keychain integration
- First-run setup wizard for new users
- Exchange connection testing and validation
- Risk parameter configuration with safety guardrails

### 1.2 Non-Goals

- Command-line configuration tools
- Programmatic API for configuration
- Multi-user/team configuration management

---

## 2. Architecture

### 2.1 High-Level Architecture

```
                    GUI Configuration Architecture
                    ==============================

+------------------------------------------------------------------+
|                         React UI Layer                            |
+------------------------------------------------------------------+
|  +----------------+  +----------------+  +----------------+       |
|  | Setup Wizard   |  | Settings Panel |  | Connection     |       |
|  | Component      |  | Component      |  | Tester         |       |
|  +----------------+  +----------------+  +----------------+       |
|           |                  |                  |                 |
|           v                  v                  v                 |
|  +----------------+  +----------------+  +----------------+       |
|  | Form Validation|  | Config Store   |  | API Client     |       |
|  | (Zod schemas)  |  | (Zustand)      |  | (REST)         |       |
|  +----------------+  +----------------+  +----------------+       |
+------------------------------------------------------------------+
                               |
                               v (REST API)
+------------------------------------------------------------------+
|                      Python Gateway Layer                         |
+------------------------------------------------------------------+
|  +----------------+  +----------------+  +----------------+       |
|  | Config API     |  | Credential     |  | Exchange       |       |
|  | Endpoints      |  | Vault          |  | Connector      |       |
|  +----------------+  +----------------+  +----------------+       |
|           |                  |                  |                 |
|           v                  v                  v                 |
|  +----------------+  +----------------+  +----------------+       |
|  | Config         |  | OS Keychain    |  | Exchange       |       |
|  | Persistence    |  | Integration    |  | APIs           |       |
|  +----------------+  +----------------+  +----------------+       |
+------------------------------------------------------------------+
                               |
                               v (stdio/config file)
+------------------------------------------------------------------+
|                       C++ Engine Layer                            |
+------------------------------------------------------------------+
|  +----------------+  +----------------+  +----------------+       |
|  | ConfigManager  |  | Hot Reload     |  | Validation     |       |
|  | (libs/core)    |  | Handler        |  | Engine         |       |
|  +----------------+  +----------------+  +----------------+       |
+------------------------------------------------------------------+
```

### 2.2 Component Diagram

```
+------------------------------------------------------------------+
|                    Configuration System                           |
+------------------------------------------------------------------+
|                                                                   |
|  UI Components                                                    |
|  +----------------+  +----------------+  +----------------+       |
|  | SetupWizard    |  | SettingsPage   |  | APIKeyManager  |       |
|  | - Welcome      |  | - General      |  | - Add key      |       |
|  | - Exchange     |  | - Trading      |  | - Test key     |       |
|  | - Risk         |  | - Risk         |  | - Rotate key   |       |
|  | - Complete     |  | - Advanced     |  | - Delete key   |       |
|  +----------------+  +----------------+  +----------------+       |
|                                                                   |
|  State Management                                                 |
|  +----------------+  +----------------+  +----------------+       |
|  | configStore    |  | wizardStore    |  | keyStore       |       |
|  | - settings     |  | - step         |  | - keys[]       |       |
|  | - isDirty      |  | - data         |  | - status       |       |
|  | - save()       |  | - validate()   |  | - test()       |       |
|  +----------------+  +----------------+  +----------------+       |
|                                                                   |
|  API Layer                                                        |
|  +----------------+  +----------------+  +----------------+       |
|  | configApi      |  | keychainApi    |  | exchangeApi    |       |
|  | - get/set      |  | - store        |  | - testConn     |       |
|  | - validate     |  | - retrieve     |  | - getBalance   |       |
|  | - export       |  | - delete       |  | - getSymbols   |       |
|  +----------------+  +----------------+  +----------------+       |
|                                                                   |
+------------------------------------------------------------------+
```

---

## 3. First-Run Setup Wizard

### 3.1 Wizard Flow

```
                        Setup Wizard Flow
                        =================

+------------------+
| 1. Welcome       |
| - Introduction   |
| - Terms accept   |
+------------------+
        |
        v
+------------------+
| 2. Exchange      |
| - Select exchange|
| - Enter API keys |
| - Test connection|
+------------------+
        |
        v
+------------------+
| 3. Risk Settings |
| - Max position   |
| - Daily loss limit|
| - Circuit breaker|
+------------------+
        |
        v
+------------------+
| 4. Trading Mode  |
| - Simulated      |
| - Paper trading  |
| - Live (locked)  |
+------------------+
        |
        v
+------------------+
| 5. Security      |
| - 2FA setup      |
| - Backup codes   |
| - Recovery email |
+------------------+
        |
        v
+------------------+
| 6. Complete      |
| - Summary        |
| - Launch app     |
+------------------+
```

### 3.2 Wizard Component Structure

```typescript
// src/features/setup/SetupWizard.tsx
interface WizardStep {
  id: string;
  title: string;
  description: string;
  component: React.ComponentType<StepProps>;
  validation: z.ZodSchema;
  canSkip: boolean;
}

const WIZARD_STEPS: WizardStep[] = [
  {
    id: 'welcome',
    title: 'Welcome to VeloZ',
    description: 'Get started with your trading journey',
    component: WelcomeStep,
    validation: z.object({ termsAccepted: z.literal(true) }),
    canSkip: false,
  },
  {
    id: 'exchange',
    title: 'Connect Exchange',
    description: 'Add your exchange API credentials',
    component: ExchangeStep,
    validation: exchangeConfigSchema,
    canSkip: false,
  },
  {
    id: 'risk',
    title: 'Risk Settings',
    description: 'Configure your risk management',
    component: RiskStep,
    validation: riskConfigSchema,
    canSkip: false,
  },
  {
    id: 'mode',
    title: 'Trading Mode',
    description: 'Choose how you want to trade',
    component: TradingModeStep,
    validation: tradingModeSchema,
    canSkip: false,
  },
  {
    id: 'security',
    title: 'Security Setup',
    description: 'Protect your account',
    component: SecurityStep,
    validation: securityConfigSchema,
    canSkip: true,
  },
  {
    id: 'complete',
    title: 'Setup Complete',
    description: 'You are ready to start trading',
    component: CompleteStep,
    validation: z.object({}),
    canSkip: false,
  },
];
```

### 3.3 Wizard State Management

```typescript
// src/stores/wizardStore.ts
interface WizardState {
  currentStep: number;
  completedSteps: Set<string>;
  data: WizardData;
  errors: Record<string, string[]>;

  // Actions
  nextStep: () => void;
  prevStep: () => void;
  goToStep: (step: number) => void;
  updateData: (partial: Partial<WizardData>) => void;
  validateStep: (stepId: string) => Promise<boolean>;
  complete: () => Promise<void>;
  reset: () => void;
}

interface WizardData {
  // Step 1: Welcome
  termsAccepted: boolean;

  // Step 2: Exchange
  exchange: 'binance' | 'binance_futures' | 'okx';
  apiKey: string;
  apiSecret: string;
  testnet: boolean;

  // Step 3: Risk
  maxPositionSize: number;
  dailyLossLimit: number;
  circuitBreakerEnabled: boolean;
  circuitBreakerThreshold: number;

  // Step 4: Trading Mode
  tradingMode: 'simulated' | 'paper' | 'live';
  simulatedBalance: number;

  // Step 5: Security
  twoFactorEnabled: boolean;
  recoveryEmail: string;
}
```

---

## 4. API Key Management

### 4.1 Keychain Integration Architecture

```
                    Keychain Integration
                    ====================

+------------------+     +------------------+     +------------------+
| React UI         |     | Gateway          |     | OS Keychain      |
+------------------+     +------------------+     +------------------+
        |                        |                        |
        |--Store API Key-------->|                        |
        |                        |--Encrypt + Store------>|
        |                        |                        |
        |<--Success--------------|<--Success--------------|
        |                        |                        |
        |--Retrieve Key--------->|                        |
        |                        |--Retrieve------------->|
        |                        |<--Encrypted Key--------|
        |                        |--Decrypt-------------->|
        |<--Decrypted Key--------|                        |
        |                        |                        |
```

### 4.2 Platform-Specific Keychain APIs

```python
# apps/gateway/keychain.py
import platform
from abc import ABC, abstractmethod
from typing import Optional

class KeychainBackend(ABC):
    @abstractmethod
    def store(self, service: str, account: str, secret: str) -> bool:
        pass

    @abstractmethod
    def retrieve(self, service: str, account: str) -> Optional[str]:
        pass

    @abstractmethod
    def delete(self, service: str, account: str) -> bool:
        pass


class WindowsKeychain(KeychainBackend):
    """Windows Credential Manager via pywin32"""

    def store(self, service: str, account: str, secret: str) -> bool:
        import win32cred
        credential = {
            'Type': win32cred.CRED_TYPE_GENERIC,
            'TargetName': f'{service}/{account}',
            'CredentialBlob': secret.encode('utf-16-le'),
            'Persist': win32cred.CRED_PERSIST_LOCAL_MACHINE,
            'UserName': account,
        }
        win32cred.CredWrite(credential, 0)
        return True

    def retrieve(self, service: str, account: str) -> Optional[str]:
        import win32cred
        try:
            cred = win32cred.CredRead(
                f'{service}/{account}',
                win32cred.CRED_TYPE_GENERIC
            )
            return cred['CredentialBlob'].decode('utf-16-le')
        except Exception:
            return None

    def delete(self, service: str, account: str) -> bool:
        import win32cred
        try:
            win32cred.CredDelete(
                f'{service}/{account}',
                win32cred.CRED_TYPE_GENERIC
            )
            return True
        except Exception:
            return False


class MacOSKeychain(KeychainBackend):
    """macOS Keychain via keyring library"""

    def store(self, service: str, account: str, secret: str) -> bool:
        import keyring
        keyring.set_password(service, account, secret)
        return True

    def retrieve(self, service: str, account: str) -> Optional[str]:
        import keyring
        return keyring.get_password(service, account)

    def delete(self, service: str, account: str) -> bool:
        import keyring
        try:
            keyring.delete_password(service, account)
            return True
        except keyring.errors.PasswordDeleteError:
            return False


class LinuxKeychain(KeychainBackend):
    """Linux Secret Service via keyring library"""

    def store(self, service: str, account: str, secret: str) -> bool:
        import keyring
        keyring.set_password(service, account, secret)
        return True

    def retrieve(self, service: str, account: str) -> Optional[str]:
        import keyring
        return keyring.get_password(service, account)

    def delete(self, service: str, account: str) -> bool:
        import keyring
        try:
            keyring.delete_password(service, account)
            return True
        except keyring.errors.PasswordDeleteError:
            return False


def get_keychain() -> KeychainBackend:
    system = platform.system()
    if system == 'Windows':
        return WindowsKeychain()
    elif system == 'Darwin':
        return MacOSKeychain()
    else:
        return LinuxKeychain()
```

### 4.3 API Key UI Component

```typescript
// src/features/settings/APIKeyManager.tsx
interface APIKey {
  id: string;
  exchange: string;
  name: string;
  permissions: string[];
  createdAt: string;
  lastUsed: string;
  status: 'active' | 'expired' | 'revoked';
}

export function APIKeyManager() {
  const [keys, setKeys] = useState<APIKey[]>([]);
  const [showAddModal, setShowAddModal] = useState(false);
  const [testingKey, setTestingKey] = useState<string | null>(null);

  const addKey = async (data: AddKeyFormData) => {
    // Validate key format
    if (!validateKeyFormat(data.exchange, data.apiKey)) {
      throw new Error('Invalid API key format');
    }

    // Test connection
    const testResult = await testExchangeConnection(data);
    if (!testResult.success) {
      throw new Error(`Connection failed: ${testResult.error}`);
    }

    // Check permissions (must NOT have withdrawal)
    if (testResult.permissions.includes('withdraw')) {
      throw new Error(
        'API key has withdrawal permissions. ' +
        'Please create a new key without withdrawal access.'
      );
    }

    // Store in keychain
    await keychainApi.store({
      exchange: data.exchange,
      apiKey: data.apiKey,
      apiSecret: data.apiSecret,
      name: data.name,
    });

    // Refresh key list
    await refreshKeys();
  };

  const testKey = async (keyId: string) => {
    setTestingKey(keyId);
    try {
      const result = await exchangeApi.testConnection(keyId);
      // Show result
    } finally {
      setTestingKey(null);
    }
  };

  return (
    <div className="api-key-manager">
      <div className="header">
        <h2>API Keys</h2>
        <Button onClick={() => setShowAddModal(true)}>
          Add API Key
        </Button>
      </div>

      <Table
        columns={[
          { title: 'Exchange', dataIndex: 'exchange' },
          { title: 'Name', dataIndex: 'name' },
          { title: 'Permissions', dataIndex: 'permissions',
            render: (perms) => perms.join(', ') },
          { title: 'Status', dataIndex: 'status' },
          { title: 'Actions', render: (_, key) => (
            <>
              <Button onClick={() => testKey(key.id)}
                loading={testingKey === key.id}>
                Test
              </Button>
              <Button danger onClick={() => deleteKey(key.id)}>
                Delete
              </Button>
            </>
          )},
        ]}
        dataSource={keys}
      />

      <AddKeyModal
        open={showAddModal}
        onClose={() => setShowAddModal(false)}
        onSubmit={addKey}
      />
    </div>
  );
}
```

---

## 5. Settings Storage

### 5.1 Configuration Schema

```typescript
// src/types/config.ts
import { z } from 'zod';

export const generalConfigSchema = z.object({
  language: z.enum(['en', 'zh', 'ja', 'ko']).default('en'),
  theme: z.enum(['light', 'dark', 'system']).default('system'),
  timezone: z.string().default('UTC'),
  dateFormat: z.enum(['ISO', 'US', 'EU']).default('ISO'),
  notifications: z.object({
    orderFills: z.boolean().default(true),
    priceAlerts: z.boolean().default(true),
    systemAlerts: z.boolean().default(true),
    sound: z.boolean().default(true),
  }),
});

export const tradingConfigSchema = z.object({
  defaultExchange: z.string().optional(),
  defaultSymbol: z.string().default('BTCUSDT'),
  orderConfirmation: z.boolean().default(true),
  doubleClickToTrade: z.boolean().default(false),
  defaultOrderType: z.enum(['limit', 'market']).default('limit'),
  defaultTimeInForce: z.enum(['GTC', 'IOC', 'FOK']).default('GTC'),
  slippageTolerance: z.number().min(0).max(0.1).default(0.005),
});

export const riskConfigSchema = z.object({
  maxPositionSize: z.number().min(0).max(1).default(0.1),
  maxPositionValue: z.number().min(0).default(10000),
  dailyLossLimit: z.number().min(0).max(1).default(0.05),
  maxOpenOrders: z.number().min(1).max(100).default(10),
  circuitBreaker: z.object({
    enabled: z.boolean().default(true),
    threshold: z.number().min(0).max(1).default(0.1),
    cooldownMinutes: z.number().min(1).max(1440).default(60),
  }),
  requireConfirmation: z.object({
    largeOrders: z.boolean().default(true),
    largeOrderThreshold: z.number().default(1000),
    marketOrders: z.boolean().default(true),
  }),
});

export const advancedConfigSchema = z.object({
  apiRateLimit: z.number().min(1).max(100).default(10),
  websocketReconnectDelay: z.number().min(1000).max(60000).default(5000),
  orderBookDepth: z.number().min(5).max(100).default(20),
  logLevel: z.enum(['debug', 'info', 'warn', 'error']).default('info'),
  telemetry: z.boolean().default(true),
});

export const configSchema = z.object({
  general: generalConfigSchema,
  trading: tradingConfigSchema,
  risk: riskConfigSchema,
  advanced: advancedConfigSchema,
});

export type Config = z.infer<typeof configSchema>;
```

### 5.2 Configuration Persistence

```python
# apps/gateway/config_manager.py
import json
import os
from pathlib import Path
from typing import Any, Dict, Optional
from dataclasses import dataclass, asdict
from cryptography.fernet import Fernet
import hashlib

@dataclass
class ConfigPaths:
    base_dir: Path
    config_file: Path
    backup_dir: Path
    encryption_key_file: Path

    @classmethod
    def get_default(cls) -> 'ConfigPaths':
        if os.name == 'nt':  # Windows
            base = Path(os.environ.get('APPDATA', '')) / 'VeloZ'
        elif os.name == 'darwin':  # macOS
            base = Path.home() / 'Library' / 'Application Support' / 'VeloZ'
        else:  # Linux
            base = Path.home() / '.config' / 'veloz'

        return cls(
            base_dir=base,
            config_file=base / 'config.json',
            backup_dir=base / 'backups',
            encryption_key_file=base / '.key',
        )


class ConfigManager:
    def __init__(self, paths: Optional[ConfigPaths] = None):
        self.paths = paths or ConfigPaths.get_default()
        self._ensure_directories()
        self._encryption_key = self._get_or_create_key()

    def _ensure_directories(self):
        self.paths.base_dir.mkdir(parents=True, exist_ok=True)
        self.paths.backup_dir.mkdir(parents=True, exist_ok=True)

    def _get_or_create_key(self) -> bytes:
        if self.paths.encryption_key_file.exists():
            return self.paths.encryption_key_file.read_bytes()
        key = Fernet.generate_key()
        self.paths.encryption_key_file.write_bytes(key)
        os.chmod(self.paths.encryption_key_file, 0o600)
        return key

    def load(self) -> Dict[str, Any]:
        if not self.paths.config_file.exists():
            return self._get_defaults()

        try:
            content = self.paths.config_file.read_text()
            return json.loads(content)
        except (json.JSONDecodeError, IOError) as e:
            # Try to restore from backup
            return self._restore_from_backup() or self._get_defaults()

    def save(self, config: Dict[str, Any]) -> bool:
        # Validate config
        if not self._validate(config):
            raise ValueError("Invalid configuration")

        # Create backup
        self._create_backup()

        # Save config
        try:
            content = json.dumps(config, indent=2)
            self.paths.config_file.write_text(content)
            return True
        except IOError as e:
            raise RuntimeError(f"Failed to save config: {e}")

    def _create_backup(self):
        if self.paths.config_file.exists():
            import time
            timestamp = int(time.time())
            backup_file = self.paths.backup_dir / f'config_{timestamp}.json'
            backup_file.write_text(self.paths.config_file.read_text())

            # Keep only last 10 backups
            backups = sorted(self.paths.backup_dir.glob('config_*.json'))
            for old_backup in backups[:-10]:
                old_backup.unlink()

    def _restore_from_backup(self) -> Optional[Dict[str, Any]]:
        backups = sorted(self.paths.backup_dir.glob('config_*.json'))
        if backups:
            try:
                return json.loads(backups[-1].read_text())
            except (json.JSONDecodeError, IOError):
                pass
        return None

    def _validate(self, config: Dict[str, Any]) -> bool:
        # Basic validation - full validation done in UI
        required_sections = ['general', 'trading', 'risk', 'advanced']
        return all(section in config for section in required_sections)

    def _get_defaults(self) -> Dict[str, Any]:
        return {
            'general': {
                'language': 'en',
                'theme': 'system',
                'timezone': 'UTC',
                'dateFormat': 'ISO',
                'notifications': {
                    'orderFills': True,
                    'priceAlerts': True,
                    'systemAlerts': True,
                    'sound': True,
                },
            },
            'trading': {
                'defaultSymbol': 'BTCUSDT',
                'orderConfirmation': True,
                'doubleClickToTrade': False,
                'defaultOrderType': 'limit',
                'defaultTimeInForce': 'GTC',
                'slippageTolerance': 0.005,
            },
            'risk': {
                'maxPositionSize': 0.1,
                'maxPositionValue': 10000,
                'dailyLossLimit': 0.05,
                'maxOpenOrders': 10,
                'circuitBreaker': {
                    'enabled': True,
                    'threshold': 0.1,
                    'cooldownMinutes': 60,
                },
                'requireConfirmation': {
                    'largeOrders': True,
                    'largeOrderThreshold': 1000,
                    'marketOrders': True,
                },
            },
            'advanced': {
                'apiRateLimit': 10,
                'websocketReconnectDelay': 5000,
                'orderBookDepth': 20,
                'logLevel': 'info',
                'telemetry': True,
            },
        }

    def export_config(self, include_sensitive: bool = False) -> str:
        """Export config for backup/sharing"""
        config = self.load()
        if not include_sensitive:
            # Remove sensitive fields
            config.pop('apiKeys', None)
        return json.dumps(config, indent=2)

    def import_config(self, config_json: str) -> bool:
        """Import config from backup"""
        try:
            config = json.loads(config_json)
            if self._validate(config):
                self.save(config)
                return True
        except json.JSONDecodeError:
            pass
        return False
```

---

## 6. Exchange Connection Testing

### 6.1 Connection Test Flow

```
                    Connection Test Flow
                    ====================

User enters API credentials
           |
           v
+------------------+
| Format Validation|
| - Key length     |
| - Character set  |
+------------------+
           |
           v
+------------------+
| Permission Check |
| - List perms     |
| - Block withdraw |
+------------------+
           |
           v
+------------------+
| Balance Query    |
| - Test read      |
| - Verify access  |
+------------------+
           |
           v
+------------------+
| Latency Test     |
| - Measure RTT    |
| - Check stability|
+------------------+
           |
           v
+------------------+
| Result Display   |
| - Success/Fail   |
| - Recommendations|
+------------------+
```

### 6.2 Connection Test API

```python
# apps/gateway/exchange_tester.py
from dataclasses import dataclass
from typing import List, Optional
import time
import asyncio

@dataclass
class ConnectionTestResult:
    success: bool
    exchange: str
    permissions: List[str]
    balance_available: bool
    latency_ms: float
    error: Optional[str] = None
    warnings: List[str] = None

    def __post_init__(self):
        self.warnings = self.warnings or []


class ExchangeTester:
    def __init__(self):
        self.adapters = {
            'binance': BinanceAdapter(),
            'binance_futures': BinanceFuturesAdapter(),
            'okx': OKXAdapter(),
        }

    async def test_connection(
        self,
        exchange: str,
        api_key: str,
        api_secret: str,
        testnet: bool = False
    ) -> ConnectionTestResult:
        adapter = self.adapters.get(exchange)
        if not adapter:
            return ConnectionTestResult(
                success=False,
                exchange=exchange,
                permissions=[],
                balance_available=False,
                latency_ms=0,
                error=f"Unsupported exchange: {exchange}"
            )

        warnings = []

        # Test 1: Get permissions
        try:
            start = time.time()
            permissions = await adapter.get_api_permissions(
                api_key, api_secret, testnet
            )
            latency = (time.time() - start) * 1000

            # Check for dangerous permissions
            if 'withdraw' in permissions or 'withdrawal' in permissions:
                return ConnectionTestResult(
                    success=False,
                    exchange=exchange,
                    permissions=permissions,
                    balance_available=False,
                    latency_ms=latency,
                    error="API key has withdrawal permissions. "
                          "Please create a new key without withdrawal access."
                )

            # Warn about unnecessary permissions
            unnecessary = set(permissions) - {'read', 'spot', 'futures', 'trade'}
            if unnecessary:
                warnings.append(
                    f"API key has unnecessary permissions: {unnecessary}. "
                    "Consider creating a key with minimal permissions."
                )

        except Exception as e:
            return ConnectionTestResult(
                success=False,
                exchange=exchange,
                permissions=[],
                balance_available=False,
                latency_ms=0,
                error=f"Failed to verify API key: {str(e)}"
            )

        # Test 2: Check balance access
        try:
            balance = await adapter.get_balance(api_key, api_secret, testnet)
            balance_available = balance is not None
        except Exception:
            balance_available = False
            warnings.append("Could not verify balance access")

        # Test 3: Latency check
        latencies = []
        for _ in range(3):
            start = time.time()
            await adapter.ping(api_key, api_secret, testnet)
            latencies.append((time.time() - start) * 1000)
            await asyncio.sleep(0.1)

        avg_latency = sum(latencies) / len(latencies)

        if avg_latency > 500:
            warnings.append(
                f"High latency detected ({avg_latency:.0f}ms). "
                "Consider using a server closer to the exchange."
            )

        return ConnectionTestResult(
            success=True,
            exchange=exchange,
            permissions=permissions,
            balance_available=balance_available,
            latency_ms=avg_latency,
            warnings=warnings
        )
```

---

## 7. Settings UI Components

### 7.1 Settings Page Structure

```typescript
// src/pages/SettingsPage.tsx
export function SettingsPage() {
  const [activeTab, setActiveTab] = useState('general');

  return (
    <div className="settings-page">
      <PageHeader title="Settings" />

      <Tabs activeKey={activeTab} onChange={setActiveTab}>
        <TabPane tab="General" key="general">
          <GeneralSettings />
        </TabPane>
        <TabPane tab="Trading" key="trading">
          <TradingSettings />
        </TabPane>
        <TabPane tab="Risk Management" key="risk">
          <RiskSettings />
        </TabPane>
        <TabPane tab="API Keys" key="apikeys">
          <APIKeyManager />
        </TabPane>
        <TabPane tab="Advanced" key="advanced">
          <AdvancedSettings />
        </TabPane>
      </Tabs>
    </div>
  );
}
```

### 7.2 Risk Settings Component

```typescript
// src/features/settings/RiskSettings.tsx
export function RiskSettings() {
  const { config, updateConfig, isDirty, save } = useConfigStore();
  const form = useForm<RiskConfig>({
    resolver: zodResolver(riskConfigSchema),
    defaultValues: config.risk,
  });

  const onSubmit = async (data: RiskConfig) => {
    await updateConfig({ risk: data });
    await save();
  };

  return (
    <Form form={form} onFinish={onSubmit} layout="vertical">
      <Card title="Position Limits">
        <Form.Item
          name="maxPositionSize"
          label="Maximum Position Size (% of portfolio)"
          tooltip="Maximum size of any single position as percentage of total portfolio"
        >
          <Slider
            min={0.01}
            max={1}
            step={0.01}
            marks={{ 0.1: '10%', 0.25: '25%', 0.5: '50%', 1: '100%' }}
          />
        </Form.Item>

        <Form.Item
          name="maxPositionValue"
          label="Maximum Position Value (USD)"
          tooltip="Maximum value of any single position in USD"
        >
          <InputNumber
            min={0}
            max={1000000}
            formatter={(v) => `$ ${v}`.replace(/\B(?=(\d{3})+(?!\d))/g, ',')}
          />
        </Form.Item>

        <Form.Item
          name="maxOpenOrders"
          label="Maximum Open Orders"
          tooltip="Maximum number of open orders at any time"
        >
          <InputNumber min={1} max={100} />
        </Form.Item>
      </Card>

      <Card title="Loss Limits">
        <Form.Item
          name="dailyLossLimit"
          label="Daily Loss Limit (% of portfolio)"
          tooltip="Trading will be paused if daily loss exceeds this percentage"
        >
          <Slider
            min={0.01}
            max={0.5}
            step={0.01}
            marks={{ 0.05: '5%', 0.1: '10%', 0.25: '25%', 0.5: '50%' }}
          />
        </Form.Item>

        <Alert
          type="info"
          message="When daily loss limit is reached, all trading will be paused until the next day or manual reset."
        />
      </Card>

      <Card title="Circuit Breaker">
        <Form.Item
          name={['circuitBreaker', 'enabled']}
          valuePropName="checked"
        >
          <Switch /> Enable Circuit Breaker
        </Form.Item>

        <Form.Item
          name={['circuitBreaker', 'threshold']}
          label="Trigger Threshold (% loss)"
          tooltip="Circuit breaker triggers when loss exceeds this percentage"
        >
          <Slider
            min={0.01}
            max={0.5}
            step={0.01}
            disabled={!form.watch('circuitBreaker.enabled')}
          />
        </Form.Item>

        <Form.Item
          name={['circuitBreaker', 'cooldownMinutes']}
          label="Cooldown Period (minutes)"
          tooltip="Time before trading can resume after circuit breaker triggers"
        >
          <InputNumber
            min={1}
            max={1440}
            disabled={!form.watch('circuitBreaker.enabled')}
          />
        </Form.Item>
      </Card>

      <Card title="Order Confirmation">
        <Form.Item
          name={['requireConfirmation', 'largeOrders']}
          valuePropName="checked"
        >
          <Switch /> Require confirmation for large orders
        </Form.Item>

        <Form.Item
          name={['requireConfirmation', 'largeOrderThreshold']}
          label="Large Order Threshold (USD)"
        >
          <InputNumber
            min={0}
            disabled={!form.watch('requireConfirmation.largeOrders')}
          />
        </Form.Item>

        <Form.Item
          name={['requireConfirmation', 'marketOrders']}
          valuePropName="checked"
        >
          <Switch /> Require confirmation for market orders
        </Form.Item>
      </Card>

      <div className="form-actions">
        <Button type="primary" htmlType="submit" disabled={!isDirty}>
          Save Changes
        </Button>
        <Button onClick={() => form.reset()}>Reset</Button>
      </div>
    </Form>
  );
}
```

---

## 8. API Contracts

### 8.1 Configuration API

```
GET /api/config
Response: { config: Config }

PUT /api/config
Body: { config: Partial<Config> }
Response: { success: boolean, config: Config }

POST /api/config/validate
Body: { config: Partial<Config> }
Response: { valid: boolean, errors: ValidationError[] }

POST /api/config/export
Response: { configJson: string }

POST /api/config/import
Body: { configJson: string }
Response: { success: boolean, config: Config }
```

### 8.2 Keychain API

```
GET /api/keys
Response: { keys: APIKeyInfo[] }

POST /api/keys
Body: { exchange: string, apiKey: string, apiSecret: string, name: string }
Response: { success: boolean, keyId: string }

DELETE /api/keys/:keyId
Response: { success: boolean }

POST /api/keys/:keyId/test
Response: { result: ConnectionTestResult }
```

### 8.3 Exchange API

```
POST /api/exchange/test
Body: { exchange: string, apiKey: string, apiSecret: string, testnet: boolean }
Response: { result: ConnectionTestResult }

GET /api/exchange/:exchange/symbols
Response: { symbols: Symbol[] }

GET /api/exchange/:exchange/balance
Response: { balances: Balance[] }
```

---

## 9. Security Considerations

### 9.1 API Key Security

| Threat | Mitigation |
|--------|------------|
| Key exposure in logs | Never log API keys; mask in UI |
| Key exposure in memory | Clear from memory after use |
| Key theft via XSS | Store in keychain, not localStorage |
| Unauthorized access | Require password for key operations |

### 9.2 Configuration Security

| Threat | Mitigation |
|--------|------------|
| Config tampering | Validate all config changes |
| Sensitive data exposure | Encrypt sensitive fields |
| Backup theft | Encrypt backup files |
| Import attacks | Validate imported config strictly |

---

## 10. Implementation Plan

### 10.1 Phase Breakdown

| Phase | Duration | Deliverables |
|-------|----------|--------------|
| **Phase 1: Backend** | 1 week | Config manager, keychain integration |
| **Phase 2: Wizard** | 1 week | Setup wizard UI, validation |
| **Phase 3: Settings** | 1 week | Settings pages, forms |
| **Phase 4: Testing** | 0.5 week | Integration tests, E2E tests |
| **Phase 5: Polish** | 0.5 week | UX improvements, documentation |

### 10.2 Dependencies

```
                    Implementation Dependencies
                    ===========================

+------------------+
| Keychain Backend |
+------------------+
        |
        v
+------------------+     +------------------+
| Config Manager   |---->| Setup Wizard     |
+------------------+     +------------------+
        |                        |
        v                        v
+------------------+     +------------------+
| Settings API     |     | Wizard API       |
+------------------+     +------------------+
        |                        |
        +------------------------+
                    |
                    v
            +------------------+
            | Settings UI      |
            +------------------+
```

---

## 11. Testing Strategy

### 11.1 Unit Tests

```typescript
// tests/config.test.ts
describe('ConfigManager', () => {
  test('loads default config when no file exists', async () => {
    const config = await configApi.load();
    expect(config.general.language).toBe('en');
  });

  test('validates config before saving', async () => {
    const invalidConfig = { general: { language: 'invalid' } };
    await expect(configApi.save(invalidConfig)).rejects.toThrow();
  });

  test('creates backup before saving', async () => {
    await configApi.save(validConfig);
    const backups = await configApi.listBackups();
    expect(backups.length).toBeGreaterThan(0);
  });
});

describe('KeychainManager', () => {
  test('stores and retrieves API key', async () => {
    await keychainApi.store({
      exchange: 'binance',
      apiKey: 'test-key',
      apiSecret: 'test-secret',
    });
    const key = await keychainApi.retrieve('binance');
    expect(key.apiKey).toBe('test-key');
  });

  test('rejects keys with withdrawal permission', async () => {
    const result = await exchangeApi.testConnection({
      exchange: 'binance',
      apiKey: 'key-with-withdraw',
      apiSecret: 'secret',
    });
    expect(result.success).toBe(false);
    expect(result.error).toContain('withdrawal');
  });
});
```

### 11.2 E2E Tests

```typescript
// tests/e2e/setup-wizard.spec.ts
test('completes setup wizard', async ({ page }) => {
  await page.goto('/setup');

  // Step 1: Welcome
  await page.getByRole('checkbox', { name: /accept terms/i }).check();
  await page.getByRole('button', { name: /next/i }).click();

  // Step 2: Exchange
  await page.getByRole('combobox', { name: /exchange/i }).selectOption('binance');
  await page.getByLabel(/api key/i).fill('test-api-key');
  await page.getByLabel(/api secret/i).fill('test-api-secret');
  await page.getByRole('button', { name: /test connection/i }).click();
  await expect(page.getByText(/connection successful/i)).toBeVisible();
  await page.getByRole('button', { name: /next/i }).click();

  // Step 3: Risk
  await page.getByRole('slider', { name: /max position/i }).fill('0.1');
  await page.getByRole('button', { name: /next/i }).click();

  // Step 4: Mode
  await page.getByRole('radio', { name: /simulated/i }).check();
  await page.getByRole('button', { name: /next/i }).click();

  // Step 5: Complete
  await expect(page.getByText(/setup complete/i)).toBeVisible();
  await page.getByRole('button', { name: /start trading/i }).click();

  // Verify redirect to dashboard
  await expect(page).toHaveURL('/dashboard');
});
```

---

## 12. Success Metrics

| Metric | Target | Measurement |
|--------|--------|-------------|
| Wizard completion rate | > 90% | Analytics |
| Config save success rate | > 99% | Telemetry |
| Key storage success rate | > 99% | Telemetry |
| Connection test accuracy | > 95% | User feedback |
| Settings page load time | < 500ms | Performance monitoring |

---

## 13. Related Documentation

- [One-click Installer System](01-installer-system.md)
- [Security Education System](05-security-education.md)
- [Security Best Practices](../../guides/user/security-best-practices.md)
- [Configuration Guide](../../guides/user/configuration.md)
