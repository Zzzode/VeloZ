/**
 * VeloZ Electron Main Process
 *
 * This is the main entry point for the VeloZ desktop application.
 * It manages:
 * - Application lifecycle
 * - Window management
 * - Engine and gateway process spawning
 * - Auto-updates
 * - IPC communication with renderer
 */

const { app, BrowserWindow, ipcMain, dialog, shell, Menu, Tray, nativeTheme } = require('electron');
const path = require('path');
const { spawn } = require('child_process');
const fs = require('fs');
const Store = require('electron-store');
const { autoUpdater } = require('electron-updater');
const semver = require('semver');

// Handle Squirrel events for Windows installer
if (require('electron-squirrel-startup')) {
  app.quit();
}

// Application state
let mainWindow = null;
let tray = null;
let engineProcess = null;
let gatewayProcess = null;
let isQuitting = false;

// Configuration store
const store = new Store({
  name: 'veloz-config',
  defaults: {
    windowBounds: { width: 1400, height: 900 },
    autoStart: false,
    minimizeToTray: true,
    updateChannel: 'stable',
    theme: 'system',
    lastVersion: null,
    firstRun: true,
  },
});

// Get resource paths
function getResourcePath(resourceName) {
  if (app.isPackaged) {
    return path.join(process.resourcesPath, resourceName);
  }
  return path.join(__dirname, '..', 'resources', resourceName);
}

// Get platform-specific engine executable
function getEnginePath() {
  const engineDir = getResourcePath('engine');
  const platform = process.platform;
  const ext = platform === 'win32' ? '.exe' : '';
  return path.join(engineDir, `veloz_engine${ext}`);
}

// Get Python path for gateway
function getPythonPath() {
  const gatewayDir = getResourcePath('gateway');
  const platform = process.platform;

  if (platform === 'win32') {
    return path.join(gatewayDir, 'python', 'python.exe');
  } else if (platform === 'darwin') {
    return path.join(gatewayDir, 'python', 'bin', 'python3');
  } else {
    return path.join(gatewayDir, 'python', 'bin', 'python3');
  }
}

// Get gateway script path
function getGatewayPath() {
  return path.join(getResourcePath('gateway'), 'gateway.py');
}

// Get UI path
function getUIPath() {
  return path.join(getResourcePath('ui'), 'index.html');
}

// Create the main application window
function createWindow() {
  const { width, height } = store.get('windowBounds');

  mainWindow = new BrowserWindow({
    width,
    height,
    minWidth: 1024,
    minHeight: 768,
    title: 'VeloZ',
    icon: path.join(__dirname, '..', 'assets', 'icons', 'icon.png'),
    webPreferences: {
      preload: path.join(__dirname, 'preload.js'),
      nodeIntegration: false,
      contextIsolation: true,
      sandbox: true,
    },
    show: false,
    backgroundColor: nativeTheme.shouldUseDarkColors ? '#1a1a2e' : '#ffffff',
    titleBarStyle: process.platform === 'darwin' ? 'hiddenInset' : 'default',
  });

  // Load the UI
  const uiPath = getUIPath();
  if (fs.existsSync(uiPath)) {
    mainWindow.loadFile(uiPath);
  } else {
    // Development mode - load from Vite dev server
    mainWindow.loadURL('http://localhost:5173');
  }

  // Show window when ready
  mainWindow.once('ready-to-show', () => {
    mainWindow.show();

    // Check for first run
    if (store.get('firstRun')) {
      showFirstRunWizard();
    }

    // Check for updates
    checkForUpdates();
  });

  // Save window bounds on resize
  mainWindow.on('resize', () => {
    const bounds = mainWindow.getBounds();
    store.set('windowBounds', { width: bounds.width, height: bounds.height });
  });

  // Handle close to tray
  mainWindow.on('close', (event) => {
    if (!isQuitting && store.get('minimizeToTray')) {
      event.preventDefault();
      mainWindow.hide();
      return false;
    }
  });

  mainWindow.on('closed', () => {
    mainWindow = null;
  });

  // Create application menu
  createMenu();

  return mainWindow;
}

// Create system tray
function createTray() {
  const iconPath = path.join(__dirname, '..', 'assets', 'icons', 'tray-icon.png');

  tray = new Tray(iconPath);
  tray.setToolTip('VeloZ Trading Platform');

  const contextMenu = Menu.buildFromTemplate([
    {
      label: 'Show VeloZ',
      click: () => {
        if (mainWindow) {
          mainWindow.show();
          mainWindow.focus();
        }
      },
    },
    { type: 'separator' },
    {
      label: 'Start Engine',
      click: () => startEngine(),
    },
    {
      label: 'Stop Engine',
      click: () => stopEngine(),
    },
    { type: 'separator' },
    {
      label: 'Check for Updates',
      click: () => checkForUpdates(true),
    },
    { type: 'separator' },
    {
      label: 'Quit',
      click: () => {
        isQuitting = true;
        app.quit();
      },
    },
  ]);

  tray.setContextMenu(contextMenu);

  tray.on('double-click', () => {
    if (mainWindow) {
      mainWindow.show();
      mainWindow.focus();
    }
  });
}

