import { describe, it, expect, vi, beforeEach } from 'vitest';
import { render, screen, fireEvent } from '@testing-library/react';
import { OnboardingWizard } from '../components/OnboardingWizard';
import { useSecurityEducationStore } from '../store';

// Mock the store
vi.mock('../store', async () => {
  const actual = await vi.importActual('../store');
  return {
    ...actual,
    useSecurityEducationStore: vi.fn(),
  };
});

const mockStore = {
  onboarding: {
    currentStep: 'welcome',
    completedSteps: [],
    isComplete: false,
    startedAt: null,
    completedAt: null,
  },
  safetyChecklist: [
    { id: 'test-1', label: 'Test item 1', checked: false, required: true },
    { id: 'test-2', label: 'Test item 2', checked: false, required: true },
  ],
  startOnboarding: vi.fn(),
  completeStep: vi.fn(),
  goToNextStep: vi.fn(),
  goToPreviousStep: vi.fn(),
  toggleChecklistItem: vi.fn(),
  isChecklistComplete: vi.fn(() => false),
};

describe('OnboardingWizard', () => {
  beforeEach(() => {
    vi.clearAllMocks();
    (useSecurityEducationStore as unknown as ReturnType<typeof vi.fn>).mockReturnValue(mockStore);
  });

  it('should render welcome step by default', () => {
    render(<OnboardingWizard />);

    expect(screen.getByText('Your Security Matters')).toBeInTheDocument();
  });

  it('should call startOnboarding on mount', () => {
    render(<OnboardingWizard />);

    expect(mockStore.startOnboarding).toHaveBeenCalled();
  });

  it('should show progress indicator', () => {
    render(<OnboardingWizard />);

    expect(screen.getByText('Welcome')).toBeInTheDocument();
    expect(screen.getByText('API')).toBeInTheDocument();
    expect(screen.getByText('Risk')).toBeInTheDocument();
    expect(screen.getByText('Modes')).toBeInTheDocument();
    expect(screen.getByText('Checklist')).toBeInTheDocument();
  });

  it('should navigate to next step when Continue is clicked', () => {
    render(<OnboardingWizard />);

    fireEvent.click(screen.getByText('Continue'));

    expect(mockStore.completeStep).toHaveBeenCalledWith('welcome');
    expect(mockStore.goToNextStep).toHaveBeenCalled();
  });

  it('should show Skip for Now on first step', () => {
    const onSkip = vi.fn();
    render(<OnboardingWizard onSkip={onSkip} />);

    expect(screen.getByText('Skip for Now')).toBeInTheDocument();
  });

  it('should call onSkip when Skip for Now is clicked', () => {
    const onSkip = vi.fn();
    render(<OnboardingWizard onSkip={onSkip} />);

    fireEvent.click(screen.getByText('Skip for Now'));

    expect(onSkip).toHaveBeenCalled();
  });

  it('should show Back button on non-first steps', () => {
    (useSecurityEducationStore as unknown as ReturnType<typeof vi.fn>).mockReturnValue({
      ...mockStore,
      onboarding: {
        ...mockStore.onboarding,
        currentStep: 'api-security',
      },
    });

    render(<OnboardingWizard />);

    expect(screen.getByText('Back')).toBeInTheDocument();
  });

  it('should navigate back when Back is clicked', () => {
    (useSecurityEducationStore as unknown as ReturnType<typeof vi.fn>).mockReturnValue({
      ...mockStore,
      onboarding: {
        ...mockStore.onboarding,
        currentStep: 'api-security',
      },
    });

    render(<OnboardingWizard />);

    fireEvent.click(screen.getByText('Back'));

    expect(mockStore.goToPreviousStep).toHaveBeenCalled();
  });

  it('should show Complete button on last step', () => {
    (useSecurityEducationStore as unknown as ReturnType<typeof vi.fn>).mockReturnValue({
      ...mockStore,
      onboarding: {
        ...mockStore.onboarding,
        currentStep: 'safety-checklist',
      },
    });

    render(<OnboardingWizard />);

    expect(screen.getByText('Complete')).toBeInTheDocument();
  });

  it('should call onComplete when wizard is completed', () => {
    const onComplete = vi.fn();
    (useSecurityEducationStore as unknown as ReturnType<typeof vi.fn>).mockReturnValue({
      ...mockStore,
      onboarding: {
        ...mockStore.onboarding,
        isComplete: true,
      },
    });

    render(<OnboardingWizard onComplete={onComplete} />);

    expect(onComplete).toHaveBeenCalled();
  });
});
