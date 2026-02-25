# VeloZ Installer

Cross-platform installer for the VeloZ Trading Platform, built with Electron Forge.

## Overview

The VeloZ installer packages the complete trading platform into native installers for Windows, macOS, and Linux. It includes:

- **C++ Trading Engine** - High-performance order execution
- **Python Gateway** - HTTP/WebSocket API server
- **React UI** - Modern web-based trading interface
- **Auto-Updater** - Seamless background updates

## Supported Platforms

| Platform | Format | Architecture |
|----------|--------|--------------|
| Windows 10/11 | .exe (Squirrel) | x64 |
| macOS 12+ | .dmg | x64, arm64 |
| Ubuntu 22.04+ | .deb | x64 |
| Fedora 38+ | .rpm | x64 |

## Development

### Prerequisites

- Node.js 20+
- npm 10+
- Platform-specific build tools:
  - **Windows**: Visual Studio Build Tools
  - **macOS**: Xcode Command Line Tools
  - **Linux**: dpkg, fakeroot, rpm

### Setup

```bash
cd apps/installer
npm install
```

### Build Resources

Before building the installer, you need to build and copy the resources:

```bash
# From project root
./scripts/build_installer.sh dev
```

Or manually:

```bash
# Build engine
cmake --preset dev
cmake --build --preset dev-engine -j

# Build UI
cd apps/ui
npm ci
npm run build

# Copy resources
cp -r build/dev/apps/engine/veloz_engine apps/installer/resources/engine/
cp -r apps/ui/dist/* apps/installer/resources/ui/
cp apps/gateway/*.py apps/installer/resources/gateway/
```

### Development Mode

Run the installer in development mode:

```bash
npm start
```

### Build Installers

```bash
# Build for current platform
npm run make

# Build for specific platform
npm run make:win    # Windows
npm run make:mac    # macOS
npm run make:linux  # Linux
```

Output will be in `out/make/`.

## Project Structure

```
apps/installer/
├── src/
│   ├── main.js           # Electron main process
│   ├── preload.js        # Secure IPC bridge
│   ├── updater.js        # Auto-update module
│   ├── migrations/       # Config migration scripts
│   └── types/            # TypeScript definitions
├── resources/
│   ├── engine/           # C++ engine binary
│   ├── gateway/          # Python gateway
│   ├── ui/               # Built React UI
│   └── config/           # Default configuration
├── assets/
│   └── icons/            # Application icons
├── forge.config.js       # Electron Forge config
├── entitlements.plist    # macOS entitlements
└── package.json
```

## Configuration

### Environment Variables

| Variable | Description |
|----------|-------------|
| `APPLE_ID` | Apple ID for notarization |
| `APPLE_ID_PASSWORD` | App-specific password |
| `APPLE_TEAM_ID` | Apple Developer Team ID |
| `APPLE_IDENTITY` | Code signing identity |
| `WINDOWS_CERT_FILE` | Path to Windows certificate |
| `WINDOWS_CERT_PASSWORD` | Certificate password |

### Code Signing

#### macOS

1. Obtain an Apple Developer ID certificate
2. Set environment variables:
   ```bash
   export APPLE_ID="your@email.com"
   export APPLE_ID_PASSWORD="app-specific-password"
   export APPLE_TEAM_ID="XXXXXXXXXX"
   export APPLE_IDENTITY="Developer ID Application: Your Name (XXXXXXXXXX)"
   ```
3. Build with signing enabled

#### Windows

1. Obtain an EV code signing certificate
2. Set environment variables:
   ```bash
   export WINDOWS_CERT_FILE="/path/to/certificate.pfx"
   export WINDOWS_CERT_PASSWORD="password"
   ```
3. Build with signing enabled

## Auto-Updates

The installer includes an auto-update system using `electron-updater`.

### Update Channels

- **stable** - Production releases
- **beta** - Pre-release testing
- **nightly** - Development builds

### Update Server

Updates are served from GitHub Releases by default. Configure a custom update server in `forge.config.js`:

```javascript
autoUpdater.setFeedURL({
  provider: 'generic',
  url: 'https://your-update-server.com/releases'
});
```

## Testing

### Automated Tests

```bash
# Windows (PowerShell)
.\tests\installer\windows_install_test.ps1 -InstallerPath ".\VeloZ-Setup.exe"

# macOS
./tests/installer/macos_install_test.sh VeloZ.dmg

# Ubuntu
sudo ./tests/installer/ubuntu_install_test.sh veloz_1.0.0_amd64.deb
```

### Manual Testing Checklist

- [ ] Fresh installation
- [ ] Upgrade from previous version
- [ ] Uninstall and reinstall
- [ ] Auto-update flow
- [ ] Configuration migration
- [ ] Code signing verification

## Troubleshooting

### macOS: "App is damaged"

The app may not be properly notarized. Check with:
```bash
spctl -a -v /Applications/VeloZ.app
```

### Windows: SmartScreen Warning

New certificates need reputation. Submit to Microsoft for review or wait for organic reputation building.

### Linux: Missing Dependencies

Install required libraries:
```bash
sudo apt install libsecret-1-0 libnotify4
```

## License

MIT License - see [LICENSE](../../LICENSE) for details.
