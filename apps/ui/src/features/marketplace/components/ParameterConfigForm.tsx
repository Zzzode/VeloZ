/**
 * Parameter Configuration Form Component
 * Form for configuring strategy parameters with presets
 */

import { RotateCcw, Play, Rocket } from 'lucide-react';
import { Button, Card } from '@/shared/components';
import type { StrategyParameter, StrategyPreset } from '../types';
import { ParameterInput } from './ParameterInput';
import { useState, useMemo, useCallback } from 'react';

interface ParameterConfigFormProps {
  parameters: StrategyParameter[];
  presets: StrategyPreset[];
  initialValues?: Record<string, number | string | boolean>;
  onRunBacktest?: (values: Record<string, number | string | boolean>) => void;
  onDeploy?: (values: Record<string, number | string | boolean>) => void;
  isBacktesting?: boolean;
  isDeploying?: boolean;
  className?: string;
}

export function ParameterConfigForm({
  parameters,
  presets,
  initialValues,
  onRunBacktest,
  onDeploy,
  isBacktesting = false,
  isDeploying = false,
  className = '',
}: ParameterConfigFormProps) {
  // Initialize values from initialValues or defaults
  const defaultValues = useMemo(() => {
    const values: Record<string, number | string | boolean> = {};
    parameters.forEach((param) => {
      values[param.name] = initialValues?.[param.name] ?? param.default;
    });
    return values;
  }, [parameters, initialValues]);

  const [values, setValues] = useState(defaultValues);
  const [selectedPreset, setSelectedPreset] = useState<string | null>(null);
  const [errors, setErrors] = useState<Record<string, string>>({});

  // Group parameters by group
  const groupedParameters = useMemo(() => {
    const groups: Record<string, StrategyParameter[]> = {};
    parameters.forEach((param) => {
      const group = param.group ?? 'General';
      if (!groups[group]) {
        groups[group] = [];
      }
      groups[group].push(param);
    });
    return groups;
  }, [parameters]);

  const handleValueChange = useCallback(
    (name: string, value: number | string | boolean) => {
      setValues((prev) => ({ ...prev, [name]: value }));
      setSelectedPreset(null);
      // Clear error when value changes
      if (errors[name]) {
        setErrors((prev) => {
          const next = { ...prev };
          delete next[name];
          return next;
        });
      }
    },
    [errors]
  );

  const handlePresetSelect = useCallback(
    (preset: StrategyPreset) => {
      setValues((prev) => ({ ...prev, ...preset.values }));
      setSelectedPreset(preset.name);
      setErrors({});
    },
    []
  );

  const handleReset = useCallback(() => {
    setValues(defaultValues);
    setSelectedPreset(null);
    setErrors({});
  }, [defaultValues]);

  const validateValues = useCallback((): boolean => {
    const newErrors: Record<string, string> = {};

    parameters.forEach((param) => {
      const value = values[param.name];

      if (param.type === 'integer' || param.type === 'float' || param.type === 'percentage' || param.type === 'currency' || param.type === 'price') {
        const numValue = value as number;
        if (param.min !== undefined && numValue < param.min) {
          newErrors[param.name] = `Must be at least ${param.min}`;
        }
        if (param.max !== undefined && numValue > param.max) {
          newErrors[param.name] = `Must be at most ${param.max}`;
        }
      }
    });

    setErrors(newErrors);
    return Object.keys(newErrors).length === 0;
  }, [parameters, values]);

  const handleRunBacktest = useCallback(() => {
    if (validateValues()) {
      onRunBacktest?.(values);
    }
  }, [validateValues, onRunBacktest, values]);

  const handleDeploy = useCallback(() => {
    if (validateValues()) {
      onDeploy?.(values);
    }
  }, [validateValues, onDeploy, values]);

  return (
    <div className={`space-y-6 ${className}`}>
      {/* Info Banner */}
      <div className="p-3 bg-info/10 border border-info/20 rounded-lg">
        <p className="text-sm text-info">
          Parameters affect strategy behavior. Start with defaults and adjust based on backtest results.
        </p>
      </div>

      {/* Presets */}
      {presets.length > 0 && (
        <div className="space-y-3">
          <h4 className="text-sm font-medium text-text">Quick Setup</h4>
          <div className="grid grid-cols-1 sm:grid-cols-3 gap-3">
            {presets.map((preset) => (
              <button
                key={preset.name}
                onClick={() => handlePresetSelect(preset)}
                className={`p-3 rounded-lg border text-left transition-colors ${
                  selectedPreset === preset.name
                    ? 'border-primary bg-primary/10'
                    : 'border-border hover:border-primary/50'
                }`}
              >
                <p className="font-medium text-text">{preset.label}</p>
                <p className="text-xs text-text-muted mt-1">{preset.description}</p>
              </button>
            ))}
          </div>
        </div>
      )}

      {/* Parameter Groups */}
      {Object.entries(groupedParameters).map(([group, params]) => (
        <Card key={group} title={group} className="space-y-4">
          <div className="grid grid-cols-1 md:grid-cols-2 gap-4">
            {params.map((param) => (
              <ParameterInput
                key={param.name}
                parameter={param}
                value={values[param.name]}
                onChange={(value) => handleValueChange(param.name, value)}
                error={errors[param.name]}
              />
            ))}
          </div>
        </Card>
      ))}

      {/* Actions */}
      <div className="flex flex-col sm:flex-row gap-3 pt-4 border-t border-border">
        <Button
          variant="secondary"
          onClick={handleReset}
          leftIcon={<RotateCcw className="w-4 h-4" />}
        >
          Reset to Defaults
        </Button>
        <div className="flex-1" />
        {onRunBacktest && (
          <Button
            variant="secondary"
            onClick={handleRunBacktest}
            isLoading={isBacktesting}
            leftIcon={<Play className="w-4 h-4" />}
          >
            Run Backtest
          </Button>
        )}
        {onDeploy && (
          <Button
            variant="primary"
            onClick={handleDeploy}
            isLoading={isDeploying}
            leftIcon={<Rocket className="w-4 h-4" />}
          >
            Deploy Strategy
          </Button>
        )}
      </div>
    </div>
  );
}
