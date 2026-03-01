#!/bin/bash
# VeloZ macOS Installation Test Script
# Tests installation, verification, and uninstallation on macOS

set -e

# Configuration
DMG_PATH="${1:-VeloZ.dmg}"
EXPECTED_VERSION="${2:-1.0.0}"
SKIP_UNINSTALL="${3:-false}"
APP_PATH="/Applications/VeloZ.app"

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
NC='\033[0m' # No Color

# Test counters
TESTS_PASSED=0
TESTS_FAILED=0

# Test result logging
test_result() {
    local test_name="$1"
    local passed="$2"
    local message="${3:-}"

    if [ "$passed" = "true" ]; then
        echo -e "${GREEN}[PASS]${NC} $test_name"
        ((TESTS_PASSED++))
    else
        echo -e "${RED}[FAIL]${NC} $test_name"
        if [ -n "$message" ]; then
            echo -e "       ${YELLOW}$message${NC}"
        fi
        ((TESTS_FAILED++))
    fi
}

section() {
    echo -e "\n${CYAN}=== $1 ===${NC}"
}

# Pre-installation checks
test_prerequisites() {
    section "Prerequisites"

    # Check DMG exists
    if [ -f "$DMG_PATH" ]; then
        test_result "DMG file exists" "true"
    else
        test_result "DMG file exists" "false" "Path: $DMG_PATH"
        exit 1
    fi

    # Check disk space (need at least 500MB)
    FREE_SPACE=$(df -k /Applications | tail -1 | awk '{print $4}')
    FREE_MB=$((FREE_SPACE / 1024))
    if [ "$FREE_MB" -gt 500 ]; then
        test_result "Sufficient disk space" "true" "Free: ${FREE_MB}MB"
    else
        test_result "Sufficient disk space" "false" "Free: ${FREE_MB}MB (need 500MB)"
    fi

    # Check for existing installation
    if [ -d "$APP_PATH" ]; then
        echo -e "${YELLOW}[INFO]${NC} Existing installation detected - will be replaced"
    fi
}

# Install from DMG
install_veloz() {
    section "Installation"

    local start_time=$(date +%s)

    # Mount DMG
    echo "Mounting DMG..."
    MOUNT_OUTPUT=$(hdiutil attach "$DMG_PATH" -nobrowse 2>&1)
    MOUNT_POINT=$(echo "$MOUNT_OUTPUT" | grep "/Volumes" | awk -F'\t' '{print $NF}')

    if [ -n "$MOUNT_POINT" ] && [ -d "$MOUNT_POINT" ]; then
        test_result "DMG mounted successfully" "true" "Mount point: $MOUNT_POINT"
    else
        test_result "DMG mounted successfully" "false" "Output: $MOUNT_OUTPUT"
        exit 1
    fi

    # Find app in DMG
    APP_IN_DMG=$(find "$MOUNT_POINT" -name "*.app" -maxdepth 1 | head -1)
    if [ -n "$APP_IN_DMG" ]; then
        test_result "App found in DMG" "true"
    else
        test_result "App found in DMG" "false"
        hdiutil detach "$MOUNT_POINT" 2>/dev/null || true
        exit 1
    fi

    # Remove existing installation
    if [ -d "$APP_PATH" ]; then
        echo "Removing existing installation..."
        rm -rf "$APP_PATH"
    fi

    # Copy to Applications
    echo "Installing to Applications..."
    cp -R "$APP_IN_DMG" /Applications/

    if [ -d "$APP_PATH" ]; then
        test_result "App copied to Applications" "true"
    else
        test_result "App copied to Applications" "false"
        hdiutil detach "$MOUNT_POINT" 2>/dev/null || true
        exit 1
    fi

    # Unmount DMG
    hdiutil detach "$MOUNT_POINT" 2>/dev/null || true

    local end_time=$(date +%s)
    local duration=$((end_time - start_time))

    if [ "$duration" -lt 180 ]; then
        test_result "Installation completed in < 3 minutes" "true" "Duration: ${duration}s"
    else
        test_result "Installation completed in < 3 minutes" "false" "Duration: ${duration}s"
    fi
}

