import { useQuery, useMutation, useQueryClient } from '@tanstack/react-query';
import { ConfigFormData } from '../types/config';

const API_BASE = 'http://127.0.0.1:8080';

interface ConfigResponse {
  // Gateway
  host?: string;
  port?: number;
  preset?: string;

  // Market Data
  market_source?: string;
  market_symbol?: string;

  // Execution
  execution_mode?: string;

  // Binance
  binance_base_url?: string;
  binance_ws_base_url?: string;
  binance_trade_base_url?: string;
  binance_api_key?: string;
  binance_trade_enabled?: boolean;
  binance_user_stream_connected?: boolean;

  // OKX
  okx_api_key?: string;
  okx_base_url?: string;
  okx_ws_url?: string;
  okx_trade_url?: string;

  // Bybit
  bybit_api_key?: string;
  bybit_base_url?: string;
  bybit_ws_url?: string;
  bybit_trade_url?: string;

  // Coinbase
  coinbase_api_key?: string;
  coinbase_base_url?: string;
  coinbase_ws_url?: string;

  // Auth
  auth_enabled?: boolean;
}

async function fetchConfig(): Promise<ConfigResponse> {
  const response = await fetch(`${API_BASE}/api/config`);
  if (!response.ok) {
    throw new Error('Failed to fetch configuration');
  }
  return response.json();
}

async function updateConfig(config: Partial<ConfigFormData>): Promise<void> {
  const response = await fetch(`${API_BASE}/api/config`, {
    method: 'POST',
    headers: {
      'Content-Type': 'application/json',
    },
    body: JSON.stringify(config),
  });

  if (!response.ok) {
    const error = await response.json();
    throw new Error(error.message || 'Failed to update configuration');
  }
}

export function useConfig() {
  const queryClient = useQueryClient();

  const { data, isLoading, error, refetch } = useQuery({
    queryKey: ['config'],
    queryFn: fetchConfig,
    staleTime: 30000, // 30 seconds
  });

  const mutation = useMutation({
    mutationFn: updateConfig,
    onSuccess: () => {
      queryClient.invalidateQueries({ queryKey: ['config'] });
    },
  });

  return {
    config: data,
    isLoading,
    error,
    refetch,
    updateConfig: mutation.mutate,
    isUpdating: mutation.isPending,
    updateError: mutation.error,
    updateSuccess: mutation.isSuccess,
  };
}
