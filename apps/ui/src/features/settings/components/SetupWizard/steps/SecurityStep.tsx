import { Button } from '@/shared/components';
import { ShieldCheck, Smartphone, Mail, Key } from 'lucide-react';

interface SecurityStepProps {
  twoFactorEnabled: boolean;
  recoveryEmail: string;
  onTwoFactorEnabledChange: (enabled: boolean) => void;
  onRecoveryEmailChange: (email: string) => void;
  onNext: () => void;
  onBack: () => void;
  onSkip: () => void;
}

export function SecurityStep({
  twoFactorEnabled,
  recoveryEmail,
  onTwoFactorEnabledChange,
  onRecoveryEmailChange,
  onNext,
  onBack,
  onSkip,
}: SecurityStepProps) {
  const isEmailValid = !recoveryEmail || /^[^\s@]+@[^\s@]+\.[^\s@]+$/.test(recoveryEmail);

  return (
    <div className="max-w-2xl mx-auto py-6">
      <h2 className="text-2xl font-bold text-text mb-2">Security Setup</h2>
      <p className="text-text-muted mb-6">
        Protect your account with additional security measures.
      </p>

      {/* Security Overview */}
      <div className="bg-blue-500/10 border border-blue-500/30 rounded-lg p-4 mb-6">
        <div className="flex items-start gap-3">
          <ShieldCheck className="w-5 h-5 text-blue-500 flex-shrink-0 mt-0.5" />
          <div className="text-sm text-blue-700">
            <p className="font-medium">Optional but Recommended</p>
            <p className="text-blue-600">
              These security features help protect your account and can be configured
              later from Settings.
            </p>
          </div>
        </div>
      </div>

      {/* Security Options */}
      <div className="space-y-4 mb-8">
        {/* Two-Factor Authentication */}
        <div className="border border-border rounded-lg p-4">
          <div className="flex items-start gap-4">
            <div className="w-10 h-10 bg-primary/10 rounded-lg flex items-center justify-center flex-shrink-0">
              <Smartphone className="w-5 h-5 text-primary" />
            </div>
            <div className="flex-1">
              <div className="flex items-center justify-between">
                <div>
                  <h3 className="font-medium text-text">Two-Factor Authentication</h3>
                  <p className="text-sm text-text-muted mt-1">
                    Add an extra layer of security with 2FA
                  </p>
                </div>
                <label className="relative inline-flex items-center cursor-pointer">
                  <input
                    type="checkbox"
                    checked={twoFactorEnabled}
                    onChange={(e) => onTwoFactorEnabledChange(e.target.checked)}
                    className="sr-only peer"
                  />
                  <div className="w-11 h-6 bg-gray-200 peer-focus:outline-none peer-focus:ring-4 peer-focus:ring-primary/20 rounded-full peer peer-checked:after:translate-x-full peer-checked:after:border-white after:content-[''] after:absolute after:top-[2px] after:left-[2px] after:bg-white after:border-gray-300 after:border after:rounded-full after:h-5 after:w-5 after:transition-all peer-checked:bg-primary"></div>
                </label>
              </div>

              {twoFactorEnabled && (
                <div className="mt-4 p-3 bg-gray-50 rounded-lg">
                  <p className="text-sm text-text-muted">
                    2FA will be configured after setup. You'll need an authenticator app
                    like Google Authenticator or Authy.
                  </p>
                </div>
              )}
            </div>
          </div>
        </div>

        {/* Recovery Email */}
        <div className="border border-border rounded-lg p-4">
          <div className="flex items-start gap-4">
            <div className="w-10 h-10 bg-primary/10 rounded-lg flex items-center justify-center flex-shrink-0">
              <Mail className="w-5 h-5 text-primary" />
            </div>
            <div className="flex-1">
              <h3 className="font-medium text-text">Recovery Email</h3>
              <p className="text-sm text-text-muted mt-1">
                Receive security alerts and account recovery options
              </p>
              <div className="mt-3">
                <input
                  type="email"
                  value={recoveryEmail}
                  onChange={(e) => onRecoveryEmailChange(e.target.value)}
                  placeholder="your@email.com"
                  className={`
                    w-full px-3 py-2 border rounded-md text-sm
                    focus:outline-none focus:ring-2 focus:border-transparent
                    ${
                      !isEmailValid
                        ? 'border-red-300 focus:ring-red-500'
                        : 'border-gray-300 focus:ring-primary'
                    }
                  `}
                />
                {!isEmailValid && (
                  <p className="text-xs text-red-500 mt-1">
                    Please enter a valid email address
                  </p>
                )}
              </div>
            </div>
          </div>
        </div>

        {/* Backup Codes Info */}
        <div className="border border-border rounded-lg p-4 opacity-60">
          <div className="flex items-start gap-4">
            <div className="w-10 h-10 bg-gray-100 rounded-lg flex items-center justify-center flex-shrink-0">
              <Key className="w-5 h-5 text-gray-500" />
            </div>
            <div className="flex-1">
              <h3 className="font-medium text-text">Backup Codes</h3>
              <p className="text-sm text-text-muted mt-1">
                Generate backup codes for account recovery
              </p>
              <p className="text-xs text-text-muted mt-2">
                Available after enabling 2FA
              </p>
            </div>
          </div>
        </div>
      </div>

      {/* Security Tips */}
      <div className="bg-surface border border-border rounded-lg p-4 mb-8">
        <h3 className="font-medium text-text mb-3">Security Best Practices</h3>
        <ul className="space-y-2 text-sm text-text-muted">
          <li className="flex items-start gap-2">
            <span className="w-1.5 h-1.5 bg-primary rounded-full mt-1.5 flex-shrink-0" />
            Use a unique, strong password for your VeloZ account
          </li>
          <li className="flex items-start gap-2">
            <span className="w-1.5 h-1.5 bg-primary rounded-full mt-1.5 flex-shrink-0" />
            Enable IP restrictions on your exchange API keys
          </li>
          <li className="flex items-start gap-2">
            <span className="w-1.5 h-1.5 bg-primary rounded-full mt-1.5 flex-shrink-0" />
            Never share your API keys or recovery codes
          </li>
          <li className="flex items-start gap-2">
            <span className="w-1.5 h-1.5 bg-primary rounded-full mt-1.5 flex-shrink-0" />
            Regularly review your connected devices and sessions
          </li>
        </ul>
      </div>

      {/* Navigation */}
      <div className="flex items-center gap-4">
        <Button variant="secondary" onClick={onBack} className="flex-1">
          Back
        </Button>
        <Button variant="secondary" onClick={onSkip}>
          Skip
        </Button>
        <Button
          variant="primary"
          onClick={onNext}
          disabled={!isEmailValid}
          className="flex-1"
        >
          Continue
        </Button>
      </div>
    </div>
  );
}
