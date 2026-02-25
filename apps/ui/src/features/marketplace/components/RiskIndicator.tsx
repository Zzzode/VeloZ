/**
 * Risk Indicator Component
 * Visual 10-segment bar showing strategy risk level
 */

import type { RiskLevel } from '../types';
import { RISK_LEVEL_INFO } from '../types';

interface RiskIndicatorProps {
  level: RiskLevel;
  showLabel?: boolean;
  size?: 'sm' | 'md' | 'lg';
  className?: string;
}

export function RiskIndicator({
  level,
  showLabel = true,
  size = 'md',
  className = '',
}: RiskIndicatorProps) {
  const info = RISK_LEVEL_INFO[level];
  const totalSegments = 10;

  const sizeClasses = {
    sm: { segment: 'h-1.5 w-1.5', gap: 'gap-0.5', text: 'text-xs' },
    md: { segment: 'h-2 w-2', gap: 'gap-1', text: 'text-sm' },
    lg: { segment: 'h-3 w-3', gap: 'gap-1', text: 'text-base' },
  };

  const { segment, gap, text } = sizeClasses[size];

  return (
    <div className={`flex items-center ${gap} ${className}`}>
      <div className={`flex ${gap}`} role="meter" aria-valuenow={info.segments} aria-valuemin={0} aria-valuemax={10}>
        {Array.from({ length: totalSegments }).map((_, index) => (
          <div
            key={index}
            className={`${segment} rounded-sm transition-colors`}
            style={{
              backgroundColor: index < info.segments ? info.color : 'var(--color-border)',
            }}
            aria-hidden="true"
          />
        ))}
      </div>
      {showLabel && (
        <span className={`${text} font-medium ml-2`} style={{ color: info.color }}>
          {info.label}
        </span>
      )}
    </div>
  );
}
