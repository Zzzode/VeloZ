/**
 * Parameter Input Component
 * Dynamic input for strategy parameters based on type
 */

import { Info } from 'lucide-react';
import { Input, Select } from '@/shared/components';
import type { StrategyParameter } from '../types';
import { useState } from 'react';

interface ParameterInputProps {
  parameter: StrategyParameter;
  value: number | string | boolean;
  onChange: (value: number | string | boolean) => void;
  error?: string;
  className?: string;
}

export function ParameterInput({
  parameter,
  value,
  onChange,
  error,
  className = '',
}: ParameterInputProps) {
  const [showTooltip, setShowTooltip] = useState(false);

  const renderInput = () => {
    switch (parameter.type) {
      case 'integer':
      case 'float':
      case 'percentage':
      case 'currency':
      case 'price':
        return (
          <div className="relative">
            <Input
              type="number"
              value={value as number}
              onChange={(e) => {
                const val = parameter.type === 'integer'
                  ? parseInt(e.target.value, 10)
                  : parseFloat(e.target.value);
                onChange(isNaN(val) ? parameter.default : val);
              }}
              min={parameter.min}
              max={parameter.max}
              step={parameter.step ?? (parameter.type === 'integer' ? 1 : 0.1)}
              className="pr-12"
            />
            <span className="absolute right-3 top-1/2 -translate-y-1/2 text-sm text-text-muted">
              {parameter.type === 'percentage' && '%'}
              {parameter.type === 'currency' && '$'}
              {parameter.type === 'price' && 'USD'}
            </span>
          </div>
        );

      case 'boolean':
        return (
          <label className="flex items-center gap-3 cursor-pointer">
            <input
              type="checkbox"
              checked={value as boolean}
              onChange={(e) => onChange(e.target.checked)}
              className="w-5 h-5 rounded border-border text-primary focus:ring-primary"
            />
            <span className="text-sm text-text">Enable</span>
          </label>
        );

      case 'select': {
        const selectOptions = (parameter.options ?? []).map(opt => ({
          value: String(opt.value),
          label: opt.label,
        }));
        return (
          <Select
            value={String(value)}
            onChange={(e) => onChange(e.target.value)}
            options={selectOptions}
          />
        );
      }

      default:
        return (
          <Input
            type="text"
            value={value as string}
            onChange={(e) => onChange(e.target.value)}
          />
        );
    }
  };

  const renderSlider = () => {
    if (
      parameter.type === 'boolean' ||
      parameter.type === 'select' ||
      parameter.min === undefined ||
      parameter.max === undefined
    ) {
      return null;
    }

    const numValue = value as number;
    const percentage = ((numValue - parameter.min) / (parameter.max - parameter.min)) * 100;

    return (
      <div className="mt-2">
        <input
          type="range"
          min={parameter.min}
          max={parameter.max}
          step={parameter.step ?? (parameter.type === 'integer' ? 1 : 0.1)}
          value={numValue}
          onChange={(e) => {
            const val = parameter.type === 'integer'
              ? parseInt(e.target.value, 10)
              : parseFloat(e.target.value);
            onChange(val);
          }}
          className="w-full h-2 bg-border rounded-lg appearance-none cursor-pointer accent-primary"
          style={{
            background: `linear-gradient(to right, var(--color-primary) 0%, var(--color-primary) ${percentage}%, var(--color-border) ${percentage}%, var(--color-border) 100%)`,
          }}
        />
        <div className="flex justify-between text-xs text-text-muted mt-1">
          <span>{parameter.min}</span>
          <span>{parameter.max}</span>
        </div>
      </div>
    );
  };

  return (
    <div className={`space-y-1 ${className}`}>
      {/* Label with Info */}
      <div className="flex items-center gap-2">
        <label className="text-sm font-medium text-text">
          {parameter.label}
        </label>
        <div className="relative">
          <button
            type="button"
            onMouseEnter={() => setShowTooltip(true)}
            onMouseLeave={() => setShowTooltip(false)}
            onFocus={() => setShowTooltip(true)}
            onBlur={() => setShowTooltip(false)}
            className="text-text-muted hover:text-text"
            aria-label={`Info about ${parameter.label}`}
          >
            <Info className="w-4 h-4" />
          </button>
          {showTooltip && (
            <div className="absolute left-full ml-2 top-1/2 -translate-y-1/2 z-10 w-64 p-2 bg-surface border border-border rounded-lg shadow-lg text-xs text-text-muted">
              {parameter.description}
              {parameter.min !== undefined && parameter.max !== undefined && (
                <span className="block mt-1 text-text">
                  Range: {parameter.min} - {parameter.max}
                </span>
              )}
            </div>
          )}
        </div>
      </div>

      {/* Input */}
      {renderInput()}

      {/* Slider (for numeric types) */}
      {renderSlider()}

      {/* Error */}
      {error && (
        <p className="text-xs text-danger">{error}</p>
      )}

      {/* Description (always visible for mobile) */}
      <p className="text-xs text-text-muted sm:hidden">
        {parameter.description}
      </p>
    </div>
  );
}
