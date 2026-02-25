/**
 * VeloZ Auto-Updater Module
 *
 * Handles automatic updates with:
 * - Multiple release channels (stable, beta, nightly)
 * - Delta updates for faster downloads
 * - Rollback capability
 * - Update verification
 */

const { app, dialog, BrowserWindow } = require('electron');
const { autoUpdater } = require('electron-updater');
const Store = require('electron-store');
const crypto = require('crypto');
const fs = require('fs');
const path = require('path');
const https = require('https');

// Configuration store
const store = new Store({
  name: 'veloz-updater',
  defaults: {
    channel: 'stable',
    autoDownload: false,
    autoInstall: false,
    lastCheck: null,
    lastVersion: null,
    skippedVersions: [],
    updateHistory: [],
  },
});

// Update channels configuration
const CHANNELS = {
  stable: {
    name: 'Stable',
    description: 'Recommended for most users',
    feedUrl: 'https://releases.veloz.io/stable',
  },
  beta: {
    name: 'Beta',
    description: 'Preview upcoming features',
    feedUrl: 'https://releases.veloz.io/beta',
  },
  nightly: {
    name: 'Nightly',
    description: 'Latest development builds',
    feedUrl: 'https://releases.veloz.io/nightly',
  },
};

// Update state
let updateState = {
  checking: false,
  available: false,
  downloading: false,
  downloaded: false,
  error: null,
  info: null,
  progress: null,
};

/**
 * Initialize the auto-updater
 * @param {BrowserWindow} mainWindow - Main application window
 */
function initUpdater(mainWindow) {
  // Configure updater
  autoUpdater.autoDownload = store.get('autoDownload');
  autoUpdater.autoInstallOnAppQuit = store.get('autoInstall');
  autoUpdater.allowDowngrade = false;
  autoUpdater.allowPrerelease = store.get('channel') !== 'stable';

  // Set feed URL based on channel
  const channel = store.get('channel');
  if (CHANNELS[channel]) {
    autoUpdater.setFeedURL({
      provider: 'generic',
      url: CHANNELS[channel].feedUrl,
    });
  }

  // Event handlers
  autoUpdater.on('checking-for-update', () => {
    updateState = { ...updateState, checking: true, error: null };
    mainWindow?.webContents.send('updater:state', updateState);
  });

  autoUpdater.on('update-available', (info) => {
    updateState = {
      ...updateState,
      checking: false,
      available: true,
      info: {
        version: info.version,
        releaseDate: info.releaseDate,
        releaseNotes: info.releaseNotes,
        files: info.files,
      },
    };
    mainWindow?.webContents.send('updater:state', updateState);
    mainWindow?.webContents.send('updater:available', updateState.info);

    // Record check
    store.set('lastCheck', new Date().toISOString());
    store.set('lastVersion', info.version);
  });

  autoUpdater.on('update-not-available', (info) => {
    updateState = {
      ...updateState,
      checking: false,
      available: false,
      info: null,
    };
    mainWindow?.webContents.send('updater:state', updateState);
    mainWindow?.webContents.send('updater:not-available');

    store.set('lastCheck', new Date().toISOString());
  });

  autoUpdater.on('download-progress', (progress) => {
    updateState = {
      ...updateState,
      downloading: true,
      progress: {
        percent: progress.percent,
        bytesPerSecond: progress.bytesPerSecond,
        transferred: progress.transferred,
        total: progress.total,
      },
    };
    mainWindow?.webContents.send('updater:state', updateState);
    mainWindow?.webContents.send('updater:progress', updateState.progress);
  });

  autoUpdater.on('update-downloaded', (info) => {
    updateState = {
      ...updateState,
      downloading: false,
      downloaded: true,
      progress: null,
    };
    mainWindow?.webContents.send('updater:state', updateState);
    mainWindow?.webContents.send('updater:downloaded', info);

    // Add to update history
    const history = store.get('updateHistory');
    history.unshift({
      version: info.version,
      downloadedAt: new Date().toISOString(),
      installedAt: null,
    });
    store.set('updateHistory', history.slice(0, 20)); // Keep last 20
  });

  autoUpdater.on('error', (error) => {
    updateState = {
      ...updateState,
      checking: false,
      downloading: false,
      error: error.message,
    };
    mainWindow?.webContents.send('updater:state', updateState);
    mainWindow?.webContents.send('updater:error', error.message);
  });

  return autoUpdater;
}

