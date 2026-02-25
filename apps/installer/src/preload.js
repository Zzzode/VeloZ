/**
 * VeloZ Preload Script
 *
 * This script runs in the renderer process before the web page loads.
 * It exposes a secure API to the renderer via contextBridge.
 */

const { contextBridge, ipcRenderer } = require('electron');

// Expose protected methods to the renderer process
contextBridge.exposeInMainWorld('veloz', {
  // Engine control
  engine: {
    start: () => ipcRenderer.invoke('engine:start'),
    stop: () => ipcRenderer.invoke('engine:stop'),
    sendCommand: (command) => ipcRenderer.invoke('engine:command', command),
    getStatus: () => ipcRenderer.invoke('engine:status'),
    onEvent: (callback) => {
      const handler = (_, event) => callback(event);
      ipcRenderer.on('engine:event', handler);
      return () => ipcRenderer.removeListener('engine:event', handler);
    },
    onStarted: (callback) => {
      const handler = () => callback();
      ipcRenderer.on('engine:started', handler);
      return () => ipcRenderer.removeListener('engine:started', handler);
    },
    onStopped: (callback) => {
      const handler = (_, code) => callback(code);
      ipcRenderer.on('engine:stopped', handler);
      return () => ipcRenderer.removeListener('engine:stopped', handler);
    },
    onError: (callback) => {
      const handler = (_, error) => callback(error);
      ipcRenderer.on('engine:error', handler);
      return () => ipcRenderer.removeListener('engine:error', handler);
    },
  },

  // Gateway control
  gateway: {
    start: () => ipcRenderer.invoke('gateway:start'),
    stop: () => ipcRenderer.invoke('gateway:stop'),
    getStatus: () => ipcRenderer.invoke('gateway:status'),
  },

  // Configuration
  config: {
    get: (key) => ipcRenderer.invoke('config:get', key),
    set: (key, value) => ipcRenderer.invoke('config:set', key, value),
    getAll: () => ipcRenderer.invoke('config:getAll'),
    onLoaded: (callback) => {
      const handler = (_, config) => callback(config);
      ipcRenderer.on('config:loaded', handler);
      return () => ipcRenderer.removeListener('config:loaded', handler);
    },
  },

  // App information
  app: {
    getVersion: () => ipcRenderer.invoke('app:version'),
    getPaths: () => ipcRenderer.invoke('app:paths'),
  },

  // Auto-updates
  updates: {
    check: () => ipcRenderer.invoke('update:check'),
    download: () => ipcRenderer.invoke('update:download'),
    install: () => ipcRenderer.invoke('update:install'),
    onChecking: (callback) => {
      const handler = () => callback();
      ipcRenderer.on('update:checking', handler);
      return () => ipcRenderer.removeListener('update:checking', handler);
    },
    onAvailable: (callback) => {
      const handler = (_, info) => callback(info);
      ipcRenderer.on('update:available', handler);
      return () => ipcRenderer.removeListener('update:available', handler);
    },
    onNotAvailable: (callback) => {
      const handler = () => callback();
      ipcRenderer.on('update:not-available', handler);
      return () => ipcRenderer.removeListener('update:not-available', handler);
    },
    onProgress: (callback) => {
      const handler = (_, progress) => callback(progress);
      ipcRenderer.on('update:progress', handler);
      return () => ipcRenderer.removeListener('update:progress', handler);
    },
    onDownloaded: (callback) => {
      const handler = (_, info) => callback(info);
      ipcRenderer.on('update:downloaded', handler);
      return () => ipcRenderer.removeListener('update:downloaded', handler);
    },
    onError: (callback) => {
      const handler = (_, error) => callback(error);
      ipcRenderer.on('update:error', handler);
      return () => ipcRenderer.removeListener('update:error', handler);
    },
  },

  // Shell operations
  shell: {
    openExternal: (url) => ipcRenderer.invoke('shell:openExternal', url),
    openPath: (path) => ipcRenderer.invoke('shell:openPath', path),
  },

  // Menu events
  menu: {
    onNewStrategy: (callback) => {
      const handler = () => callback();
      ipcRenderer.on('menu:new-strategy', handler);
      return () => ipcRenderer.removeListener('menu:new-strategy', handler);
    },
    onViewOrders: (callback) => {
      const handler = () => callback();
      ipcRenderer.on('menu:view-orders', handler);
      return () => ipcRenderer.removeListener('menu:view-orders', handler);
    },
    onViewPositions: (callback) => {
      const handler = () => callback();
      ipcRenderer.on('menu:view-positions', handler);
      return () => ipcRenderer.removeListener('menu:view-positions', handler);
    },
  },

  // Dialogs
  dialogs: {
    onFirstRunWizard: (callback) => {
      const handler = () => callback();
      ipcRenderer.on('show:first-run-wizard', handler);
      return () => ipcRenderer.removeListener('show:first-run-wizard', handler);
    },
    onPreferences: (callback) => {
      const handler = () => callback();
      ipcRenderer.on('show:preferences', handler);
      return () => ipcRenderer.removeListener('show:preferences', handler);
    },
  },

  // Deep links
  deepLinks: {
    onLink: (callback) => {
      const handler = (_, url) => callback(url);
      ipcRenderer.on('deep-link', handler);
      return () => ipcRenderer.removeListener('deep-link', handler);
    },
  },

  // Platform info
  platform: {
    os: process.platform,
    arch: process.arch,
    isPackaged: process.env.NODE_ENV === 'production',
  },
});
