# P0-1: One-click Installer System

**Version**: 1.0.0
**Date**: 2026-02-25
**Status**: Design Phase
**Complexity**: High
**Estimated Time**: 4-6 weeks

---

## 1. Overview

### 1.1 Goals

- Enable non-technical users to install VeloZ with a single click
- Provide native installers for Windows, macOS, and Linux
- Include all dependencies (C++ runtime, Python, Node.js)
- Support automatic updates with rollback capability
- Migrate user configuration across versions

### 1.2 Non-Goals

- Server/cloud deployment automation
- Docker/Kubernetes orchestration
- Enterprise fleet management

---

## 2. Architecture

### 2.1 High-Level Architecture

```
                        Installer System Architecture
                        =============================

+------------------+     +------------------+     +------------------+
|   Build Server   |     |  Update Server   |     |   User Machine   |
|   (CI/CD)        |     |  (CDN/S3)        |     |                  |
+------------------+     +------------------+     +------------------+
        |                        |                        |
        v                        v                        v
+------------------+     +------------------+     +------------------+
| Package Builder  |     | Update Manifest  |     | Installer        |
| - Windows MSI    |     | - Version info   |     | - Unpack         |
| - macOS DMG      |     | - Checksums      |     | - Configure      |
| - Linux DEB/RPM  |     | - Delta patches  |     | - Register       |
+------------------+     +------------------+     +------------------+
        |                        |                        |
        v                        v                        v
+------------------+     +------------------+     +------------------+
| Code Signing     |     | Release Channels |     | Auto-Updater     |
| - Windows        |     | - Stable         |     | - Check updates  |
| - macOS Notarize |     | - Beta           |     | - Download       |
| - Linux GPG      |     | - Nightly        |     | - Apply          |
+------------------+     +------------------+     +------------------+
```

### 2.2 Component Diagram

```
+------------------------------------------------------------------+
|                        VeloZ Installer                            |
+------------------------------------------------------------------+
|                                                                   |
|  +------------------+  +------------------+  +------------------+ |
|  | Platform         |  | Dependency       |  | Configuration    | |
|  | Detector         |  | Manager          |  | Migrator         | |
|  +------------------+  +------------------+  +------------------+ |
|           |                    |                    |             |
|           v                    v                    v             |
|  +------------------+  +------------------+  +------------------+ |
|  | Native Installer |  | Runtime Bundle   |  | Settings Store   | |
|  | - MSI/DMG/DEB    |  | - C++ Runtime    |  | - JSON Config    | |
|  | - Uninstaller    |  | - Python 3.11    |  | - Keychain       | |
|  +------------------+  +------------------+  +------------------+ |
|           |                    |                    |             |
|           v                    v                    v             |
|  +------------------+  +------------------+  +------------------+ |
|  | Shortcut Creator |  | Service Manager  |  | Update Checker   | |
|  | - Desktop        |  | - Auto-start     |  | - Background     | |
|  | - Start Menu     |  | - System tray    |  | - Notifications  | |
|  +------------------+  +------------------+  +------------------+ |
|                                                                   |
+------------------------------------------------------------------+
```

---

## 3. Technology Stack

### 3.1 Build Tools

| Platform | Tool | Justification |
|----------|------|---------------|
| **Cross-platform** | Electron Forge | Mature, well-documented, handles code signing |
| **Windows** | WiX Toolset | Industry standard MSI creation |
| **macOS** | create-dmg | Simple DMG creation with customization |
| **Linux** | electron-builder | DEB/RPM/AppImage support |

### 3.2 Auto-Update Framework

| Component | Technology | Justification |
|-----------|------------|---------------|
| **Update Server** | AWS S3 + CloudFront | Global CDN, cost-effective |
| **Update Protocol** | Squirrel (Windows/macOS) | Battle-tested, delta updates |
| **Linux Updates** | AppImageUpdate | Native Linux update mechanism |

### 3.3 Runtime Dependencies

| Dependency | Version | Size | Bundling Strategy |
|------------|---------|------|-------------------|
| C++ Runtime | MSVC 2022 / libc++ | ~5MB | Bundle with installer |
| Python | 3.11.x | ~40MB | Embedded distribution |
| Node.js | 20.x LTS | ~30MB | Bundled with Electron |

---

## 4. Package Structure

### 4.1 Windows Package