/**
 * Check for updates
 * @param {boolean} silent - Don't show dialogs
 * @returns {Promise<Object>} Update info or null
 */
async function checkForUpdates(silent = false) {
  try {
    const result = await autoUpdater.checkForUpdates();
    return result?.updateInfo || null;
  } catch (error) {
    if (!silent) {
      throw error;
    }
    console.error('Update check failed:', error);
    return null;
  }
}

/**
 * Download available update
 * @returns {Promise<void>}
 */
async function downloadUpdate() {
  if (!updateState.available) {
    throw new Error('No update available');
  }

  return autoUpdater.downloadUpdate();
}

/**
 * Install downloaded update
 * @param {boolean} silent - Install silently
 */
function installUpdate(silent = false) {
  if (!updateState.downloaded) {
    throw new Error('No update downloaded');
  }

  // Mark as installed in history
  const history = store.get('updateHistory');
  if (history.length > 0 && !history[0].installedAt) {
    history[0].installedAt = new Date().toISOString();
    store.set('updateHistory', history);
  }

  autoUpdater.quitAndInstall(silent);
}

/**
 * Set update channel
 * @param {string} channel - Channel name (stable, beta, nightly)
 */
function setChannel(channel) {
  if (!CHANNELS[channel]) {
    throw new Error(`Invalid channel: ${channel}`);
  }

  store.set('channel', channel);
  autoUpdater.allowPrerelease = channel !== 'stable';
  autoUpdater.setFeedURL({
    provider: 'generic',
    url: CHANNELS[channel].feedUrl,
  });
}

/**
 * Get current channel
 * @returns {string} Current channel name
 */
function getChannel() {
  return store.get('channel');
}

/**
 * Get available channels
 * @returns {Object} Channel configurations
 */
function getChannels() {
  return CHANNELS;
}

/**
 * Skip a version
 * @param {string} version - Version to skip
 */
function skipVersion(version) {
  const skipped = store.get('skippedVersions');
  if (!skipped.includes(version)) {
    skipped.push(version);
    store.set('skippedVersions', skipped);
  }
}

/**
 * Check if version is skipped
 * @param {string} version - Version to check
 * @returns {boolean}
 */
function isVersionSkipped(version) {
  return store.get('skippedVersions').includes(version);
}

/**
 * Clear skipped versions
 */
function clearSkippedVersions() {
  store.set('skippedVersions', []);
}

/**
 * Get update history
 * @returns {Array} Update history
 */
function getUpdateHistory() {
  return store.get('updateHistory');
}

/**
 * Get current update state
 * @returns {Object} Current state
 */
function getState() {
  return { ...updateState };
}

/**
 * Set auto-download preference
 * @param {boolean} enabled
 */
function setAutoDownload(enabled) {
  store.set('autoDownload', enabled);
  autoUpdater.autoDownload = enabled;
}

/**
 * Set auto-install preference
 * @param {boolean} enabled
 */
function setAutoInstall(enabled) {
  store.set('autoInstall', enabled);
  autoUpdater.autoInstallOnAppQuit = enabled;
}

/**
 * Verify update file integrity
 * @param {string} filePath - Path to update file
 * @param {string} expectedHash - Expected SHA256 hash
 * @returns {Promise<boolean>}
 */
async function verifyUpdate(filePath, expectedHash) {
  return new Promise((resolve, reject) => {
    const hash = crypto.createHash('sha256');
    const stream = fs.createReadStream(filePath);

    stream.on('data', (data) => hash.update(data));
    stream.on('end', () => {
      const actualHash = hash.digest('hex');
      resolve(actualHash === expectedHash);
    });
    stream.on('error', reject);
  });
}

/**
 * Fetch update manifest from server
 * @param {string} channel - Update channel
 * @returns {Promise<Object>} Update manifest
 */
