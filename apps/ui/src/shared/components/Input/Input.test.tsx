import { describe, it, expect, vi } from 'vitest';
import { render, screen, userEvent } from '@/test/test-utils';
import { Input } from './Input';

describe('Input', () => {
  describe('rendering', () => {
    it('renders input element', () => {
      render(<Input />);
      expect(screen.getByRole('textbox')).toBeInTheDocument();
    });

    it('renders with label', () => {
      render(<Input label="Email" />);
      expect(screen.getByLabelText('Email')).toBeInTheDocument();
    });

    it('renders with placeholder', () => {
      render(<Input placeholder="Enter email" />);
      expect(screen.getByPlaceholderText('Enter email')).toBeInTheDocument();
    });
  });

  describe('label and id', () => {
    it('generates id from label', () => {
      render(<Input label="User Name" />);
      const input = screen.getByRole('textbox');
      expect(input).toHaveAttribute('id', 'user-name');
    });

    it('uses provided id over generated one', () => {
      render(<Input label="User Name" id="custom-id" />);
      const input = screen.getByRole('textbox');
      expect(input).toHaveAttribute('id', 'custom-id');
    });
  });

  describe('error state', () => {
    it('displays error message', () => {
      render(<Input label="Email" error="Invalid email" />);
      expect(screen.getByText('Invalid email')).toBeInTheDocument();
    });

    it('applies error styling', () => {
      render(<Input error="Error" />);
      expect(screen.getByRole('textbox')).toHaveClass('border-danger');
    });

    it('sets aria-invalid when error exists', () => {
      render(<Input error="Error" />);
      expect(screen.getByRole('textbox')).toHaveAttribute('aria-invalid', 'true');
    });

    it('sets aria-describedby to error id', () => {
      render(<Input label="Email" error="Invalid email" />);
      const input = screen.getByRole('textbox');
      expect(input).toHaveAttribute('aria-describedby', 'email-error');
    });
  });

  describe('helper text', () => {
    it('displays helper text', () => {
      render(<Input helperText="Enter your email address" />);
      expect(screen.getByText('Enter your email address')).toBeInTheDocument();
    });

    it('does not display helper text when error exists', () => {
      render(<Input helperText="Helper" error="Error" />);
      expect(screen.queryByText('Helper')).not.toBeInTheDocument();
      expect(screen.getByText('Error')).toBeInTheDocument();
    });
  });

  describe('icons', () => {
    it('renders left icon', () => {
      render(<Input leftIcon={<span data-testid="left-icon">@</span>} />);
      expect(screen.getByTestId('left-icon')).toBeInTheDocument();
    });

    it('renders right icon', () => {
      render(<Input rightIcon={<span data-testid="right-icon">!</span>} />);
      expect(screen.getByTestId('right-icon')).toBeInTheDocument();
    });

    it('applies padding for left icon', () => {
      render(<Input leftIcon={<span>@</span>} />);
      expect(screen.getByRole('textbox')).toHaveClass('pl-10');
    });

    it('applies padding for right icon', () => {
      render(<Input rightIcon={<span>!</span>} />);
      expect(screen.getByRole('textbox')).toHaveClass('pr-10');
    });
  });

  describe('fullWidth', () => {
    it('applies full width by default', () => {
      render(<Input />);
      expect(screen.getByRole('textbox')).toHaveClass('w-full');
    });

    it('does not apply full width when fullWidth is false', () => {
      render(<Input fullWidth={false} />);
      expect(screen.getByRole('textbox')).not.toHaveClass('w-full');
    });
  });

  describe('disabled state', () => {
    it('can be disabled', () => {
      render(<Input disabled />);
      expect(screen.getByRole('textbox')).toBeDisabled();
    });
  });

  describe('interactions', () => {
    it('allows typing', async () => {
      const user = userEvent.setup();
      render(<Input />);
      const input = screen.getByRole('textbox');

      await user.type(input, 'Hello');
      expect(input).toHaveValue('Hello');
    });

    it('calls onChange when value changes', async () => {
      const user = userEvent.setup();
      const handleChange = vi.fn();
      render(<Input onChange={handleChange} />);

      await user.type(screen.getByRole('textbox'), 'a');
      expect(handleChange).toHaveBeenCalled();
    });

    it('calls onFocus when focused', async () => {
      const user = userEvent.setup();
      const handleFocus = vi.fn();
      render(<Input onFocus={handleFocus} />);

      await user.click(screen.getByRole('textbox'));
      expect(handleFocus).toHaveBeenCalled();
    });

    it('calls onBlur when blurred', async () => {
      const user = userEvent.setup();
      const handleBlur = vi.fn();
      render(<Input onBlur={handleBlur} />);

      const input = screen.getByRole('textbox');
      await user.click(input);
      await user.tab();
      expect(handleBlur).toHaveBeenCalled();
    });
  });

  describe('ref forwarding', () => {
    it('forwards ref to input element', () => {
      const ref = vi.fn();
      render(<Input ref={ref} />);
      expect(ref).toHaveBeenCalled();
      expect(ref.mock.calls[0][0]).toBeInstanceOf(HTMLInputElement);
    });
  });

  describe('custom className', () => {
    it('applies custom className', () => {
      render(<Input className="custom-class" />);
      expect(screen.getByRole('textbox')).toHaveClass('custom-class');
    });
  });

  describe('input types', () => {
    it('renders password type', () => {
      const { container } = render(<Input type="password" />);
      const passwordInput = container.querySelector('input[type="password"]');
      expect(passwordInput).toBeInTheDocument();
    });

    it('renders number type', () => {
      render(<Input type="number" />);
      expect(screen.getByRole('spinbutton')).toBeInTheDocument();
    });
  });
});
