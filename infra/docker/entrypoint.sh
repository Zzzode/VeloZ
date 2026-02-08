#!/bin/bash
# VeloZ Engine Entry Point

set -e

# Function to log messages
log() {
    echo "$(date '+%Y-%m-%d %H:%M:%S') - $1"
}

# Check if engine binary exists
if [ ! -f "/app/veloz_engine" ]; then
    log "ERROR: VeloZ engine binary not found at /app/veloz_engine"
    exit 1
fi

# Check if gateway code exists
if [ ! -f "/app/gateway/gateway.py" ]; then
    log "ERROR: Gateway code not found at /app/gateway/gateway.py"
    exit 1
fi

# Create logs directory
mkdir -p /app/logs

# Set default API key and secret if not provided
if [ -z "$VELOZ_BINANCE_API_KEY" ]; then
    log "WARNING: VELOZ_BINANCE_API_KEY not set, using dummy values"
    export VELOZ_BINANCE_API_KEY="dummy_key"
fi

if [ -z "$VELOZ_BINANCE_API_SECRET" ]; then
    log "WARNING: VELOZ_BINANCE_API_SECRET not set, using dummy values"
    export VELOZ_BINANCE_API_SECRET="dummy_secret"
fi

# Display configuration summary
log "VeloZ Engine Configuration:"
log "  PRESET: $VELOZ_PRESET"
log "  HOST: $VELOZ_HOST"
log "  PORT: $VELOZ_PORT"
log "  MARKET SOURCE: $VELOZ_MARKET_SOURCE"
log "  MARKET SYMBOL: $VELOZ_MARKET_SYMBOL"
log "  EXECUTION MODE: $VELOZ_EXECUTION_MODE"
log "  BINANCE BASE URL: $VELOZ_BINANCE_BASE_URL"
log "  BINANCE TRADE URL: $VELOZ_BINANCE_TRADE_BASE_URL"

# Run the engine via gateway
log "Starting VeloZ Engine..."
exec gunicorn --bind "$VELOZ_HOST:$VELOZ_PORT" --workers 4 --threads 2 gateway.gateway:app
