import { Info, Check } from 'lucide-react';
import { useSecurityEducationStore } from '../../store';

export function SafetyChecklistStep() {
  const { safetyChecklist, toggleChecklistItem, isChecklistComplete } =
    useSecurityEducationStore();

  const allChecked = isChecklistComplete();

  return (
    <div>
      <h1 className="text-2xl font-bold text-text mb-4 text-center">
        Safety Checklist
      </h1>

      <p className="text-text-muted mb-8 text-center">
        Confirm you understand these important points:
      </p>

      {/* Checklist */}
      <div className="bg-background-secondary rounded-xl p-6 space-y-3">
        {safetyChecklist.map((item) => (
          <label
            key={item.id}
            className={`
              flex items-start gap-3 p-3 rounded-lg cursor-pointer transition-colors
              ${item.checked ? 'bg-success/5' : 'bg-background hover:bg-gray-50'}
            `}
          >
            <div className="relative flex items-center justify-center mt-0.5">
              <input
                type="checkbox"
                checked={item.checked}
                onChange={() => toggleChecklistItem(item.id)}
                className="sr-only"
              />
              <div
                className={`
                  w-5 h-5 rounded border-2 flex items-center justify-center transition-colors
                  ${
                    item.checked
                      ? 'bg-success border-success'
                      : 'border-gray-300'
                  }
                `}
              >
                {item.checked && <Check className="w-3 h-3 text-white" />}
              </div>
            </div>
            <span
              className={`text-sm ${
                item.checked ? 'text-text' : 'text-text-muted'
              }`}
            >
              {item.label}
            </span>
          </label>
        ))}
      </div>

      {/* Info box */}
      <div
        className={`
          mt-6 rounded-xl p-4 transition-colors
          ${
            allChecked
              ? 'bg-success/10 border border-success/20'
              : 'bg-primary/5 border border-primary/20'
          }
        `}
      >
        <div className="flex items-start gap-3">
          <Info
            className={`w-5 h-5 flex-shrink-0 mt-0.5 ${
              allChecked ? 'text-success' : 'text-primary'
            }`}
          />
          <p className="text-sm text-text-muted">
            {allChecked
              ? 'Great! You have acknowledged all safety requirements. Click Complete to finish the security guide.'
              : 'You must check all boxes to continue.'}
          </p>
        </div>
      </div>
    </div>
  );
}
