import { useState } from 'react';
import { Button, Input } from '@/shared/components';
import { CheckCircle, AlertTriangle, Eye, EyeOff, HelpCircle } from 'lucide-react';
import type { ConnectionTestResult } from '../../../store/configStore';

interface ExchangeStepProps {
  exchange: string;
  apiKey: string;
  apiSecret: string;
  passphrase: string;
  testnet: boolean;
  onExchangeChange: (exchange: string) => void;
  onApiKeyChange: (key: string) => void;
  onApiSecretChange: (secret: string) => void;
  onPassphraseChange: (passphrase: string) => void;
  onTestnetChange: (testnet: boolean) => void;
  onTestConnection: () => Promise<ConnectionTestResult>;
  onNext: () => void;
  onBack: () => void;
}

const EXCHANGES = [
  {
    id: 'binance',
    name: 'Binance',
    logo: '/images/exchanges/binance.svg',
    description: 'Spot & Futures',
    requiresPassphrase: false,
  },
  {
    id: 'okx',
    name: 'OKX',
    logo: '/images/exchanges/okx.svg',
    description: 'Spot & Futures',
    requiresPassphrase: true,
  },
  {
    id: 'bybit',
    name: 'Bybit',
    logo: '/images/exchanges/bybit.svg',
    description: 'Spot & Futures',
    requiresPassphrase: false,
  },
  {
    id: 'coinbase',
    name: 'Coinbase',
    logo: '/images/exchanges/coinbase.svg',
    description: 'Spot Trading',
    requiresPassphrase: false,
  },
];

