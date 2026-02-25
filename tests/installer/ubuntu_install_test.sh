#!/bin/bash
# VeloZ Ubuntu Installation Test Script
# Tests installation, verification, and uninstallation on Ubuntu/Debian

set -e

# Configuration
DEB_PATH="${1:-veloz_1.0.0_amd64.deb}"
EXPECTED_VERSION="${2:-1.0.0}"
SKIP_UNINSTALL="${3:-false}"

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

# Check if running as root or with sudo
check_sudo() {
    if [ "$EUID" -ne 0 ]; then
        echo -e "${YELLOW}[WARN]${NC} This script requires sudo privileges for installation"
        echo "Please run with: sudo $0 $*"
        exit 1
    fi
}

# Pre-installation checks
test_prerequisites() {
    section "Prerequisites"

    # Check DEB exists
    if [ -f "$DEB_PATH" ]; then
        test_result "DEB package exists" "true"
    else
        test_result "DEB package exists" "false" "Path: $DEB_PATH"
        exit 1
    fi

    # Check package integrity
    if dpkg-deb --info "$DEB_PATH" > /dev/null 2>&1; then
        test_result "Package is valid DEB" "true"
    else
        test_result "Package is valid DEB" "false"
        exit 1
    fi

    # Check disk space (need at least 500MB)
    FREE_SPACE=$(df -k /opt | tail -1 | awk '{print $4}')
    FREE_MB=$((FREE_SPACE / 1024))
    if [ "$FREE_MB" -gt 500 ]; then
        test_result "Sufficient disk space" "true" "Free: ${FREE_MB}MB"
    else
        test_result "Sufficient disk space" "false" "Free: ${FREE_MB}MB (need 500MB)"
    fi

    # Check for existing installation
    if dpkg -l | grep -q "veloz"; then
        echo -e "${YELLOW}[INFO]${NC} Existing installation detected - will be upgraded"
    fi

    # Check required dependencies are available
    DEPS_AVAILABLE=true
    for dep in libsecret-1-0 libnotify4; do
        if ! apt-cache show "$dep" > /dev/null 2>&1; then
            echo -e "${YELLOW}[WARN]${NC} Dependency $dep may not be available"
            DEPS_AVAILABLE=false
        fi
    done

    if [ "$DEPS_AVAILABLE" = "true" ]; then
        test_result "Required dependencies available" "true"
    else
        test_result "Required dependencies available" "false" "Some dependencies may be missing"
    fi
}

# Install package
install_veloz() {
    section "Installation"

    local start_time=$(date +%s)

    # Install package
    echo "Installing VeloZ..."
    if apt install -y "./$DEB_PATH" 2>&1; then
        test_result "Package installed successfully" "true"
    else
        test_result "Package installed successfully" "false"
        exit 1
    fi

    local end_time=$(date +%s)
    local duration=$((end_time - start_time))

    if [ "$duration" -lt 300 ]; then
        test_result "Installation completed in < 5 minutes" "true" "Duration: ${duration}s"
    else
        test_result "Installation completed in < 5 minutes" "false" "Duration: ${duration}s"
    fi
}

