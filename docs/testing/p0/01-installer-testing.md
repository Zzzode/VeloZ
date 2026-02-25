# One-click Installer Testing

## 1. Test Strategy

### 1.1 Objectives

- Verify seamless installation experience across all supported platforms
- Ensure all dependencies are correctly bundled and installed
- Validate upgrade and rollback scenarios
- Confirm uninstall completely removes all components
- Test installation under various system configurations

### 1.2 Scope

| In Scope | Out of Scope |
|----------|--------------|
| Windows 10/11 (x64) | Windows ARM |
| macOS 12+ (Intel & Apple Silicon) | macOS < 12 |
| Ubuntu 22.04+ (x64) | Other Linux distributions |
| Fresh install scenarios | Enterprise deployment |
| Upgrade from previous versions | Silent/unattended install |
| Rollback scenarios | Custom installation paths |
| Uninstall verification | Network installation |

### 1.3 Test Approach

1. **Platform Matrix Testing**: Test on all supported OS versions
2. **Clean Room Testing**: Use fresh VM images for each test
3. **Dependency Verification**: Validate all bundled components
4. **Regression Testing**: Automated tests for each build
5. **User Journey Testing**: End-to-end installation workflows

## 2. Test Environment

### 2.1 Platform Matrix

| Platform | Version | Architecture | VM Image |
|----------|---------|--------------|----------|
| Windows | 10 21H2 | x64 | windows-10-21h2-clean |
| Windows | 11 23H2 | x64 | windows-11-23h2-clean |
| macOS | 12 Monterey | Intel | macos-12-intel-clean |
| macOS | 13 Ventura | Intel | macos-13-intel-clean |
| macOS | 14 Sonoma | Apple Silicon | macos-14-arm-clean |
| Ubuntu | 22.04 LTS | x64 | ubuntu-22.04-clean |
| Ubuntu | 24.04 LTS | x64 | ubuntu-24.04-clean |

### 2.2 Test Data Requirements

- Previous version installers (for upgrade testing)
- Sample configuration files
- Test exchange API keys (testnet)
- Network simulation tools

## 3. Test Cases

### 3.1 Fresh Install Scenarios

#### TC-INS-001: Fresh Install on Windows 10

| Field | Value |
|-------|-------|
| **ID** | TC-INS-001 |
| **Priority** | P0 |
| **Preconditions** | Clean Windows 10 21H2 VM, no previous VeloZ installation |
| **Test Data** | VeloZ installer v1.0.0 |

**Steps:**
1. Download VeloZ installer from official website
2. Double-click installer executable
3. Accept license agreement
4. Select installation directory (default)
5. Click "Install"
6. Wait for installation to complete
7. Click "Finish" to launch VeloZ

**Expected Results:**
- [ ] Installer launches without UAC errors
- [ ] Progress bar shows accurate progress
- [ ] Installation completes in < 2 minutes
- [ ] Desktop shortcut created
- [ ] Start menu entry created
- [ ] VeloZ launches successfully
- [ ] All dependencies installed (Visual C++ Runtime)
- [ ] No error messages in Windows Event Log

#### TC-INS-002: Fresh Install on Windows 11

| Field | Value |
|-------|-------|
| **ID** | TC-INS-002 |
| **Priority** | P0 |
| **Preconditions** | Clean Windows 11 23H2 VM |

**Steps:** Same as TC-INS-001

**Expected Results:** Same as TC-INS-001, plus:
- [ ] Windows Defender SmartScreen does not block installer
- [ ] Installer signed with valid code signing certificate

#### TC-INS-003: Fresh Install on macOS Intel

| Field | Value |
|-------|-------|
| **ID** | TC-INS-003 |
| **Priority** | P0 |
| **Preconditions** | Clean macOS 13 Ventura (Intel), no previous VeloZ installation |

**Steps:**
1. Download VeloZ.dmg from official website
2. Double-click DMG to mount
3. Drag VeloZ.app to Applications folder
4. Eject DMG
5. Launch VeloZ from Applications
6. Grant necessary permissions when prompted

**Expected Results:**
- [ ] DMG mounts without Gatekeeper warnings
- [ ] App is properly code-signed and notarized
- [ ] App launches without "unidentified developer" warning
- [ ] Permissions dialog appears for network access
- [ ] App appears in Launchpad
- [ ] No crash reports generated

#### TC-INS-004: Fresh Install on macOS Apple Silicon

