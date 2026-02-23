# UI Configuration Guide

This guide explains how to configure VeloZ using the web-based UI configuration interface.

## Overview

VeloZ v1.0 includes a comprehensive web UI for managing all environment variables and exchange configurations. This eliminates the need to manually edit environment variables or configuration files.

## Accessing the Configuration UI

1. **Start the Gateway**:
   ```bash
   ./scripts/run_gateway.sh dev
   ```

2. **Open the UI**:
   Navigate to `http://127.0.0.1:8080` in your web browser

3. **Go to Settings**:
   Click on "Settings" in the navigation menu

## Configuration Sections

The configuration UI is organized into five tabs:

### 1. General Settings

Configure basic gateway and market data settings:

- **Host**: Gateway server host address (default: `0.0.0.0`)
- **Port**: Gateway server port (default: `8080`)
- **Build Preset**: Development, Release, or ASan build
- **Market Data Source**: Choose from:
  - Simulation (no real exchange connection)
  - Binance REST
  - OKX REST
  - Bybit REST
  - Coinbase REST
- **Trading Symbol**: Symbol to track (e.g., `BTCUSDT`, `ETHUSDT`)
- **Execution Mode**: Choose from:
  - Simulation (no real orders)
  - Binance Testnet
  - Binance Production ⚠️
  - OKX Testnet
  - OKX Production ⚠️
  - Bybit Testnet
  - Bybit Production ⚠️
  - Coinbase Sandbox
  - Coinbase Production ⚠️

### 2. Binance Configuration

Configure Binance exchange connection:

- **Base URL**: Market data API URL
  - Testnet: `https://testnet.binance.vision`
  - Production: `https://api.binance.com`
- **Trade Base URL**: Trading API URL
  - Testnet: `https://testnet.binance.vision`
  - Production: `https://api.binance.com`
- **WebSocket Base URL**: User data stream URL
  - Testnet: `wss://testnet.binance.vision/ws`
  - Production: `wss://stream.binance.com:9443/ws`
- **API Key**: Your Binance API key
- **API Secret**: Your Binance API secret

**Getting Binance API Keys:**
- Testnet: https://testnet.binance.vision (GitHub login)
- Production: https://www.binance.com → API Management

### 3. OKX Configuration

Configure OKX exchange connection:

- **Base URL**: REST API URL
  - Testnet: `https://www.okx.com`
  - Production: `https://www.okx.com`
- **API Key**: Your OKX API key
- **API Secret**: Your OKX API secret
- **Passphrase**: Your OKX API passphrase (required)

**Note:** OKX requires a passphrase in addition to API key and secret.

### 4. Bybit Configuration

Configure Bybit exchange connection:

- **Base URL**: REST API URL
  - Testnet: `https://api-testnet.bybit.com`
  - Production: `https://api.bybit.com`
- **API Key**: Your Bybit API key
- **API Secret**: Your Bybit API secret

### 5. Coinbase Configuration

Configure Coinbase exchange connection:

- **Base URL**: REST API URL
  - Sandbox: `https://api-public.sandbox.pro.coinbase.com`
  - Production: `https://api.coinbase.com`
- **API Key**: Your Coinbase API key
- **API Secret**: Your Coinbase API secret

## Saving Configuration

1. Fill in the desired configuration fields
2. Click **"Save Configuration"** button at the bottom
3. Wait for success confirmation
4. **Restart the gateway** for changes to take effect:
   ```bash
   # Stop the gateway (Ctrl+C in the terminal)
   # Start again
   ./scripts/run_gateway.sh dev
   ```

## Security Features

### API Secret Protection

- **Secrets are never displayed** after being saved
- API keys and secrets use password fields (masked by default)
- Use the **"Show API secrets"** checkbox to view while entering
- Secrets are stored in `.veloz_config.json` (excluded from git)

### Best Practices

1. **Never commit API keys** to version control
2. **Use testnet first** before production trading
3. **Set IP restrictions** on exchange API keys when possible
4. **Use read-only keys** for market data only
5. **Enable 2FA** on your exchange accounts
6. **Rotate API keys regularly**

## Configuration File

Configuration is saved to `.veloz_config.json` in the `apps/gateway/` directory.

**Example:**
```json
{
  "marketSource": "binance_rest",
  "marketSymbol": "BTCUSDT",
  "executionMode": "binance_testnet_spot",
  "binanceBaseUrl": "https://testnet.binance.vision",
  "binanceTradeBaseUrl": "https://testnet.binance.vision",
  "binanceWsBaseUrl": "wss://testnet.binance.vision/ws",
  "binanceApiKey": "your-api-key",
  "binanceApiSecret": "your-api-secret"
}
```

**Important:** Add `.veloz_config.json` to your `.gitignore` to prevent committing secrets.

## Common Scenarios

### Scenario 1: Connect to Binance Testnet

