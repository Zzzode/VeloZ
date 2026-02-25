# PRD: One-Click Installer

## Document Information

| Field | Value |
|-------|-------|
| Feature | One-Click Installer |
| Priority | P0 |
| Status | Draft |
| Owner | Product Manager |
| Target Release | v2.0 |
| Last Updated | 2026-02-25 |

## Executive Summary

Transform VeloZ installation from a developer-focused CLI process (git clone, cmake, environment setup) into a native desktop installer that non-technical users can complete in under 5 minutes without any command-line interaction.

## Problem Statement

### Current Installation Process

```
Current User Journey (30-60 minutes, technical users only):

1. Install prerequisites (CMake, Clang, OpenSSL, Python)
2. Clone repository: git clone https://github.com/Zzzode/VeloZ.git
3. Build: cmake --preset dev && cmake --build --preset dev-all
4. Configure: export VELOZ_* environment variables
5. Run: ./scripts/run_gateway.sh dev
6. Troubleshoot build errors (common)
```

### User Pain Points

| Pain Point | Impact | Frequency |
|------------|--------|-----------|
| Prerequisite installation | Blocks 40% of attempts | Every install |
| Build failures | Blocks 30% of attempts | Common |
| Environment variable confusion | Causes misconfiguration | Every install |
| No visual feedback | User uncertainty | Every install |
| No upgrade path | Manual reinstall required | Every update |

### Target User Profile

**Persona: Alex, Personal Trader**
- Age: 35, trades crypto part-time
- Technical skills: Can install desktop apps, cannot use terminal
- Goal: Run automated trading strategies without coding
- Frustration: "I gave up after the cmake step failed"

## Goals and Success Metrics

### Goals

1. **Accessibility**: Any user who can install a desktop app can install VeloZ
2. **Speed**: Installation completes in < 5 minutes on standard hardware
3. **Reliability**: > 95% installation success rate across supported platforms
4. **Maintainability**: Automatic updates without user intervention

### Success Metrics

| Metric | Current | Target | Measurement |
|--------|---------|--------|-------------|
| Installation success rate | ~60% | > 95% | Installer telemetry |
| Time to complete install | 30-60 min | < 5 min | Installer telemetry |
| Support tickets (install) | High | < 5% of installs | Support system |
| Update adoption rate | Manual | > 80% within 7 days | Update telemetry |

## User Stories

### US-1: Download and Install (Windows)

**As a** personal trader on Windows
**I want to** download and install VeloZ with a single executable
**So that** I can start trading without technical setup

**Acceptance Criteria:**
- [ ] Single .exe download from website
- [ ] Windows SmartScreen shows verified publisher
- [ ] Installation wizard with < 5 screens
- [ ] No command-line interaction required
- [ ] Desktop shortcut created automatically
- [ ] Start menu entry created
- [ ] Installation completes in < 5 minutes

### US-2: Download and Install (macOS)

**As a** personal trader on macOS
**I want to** install VeloZ from a .dmg file
**So that** I can use familiar macOS installation patterns

**Acceptance Criteria:**
- [ ] Single .dmg download from website
- [ ] Drag-to-Applications installation
- [ ] Gatekeeper shows verified developer
- [ ] First launch prompts for permissions (if needed)
- [ ] Application appears in Launchpad
- [ ] Installation completes in < 3 minutes

### US-3: Download and Install (Linux)

**As a** personal trader on Linux
**I want to** install VeloZ using my distribution's package format
**So that** I can manage it with my system's package manager

**Acceptance Criteria:**
- [ ] .deb package for Debian/Ubuntu
- [ ] .rpm package for Fedora/RHEL
- [ ] AppImage for universal compatibility
- [ ] Desktop entry created
- [ ] Installation via GUI software center supported
- [ ] Installation completes in < 5 minutes

### US-4: Automatic Updates

**As a** VeloZ user
**I want to** receive automatic updates
**So that** I always have the latest features and security fixes

**Acceptance Criteria:**
- [ ] Background update check on application start
- [ ] Non-intrusive update notification
- [ ] One-click update installation
- [ ] Automatic restart after update
- [ ] Rollback option if update fails
- [ ] Update history viewable in settings

### US-5: Uninstall

**As a** VeloZ user
**I want to** completely uninstall VeloZ
**So that** no files remain on my system

