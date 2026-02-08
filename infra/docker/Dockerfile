# VeloZ Trading Engine Dockerfile
# This Dockerfile builds a production-ready image of the VeloZ trading engine

FROM ubuntu:24.04 AS base
LABEL maintainer="VeloZ Team"
LABEL description="VeloZ Trading Engine - High Performance Crypto Trading System"

# Install build dependencies
FROM base AS builder
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    git \
    libssl-dev \
    python3-pip \
    && rm -rf /var/lib/apt/lists/*

# Install Python dependencies
RUN pip3 install --no-cache-dir flask gunicorn

# Create working directory
WORKDIR /app

# Copy source code
COPY . .

# Build the engine
RUN cmake --preset dev && cmake --build --preset dev -j

# Create production image
FROM base AS production

# Install runtime dependencies
RUN apt-get update && apt-get install -y \
    python3 \
    python3-flask \
    python3-gunicorn \
    && rm -rf /var/lib/apt/lists/*

# Create app user
RUN useradd -r -s /bin/false veloz

# Copy built engine from builder
WORKDIR /app
COPY --from=builder /app/build/dev/apps/engine/veloz_engine /app/veloz_engine
COPY --from=builder /app/apps/gateway/ /app/gateway/
COPY --from=builder /app/apps/ui/ /app/ui/

# Copy configuration files
COPY --from=builder /app/infra/docker/entrypoint.sh /app/entrypoint.sh

# Set permissions
RUN chown -R veloz:veloz /app && \
    chmod +x /app/veloz_engine && \
    chmod +x /app/entrypoint.sh

# Switch to non-root user
USER veloz

# Expose API port
EXPOSE 8080

# Set environment variables
ENV VELOZ_PRESET=dev
ENV VELOZ_HOST=0.0.0.0
ENV VELOZ_PORT=8080
ENV VELOZ_MARKET_SOURCE=binance_rest
ENV VELOZ_MARKET_SYMBOL=BTCUSDT
ENV VELOZ_EXECUTION_MODE=sim_engine
ENV VELOZ_BINANCE_BASE_URL=https://api.binance.com
ENV VELOZ_BINANCE_TRADE_BASE_URL=https://testnet.binance.vision
ENV VELOZ_BINANCE_API_KEY=""
ENV VELOZ_BINANCE_API_SECRET=""

# Health check
HEALTHCHECK --interval=30s --timeout=10s --start-period=5s --retries=3 \
    CMD curl -f http://localhost:8080/health || exit 1

# Entry point
ENTRYPOINT ["/app/entrypoint.sh"]

# Default command
CMD ["gunicorn", "--bind", "0.0.0.0:8080", "--workers", "4", "--threads", "2", "gateway.gateway:app"]