export function ExchangeStep({
  exchange,
  apiKey,
  apiSecret,
  passphrase,
  testnet,
  onExchangeChange,
  onApiKeyChange,
  onApiSecretChange,
  onPassphraseChange,
  onTestnetChange,
  onTestConnection,
  onNext,
  onBack,
}: ExchangeStepProps) {
  const [showSecret, setShowSecret] = useState(false);
  const [showPassphrase, setShowPassphrase] = useState(false);
  const [isTesting, setIsTesting] = useState(false);
  const [testResult, setTestResult] = useState<ConnectionTestResult | null>(null);

  const selectedExchange = EXCHANGES.find((e) => e.id === exchange);
  const requiresPassphrase = selectedExchange?.requiresPassphrase ?? false;

  const handleTestConnection = async () => {
    setIsTesting(true);
    setTestResult(null);

    try {
      const result = await onTestConnection();
      setTestResult(result);
    } catch {
      setTestResult({
        success: false,
        exchange,
        permissions: [],
        balanceAvailable: false,
        latencyMs: 0,
        error: 'Connection test failed',
        warnings: [],
      });
    } finally {
      setIsTesting(false);
    }
  };

  const canProceed =
    apiKey.length > 0 &&
    apiSecret.length > 0 &&
    (!requiresPassphrase || passphrase.length > 0);

  return (
    <div className="max-w-2xl mx-auto py-6">
      <h2 className="text-2xl font-bold text-text mb-2">Connect Your Exchange</h2>
      <p className="text-text-muted mb-6">
        Choose your exchange and enter your API credentials.
      </p>

      {/* Exchange Selection */}
      <div className="mb-6">
        <label className="block text-sm font-medium text-text mb-3">
          Select Exchange
        </label>
        <div className="grid grid-cols-2 md:grid-cols-4 gap-3">
          {EXCHANGES.map((ex) => (
            <button
              key={ex.id}
              type="button"
              onClick={() => onExchangeChange(ex.id)}
              className={`
                p-4 rounded-lg border-2 transition-all
                ${
                  exchange === ex.id
                    ? 'border-primary bg-primary/5'
                    : 'border-border hover:border-gray-300'
                }
              `}
            >
              <div className="w-12 h-12 bg-gray-100 rounded-lg mx-auto mb-2 flex items-center justify-center">
                <span className="text-lg font-bold text-gray-600">
                  {ex.name.charAt(0)}
                </span>
              </div>
              <div className="text-sm font-medium text-text">{ex.name}</div>
              <div className="text-xs text-text-muted">{ex.description}</div>
            </button>
          ))}
        </div>
      </div>

      {/* Security Notice */}
      <div className="bg-yellow-500/10 border border-yellow-500/30 rounded-lg p-4 mb-6">
        <div className="flex items-start gap-3">
          <AlertTriangle className="w-5 h-5 text-yellow-600 flex-shrink-0 mt-0.5" />
          <div className="text-sm">
            <p className="font-medium text-yellow-700 mb-1">Security Notice</p>
            <ul className="text-yellow-600 space-y-1">
              <li>Never share your API keys with anyone</li>
              <li>Enable IP restrictions on your exchange</li>
              <li>Use read-only keys for testing</li>
              <li>Never enable withdrawal permissions</li>
            </ul>
          </div>
        </div>
      </div>

      {/* API Credentials */}
      <div className="space-y-4 mb-6">
        <div>
          <Input
            label="API Key"
            value={apiKey}
            onChange={(e) => onApiKeyChange(e.target.value)}
            placeholder="Enter your API key"
            className="font-mono"
          />
        </div>

        <div>
          <label className="block text-sm font-medium text-text mb-1">
            API Secret
          </label>
          <div className="relative">
            <input
              type={showSecret ? 'text' : 'password'}
              value={apiSecret}
              onChange={(e) => onApiSecretChange(e.target.value)}
              placeholder="Enter your API secret"
              className="w-full px-3 py-2.5 pr-10 border border-gray-300 rounded-md text-sm font-mono focus:outline-none focus:ring-2 focus:ring-primary focus:border-transparent"
            />
            <button
              type="button"
              onClick={() => setShowSecret(!showSecret)}
              className="absolute inset-y-0 right-0 pr-3 flex items-center text-gray-400 hover:text-gray-600"
            >
              {showSecret ? (
                <EyeOff className="w-5 h-5" />
              ) : (
                <Eye className="w-5 h-5" />
              )}
            </button>
          </div>
        </div>

        {requiresPassphrase && (
          <div>
            <label className="block text-sm font-medium text-text mb-1">
              Passphrase (required for OKX)
            </label>
            <div className="relative">
              <input
                type={showPassphrase ? 'text' : 'password'}
                value={passphrase}
                onChange={(e) => onPassphraseChange(e.target.value)}
                placeholder="Enter your passphrase"
                className="w-full px-3 py-2.5 pr-10 border border-gray-300 rounded-md text-sm font-mono focus:outline-none focus:ring-2 focus:ring-primary focus:border-transparent"
              />
              <button
                type="button"
                onClick={() => setShowPassphrase(!showPassphrase)}
                className="absolute inset-y-0 right-0 pr-3 flex items-center text-gray-400 hover:text-gray-600"
              >
                {showPassphrase ? (
                  <EyeOff className="w-5 h-5" />
                ) : (
                  <Eye className="w-5 h-5" />
                )}
              </button>
            </div>
          </div>
        )}
      </div>

      {/* Testnet Toggle */}
      <div className="flex items-center gap-3 mb-6">
        <label className="relative inline-flex items-center cursor-pointer">
          <input
            type="checkbox"
            checked={testnet}
            onChange={(e) => onTestnetChange(e.target.checked)}
            className="sr-only peer"
          />
          <div className="w-11 h-6 bg-gray-200 peer-focus:outline-none peer-focus:ring-4 peer-focus:ring-primary/20 rounded-full peer peer-checked:after:translate-x-full peer-checked:after:border-white after:content-[''] after:absolute after:top-[2px] after:left-[2px] after:bg-white after:border-gray-300 after:border after:rounded-full after:h-5 after:w-5 after:transition-all peer-checked:bg-primary"></div>
        </label>
        <span className="text-sm text-text">Use Testnet (recommended for beginners)</span>
      </div>

      {/* Help Link */}
      <a
        href="#"
        className="inline-flex items-center gap-1 text-sm text-primary hover:underline mb-6"
      >
        <HelpCircle className="w-4 h-4" />
        How do I create API keys?
      </a>

      {/* Test Connection */}
      <div className="mb-6">
        <Button
          variant="secondary"
          onClick={handleTestConnection}
          isLoading={isTesting}
          disabled={!canProceed || isTesting}
          fullWidth
        >
          Test Connection
        </Button>
      </div>

      {/* Test Result */}
      {testResult && (
        <div
          className={`
            rounded-lg p-4 mb-6
            ${
              testResult.success
                ? 'bg-green-500/10 border border-green-500/30'
                : 'bg-red-500/10 border border-red-500/30'
            }
          `}
        >
          <div className="flex items-start gap-3">
            {testResult.success ? (
              <CheckCircle className="w-6 h-6 text-green-500 flex-shrink-0" />
            ) : (
              <AlertTriangle className="w-6 h-6 text-red-500 flex-shrink-0" />
            )}
            <div className="flex-1">
              <p
                className={`font-medium ${testResult.success ? 'text-green-700' : 'text-red-700'}`}
              >
                {testResult.success ? 'Connection Successful!' : 'Connection Failed'}
              </p>
              {testResult.success ? (
                <div className="mt-2 text-sm text-green-600 space-y-1">
                  <p>Permissions: {testResult.permissions.join(', ') || 'Read'}</p>
                  {testResult.balanceAvailable && (
                    <p>Balance access: Verified</p>
                  )}
                  <p>Latency: {testResult.latencyMs.toFixed(0)}ms</p>
                </div>
              ) : (
                <p className="mt-1 text-sm text-red-600">{testResult.error}</p>
              )}
              {testResult.warnings.length > 0 && (
                <div className="mt-2 text-sm text-yellow-600">
                  {testResult.warnings.map((warning, i) => (
                    <p key={i}>{warning}</p>
                  ))}
                </div>
              )}
            </div>
          </div>
        </div>
      )}

      {/* Navigation */}
      <div className="flex items-center gap-4">
        <Button variant="secondary" onClick={onBack} className="flex-1">
          Back
        </Button>
        <Button
          variant="primary"
          onClick={onNext}
          disabled={!canProceed}
          className="flex-1"
        >
          Continue
        </Button>
      </div>
    </div>
  );
}
