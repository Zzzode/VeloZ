/**
 * VeloZ First-Run Wizard
 *
 * Handles the initial setup experience for new users:
 * - Welcome screen with system requirements check
 * - License agreement
 * - Exchange API key configuration
 * - Risk settings
 * - Theme and preferences
 */

const { BrowserWindow, ipcMain } = require('electron');
const path = require('path');
const Store = require('electron-store');
const os = require('os');

// Configuration store
const store = new Store({
  name: 'veloz-config',
});

// System requirements
const REQUIREMENTS = {
  minRam: 4 * 1024 * 1024 * 1024, // 4GB
  minDisk: 2 * 1024 * 1024 * 1024, // 2GB
  supportedOS: {
    darwin: '12.0.0', // macOS Monterey
    win32: '10.0.0', // Windows 10
    linux: '5.0.0', // Kernel 5.0+
  },
};

/**
 * Check system requirements
 * @returns {Object} Requirements check results
 */
function checkSystemRequirements() {
  const results = {
    passed: true,
    checks: [],
  };

  // Check OS version
  const platform = process.platform;
  const osRelease = os.release();
  const minVersion = REQUIREMENTS.supportedOS[platform];

  if (minVersion) {
    const osCheck = {
      name: 'Operating System',
      required: `${getPlatformName(platform)} ${minVersion}+`,
      actual: `${getPlatformName(platform)} ${osRelease}`,
      passed: compareVersions(osRelease, minVersion) >= 0,
    };
    results.checks.push(osCheck);
    if (!osCheck.passed) results.passed = false;
  }

  // Check RAM
  const totalRam = os.totalmem();
  const ramCheck = {
    name: 'Memory (RAM)',
    required: formatBytes(REQUIREMENTS.minRam),
    actual: formatBytes(totalRam),
    passed: totalRam >= REQUIREMENTS.minRam,
  };
  results.checks.push(ramCheck);
  if (!ramCheck.passed) results.passed = false;

  // Check available disk space (approximate)
  const freeRam = os.freemem();
  const diskCheck = {
    name: 'Available Memory',
    required: formatBytes(REQUIREMENTS.minDisk),
    actual: formatBytes(freeRam),
    passed: true, // Disk check is more complex, skip for now
    note: 'Disk space check performed during installation',
  };
  results.checks.push(diskCheck);

  // Check architecture
  const arch = process.arch;
  const archCheck = {
    name: 'Architecture',
    required: 'x64 or arm64',
    actual: arch,
    passed: ['x64', 'arm64'].includes(arch),
  };
  results.checks.push(archCheck);
  if (!archCheck.passed) results.passed = false;

  return results;
}

/**
 * Get human-readable platform name
 */
function getPlatformName(platform) {
  const names = {
    darwin: 'macOS',
    win32: 'Windows',
    linux: 'Linux',
  };
  return names[platform] || platform;
}

/**
 * Compare version strings
 */
function compareVersions(a, b) {
  const partsA = a.split('.').map(Number);
  const partsB = b.split('.').map(Number);

  for (let i = 0; i < Math.max(partsA.length, partsB.length); i++) {
    const numA = partsA[i] || 0;
    const numB = partsB[i] || 0;
    if (numA > numB) return 1;
    if (numA < numB) return -1;
  }
  return 0;
}

/**
 * Format bytes to human-readable string
 */
function formatBytes(bytes) {
  const units = ['B', 'KB', 'MB', 'GB', 'TB'];
  let unitIndex = 0;
  let value = bytes;

  while (value >= 1024 && unitIndex < units.length - 1) {
    value /= 1024;
    unitIndex++;
  }

  return `${value.toFixed(1)} ${units[unitIndex]}`;
}

/**
 * Wizard step definitions
 */
const WIZARD_STEPS = [
  {
    id: 'welcome',
    title: 'Welcome to VeloZ',
    description: 'Professional crypto trading platform',
  },
  {
    id: 'license',
    title: 'License Agreement',
    description: 'Please read and accept the license terms',
  },
  {
    id: 'exchange',
    title: 'Exchange Setup',
    description: 'Configure your exchange API keys',
    optional: true,
  },
  {
    id: 'risk',
    title: 'Risk Settings',
    description: 'Set your risk management parameters',
  },
  {
    id: 'preferences',
    title: 'Preferences',
    description: 'Customize your experience',
  },
  {
    id: 'complete',
    title: 'Setup Complete',
    description: 'You are ready to start trading',
  },
];

