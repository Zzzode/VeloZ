import { TestTube, Zap, FileText } from 'lucide-react';
import type { TradingMode } from '../types';

interface TradingModeBadgeProps {
  mode: TradingMode;
  size?: 'sm' | 'md' | 'lg';
  showIcon?: boolean;
  onClick?: () => void;
}

const modeConfig: Record<
  TradingMode,
  {
    icon: typeof TestTube;
    label: string;
    bgColor: string;
    textColor: string;
    borderColor: string;
  }
> = {
  simulated: {
    icon: TestTube,
    label: 'SIMULATED',
    bgColor: 'bg-simulated/10',
    textColor: 'text-simulated',
    borderColor: 'border-simulated/20',
  },
  paper: {
    icon: FileText,
    label: 'PAPER',
    bgColor: 'bg-warning/10',
    textColor: 'text-warning',
    borderColor: 'border-warning/20',
  },
  live: {
    icon: Zap,
    label: 'LIVE',
    bgColor: 'bg-success/10',
    textColor: 'text-success',
    borderColor: 'border-success/20',
  },
};

const sizeClasses = {
  sm: 'px-2 py-0.5 text-xs',
  md: 'px-3 py-1 text-sm',
  lg: 'px-4 py-1.5 text-base',
};

const iconSizes = {
  sm: 'w-3 h-3',
  md: 'w-4 h-4',
  lg: 'w-5 h-5',
};

export function TradingModeBadge({
  mode,
  size = 'md',
  showIcon = true,
  onClick,
}: TradingModeBadgeProps) {
  const config = modeConfig[mode];
  const Icon = config.icon;

  const Component = onClick ? 'button' : 'div';

  return (
    <Component
      type={onClick ? 'button' : undefined}
      onClick={onClick}
      className={`
        inline-flex items-center gap-1.5 font-medium rounded-full border
        ${config.bgColor} ${config.textColor} ${config.borderColor}
        ${sizeClasses[size]}
        ${onClick ? 'cursor-pointer hover:opacity-80 transition-opacity' : ''}
      `}
    >
      {showIcon && <Icon className={iconSizes[size]} />}
      {config.label}
    </Component>
  );
}