# Verify installation
test_installation() {
    section "Installation Verification"

    # Check package is installed
    if dpkg -l | grep -q "veloz"; then
        test_result "Package registered in dpkg" "true"
    else
        test_result "Package registered in dpkg" "false"
        return
    fi

    # Check binary exists
    BINARY_PATH=$(which veloz 2>/dev/null || echo "")
    if [ -n "$BINARY_PATH" ] && [ -x "$BINARY_PATH" ]; then
        test_result "Binary exists and is executable" "true" "Path: $BINARY_PATH"
    else
        # Check alternative location
        if [ -x "/opt/veloz/veloz" ]; then
            test_result "Binary exists and is executable" "true" "Path: /opt/veloz/veloz"
            BINARY_PATH="/opt/veloz/veloz"
        else
            test_result "Binary exists and is executable" "false"
        fi
    fi

    # Check installation directory
    if [ -d "/opt/veloz" ]; then
        test_result "Installation directory exists" "true"
    else
        test_result "Installation directory exists" "false"
    fi

    # Check resources
    if [ -d "/opt/veloz/resources" ]; then
        test_result "Resources directory exists" "true"
    else
        test_result "Resources directory exists" "false"
    fi

    # Check engine
    if [ -x "/opt/veloz/resources/engine/veloz_engine" ]; then
        test_result "Trading engine exists" "true"
    else
        test_result "Trading engine exists" "false"
    fi

    # Check gateway
    if [ -d "/opt/veloz/resources/gateway" ]; then
        test_result "Gateway exists" "true"
    else
        test_result "Gateway exists" "false"
    fi

    # Check UI
    if [ -f "/opt/veloz/resources/ui/index.html" ]; then
        test_result "UI assets exist" "true"
    else
        test_result "UI assets exist" "false"
    fi

    # Check desktop entry
    if [ -f "/usr/share/applications/veloz.desktop" ]; then
        test_result "Desktop entry exists" "true"
    else
        test_result "Desktop entry exists" "false"
    fi

    # Validate desktop entry
    if [ -f "/usr/share/applications/veloz.desktop" ]; then
        if desktop-file-validate /usr/share/applications/veloz.desktop 2>/dev/null; then
            test_result "Desktop entry is valid" "true"
        else
            test_result "Desktop entry is valid" "false"
        fi
    fi

    # Check icon
    if [ -f "/usr/share/icons/hicolor/256x256/apps/veloz.png" ] || \
       [ -f "/usr/share/pixmaps/veloz.png" ]; then
        test_result "Application icon exists" "true"
    else
        test_result "Application icon exists" "false"
    fi
}

# Test dependencies
test_dependencies() {
    section "Dependencies"

    BINARY_PATH="/opt/veloz/veloz"
    if [ ! -x "$BINARY_PATH" ]; then
        BINARY_PATH=$(which veloz 2>/dev/null || echo "")
    fi

    if [ -z "$BINARY_PATH" ]; then
        test_result "Dependency check" "false" "Binary not found"
        return
    fi

    # Check for missing shared libraries
    MISSING_LIBS=$(ldd "$BINARY_PATH" 2>/dev/null | grep "not found" || true)
    if [ -z "$MISSING_LIBS" ]; then
        test_result "All shared libraries found" "true"
    else
        test_result "All shared libraries found" "false" "Missing: $MISSING_LIBS"
    fi

    # Check engine dependencies
    ENGINE_PATH="/opt/veloz/resources/engine/veloz_engine"
    if [ -x "$ENGINE_PATH" ]; then
        ENGINE_MISSING=$(ldd "$ENGINE_PATH" 2>/dev/null | grep "not found" || true)
        if [ -z "$ENGINE_MISSING" ]; then
            test_result "Engine dependencies satisfied" "true"
        else
            test_result "Engine dependencies satisfied" "false" "Missing: $ENGINE_MISSING"
        fi
    fi
}

# Test version
test_version() {
    section "Version Check"

    # Get installed version from dpkg
    INSTALLED_VERSION=$(dpkg -l veloz 2>/dev/null | grep "veloz" | awk '{print $3}' || echo "unknown")

    if [ "$INSTALLED_VERSION" = "$EXPECTED_VERSION" ]; then
        test_result "Version matches expected" "true" "Version: $INSTALLED_VERSION"
    else
        test_result "Version matches expected" "false" "Expected: $EXPECTED_VERSION, Got: $INSTALLED_VERSION"
    fi

    # Try to get version from binary
    BINARY_PATH="/opt/veloz/veloz"
    if [ -x "$BINARY_PATH" ]; then
        BINARY_VERSION=$("$BINARY_PATH" --version 2>/dev/null | head -1 || echo "unknown")
        echo -e "${CYAN}[INFO]${NC} Binary reports: $BINARY_VERSION"
    fi
}

