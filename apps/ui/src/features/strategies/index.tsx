/**
 * Strategies feature module - main page
 */
import { useState, useCallback } from 'react';
import { Card } from '@/shared/components';
import type { StrategySummary } from '@/shared/api';
import { StrategyList, StrategyConfigModal, StrategyDetailPanel } from './components';
import { useStrategies } from './hooks';

export default function Strategies() {
  const { data: strategies } = useStrategies();

  // Selected strategy for detail view
  const [selectedStrategy, setSelectedStrategy] = useState<StrategySummary | null>(null);

  // Strategy being configured
  const [configuringStrategy, setConfiguringStrategy] = useState<StrategySummary | null>(null);

  const handleSelectStrategy = useCallback((strategy: StrategySummary) => {
    setSelectedStrategy(strategy);
  }, []);

  const handleConfigureStrategy = useCallback((strategy: StrategySummary) => {
    setConfiguringStrategy(strategy);
  }, []);

  const handleCloseConfig = useCallback(() => {
    setConfiguringStrategy(null);
  }, []);

  const handleBackToList = useCallback(() => {
    setSelectedStrategy(null);
  }, []);

  // Calculate summary stats
  const runningCount = strategies?.filter(s => s.state === 'Running').length ?? 0;
  const totalPnl = strategies?.reduce((sum, s) => sum + s.pnl, 0) ?? 0;
  const totalTrades = strategies?.reduce((sum, s) => sum + s.trade_count, 0) ?? 0;

  // Show detail view if a strategy is selected
  if (selectedStrategy) {
    return (
      <>
        <StrategyDetailPanel
          strategy={selectedStrategy}
          onBack={handleBackToList}
          onConfigure={() => handleConfigureStrategy(selectedStrategy)}
        />
        <StrategyConfigModal
          strategy={configuringStrategy}
          isOpen={configuringStrategy !== null}
          onClose={handleCloseConfig}
        />
      </>
    );
  }

  return (
    <div className="space-y-6">
      {/* Page Header */}
      <div>
        <h1 className="text-2xl font-bold text-text">Strategies</h1>
        <p className="text-text-muted mt-1">Manage and monitor trading strategies</p>
      </div>

      {/* Summary Stats */}
      <div className="grid grid-cols-1 md:grid-cols-3 gap-4">
        <Card>
          <div className="text-center">
            <p className="text-sm text-text-muted">Running Strategies</p>
            <p className="text-3xl font-bold text-success mt-1">{runningCount}</p>
            <p className="text-xs text-text-muted mt-1">
              of {strategies?.length ?? 0} total
            </p>
          </div>
        </Card>
        <Card>
          <div className="text-center">
            <p className="text-sm text-text-muted">Total P&L</p>
            <p className={`text-3xl font-bold mt-1 ${totalPnl >= 0 ? 'text-success' : 'text-danger'}`}>
              {totalPnl >= 0 ? '+' : ''}${totalPnl.toLocaleString('en-US', { minimumFractionDigits: 2 })}
            </p>
          </div>
        </Card>
        <Card>
          <div className="text-center">
            <p className="text-sm text-text-muted">Total Trades</p>
            <p className="text-3xl font-bold text-text mt-1">{totalTrades}</p>
          </div>
        </Card>
      </div>

      {/* Strategy List */}
      <Card title="Active Strategies" subtitle="Click a strategy to view details">
        <StrategyList
          onConfigure={handleConfigureStrategy}
          onSelect={handleSelectStrategy}
        />
      </Card>

      {/* Config Modal */}
      <StrategyConfigModal
        strategy={configuringStrategy}
        isOpen={configuringStrategy !== null}
        onClose={handleCloseConfig}
      />
    </div>
  );
}