```
VeloZ-Setup-1.0.0.exe (NSIS/MSI)
|
+-- VeloZ/
    +-- veloz.exe              # Electron main process
    +-- resources/
    |   +-- app.asar           # React UI bundle
    |   +-- engine/
    |   |   +-- veloz_engine.exe
    |   |   +-- *.dll          # C++ runtime
    |   +-- gateway/
    |   |   +-- python/        # Embedded Python
    |   |   +-- gateway.py
    |   +-- config/
    |       +-- default.json
    +-- locales/               # i18n files
    +-- Uninstall.exe
```

### 4.2 macOS Package

```
VeloZ-1.0.0.dmg
|
+-- VeloZ.app/
    +-- Contents/
        +-- MacOS/
        |   +-- VeloZ          # Electron main
        +-- Resources/
        |   +-- app.asar
        |   +-- engine/
        |   |   +-- veloz_engine
        |   +-- gateway/
        |   |   +-- python/
        |   |   +-- gateway.py
        |   +-- icon.icns
        +-- Info.plist
        +-- embedded.provisionprofile
```

### 4.3 Linux Package

```
veloz_1.0.0_amd64.deb
|
+-- DEBIAN/
|   +-- control
|   +-- postinst
|   +-- prerm
+-- opt/veloz/
|   +-- veloz               # Electron main
|   +-- resources/
|   +-- chrome-sandbox
+-- usr/share/
    +-- applications/veloz.desktop
    +-- icons/veloz.png
```

---

## 5. Installation Flow

### 5.1 User Journey

```
Download Installer
       |
       v
+------------------+
| Platform Check   |
| - OS version     |
| - Architecture   |
| - Disk space     |
+------------------+
       |
       v
+------------------+
| License Agreement|
| - EULA display   |
| - Accept/Decline |
+------------------+
       |
       v
+------------------+
| Install Location |
| - Default path   |
| - Custom path    |
| - Space check    |
+------------------+
       |
       v
+------------------+
| Component Select |
| - Core (required)|
| - Sample strats  |
| - Documentation  |
+------------------+
       |
       v
+------------------+
| Installation     |
| - Extract files  |
| - Register       |
| - Create shortcuts|
+------------------+
       |
       v
+------------------+
| First Run Setup  |
| - Launch wizard  |
| - API key config |
+------------------+
```

### 5.2 Installation Sequence Diagram

```
User          Installer       FileSystem      Registry/plist    Gateway
  |               |               |                |               |
  |--Download---->|               |                |               |
  |               |               |                |               |
  |--Run--------->|               |                |               |
  |               |--Check OS---->|                |               |
  |               |<--OK----------|                |               |
  |               |               |                |               |
  |<--Show EULA---|               |                |               |
  |--Accept------>|               |                |               |
  |               |               |                |               |
  |               |--Extract----->|                |               |
  |               |--Register-----|--------------->|               |
  |               |--Shortcuts--->|                |               |
  |               |               |                |               |
  |               |--Start--------|----------------|-------------->|
  |               |               |                |               |
  |<--Complete----|               |                |               |
  |               |               |                |               |
```

---

## 6. Auto-Update System

### 6.1 Update Architecture

```
                        Auto-Update Flow
                        ================

+------------------+     +------------------+     +------------------+
| VeloZ App        |     | Update Server    |     | CDN (CloudFront) |
+------------------+     +------------------+     +------------------+
        |                        |                        |
        |--Check for updates---->|                        |
        |                        |                        |
        |<--Update manifest------|                        |
        |   (version, size,      |                        |
        |    checksum, notes)    |                        |
        |                        |                        |
        |--Download delta--------|----------------------->|
        |                        |                        |
        |<--Delta package--------|------------------------|
        |                        |                        |
        |--Verify checksum------>|                        |
        |                        |                        |
        |--Apply update (restart)|                        |
        |                        |                        |
```

### 6.2 Update Manifest Format

```json
{
  "version": "1.1.0",
  "releaseDate": "2026-03-15T00:00:00Z",
  "releaseNotes": "## What's New\n- Improved charting\n- Bug fixes",
  "mandatory": false,
  "platforms": {
    "win32-x64": {
      "url": "https://cdn.veloz.io/releases/1.1.0/VeloZ-1.1.0-win32-x64-delta.nupkg",
      "size": 15728640,
      "sha256": "abc123...",
      "fullUrl": "https://cdn.veloz.io/releases/1.1.0/VeloZ-Setup-1.1.0.exe",
      "fullSize": 157286400,
      "fullSha256": "def456..."
    },
    "darwin-x64": {
      "url": "https://cdn.veloz.io/releases/1.1.0/VeloZ-1.1.0-darwin-x64.zip",
      "size": 52428800,
      "sha256": "ghi789..."
    },
    "darwin-arm64": {
      "url": "https://cdn.veloz.io/releases/1.1.0/VeloZ-1.1.0-darwin-arm64.zip",
      "size": 48576512,
      "sha256": "jkl012..."
    },
    "linux-x64": {
      "url": "https://cdn.veloz.io/releases/1.1.0/veloz_1.1.0_amd64.deb",
      "size": 62914560,
      "sha256": "mno345..."
    }
  },
  "minVersion": "1.0.0",
  "channels": {
    "stable": true,
    "beta": true,
    "nightly": true
  }
}
```

