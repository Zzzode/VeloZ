/**
 * VeloZ Strategy Management - Unit Tests
 */

import { describe, it, expect, vi, beforeEach } from 'vitest';
import {
  StrategyStatus,
  ParamType,
  validateParam,
  validateAllParams,
  convertParamValue,
  renderParamInput,
  renderStrategyCard,
} from './strategies.js';

describe('StrategyStatus', () => {
  it('should have correct status values', () => {
    expect(StrategyStatus.STOPPED).toBe('stopped');
    expect(StrategyStatus.RUNNING).toBe('running');
    expect(StrategyStatus.PAUSED).toBe('paused');
    expect(StrategyStatus.ERROR).toBe('error');
  });
});

describe('ParamType', () => {
  it('should have correct type values', () => {
    expect(ParamType.INT).toBe('int');
    expect(ParamType.FLOAT).toBe('float');
    expect(ParamType.STRING).toBe('string');
    expect(ParamType.BOOL).toBe('bool');
    expect(ParamType.ENUM).toBe('enum');
  });
});

describe('validateParam', () => {
  describe('INT type', () => {
    const schema = { type: ParamType.INT, min: 1, max: 100 };

    it('should validate valid integer', () => {
      expect(validateParam(50, schema)).toEqual({ valid: true, error: null });
      expect(validateParam('50', schema)).toEqual({ valid: true, error: null });
    });

    it('should reject non-integer', () => {
      expect(validateParam('abc', schema).valid).toBe(false);
      expect(validateParam('abc', schema).error).toBe('Must be an integer');
    });

    it('should reject value below min', () => {
      expect(validateParam(0, schema).valid).toBe(false);
      expect(validateParam(0, schema).error).toBe('Must be at least 1');
    });

    it('should reject value above max', () => {
      expect(validateParam(101, schema).valid).toBe(false);
      expect(validateParam(101, schema).error).toBe('Must be at most 100');
    });

    it('should reject empty value', () => {
      expect(validateParam('', schema).valid).toBe(false);
      expect(validateParam(null, schema).valid).toBe(false);
      expect(validateParam(undefined, schema).valid).toBe(false);
    });
  });

  describe('FLOAT type', () => {
    const schema = { type: ParamType.FLOAT, min: 0.01, max: 1.0 };

    it('should validate valid float', () => {
      expect(validateParam(0.5, schema)).toEqual({ valid: true, error: null });
      expect(validateParam('0.5', schema)).toEqual({ valid: true, error: null });
    });

    it('should reject non-number', () => {
      expect(validateParam('abc', schema).valid).toBe(false);
      expect(validateParam('abc', schema).error).toBe('Must be a number');
    });

    it('should reject value below min', () => {
      expect(validateParam(0.001, schema).valid).toBe(false);
    });

    it('should reject value above max', () => {
      expect(validateParam(1.5, schema).valid).toBe(false);
    });
  });

  describe('STRING type', () => {
    const schema = { type: ParamType.STRING, min: 2, max: 10 };

    it('should validate valid string', () => {
      expect(validateParam('hello', schema)).toEqual({ valid: true, error: null });
    });

    it('should reject string too short', () => {
      expect(validateParam('a', schema).valid).toBe(false);
      expect(validateParam('a', schema).error).toBe('Must be at least 2 characters');
    });

    it('should reject string too long', () => {
      expect(validateParam('12345678901', schema).valid).toBe(false);
      expect(validateParam('12345678901', schema).error).toBe('Must be at most 10 characters');
    });
  });

  describe('BOOL type', () => {
    const schema = { type: ParamType.BOOL };

    it('should validate boolean values', () => {
      expect(validateParam(true, schema)).toEqual({ valid: true, error: null });
      expect(validateParam(false, schema)).toEqual({ valid: true, error: null });
      expect(validateParam('true', schema)).toEqual({ valid: true, error: null });
      expect(validateParam('false', schema)).toEqual({ valid: true, error: null });
    });

    it('should reject non-boolean', () => {
      expect(validateParam('yes', schema).valid).toBe(false);
    });
  });

  describe('ENUM type', () => {
    const schema = { type: ParamType.ENUM, options: ['BUY', 'SELL', 'HOLD'] };

    it('should validate valid option', () => {
      expect(validateParam('BUY', schema)).toEqual({ valid: true, error: null });
      expect(validateParam('SELL', schema)).toEqual({ valid: true, error: null });
    });

    it('should reject invalid option', () => {
      expect(validateParam('INVALID', schema).valid).toBe(false);
      expect(validateParam('INVALID', schema).error).toBe('Must be one of: BUY, SELL, HOLD');
    });
  });
});

describe('validateAllParams', () => {
  const schema = {
    lookback: { type: ParamType.INT, min: 1, max: 100 },
    threshold: { type: ParamType.FLOAT, min: 0, max: 1 },
    enabled: { type: ParamType.BOOL },
  };

  it('should validate all valid params', () => {
    const params = { lookback: 20, threshold: 0.5, enabled: true };
    const result = validateAllParams(params, schema);
    expect(result.valid).toBe(true);
    expect(result.errors).toEqual({});
  });

  it('should collect all errors', () => {
    const params = { lookback: 0, threshold: 2, enabled: 'invalid' };
    const result = validateAllParams(params, schema);
    expect(result.valid).toBe(false);
    expect(result.errors.lookback).toBeDefined();
    expect(result.errors.threshold).toBeDefined();
    expect(result.errors.enabled).toBeDefined();
  });

  it('should handle partial errors', () => {
    const params = { lookback: 50, threshold: 2, enabled: true };
    const result = validateAllParams(params, schema);
    expect(result.valid).toBe(false);
    expect(result.errors.lookback).toBeUndefined();
    expect(result.errors.threshold).toBeDefined();
    expect(result.errors.enabled).toBeUndefined();
  });
});

