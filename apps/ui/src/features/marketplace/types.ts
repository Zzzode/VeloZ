/**
 * Strategy Marketplace Type Definitions
 * Types for strategy templates, marketplace browsing, and configuration
 */

// =============================================================================
// Strategy Category Types
// =============================================================================

/** Strategy category */
export type StrategyCategory =
  | 'momentum'
  | 'mean_reversion'
  | 'grid'
  | 'market_making'
  | 'dca'
  | 'arbitrage';

/** Strategy risk level */
export type RiskLevel = 'low' | 'medium' | 'high';

/** Strategy difficulty */
export type StrategyDifficulty = 'beginner' | 'intermediate' | 'advanced';

/** Time frame for strategy */
export type TimeFrame = 'scalping' | 'intraday' | 'swing' | 'position';

/** Market type */
export type MarketType = 'spot' | 'futures';

/** Supported exchange */
export type Exchange = 'binance' | 'okx' | 'bybit';

// =============================================================================
// Strategy Template Types
// =============================================================================

/** Strategy parameter type */
export type ParameterType =
  | 'integer'
  | 'float'
  | 'percentage'
  | 'currency'
  | 'price'
  | 'boolean'
  | 'select';

/** Strategy parameter definition */
export interface StrategyParameter {
  name: string;
  label: string;
  type: ParameterType;
  default: number | string | boolean;
  min?: number;
  max?: number;
  step?: number;
  options?: Array<{ value: string | number; label: string }>;
  description: string;
  group?: string;
}

/** Strategy preset configuration */
export interface StrategyPreset {
  name: string;
  label: string;
  description: string;
  values: Record<string, number | string | boolean>;
}

/** Performance metrics for a strategy */
export interface StrategyPerformanceMetrics {
  return_12m: number;
  return_6m: number;
  return_3m: number;
  return_1m: number;
  max_drawdown: number;
  avg_drawdown: number;
  recovery_time_days: number;
  volatility: number;
  sharpe_ratio: number;
  profit_factor: number;
  win_rate: number;
  avg_win: number;
  avg_loss: number;
  total_trades: number;
  trades_per_month: number;
}

/** Monthly return data */
export interface MonthlyReturn {
  month: string;
  year: number;
  return_pct: number;
}

/** Strategy requirements */
export interface StrategyRequirements {
  min_capital: number;
  recommended_capital: number;
  supported_exchanges: Exchange[];
  supported_markets: string[];
  api_permissions: string[];
}

/** Strategy template (marketplace item) */
export interface StrategyTemplate {
  id: string;
  name: string;
  author: string;
  category: StrategyCategory;
  risk_level: RiskLevel;
  difficulty: StrategyDifficulty;
  time_frame: TimeFrame;
  market_types: MarketType[];
  description: string;
  how_it_works: string;
  when_to_use: string[];
  when_not_to_use: string[];
  risks: string[];
  parameters: StrategyParameter[];
  presets: StrategyPreset[];
  performance: StrategyPerformanceMetrics;
  monthly_returns: MonthlyReturn[];
  requirements: StrategyRequirements;
  rating: number;
  review_count: number;
  user_count: number;
  created_at: string;
  updated_at: string;
  version: string;
  tags: string[];
}

/** Strategy template summary (for list view) */
export interface StrategyTemplateSummary {
  id: string;
  name: string;
  author: string;
  category: StrategyCategory;
  risk_level: RiskLevel;
  difficulty: StrategyDifficulty;
  description: string;
  return_12m: number;
  max_drawdown: number;
  sharpe_ratio: number;
  win_rate: number;
  trades_per_month: number;
  min_capital: number;
  rating: number;
  review_count: number;
  user_count: number;
}

// =============================================================================
// Filter and Search Types
// =============================================================================

/** Sort options for strategy list */
export type StrategySortOption =
  | 'performance'
  | 'rating'
  | 'popularity'
  | 'newest'
  | 'name';

/** Strategy filter state */
export interface StrategyFilters {
  search: string;
  categories: StrategyCategory[];
  risk_levels: RiskLevel[];
  difficulties: StrategyDifficulty[];
  time_frames: TimeFrame[];
  market_types: MarketType[];
  exchanges: Exchange[];
  min_performance: number;
  max_drawdown: number;
  min_rating: number;
  sort_by: StrategySortOption;
  sort_order: 'asc' | 'desc';
}