| Field | Value |
|-------|-------|
| **ID** | TC-INS-004 |
| **Priority** | P0 |
| **Preconditions** | Clean macOS 14 Sonoma (Apple Silicon) |

**Steps:** Same as TC-INS-003

**Expected Results:** Same as TC-INS-003, plus:
- [ ] App runs natively (not via Rosetta 2)
- [ ] Activity Monitor shows "Apple" architecture

#### TC-INS-005: Fresh Install on Ubuntu 22.04

| Field | Value |
|-------|-------|
| **ID** | TC-INS-005 |
| **Priority** | P0 |
| **Preconditions** | Clean Ubuntu 22.04 LTS, standard desktop installation |

**Steps:**
1. Download VeloZ.deb from official website
2. Open terminal in download directory
3. Run: `sudo apt install ./veloz_1.0.0_amd64.deb`
4. Enter password when prompted
5. Launch VeloZ from application menu

**Expected Results:**
- [ ] Package installs without dependency errors
- [ ] All required libraries installed automatically
- [ ] Desktop entry created in /usr/share/applications
- [ ] Binary installed in /usr/bin or /opt/veloz
- [ ] Man page installed (optional)
- [ ] VeloZ launches from application menu
- [ ] VeloZ launches from command line: `veloz`

#### TC-INS-006: Fresh Install on Ubuntu 24.04

| Field | Value |
|-------|-------|
| **ID** | TC-INS-006 |
| **Priority** | P0 |
| **Preconditions** | Clean Ubuntu 24.04 LTS |

**Steps:** Same as TC-INS-005

**Expected Results:** Same as TC-INS-005

### 3.2 Upgrade Scenarios

#### TC-UPG-001: Upgrade from v0.9 to v1.0 (Windows)

| Field | Value |
|-------|-------|
| **ID** | TC-UPG-001 |
| **Priority** | P0 |
| **Preconditions** | Windows 10/11 with VeloZ v0.9 installed and configured |
| **Test Data** | VeloZ v0.9 installer, v1.0 installer, sample config |

**Steps:**
1. Verify v0.9 is installed and working
2. Note current configuration (API keys, strategies)
3. Download v1.0 installer
4. Run v1.0 installer
5. Select "Upgrade existing installation"
6. Complete installation
7. Launch VeloZ v1.0

**Expected Results:**
- [ ] Installer detects existing installation
- [ ] User prompted to upgrade (not fresh install)
- [ ] Configuration files preserved
- [ ] API keys retained (encrypted)
- [ ] Strategy configurations migrated
- [ ] Historical data preserved
- [ ] v1.0 launches with previous settings
- [ ] No duplicate entries in Start menu

#### TC-UPG-002: Upgrade from v0.9 to v1.0 (macOS)

| Field | Value |
|-------|-------|
| **ID** | TC-UPG-002 |
| **Priority** | P0 |
| **Preconditions** | macOS with VeloZ v0.9 installed |

**Steps:**
1. Verify v0.9 is installed in /Applications
2. Note current configuration
3. Download v1.0 DMG
4. Drag v1.0 app to Applications (replace existing)
5. Launch VeloZ v1.0

**Expected Results:**
- [ ] Replacement dialog appears
- [ ] Configuration in ~/Library/Application Support/VeloZ preserved
- [ ] Keychain entries preserved
- [ ] v1.0 launches with previous settings

#### TC-UPG-003: Upgrade from v0.9 to v1.0 (Ubuntu)

| Field | Value |
|-------|-------|
| **ID** | TC-UPG-003 |
| **Priority** | P0 |
| **Preconditions** | Ubuntu with VeloZ v0.9 installed |

**Steps:**
1. Verify v0.9 is installed: `dpkg -l | grep veloz`
2. Note current configuration in ~/.config/veloz
3. Download v1.0 deb package
4. Run: `sudo apt install ./veloz_1.0.0_amd64.deb`
5. Launch VeloZ v1.0

**Expected Results:**
- [ ] apt detects upgrade scenario
- [ ] Configuration in ~/.config/veloz preserved
- [ ] v1.0 launches with previous settings
- [ ] `dpkg -l | grep veloz` shows v1.0

### 3.3 Rollback Scenarios

#### TC-RBK-001: Rollback from v1.0 to v0.9 (Windows)

