import { useCallback } from 'react';
import { useNavigate } from 'react-router-dom';
import { Modal } from '@/shared/components';
import { useConfigStore } from '../../store/configStore';
import { WizardProgress } from './WizardProgress';
import {
  WelcomeStep,
  ExchangeStep,
  RiskStep,
  TradingModeStep,
  SecurityStep,
  CompleteStep,
} from './steps';

interface SetupWizardProps {
  isOpen: boolean;
  onClose: () => void;
}

const WIZARD_STEPS = [
  { id: 'welcome', title: 'Welcome' },
  { id: 'exchange', title: 'Exchange' },
  { id: 'risk', title: 'Risk' },
  { id: 'mode', title: 'Mode' },
  { id: 'security', title: 'Security' },
  { id: 'complete', title: 'Complete' },
];

export function SetupWizard({ isOpen, onClose }: SetupWizardProps) {
  const navigate = useNavigate();
  const {
    currentStep,
    wizardData,
    setWizardData,
    nextStep,
    prevStep,
    completeWizard,
    testNewConnection,
    isLoading,
  } = useConfigStore();

  const handleSkipWizard = useCallback(() => {
    onClose();
    navigate('/dashboard');
  }, [onClose, navigate]);

  const handleTestConnection = useCallback(async () => {
    if (!wizardData.exchange || !wizardData.apiKey || !wizardData.apiSecret) {
      return {
        success: false,
        exchange: wizardData.exchange || '',
        permissions: [],
        balanceAvailable: false,
        latencyMs: 0,
        error: 'Missing credentials',
        warnings: [],
      };
    }

    return testNewConnection({
      exchange: wizardData.exchange,
      apiKey: wizardData.apiKey,
      apiSecret: wizardData.apiSecret,
      passphrase: wizardData.passphrase,
      testnet: wizardData.testnet ?? true,
    });
  }, [wizardData, testNewConnection]);

  const handleComplete = useCallback(async () => {
    try {
      await completeWizard();
      // Stay on complete step to show success
    } catch (error) {
      console.error('Failed to complete wizard:', error);
    }
  }, [completeWizard]);

  const handleStartSimulated = useCallback(() => {
    onClose();
    navigate('/dashboard');
  }, [onClose, navigate]);

  const handleStartLive = useCallback(() => {
    onClose();
    navigate('/dashboard');
  }, [onClose, navigate]);

  const handleExploreStrategies = useCallback(() => {
    onClose();
    navigate('/strategies');
  }, [onClose, navigate]);

  const handleViewDocs = useCallback(() => {
    window.open('https://docs.veloz.trade', '_blank');
  }, []);

  const renderStep = () => {
    switch (currentStep) {
      case 0:
        return (
          <WelcomeStep
            termsAccepted={wizardData.termsAccepted ?? false}
            onTermsChange={(accepted) => setWizardData({ termsAccepted: accepted })}
            onNext={nextStep}
            onSkip={handleSkipWizard}
          />
        );

      case 1:
        return (
          <ExchangeStep
            exchange={wizardData.exchange ?? 'binance'}
            apiKey={wizardData.apiKey ?? ''}
            apiSecret={wizardData.apiSecret ?? ''}
            passphrase={wizardData.passphrase ?? ''}
            testnet={wizardData.testnet ?? true}
            onExchangeChange={(exchange) =>
              setWizardData({
                exchange: exchange as 'binance' | 'binance_futures' | 'okx' | 'bybit' | 'coinbase',
              })
            }
            onApiKeyChange={(apiKey) => setWizardData({ apiKey })}
            onApiSecretChange={(apiSecret) => setWizardData({ apiSecret })}
            onPassphraseChange={(passphrase) => setWizardData({ passphrase })}
            onTestnetChange={(testnet) => setWizardData({ testnet })}
            onTestConnection={handleTestConnection}
            onNext={nextStep}
            onBack={prevStep}
          />
        );

      case 2:
        return (
          <RiskStep
            maxPositionSize={wizardData.maxPositionSize ?? 0.1}
            dailyLossLimit={wizardData.dailyLossLimit ?? 0.05}
            circuitBreakerEnabled={wizardData.circuitBreakerEnabled ?? true}
            circuitBreakerThreshold={wizardData.circuitBreakerThreshold ?? 0.1}
            onMaxPositionSizeChange={(value) => setWizardData({ maxPositionSize: value })}
            onDailyLossLimitChange={(value) => setWizardData({ dailyLossLimit: value })}
            onCircuitBreakerEnabledChange={(enabled) =>
              setWizardData({ circuitBreakerEnabled: enabled })
            }
            onCircuitBreakerThresholdChange={(value) =>
              setWizardData({ circuitBreakerThreshold: value })
            }
            onNext={nextStep}
            onBack={prevStep}
          />
        );

      case 3:
        return (
          <TradingModeStep
            tradingMode={wizardData.tradingMode ?? 'simulated'}
            simulatedBalance={wizardData.simulatedBalance ?? 10000}
            onTradingModeChange={(mode) => setWizardData({ tradingMode: mode })}
            onSimulatedBalanceChange={(balance) =>
              setWizardData({ simulatedBalance: balance })
            }
            onNext={nextStep}
            onBack={prevStep}
          />
        );

      case 4:
        return (
          <SecurityStep
            twoFactorEnabled={wizardData.twoFactorEnabled ?? false}
            recoveryEmail={wizardData.recoveryEmail ?? ''}
            onTwoFactorEnabledChange={(enabled) =>
              setWizardData({ twoFactorEnabled: enabled })
            }
            onRecoveryEmailChange={(email) => setWizardData({ recoveryEmail: email })}
            onNext={handleComplete}
            onBack={prevStep}
            onSkip={handleComplete}
          />
        );

      case 5:
        return (
          <CompleteStep
            tradingMode={wizardData.tradingMode ?? 'simulated'}
            onStartSimulated={handleStartSimulated}
            onStartLive={handleStartLive}
            onExploreStrategies={handleExploreStrategies}
            onViewDocs={handleViewDocs}
          />
        );

      default:
        return null;
    }
  };

  return (
    <Modal
      isOpen={isOpen}
      onClose={onClose}
      title=""
      size="lg"
      showCloseButton={currentStep === 0}
    >
      <div className="min-h-[600px] flex flex-col">
        {/* Progress indicator (hide on welcome and complete) */}
        {currentStep > 0 && currentStep < 5 && (
          <div className="px-6 pt-4 border-b border-border">
            <WizardProgress currentStep={currentStep} steps={WIZARD_STEPS} />
          </div>
        )}

        {/* Step content */}
        <div className="flex-1 px-6 py-4 overflow-y-auto">
          {isLoading ? (
            <div className="flex items-center justify-center h-full">
              <div className="text-center">
                <div className="animate-spin rounded-full h-12 w-12 border-b-2 border-primary mx-auto mb-4"></div>
                <p className="text-text-muted">Saving configuration...</p>
              </div>
            </div>
          ) : (
            renderStep()
          )}
        </div>
      </div>
    </Modal>
  );
}