describe('convertParamValue', () => {
  it('should convert to integer', () => {
    expect(convertParamValue('42', ParamType.INT)).toBe(42);
    expect(convertParamValue('42.9', ParamType.INT)).toBe(42);
  });

  it('should convert to float', () => {
    expect(convertParamValue('3.14', ParamType.FLOAT)).toBeCloseTo(3.14);
    expect(convertParamValue('42', ParamType.FLOAT)).toBe(42);
  });

  it('should convert to boolean', () => {
    expect(convertParamValue('true', ParamType.BOOL)).toBe(true);
    expect(convertParamValue('false', ParamType.BOOL)).toBe(false);
    expect(convertParamValue(true, ParamType.BOOL)).toBe(true);
  });

  it('should pass through string', () => {
    expect(convertParamValue('hello', ParamType.STRING)).toBe('hello');
  });
});

describe('renderParamInput', () => {
  it('should render number input for INT', () => {
    const schema = { type: ParamType.INT, min: 1, max: 100, description: 'Test param' };
    const html = renderParamInput('lookback_period', schema, 20);

    expect(html).toContain('type="number"');
    expect(html).toContain('id="param-lookback_period"');
    expect(html).toContain('value="20"');
    expect(html).toContain('min="1"');
    expect(html).toContain('max="100"');
    expect(html).toContain('Lookback Period');
    expect(html).toContain('Test param');
  });

  it('should render select for BOOL', () => {
    const schema = { type: ParamType.BOOL };
    const html = renderParamInput('enabled', schema, true);

    expect(html).toContain('<select');
    expect(html).toContain('value="true"');
    expect(html).toContain('value="false"');
  });

  it('should render select for ENUM', () => {
    const schema = { type: ParamType.ENUM, options: ['BUY', 'SELL'] };
    const html = renderParamInput('side', schema, 'BUY');

    expect(html).toContain('<select');
    expect(html).toContain('BUY');
    expect(html).toContain('SELL');
  });

  it('should show HOT badge when hot=true', () => {
    const schema = { type: ParamType.INT, hot: true };
    const html = renderParamInput('param', schema, 10);

    expect(html).toContain('hot-badge');
    expect(html).toContain('HOT');
  });

  it('should render disabled input', () => {
    const schema = { type: ParamType.INT };
    const html = renderParamInput('param', schema, 10, true);

    expect(html).toContain('disabled');
  });
});

describe('renderStrategyCard', () => {
  const mockStrategy = {
    id: 'strat-001',
    name: 'BTC Momentum',
    type: 'momentum',
    symbol: 'BTCUSDT',
    status: 'running',
    capital: 10000,
    pnl: 523.45,
    pnl_percent: 5.23,
    trades: 47,
    win_rate: 63.8,
    max_drawdown: 3.2,
    start_time: Date.now() - 86400000,
  };

  it('should render strategy card with correct data', () => {
    const html = renderStrategyCard(mockStrategy);

    expect(html).toContain('BTC Momentum');
    expect(html).toContain('momentum');
    expect(html).toContain('BTCUSDT');
    expect(html).toContain('badge-running');
    expect(html).toContain('47'); // trades
    expect(html).toContain('63.8%'); // win rate
  });

  it('should show pause/stop buttons for running strategy', () => {
    const html = renderStrategyCard({ ...mockStrategy, status: 'running' });

    expect(html).toContain('data-action="pause"');
    expect(html).toContain('data-action="stop"');
    expect(html).not.toContain('data-action="start"');
  });

  it('should show resume/stop buttons for paused strategy', () => {
    const html = renderStrategyCard({ ...mockStrategy, status: 'paused' });

    expect(html).toContain('data-action="start"');
    expect(html).toContain('data-action="stop"');
    expect(html).not.toContain('data-action="pause"');
  });

  it('should show start button for stopped strategy', () => {
    const html = renderStrategyCard({ ...mockStrategy, status: 'stopped' });

    expect(html).toContain('data-action="start"');
    expect(html).not.toContain('data-action="pause"');
    expect(html).not.toContain('data-action="stop"');
  });

  it('should always show edit and delete buttons', () => {
    const html = renderStrategyCard(mockStrategy);

    expect(html).toContain('data-action="edit"');
    expect(html).toContain('data-action="delete"');
  });

  it('should include strategy ID in data attributes', () => {
    const html = renderStrategyCard(mockStrategy);

    expect(html).toContain('data-strategy-id="strat-001"');
    expect(html).toContain('data-id="strat-001"');
  });

  it('should handle missing optional fields', () => {
    const minimalStrategy = {
      id: 'strat-002',
      name: 'Test Strategy',
    };
    const html = renderStrategyCard(minimalStrategy);

    expect(html).toContain('Test Strategy');
    expect(html).toContain('custom'); // default type
    expect(html).toContain('0'); // default trades
  });
});