1. Go to Settings → General tab
2. Set **Market Data Source** to "Binance REST"
3. Set **Trading Symbol** to "BTCUSDT"
4. Set **Execution Mode** to "Binance Testnet"
5. Go to Binance tab
6. Enter your testnet API key and secret
7. Set URLs to testnet values:
   - Base URL: `https://testnet.binance.vision`
   - Trade Base URL: `https://testnet.binance.vision`
   - WebSocket URL: `wss://testnet.binance.vision/ws`
8. Click "Save Configuration"
9. Restart gateway

### Scenario 2: Switch to Production (⚠️ Real Money)

1. Go to Settings → General tab
2. Set **Execution Mode** to "Binance Production"
3. Go to Binance tab
4. Enter your **production** API key and secret
5. Set URLs to production values:
   - Base URL: `https://api.binance.com`
   - Trade Base URL: `https://api.binance.com`
   - WebSocket URL: `wss://stream.binance.com:9443/ws`
6. Click "Save Configuration"
7. Restart gateway
8. **Verify connection** before placing orders

### Scenario 3: Multi-Exchange Setup

1. Configure each exchange in its respective tab
2. Set appropriate API keys for each exchange
3. Choose execution mode that matches your primary exchange
4. Save configuration
5. Restart gateway

## Troubleshooting

### Configuration Not Taking Effect

**Problem:** Changes don't apply after saving

**Solution:** Restart the gateway:
```bash
# Stop with Ctrl+C
./scripts/run_gateway.sh dev
```

### API Connection Errors

**Problem:** "binance_not_configured" or connection errors

**Solutions:**
1. Verify API key and secret are correct
2. Check that URLs match your execution mode (testnet vs production)
3. Ensure API key has required permissions (Spot Trading)
4. Check if IP whitelist is configured on exchange
5. Verify API key is not expired

### Configuration File Errors

**Problem:** Cannot save configuration

**Solutions:**
1. Check file permissions in `apps/gateway/` directory
2. Ensure `.veloz_config.json` is writable
3. Check gateway logs for error details

### UI Not Loading Configuration

**Problem:** Settings page shows loading or errors

**Solutions:**
1. Verify gateway is running: `curl http://127.0.0.1:8080/api/health`
2. Check browser console for errors
3. Try refreshing the page
4. Check gateway logs for API errors

## API Reference

### GET /api/config

Fetch current configuration.

**Example:**
```bash
curl http://127.0.0.1:8080/api/config
```

**Response:**
```json
{
  "host": "0.0.0.0",
  "port": 8080,
  "market_source": "binance_rest",
  "market_symbol": "BTCUSDT",
  "execution_mode": "binance_testnet_spot",
  "binance_base_url": "https://testnet.binance.vision",
  "binance_trade_enabled": true,
  "binance_user_stream_connected": true,
  "auth_enabled": false
}
```

### POST /api/config

Update configuration.

**Example:**
```bash
curl -X POST http://127.0.0.1:8080/api/config \
  -H "Content-Type: application/json" \
  -d '{
    "marketSource": "binance_rest",
    "marketSymbol": "ETHUSDT",
    "executionMode": "binance_testnet_spot",
    "binanceApiKey": "your-api-key",
    "binanceApiSecret": "your-api-secret"
  }'
```

**Response:**
```json
{
  "ok": true,
  "message": "Configuration saved. Restart the gateway for changes to take effect.",
  "config_file": "/path/to/.veloz_config.json"
}
```

## Environment Variables Mapping

The UI configuration maps to these environment variables:

| UI Field | Environment Variable |
|----------|---------------------|
| Host | `VELOZ_HOST` |
| Port | `VELOZ_PORT` |
| Build Preset | `VELOZ_PRESET` |
| Market Data Source | `VELOZ_MARKET_SOURCE` |
| Trading Symbol | `VELOZ_MARKET_SYMBOL` |
| Execution Mode | `VELOZ_EXECUTION_MODE` |
| Binance Base URL | `VELOZ_BINANCE_BASE_URL` |
| Binance Trade Base URL | `VELOZ_BINANCE_TRADE_BASE_URL` |
| Binance WebSocket URL | `VELOZ_BINANCE_WS_BASE_URL` |
| Binance API Key | `VELOZ_BINANCE_API_KEY` |
| Binance API Secret | `VELOZ_BINANCE_API_SECRET` |
| OKX API Key | `VELOZ_OKX_API_KEY` |
| OKX API Secret | `VELOZ_OKX_API_SECRET` |
| OKX Passphrase | `VELOZ_OKX_PASSPHRASE` |
| Bybit API Key | `VELOZ_BYBIT_API_KEY` |
| Bybit API Secret | `VELOZ_BYBIT_API_SECRET` |
| Coinbase API Key | `VELOZ_COINBASE_API_KEY` |
| Coinbase API Secret | `VELOZ_COINBASE_API_SECRET` |

## See Also

- [Configuration Guide](configuration.md) - Complete environment variable reference
- [Binance Integration](binance.md) - Binance-specific setup details
- [Getting Started](getting-started.md) - Initial setup guide
- [Security Best Practices](security-best-practices.md) - Security recommendations