// Create application menu
function createMenu() {
  const isMac = process.platform === 'darwin';

  const template = [
    // App menu (macOS only)
    ...(isMac
      ? [
          {
            label: app.name,
            submenu: [
              { role: 'about' },
              { type: 'separator' },
              {
                label: 'Preferences...',
                accelerator: 'CmdOrCtrl+,',
                click: () => showPreferences(),
              },
              { type: 'separator' },
              { role: 'services' },
              { type: 'separator' },
              { role: 'hide' },
              { role: 'hideOthers' },
              { role: 'unhide' },
              { type: 'separator' },
              { role: 'quit' },
            ],
          },
        ]
      : []),
    // File menu
    {
      label: 'File',
      submenu: [
        {
          label: 'New Strategy',
          accelerator: 'CmdOrCtrl+N',
          click: () => mainWindow?.webContents.send('menu:new-strategy'),
        },
        {
          label: 'Open Configuration',
          accelerator: 'CmdOrCtrl+O',
          click: () => openConfiguration(),
        },
        { type: 'separator' },
        isMac ? { role: 'close' } : { role: 'quit' },
      ],
    },
    // Edit menu
    {
      label: 'Edit',
      submenu: [
        { role: 'undo' },
        { role: 'redo' },
        { type: 'separator' },
        { role: 'cut' },
        { role: 'copy' },
        { role: 'paste' },
        { role: 'selectAll' },
      ],
    },
    // View menu
    {
      label: 'View',
      submenu: [
        { role: 'reload' },
        { role: 'forceReload' },
        { role: 'toggleDevTools' },
        { type: 'separator' },
        { role: 'resetZoom' },
        { role: 'zoomIn' },
        { role: 'zoomOut' },
        { type: 'separator' },
        { role: 'togglefullscreen' },
      ],
    },
    // Trading menu
    {
      label: 'Trading',
      submenu: [
        {
          label: 'Start Engine',
          accelerator: 'CmdOrCtrl+Shift+S',
          click: () => startEngine(),
        },
        {
          label: 'Stop Engine',
          accelerator: 'CmdOrCtrl+Shift+X',
          click: () => stopEngine(),
        },
        { type: 'separator' },
        {
          label: 'View Orders',
          click: () => mainWindow?.webContents.send('menu:view-orders'),
        },
        {
          label: 'View Positions',
          click: () => mainWindow?.webContents.send('menu:view-positions'),
        },
      ],
    },
    // Window menu
    {
      label: 'Window',
      submenu: [
        { role: 'minimize' },
        { role: 'zoom' },
        ...(isMac
          ? [{ type: 'separator' }, { role: 'front' }, { type: 'separator' }, { role: 'window' }]
          : [{ role: 'close' }]),
      ],
    },
    // Help menu
    {
      label: 'Help',
      submenu: [
        {
          label: 'Documentation',
          click: () => shell.openExternal('https://docs.veloz.io'),
        },
        {
          label: 'Report Issue',
          click: () => shell.openExternal('https://github.com/Zzzode/VeloZ/issues'),
        },
        { type: 'separator' },
        {
          label: 'Check for Updates',
          click: () => checkForUpdates(true),
        },
        { type: 'separator' },
        {
          label: 'About VeloZ',
          click: () => showAbout(),
        },
      ],
    },
  ];

  const menu = Menu.buildFromTemplate(template);
  Menu.setApplicationMenu(menu);
}

// Start the trading engine
async function startEngine() {
  if (engineProcess) {
    console.log('Engine already running');
    return { success: false, error: 'Engine already running' };
  }

  const enginePath = getEnginePath();

  if (!fs.existsSync(enginePath)) {
    const error = `Engine not found at: ${enginePath}`;
    console.error(error);
    mainWindow?.webContents.send('engine:error', error);
    return { success: false, error };
  }

  try {
    engineProcess = spawn(enginePath, ['--stdio'], {
      stdio: ['pipe', 'pipe', 'pipe'],
    });

    engineProcess.stdout.on('data', (data) => {
      const lines = data.toString().split('\n').filter(Boolean);
      lines.forEach((line) => {
        try {
          const event = JSON.parse(line);
          mainWindow?.webContents.send('engine:event', event);
        } catch {
          console.log('Engine output:', line);
        }
      });
    });

    engineProcess.stderr.on('data', (data) => {
      console.error('Engine error:', data.toString());
      mainWindow?.webContents.send('engine:error', data.toString());
    });

    engineProcess.on('close', (code) => {
      console.log(`Engine exited with code ${code}`);
      engineProcess = null;
      mainWindow?.webContents.send('engine:stopped', code);
    });

    mainWindow?.webContents.send('engine:started');
    return { success: true };
  } catch (error) {
    console.error('Failed to start engine:', error);
    mainWindow?.webContents.send('engine:error', error.message);
    return { success: false, error: error.message };
  }
}

