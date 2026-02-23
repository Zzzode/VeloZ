# Settings Feature

UI-based configuration management for VeloZ trading framework.

## Features

- **General Settings**: Gateway host, port, build preset
- **Market Data**: Configure market data source and trading symbol
- **Execution Mode**: Select execution mode (simulation, testnet, production)
- **Exchange Configuration**: Configure API credentials for:
  - Binance (testnet and production)
  - OKX (with passphrase support)
  - Bybit
  - Coinbase

## Components

### ConfigForm

Main configuration form with tabbed interface for different settings categories.

**Features:**
- Tab-based navigation (General, Binance, OKX, Bybit, Coinbase)
- Password field masking with toggle
- Form validation
- Success/error notifications
- Restart reminder

### useConfig Hook

React Query hook for fetching and updating configuration.

**API:**
```typescript
const {
  config,           // Current configuration
  isLoading,        // Loading state
  error,            // Fetch error
  refetch,          // Refetch config
  updateConfig,     // Update config function
  isUpdating,       // Update in progress
  updateError,      // Update error
  updateSuccess,    // Update success flag
} = useConfig();
```

## Usage

### In Settings Page

```typescript
import ConfigForm from './components/ConfigForm';

export default function Settings() {
  return (
    <div className="space-y-6">
      <div>
        <h1 className="text-2xl font-bold text-text">Settings</h1>
        <p className="text-text-muted mt-1">
          Configure your trading environment
        </p>
      </div>
      <ConfigForm />
    </div>
  );
}
```

### Programmatic Config Update

```typescript
import { useConfig } from '@/features/settings/hooks';

function MyComponent() {
  const { updateConfig } = useConfig();

  const handleSave = () => {
    updateConfig({
      marketSource: 'binance_rest',
      marketSymbol: 'ETHUSDT',
      executionMode: 'binance_testnet_spot',
    });
  };

  return <button onClick={handleSave}>Save</button>;
}
```

## API Endpoints

### GET /api/config

Fetch current configuration.

**Response:**
```json
{
  "host": "0.0.0.0",
  "port": 8080,
  "market_source": "binance_rest",
  "market_symbol": "BTCUSDT",
  "execution_mode": "binance_testnet_spot",
  "binance_base_url": "https://api.binance.com",
  "binance_trade_enabled": true,
  "auth_enabled": false
}
```

### POST /api/config

Update configuration.

**Request:**
```json
{
  "marketSource": "binance_rest",
  "marketSymbol": "ETHUSDT",
  "executionMode": "binance_testnet_spot",
  "binanceApiKey": "your-api-key",
  "binanceApiSecret": "your-api-secret"
}
```

**Response:**
```json
{
  "ok": true,
  "message": "Configuration saved. Restart the gateway for changes to take effect.",
  "config_file": "/path/to/.veloz_config.json"
}
```

## Configuration File

Configuration is saved to `.veloz_config.json` in the gateway directory.

**Important:** The gateway must be restarted for configuration changes to take effect:

```bash
# Stop the gateway (Ctrl+C)
# Restart with new configuration
./scripts/run_gateway.sh dev
```

## Environment Variables

The following environment variables can be configured via the UI:

### Gateway
- `VELOZ_HOST` - Server host address
- `VELOZ_PORT` - Server port
- `VELOZ_PRESET` - Build preset (dev/release/asan)

### Market Data
- `VELOZ_MARKET_SOURCE` - Market data source
- `VELOZ_MARKET_SYMBOL` - Trading symbol

### Execution
- `VELOZ_EXECUTION_MODE` - Execution mode

### Binance
- `VELOZ_BINANCE_BASE_URL` - REST API URL
- `VELOZ_BINANCE_WS_BASE_URL` - WebSocket URL
- `VELOZ_BINANCE_TRADE_BASE_URL` - Trade API URL
- `VELOZ_BINANCE_API_KEY` - API key
- `VELOZ_BINANCE_API_SECRET` - API secret

### OKX
- `VELOZ_OKX_API_KEY` - API key
- `VELOZ_OKX_API_SECRET` - API secret
- `VELOZ_OKX_PASSPHRASE` - API passphrase (required)
- `VELOZ_OKX_BASE_URL` - REST API URL
- `VELOZ_OKX_WS_URL` - WebSocket URL
- `VELOZ_OKX_TRADE_URL` - Trade API URL

### Bybit
- `VELOZ_BYBIT_API_KEY` - API key
- `VELOZ_BYBIT_API_SECRET` - API secret
- `VELOZ_BYBIT_BASE_URL` - REST API URL
- `VELOZ_BYBIT_WS_URL` - WebSocket URL
- `VELOZ_BYBIT_TRADE_URL` - Trade API URL

### Coinbase
- `VELOZ_COINBASE_API_KEY` - API key
- `VELOZ_COINBASE_API_SECRET` - API secret
- `VELOZ_COINBASE_BASE_URL` - REST API URL
- `VELOZ_COINBASE_WS_URL` - WebSocket URL

## Security Notes

- **API secrets are never displayed** after being saved
- Secrets are masked with password fields
- Use the "Show API secrets" toggle to view while entering
- Configuration file should be added to `.gitignore`
- **Never commit API keys** to version control

## Testing

### Manual Testing

1. Start the gateway:
   ```bash
   ./scripts/run_gateway.sh dev
   ```

2. Open the UI: http://127.0.0.1:8080

3. Navigate to Settings page

4. Update configuration and save

5. Restart gateway to apply changes

### API Testing

```bash
# Fetch current config
curl http://127.0.0.1:8080/api/config

# Update config
curl -X POST http://127.0.0.1:8080/api/config \
  -H "Content-Type: application/json" \
  -d '{
    "marketSource": "binance_rest",
    "marketSymbol": "ETHUSDT"
  }'
```

## Future Enhancements

- [ ] Hot reload configuration without restart
- [ ] Configuration validation before save
- [ ] Configuration backup/restore
- [ ] Configuration templates (testnet, production)
- [ ] Configuration diff viewer
- [ ] Environment variable export script
