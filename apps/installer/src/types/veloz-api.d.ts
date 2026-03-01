/**
 * VeloZ Electron API Type Definitions
 *
 * These types define the API exposed to the renderer process
 * via the preload script's contextBridge.
 */

declare global {
  interface Window {
    veloz: VelozAPI;
  }
}

export interface VelozAPI {
  engine: EngineAPI;
  gateway: GatewayAPI;
  config: ConfigAPI;
  app: AppAPI;
  updates: UpdatesAPI;
  shell: ShellAPI;
  menu: MenuAPI;
  dialogs: DialogsAPI;
  deepLinks: DeepLinksAPI;
  platform: PlatformInfo;
}

// Engine Control API
export interface EngineAPI {
  start(): Promise<OperationResult>;
  stop(): Promise<OperationResult>;
  sendCommand(command: string): Promise<OperationResult>;
  getStatus(): Promise<EngineStatus>;
  onEvent(callback: (event: EngineEvent) => void): () => void;
  onStarted(callback: () => void): () => void;
  onStopped(callback: (code: number) => void): () => void;
  onError(callback: (error: string) => void): () => void;
}

export interface EngineStatus {
  running: boolean;
}

export interface EngineEvent {
  type: string;
  timestamp?: string;
  [key: string]: unknown;
}

// Gateway Control API
export interface GatewayAPI {
  start(): Promise<OperationResult>;
  stop(): Promise<OperationResult>;
  getStatus(): Promise<GatewayStatus>;
}

export interface GatewayStatus {
  running: boolean;
}

// Configuration API
export interface ConfigAPI {
  get<T = unknown>(key: string): Promise<T>;
  set<T = unknown>(key: string, value: T): Promise<void>;
  getAll(): Promise<VelozConfig>;
  onLoaded(callback: (config: VelozConfig) => void): () => void;
}

export interface VelozConfig {
  windowBounds: { width: number; height: number };
  autoStart: boolean;
  minimizeToTray: boolean;
  updateChannel: 'stable' | 'beta' | 'nightly';
  theme: 'system' | 'light' | 'dark';
  lastVersion: string | null;
  firstRun: boolean;
  [key: string]: unknown;
}

// App Information API
export interface AppAPI {
  getVersion(): Promise<string>;
  getPaths(): Promise<AppPaths>;
}

export interface AppPaths {
  userData: string;
  logs: string;
  temp: string;
}

// Auto-Updates API
export interface UpdatesAPI {
  check(): Promise<void>;
  download(): Promise<void>;
  install(): Promise<void>;
  onChecking(callback: () => void): () => void;
  onAvailable(callback: (info: UpdateInfo) => void): () => void;
  onNotAvailable(callback: () => void): () => void;
  onProgress(callback: (progress: UpdateProgress) => void): () => void;
  onDownloaded(callback: (info: UpdateInfo) => void): () => void;
  onError(callback: (error: string) => void): () => void;
}

export interface UpdateInfo {
  version: string;
  releaseDate?: string;
  releaseNotes?: string;
}

export interface UpdateProgress {
  percent: number;
  bytesPerSecond: number;
  transferred: number;
  total: number;
}

// Shell Operations API
export interface ShellAPI {
  openExternal(url: string): Promise<void>;
  openPath(path: string): Promise<void>;
}

// Menu Events API
export interface MenuAPI {
  onNewStrategy(callback: () => void): () => void;
  onViewOrders(callback: () => void): () => void;
  onViewPositions(callback: () => void): () => void;
}

// Dialogs API
export interface DialogsAPI {
  onFirstRunWizard(callback: () => void): () => void;
  onPreferences(callback: () => void): () => void;
}

// Deep Links API
export interface DeepLinksAPI {
  onLink(callback: (url: string) => void): () => void;
}

// Platform Information
export interface PlatformInfo {
  os: 'darwin' | 'win32' | 'linux';
  arch: 'x64' | 'arm64' | 'ia32';
  isPackaged: boolean;
}

// Common Types
export interface OperationResult {
  success: boolean;
  error?: string;
}

export {};
