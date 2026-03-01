/**
 * Strategy list/grid component
 */
import { Spinner } from '@/shared/components';
import type { StrategySummary } from '@/shared/api';
import { useStrategies } from '../hooks';
import { StrategyCard } from './StrategyCard';

interface StrategyListProps {
  onConfigure?: (strategy: StrategySummary) => void;
  onSelect?: (strategy: StrategySummary) => void;
}

export function StrategyList({ onConfigure, onSelect }: StrategyListProps) {
  const { data: strategies, isLoading, error } = useStrategies();

  if (isLoading) {
    return (
      <div className="flex items-center justify-center py-12">
        <Spinner size="lg" />
      </div>
    );
  }

  if (error) {
    return (
      <div className="text-center py-12">
        <p className="text-danger">Failed to load strategies</p>
        <p className="text-sm text-text-muted mt-1">
          {error instanceof Error ? error.message : 'Unknown error'}
        </p>
      </div>
    );
  }

  if (!strategies || strategies.length === 0) {
    return (
      <div className="text-center py-12">
        <p className="text-text-muted">No strategies available</p>
        <p className="text-sm text-text-muted mt-1">
          Configure strategies in the engine to see them here
        </p>
      </div>
    );
  }

  return (
    <div className="grid grid-cols-1 md:grid-cols-2 lg:grid-cols-3 gap-4">
      {strategies.map((strategy) => (
        <StrategyCard
          key={strategy.id}
          strategy={strategy}
          onConfigure={onConfigure}
          onSelect={onSelect}
        />
      ))}
    </div>
  );
}