| Field | Value |
|-------|-------|
| **ID** | TC-RBK-001 |
| **Priority** | P1 |
| **Preconditions** | Windows with VeloZ v1.0 installed (upgraded from v0.9) |

**Steps:**
1. Uninstall VeloZ v1.0 via Control Panel
2. Select "Keep configuration files" if prompted
3. Install VeloZ v0.9
4. Launch VeloZ v0.9

**Expected Results:**
- [ ] v1.0 uninstalls cleanly
- [ ] Configuration files preserved (if selected)
- [ ] v0.9 installs successfully
- [ ] v0.9 can read preserved configuration
- [ ] Warning shown if config format incompatible

#### TC-RBK-002: Rollback via Windows System Restore

| Field | Value |
|-------|-------|
| **ID** | TC-RBK-002 |
| **Priority** | P2 |
| **Preconditions** | Windows with System Restore enabled, restore point before v1.0 upgrade |

**Steps:**
1. Open System Restore
2. Select restore point before v1.0 upgrade
3. Perform system restore
4. Launch VeloZ

**Expected Results:**
- [ ] System restore completes successfully
- [ ] VeloZ v0.9 restored
- [ ] Configuration restored to pre-upgrade state

### 3.4 Uninstall Scenarios

#### TC-UNI-001: Complete Uninstall (Windows)

| Field | Value |
|-------|-------|
| **ID** | TC-UNI-001 |
| **Priority** | P0 |
| **Preconditions** | Windows with VeloZ installed and configured |

**Steps:**
1. Open Control Panel > Programs and Features
2. Select VeloZ
3. Click Uninstall
4. Select "Remove all data including configuration"
5. Complete uninstall
6. Restart computer

**Expected Results:**
- [ ] Uninstaller runs without errors
- [ ] All files in Program Files removed
- [ ] Desktop shortcut removed
- [ ] Start menu entry removed
- [ ] Registry entries cleaned up
- [ ] AppData folder removed (if selected)
- [ ] No VeloZ processes running after restart
- [ ] Windows Event Log shows clean uninstall

#### TC-UNI-002: Uninstall Preserving Configuration (Windows)

| Field | Value |
|-------|-------|
| **ID** | TC-UNI-002 |
| **Priority** | P1 |
| **Preconditions** | Windows with VeloZ installed and configured |

**Steps:**
1. Uninstall VeloZ
2. Select "Keep configuration files"
3. Complete uninstall
4. Verify AppData folder contents

**Expected Results:**
- [ ] Application files removed
- [ ] Configuration files in AppData preserved
- [ ] API keys preserved (encrypted)
- [ ] Re-installation can use preserved config

#### TC-UNI-003: Complete Uninstall (macOS)

| Field | Value |
|-------|-------|
| **ID** | TC-UNI-003 |
| **Priority** | P0 |
| **Preconditions** | macOS with VeloZ installed |

**Steps:**
1. Quit VeloZ if running
2. Drag VeloZ.app from Applications to Trash
3. Empty Trash
4. Remove ~/Library/Application Support/VeloZ (optional)
5. Remove ~/Library/Preferences/com.veloz.* (optional)
6. Remove Keychain entries (optional)

**Expected Results:**
- [ ] App removed from Applications
- [ ] No VeloZ processes running
- [ ] Support files removable manually
- [ ] Keychain entries removable manually

#### TC-UNI-004: Complete Uninstall (Ubuntu)

| Field | Value |
|-------|-------|
| **ID** | TC-UNI-004 |
| **Priority** | P0 |
| **Preconditions** | Ubuntu with VeloZ installed |

**Steps:**
1. Run: `sudo apt remove veloz`
2. Run: `sudo apt purge veloz` (to remove config)
3. Remove ~/.config/veloz manually if needed

**Expected Results:**
- [ ] Package removed cleanly
- [ ] `dpkg -l | grep veloz` shows no results
- [ ] No veloz binary in PATH
- [ ] Desktop entry removed

### 3.5 Dependency Verification

#### TC-DEP-001: Visual C++ Runtime (Windows)

| Field | Value |
|-------|-------|
| **ID** | TC-DEP-001 |
| **Priority** | P0 |
| **Preconditions** | Clean Windows without Visual C++ Runtime |

**Steps:**
1. Verify no Visual C++ Runtime installed
2. Install VeloZ
3. Launch VeloZ

**Expected Results:**
- [ ] Installer bundles or downloads VC++ Runtime
- [ ] VC++ Runtime installed automatically
- [ ] VeloZ launches without DLL errors