/** Default filter values */
export const DEFAULT_FILTERS: StrategyFilters = {
  search: '',
  categories: [],
  risk_levels: [],
  difficulties: [],
  time_frames: [],
  market_types: [],
  exchanges: [],
  min_performance: 0,
  max_drawdown: 100,
  min_rating: 0,
  sort_by: 'performance',
  sort_order: 'desc',
};

// =============================================================================
// User Interaction Types
// =============================================================================

/** User's favorite strategy */
export interface FavoriteStrategy {
  strategy_id: string;
  added_at: string;
}

/** User's strategy rating */
export interface UserRating {
  strategy_id: string;
  rating: number;
  review?: string;
  created_at: string;
}

/** Strategy comparison item */
export interface ComparisonItem {
  strategy: StrategyTemplateSummary;
  selected: boolean;
}

// =============================================================================
// Backtest Types (Marketplace-specific)
// =============================================================================

/** Backtest progress update */
export interface BacktestProgress {
  backtest_id: string;
  status: 'queued' | 'running' | 'completed' | 'failed';
  progress_pct: number;
  current_date?: string;
  message?: string;
}

/** Quick backtest request */
export interface QuickBacktestRequest {
  strategy_id: string;
  parameters: Record<string, number | string | boolean>;
  symbol: string;
  start_date: string;
  end_date: string;
  initial_capital: number;
}

// =============================================================================
// Deployment Types
// =============================================================================

/** Strategy deployment status */
export type DeploymentStatus = 'pending' | 'running' | 'paused' | 'stopped' | 'error';

/** Deployed strategy instance */
export interface DeployedStrategy {
  deployment_id: string;
  strategy_id: string;
  strategy_name: string;
  status: DeploymentStatus;
  parameters: Record<string, number | string | boolean>;
  symbol: string;
  capital_allocated: number;
  pnl: number;
  pnl_pct: number;
  trades_today: number;
  trades_total: number;
  started_at: string;
  last_trade_at?: string;
}

// =============================================================================
// API Response Types
// =============================================================================

/** List strategy templates response */
export interface ListStrategyTemplatesResponse {
  templates: StrategyTemplateSummary[];
  total: number;
  page: number;
  page_size: number;
}

/** Get strategy template response */
export interface GetStrategyTemplateResponse {
  template: StrategyTemplate;
}

/** Deploy strategy request */
export interface DeployStrategyRequest {
  strategy_id: string;
  parameters: Record<string, number | string | boolean>;
  symbol: string;
  capital: number;
}

/** Deploy strategy response */
export interface DeployStrategyResponse {
  deployment_id: string;
  status: DeploymentStatus;
  message: string;
}

/** List deployments response */
export interface ListDeploymentsResponse {
  deployments: DeployedStrategy[];
  total: number;
}

// =============================================================================
// UI Helper Types
// =============================================================================

/** Category display info */
export interface CategoryInfo {
  value: StrategyCategory;
  label: string;
  color: string;
  icon: string;
}

/** Category configuration */
export const CATEGORY_INFO: Record<StrategyCategory, CategoryInfo> = {
  momentum: {
    value: 'momentum',
    label: 'Momentum',
    color: '#7c3aed',
    icon: 'trending-up',
  },
  mean_reversion: {
    value: 'mean_reversion',
    label: 'Mean Reversion',
    color: '#2563eb',
    icon: 'refresh-cw',
  },
  grid: {
    value: 'grid',
    label: 'Grid Trading',
    color: '#059669',
    icon: 'grid',
  },
  market_making: {
    value: 'market_making',
    label: 'Market Making',
    color: '#d97706',
    icon: 'layers',
  },
  dca: {
    value: 'dca',
    label: 'DCA',
    color: '#db2777',
    icon: 'calendar',
  },
  arbitrage: {
    value: 'arbitrage',
    label: 'Arbitrage',
    color: '#0891b2',
    icon: 'shuffle',
  },
};

/** Risk level display info */
export const RISK_LEVEL_INFO: Record<RiskLevel, { label: string; color: string; segments: number }> = {
  low: { label: 'Low', color: '#22c55e', segments: 3 },
  medium: { label: 'Medium', color: '#f59e0b', segments: 6 },
  high: { label: 'High', color: '#ef4444', segments: 10 },
};

/** Difficulty display info */
export const DIFFICULTY_INFO: Record<StrategyDifficulty, { label: string; color: string }> = {
  beginner: { label: 'Beginner', color: '#22c55e' },
  intermediate: { label: 'Intermediate', color: '#f59e0b' },
  advanced: { label: 'Advanced', color: '#ef4444' },
};
