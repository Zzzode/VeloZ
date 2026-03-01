const { FusesPlugin } = require('@electron-forge/plugin-fuses');
const { FuseV1Options, FuseVersion } = require('@electron/fuses');
const path = require('path');

module.exports = {
  packagerConfig: {
    name: 'VeloZ',
    executableName: 'veloz',
    appBundleId: 'io.veloz.app',
    appCategoryType: 'public.app-category.finance',
    icon: path.join(__dirname, 'assets', 'icons', 'icon'),
    asar: true,
    extraResource: [
      path.join(__dirname, 'resources', 'engine'),
      path.join(__dirname, 'resources', 'gateway'),
      path.join(__dirname, 'resources', 'ui'),
      path.join(__dirname, 'resources', 'config'),
    ],
    // macOS code signing
    osxSign: {
      identity: process.env.APPLE_IDENTITY || '-',
      'hardened-runtime': true,
      entitlements: path.join(__dirname, 'entitlements.plist'),
      'entitlements-inherit': path.join(__dirname, 'entitlements.plist'),
      'signature-flags': 'library',
    },
    osxNotarize: process.env.APPLE_ID ? {
      appleId: process.env.APPLE_ID,
      appleIdPassword: process.env.APPLE_ID_PASSWORD,
      teamId: process.env.APPLE_TEAM_ID,
    } : undefined,
    // Windows code signing
    win32metadata: {
      CompanyName: 'VeloZ',
      ProductName: 'VeloZ Trading Platform',
      FileDescription: 'Professional Crypto Trading Platform',
      OriginalFilename: 'veloz.exe',
    },
  },
  rebuildConfig: {},
  makers: [
    // Windows - Squirrel installer
    {
      name: '@electron-forge/maker-squirrel',
      config: {
        name: 'VeloZ',
        authors: 'VeloZ Team',
        description: 'Professional Crypto Trading Platform',
        iconUrl: 'https://raw.githubusercontent.com/Zzzode/VeloZ/master/apps/installer/assets/icons/icon.ico',
        setupIcon: path.join(__dirname, 'assets', 'icons', 'icon.ico'),
        loadingGif: path.join(__dirname, 'assets', 'installer-loading.gif'),
        // Code signing for Windows
        certificateFile: process.env.WINDOWS_CERT_FILE,
        certificatePassword: process.env.WINDOWS_CERT_PASSWORD,
      },
    },
    // macOS - DMG
    {
      name: '@electron-forge/maker-dmg',
      config: {
        name: 'VeloZ',
        icon: path.join(__dirname, 'assets', 'icons', 'icon.icns'),
        background: path.join(__dirname, 'assets', 'dmg-background.png'),
        format: 'ULFO',
        contents: [
          { x: 130, y: 220, type: 'file', path: path.join(__dirname, 'out', 'VeloZ-darwin-x64', 'VeloZ.app') },
          { x: 410, y: 220, type: 'link', path: '/Applications' },
        ],
        window: {
          width: 540,
          height: 380,
        },
      },
    },
    // macOS - ZIP (for auto-update)
    {
      name: '@electron-forge/maker-zip',
      platforms: ['darwin'],
    },
    // Linux - DEB
    {
      name: '@electron-forge/maker-deb',
      config: {
        options: {
          name: 'veloz',
          productName: 'VeloZ',
          genericName: 'Trading Platform',
          description: 'Professional Crypto Trading Platform',
          productDescription: 'VeloZ is a quantitative trading framework for crypto markets with automated strategies, real-time charting, and risk management.',
          maintainer: 'VeloZ Team <team@veloz.io>',
          homepage: 'https://veloz.io',
          icon: path.join(__dirname, 'assets', 'icons', 'icon.png'),
          categories: ['Finance', 'Office'],
          section: 'misc',
          priority: 'optional',
          depends: ['libsecret-1-0', 'libnotify4'],
          mimeType: ['x-scheme-handler/veloz'],
        },
      },
    },
    // Linux - RPM
    {
      name: '@electron-forge/maker-rpm',
      config: {
        options: {
          name: 'veloz',
          productName: 'VeloZ',
          genericName: 'Trading Platform',
          description: 'Professional Crypto Trading Platform',
          productDescription: 'VeloZ is a quantitative trading framework for crypto markets with automated strategies, real-time charting, and risk management.',
          license: 'MIT',
          homepage: 'https://veloz.io',
          icon: path.join(__dirname, 'assets', 'icons', 'icon.png'),
          categories: ['Finance', 'Office'],
          requires: ['libsecret', 'libnotify'],
        },
      },
    },
  ],
  publishers: [
    {
      name: '@electron-forge/publisher-github',
      config: {
        repository: {
          owner: 'Zzzode',
          name: 'VeloZ',
        },
        prerelease: false,
        draft: true,
      },
    },
  ],
  plugins: [
    {
      name: '@electron-forge/plugin-auto-unpack-natives',
      config: {},
    },
    // Fuses are used to enable/disable various Electron functionality
    new FusesPlugin({
      version: FuseVersion.V1,
      [FuseV1Options.RunAsNode]: false,
      [FuseV1Options.EnableCookieEncryption]: true,
      [FuseV1Options.EnableNodeOptionsEnvironmentVariable]: false,
      [FuseV1Options.EnableNodeCliInspectArguments]: false,
      [FuseV1Options.EnableEmbeddedAsarIntegrityValidation]: true,
      [FuseV1Options.OnlyLoadAppFromAsar]: true,
    }),
  ],
};