# Verify installation
test_installation() {
    section "Installation Verification"

    # Check app bundle exists
    if [ -d "$APP_PATH" ]; then
        test_result "App bundle exists" "true"
    else
        test_result "App bundle exists" "false"
        return
    fi

    # Check main executable
    MAIN_EXEC="$APP_PATH/Contents/MacOS/VeloZ"
    if [ -x "$MAIN_EXEC" ]; then
        test_result "Main executable exists and is executable" "true"
    else
        test_result "Main executable exists and is executable" "false"
    fi

    # Check Info.plist
    INFO_PLIST="$APP_PATH/Contents/Info.plist"
    if [ -f "$INFO_PLIST" ]; then
        test_result "Info.plist exists" "true"
    else
        test_result "Info.plist exists" "false"
    fi

    # Check resources
    RESOURCES="$APP_PATH/Contents/Resources"
    if [ -d "$RESOURCES" ]; then
        test_result "Resources directory exists" "true"
    else
        test_result "Resources directory exists" "false"
    fi

    # Check engine
    ENGINE_PATH="$RESOURCES/engine/veloz_engine"
    if [ -x "$ENGINE_PATH" ]; then
        test_result "Trading engine exists" "true"
    else
        test_result "Trading engine exists" "false"
    fi

    # Check gateway
    GATEWAY_PATH="$RESOURCES/gateway"
    if [ -d "$GATEWAY_PATH" ]; then
        test_result "Gateway exists" "true"
    else
        test_result "Gateway exists" "false"
    fi

    # Check UI
    UI_PATH="$RESOURCES/ui/index.html"
    if [ -f "$UI_PATH" ]; then
        test_result "UI assets exist" "true"
    else
        test_result "UI assets exist" "false"
    fi
}

# Test code signing
test_code_signing() {
    section "Code Signing"

    # Check code signature
    if codesign -v "$APP_PATH" 2>/dev/null; then
        test_result "App is properly code-signed" "true"
    else
        test_result "App is properly code-signed" "false" "Run: codesign -v \"$APP_PATH\""
    fi

    # Check signature details
    SIGN_INFO=$(codesign -dv "$APP_PATH" 2>&1)
    if echo "$SIGN_INFO" | grep -q "Authority"; then
        test_result "Signature has authority chain" "true"
    else
        test_result "Signature has authority chain" "false"
    fi

    # Check notarization (Gatekeeper)
    SPCTL_OUTPUT=$(spctl -a -v "$APP_PATH" 2>&1)
    if echo "$SPCTL_OUTPUT" | grep -q "accepted"; then
        test_result "App passes Gatekeeper" "true"
    else
        test_result "App passes Gatekeeper" "false" "May not be notarized"
    fi

    # Check architecture
    ARCH_INFO=$(file "$APP_PATH/Contents/MacOS/VeloZ")
    if echo "$ARCH_INFO" | grep -q "arm64"; then
        echo -e "${CYAN}[INFO]${NC} Architecture: Apple Silicon (arm64)"
    elif echo "$ARCH_INFO" | grep -q "x86_64"; then
        echo -e "${CYAN}[INFO]${NC} Architecture: Intel (x86_64)"
    fi

    # Check if running natively (not Rosetta)
    if [ "$(uname -m)" = "arm64" ]; then
        if echo "$ARCH_INFO" | grep -q "arm64"; then
            test_result "App runs natively (not Rosetta)" "true"
        else
            test_result "App runs natively (not Rosetta)" "false" "x86_64 binary on arm64 Mac"
        fi
    fi
}

# Test application launch
test_launch() {
    section "Application Launch"

    MAIN_EXEC="$APP_PATH/Contents/MacOS/VeloZ"

    if [ ! -x "$MAIN_EXEC" ]; then
        test_result "Application launch" "false" "Executable not found"
        return
    fi

    # Start application in background
    echo "Starting VeloZ..."
    "$MAIN_EXEC" &
    APP_PID=$!

    # Wait for app to start
    sleep 5

    # Check if process is running
    if ps -p $APP_PID > /dev/null 2>&1; then
        test_result "Application started" "true"

        # Check for window (using AppleScript)
        WINDOW_COUNT=$(osascript -e 'tell application "System Events" to count windows of process "VeloZ"' 2>/dev/null || echo "0")
        if [ "$WINDOW_COUNT" -gt 0 ]; then
            test_result "Main window created" "true"
        else
            test_result "Main window created" "false" "No windows detected"
        fi

        # Clean up
        kill $APP_PID 2>/dev/null || true
        wait $APP_PID 2>/dev/null || true
    else
        test_result "Application started" "false" "Process exited immediately"
    fi
}

