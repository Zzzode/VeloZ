/**
 * Strategy Detail Page Component
 * Full detail view for a strategy template
 */

import { useState, useCallback } from 'react';
import {
  ArrowLeft,
  Heart,
  Play,
  FlaskConical,
  AlertTriangle,
  CheckCircle,
  XCircle,
} from 'lucide-react';
import { Card, Button, Modal } from '@/shared/components';
import type { StrategyTemplate, BacktestProgress as BacktestProgressType } from '../types';
import { CategoryBadge } from './CategoryBadge';
import { RiskIndicator } from './RiskIndicator';
import { PerformanceMetrics } from './PerformanceMetrics';
import { MonthlyReturnsTable } from './MonthlyReturnsTable';
import { ParameterConfigForm } from './ParameterConfigForm';
import { BacktestProgress } from './BacktestProgress';
import { useMarketplaceStore } from '../store';

interface StrategyDetailPageProps {
  strategy: StrategyTemplate;
  onBack: () => void;
  onDeploy?: (params: Record<string, number | string | boolean>) => void;
  onRunBacktest?: (params: Record<string, number | string | boolean>) => void;
  backtestProgress?: BacktestProgressType | null;
  className?: string;
}

export function StrategyDetailPage({
  strategy,
  onBack,
  onDeploy,
  onRunBacktest,
  backtestProgress,
  className = '',
}: StrategyDetailPageProps) {
  const { addFavorite, removeFavorite, isFavorite } = useMarketplaceStore();
  const [isConfigModalOpen, setIsConfigModalOpen] = useState(false);
  const [configMode, setConfigMode] = useState<'backtest' | 'deploy'>('backtest');

  const favorite = isFavorite(strategy.id);

  const handleFavoriteClick = () => {
    if (favorite) {
      removeFavorite(strategy.id);
    } else {
      addFavorite(strategy.id);
    }
  };

  const handleOpenConfig = (mode: 'backtest' | 'deploy') => {
    setConfigMode(mode);
    setIsConfigModalOpen(true);
  };

  const handleConfigSubmit = useCallback(
    (params: Record<string, number | string | boolean>) => {
      if (configMode === 'backtest') {
        onRunBacktest?.(params);
      } else {
        onDeploy?.(params);
      }
      setIsConfigModalOpen(false);
    },
    [configMode, onRunBacktest, onDeploy]
  );

  return (
    <div className={`space-y-6 ${className}`}>
      {/* Back Button */}
      <button
        onClick={onBack}
        className="flex items-center gap-2 text-text-muted hover:text-text transition-colors"
      >
        <ArrowLeft className="w-4 h-4" />
        Back to Marketplace
      </button>

      {/* Header Section */}
      <Card>
        <div className="space-y-4">
          {/* Category and Favorite */}
          <div className="flex items-start justify-between">
            <CategoryBadge category={strategy.category} />
            <button
              onClick={handleFavoriteClick}
              className={`p-2 rounded-full transition-colors ${
                favorite
                  ? 'text-danger bg-danger/10'
                  : 'text-text-muted hover:bg-surface-hover'
              }`}
              aria-label={favorite ? 'Remove from favorites' : 'Add to favorites'}
            >
              <Heart className={`w-5 h-5 ${favorite ? 'fill-current' : ''}`} />
            </button>
          </div>

          {/* Title and Author */}
          <div>
            <h1 className="text-2xl font-bold text-text">{strategy.name}</h1>
            <p className="text-text-muted mt-1">by {strategy.author}</p>
          </div>

          {/* Rating */}
          <div className="flex items-center gap-2">
            <span className="text-sm text-text-muted">
              ⭐ {strategy.rating.toFixed(1)} ({strategy.review_count} reviews)
            </span>
          </div>

          {/* Description */}
          <p className="text-text-muted">{strategy.description}</p>

          {/* Action Buttons */}
          <div className="flex flex-wrap gap-3 pt-4 border-t border-border">
            <Button
              variant="primary"
              onClick={() => handleOpenConfig('deploy')}
              leftIcon={<Play className="w-4 h-4" />}
            >
              Use This Strategy
            </Button>
            <Button
              variant="secondary"
              onClick={() => handleOpenConfig('backtest')}
              leftIcon={<FlaskConical className="w-4 h-4" />}
            >
              Run Backtest
            </Button>
            <Button
              variant="secondary"
              onClick={handleFavoriteClick}
              leftIcon={<Heart className={`w-4 h-4 ${favorite ? 'fill-current text-danger' : ''}`} />}
            >
              {favorite ? 'Remove from Favorites' : 'Add to Favorites'}
            </Button>
          </div>
        </div>
      </Card>

      {/* Backtest Progress (if running) */}
      {backtestProgress && <BacktestProgress progress={backtestProgress} />}

      {/* Quick Stats Grid */}
      <div className="grid grid-cols-2 md:grid-cols-4 gap-4">
        <Card>
          <div className="text-center">
            <p className="text-xs text-text-muted">12M Return</p>
            <p
              className={`text-2xl font-bold ${
                strategy.performance.return_12m >= 0 ? 'text-success' : 'text-danger'
              }`}
            >
              {strategy.performance.return_12m >= 0 ? '+' : ''}
              {strategy.performance.return_12m.toFixed(1)}%
            </p>
          </div>
        </Card>
        <Card>
          <div className="text-center">
            <p className="text-xs text-text-muted">Max Drawdown</p>
            <p className="text-2xl font-bold text-danger">
              {strategy.performance.max_drawdown.toFixed(1)}%
            </p>
          </div>
        </Card>
        <Card>
          <div className="text-center">
            <p className="text-xs text-text-muted">Sharpe Ratio</p>
            <p className="text-2xl font-bold text-text">
              {strategy.performance.sharpe_ratio.toFixed(2)}
            </p>
          </div>
        </Card>
        <Card>
          <div className="text-center">
            <p className="text-xs text-text-muted">Win Rate</p>
            <p className="text-2xl font-bold text-text">
              {strategy.performance.win_rate.toFixed(1)}%
            </p>
          </div>
        </Card>
      </div>

      {/* Risk Level */}
      <Card title="Risk Assessment">
        <div className="flex items-center justify-between">
          <div>
            <p className="text-sm text-text-muted mb-2">Risk Level</p>
            <RiskIndicator level={strategy.risk_level} size="lg" />
          </div>
          <div className="text-right">
            <p className="text-sm text-text-muted">Difficulty</p>
            <p className="text-lg font-medium text-text capitalize">{strategy.difficulty}</p>
          </div>
        </div>
      </Card>

      {/* Performance Metrics */}
      <PerformanceMetrics metrics={strategy.performance} />

      {/* Monthly Returns */}
      <MonthlyReturnsTable returns={strategy.monthly_returns} />

      {/* How It Works */}
      <Card title="How It Works" subtitle="Strategy logic and execution flow">
        <div className="prose prose-sm max-w-none text-text-muted">
          <pre className="whitespace-pre-wrap bg-surface-hover p-4 rounded-lg text-sm">
            {strategy.how_it_works}
          </pre>
        </div>
      </Card>

      {/* When to Use / Not Use */}
      <div className="grid grid-cols-1 md:grid-cols-2 gap-4">
        <Card title="When to Use" className="border-l-4 border-l-success">
          <ul className="space-y-2">
            {strategy.when_to_use.map((item, index) => (
              <li key={index} className="flex items-start gap-2 text-sm text-text-muted">
                <CheckCircle className="w-4 h-4 text-success mt-0.5 flex-shrink-0" />
                {item}
              </li>
            ))}
          </ul>
        </Card>
        <Card title="When NOT to Use" className="border-l-4 border-l-danger">
          <ul className="space-y-2">
            {strategy.when_not_to_use.map((item, index) => (
              <li key={index} className="flex items-start gap-2 text-sm text-text-muted">
                <XCircle className="w-4 h-4 text-danger mt-0.5 flex-shrink-0" />
                {item}
              </li>
            ))}
          </ul>
        </Card>
      </div>

      {/* Risks */}
      <Card title="Risks" subtitle="Important considerations before using this strategy">
        <div className="space-y-3">
          {strategy.risks.map((risk, index) => (
            <div
              key={index}
              className="flex items-start gap-3 p-3 bg-warning/10 rounded-lg"
            >
              <AlertTriangle className="w-5 h-5 text-warning mt-0.5 flex-shrink-0" />
              <p className="text-sm text-text-muted">{risk}</p>
            </div>
          ))}
        </div>
      </Card>

      {/* Requirements */}
      <Card title="Requirements" subtitle="What you need to run this strategy">
        <div className="grid grid-cols-1 md:grid-cols-2 gap-4">
          <div className="space-y-3">
            <div className="flex justify-between">
              <span className="text-sm text-text-muted">Minimum Capital</span>
              <span className="text-sm font-medium text-text">
                ${strategy.requirements.min_capital.toLocaleString()} USDT
              </span>
            </div>
            <div className="flex justify-between">
              <span className="text-sm text-text-muted">Recommended Capital</span>
              <span className="text-sm font-medium text-text">
                ${strategy.requirements.recommended_capital.toLocaleString()} USDT
              </span>
            </div>
          </div>
          <div className="space-y-3">
            <div>
              <span className="text-sm text-text-muted">Supported Exchanges</span>
              <div className="flex flex-wrap gap-2 mt-1">
                {strategy.requirements.supported_exchanges.map((exchange) => (
                  <span
                    key={exchange}
                    className="px-2 py-1 text-xs bg-surface-hover rounded capitalize"
                  >
                    {exchange}
                  </span>
                ))}
              </div>
            </div>
            <div>
              <span className="text-sm text-text-muted">Supported Markets</span>
              <div className="flex flex-wrap gap-2 mt-1">
                {strategy.requirements.supported_markets.map((market) => (
                  <span
                    key={market}
                    className="px-2 py-1 text-xs bg-surface-hover rounded"
                  >
                    {market}
                  </span>
                ))}
              </div>
            </div>
          </div>
        </div>
        <div className="mt-4 pt-4 border-t border-border">
          <span className="text-sm text-text-muted">API Permissions Required</span>
          <div className="flex flex-wrap gap-2 mt-2">
            {strategy.requirements.api_permissions.map((permission) => (
              <span
                key={permission}
                className="px-2 py-1 text-xs bg-primary/10 text-primary rounded"
              >
                {permission}
              </span>
            ))}
          </div>
        </div>
      </Card>

      {/* Strategy Info */}
      <Card>
        <div className="flex flex-wrap gap-6 text-sm text-text-muted">
          <div>
            <span className="font-medium">Version:</span> {strategy.version}
          </div>
          <div>
            <span className="font-medium">Users:</span> {strategy.user_count.toLocaleString()}
          </div>
          <div>
            <span className="font-medium">Updated:</span>{' '}
            {new Date(strategy.updated_at).toLocaleDateString()}
          </div>
        </div>
        <div className="mt-3 flex flex-wrap gap-2">
          {strategy.tags.map((tag) => (
            <span
              key={tag}
              className="px-2 py-1 text-xs bg-surface-hover rounded-full"
            >
              #{tag}
            </span>
          ))}
        </div>
      </Card>

      {/* Configuration Modal */}
      <Modal
        isOpen={isConfigModalOpen}
        onClose={() => setIsConfigModalOpen(false)}
        title={configMode === 'backtest' ? 'Configure Backtest' : 'Configure Strategy'}
        size="lg"
      >
        <ParameterConfigForm
          parameters={strategy.parameters}
          presets={strategy.presets}
          onRunBacktest={configMode === 'backtest' ? handleConfigSubmit : undefined}
          onDeploy={configMode === 'deploy' ? handleConfigSubmit : undefined}
        />
      </Modal>
    </div>
  );
}
