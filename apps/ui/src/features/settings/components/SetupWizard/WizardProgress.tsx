import { CheckCircle } from 'lucide-react';

interface WizardProgressProps {
  currentStep: number;
  steps: Array<{
    id: string;
    title: string;
  }>;
}

export function WizardProgress({ currentStep, steps }: WizardProgressProps) {
  return (
    <div className="w-full py-4">
      <div className="flex items-center justify-between">
        {steps.map((step, index) => (
          <div key={step.id} className="flex items-center flex-1">
            {/* Step indicator */}
            <div className="flex flex-col items-center">
              <div
                className={`
                  w-10 h-10 rounded-full flex items-center justify-center font-semibold text-sm
                  transition-all duration-200
                  ${
                    index < currentStep
                      ? 'bg-success text-white'
                      : index === currentStep
                        ? 'bg-primary text-white ring-4 ring-primary/20'
                        : 'bg-gray-200 text-gray-500'
                  }
                `}
              >
                {index < currentStep ? (
                  <CheckCircle className="w-6 h-6" />
                ) : (
                  index + 1
                )}
              </div>
              <span
                className={`
                  mt-2 text-xs font-medium whitespace-nowrap
                  ${index <= currentStep ? 'text-text' : 'text-text-muted'}
                `}
              >
                {step.title}
              </span>
            </div>

            {/* Connector line */}
            {index < steps.length - 1 && (
              <div
                className={`
                  flex-1 h-1 mx-2 rounded
                  ${index < currentStep ? 'bg-success' : 'bg-gray-200'}
                `}
              />
            )}
          </div>
        ))}
      </div>
    </div>
  );
}
