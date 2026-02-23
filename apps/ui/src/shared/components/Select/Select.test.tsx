import { describe, it, expect, vi } from 'vitest';
import { render, screen, userEvent } from '@/test/test-utils';
import { Select } from './Select';

const defaultOptions = [
  { value: 'option1', label: 'Option 1' },
  { value: 'option2', label: 'Option 2' },
  { value: 'option3', label: 'Option 3' },
];

describe('Select', () => {
  describe('rendering', () => {
    it('renders select element', () => {
      render(<Select options={defaultOptions} />);
      expect(screen.getByRole('combobox')).toBeInTheDocument();
    });

    it('renders all options', () => {
      render(<Select options={defaultOptions} />);
      expect(screen.getByRole('option', { name: 'Option 1' })).toBeInTheDocument();
      expect(screen.getByRole('option', { name: 'Option 2' })).toBeInTheDocument();
      expect(screen.getByRole('option', { name: 'Option 3' })).toBeInTheDocument();
    });

    it('renders with label', () => {
      render(<Select label="Choose option" options={defaultOptions} />);
      expect(screen.getByLabelText('Choose option')).toBeInTheDocument();
    });

    it('renders placeholder option', () => {
      render(<Select options={defaultOptions} placeholder="Select an option" />);
      expect(screen.getByRole('option', { name: 'Select an option' })).toBeInTheDocument();
    });
  });

  describe('label and id', () => {
    it('generates id from label', () => {
      render(<Select label="Select Type" options={defaultOptions} />);
      expect(screen.getByRole('combobox')).toHaveAttribute('id', 'select-type');
    });

    it('uses provided id over generated one', () => {
      render(<Select label="Select Type" id="custom-id" options={defaultOptions} />);
      expect(screen.getByRole('combobox')).toHaveAttribute('id', 'custom-id');
    });
  });

  describe('error state', () => {
    it('displays error message', () => {
      render(<Select options={defaultOptions} error="Selection required" />);
      expect(screen.getByText('Selection required')).toBeInTheDocument();
    });

    it('applies error styling', () => {
      render(<Select options={defaultOptions} error="Error" />);
      expect(screen.getByRole('combobox')).toHaveClass('border-danger');
    });

    it('sets aria-invalid when error exists', () => {
      render(<Select options={defaultOptions} error="Error" />);
      expect(screen.getByRole('combobox')).toHaveAttribute('aria-invalid', 'true');
    });

    it('sets aria-describedby to error id', () => {
      render(<Select label="Type" options={defaultOptions} error="Invalid" />);
      expect(screen.getByRole('combobox')).toHaveAttribute('aria-describedby', 'type-error');
    });
  });

  describe('helper text', () => {
    it('displays helper text', () => {
      render(<Select options={defaultOptions} helperText="Choose one option" />);
      expect(screen.getByText('Choose one option')).toBeInTheDocument();
    });

    it('does not display helper text when error exists', () => {
      render(<Select options={defaultOptions} helperText="Helper" error="Error" />);
      expect(screen.queryByText('Helper')).not.toBeInTheDocument();
      expect(screen.getByText('Error')).toBeInTheDocument();
    });
  });

  describe('disabled options', () => {
    it('renders disabled options', () => {
      const options = [
        { value: 'enabled', label: 'Enabled' },
        { value: 'disabled', label: 'Disabled', disabled: true },
      ];
      render(<Select options={options} />);
      expect(screen.getByRole('option', { name: 'Disabled' })).toBeDisabled();
    });
  });

  describe('fullWidth', () => {
    it('applies full width by default', () => {
      render(<Select options={defaultOptions} />);
      expect(screen.getByRole('combobox')).toHaveClass('w-full');
    });

    it('does not apply full width when fullWidth is false', () => {
      render(<Select options={defaultOptions} fullWidth={false} />);
      expect(screen.getByRole('combobox')).not.toHaveClass('w-full');
    });
  });

  describe('disabled state', () => {
    it('can be disabled', () => {
      render(<Select options={defaultOptions} disabled />);
      expect(screen.getByRole('combobox')).toBeDisabled();
    });
  });

  describe('interactions', () => {
    it('allows selection', async () => {
      const user = userEvent.setup();
      render(<Select options={defaultOptions} />);
      const select = screen.getByRole('combobox');

      await user.selectOptions(select, 'option2');
      expect(select).toHaveValue('option2');
    });

    it('calls onChange when selection changes', async () => {
      const user = userEvent.setup();
      const handleChange = vi.fn();
      render(<Select options={defaultOptions} onChange={handleChange} />);

      await user.selectOptions(screen.getByRole('combobox'), 'option1');
      expect(handleChange).toHaveBeenCalled();
    });
  });

  describe('ref forwarding', () => {
    it('forwards ref to select element', () => {
      const ref = vi.fn();
      render(<Select options={defaultOptions} ref={ref} />);
      expect(ref).toHaveBeenCalled();
      expect(ref.mock.calls[0][0]).toBeInstanceOf(HTMLSelectElement);
    });
  });

  describe('custom className', () => {
    it('applies custom className', () => {
      render(<Select options={defaultOptions} className="custom-class" />);
      expect(screen.getByRole('combobox')).toHaveClass('custom-class');
    });
  });

  describe('controlled value', () => {
    it('respects controlled value', () => {
      render(<Select options={defaultOptions} value="option2" onChange={() => {}} />);
      expect(screen.getByRole('combobox')).toHaveValue('option2');
    });
  });
});