// Stop the trading engine
function stopEngine() {
  if (!engineProcess) {
    return { success: false, error: 'Engine not running' };
  }

  try {
    engineProcess.kill('SIGTERM');
    engineProcess = null;
    mainWindow?.webContents.send('engine:stopped', 0);
    return { success: true };
  } catch (error) {
    console.error('Failed to stop engine:', error);
    return { success: false, error: error.message };
  }
}

// Send command to engine
function sendEngineCommand(command) {
  if (!engineProcess) {
    return { success: false, error: 'Engine not running' };
  }

  try {
    engineProcess.stdin.write(command + '\n');
    return { success: true };
  } catch (error) {
    console.error('Failed to send command:', error);
    return { success: false, error: error.message };
  }
}

// Start the Python gateway
async function startGateway() {
  if (gatewayProcess) {
    console.log('Gateway already running');
    return { success: false, error: 'Gateway already running' };
  }

  const pythonPath = getPythonPath();
  const gatewayPath = getGatewayPath();

  if (!fs.existsSync(gatewayPath)) {
    const error = `Gateway not found at: ${gatewayPath}`;
    console.error(error);
    return { success: false, error };
  }

  try {
    gatewayProcess = spawn(pythonPath, [gatewayPath], {
      stdio: ['pipe', 'pipe', 'pipe'],
      env: {
        ...process.env,
        VELOZ_ENGINE_PATH: getEnginePath(),
      },
    });

    gatewayProcess.stdout.on('data', (data) => {
      console.log('Gateway:', data.toString());
    });

    gatewayProcess.stderr.on('data', (data) => {
      console.error('Gateway error:', data.toString());
    });

    gatewayProcess.on('close', (code) => {
      console.log(`Gateway exited with code ${code}`);
      gatewayProcess = null;
    });

    return { success: true };
  } catch (error) {
    console.error('Failed to start gateway:', error);
    return { success: false, error: error.message };
  }
}

// Stop the gateway
function stopGateway() {
  if (!gatewayProcess) {
    return { success: false, error: 'Gateway not running' };
  }

  try {
    gatewayProcess.kill('SIGTERM');
    gatewayProcess = null;
    return { success: true };
  } catch (error) {
    console.error('Failed to stop gateway:', error);
    return { success: false, error: error.message };
  }
}

// Auto-update functionality
function checkForUpdates(manual = false) {
  autoUpdater.autoDownload = false;
  autoUpdater.autoInstallOnAppQuit = true;

  autoUpdater.on('checking-for-update', () => {
    console.log('Checking for updates...');
    if (manual) {
      mainWindow?.webContents.send('update:checking');
    }
  });

  autoUpdater.on('update-available', (info) => {
    console.log('Update available:', info.version);
    mainWindow?.webContents.send('update:available', {
      version: info.version,
      releaseDate: info.releaseDate,
      releaseNotes: info.releaseNotes,
    });

    if (manual) {
      dialog
        .showMessageBox(mainWindow, {
          type: 'info',
          title: 'Update Available',
          message: `VeloZ ${info.version} is available.`,
          detail: 'Would you like to download and install it now?',
          buttons: ['Download', 'Later'],
          defaultId: 0,
        })
        .then((result) => {
          if (result.response === 0) {
            autoUpdater.downloadUpdate();
          }
        });
    }
  });

  autoUpdater.on('update-not-available', () => {
    console.log('No updates available');
    if (manual) {
      mainWindow?.webContents.send('update:not-available');
      dialog.showMessageBox(mainWindow, {
        type: 'info',
        title: 'No Updates',
        message: 'You are running the latest version of VeloZ.',
      });
    }
  });

  autoUpdater.on('download-progress', (progress) => {
    mainWindow?.webContents.send('update:progress', {
      percent: progress.percent,
      bytesPerSecond: progress.bytesPerSecond,
      transferred: progress.transferred,
      total: progress.total,
    });
  });

  autoUpdater.on('update-downloaded', (info) => {
    console.log('Update downloaded:', info.version);
    mainWindow?.webContents.send('update:downloaded', info);

    dialog
      .showMessageBox(mainWindow, {
        type: 'info',
        title: 'Update Ready',
        message: `VeloZ ${info.version} has been downloaded.`,
        detail: 'The update will be installed when you restart the application.',
        buttons: ['Restart Now', 'Later'],
        defaultId: 0,
      })
      .then((result) => {
        if (result.response === 0) {
          isQuitting = true;
          autoUpdater.quitAndInstall();
        }
      });
  });

  autoUpdater.on('error', (error) => {
    console.error('Update error:', error);
    mainWindow?.webContents.send('update:error', error.message);
    if (manual) {
      dialog.showMessageBox(mainWindow, {
        type: 'error',
        title: 'Update Error',
        message: 'Failed to check for updates.',
        detail: error.message,
      });
    }
  });

  autoUpdater.checkForUpdates();
}