# Test application launch
test_launch() {
    section "Application Launch"

    BINARY_PATH="/opt/veloz/veloz"
    if [ ! -x "$BINARY_PATH" ]; then
        BINARY_PATH=$(which veloz 2>/dev/null || echo "")
    fi

    if [ -z "$BINARY_PATH" ]; then
        test_result "Application launch" "false" "Binary not found"
        return
    fi

    # Check if we have a display
    if [ -z "$DISPLAY" ]; then
        echo -e "${YELLOW}[INFO]${NC} No display available - skipping GUI launch test"
        test_result "Application launch (headless)" "true" "Skipped - no display"
        return
    fi

    # Start application in background
    echo "Starting VeloZ..."
    "$BINARY_PATH" &
    APP_PID=$!

    # Wait for app to start
    sleep 5

    # Check if process is running
    if ps -p $APP_PID > /dev/null 2>&1; then
        test_result "Application started" "true"

        # Clean up
        kill $APP_PID 2>/dev/null || true
        wait $APP_PID 2>/dev/null || true
    else
        # Check exit code
        wait $APP_PID 2>/dev/null
        EXIT_CODE=$?
        test_result "Application started" "false" "Process exited with code: $EXIT_CODE"
    fi
}

# Test engine functionality
test_engine() {
    section "Engine Functionality"

    ENGINE_PATH="/opt/veloz/resources/engine/veloz_engine"

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
}

# Test command line interface
test_cli() {
    section "Command Line Interface"

    # Check if veloz is in PATH
    if command -v veloz > /dev/null 2>&1; then
        test_result "veloz command available in PATH" "true"
    else
        test_result "veloz command available in PATH" "false"
    fi

    # Check man page (if installed)
    if man -w veloz > /dev/null 2>&1; then
        test_result "Man page installed" "true"
    else
        test_result "Man page installed" "false" "Optional"
    fi
}

# Test uninstallation
test_uninstall() {
    section "Uninstallation"

    if ! dpkg -l | grep -q "veloz"; then
        test_result "Uninstall test" "false" "Package not installed"
        return
    fi

    # Remove package
    echo "Removing VeloZ..."
    if apt remove -y veloz 2>&1; then
        test_result "Package removed" "true"
    else
        test_result "Package removed" "false"
        return
    fi

    # Verify removal
    if ! dpkg -l | grep -q "^ii.*veloz"; then
        test_result "Package no longer registered" "true"
    else
        test_result "Package no longer registered" "false"
    fi

    # Check binary removed
    if ! command -v veloz > /dev/null 2>&1; then
        test_result "Binary removed from PATH" "true"
    else
        test_result "Binary removed from PATH" "false"
    fi

    # Check desktop entry removed
    if [ ! -f "/usr/share/applications/veloz.desktop" ]; then
        test_result "Desktop entry removed" "true"
    else
        test_result "Desktop entry removed" "false"
    fi

    # Purge to remove config files
    echo "Purging configuration..."
    apt purge -y veloz 2>/dev/null || true

    # Check for leftover user config
    if [ -d "$HOME/.config/veloz" ]; then
        echo -e "${YELLOW}[INFO]${NC} User config remains at: $HOME/.config/veloz"
        echo "       This can be manually removed if desired"
    fi
}

# Main execution
main() {
    echo -e "\n${CYAN}========================================${NC}"
    echo -e "${CYAN}   VeloZ Ubuntu Installation Test${NC}"
    echo -e "${CYAN}========================================${NC}\n"

    echo "DEB: $DEB_PATH"
    echo "Expected Version: $EXPECTED_VERSION"
    echo ""

    check_sudo

    test_prerequisites
    install_veloz
    test_installation
    test_dependencies
    test_version
    test_cli
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
