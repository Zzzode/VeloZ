import { useState, useEffect } from 'react';
import { Button, Card, Modal } from '@/shared/components';
import { useConfigStore, type APIKeyInfo, type ConnectionTestResult } from '../../store/configStore';
import { Plus, Trash2, CheckCircle, AlertTriangle, Eye, EyeOff } from 'lucide-react';

const EXCHANGES = [
  { id: 'binance', name: 'Binance', requiresPassphrase: false },
  { id: 'binance_futures', name: 'Binance Futures', requiresPassphrase: false },
  { id: 'okx', name: 'OKX', requiresPassphrase: true },
  { id: 'bybit', name: 'Bybit', requiresPassphrase: false },
  { id: 'coinbase', name: 'Coinbase', requiresPassphrase: false },
];

export function APIKeyManager() {
  const { apiKeys, isLoadingKeys, loadAPIKeys, addAPIKey, deleteAPIKey, testConnection } =
    useConfigStore();

  const [showAddModal, setShowAddModal] = useState(false);
  const [testingKeyId, setTestingKeyId] = useState<string | null>(null);
  const [testResults, setTestResults] = useState<Record<string, ConnectionTestResult>>({});
  const [deletingKeyId, setDeletingKeyId] = useState<string | null>(null);

  useEffect(() => {
    loadAPIKeys();
  }, [loadAPIKeys]);

  const handleTestKey = async (keyId: string) => {
    setTestingKeyId(keyId);
    try {
      const result = await testConnection(keyId);
      setTestResults((prev) => ({ ...prev, [keyId]: result }));
    } finally {
      setTestingKeyId(null);
    }
  };

  const handleDeleteKey = async (keyId: string) => {
    setDeletingKeyId(keyId);
    try {
      await deleteAPIKey(keyId);
    } finally {
      setDeletingKeyId(null);
    }
  };

  const getStatusBadge = (status: APIKeyInfo['status']) => {
    switch (status) {
      case 'active':
        return (
          <span className="px-2 py-0.5 bg-green-500/10 text-green-600 text-xs font-medium rounded">
            Active
          </span>
        );
      case 'expired':
        return (
          <span className="px-2 py-0.5 bg-yellow-500/10 text-yellow-600 text-xs font-medium rounded">
            Expired
          </span>
        );
      case 'revoked':
        return (
          <span className="px-2 py-0.5 bg-red-500/10 text-red-600 text-xs font-medium rounded">
            Revoked
          </span>
        );
    }
  };

  return (
    <Card title="API Keys">
      <div className="space-y-4">
        {/* Header */}
        <div className="flex items-center justify-between">
          <p className="text-sm text-text-muted">
            Manage your exchange API keys securely
          </p>
          <Button
            variant="primary"
            size="sm"
            leftIcon={<Plus className="w-4 h-4" />}
            onClick={() => setShowAddModal(true)}
          >
            Add API Key
          </Button>
        </div>

        {/* Key List */}
        {isLoadingKeys ? (
          <div className="py-8 text-center text-text-muted">Loading API keys...</div>
        ) : apiKeys.length === 0 ? (
          <div className="py-8 text-center">
            <p className="text-text-muted mb-4">No API keys configured</p>
            <Button
              variant="secondary"
              size="sm"
              leftIcon={<Plus className="w-4 h-4" />}
              onClick={() => setShowAddModal(true)}
            >
              Add Your First API Key
            </Button>
          </div>
        ) : (
          <div className="space-y-3">
            {apiKeys.map((key) => (
              <div
                key={key.id}
                className="border border-border rounded-lg p-4 hover:border-gray-300 transition-colors"
              >
                <div className="flex items-start justify-between">
                  <div className="flex-1">
                    <div className="flex items-center gap-2 mb-1">
                      <span className="font-medium text-text">{key.name}</span>
                      {getStatusBadge(key.status)}
                      {key.testnet && (
                        <span className="px-2 py-0.5 bg-blue-500/10 text-blue-600 text-xs font-medium rounded">
                          Testnet
                        </span>
                      )}
                    </div>
                    <div className="text-sm text-text-muted">
                      <span className="capitalize">{key.exchange}</span>
                      <span className="mx-2">|</span>
                      <span>
                        Permissions: {key.permissions.join(', ') || 'Read'}
                      </span>
                    </div>
                    <div className="text-xs text-text-muted mt-1">
                      Created: {new Date(key.createdAt).toLocaleDateString()}
                      {key.lastUsed && (
                        <>
                          <span className="mx-2">|</span>
                          Last used: {new Date(key.lastUsed).toLocaleDateString()}
                        </>
                      )}
                    </div>
                  </div>

                  <div className="flex items-center gap-2">
                    <Button
                      variant="secondary"
                      size="sm"
                      onClick={() => handleTestKey(key.id)}
                      isLoading={testingKeyId === key.id}
                      disabled={testingKeyId !== null}
                    >
                      Test
                    </Button>
                    <Button
                      variant="danger"
                      size="sm"
                      onClick={() => handleDeleteKey(key.id)}
                      isLoading={deletingKeyId === key.id}
                      disabled={deletingKeyId !== null}
                    >
                      <Trash2 className="w-4 h-4" />
                    </Button>
                  </div>
                </div>

                {/* Test Result */}
                {testResults[key.id] && (
                  <div
                    className={`
                      mt-3 p-3 rounded-lg text-sm
                      ${
                        testResults[key.id].success
                          ? 'bg-green-500/10 text-green-700'
                          : 'bg-red-500/10 text-red-700'
                      }
                    `}
                  >
                    <div className="flex items-center gap-2">
                      {testResults[key.id].success ? (
                        <CheckCircle className="w-4 h-4" />
                      ) : (
                        <AlertTriangle className="w-4 h-4" />
                      )}
                      <span>
                        {testResults[key.id].success
                          ? `Connection successful (${testResults[key.id].latencyMs.toFixed(0)}ms)`
                          : testResults[key.id].error}
                      </span>
                    </div>
                  </div>
                )}
              </div>
            ))}
          </div>
        )}

        {/* Security Notice */}
        <div className="bg-blue-500/10 border border-blue-500/30 rounded-lg p-3 text-sm text-blue-700">
          <p className="font-medium">Security Note</p>
          <p className="text-blue-600 mt-1">
            API keys are stored securely using your system's keychain. Never share your
            API keys with anyone.
          </p>
        </div>
      </div>

      {/* Add Key Modal */}
      <AddKeyModal
        isOpen={showAddModal}
        onClose={() => setShowAddModal(false)}
        onAdd={addAPIKey}
      />
    </Card>
  );
}

