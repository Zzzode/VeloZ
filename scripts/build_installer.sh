#!/bin/bash
# VeloZ Installer Build Script
# Builds cross-platform installers for VeloZ

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
INSTALLER_DIR="$PROJECT_ROOT/apps/installer"

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
NC='\033[0m'

# Default values
BUILD_TYPE="${1:-dev}"
PLATFORM="${2:-$(uname -s | tr '[:upper:]' '[:lower:]')}"
ARCH="${3:-$(uname -m)}"

# Normalize platform name
case "$PLATFORM" in
    darwin|macos|osx)
        PLATFORM="darwin"
        ;;
    linux)
        PLATFORM="linux"
        ;;
    windows|win32|win)
        PLATFORM="win32"
        ;;
esac

# Normalize architecture
case "$ARCH" in
    x86_64|amd64)
        ARCH="x64"
        ;;
    aarch64|arm64)
        ARCH="arm64"
        ;;
esac

echo -e "${CYAN}========================================${NC}"
echo -e "${CYAN}   VeloZ Installer Build${NC}"
echo -e "${CYAN}========================================${NC}"
echo ""
echo "Build Type: $BUILD_TYPE"
echo "Platform: $PLATFORM"
echo "Architecture: $ARCH"
echo ""

# Step 1: Build C++ Engine
build_engine() {
    echo -e "${CYAN}[1/4] Building C++ Engine...${NC}"

    cd "$PROJECT_ROOT"

    # Configure
    if [ "$BUILD_TYPE" = "release" ]; then
        cmake --preset release
        cmake --build --preset release-engine -j$(nproc 2>/dev/null || sysctl -n hw.ncpu)
        ENGINE_BUILD_DIR="$PROJECT_ROOT/build/release"
    else
        cmake --preset dev
        cmake --build --preset dev-engine -j$(nproc 2>/dev/null || sysctl -n hw.ncpu)
        ENGINE_BUILD_DIR="$PROJECT_ROOT/build/dev"
    fi

    # Copy engine to installer resources
    ENGINE_SRC="$ENGINE_BUILD_DIR/apps/engine/veloz_engine"
    if [ "$PLATFORM" = "win32" ]; then
        ENGINE_SRC="${ENGINE_SRC}.exe"
    fi

    if [ -f "$ENGINE_SRC" ]; then
        mkdir -p "$INSTALLER_DIR/resources/engine"
        cp "$ENGINE_SRC" "$INSTALLER_DIR/resources/engine/"

        # Copy any required DLLs/dylibs
        if [ "$PLATFORM" = "win32" ]; then
            cp "$ENGINE_BUILD_DIR/apps/engine/"*.dll "$INSTALLER_DIR/resources/engine/" 2>/dev/null || true
        elif [ "$PLATFORM" = "darwin" ]; then
            cp "$ENGINE_BUILD_DIR/apps/engine/"*.dylib "$INSTALLER_DIR/resources/engine/" 2>/dev/null || true
        else
            cp "$ENGINE_BUILD_DIR/apps/engine/"*.so "$INSTALLER_DIR/resources/engine/" 2>/dev/null || true
        fi

        echo -e "${GREEN}Engine built successfully${NC}"
    else
        echo -e "${RED}Engine build failed - binary not found${NC}"
        exit 1
    fi
}

# Step 2: Prepare Gateway
prepare_gateway() {
    echo -e "${CYAN}[2/4] Preparing Gateway...${NC}"

    GATEWAY_SRC="$PROJECT_ROOT/apps/gateway"
    GATEWAY_DEST="$INSTALLER_DIR/resources/gateway"

    mkdir -p "$GATEWAY_DEST"

    # Copy gateway binary
    if [ "$BUILD_TYPE" = "release" ]; then
        GATEWAY_BIN="$PROJECT_ROOT/build/release/apps/gateway/veloz_gateway"
    else
        GATEWAY_BIN="$PROJECT_ROOT/build/dev/apps/gateway/veloz_gateway"
    fi

    if [ -f "$GATEWAY_BIN" ]; then
        cp "$GATEWAY_BIN" "$GATEWAY_DEST/"
        echo -e "${GREEN}Gateway prepared successfully${NC}"
    else
        echo -e "${YELLOW}Gateway binary not found, skipping${NC}"
    fi
}

# Step 3: Build UI
build_ui() {
    echo -e "${CYAN}[3/4] Building UI...${NC}"

    UI_DIR="$PROJECT_ROOT/apps/ui"
    UI_DEST="$INSTALLER_DIR/resources/ui"

    cd "$UI_DIR"

    # Install dependencies if needed
    if [ ! -d "node_modules" ]; then
        echo "Installing UI dependencies..."
        npm ci
    fi

    # Build UI
    npm run build

    # Copy built UI to installer resources
    mkdir -p "$UI_DEST"
    cp -r dist/* "$UI_DEST/"

    echo -e "${GREEN}UI built successfully${NC}"
}

# Step 4: Build Installer
build_installer() {
    echo -e "${CYAN}[4/4] Building Installer...${NC}"

    cd "$INSTALLER_DIR"

    # Install dependencies if needed
    if [ ! -d "node_modules" ]; then
        echo "Installing installer dependencies..."
        npm ci
    fi

    # Build for specific platform
    case "$PLATFORM" in
        darwin)
            echo "Building macOS installer..."
            npm run make -- --platform darwin --arch "$ARCH"
            ;;
        linux)
            echo "Building Linux installer..."
            npm run make -- --platform linux --arch "$ARCH"
            ;;
        win32)
            echo "Building Windows installer..."
            npm run make -- --platform win32 --arch "$ARCH"
            ;;
        *)
            echo -e "${RED}Unknown platform: $PLATFORM${NC}"
            exit 1
            ;;
    esac

    echo -e "${GREEN}Installer built successfully${NC}"
}

# Show output location
show_output() {
    echo ""
    echo -e "${CYAN}========================================${NC}"
    echo -e "${CYAN}   Build Complete${NC}"
    echo -e "${CYAN}========================================${NC}"
    echo ""
    echo "Output location:"
    echo "  $INSTALLER_DIR/out/make/"
    echo ""

    # List built artifacts
    if [ -d "$INSTALLER_DIR/out/make" ]; then
        echo "Built artifacts:"
        find "$INSTALLER_DIR/out/make" -type f \( -name "*.exe" -o -name "*.dmg" -o -name "*.deb" -o -name "*.rpm" -o -name "*.zip" \) 2>/dev/null | while read -r file; do
            SIZE=$(du -h "$file" | cut -f1)
            echo "  $file ($SIZE)"
        done
    fi
}

# Clean build artifacts
clean() {
    echo -e "${CYAN}Cleaning build artifacts...${NC}"

    rm -rf "$INSTALLER_DIR/out"
    rm -rf "$INSTALLER_DIR/resources/engine/"*
    rm -rf "$INSTALLER_DIR/resources/gateway/"*
    rm -rf "$INSTALLER_DIR/resources/ui/"*

    echo -e "${GREEN}Clean complete${NC}"
}

# Main
main() {
    case "${1:-build}" in
        clean)
            clean
            ;;
        engine)
            build_engine
            ;;
        gateway)
            prepare_gateway
            ;;
        ui)
            build_ui
            ;;
        installer)
            build_installer
            ;;
        build|*)
            build_engine
            prepare_gateway
            build_ui
            build_installer
            show_output
            ;;
    esac
}

# Handle arguments
if [ "$1" = "clean" ]; then
    clean
    exit 0
fi

main "$@"
