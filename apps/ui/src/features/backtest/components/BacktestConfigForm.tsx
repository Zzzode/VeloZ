import { useForm } from 'react-hook-form';
import { zodResolver } from '@hookform/resolvers/zod';
import { Button, Input, Select } from '@/shared/components';
import {
  backtestConfigSchema,
  defaultBacktestConfig,
  type BacktestConfigFormData,
} from '../schemas';

interface BacktestConfigFormProps {
  onSubmit: (data: BacktestConfigFormData) => void;
  isLoading?: boolean;
  strategies?: Array<{ id: string; name: string }>;
}

const SYMBOL_OPTIONS = [
  { value: 'BTCUSDT', label: 'BTC/USDT' },
  { value: 'ETHUSDT', label: 'ETH/USDT' },
  { value: 'BNBUSDT', label: 'BNB/USDT' },
  { value: 'SOLUSDT', label: 'SOL/USDT' },
  { value: 'XRPUSDT', label: 'XRP/USDT' },
];

export function BacktestConfigForm({
  onSubmit,
  isLoading = false,
  strategies = [],
}: BacktestConfigFormProps) {
  const {
    register,
    handleSubmit,
    formState: { errors },
  } = useForm<BacktestConfigFormData>({
    resolver: zodResolver(backtestConfigSchema),
    defaultValues: defaultBacktestConfig,
  });

  const strategyOptions = strategies.length > 0
    ? strategies.map((s) => ({ value: s.id, label: s.name }))
    : [
        { value: 'ma_crossover', label: 'MA Crossover' },
        { value: 'rsi_reversal', label: 'RSI Reversal' },
        { value: 'breakout', label: 'Breakout Strategy' },
        { value: 'mean_reversion', label: 'Mean Reversion' },
      ];

  // Get today's date and 30 days ago for default date range
  const today = new Date().toISOString().split('T')[0];
  const thirtyDaysAgo = new Date(Date.now() - 30 * 24 * 60 * 60 * 1000)
    .toISOString()
    .split('T')[0];

  return (
    <form onSubmit={handleSubmit(onSubmit)} className="space-y-6">
      <div className="grid grid-cols-1 md:grid-cols-2 gap-4">
        <Select
          label="Symbol"
          options={SYMBOL_OPTIONS}
          error={errors.symbol?.message}
          {...register('symbol')}
        />

        <Select
          label="Strategy"
          options={strategyOptions}
          placeholder="Select a strategy"
          error={errors.strategy?.message}
          {...register('strategy')}
        />
      </div>

      <div className="grid grid-cols-1 md:grid-cols-2 gap-4">
        <Input
          type="date"
          label="Start Date"
          defaultValue={thirtyDaysAgo}
          error={errors.start_date?.message}
          {...register('start_date')}
        />

        <Input
          type="date"
          label="End Date"
          defaultValue={today}
          error={errors.end_date?.message}
          {...register('end_date')}
        />
      </div>

      <div className="grid grid-cols-1 md:grid-cols-3 gap-4">
        <Input
          type="number"
          label="Initial Capital"
          step="100"
          min="0"
          error={errors.initial_capital?.message}
          helperText="Starting balance in USD"
          {...register('initial_capital', { valueAsNumber: true })}
        />

        <Input
          type="number"
          label="Commission"
          step="0.0001"
          min="0"
          max="1"
          error={errors.commission?.message}
          helperText="Per trade (e.g., 0.001 = 0.1%)"
          {...register('commission', { valueAsNumber: true })}
        />

        <Input
          type="number"
          label="Slippage"
          step="0.0001"
          min="0"
          max="1"
          error={errors.slippage?.message}
          helperText="Expected slippage (e.g., 0.0005 = 0.05%)"
          {...register('slippage', { valueAsNumber: true })}
        />
      </div>

      <div className="flex justify-end pt-4 border-t border-border">
        <Button type="submit" isLoading={isLoading} disabled={isLoading}>
          Run Backtest
        </Button>
      </div>
    </form>
  );
}
