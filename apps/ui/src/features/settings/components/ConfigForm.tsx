import { useState, useEffect } from 'react';
import { Card } from '@/shared/components';
import { useConfig } from '../hooks/useConfig';
import type { ConfigFormData } from '../types/config';

const EXECUTION_MODES = [
  { value: 'sim_engine', label: 'Simulation' },
  { value: 'binance_testnet_spot', label: 'Binance Testnet' },
  { value: 'binance_spot', label: 'Binance Production' },
  { value: 'okx_testnet', label: 'OKX Testnet' },
  { value: 'okx_spot', label: 'OKX Production' },
  { value: 'bybit_testnet', label: 'Bybit Testnet' },
  { value: 'bybit_spot', label: 'Bybit Production' },
  { value: 'coinbase_sandbox', label: 'Coinbase Sandbox' },
  { value: 'coinbase_spot', label: 'Coinbase Production' },
];

const MARKET_SOURCES = [
  { value: 'sim', label: 'Simulation' },
  { value: 'binance_rest', label: 'Binance REST' },
  { value: 'okx_rest', label: 'OKX REST' },
  { value: 'bybit_rest', label: 'Bybit REST' },
  { value: 'coinbase_rest', label: 'Coinbase REST' },
];

export default function ConfigForm() {
  const { config, isLoading, updateConfig, isUpdating, updateSuccess, updateError } = useConfig();
  const [formData, setFormData] = useState<Partial<ConfigFormData>>({});
  const [showSecrets, setShowSecrets] = useState(false);
  const [activeTab, setActiveTab] = useState<'general' | 'binance' | 'okx' | 'bybit' | 'coinbase'>('general');

  useEffect(() => {
    if (config) {
      setFormData({
        host: config.host || '0.0.0.0',
        port: String(config.port || 8080),
        preset: config.preset || 'dev',
        marketSource: config.market_source || 'sim',
        marketSymbol: config.market_symbol || 'BTCUSDT',
        executionMode: config.execution_mode || 'sim_engine',
        binanceBaseUrl: config.binance_base_url || 'https://api.binance.com',
        binanceWsBaseUrl: config.binance_ws_base_url || 'wss://testnet.binance.vision/ws',
        binanceTradeBaseUrl: config.binance_trade_base_url || 'https://testnet.binance.vision',
        binanceApiKey: config.binance_api_key || '',
        binanceApiSecret: '', // Never show secrets
        okxApiKey: config.okx_api_key || '',
        okxApiSecret: '',
        okxPassphrase: '',
        okxBaseUrl: config.okx_base_url || 'https://www.okx.com',
        okxWsUrl: config.okx_ws_url || 'wss://ws.okx.com:8443/ws/v5/public',
        okxTradeUrl: config.okx_trade_url || 'https://www.okx.com',
        bybitApiKey: config.bybit_api_key || '',
        bybitApiSecret: '',
        bybitBaseUrl: config.bybit_base_url || 'https://api.bybit.com',
        bybitWsUrl: config.bybit_ws_url || 'wss://stream.bybit.com/v5/public/spot',
        bybitTradeUrl: config.bybit_trade_url || 'https://api.bybit.com',
        coinbaseApiKey: config.coinbase_api_key || '',
        coinbaseApiSecret: '',
        coinbaseBaseUrl: config.coinbase_base_url || 'https://api.coinbase.com',
        coinbaseWsUrl: config.coinbase_ws_url || 'wss://ws-feed.exchange.coinbase.com',
      });
    }
  }, [config]);

  const handleSubmit = (e: React.FormEvent) => {
    e.preventDefault();
    updateConfig(formData);
  };

  const handleChange = (field: keyof ConfigFormData, value: string) => {
    setFormData((prev) => ({ ...prev, [field]: value }));
  };

  if (isLoading) {
    return (
      <div className="flex items-center justify-center p-8">
        <div className="text-text-muted">Loading configuration...</div>
      </div>
    );
  }

  return (
    <form onSubmit={handleSubmit} className="space-y-6">
      {/* Success/Error Messages */}
      {updateSuccess && (
        <div className="bg-green-500/10 border border-green-500/50 text-green-500 px-4 py-3 rounded">
          Configuration updated successfully! Restart the gateway for changes to take effect.
        </div>
      )}
      {updateError && (
        <div className="bg-red-500/10 border border-red-500/50 text-red-500 px-4 py-3 rounded">
          Failed to update configuration: {(updateError as Error).message}
        </div>
      )}

      {/* Tabs */}
      <div className="flex space-x-2 border-b border-border">
        {(['general', 'binance', 'okx', 'bybit', 'coinbase'] as const).map((tab) => (
          <button
            key={tab}
            type="button"
            onClick={() => setActiveTab(tab)}
            className={`px-4 py-2 font-medium transition-colors ${
              activeTab === tab
                ? 'text-primary border-b-2 border-primary'
                : 'text-text-muted hover:text-text'
            }`}
          >
            {tab.charAt(0).toUpperCase() + tab.slice(1)}
          </button>
        ))}
      </div>

      {/* General Settings */}
      {activeTab === 'general' && (
        <Card title="General Settings">
          <div className="space-y-4">
            <div className="grid grid-cols-1 md:grid-cols-2 gap-4">
              <div>
                <label className="block text-sm font-medium text-text mb-1">
                  Host
                </label>
                <input
                  type="text"
                  value={formData.host || ''}
                  onChange={(e) => handleChange('host', e.target.value)}
                  className="w-full px-3 py-2 bg-surface border border-border rounded text-text"
                  placeholder="0.0.0.0"
                />
              </div>

              <div>
                <label className="block text-sm font-medium text-text mb-1">
                  Port
                </label>
                <input
                  type="number"
                  value={formData.port || ''}
                  onChange={(e) => handleChange('port', e.target.value)}
                  className="w-full px-3 py-2 bg-surface border border-border rounded text-text"
                  placeholder="8080"
                />
              </div>
            </div>

            <div>
              <label className="block text-sm font-medium text-text mb-1">
                Build Preset
              </label>
              <select
                value={formData.preset || 'dev'}
                onChange={(e) => handleChange('preset', e.target.value)}
                className="w-full px-3 py-2 bg-surface border border-border rounded text-text"
              >
                <option value="dev">Development</option>
                <option value="release">Release</option>
                <option value="asan">ASan (Debug)</option>
              </select>
            </div>

            <div>
              <label className="block text-sm font-medium text-text mb-1">
                Market Data Source
              </label>
              <select
                value={formData.marketSource || 'sim'}
                onChange={(e) => handleChange('marketSource', e.target.value)}
                className="w-full px-3 py-2 bg-surface border border-border rounded text-text"
              >
                {MARKET_SOURCES.map((source) => (
                  <option key={source.value} value={source.value}>
                    {source.label}
                  </option>
                ))}
              </select>
            </div>

            <div>
              <label className="block text-sm font-medium text-text mb-1">
                Trading Symbol
              </label>
              <input
                type="text"
                value={formData.marketSymbol || ''}
                onChange={(e) => handleChange('marketSymbol', e.target.value)}
                className="w-full px-3 py-2 bg-surface border border-border rounded text-text"
                placeholder="BTCUSDT"
              />
            </div>

            <div>
              <label className="block text-sm font-medium text-text mb-1">
                Execution Mode
              </label>
              <select
                value={formData.executionMode || 'sim_engine'}
                onChange={(e) => handleChange('executionMode', e.target.value)}
                className="w-full px-3 py-2 bg-surface border border-border rounded text-text"
              >
                {EXECUTION_MODES.map((mode) => (
                  <option key={mode.value} value={mode.value}>
                    {mode.label}
                  </option>
                ))}
              </select>
              <p className="text-xs text-text-muted mt-1">
                ⚠️ Production modes use real funds. Use testnet for testing.
              </p>
            </div>
          </div>
        </Card>
      )}

      {/* Binance Settings */}
      {activeTab === 'binance' && (
        <Card title="Binance Configuration">
          <div className="space-y-4">
            <div>
              <label className="block text-sm font-medium text-text mb-1">
                Base URL (Market Data)
              </label>
              <input
                type="text"
                value={formData.binanceBaseUrl || ''}
                onChange={(e) => handleChange('binanceBaseUrl', e.target.value)}
                className="w-full px-3 py-2 bg-surface border border-border rounded text-text font-mono text-sm"
                placeholder="https://api.binance.com"
              />
            </div>

            <div>
              <label className="block text-sm font-medium text-text mb-1">
                Trade Base URL
              </label>
              <input
                type="text"
                value={formData.binanceTradeBaseUrl || ''}
                onChange={(e) => handleChange('binanceTradeBaseUrl', e.target.value)}
                className="w-full px-3 py-2 bg-surface border border-border rounded text-text font-mono text-sm"
                placeholder="https://testnet.binance.vision"
              />
            </div>

            <div>
              <label className="block text-sm font-medium text-text mb-1">
                WebSocket Base URL
              </label>
              <input
                type="text"
                value={formData.binanceWsBaseUrl || ''}
                onChange={(e) => handleChange('binanceWsBaseUrl', e.target.value)}
                className="w-full px-3 py-2 bg-surface border border-border rounded text-text font-mono text-sm"
                placeholder="wss://testnet.binance.vision/ws"
              />
            </div>

            <div>
              <label className="block text-sm font-medium text-text mb-1">
                API Key
              </label>
              <input
                type={showSecrets ? 'text' : 'password'}
                value={formData.binanceApiKey || ''}
                onChange={(e) => handleChange('binanceApiKey', e.target.value)}
                className="w-full px-3 py-2 bg-surface border border-border rounded text-text font-mono text-sm"
                placeholder="Enter Binance API Key"
              />
            </div>

            <div>
              <label className="block text-sm font-medium text-text mb-1">
                API Secret
              </label>
              <input
                type={showSecrets ? 'text' : 'password'}
                value={formData.binanceApiSecret || ''}
                onChange={(e) => handleChange('binanceApiSecret', e.target.value)}
                className="w-full px-3 py-2 bg-surface border border-border rounded text-text font-mono text-sm"
                placeholder="Enter Binance API Secret"
              />
            </div>

            <div className="flex items-center">
              <input
                type="checkbox"
                id="showSecrets"
                checked={showSecrets}
                onChange={(e) => setShowSecrets(e.target.checked)}
                className="mr-2"
              />
              <label htmlFor="showSecrets" className="text-sm text-text-muted">
                Show API secrets
              </label>
            </div>

            {config?.binance_trade_enabled && (
              <div className="bg-green-500/10 border border-green-500/50 text-green-500 px-4 py-2 rounded text-sm">
                ✓ Binance trading is enabled
                {config.binance_user_stream_connected && ' • User stream connected'}
              </div>
            )}
          </div>
        </Card>
      )}

      {/* OKX Settings */}
      {activeTab === 'okx' && (
        <Card title="OKX Configuration">
          <div className="space-y-4">
            <div className="bg-blue-500/10 border border-blue-500/50 text-blue-500 px-4 py-2 rounded text-sm">
              ℹ️ OKX requires a passphrase in addition to API key and secret
            </div>

            <div>
              <label className="block text-sm font-medium text-text mb-1">
                Base URL
              </label>
              <input
                type="text"
                value={formData.okxBaseUrl || ''}
                onChange={(e) => handleChange('okxBaseUrl', e.target.value)}
                className="w-full px-3 py-2 bg-surface border border-border rounded text-text font-mono text-sm"
                placeholder="https://www.okx.com"
              />
            </div>

            <div>
              <label className="block text-sm font-medium text-text mb-1">
                API Key
              </label>
              <input
                type={showSecrets ? 'text' : 'password'}
                value={formData.okxApiKey || ''}
                onChange={(e) => handleChange('okxApiKey', e.target.value)}
                className="w-full px-3 py-2 bg-surface border border-border rounded text-text font-mono text-sm"
                placeholder="Enter OKX API Key"
              />
            </div>

            <div>
              <label className="block text-sm font-medium text-text mb-1">
                API Secret
              </label>
              <input
                type={showSecrets ? 'text' : 'password'}
                value={formData.okxApiSecret || ''}
                onChange={(e) => handleChange('okxApiSecret', e.target.value)}
                className="w-full px-3 py-2 bg-surface border border-border rounded text-text font-mono text-sm"
                placeholder="Enter OKX API Secret"
              />
            </div>

            <div>
              <label className="block text-sm font-medium text-text mb-1">
                Passphrase
              </label>
              <input
                type={showSecrets ? 'text' : 'password'}
                value={formData.okxPassphrase || ''}
                onChange={(e) => handleChange('okxPassphrase', e.target.value)}
                className="w-full px-3 py-2 bg-surface border border-border rounded text-text font-mono text-sm"
                placeholder="Enter OKX Passphrase"
              />
            </div>
          </div>
        </Card>
      )}

      {/* Bybit Settings */}
      {activeTab === 'bybit' && (
        <Card title="Bybit Configuration">
          <div className="space-y-4">
            <div>
              <label className="block text-sm font-medium text-text mb-1">
                Base URL
              </label>
              <input
                type="text"
                value={formData.bybitBaseUrl || ''}
                onChange={(e) => handleChange('bybitBaseUrl', e.target.value)}
                className="w-full px-3 py-2 bg-surface border border-border rounded text-text font-mono text-sm"
                placeholder="https://api.bybit.com"
              />
            </div>

            <div>
              <label className="block text-sm font-medium text-text mb-1">
                API Key
              </label>
              <input
                type={showSecrets ? 'text' : 'password'}
                value={formData.bybitApiKey || ''}
                onChange={(e) => handleChange('bybitApiKey', e.target.value)}
                className="w-full px-3 py-2 bg-surface border border-border rounded text-text font-mono text-sm"
                placeholder="Enter Bybit API Key"
              />
            </div>

            <div>
              <label className="block text-sm font-medium text-text mb-1">
                API Secret
              </label>
              <input
                type={showSecrets ? 'text' : 'password'}
                value={formData.bybitApiSecret || ''}
                onChange={(e) => handleChange('bybitApiSecret', e.target.value)}
                className="w-full px-3 py-2 bg-surface border border-border rounded text-text font-mono text-sm"
                placeholder="Enter Bybit API Secret"
              />
            </div>
          </div>
        </Card>
      )}

      {/* Coinbase Settings */}
      {activeTab === 'coinbase' && (
        <Card title="Coinbase Configuration">
          <div className="space-y-4">
            <div>
              <label className="block text-sm font-medium text-text mb-1">
                Base URL
              </label>
              <input
                type="text"
                value={formData.coinbaseBaseUrl || ''}
                onChange={(e) => handleChange('coinbaseBaseUrl', e.target.value)}
                className="w-full px-3 py-2 bg-surface border border-border rounded text-text font-mono text-sm"
                placeholder="https://api.coinbase.com"
              />
            </div>

            <div>
              <label className="block text-sm font-medium text-text mb-1">
                API Key
              </label>
              <input
                type={showSecrets ? 'text' : 'password'}
                value={formData.coinbaseApiKey || ''}
                onChange={(e) => handleChange('coinbaseApiKey', e.target.value)}
                className="w-full px-3 py-2 bg-surface border border-border rounded text-text font-mono text-sm"
                placeholder="Enter Coinbase API Key"
              />
            </div>

            <div>
              <label className="block text-sm font-medium text-text mb-1">
                API Secret
              </label>
              <input
                type={showSecrets ? 'text' : 'password'}
                value={formData.coinbaseApiSecret || ''}
                onChange={(e) => handleChange('coinbaseApiSecret', e.target.value)}
                className="w-full px-3 py-2 bg-surface border border-border rounded text-text font-mono text-sm"
                placeholder="Enter Coinbase API Secret"
              />
            </div>
          </div>
        </Card>
      )}

      {/* Action Buttons */}
      <div className="flex justify-end space-x-4">
        <button
          type="button"
          onClick={() => window.location.reload()}
          className="px-4 py-2 text-text-muted hover:text-text transition-colors"
        >
          Cancel
        </button>
        <button
          type="submit"
          disabled={isUpdating}
          className="px-6 py-2 bg-primary text-white rounded hover:bg-primary-dark transition-colors disabled:opacity-50 disabled:cursor-not-allowed"
        >
          {isUpdating ? 'Saving...' : 'Save Configuration'}
        </button>
      </div>

      {/* Warning */}
      <div className="bg-yellow-500/10 border border-yellow-500/50 text-yellow-500 px-4 py-3 rounded text-sm">
        <strong>⚠️ Important:</strong> Configuration changes require restarting the gateway to take effect.
        Run <code className="bg-surface px-2 py-1 rounded">./scripts/run_gateway.sh dev</code> after saving.
      </div>
    </form>
  );
}
