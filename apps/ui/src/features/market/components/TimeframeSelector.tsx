/**
 * Timeframe selector component
 */
import { Button } from '@/shared/components';
import { useMarketStore, type Timeframe } from '../store';

const TIMEFRAMES: { value: Timeframe; label: string }[] = [
  { value: '1m', label: '1m' },
  { value: '5m', label: '5m' },
  { value: '15m', label: '15m' },
  { value: '1h', label: '1H' },
  { value: '4h', label: '4H' },
  { value: '1d', label: '1D' },
];

export function TimeframeSelector() {
  const selectedTimeframe = useMarketStore((state) => state.selectedTimeframe);
  const setSelectedTimeframe = useMarketStore((state) => state.setSelectedTimeframe);

  return (
    <div className="flex gap-1">
      {TIMEFRAMES.map(({ value, label }) => (
        <Button
          key={value}
          variant={selectedTimeframe === value ? 'primary' : 'secondary'}
          size="sm"
          onClick={() => setSelectedTimeframe(value)}
          className="min-w-[40px]"
        >
          {label}
        </Button>
      ))}
    </div>
  );
}