### 6.3 Update States

```
                    Update State Machine
                    ====================

    +----------+     +----------+     +----------+
    | Idle     |---->| Checking |---->| Available|
    +----------+     +----------+     +----------+
         ^                |                |
         |                v                v
         |           +----------+     +----------+
         |           | No Update|     |Downloading|
         |           +----------+     +----------+
         |                |                |
         |                v                v
         |           +----------+     +----------+
         +-----------|  Done    |<----| Ready    |
                     +----------+     +----------+
                                           |
                                           v
                                      +----------+
                                      | Applying |
                                      +----------+
                                           |
                                           v
                                      +----------+
                                      | Restart  |
                                      +----------+
```

---

## 7. Configuration Migration

### 7.1 Migration Strategy

```
Version N Config
       |
       v
+------------------+
| Backup Current   |
| - config.json    |
| - strategies/    |
| - logs/          |
+------------------+
       |
       v
+------------------+
| Schema Migration |
| - Add new fields |
| - Remove deprecated|
| - Transform values|
+------------------+
       |
       v
+------------------+
| Validate         |
| - Type checking  |
| - Range checking |
| - Dependency check|
+------------------+
       |
       v
+------------------+
| Apply or Rollback|
| - Success: apply |
| - Failure: restore|
+------------------+
```

### 7.2 Migration Script Interface

```typescript
// migrations/1.0.0-to-1.1.0.ts
interface ConfigMigration {
  fromVersion: string;
  toVersion: string;

  migrate(config: OldConfig): NewConfig;
  rollback(config: NewConfig): OldConfig;
  validate(config: NewConfig): ValidationResult;
}

export const migration_1_0_0_to_1_1_0: ConfigMigration = {
  fromVersion: '1.0.0',
  toVersion: '1.1.0',

  migrate(config) {
    return {
      ...config,
      // Add new fields
      charting: {
        defaultTimeframe: '1h',
        indicators: [],
      },
      // Transform existing fields
      risk: {
        ...config.risk,
        dailyLossLimit: config.risk.maxLoss ?? 0.1,
      },
    };
  },

  rollback(config) {
    const { charting, ...rest } = config;
    return rest;
  },

  validate(config) {
    const errors: string[] = [];
    if (config.charting.defaultTimeframe === undefined) {
      errors.push('Missing charting.defaultTimeframe');
    }
    return { valid: errors.length === 0, errors };
  },
};
```

---

## 8. Platform-Specific Considerations

### 8.1 Windows

| Aspect | Implementation |
|--------|----------------|
| **Code Signing** | EV Certificate from DigiCert |
| **SmartScreen** | Reputation building via signing |
| **Registry** | HKLM/Software/VeloZ for settings |
| **Firewall** | Prompt for network access |
| **Auto-start** | Registry Run key or Task Scheduler |

### 8.2 macOS

| Aspect | Implementation |
|--------|----------------|
| **Code Signing** | Apple Developer ID |
| **Notarization** | Required for Gatekeeper |
| **Keychain** | Store API keys securely |
| **Sandbox** | App Sandbox entitlements |
| **Auto-start** | LaunchAgent plist |

### 8.3 Linux

| Aspect | Implementation |
|--------|----------------|
| **Package Format** | DEB (Ubuntu/Debian), RPM (Fedora/RHEL), AppImage |
| **Dependencies** | libsecret (keyring), libnotify |
| **Desktop Entry** | XDG .desktop file |
| **Auto-start** | XDG autostart or systemd user service |
| **Permissions** | Polkit for privileged operations |

---

## 9. Security Considerations

### 9.1 Code Signing Chain

```
                    Code Signing Trust Chain
                    ========================

+------------------+     +------------------+     +------------------+
| Root CA          |     | Intermediate CA  |     | Code Signing Cert|
| (DigiCert/Apple) |---->| (DigiCert/Apple) |---->| (VeloZ)          |
+------------------+     +------------------+     +------------------+
                                                         |
                                                         v
                                                  +------------------+
                                                  | Signed Installer |
                                                  | - Timestamp      |
                                                  | - Signature      |
                                                  +------------------+
```

