import { useState, useRef } from 'react';
import { Button, Card, Modal } from '@/shared/components';
import { useConfigStore } from '../../store/configStore';
import { Download, Upload, FileText, CheckCircle, AlertTriangle } from 'lucide-react';

export function ImportExport() {
  const { exportConfig, importConfig } = useConfigStore();
  const [isExporting, setIsExporting] = useState(false);
  const [isImporting, setIsImporting] = useState(false);
  const [showImportModal, setShowImportModal] = useState(false);
  const [importData, setImportData] = useState('');
  const [importError, setImportError] = useState<string | null>(null);
  const [importSuccess, setImportSuccess] = useState(false);
  const [exportSuccess, setExportSuccess] = useState(false);
  const fileInputRef = useRef<HTMLInputElement>(null);

  const handleExport = async () => {
    setIsExporting(true);
    setExportSuccess(false);

    try {
      const configJson = await exportConfig(false);

      // Create and download file
      const blob = new Blob([configJson], { type: 'application/json' });
      const url = URL.createObjectURL(blob);
      const a = document.createElement('a');
      a.href = url;
      a.download = `veloz-config-${new Date().toISOString().split('T')[0]}.json`;
      document.body.appendChild(a);
      a.click();
      document.body.removeChild(a);
      URL.revokeObjectURL(url);

      setExportSuccess(true);
      setTimeout(() => setExportSuccess(false), 3000);
    } catch (error) {
      console.error('Export failed:', error);
    } finally {
      setIsExporting(false);
    }
  };

  const handleFileSelect = (e: React.ChangeEvent<HTMLInputElement>) => {
    const file = e.target.files?.[0];
    if (!file) return;

    const reader = new FileReader();
    reader.onload = (event) => {
      const content = event.target?.result as string;
      setImportData(content);
      setShowImportModal(true);
      setImportError(null);
    };
    reader.readAsText(file);

    // Reset file input
    if (fileInputRef.current) {
      fileInputRef.current.value = '';
    }
  };

  const handleImport = async () => {
    setIsImporting(true);
    setImportError(null);

    try {
      const success = await importConfig(importData);
      if (success) {
        setImportSuccess(true);
        setShowImportModal(false);
        setTimeout(() => setImportSuccess(false), 3000);
      } else {
        setImportError('Invalid configuration format');
      }
    } catch (error) {
      setImportError(
        error instanceof Error ? error.message : 'Failed to import configuration'
      );
    } finally {
      setIsImporting(false);
    }
  };

  const validateImportData = () => {
    try {
      JSON.parse(importData);
      return true;
    } catch {
      return false;
    }
  };

  return (
    <Card title="Import / Export">
      <div className="space-y-6">
        {/* Success Messages */}
        {exportSuccess && (
          <div className="flex items-center gap-2 bg-green-500/10 border border-green-500/50 text-green-700 px-4 py-3 rounded">
            <CheckCircle className="w-5 h-5" />
            Configuration exported successfully!
          </div>
        )}
        {importSuccess && (
          <div className="flex items-center gap-2 bg-green-500/10 border border-green-500/50 text-green-700 px-4 py-3 rounded">
            <CheckCircle className="w-5 h-5" />
            Configuration imported successfully!
          </div>
        )}

        {/* Export Section */}
        <div className="border border-border rounded-lg p-4">
          <div className="flex items-start gap-4">
            <div className="w-10 h-10 bg-primary/10 rounded-lg flex items-center justify-center flex-shrink-0">
              <Download className="w-5 h-5 text-primary" />
            </div>
            <div className="flex-1">
              <h3 className="font-medium text-text">Export Configuration</h3>
              <p className="text-sm text-text-muted mt-1">
                Download your current settings as a JSON file. API keys and secrets are
                not included for security.
              </p>
              <Button
                variant="secondary"
                size="sm"
                className="mt-3"
                onClick={handleExport}
                isLoading={isExporting}
                leftIcon={<Download className="w-4 h-4" />}
              >
                Export Settings
              </Button>
            </div>
          </div>
        </div>

        {/* Import Section */}
        <div className="border border-border rounded-lg p-4">
          <div className="flex items-start gap-4">
            <div className="w-10 h-10 bg-primary/10 rounded-lg flex items-center justify-center flex-shrink-0">
              <Upload className="w-5 h-5 text-primary" />
            </div>
            <div className="flex-1">
              <h3 className="font-medium text-text">Import Configuration</h3>
              <p className="text-sm text-text-muted mt-1">
                Restore settings from a previously exported configuration file.
              </p>
              <input
                ref={fileInputRef}
                type="file"
                accept=".json"
                onChange={handleFileSelect}
                className="hidden"
              />
              <Button
                variant="secondary"
                size="sm"
                className="mt-3"
                onClick={() => fileInputRef.current?.click()}
                leftIcon={<Upload className="w-4 h-4" />}
              >
                Import Settings
              </Button>
            </div>
          </div>
        </div>

        {/* Warning */}
        <div className="bg-yellow-500/10 border border-yellow-500/30 rounded-lg p-3 text-sm text-yellow-700">
          <div className="flex items-start gap-2">
            <AlertTriangle className="w-5 h-5 flex-shrink-0 mt-0.5" />
            <div>
              <p className="font-medium">Important</p>
              <p className="text-yellow-600 mt-1">
                Importing a configuration will overwrite your current settings. Make
                sure to export your current configuration first if you want to keep a
                backup.
              </p>
            </div>
          </div>
        </div>
      </div>

      {/* Import Preview Modal */}
      <Modal
        isOpen={showImportModal}
        onClose={() => setShowImportModal(false)}
        title="Import Configuration"
        size="md"
      >
        <div className="space-y-4">
          {importError && (
            <div className="bg-red-500/10 border border-red-500/30 text-red-700 px-4 py-3 rounded text-sm">
              {importError}
            </div>
          )}

          <div className="flex items-start gap-3 p-4 bg-surface border border-border rounded-lg">
            <FileText className="w-6 h-6 text-text-muted flex-shrink-0" />
            <div className="flex-1 min-w-0">
              <p className="font-medium text-text">Configuration Preview</p>
              <pre className="mt-2 p-3 bg-gray-100 rounded text-xs overflow-auto max-h-60 text-text-muted">
                {importData.substring(0, 1000)}
                {importData.length > 1000 && '...'}
              </pre>
            </div>
          </div>

          <div className="bg-yellow-500/10 border border-yellow-500/30 rounded-lg p-3 text-sm text-yellow-700">
            <p>
              This will replace your current configuration. API keys will need to be
              re-entered after import.
            </p>
          </div>

          <div className="flex justify-end gap-3 pt-4">
            <Button variant="secondary" onClick={() => setShowImportModal(false)}>
              Cancel
            </Button>
            <Button
              variant="primary"
              onClick={handleImport}
              isLoading={isImporting}
              disabled={!validateImportData()}
            >
              Import Configuration
            </Button>
          </div>
        </div>
      </Modal>
    </Card>
  );
}