async function fetchManifest(channel = 'stable') {
  const url = `${CHANNELS[channel].feedUrl}/update-manifest.json`;

  return new Promise((resolve, reject) => {
    https
      .get(url, (res) => {
        let data = '';
        res.on('data', (chunk) => (data += chunk));
        res.on('end', () => {
          try {
            resolve(JSON.parse(data));
          } catch (error) {
            reject(new Error('Invalid manifest format'));
          }
        });
      })
      .on('error', reject);
  });
}

/**
 * Create rollback point before update
 * @param {string} backupDir - Backup directory
 * @returns {string} Backup path
 */
function createRollbackPoint(backupDir) {
  const appPath = app.getAppPath();
  const version = app.getVersion();
  const timestamp = new Date().toISOString().replace(/[:.]/g, '-');
  const backupPath = path.join(backupDir, `veloz-${version}-${timestamp}`);

  // Create backup directory
  if (!fs.existsSync(backupDir)) {
    fs.mkdirSync(backupDir, { recursive: true });
  }

  // Copy current app to backup
  copyDirectory(appPath, backupPath);

  return backupPath;
}

/**
 * Restore from rollback point
 * @param {string} backupPath - Path to backup
 */
function restoreFromRollback(backupPath) {
  if (!fs.existsSync(backupPath)) {
    throw new Error('Rollback point not found');
  }

  const appPath = app.getAppPath();

  // Remove current app
  fs.rmSync(appPath, { recursive: true, force: true });

  // Restore from backup
  copyDirectory(backupPath, appPath);
}

/**
 * List available rollback points
 * @param {string} backupDir - Backup directory
 * @returns {Array} List of rollback points
 */
function listRollbackPoints(backupDir) {
  if (!fs.existsSync(backupDir)) {
    return [];
  }

  const entries = fs.readdirSync(backupDir, { withFileTypes: true });
  const points = [];

  for (const entry of entries) {
    if (entry.isDirectory() && entry.name.startsWith('veloz-')) {
      const match = entry.name.match(/veloz-(.+?)-(\d{4}-\d{2}-\d{2}T.+)/);
      if (match) {
        const fullPath = path.join(backupDir, entry.name);
        const stats = fs.statSync(fullPath);
        points.push({
          name: entry.name,
          path: fullPath,
          version: match[1],
          timestamp: match[2],
          created: stats.birthtime,
        });
      }
    }
  }

  return points.sort((a, b) => b.created - a.created);
}

/**
 * Clean old rollback points
 * @param {string} backupDir - Backup directory
 * @param {number} keepCount - Number to keep
 */
function cleanRollbackPoints(backupDir, keepCount = 3) {
  const points = listRollbackPoints(backupDir);

  if (points.length <= keepCount) {
    return;
  }

  const toDelete = points.slice(keepCount);
  for (const point of toDelete) {
    fs.rmSync(point.path, { recursive: true, force: true });
    console.log(`Deleted rollback point: ${point.name}`);
  }
}

// Helper function to copy directory
function copyDirectory(src, dest) {
  if (!fs.existsSync(dest)) {
    fs.mkdirSync(dest, { recursive: true });
  }

  const entries = fs.readdirSync(src, { withFileTypes: true });

  for (const entry of entries) {
    const srcPath = path.join(src, entry.name);
    const destPath = path.join(dest, entry.name);

    if (entry.isDirectory()) {
      copyDirectory(srcPath, destPath);
    } else {
      fs.copyFileSync(srcPath, destPath);
    }
  }
}

module.exports = {
  initUpdater,
  checkForUpdates,
  downloadUpdate,
  installUpdate,
  setChannel,
  getChannel,
  getChannels,
  skipVersion,
  isVersionSkipped,
  clearSkippedVersions,
  getUpdateHistory,
  getState,
  setAutoDownload,
  setAutoInstall,
  verifyUpdate,
  fetchManifest,
  createRollbackPoint,
  restoreFromRollback,
  listRollbackPoints,
  cleanRollbackPoints,
};
