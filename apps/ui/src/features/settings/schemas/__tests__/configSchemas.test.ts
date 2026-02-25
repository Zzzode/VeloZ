import { describe, it, expect } from 'vitest';
import {
  generalConfigSchema,
  riskConfigSchema,
  apiKeySchema,
  configSchema,
  defaultConfig,
} from '../configSchemas';

describe('Configuration Schemas', () => {
  describe('generalConfigSchema', () => {
    it('should validate valid general config', () => {
      const validConfig = {
        language: 'en',
        theme: 'dark',
        timezone: 'UTC',
        dateFormat: 'ISO',
        notifications: {
          orderFills: true,
          priceAlerts: true,
          systemAlerts: true,
          sound: false,
        },
      };
      const result = generalConfigSchema.safeParse(validConfig);
      expect(result.success).toBe(true);
    });

    it('should reject invalid language', () => {
      const invalidConfig = {
        language: 'invalid',
        theme: 'dark',
        timezone: 'UTC',
        dateFormat: 'ISO',
        notifications: {
          orderFills: true,
          priceAlerts: true,
          systemAlerts: true,
          sound: false,
        },
      };
      const result = generalConfigSchema.safeParse(invalidConfig);
      expect(result.success).toBe(false);
    });

    it('should use defaults for missing fields', () => {
      const partialConfig = {
        notifications: {
          orderFills: true,
          priceAlerts: true,
          systemAlerts: true,
          sound: true,
        },
      };
      const result = generalConfigSchema.safeParse(partialConfig);
      expect(result.success).toBe(true);
      if (result.success) {
        expect(result.data.language).toBe('en');
        expect(result.data.theme).toBe('system');
      }
    });
  });

  describe('riskConfigSchema', () => {
    it('should validate valid risk config', () => {
      const validConfig = {
        maxPositionSize: 0.1,
        maxPositionValue: 10000,
        dailyLossLimit: 0.05,
        maxOpenOrders: 10,
        circuitBreaker: {
          enabled: true,
          threshold: 0.1,
          cooldownMinutes: 60,
        },
        requireConfirmation: {
          largeOrders: true,
          largeOrderThreshold: 1000,
          marketOrders: true,
        },
      };
      const result = riskConfigSchema.safeParse(validConfig);
      expect(result.success).toBe(true);
    });

    it('should reject maxPositionSize > 1', () => {
      const invalidConfig = {
        maxPositionSize: 1.5,
        maxPositionValue: 10000,
        dailyLossLimit: 0.05,
        maxOpenOrders: 10,
        circuitBreaker: {
          enabled: true,
          threshold: 0.1,
          cooldownMinutes: 60,
        },
        requireConfirmation: {
          largeOrders: true,
          largeOrderThreshold: 1000,
          marketOrders: true,
        },
      };
      const result = riskConfigSchema.safeParse(invalidConfig);
      expect(result.success).toBe(false);
    });

    it('should reject negative dailyLossLimit', () => {
      const invalidConfig = {
        maxPositionSize: 0.1,
        maxPositionValue: 10000,
        dailyLossLimit: -0.05,
        maxOpenOrders: 10,
        circuitBreaker: {
          enabled: true,
          threshold: 0.1,
          cooldownMinutes: 60,
        },
        requireConfirmation: {
          largeOrders: true,
          largeOrderThreshold: 1000,
          marketOrders: true,
        },
      };
      const result = riskConfigSchema.safeParse(invalidConfig);
      expect(result.success).toBe(false);
    });
  });

  describe('apiKeySchema', () => {
    it('should validate valid API key data', () => {
      const validKey = {
        exchange: 'binance',
        apiKey: 'abcdefghijklmnopqrstuvwxyz123456',
        apiSecret: 'secretsecretsecretsecret12345678',
        name: 'Main Account',
        testnet: true,
      };
      const result = apiKeySchema.safeParse(validKey);
      expect(result.success).toBe(true);
    });

    it('should reject short API key', () => {
      const invalidKey = {
        exchange: 'binance',
        apiKey: 'short',
        apiSecret: 'secretsecretsecretsecret12345678',
        name: 'Main Account',
        testnet: true,
      };
      const result = apiKeySchema.safeParse(invalidKey);
      expect(result.success).toBe(false);
    });

    it('should reject API key with spaces', () => {
      const invalidKey = {
        exchange: 'binance',
        apiKey: 'abc def ghi jkl mno pqr stu vwx',
        apiSecret: 'secretsecretsecretsecret12345678',
        name: 'Main Account',
        testnet: true,
      };
      const result = apiKeySchema.safeParse(invalidKey);
      expect(result.success).toBe(false);
    });

    it('should require passphrase for OKX', () => {
      const okxKey = {
        exchange: 'okx',
        apiKey: 'abcdefghijklmnopqrstuvwxyz123456',
        apiSecret: 'secretsecretsecretsecret12345678',
        passphrase: 'mypassphrase',
        name: 'OKX Account',
        testnet: true,
      };
      const result = apiKeySchema.safeParse(okxKey);
      expect(result.success).toBe(true);
    });
  });

  describe('configSchema', () => {
    it('should validate default config', () => {
      const result = configSchema.safeParse(defaultConfig);
      expect(result.success).toBe(true);
    });

    it('should validate complete config', () => {
      const completeConfig = {
        general: {
          language: 'en',
          theme: 'dark',
          timezone: 'America/New_York',
          dateFormat: 'US',
          notifications: {
            orderFills: true,
            priceAlerts: false,
            systemAlerts: true,
            sound: true,
          },
        },
        trading: {
          defaultSymbol: 'ETHUSDT',
          orderConfirmation: true,
          doubleClickToTrade: true,
          defaultOrderType: 'market',
          defaultTimeInForce: 'IOC',
          slippageTolerance: 0.01,
        },
        risk: {
          maxPositionSize: 0.15,
          maxPositionValue: 25000,
          dailyLossLimit: 0.03,
          maxOpenOrders: 5,
          circuitBreaker: {
            enabled: true,
            threshold: 0.05,
            cooldownMinutes: 30,
          },
          requireConfirmation: {
            largeOrders: true,
            largeOrderThreshold: 500,
            marketOrders: false,
          },
        },
        advanced: {
          apiRateLimit: 5,
          websocketReconnectDelay: 3000,
          orderBookDepth: 10,
          logLevel: 'debug',
          telemetry: false,
        },
        security: {
          twoFactorEnabled: true,
          recoveryEmail: 'test@example.com',
          sessionTimeout: 60,
          requirePasswordForTrades: true,
        },
      };
      const result = configSchema.safeParse(completeConfig);
      expect(result.success).toBe(true);
    });
  });
});