### 9.2 Update Security

| Threat | Mitigation |
|--------|------------|
| MITM attacks | HTTPS + certificate pinning |
| Tampered updates | SHA-256 checksum verification |
| Rollback attacks | Minimum version enforcement |
| Compromised CDN | Code signing verification |

### 9.3 Secure Storage

```
                    Credential Storage
                    ==================

Windows:
+------------------+
| Windows DPAPI    |
| - User-scoped    |
| - Machine-scoped |
+------------------+

macOS:
+------------------+
| Keychain Services|
| - Login keychain |
| - Access control |
+------------------+

Linux:
+------------------+
| Secret Service   |
| - GNOME Keyring  |
| - KWallet        |
+------------------+
```

---

## 10. Build Pipeline

### 10.1 CI/CD Architecture

```
                        Build Pipeline
                        ==============

+------------------+     +------------------+     +------------------+
| Source Code      |     | Build Matrix     |     | Artifact Store   |
| (GitHub)         |---->| (GitHub Actions) |---->| (S3/GCS)         |
+------------------+     +------------------+     +------------------+
                                |
                                v
                    +------------------------+
                    |    Build Jobs          |
                    +------------------------+
                    | - windows-x64          |
                    | - macos-x64            |
                    | - macos-arm64          |
                    | - linux-x64            |
                    +------------------------+
                                |
                                v
                    +------------------------+
                    |    Post-Build          |
                    +------------------------+
                    | - Code signing         |
                    | - Notarization         |
                    | - Checksum generation  |
                    | - Manifest update      |
                    +------------------------+
                                |
                                v
                    +------------------------+
                    |    Release             |
                    +------------------------+
                    | - CDN upload           |
                    | - GitHub Release       |
                    | - Update manifest      |
                    +------------------------+
```

### 10.2 GitHub Actions Workflow

```yaml
# .github/workflows/release.yml
name: Release Build

on:
  push:
    tags:
      - 'v*'

jobs:
  build-windows:
    runs-on: windows-latest
    steps:
      - uses: actions/checkout@v4
      - name: Setup Node.js
        uses: actions/setup-node@v4
        with:
          node-version: '20'
      - name: Build C++ Engine
        run: |
          cmake --preset release
          cmake --build --preset release-all
      - name: Build Electron App
        run: |
          cd apps/ui
          npm ci
          npm run build
          npm run package:win
      - name: Sign Installer
        env:
          WINDOWS_CERT: ${{ secrets.WINDOWS_CERT }}
          WINDOWS_CERT_PASSWORD: ${{ secrets.WINDOWS_CERT_PASSWORD }}
        run: |
          # Sign with signtool
      - name: Upload Artifact
        uses: actions/upload-artifact@v4
        with:
          name: windows-installer
          path: apps/ui/dist/*.exe

  build-macos:
    runs-on: macos-latest
    steps:
      - uses: actions/checkout@v4
      - name: Build and Sign
        env:
          APPLE_ID: ${{ secrets.APPLE_ID }}
          APPLE_ID_PASSWORD: ${{ secrets.APPLE_ID_PASSWORD }}
          APPLE_TEAM_ID: ${{ secrets.APPLE_TEAM_ID }}
        run: |
          # Build, sign, and notarize
      - name: Upload Artifact
        uses: actions/upload-artifact@v4
        with:
          name: macos-installer
          path: apps/ui/dist/*.dmg

  build-linux:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - name: Build Packages
        run: |
          # Build DEB and RPM
      - name: Upload Artifacts
        uses: actions/upload-artifact@v4
        with:
          name: linux-installers
          path: |
            apps/ui/dist/*.deb
            apps/ui/dist/*.rpm
            apps/ui/dist/*.AppImage

  publish:
    needs: [build-windows, build-macos, build-linux]
    runs-on: ubuntu-latest
    steps:
      - name: Download Artifacts
        uses: actions/download-artifact@v4
      - name: Generate Checksums
        run: |
          sha256sum */* > checksums.txt
      - name: Update Manifest
        run: |
          # Generate update manifest JSON
      - name: Upload to CDN
        run: |
          aws s3 sync ./artifacts s3://veloz-releases/${{ github.ref_name }}/
      - name: Create GitHub Release
        uses: softprops/action-gh-release@v1
        with:
          files: |
            windows-installer/*
            macos-installer/*
            linux-installers/*
            checksums.txt
```