**Acceptance Criteria:**
- [ ] Standard OS uninstall mechanism (Add/Remove Programs, drag to Trash)
- [ ] Option to keep or remove user data
- [ ] All application files removed
- [ ] Registry entries cleaned (Windows)
- [ ] Confirmation before data deletion

### US-6: Installation Recovery

**As a** user whose installation failed
**I want to** recover and retry installation
**So that** I don't have to manually clean up

**Acceptance Criteria:**
- [ ] Clear error message explaining failure
- [ ] "Retry" button attempts installation again
- [ ] "Clean and Retry" removes partial install
- [ ] Support link for unresolved issues
- [ ] Error details copyable for support tickets

## User Flows

### Flow 1: Windows Installation

```
+------------------+     +------------------+     +------------------+
|   Download       |     |   Run Installer  |     |   Welcome        |
|   VeloZ.exe      | --> |   (UAC prompt)   | --> |   Screen         |
+------------------+     +------------------+     +--------+---------+
                                                          |
                                                          v
+------------------+     +------------------+     +------------------+
|   Installation   |     |   Select         |     |   License        |
|   Complete       | <-- |   Location       | <-- |   Agreement      |
+--------+---------+     +------------------+     +------------------+
         |
         v
+------------------+     +------------------+
|   Launch VeloZ   | --> |   First-Run      |
|   (checkbox)     |     |   Setup Wizard   |
+------------------+     +------------------+
```

### Flow 2: macOS Installation

```
+------------------+     +------------------+     +------------------+
|   Download       |     |   Open DMG       |     |   Drag to        |
|   VeloZ.dmg      | --> |   (auto-mount)   | --> |   Applications   |
+------------------+     +------------------+     +--------+---------+
                                                          |
                                                          v
+------------------+     +------------------+     +------------------+
|   First-Run      |     |   Gatekeeper     |     |   Launch from    |
|   Setup Wizard   | <-- |   Approval       | <-- |   Launchpad      |
+------------------+     +------------------+     +------------------+
```

### Flow 3: Automatic Update

```
+------------------+     +------------------+     +------------------+
|   App Start      |     |   Check for      |     |   Update         |
|                  | --> |   Updates        | --> |   Available?     |
+------------------+     +------------------+     +--------+---------+
                                                          |
                              +---------------------------+
                              |                           |
                              v                           v
                    +------------------+        +------------------+
                    |   No: Continue   |        |   Yes: Show      |
                    |   Normal Start   |        |   Notification   |
                    +------------------+        +--------+---------+
                                                         |
                                                         v
                    +------------------+        +------------------+
                    |   Update         |        |   User Clicks    |
                    |   Complete       | <----- |   "Update Now"   |
                    +--------+---------+        +------------------+
                             |
                             v
                    +------------------+
                    |   Restart App    |
                    +------------------+
```

## Wireframes

### Windows Installer - Welcome Screen

```
+------------------------------------------------------------------+
|                                                          [X]      |
|                                                                   |
|                         [VeloZ Logo]                              |
|                                                                   |
|                    Welcome to VeloZ Setup                         |
|                                                                   |
|    VeloZ is a quantitative trading platform for crypto markets.   |
|                                                                   |
|    This wizard will guide you through the installation.           |
|                                                                   |
|    Click Next to continue, or Cancel to exit Setup.               |
|                                                                   |
|                                                                   |
|                                                                   |
|                                        [Cancel]  [Next >]         |
+------------------------------------------------------------------+
```

### Windows Installer - Installation Location

```
+------------------------------------------------------------------+
|                                                          [X]      |
|                                                                   |
|    Choose Install Location                                        |
|    --------------------------------------------------------       |
|                                                                   |
|    VeloZ will be installed to the following folder:               |
|                                                                   |
|    +--------------------------------------------------+           |
|    | C:\Program Files\VeloZ                           | [Browse]  |
|    +--------------------------------------------------+           |
|                                                                   |
|    Space required: 250 MB                                         |
|    Space available: 120 GB                                        |
|                                                                   |
|    [ ] Create desktop shortcut                                    |
|    [x] Create Start menu entry                                    |
|    [x] Launch VeloZ after installation                            |
|                                                                   |
|                                        [< Back]  [Install]        |
+------------------------------------------------------------------+
```

### Windows Installer - Progress