// Show first run wizard
function showFirstRunWizard() {
  mainWindow?.webContents.send('show:first-run-wizard');
  store.set('firstRun', false);
}

// Show preferences dialog
function showPreferences() {
  mainWindow?.webContents.send('show:preferences');
}

// Show about dialog
function showAbout() {
  dialog.showMessageBox(mainWindow, {
    type: 'info',
    title: 'About VeloZ',
    message: 'VeloZ Trading Platform',
    detail: `Version: ${app.getVersion()}\nElectron: ${process.versions.electron}\nNode.js: ${process.versions.node}\nChromium: ${process.versions.chrome}\n\nProfessional crypto trading platform with automated strategies.`,
  });
}

// Open configuration file
async function openConfiguration() {
  const result = await dialog.showOpenDialog(mainWindow, {
    title: 'Open Configuration',
    filters: [
      { name: 'JSON Files', extensions: ['json'] },
      { name: 'All Files', extensions: ['*'] },
    ],
    properties: ['openFile'],
  });

  if (!result.canceled && result.filePaths.length > 0) {
    const configPath = result.filePaths[0];
    try {
      const content = fs.readFileSync(configPath, 'utf-8');
      const config = JSON.parse(content);
      mainWindow?.webContents.send('config:loaded', config);
    } catch (error) {
      dialog.showMessageBox(mainWindow, {
        type: 'error',
        title: 'Error',
        message: 'Failed to load configuration',
        detail: error.message,
      });
    }
  }
}

// IPC handlers
function setupIPC() {
  // Engine control
  ipcMain.handle('engine:start', () => startEngine());
  ipcMain.handle('engine:stop', () => stopEngine());
  ipcMain.handle('engine:command', (_, command) => sendEngineCommand(command));
  ipcMain.handle('engine:status', () => ({ running: !!engineProcess }));

  // Gateway control
  ipcMain.handle('gateway:start', () => startGateway());
  ipcMain.handle('gateway:stop', () => stopGateway());
  ipcMain.handle('gateway:status', () => ({ running: !!gatewayProcess }));

  // Configuration
  ipcMain.handle('config:get', (_, key) => store.get(key));
  ipcMain.handle('config:set', (_, key, value) => store.set(key, value));
  ipcMain.handle('config:getAll', () => store.store);

  // App info
  ipcMain.handle('app:version', () => app.getVersion());
  ipcMain.handle('app:paths', () => ({
    userData: app.getPath('userData'),
    logs: app.getPath('logs'),
    temp: app.getPath('temp'),
  }));

  // Updates
  ipcMain.handle('update:check', () => checkForUpdates(true));
  ipcMain.handle('update:download', () => autoUpdater.downloadUpdate());
  ipcMain.handle('update:install', () => {
    isQuitting = true;
    autoUpdater.quitAndInstall();
  });

  // Shell operations
  ipcMain.handle('shell:openExternal', (_, url) => shell.openExternal(url));
  ipcMain.handle('shell:openPath', (_, path) => shell.openPath(path));
}

// App lifecycle
app.whenReady().then(() => {
  setupIPC();
  createWindow();
  createTray();

  app.on('activate', () => {
    if (BrowserWindow.getAllWindows().length === 0) {
      createWindow();
    } else {
      mainWindow?.show();
    }
  });
});

app.on('window-all-closed', () => {
  if (process.platform !== 'darwin') {
    app.quit();
  }
});

app.on('before-quit', () => {
  isQuitting = true;
  stopEngine();
  stopGateway();
});

// Handle deep links (veloz:// protocol)
app.setAsDefaultProtocolClient('veloz');

app.on('open-url', (event, url) => {
  event.preventDefault();
  console.log('Deep link:', url);
  mainWindow?.webContents.send('deep-link', url);
});

// Security: Prevent new window creation
app.on('web-contents-created', (_, contents) => {
  contents.setWindowOpenHandler(() => {
    return { action: 'deny' };
  });
});
