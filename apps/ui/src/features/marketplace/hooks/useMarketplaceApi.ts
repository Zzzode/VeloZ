/**
 * Marketplace API Hooks
 * TanStack Query hooks for marketplace operations
 */

import { useQuery, useMutation, useQueryClient } from '@tanstack/react-query';
import { getApiClient } from '@/shared/api/hooks/client';
import type {
  StrategyTemplateSummary,
  StrategyTemplate,
  StrategyFilters,
  BacktestProgress,
  DeployedStrategy,
  QuickBacktestRequest,
  DeployStrategyRequest,
} from '../types';

// =============================================================================
// Query Keys
// =============================================================================

export const marketplaceKeys = {
  all: ['marketplace'] as const,
  templates: () => [...marketplaceKeys.all, 'templates'] as const,
  templatesList: (filters?: Partial<StrategyFilters>) =>
    [...marketplaceKeys.templates(), 'list', filters] as const,
  templateDetail: (id: string) => [...marketplaceKeys.templates(), 'detail', id] as const,
  backtests: () => [...marketplaceKeys.all, 'backtests'] as const,
  backtestDetail: (id: string) => [...marketplaceKeys.backtests(), id] as const,
  deployments: () => [...marketplaceKeys.all, 'deployments'] as const,
  performance: (strategyId: string) =>
    [...marketplaceKeys.all, 'performance', strategyId] as const,
};

// =============================================================================
// API Response Types (from backend)
// =============================================================================

interface ListTemplatesResponse {
  templates: StrategyTemplateSummary[];
  total: number;
  page: number;
  page_size: number;
}

interface GetTemplateResponse {
  template: StrategyTemplate;
}

interface RunBacktestResponse {
  backtest_id: string;
  status: 'queued' | 'running';
}

interface DeployStrategyResponse {
  deployment_id: string;
  status: 'pending' | 'running';
  message: string;
}

interface ListDeploymentsResponse {
  deployments: DeployedStrategy[];
  total: number;
}

interface PerformanceResponse {
  strategy_id: string;
  metrics: {
    return_12m: number;
    max_drawdown: number;
    sharpe_ratio: number;
    win_rate: number;
    total_trades: number;
  };
  equity_curve: Array<{ timestamp: string; equity: number }>;
}

// =============================================================================
// Template Hooks
// =============================================================================

/**
 * Fetch list of strategy templates
 */
export function useMarketplaceTemplates(filters?: Partial<StrategyFilters>) {
  return useQuery({
    queryKey: marketplaceKeys.templatesList(filters),
    queryFn: async (): Promise<ListTemplatesResponse> => {
      const client = getApiClient();
      const params = new URLSearchParams();

      if (filters?.search) params.append('search', filters.search);
      if (filters?.categories?.length) params.append('categories', filters.categories.join(','));
      if (filters?.risk_levels?.length) params.append('risk_levels', filters.risk_levels.join(','));
      if (filters?.sort_by) params.append('sort_by', filters.sort_by);
      if (filters?.sort_order) params.append('sort_order', filters.sort_order);

      const response = await client.request<ListTemplatesResponse>(
        'GET',
        `/api/marketplace/templates?${params.toString()}`
      );
      return response;
    },
    staleTime: 30000, // 30 seconds
    refetchOnWindowFocus: false,
  });
}

/**
 * Fetch single strategy template details
 */
export function useMarketplaceTemplate(id: string | undefined) {
  return useQuery({
    queryKey: marketplaceKeys.templateDetail(id ?? ''),
    queryFn: async (): Promise<StrategyTemplate> => {
      if (!id) throw new Error('Template ID is required');
      const client = getApiClient();
      const response = await client.request<GetTemplateResponse>(
        'GET',
        `/api/marketplace/templates/${id}`
      );
      return response.template;
    },
    enabled: !!id,
    staleTime: 60000, // 1 minute
  });
}

// =============================================================================
// Backtest Hooks
// =============================================================================

/**
 * Run a backtest for a strategy
 */
export function useRunBacktest() {
  const queryClient = useQueryClient();

  return useMutation({
    mutationFn: async (request: QuickBacktestRequest): Promise<RunBacktestResponse> => {
      const client = getApiClient();
      return client.request<RunBacktestResponse>('POST', '/api/marketplace/backtest', { body: request });
    },
    onSuccess: () => {
      // Invalidate backtests list
      queryClient.invalidateQueries({ queryKey: marketplaceKeys.backtests() });
    },
  });
}

/**
 * Hook for subscribing to backtest progress via SSE
 */
export function useBacktestProgress(
  backtestId: string | null,
  _onProgress: (progress: BacktestProgress) => void,
  onComplete: () => void
) {
  // This would connect to SSE endpoint for real-time progress
  // Implementation depends on the SSE client setup
  return useQuery({
    queryKey: marketplaceKeys.backtestDetail(backtestId ?? ''),
    queryFn: async (): Promise<BacktestProgress> => {
      if (!backtestId) throw new Error('Backtest ID is required');
      const client = getApiClient();
      return client.request<BacktestProgress>('GET', `/api/marketplace/backtest/${backtestId}`);
    },
    enabled: !!backtestId,
    refetchInterval: (query) => {
      const data = query.state.data;
      if (data?.status === 'completed' || data?.status === 'failed') {
        onComplete();
        return false;
      }
      return 1000; // Poll every second while running
    },
  });
}

// =============================================================================
// Deployment Hooks
// =============================================================================

/**
 * Deploy a strategy
 */
export function useDeployStrategy() {
  const queryClient = useQueryClient();

  return useMutation({
    mutationFn: async (request: DeployStrategyRequest): Promise<DeployStrategyResponse> => {
      const client = getApiClient();
      return client.request<DeployStrategyResponse>('POST', '/api/marketplace/deploy', { body: request });
    },
    onSuccess: () => {
      // Invalidate deployments list
      queryClient.invalidateQueries({ queryKey: marketplaceKeys.deployments() });
    },
  });
}

/**
 * Fetch list of deployed strategies
 */
export function useDeployments() {
  return useQuery({
    queryKey: marketplaceKeys.deployments(),
    queryFn: async (): Promise<ListDeploymentsResponse> => {
      const client = getApiClient();
      return client.request<ListDeploymentsResponse>('GET', '/api/marketplace/deployments');
    },
    staleTime: 10000, // 10 seconds
    refetchInterval: 30000, // Refresh every 30 seconds
  });
}

// =============================================================================
// Performance Hooks
// =============================================================================

/**
 * Fetch performance data for a strategy
 */
export function useStrategyPerformance(strategyId: string | undefined) {
  return useQuery({
    queryKey: marketplaceKeys.performance(strategyId ?? ''),
    queryFn: async (): Promise<PerformanceResponse> => {
      if (!strategyId) throw new Error('Strategy ID is required');
      const client = getApiClient();
      return client.request<PerformanceResponse>(
        'GET',
        `/api/marketplace/performance?strategy_id=${strategyId}`
      );
    },
    enabled: !!strategyId,
    staleTime: 60000, // 1 minute
  });
}
