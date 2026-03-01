// Configuration types for VeloZ settings

export interface GatewayConfig {
  host: string;
  port: number;
  preset: 'dev' | 'release' | 'asan';
}

export interface MarketDataConfig {
  source: 'sim' | 'binance_rest' | 'okx_rest' | 'bybit_rest' | 'coinbase_rest';
  symbol: string;
}

export interface BinanceConfig {
  baseUrl: string;
  wsBaseUrl: string;
  tradeBaseUrl: string;
  apiKey: string;
  apiSecret: string;
}

export interface OKXConfig {
  apiKey: string;
  apiSecret: string;
  passphrase: string;
  baseUrl: string;
  wsUrl: string;
  tradeUrl: string;
}

export interface BybitConfig {
  apiKey: string;
  apiSecret: string;
  baseUrl: string;
  wsUrl: string;
  tradeUrl: string;
}

export interface CoinbaseConfig {
  apiKey: string;
  apiSecret: string;
  baseUrl: string;
  wsUrl: string;
}

export type ExecutionMode =
  | 'sim_engine'
  | 'binance_testnet_spot'
  | 'binance_spot'
  | 'okx_testnet'
  | 'okx_spot'
  | 'bybit_testnet'
  | 'bybit_spot'
  | 'coinbase_sandbox'
  | 'coinbase_spot';

export interface ExecutionConfig {
  mode: ExecutionMode;
}

export interface VeloZConfig {
  gateway: GatewayConfig;
  marketData: MarketDataConfig;
  execution: ExecutionConfig;
  binance?: BinanceConfig;
  okx?: OKXConfig;
  bybit?: BybitConfig;
  coinbase?: CoinbaseConfig;
}

export interface ConfigFormData {
  // Gateway
  host: string;
  port: string;
  preset: string;

  // Market Data
  marketSource: string;
  marketSymbol: string;

  // Execution
  executionMode: string;

  // Binance
  binanceBaseUrl: string;
  binanceWsBaseUrl: string;
  binanceTradeBaseUrl: string;
  binanceApiKey: string;
  binanceApiSecret: string;

  // OKX
  okxApiKey: string;
  okxApiSecret: string;
  okxPassphrase: string;
  okxBaseUrl: string;
  okxWsUrl: string;
  okxTradeUrl: string;

  // Bybit
  bybitApiKey: string;
  bybitApiSecret: string;
  bybitBaseUrl: string;
  bybitWsUrl: string;
  bybitTradeUrl: string;

  // Coinbase
  coinbaseApiKey: string;
  coinbaseApiSecret: string;
  coinbaseBaseUrl: string;
  coinbaseWsUrl: string;
}
