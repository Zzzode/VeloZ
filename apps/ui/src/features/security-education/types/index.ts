/**
 * Security Education System Types
 */

// Trading mode types
export type TradingMode = 'simulated' | 'paper' | 'live';

// Onboarding step types
export type OnboardingStep =
  | 'welcome'
  | 'api-security'
  | 'risk-awareness'
  | 'trading-modes'
  | 'safety-checklist';

// Onboarding progress
export interface OnboardingProgress {
  currentStep: OnboardingStep;
  completedSteps: OnboardingStep[];
  isComplete: boolean;
  startedAt: string | null;
  completedAt: string | null;
}

// Safety checklist item
export interface SafetyChecklistItem {
  id: string;
  label: string;
  checked: boolean;
  required: boolean;
}

// Loss limit configuration
export interface LossLimitConfig {
  dailyLimitPercent: number;
  dailyLimitAmount: number;
  weeklyLimitPercent: number;
  weeklyLimitAmount: number;
  enabled: boolean;
}

// Loss tracking
export interface LossTracking {
  dailyLoss: number;
  dailyLossPercent: number;
  weeklyLoss: number;
  weeklyLossPercent: number;
  lastResetDate: string;
  weekStartDate: string;
}

// Anomaly types
export type AnomalyType =
  | 'unusual-volume'
  | 'unusual-size'
  | 'rapid-loss'
  | 'revenge-trading'
  | 'off-hours';

// Anomaly alert
export interface AnomalyAlert {
  id: string;
  type: AnomalyType;
  severity: 'warning' | 'critical';
  message: string;
  details: string;
  timestamp: string;
  acknowledged: boolean;
}

// Emergency stop state
export interface EmergencyStopState {
  isActive: boolean;
  activatedAt: string | null;
  activatedBy: string | null;
  reason: string | null;
  cancelledOrders: number;
}

// Security education state
export interface SecurityEducationState {
  // Onboarding
  onboarding: OnboardingProgress;
  safetyChecklist: SafetyChecklistItem[];

  // Trading mode
  tradingMode: TradingMode;
  tradingModeLockedUntil: string | null;
  simulatedTradesCompleted: number;
  simulatedDaysCompleted: number;

  // Loss limits
  lossLimits: LossLimitConfig;
  lossTracking: LossTracking;

  // Anomaly detection
  anomalyAlerts: AnomalyAlert[];
  anomalyDetectionEnabled: boolean;

  // Emergency stop
  emergencyStop: EmergencyStopState;
}

// Risk settings
export interface RiskSettings {
  maxPositionSize: number;
  maxPositionPercent: number;
  maxLeverage: number;
  orderConfirmationThreshold: number;
}

// Alert notification types
export type AlertChannel = 'email' | 'sms' | 'push';

export interface AlertNotificationSettings {
  channels: AlertChannel[];
  emailAddress: string | null;
  phoneNumber: string | null;
  criticalAlertsOnly: boolean;
}