---

## 11. API Contracts

### 11.1 Update Check API

```
GET /api/v1/updates/check?version={current}&platform={platform}&channel={channel}

Response:
{
  "updateAvailable": true,
  "version": "1.1.0",
  "releaseDate": "2026-03-15T00:00:00Z",
  "releaseNotes": "...",
  "mandatory": false,
  "downloadUrl": "https://cdn.veloz.io/...",
  "size": 15728640,
  "sha256": "abc123..."
}
```

### 11.2 Telemetry API (Opt-in)

```
POST /api/v1/telemetry/install

Body:
{
  "installId": "uuid",
  "version": "1.0.0",
  "platform": "win32-x64",
  "osVersion": "10.0.19045",
  "success": true,
  "duration": 45000,
  "components": ["core", "samples"]
}
```

---

## 12. Implementation Plan

### 12.1 Phase Breakdown

| Phase | Duration | Deliverables |
|-------|----------|--------------|
| **Phase 1: Setup** | 1 week | Electron project, build scripts |
| **Phase 2: Windows** | 1.5 weeks | MSI installer, code signing |
| **Phase 3: macOS** | 1.5 weeks | DMG, notarization |
| **Phase 4: Linux** | 1 week | DEB, RPM, AppImage |
| **Phase 5: Auto-update** | 1 week | Update server, client |
| **Phase 6: Testing** | 1 week | Cross-platform QA |

### 12.2 Dependencies

```
                    Implementation Dependencies
                    ===========================

+------------------+
| Electron Setup   |
+------------------+
        |
        +------------------+------------------+
        |                  |                  |
        v                  v                  v
+------------------+ +------------------+ +------------------+
| Windows Build    | | macOS Build      | | Linux Build      |
+------------------+ +------------------+ +------------------+
        |                  |                  |
        +------------------+------------------+
                           |
                           v
                    +------------------+
                    | Auto-Update      |
                    +------------------+
                           |
                           v
                    +------------------+
                    | Integration Test |
                    +------------------+
```

---

## 13. Testing Strategy

### 13.1 Test Matrix

| Test Type | Windows | macOS Intel | macOS ARM | Linux |
|-----------|---------|-------------|-----------|-------|
| Fresh install | Yes | Yes | Yes | Yes |
| Upgrade install | Yes | Yes | Yes | Yes |
| Uninstall | Yes | Yes | Yes | Yes |
| Auto-update | Yes | Yes | Yes | Yes |
| Config migration | Yes | Yes | Yes | Yes |

### 13.2 Automated Tests

```typescript
// tests/installer.spec.ts
describe('Installer', () => {
  test('installs to default location', async () => {
    const result = await runInstaller({ silent: true });
    expect(result.exitCode).toBe(0);
    expect(fs.existsSync(getDefaultInstallPath())).toBe(true);
  });

  test('creates desktop shortcut', async () => {
    await runInstaller({ silent: true });
    expect(fs.existsSync(getDesktopShortcutPath())).toBe(true);
  });

  test('registers uninstaller', async () => {
    await runInstaller({ silent: true });
    expect(isUninstallerRegistered()).toBe(true);
  });

  test('migrates config from previous version', async () => {
    await installVersion('1.0.0');
    await writeConfig({ oldField: 'value' });
    await upgradeToVersion('1.1.0');
    const config = await readConfig();
    expect(config.newField).toBeDefined();
  });
});
```

---

## 14. Rollback Strategy

### 14.1 Rollback Triggers

| Trigger | Action |
|---------|--------|
| Installation failure | Restore previous version |
| Config migration failure | Restore config backup |
| Update verification failure | Keep current version |
| User-initiated rollback | Restore from backup |

### 14.2 Backup Structure

```
~/.veloz/
+-- backups/
    +-- 1.0.0/
    |   +-- config.json
    |   +-- strategies/
    |   +-- timestamp.txt
    +-- 1.1.0/
        +-- config.json
        +-- strategies/
        +-- timestamp.txt
```

---

## 15. Success Metrics

| Metric | Target | Measurement |
|--------|--------|-------------|
| Installation success rate | > 95% | Telemetry |
| Average install time | < 2 min | Telemetry |
| Update success rate | > 99% | Telemetry |
| Support tickets (install) | < 5% of installs | Support system |
| Uninstall completion | > 99% | Telemetry |

---

## 16. Related Documentation

- [GUI Configuration System](02-gui-configuration.md)
- [Security Education System](05-security-education.md)
- [Build and Run Guide](../../guides/build_and_run.md)
