import { z } from 'zod';

/**
 * Zod schema for backtest configuration form validation
 */
export const backtestConfigSchema = z.object({
  symbol: z
    .string()
    .min(1, 'Symbol is required')
    .transform((val) => val.toUpperCase()),
  strategy: z.string().min(1, 'Strategy is required'),
  start_date: z.string().min(1, 'Start date is required'),
  end_date: z.string().min(1, 'End date is required'),
  initial_capital: z
    .number({ message: 'Initial capital must be a number' })
    .positive('Initial capital must be positive'),
  commission: z
    .number({ message: 'Commission must be a number' })
    .min(0, 'Commission cannot be negative')
    .max(1, 'Commission cannot exceed 100%'),
  slippage: z
    .number({ message: 'Slippage must be a number' })
    .min(0, 'Slippage cannot be negative')
    .max(1, 'Slippage cannot exceed 100%'),
  parameters: z.record(z.string(), z.unknown()).optional(),
}).refine(
  (data) => {
    const start = new Date(data.start_date);
    const end = new Date(data.end_date);
    return end > start;
  },
  {
    message: 'End date must be after start date',
    path: ['end_date'],
  }
);

export type BacktestConfigFormData = z.infer<typeof backtestConfigSchema>;

/**
 * Default values for backtest configuration form
 */
export const defaultBacktestConfig: BacktestConfigFormData = {
  symbol: 'BTCUSDT',
  strategy: '',
  start_date: '',
  end_date: '',
  initial_capital: 10000,
  commission: 0.001,
  slippage: 0.0005,
  parameters: {},
};