```
+------------------------------------------------------------------+
|                                                          [X]      |
|                                                                   |
|    Installing VeloZ                                               |
|    --------------------------------------------------------       |
|                                                                   |
|    Please wait while VeloZ is being installed...                  |
|                                                                   |
|    [========================================          ] 75%       |
|                                                                   |
|    Installing trading engine...                                   |
|                                                                   |
|                                                                   |
|                                                                   |
|                                                                   |
|                                                                   |
|                                                                   |
|                                                    [Cancel]       |
+------------------------------------------------------------------+
```

### macOS DMG Window

```
+------------------------------------------------------------------+
|  VeloZ Installer                                                  |
+------------------------------------------------------------------+
|                                                                   |
|                                                                   |
|         +-------------+                  +-------------+          |
|         |             |                  |             |          |
|         |   [VeloZ    |      ---->       | Applications|          |
|         |    Icon]    |                  |    Folder   |          |
|         |             |                  |             |          |
|         +-------------+                  +-------------+          |
|             VeloZ                         Applications            |
|                                                                   |
|         Drag VeloZ to Applications to install                     |
|                                                                   |
+------------------------------------------------------------------+
```

### Update Notification (In-App)

```
+------------------------------------------------------------------+
|  [i] Update Available                                    [X]      |
+------------------------------------------------------------------+
|                                                                   |
|  VeloZ v2.1.0 is available (you have v2.0.0)                     |
|                                                                   |
|  What's new:                                                      |
|  - Improved chart performance                                     |
|  - New RSI indicator                                              |
|  - Bug fixes and stability improvements                           |
|                                                                   |
|  [View full changelog]                                            |
|                                                                   |
|                          [Remind me later]  [Update Now]          |
+------------------------------------------------------------------+
```

## Technical Requirements

### Platform Support

| Platform | Minimum Version | Package Format |
|----------|-----------------|----------------|
| Windows | Windows 10 (1903+) | .exe (NSIS/WiX) |
| macOS | macOS 12 (Monterey) | .dmg (signed) |
| Ubuntu | 22.04 LTS | .deb |
| Fedora | 38+ | .rpm |
| Universal Linux | - | AppImage |

### Bundled Components

| Component | Size | Purpose |
|-----------|------|---------|
| VeloZ Engine | ~50 MB | Core trading engine (pre-built) |
| Python Runtime | ~80 MB | Gateway runtime |
| UI Assets | ~20 MB | Web UI files |
| Dependencies | ~100 MB | OpenSSL, KJ libraries |
| **Total** | **~250 MB** | |

### Code Signing

| Platform | Certificate Type | Provider |
|----------|------------------|----------|
| Windows | EV Code Signing | DigiCert/Sectigo |
| macOS | Developer ID | Apple |
| Linux | GPG Signing | Self-managed |

### Update Mechanism

| Component | Technology | Behavior |
|-----------|------------|----------|
| Update check | HTTPS REST API | On app start, daily |
| Download | HTTPS with resume | Background |
| Verification | SHA-256 + signature | Before install |
| Installation | Platform-native | Requires restart |
| Rollback | Previous version backup | On failure |

## Error Handling

### Installation Errors

| Error | User Message | Recovery Action |
|-------|--------------|-----------------|
| Insufficient disk space | "VeloZ needs 250 MB of free space. Please free up space and try again." | Show disk cleanup link |
| Missing permissions | "Administrator access required. Please run as administrator." | Restart with elevation |
| Antivirus block | "Your antivirus may be blocking installation. Please add an exception for VeloZ." | Show AV instructions |
| Network failure | "Could not download required files. Please check your internet connection." | Retry button |
| Corrupted download | "Download verification failed. Please download again." | Re-download link |

### Update Errors

| Error | User Message | Recovery Action |
|-------|--------------|-----------------|
| Update download failed | "Could not download update. Will retry later." | Silent retry |
| Update install failed | "Update could not be installed. VeloZ will continue with current version." | Continue with old version |
| Rollback triggered | "Update was rolled back due to an error. Please report this issue." | Support link |

## Non-Functional Requirements

### Performance

| Requirement | Target |
|-------------|--------|
| Installation time | < 5 minutes (100 Mbps connection) |
| Download size | < 100 MB (compressed) |
| Disk footprint | < 500 MB (installed) |
| Memory during install | < 500 MB |
| Update check latency | < 2 seconds |

### Security

| Requirement | Implementation |
|-------------|----------------|
| Download integrity | SHA-256 checksum verification |
| Code authenticity | Platform code signing |
| Update security | HTTPS + signature verification |
| Privilege escalation | Only when necessary, with user consent |

