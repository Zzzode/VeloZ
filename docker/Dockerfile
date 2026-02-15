# VeloZ Docker Image
# Multi-stage build for optimized production image

# =============================================================================
# Stage 1: Build environment
# =============================================================================
FROM ubuntu:22.04 AS builder

# Prevent interactive prompts
ENV DEBIAN_FRONTEND=noninteractive

# Install build dependencies
RUN apt-get update && apt-get install -y \
    cmake \
    ninja-build \
    clang-16 \
    libssl-dev \
    git \
    && rm -rf /var/lib/apt/lists/*

# Set compiler
ENV CC=clang-16
ENV CXX=clang++-16

# Copy source code
WORKDIR /src
COPY . .

# Build release version
RUN cmake --preset release \
    && cmake --build --preset release-all -j$(nproc)

# =============================================================================
# Stage 2: Runtime environment
# =============================================================================
FROM ubuntu:22.04 AS runtime

# Install runtime dependencies
RUN apt-get update && apt-get install -y \
    libssl3 \
    python3 \
    python3-pip \
    ca-certificates \
    curl \
    && rm -rf /var/lib/apt/lists/*

# Create non-root user
RUN useradd -m -s /bin/bash veloz

# Create directories
RUN mkdir -p /opt/veloz /var/lib/veloz /var/log/veloz \
    && chown -R veloz:veloz /opt/veloz /var/lib/veloz /var/log/veloz

# Copy built artifacts
COPY --from=builder /src/build/release/apps/engine/veloz_engine /opt/veloz/
COPY --from=builder /src/apps/gateway/gateway.py /opt/veloz/
COPY --from=builder /src/apps/ui /opt/veloz/ui/
COPY --from=builder /src/scripts /opt/veloz/scripts/

# Set permissions
RUN chmod +x /opt/veloz/veloz_engine /opt/veloz/scripts/*.sh

# Switch to non-root user
USER veloz
WORKDIR /opt/veloz

# Environment variables
ENV VELOZ_HOST=0.0.0.0
ENV VELOZ_PORT=8080
ENV VELOZ_PRESET=release
ENV VELOZ_MARKET_SOURCE=sim
ENV VELOZ_EXECUTION_MODE=sim_engine

# Health check
HEALTHCHECK --interval=30s --timeout=10s --start-period=5s --retries=3 \
    CMD curl -f http://localhost:${VELOZ_PORT}/health || exit 1

# Expose port
EXPOSE 8080

# Default command
CMD ["python3", "/opt/veloz/gateway.py"]

# =============================================================================
# Stage 3: Development environment (optional)
# =============================================================================
FROM builder AS development

# Install development tools
RUN apt-get update && apt-get install -y \
    gdb \
    valgrind \
    clang-format-16 \
    clang-tidy-16 \
    python3 \
    python3-pip \
    && rm -rf /var/lib/apt/lists/*

# Create non-root user
RUN useradd -m -s /bin/bash veloz
USER veloz
WORKDIR /src

# Default command for development
CMD ["/bin/bash"]
