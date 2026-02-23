/**
 * Strategy configuration modal component
 */
import { useEffect } from 'react';
import { useForm } from 'react-hook-form';
import { Modal, Button, Input } from '@/shared/components';
import type { StrategySummary } from '@/shared/api';
import { useStrategyDetail } from '../hooks';
import { StrategyMetrics } from './StrategyMetrics';

interface StrategyConfigModalProps {
  strategy: StrategySummary | null;
  isOpen: boolean;
  onClose: () => void;
}

interface ConfigFormData {
  // Generic parameters - actual parameters would come from strategy type
  param1: string;
  param2: string;
}

export function StrategyConfigModal({ strategy, isOpen, onClose }: StrategyConfigModalProps) {
  const { data: detail, isLoading } = useStrategyDetail(strategy?.id);

  const {
    register,
    handleSubmit,
    reset,
    formState: { errors, isDirty },
  } = useForm<ConfigFormData>({
    defaultValues: {
      param1: '',
      param2: '',
    },
  });

  // Reset form when strategy changes
  useEffect(() => {
    if (strategy) {
      reset({
        param1: '',
        param2: '',
      });
    }
  }, [strategy, reset]);

  const onSubmit = (data: ConfigFormData) => {
    // TODO: Implement parameter update API call
    console.log('Update strategy parameters:', strategy?.id, data);
    onClose();
  };

  if (!strategy) return null;

  return (
    <Modal
      isOpen={isOpen}
      onClose={onClose}
      title={`Configure ${strategy.name}`}
      description={`Strategy type: ${strategy.type}`}
      size="lg"
      footer={
        <div className="flex justify-end gap-3">
          <Button variant="secondary" onClick={onClose}>
            Cancel
          </Button>
          <Button
            onClick={handleSubmit(onSubmit)}
            disabled={!isDirty}
          >
            Save Changes
          </Button>
        </div>
      }
    >
      <div className="space-y-6">
        {/* Performance Metrics */}
        {detail && !isLoading && (
          <div>
            <h4 className="text-sm font-medium text-text mb-3">Performance</h4>
            <StrategyMetrics strategy={detail} />
          </div>
        )}

        {/* Configuration Form */}
        <div>
          <h4 className="text-sm font-medium text-text mb-3">Parameters</h4>
          <form className="space-y-4">
            <Input
              label="Parameter 1"
              placeholder="Enter value"
              helperText="Strategy-specific parameter"
              {...register('param1')}
              error={errors.param1?.message}
            />
            <Input
              label="Parameter 2"
              placeholder="Enter value"
              helperText="Strategy-specific parameter"
              {...register('param2')}
              error={errors.param2?.message}
            />
            <p className="text-xs text-text-muted">
              Note: Parameter configuration will be available once the strategy parameter API is implemented.
            </p>
          </form>
        </div>
      </div>
    </Modal>
  );
}