#### TC-DEP-002: OpenSSL (All Platforms)

| Field | Value |
|-------|-------|
| **ID** | TC-DEP-002 |
| **Priority** | P0 |
| **Preconditions** | Fresh OS installation |

**Steps:**
1. Install VeloZ
2. Launch VeloZ
3. Attempt to connect to exchange (HTTPS)

**Expected Results:**
- [ ] OpenSSL bundled with installer
- [ ] HTTPS connections work
- [ ] No SSL/TLS errors

#### TC-DEP-003: Python Runtime (Gateway)

| Field | Value |
|-------|-------|
| **ID** | TC-DEP-003 |
| **Priority** | P0 |
| **Preconditions** | System without Python installed |

**Steps:**
1. Install VeloZ
2. Start VeloZ gateway component
3. Access HTTP API

**Expected Results:**
- [ ] Python runtime bundled or installed
- [ ] Gateway starts successfully
- [ ] API endpoints respond

### 3.6 Edge Cases

#### TC-EDGE-001: Installation with Insufficient Disk Space

| Field | Value |
|-------|-------|
| **ID** | TC-EDGE-001 |
| **Priority** | P1 |
| **Preconditions** | System with < 100MB free disk space |

**Steps:**
1. Attempt to install VeloZ
2. Observe installer behavior

**Expected Results:**
- [ ] Installer checks disk space before starting
- [ ] Clear error message shown
- [ ] No partial installation left behind

#### TC-EDGE-002: Installation without Admin Rights

| Field | Value |
|-------|-------|
| **ID** | TC-EDGE-002 |
| **Priority** | P1 |
| **Preconditions** | Windows standard user account (non-admin) |

**Steps:**
1. Attempt to install VeloZ
2. Observe installer behavior

**Expected Results:**
- [ ] UAC prompt appears for elevation
- [ ] If denied, clear error message shown
- [ ] Option for per-user installation (if supported)

#### TC-EDGE-003: Installation with Antivirus Active

| Field | Value |
|-------|-------|
| **ID** | TC-EDGE-003 |
| **Priority** | P1 |
| **Preconditions** | Windows with active antivirus (Windows Defender, Norton, etc.) |

**Steps:**
1. Install VeloZ with antivirus active
2. Launch VeloZ
3. Check antivirus logs

**Expected Results:**
- [ ] No false positive detections
- [ ] Installation completes successfully
- [ ] VeloZ not quarantined
- [ ] Network connections not blocked

#### TC-EDGE-004: Installation Interrupted

| Field | Value |
|-------|-------|
| **ID** | TC-EDGE-004 |
| **Priority** | P1 |
| **Preconditions** | Clean system |

**Steps:**
1. Start VeloZ installation
2. Cancel installation at 50% progress
3. Attempt to install again

**Expected Results:**
- [ ] Partial installation cleaned up
- [ ] Re-installation works correctly
- [ ] No orphaned files or registry entries

#### TC-EDGE-005: Network Disconnection During Install

| Field | Value |
|-------|-------|
| **ID** | TC-EDGE-005 |
| **Priority** | P2 |
| **Preconditions** | System with network connection |

**Steps:**
1. Start VeloZ installation
2. Disconnect network during installation
3. Observe installer behavior

**Expected Results:**
- [ ] Offline installation completes (if fully bundled)
- [ ] OR clear error message if network required
- [ ] Retry option available

## 4. Automated Test Scripts

### 4.1 Windows Installation Test (PowerShell)