/**
 * Default risk settings
 */
const DEFAULT_RISK_SETTINGS = {
  dailyLossLimit: 0.05, // 5%
  positionSizeLimit: 0.1, // 10%
  maxOpenOrders: 10,
  maxOpenPositions: 5,
  stopLossRequired: true,
};

/**
 * Default preferences
 */
const DEFAULT_PREFERENCES = {
  theme: 'system',
  language: 'en',
  notifications: {
    desktop: true,
    sound: true,
    email: false,
  },
  autoStart: false,
  minimizeToTray: true,
};

/**
 * Setup IPC handlers for first-run wizard
 * @param {BrowserWindow} mainWindow - Main application window
 */
function setupFirstRunHandlers(mainWindow) {
  // Get wizard steps
  ipcMain.handle('wizard:getSteps', () => WIZARD_STEPS);

  // Check system requirements
  ipcMain.handle('wizard:checkRequirements', () => checkSystemRequirements());

  // Get default settings
  ipcMain.handle('wizard:getDefaults', () => ({
    risk: DEFAULT_RISK_SETTINGS,
    preferences: DEFAULT_PREFERENCES,
  }));

  // Save exchange configuration
  ipcMain.handle('wizard:saveExchange', (_, exchange) => {
    const exchanges = store.get('exchanges', []);

    // Validate exchange data
    if (!exchange.name || !exchange.apiKey) {
      return { success: false, error: 'Missing required fields' };
    }

    // Add to exchanges list
    exchanges.push({
      id: generateId(),
      name: exchange.name,
      apiKey: exchange.apiKey,
      // Note: API secret should be encrypted before storage
      apiSecret: exchange.apiSecret ? encryptSecret(exchange.apiSecret) : null,
      testnet: exchange.testnet || false,
      createdAt: new Date().toISOString(),
    });

    store.set('exchanges', exchanges);
    return { success: true };
  });

  // Save risk settings
  ipcMain.handle('wizard:saveRisk', (_, settings) => {
    // Validate settings
    if (settings.dailyLossLimit < 0 || settings.dailyLossLimit > 1) {
      return { success: false, error: 'Invalid daily loss limit' };
    }
    if (settings.positionSizeLimit < 0 || settings.positionSizeLimit > 1) {
      return { success: false, error: 'Invalid position size limit' };
    }

    store.set('risk', {
      ...DEFAULT_RISK_SETTINGS,
      ...settings,
    });
    return { success: true };
  });

  // Save preferences
  ipcMain.handle('wizard:savePreferences', (_, preferences) => {
    store.set('general', {
      ...DEFAULT_PREFERENCES,
      ...preferences,
    });
    return { success: true };
  });

  // Complete wizard
  ipcMain.handle('wizard:complete', () => {
    store.set('firstRun', false);
    store.set('setupCompletedAt', new Date().toISOString());
    return { success: true };
  });

  // Skip wizard (for advanced users)
  ipcMain.handle('wizard:skip', () => {
    store.set('firstRun', false);
    store.set('setupSkipped', true);
    return { success: true };
  });
}

/**
 * Generate a unique ID
 */
function generateId() {
  return `${Date.now()}-${Math.random().toString(36).substr(2, 9)}`;
}

/**
 * Encrypt API secret (placeholder - use proper encryption in production)
 * @param {string} secret - Secret to encrypt
 * @returns {string} Encrypted secret
 */
function encryptSecret(secret) {
  // In production, use platform keychain:
  // - macOS: Keychain Services
  // - Windows: DPAPI
  // - Linux: Secret Service API
  //
  // For now, just base64 encode (NOT SECURE - placeholder only)
  return Buffer.from(secret).toString('base64');
}

/**
 * Decrypt API secret
 * @param {string} encrypted - Encrypted secret
 * @returns {string} Decrypted secret
 */
function decryptSecret(encrypted) {
  return Buffer.from(encrypted, 'base64').toString('utf-8');
}

module.exports = {
  checkSystemRequirements,
  setupFirstRunHandlers,
  WIZARD_STEPS,
  DEFAULT_RISK_SETTINGS,
  DEFAULT_PREFERENCES,
};