# Test engine functionality
test_engine() {
    section "Engine Functionality"

    ENGINE_PATH="$APP_PATH/Contents/Resources/engine/veloz_engine"

    if [ ! -x "$ENGINE_PATH" ]; then
        test_result "Engine test" "false" "Engine not found"
        return
    fi

    # Run engine help
    if "$ENGINE_PATH" --help > /dev/null 2>&1; then
        test_result "Engine smoke test (--help)" "true"
    else
        test_result "Engine smoke test (--help)" "false"
    fi

    # Check engine dependencies
    if otool -L "$ENGINE_PATH" | grep -q "not found"; then
        test_result "Engine dependencies satisfied" "false" "Missing libraries"
    else
        test_result "Engine dependencies satisfied" "true"
    fi
}

# Test version
test_version() {
    section "Version Check"

    INFO_PLIST="$APP_PATH/Contents/Info.plist"

    if [ -f "$INFO_PLIST" ]; then
        VERSION=$(/usr/libexec/PlistBuddy -c "Print CFBundleShortVersionString" "$INFO_PLIST" 2>/dev/null || echo "unknown")

        if [ "$VERSION" = "$EXPECTED_VERSION" ]; then
            test_result "Version matches expected" "true" "Version: $VERSION"
        else
            test_result "Version matches expected" "false" "Expected: $EXPECTED_VERSION, Got: $VERSION"
        fi
    else
        test_result "Version check" "false" "Info.plist not found"
    fi
}

# Test uninstallation
test_uninstall() {
    section "Uninstallation"

    if [ ! -d "$APP_PATH" ]; then
        test_result "Uninstall test" "false" "App not installed"
        return
    fi

    # Remove app
    echo "Removing VeloZ..."
    rm -rf "$APP_PATH"

    if [ ! -d "$APP_PATH" ]; then
        test_result "App removed from Applications" "true"
    else
        test_result "App removed from Applications" "false"
    fi

    # Check for leftover support files
    SUPPORT_DIR="$HOME/Library/Application Support/VeloZ"
    if [ -d "$SUPPORT_DIR" ]; then
        echo -e "${YELLOW}[INFO]${NC} Support files remain at: $SUPPORT_DIR"
        echo "       These can be manually removed if desired"
    fi

    # Check for preferences
    PREFS="$HOME/Library/Preferences/io.veloz.app.plist"
    if [ -f "$PREFS" ]; then
        echo -e "${YELLOW}[INFO]${NC} Preferences file remains at: $PREFS"
    fi
}

# Main execution
main() {
    echo -e "\n${CYAN}========================================${NC}"
    echo -e "${CYAN}   VeloZ macOS Installation Test${NC}"
    echo -e "${CYAN}========================================${NC}\n"

    echo "DMG: $DMG_PATH"
    echo "Expected Version: $EXPECTED_VERSION"
    echo ""

    test_prerequisites
    install_veloz
    test_installation
    test_code_signing
    test_version
    test_launch
    test_engine

    if [ "$SKIP_UNINSTALL" != "true" ]; then
        test_uninstall
    else
        echo -e "\n${YELLOW}[INFO]${NC} Skipping uninstall test"
    fi

    # Summary
    echo -e "\n${CYAN}========================================${NC}"
    echo -e "${CYAN}   Test Summary${NC}"
    echo -e "${CYAN}========================================${NC}"
    echo -e "${GREEN}Passed: $TESTS_PASSED${NC}"

    if [ "$TESTS_FAILED" -gt 0 ]; then
        echo -e "${RED}Failed: $TESTS_FAILED${NC}"
        echo -e "\n${RED}Some tests FAILED${NC}"
        exit 1
    else
        echo -e "${GREEN}Failed: 0${NC}"
        echo -e "\n${GREEN}All tests PASSED${NC}"
        exit 0
    fi
}

main