```powershell
# File: tests/installer/windows_install_test.ps1

param(
    [string]$InstallerPath,
    [string]$ExpectedVersion = "1.0.0"
)

$ErrorActionPreference = "Stop"

function Test-VeloZInstallation {
    Write-Host "Testing VeloZ Installation..."

    # Check installation directory
    $installDir = "${env:ProgramFiles}\VeloZ"
    if (-not (Test-Path $installDir)) {
        throw "Installation directory not found: $installDir"
    }

    # Check executable
    $exePath = Join-Path $installDir "veloz.exe"
    if (-not (Test-Path $exePath)) {
        throw "Executable not found: $exePath"
    }

    # Check version
    $version = & $exePath --version 2>&1
    if ($version -notmatch $ExpectedVersion) {
        throw "Version mismatch. Expected: $ExpectedVersion, Got: $version"
    }

    # Check desktop shortcut
    $shortcut = "$env:USERPROFILE\Desktop\VeloZ.lnk"
    if (-not (Test-Path $shortcut)) {
        Write-Warning "Desktop shortcut not found"
    }

    # Check Start menu entry
    $startMenu = "$env:ProgramData\Microsoft\Windows\Start Menu\Programs\VeloZ"
    if (-not (Test-Path $startMenu)) {
        Write-Warning "Start menu entry not found"
    }

    # Check Windows service (if applicable)
    $service = Get-Service -Name "VeloZ" -ErrorAction SilentlyContinue
    if ($service) {
        Write-Host "VeloZ service found: $($service.Status)"
    }

    Write-Host "Installation verification PASSED" -ForegroundColor Green
    return $true
}

function Test-VeloZLaunch {
    Write-Host "Testing VeloZ Launch..."

    $process = Start-Process -FilePath "${env:ProgramFiles}\VeloZ\veloz.exe" `
        -ArgumentList "--smoke-test" -PassThru -Wait -NoNewWindow

    if ($process.ExitCode -ne 0) {
        throw "VeloZ smoke test failed with exit code: $($process.ExitCode)"
    }

    Write-Host "Launch verification PASSED" -ForegroundColor Green
    return $true
}

# Main execution
try {
    # Install
    Write-Host "Installing VeloZ from: $InstallerPath"
    Start-Process -FilePath $InstallerPath -ArgumentList "/S" -Wait

    # Verify
    Test-VeloZInstallation
    Test-VeloZLaunch

    Write-Host "`nAll tests PASSED" -ForegroundColor Green
    exit 0
} catch {
    Write-Host "TEST FAILED: $_" -ForegroundColor Red
    exit 1
}
```

### 4.2 macOS Installation Test (Bash)

```bash
#!/bin/bash
# File: tests/installer/macos_install_test.sh

set -e

DMG_PATH="${1:-VeloZ.dmg}"
EXPECTED_VERSION="${2:-1.0.0}"
APP_PATH="/Applications/VeloZ.app"

echo "=== VeloZ macOS Installation Test ==="

# Mount DMG
echo "Mounting DMG..."
MOUNT_POINT=$(hdiutil attach "$DMG_PATH" -nobrowse | grep "/Volumes" | awk '{print $3}')
echo "Mounted at: $MOUNT_POINT"

# Copy to Applications
echo "Installing to Applications..."
cp -R "$MOUNT_POINT/VeloZ.app" /Applications/

# Unmount
hdiutil detach "$MOUNT_POINT"

# Verify installation
echo "Verifying installation..."

if [ ! -d "$APP_PATH" ]; then
    echo "FAILED: App not found at $APP_PATH"
    exit 1
fi

# Check code signature
echo "Checking code signature..."
codesign -v "$APP_PATH"
if [ $? -ne 0 ]; then
    echo "FAILED: Code signature invalid"
    exit 1
fi

# Check notarization
echo "Checking notarization..."
spctl -a -v "$APP_PATH" 2>&1 | grep -q "accepted"
if [ $? -ne 0 ]; then
    echo "WARNING: App may not be notarized"
fi

# Check architecture
echo "Checking architecture..."
ARCH=$(file "$APP_PATH/Contents/MacOS/VeloZ" | grep -o "arm64\|x86_64")
echo "Architecture: $ARCH"

# Check version
echo "Checking version..."
VERSION=$("$APP_PATH/Contents/MacOS/VeloZ" --version 2>&1 | head -1)
if [[ "$VERSION" != *"$EXPECTED_VERSION"* ]]; then
    echo "FAILED: Version mismatch. Expected: $EXPECTED_VERSION, Got: $VERSION"
    exit 1
fi

# Launch test
echo "Running smoke test..."
"$APP_PATH/Contents/MacOS/VeloZ" --smoke-test
if [ $? -ne 0 ]; then
    echo "FAILED: Smoke test failed"
    exit 1
fi

echo ""
echo "=== All tests PASSED ==="
exit 0
```

### 4.3 Ubuntu Installation Test (Bash)

```bash
#!/bin/bash
# File: tests/installer/ubuntu_install_test.sh

set -e

DEB_PATH="${1:-veloz_1.0.0_amd64.deb}"
EXPECTED_VERSION="${2:-1.0.0}"

echo "=== VeloZ Ubuntu Installation Test ==="