// Add Key Modal Component
interface AddKeyModalProps {
  isOpen: boolean;
  onClose: () => void;
  onAdd: (data: {
    exchange: string;
    apiKey: string;
    apiSecret: string;
    passphrase?: string;
    name: string;
    testnet: boolean;
  }) => Promise<{ success: boolean; keyId?: string; error?: string }>;
}

function AddKeyModal({ isOpen, onClose, onAdd }: AddKeyModalProps) {
  const [exchange, setExchange] = useState('binance');
  const [apiKey, setApiKey] = useState('');
  const [apiSecret, setApiSecret] = useState('');
  const [passphrase, setPassphrase] = useState('');
  const [name, setName] = useState('');
  const [testnet, setTestnet] = useState(true);
  const [showSecret, setShowSecret] = useState(false);
  const [isSubmitting, setIsSubmitting] = useState(false);
  const [error, setError] = useState<string | null>(null);

  const selectedExchange = EXCHANGES.find((e) => e.id === exchange);
  const requiresPassphrase = selectedExchange?.requiresPassphrase ?? false;

  const handleSubmit = async (e: React.FormEvent) => {
    e.preventDefault();
    setError(null);
    setIsSubmitting(true);

    try {
      const result = await onAdd({
        exchange,
        apiKey,
        apiSecret,
        passphrase: requiresPassphrase ? passphrase : undefined,
        name: name || 'Default',
        testnet,
      });

      if (result.success) {
        // Reset form and close
        setExchange('binance');
        setApiKey('');
        setApiSecret('');
        setPassphrase('');
        setName('');
        setTestnet(true);
        onClose();
      } else {
        setError(result.error || 'Failed to add API key');
      }
    } catch (err) {
      setError(err instanceof Error ? err.message : 'Failed to add API key');
    } finally {
      setIsSubmitting(false);
    }
  };

  const canSubmit =
    apiKey.length > 0 &&
    apiSecret.length > 0 &&
    (!requiresPassphrase || passphrase.length > 0);

  return (
    <Modal isOpen={isOpen} onClose={onClose} title="Add API Key" size="md">
      <form onSubmit={handleSubmit} className="space-y-4">
        {error && (
          <div className="bg-red-500/10 border border-red-500/30 text-red-700 px-4 py-3 rounded text-sm">
            {error}
          </div>
        )}

        {/* Exchange */}
        <div>
          <label className="block text-sm font-medium text-text mb-1">Exchange</label>
          <select
            value={exchange}
            onChange={(e) => setExchange(e.target.value)}
            className="w-full px-3 py-2 border border-gray-300 rounded-md text-sm focus:outline-none focus:ring-2 focus:ring-primary focus:border-transparent"
          >
            {EXCHANGES.map((ex) => (
              <option key={ex.id} value={ex.id}>
                {ex.name}
              </option>
            ))}
          </select>
        </div>

        {/* Name */}
        <div>
          <label className="block text-sm font-medium text-text mb-1">
            Name (optional)
          </label>
          <input
            type="text"
            value={name}
            onChange={(e) => setName(e.target.value)}
            placeholder="e.g., Main Account"
            className="w-full px-3 py-2 border border-gray-300 rounded-md text-sm focus:outline-none focus:ring-2 focus:ring-primary focus:border-transparent"
          />
        </div>

        {/* API Key */}
        <div>
          <label className="block text-sm font-medium text-text mb-1">API Key</label>
          <input
            type="text"
            value={apiKey}
            onChange={(e) => setApiKey(e.target.value)}
            placeholder="Enter your API key"
            className="w-full px-3 py-2 border border-gray-300 rounded-md text-sm font-mono focus:outline-none focus:ring-2 focus:ring-primary focus:border-transparent"
          />
        </div>

        {/* API Secret */}
        <div>
          <label className="block text-sm font-medium text-text mb-1">API Secret</label>
          <div className="relative">
            <input
              type={showSecret ? 'text' : 'password'}
              value={apiSecret}
              onChange={(e) => setApiSecret(e.target.value)}
              placeholder="Enter your API secret"
              className="w-full px-3 py-2 pr-10 border border-gray-300 rounded-md text-sm font-mono focus:outline-none focus:ring-2 focus:ring-primary focus:border-transparent"
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

        {/* Passphrase (OKX) */}
        {requiresPassphrase && (
          <div>
            <label className="block text-sm font-medium text-text mb-1">
              Passphrase
            </label>
            <input
              type="password"
              value={passphrase}
              onChange={(e) => setPassphrase(e.target.value)}
              placeholder="Enter your passphrase"
              className="w-full px-3 py-2 border border-gray-300 rounded-md text-sm font-mono focus:outline-none focus:ring-2 focus:ring-primary focus:border-transparent"
            />
          </div>
        )}

        {/* Testnet Toggle */}
        <div className="flex items-center gap-3">
          <label className="relative inline-flex items-center cursor-pointer">
            <input
              type="checkbox"
              checked={testnet}
              onChange={(e) => setTestnet(e.target.checked)}
              className="sr-only peer"
            />
            <div className="w-11 h-6 bg-gray-200 peer-focus:outline-none peer-focus:ring-4 peer-focus:ring-primary/20 rounded-full peer peer-checked:after:translate-x-full peer-checked:after:border-white after:content-[''] after:absolute after:top-[2px] after:left-[2px] after:bg-white after:border-gray-300 after:border after:rounded-full after:h-5 after:w-5 after:transition-all peer-checked:bg-primary"></div>
          </label>
          <span className="text-sm text-text">Use Testnet</span>
        </div>

        {/* Security Warning */}
        <div className="bg-yellow-500/10 border border-yellow-500/30 rounded-lg p-3 text-sm text-yellow-700">
          <p className="font-medium">Important</p>
          <ul className="text-yellow-600 mt-1 space-y-1 text-xs">
            <li>Never enable withdrawal permissions on your API key</li>
            <li>Enable IP restrictions on your exchange for added security</li>
          </ul>
        </div>

        {/* Actions */}
        <div className="flex justify-end gap-3 pt-4">
          <Button variant="secondary" onClick={onClose} type="button">
            Cancel
          </Button>
          <Button
            variant="primary"
            type="submit"
            disabled={!canSubmit}
            isLoading={isSubmitting}
          >
            Add API Key
          </Button>
        </div>
      </form>
    </Modal>
  );
}
