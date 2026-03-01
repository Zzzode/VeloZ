import { z } from 'zod';

// General Configuration Schema
export const generalConfigSchema = z.object({
  language: z.enum(['en', 'zh', 'ja', 'ko']).default('en'),
  theme: z.enum(['light', 'dark', 'system']).default('system'),
  timezone: z.string().default('UTC'),
  dateFormat: z.enum(['ISO', 'US', 'EU']).default('ISO'),
  notifications: z.object({
    orderFills: z.boolean().default(true),
    priceAlerts: z.boolean().default(true),
    systemAlerts: z.boolean().default(true),
    sound: z.boolean().default(true),
  }),
});

// Trading Configuration Schema
export const tradingConfigSchema = z.object({
  defaultExchange: z.string().optional(),
  defaultSymbol: z.string().default('BTCUSDT'),
  orderConfirmation: z.boolean().default(true),
  doubleClickToTrade: z.boolean().default(false),
  defaultOrderType: z.enum(['limit', 'market']).default('limit'),
  defaultTimeInForce: z.enum(['GTC', 'IOC', 'FOK']).default('GTC'),
  slippageTolerance: z.number().min(0).max(0.1).default(0.005),
});

// Risk Configuration Schema
export const riskConfigSchema = z.object({
  maxPositionSize: z.number().min(0.01).max(1).default(0.1),
  maxPositionValue: z.number().min(0).default(10000),
  dailyLossLimit: z.number().min(0.01).max(0.5).default(0.05),
  maxOpenOrders: z.number().min(1).max(100).default(10),
  circuitBreaker: z.object({
    enabled: z.boolean().default(true),
    threshold: z.number().min(0.01).max(0.5).default(0.1),
    cooldownMinutes: z.number().min(1).max(1440).default(60),
  }),
  requireConfirmation: z.object({
    largeOrders: z.boolean().default(true),
    largeOrderThreshold: z.number().default(1000),
    marketOrders: z.boolean().default(true),
  }),
});

// Advanced Configuration Schema
export const advancedConfigSchema = z.object({
  apiRateLimit: z.number().min(1).max(100).default(10),
  websocketReconnectDelay: z.number().min(1000).max(60000).default(5000),
  orderBookDepth: z.number().min(5).max(100).default(20),
  logLevel: z.enum(['debug', 'info', 'warn', 'error']).default('info'),
  telemetry: z.boolean().default(true),
});

// Security Configuration Schema
export const securityConfigSchema = z.object({
  twoFactorEnabled: z.boolean().default(false),
  recoveryEmail: z.string().email().optional().or(z.literal('')),
  sessionTimeout: z.number().min(5).max(1440).default(30),
  requirePasswordForTrades: z.boolean().default(false),
});

// Exchange Configuration Schema
export const exchangeConfigSchema = z.object({
  exchange: z.enum(['binance', 'binance_futures', 'okx', 'bybit', 'coinbase']),
  apiKey: z.string().min(1, 'API key is required'),
  apiSecret: z.string().min(1, 'API secret is required'),
  passphrase: z.string().optional(), // Required for OKX
  testnet: z.boolean().default(true),
  name: z.string().min(1, 'Connection name is required').default('Default'),
});

// Trading Mode Schema
export const tradingModeSchema = z.object({
  mode: z.enum(['simulated', 'paper', 'live']).default('simulated'),
  simulatedBalance: z.number().min(100).max(1000000).default(10000),
});

// Complete Configuration Schema
export const configSchema = z.object({
  general: generalConfigSchema,
  trading: tradingConfigSchema,
  risk: riskConfigSchema,
  advanced: advancedConfigSchema,
  security: securityConfigSchema,
});

// Wizard Data Schema
export const wizardDataSchema = z.object({
  // Step 1: Welcome
  termsAccepted: z.boolean(),

  // Step 2: Exchange
  exchange: z.enum(['binance', 'binance_futures', 'okx', 'bybit', 'coinbase']),
  apiKey: z.string(),
  apiSecret: z.string(),
  passphrase: z.string().optional(),
  testnet: z.boolean(),

  // Step 3: Risk
  maxPositionSize: z.number(),
  dailyLossLimit: z.number(),
  circuitBreakerEnabled: z.boolean(),
  circuitBreakerThreshold: z.number(),

  // Step 4: Trading Mode
  tradingMode: z.enum(['simulated', 'paper', 'live']),
  simulatedBalance: z.number(),

  // Step 5: Security
  twoFactorEnabled: z.boolean(),
  recoveryEmail: z.string(),
});

// API Key validation
export const apiKeySchema = z.object({
  exchange: z.enum(['binance', 'binance_futures', 'okx', 'bybit', 'coinbase']),
  apiKey: z.string()
    .min(16, 'API key is too short')
    .max(128, 'API key is too long')
    .regex(/^[A-Za-z0-9]+$/, 'API key contains invalid characters'),
  apiSecret: z.string()
    .min(16, 'API secret is too short')
    .max(128, 'API secret is too long'),
  passphrase: z.string().optional(),
  name: z.string().min(1).max(50).default('Default'),
  testnet: z.boolean().default(true),
});

// Type exports
export type GeneralConfig = z.infer<typeof generalConfigSchema>;
export type TradingConfig = z.infer<typeof tradingConfigSchema>;
export type RiskConfig = z.infer<typeof riskConfigSchema>;
export type AdvancedConfig = z.infer<typeof advancedConfigSchema>;
export type SecurityConfig = z.infer<typeof securityConfigSchema>;
export type ExchangeConfig = z.infer<typeof exchangeConfigSchema>;
export type TradingMode = z.infer<typeof tradingModeSchema>;
export type Config = z.infer<typeof configSchema>;
export type WizardData = z.infer<typeof wizardDataSchema>;
export type APIKeyData = z.infer<typeof apiKeySchema>;

// Default configuration values
export const defaultConfig: Config = {
  general: {
    language: 'en',
    theme: 'system',
    timezone: 'UTC',
    dateFormat: 'ISO',
    notifications: {
      orderFills: true,
      priceAlerts: true,
      systemAlerts: true,
      sound: true,
    },
  },
  trading: {
    defaultSymbol: 'BTCUSDT',
    orderConfirmation: true,
    doubleClickToTrade: false,
    defaultOrderType: 'limit',
    defaultTimeInForce: 'GTC',
    slippageTolerance: 0.005,
  },
  risk: {
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
  },
  advanced: {
    apiRateLimit: 10,
    websocketReconnectDelay: 5000,
    orderBookDepth: 20,
    logLevel: 'info',
    telemetry: true,
  },
  security: {
    twoFactorEnabled: false,
    recoveryEmail: '',
    sessionTimeout: 30,
    requirePasswordForTrades: false,
  },
};

// Default wizard data
export const defaultWizardData: Partial<WizardData> = {
  termsAccepted: false,
  exchange: 'binance',
  apiKey: '',
  apiSecret: '',
  passphrase: '',
  testnet: true,
  maxPositionSize: 0.1,
  dailyLossLimit: 0.05,
  circuitBreakerEnabled: true,
  circuitBreakerThreshold: 0.1,
  tradingMode: 'simulated',
  simulatedBalance: 10000,
  twoFactorEnabled: false,
  recoveryEmail: '',
};
