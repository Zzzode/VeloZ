import { create } from 'zustand';
import { persist, createJSONStorage } from 'zustand/middleware';
import type { Config, WizardData } from '../schemas/configSchemas';
import { defaultConfig, defaultWizardData, configSchema } from '../schemas/configSchemas';

const API_BASE = 'http://127.0.0.1:8080';

// API Key info (without sensitive data)
export interface APIKeyInfo {
  id: string;
  exchange: string;
  name: string;
  permissions: string[];
  createdAt: string;
  lastUsed: string;
  status: 'active' | 'expired' | 'revoked';
  testnet: boolean;
}

// Connection test result
export interface ConnectionTestResult {
  success: boolean;
  exchange: string;
  permissions: string[];
  balanceAvailable: boolean;
  latencyMs: number;
  error?: string;
  warnings: string[];
  balance?: Record<string, number>;
}

interface ConfigState {
  // Configuration
  config: Config;
  isDirty: boolean;
  isLoading: boolean;
  error: string | null;

  // Wizard state
  wizardCompleted: boolean;
  wizardData: Partial<WizardData>;
  currentStep: number;
  completedSteps: Set<string>;

  // API Keys
  apiKeys: APIKeyInfo[];
  isLoadingKeys: boolean;

  // Actions
  setConfig: (config: Partial<Config>) => void;
  updateConfig: (section: keyof Config, data: Partial<Config[keyof Config]>) => void;
  resetConfig: () => void;
  saveConfig: () => Promise<boolean>;
  loadConfig: () => Promise<void>;

  // Wizard actions
  setWizardData: (data: Partial<WizardData>) => void;
  nextStep: () => void;
  prevStep: () => void;
  goToStep: (step: number) => void;
  completeWizard: () => Promise<void>;
  resetWizard: () => void;

  // API Key actions
  loadAPIKeys: () => Promise<void>;
  addAPIKey: (data: {
    exchange: string;
    apiKey: string;
    apiSecret: string;
    passphrase?: string;
    name: string;
    testnet: boolean;
  }) => Promise<{ success: boolean; keyId?: string; error?: string }>;
  deleteAPIKey: (keyId: string) => Promise<boolean>;
  testConnection: (keyId: string) => Promise<ConnectionTestResult>;
  testNewConnection: (data: {
    exchange: string;
    apiKey: string;
    apiSecret: string;
    passphrase?: string;
    testnet: boolean;
  }) => Promise<ConnectionTestResult>;

  // Export/Import
  exportConfig: (includeCredentials?: boolean) => Promise<string>;
  importConfig: (configJson: string) => Promise<boolean>;
}