### Accessibility

| Requirement | Implementation |
|-------------|----------------|
| Screen reader support | ARIA labels on all controls |
| Keyboard navigation | Full keyboard accessibility |
| High contrast | Respect system settings |
| Font scaling | Support system font size |

## Sprint Planning

### Sprint 1-2: Windows Installer (2 weeks)

| Task | Estimate | Owner |
|------|----------|-------|
| Set up NSIS/WiX build pipeline | 2 days | Backend |
| Create installer UI screens | 3 days | Frontend |
| Bundle pre-built engine | 2 days | Backend |
| Code signing setup | 1 day | DevOps |
| Testing on Windows 10/11 | 2 days | QA |

**Deliverable:** Working Windows installer

### Sprint 3-4: macOS Installer (2 weeks)

| Task | Estimate | Owner |
|------|----------|-------|
| Create DMG with background | 1 day | Frontend |
| Apple Developer ID signing | 2 days | DevOps |
| Notarization pipeline | 2 days | DevOps |
| Universal binary (Intel + ARM) | 3 days | Backend |
| Testing on macOS 12/13/14 | 2 days | QA |

**Deliverable:** Working macOS installer

### Sprint 5-6: Linux Packages (2 weeks)

| Task | Estimate | Owner |
|------|----------|-------|
| Create .deb package | 2 days | Backend |
| Create .rpm package | 2 days | Backend |
| Create AppImage | 2 days | Backend |
| GPG signing setup | 1 day | DevOps |
| Testing on Ubuntu/Fedora | 3 days | QA |

**Deliverable:** Working Linux packages

### Sprint 7-8: Auto-Update System (2 weeks)

| Task | Estimate | Owner |
|------|----------|-------|
| Update server API | 3 days | Backend |
| Update client (all platforms) | 4 days | Backend |
| Update UI notifications | 2 days | Frontend |
| Rollback mechanism | 2 days | Backend |
| End-to-end testing | 3 days | QA |

**Deliverable:** Working auto-update system

## Dependencies

### External Dependencies

| Dependency | Type | Risk |
|------------|------|------|
| Code signing certificates | Procurement | Medium (lead time) |
| Apple Developer account | Procurement | Low |
| Update server infrastructure | Infrastructure | Low |

### Internal Dependencies

| Dependency | Feature | Impact |
|------------|---------|--------|
| Pre-built engine binaries | CI/CD | Blocking |
| GUI Configuration | First-run wizard | Blocking |

## Risks and Mitigations

| Risk | Probability | Impact | Mitigation |
|------|-------------|--------|------------|
| Code signing delays | Medium | High | Start procurement early |
| Antivirus false positives | Medium | Medium | Submit to AV vendors |
| macOS notarization issues | Low | High | Test early, document workarounds |
| Update server downtime | Low | Medium | CDN with fallback |

## Success Criteria

### MVP (Month 2)

- [ ] Windows installer with code signing
- [ ] macOS installer with notarization
- [ ] Installation success rate > 90%

### Full Release (Month 4)

- [ ] Linux packages (deb, rpm, AppImage)
- [ ] Auto-update system
- [ ] Installation success rate > 95%
- [ ] Update adoption > 80% within 7 days

## Appendix

### A. Installer Technology Comparison

| Technology | Windows | macOS | Linux | Pros | Cons |
|------------|---------|-------|-------|------|------|
| Electron Builder | Yes | Yes | Yes | Cross-platform | Large size |
| NSIS | Yes | No | No | Small, flexible | Windows only |
| WiX | Yes | No | No | MSI support | Complex |
| pkgbuild | No | Yes | No | Native | macOS only |
| CPack | Yes | Yes | Yes | CMake integration | Basic UI |

**Recommendation:** Use platform-native tools (NSIS/WiX for Windows, pkgbuild for macOS, CPack for Linux) for best user experience.

### B. Update Server API

```
GET /api/updates/check
Query: platform=windows&version=2.0.0&channel=stable

Response:
{
  "update_available": true,
  "version": "2.1.0",
  "release_date": "2026-03-15",
  "download_url": "https://releases.veloz.io/v2.1.0/VeloZ-2.1.0-win64.exe",
  "checksum": "sha256:abc123...",
  "signature": "...",
  "changelog": "...",
  "required": false
}
```

---

*Document Version: 1.0*
*Last Updated: 2026-02-25*