# Install package
echo "Installing package..."
sudo apt install -y "./$DEB_PATH"

# Verify installation
echo "Verifying installation..."

# Check package installed
dpkg -l | grep -q veloz
if [ $? -ne 0 ]; then
    echo "FAILED: Package not installed"
    exit 1
fi

# Check binary exists
which veloz > /dev/null
if [ $? -ne 0 ]; then
    echo "FAILED: veloz binary not in PATH"
    exit 1
fi

# Check version
VERSION=$(veloz --version 2>&1 | head -1)
if [[ "$VERSION" != *"$EXPECTED_VERSION"* ]]; then
    echo "FAILED: Version mismatch. Expected: $EXPECTED_VERSION, Got: $VERSION"
    exit 1
fi

# Check desktop entry
if [ ! -f /usr/share/applications/veloz.desktop ]; then
    echo "WARNING: Desktop entry not found"
fi

# Check dependencies
echo "Checking dependencies..."
ldd $(which veloz) | grep "not found"
if [ $? -eq 0 ]; then
    echo "FAILED: Missing dependencies"
    exit 1
fi

# Launch test
echo "Running smoke test..."
veloz --smoke-test
if [ $? -ne 0 ]; then
    echo "FAILED: Smoke test failed"
    exit 1
fi

echo ""
echo "=== All tests PASSED ==="
exit 0
```

## 5. Performance Benchmarks

### 5.1 Installation Time Targets

| Platform | Fresh Install | Upgrade | Uninstall |
|----------|---------------|---------|-----------|
| Windows | < 2 min | < 1 min | < 30 sec |
| macOS | < 1 min | < 30 sec | < 10 sec |
| Ubuntu | < 1 min | < 30 sec | < 10 sec |

### 5.2 Disk Space Requirements

| Component | Size |
|-----------|------|
| Core Engine | 50 MB |
| Gateway | 30 MB |
| UI Assets | 20 MB |
| Dependencies | 100 MB |
| **Total** | **200 MB** |

### 5.3 Memory Requirements

| Scenario | Memory |
|----------|--------|
| Idle | < 100 MB |
| Active Trading | < 500 MB |
| Peak (backtest) | < 2 GB |

## 6. Security Testing Checklist

- [ ] Installer is code-signed with valid certificate
- [ ] Certificate not expired
- [ ] Certificate from trusted CA
- [ ] No unsigned DLLs bundled
- [ ] Installer hash matches published checksum
- [ ] HTTPS used for all downloads
- [ ] No sensitive data in installer logs
- [ ] Installation directory has appropriate permissions
- [ ] Configuration files have restricted permissions
- [ ] API keys encrypted at rest
- [ ] No credentials in environment variables
- [ ] Uninstall removes all sensitive data (if selected)

## 7. UAT Plan

### 7.1 UAT Participants

- 5 Windows users (mix of 10/11)
- 3 macOS users (mix of Intel/Apple Silicon)
- 2 Ubuntu users

### 7.2 UAT Scenarios

1. **First-time User Installation**
   - Download from website
   - Install without assistance
   - Complete initial setup
   - Rate experience (1-5)

2. **Upgrade Experience**
   - Receive upgrade notification
   - Perform upgrade
   - Verify settings preserved
   - Rate experience (1-5)

3. **Uninstall and Reinstall**
   - Uninstall completely
   - Reinstall fresh
   - Reconfigure from scratch
   - Rate experience (1-5)

### 7.3 UAT Success Criteria

- Average rating >= 4.0
- No S0/S1 bugs found
- 100% successful installations
- < 5 minutes average installation time
- < 3 support requests during UAT

## 8. Bug Tracking

### 8.1 Bug Report Template

```markdown
## Bug Report: [Title]

**Component:** Installer
**Platform:** [Windows/macOS/Ubuntu]
**Version:** [Installer version]
**Severity:** [S0/S1/S2/S3]

### Environment
- OS Version:
- Architecture:
- Antivirus:
- Previous VeloZ version (if upgrade):

### Steps to Reproduce
1.
2.
3.

### Expected Result


### Actual Result


### Screenshots/Logs
[Attach relevant files]

### Workaround
[If any]
```

### 8.2 Known Issues

| ID | Description | Severity | Status | Workaround |
|----|-------------|----------|--------|------------|
| INS-001 | Example issue | S2 | Open | Example workaround |