export const useConfigStore = create<ConfigState>()(
  persist(
    (set, get) => ({
      // Initial state
      config: defaultConfig,
      isDirty: false,
      isLoading: false,
      error: null,

      wizardCompleted: false,
      wizardData: defaultWizardData,
      currentStep: 0,
      completedSteps: new Set<string>(),

      apiKeys: [],
      isLoadingKeys: false,

      // Config actions
      setConfig: (newConfig) => {
        set((state) => ({
          config: { ...state.config, ...newConfig },
          isDirty: true,
        }));
      },

      updateConfig: (section, data) => {
        set((state) => ({
          config: {
            ...state.config,
            [section]: { ...state.config[section], ...data },
          },
          isDirty: true,
        }));
      },

      resetConfig: () => {
        set({ config: defaultConfig, isDirty: false });
      },

      saveConfig: async () => {
        const { config } = get();
        set({ isLoading: true, error: null });

        try {
          // Validate config before saving
          const result = configSchema.safeParse(config);
          if (!result.success) {
            set({ error: 'Invalid configuration', isLoading: false });
            return false;
          }

          const response = await fetch(`${API_BASE}/api/config`, {
            method: 'PUT',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ config }),
          });

          if (!response.ok) {
            const errorData = await response.json();
            throw new Error(errorData.message || 'Failed to save configuration');
          }

          set({ isDirty: false, isLoading: false });
          return true;
        } catch (err) {
          set({
            error: err instanceof Error ? err.message : 'Failed to save configuration',
            isLoading: false,
          });
          return false;
        }
      },

      loadConfig: async () => {
        set({ isLoading: true, error: null });

        try {
          const response = await fetch(`${API_BASE}/api/config`);
          if (!response.ok) {
            throw new Error('Failed to load configuration');
          }

          const data = await response.json();
          if (data.config) {
            set({ config: { ...defaultConfig, ...data.config }, isLoading: false });
          } else {
            set({ isLoading: false });
          }
        } catch (err) {
          set({
            error: err instanceof Error ? err.message : 'Failed to load configuration',
            isLoading: false,
          });
        }
      },

      // Wizard actions
      setWizardData: (data) => {
        set((state) => ({
          wizardData: { ...state.wizardData, ...data },
        }));
      },

      nextStep: () => {
        set((state) => ({
          currentStep: Math.min(state.currentStep + 1, 5),
        }));
      },

      prevStep: () => {
        set((state) => ({
          currentStep: Math.max(state.currentStep - 1, 0),
        }));
      },

      goToStep: (step) => {
        set({ currentStep: Math.max(0, Math.min(step, 5)) });
      },

      completeWizard: async () => {
        const { wizardData, config } = get();
        set({ isLoading: true, error: null });

        try {
          // Save API key if provided
          if (wizardData.apiKey && wizardData.apiSecret && wizardData.exchange) {
            const keyResult = await get().addAPIKey({
              exchange: wizardData.exchange,
              apiKey: wizardData.apiKey,
              apiSecret: wizardData.apiSecret,
              passphrase: wizardData.passphrase,
              name: 'Primary',
              testnet: wizardData.testnet ?? true,
            });

            if (!keyResult.success) {
              throw new Error(keyResult.error || 'Failed to save API key');
            }
          }

          // Update config with wizard data
          const updatedConfig: Config = {
            ...config,
            trading: {
              ...config.trading,
              defaultExchange: wizardData.exchange,
            },
            risk: {
              ...config.risk,
              maxPositionSize: wizardData.maxPositionSize ?? 0.1,
              dailyLossLimit: wizardData.dailyLossLimit ?? 0.05,
              circuitBreaker: {
                ...config.risk.circuitBreaker,
                enabled: wizardData.circuitBreakerEnabled ?? true,
                threshold: wizardData.circuitBreakerThreshold ?? 0.1,
              },
            },
            security: {
              ...config.security,
              twoFactorEnabled: wizardData.twoFactorEnabled ?? false,
              recoveryEmail: wizardData.recoveryEmail ?? '',
            },
          };

          // Save config
          const response = await fetch(`${API_BASE}/api/config`, {
            method: 'PUT',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ config: updatedConfig }),
          });

          if (!response.ok) {
            throw new Error('Failed to save configuration');
          }

          set({
            config: updatedConfig,
            wizardCompleted: true,
            isLoading: false,
          });
        } catch (err) {
          set({
            error: err instanceof Error ? err.message : 'Failed to complete setup',
            isLoading: false,
          });
          throw err;
        }
      },

      resetWizard: () => {
        set({
          wizardData: defaultWizardData,
          currentStep: 0,
          completedSteps: new Set<string>(),
        });
      },

      // API Key actions
      loadAPIKeys: async () => {
        set({ isLoadingKeys: true });

        try {
          const response = await fetch(`${API_BASE}/api/exchange-keys`);
          if (!response.ok) {
            throw new Error('Failed to load API keys');
          }

          const data = await response.json();
          set({ apiKeys: data.keys || [], isLoadingKeys: false });
        } catch {
          set({ apiKeys: [], isLoadingKeys: false });
        }
      },

      addAPIKey: async (data) => {
        try {
          const response = await fetch(`${API_BASE}/api/exchange-keys`, {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify(data),
          });

          if (!response.ok) {
            const errorData = await response.json();
            return { success: false, error: errorData.message || 'Failed to add API key' };
          }

          const result = await response.json();
          await get().loadAPIKeys();
          return { success: true, keyId: result.keyId };
        } catch (err) {
          return {
            success: false,
            error: err instanceof Error ? err.message : 'Failed to add API key',
          };
        }
      },

      deleteAPIKey: async (keyId) => {
        try {
          const response = await fetch(`${API_BASE}/api/exchange-keys?key_id=${keyId}`, {
            method: 'DELETE',
          });

          if (!response.ok) {
            return false;
          }

          await get().loadAPIKeys();
          return true;
        } catch {
          return false;
        }
      },

      testConnection: async (keyId) => {
        try {
          const response = await fetch(`${API_BASE}/api/keys/${keyId}/test`, {
            method: 'POST',
          });

          if (!response.ok) {
            const errorData = await response.json();
            return {
              success: false,
              exchange: '',
              permissions: [],
              balanceAvailable: false,
              latencyMs: 0,
              error: errorData.message || 'Connection test failed',
              warnings: [],
            };
          }

          return await response.json();
        } catch (err) {
          return {
            success: false,
            exchange: '',
            permissions: [],
            balanceAvailable: false,
            latencyMs: 0,
            error: err instanceof Error ? err.message : 'Connection test failed',
            warnings: [],
          };
        }
      },

      testNewConnection: async (data) => {
        try {
          const response = await fetch(`${API_BASE}/api/exchange/test`, {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify(data),
          });

          if (!response.ok) {
            const errorData = await response.json();
            return {
              success: false,
              exchange: data.exchange,
              permissions: [],
              balanceAvailable: false,
              latencyMs: 0,
              error: errorData.message || 'Connection test failed',
              warnings: [],
            };
          }

          return await response.json();
        } catch (err) {
          return {
            success: false,
            exchange: data.exchange,
            permissions: [],
            balanceAvailable: false,
            latencyMs: 0,
            error: err instanceof Error ? err.message : 'Connection test failed',
            warnings: [],
          };
        }
      },

      // Export/Import
      exportConfig: async (includeCredentials = false) => {
        try {
          const response = await fetch(`${API_BASE}/api/config/export`, {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ includeCredentials }),
          });

          if (!response.ok) {
            throw new Error('Failed to export configuration');
          }

          const data = await response.json();
          return data.configJson;
        } catch {
          // Fallback to local config
          const { config } = get();
          return JSON.stringify(config, null, 2);
        }
      },

      importConfig: async (configJson) => {
        try {
          const parsedConfig = JSON.parse(configJson);
          const result = configSchema.safeParse(parsedConfig);

          if (!result.success) {
            set({ error: 'Invalid configuration format' });
            return false;
          }

          const response = await fetch(`${API_BASE}/api/config/import`, {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ configJson }),
          });

          if (!response.ok) {
            throw new Error('Failed to import configuration');
          }

          set({ config: result.data, isDirty: false });
          return true;
        } catch (err) {
          set({
            error: err instanceof Error ? err.message : 'Failed to import configuration',
          });
          return false;
        }
      },
    }),
    {
      name: 'veloz-config',
      storage: createJSONStorage(() => localStorage),
      partialize: (state) => ({
        wizardCompleted: state.wizardCompleted,
        config: state.config,
      }),
    }
  )
);
