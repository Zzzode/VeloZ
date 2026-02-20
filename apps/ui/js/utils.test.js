/**
 * VeloZ UI Utility Functions - Unit Tests
 */

import { describe, it, expect } from 'vitest';
import {
  escapeHtml,
  formatParamName,
  formatUptime,
  formatPrice,
  formatQuantity,
  formatPnL,
  formatPercent,
  calculateSpread,
  validateOrderParams,
  normalizeApiBase,
  generateClientOrderId,
} from './utils.js';

describe('escapeHtml', () => {
  it('should escape HTML special characters', () => {
    expect(escapeHtml('<script>alert("xss")</script>')).toBe(
      '&lt;script&gt;alert(&quot;xss&quot;)&lt;/script&gt;'
    );
  });

  it('should handle ampersands', () => {
    expect(escapeHtml('foo & bar')).toBe('foo &amp; bar');
  });

  it('should handle single quotes', () => {
    expect(escapeHtml("it's")).toBe("it&#039;s");
  });

  it('should return empty string for null/undefined', () => {
    expect(escapeHtml(null)).toBe('');
    expect(escapeHtml(undefined)).toBe('');
  });

  it('should convert non-strings to strings', () => {
    expect(escapeHtml(123)).toBe('123');
    expect(escapeHtml(true)).toBe('true');
  });
});

describe('formatParamName', () => {
  it('should convert snake_case to Title Case', () => {
    expect(formatParamName('lookback_period')).toBe('Lookback Period');
    expect(formatParamName('entry_threshold')).toBe('Entry Threshold');
  });

  it('should handle single words', () => {
    expect(formatParamName('spread')).toBe('Spread');
  });

  it('should handle multiple underscores', () => {
    expect(formatParamName('max_position_size_pct')).toBe('Max Position Size Pct');
  });
});

describe('formatUptime', () => {
  it('should format hours and minutes', () => {
    expect(formatUptime(3600000)).toBe('1h 0m'); // 1 hour
    expect(formatUptime(5400000)).toBe('1h 30m'); // 1.5 hours
  });

  it('should format days and hours for long durations', () => {
    // Note: formatUptime only shows days when hours > 24
    expect(formatUptime(86400000)).toBe('24h 0m'); // exactly 24 hours stays as hours
    expect(formatUptime(90000000)).toBe('1d 1h'); // 25 hours shows as days
    expect(formatUptime(259200000)).toBe('3d 0h'); // 72 hours
  });

  it('should handle zero', () => {
    expect(formatUptime(0)).toBe('0h 0m');
  });
});

describe('formatPrice', () => {
  it('should format with default 2 decimals', () => {
    expect(formatPrice(42150.5)).toBe('42150.50');
    expect(formatPrice(100)).toBe('100.00');
  });

  it('should respect custom decimal places', () => {
    // toFixed truncates, doesn't round in some JS engines
    expect(formatPrice(42150.12345, 4)).toBe('42150.1234');
  });

  it('should return dash for invalid values', () => {
    expect(formatPrice(null)).toBe('-');
    expect(formatPrice(undefined)).toBe('-');
    expect(formatPrice(NaN)).toBe('-');
  });
});

describe('formatQuantity', () => {
  it('should format with default 4 decimals', () => {
    expect(formatQuantity(0.12345)).toBe('0.1235');
    expect(formatQuantity(1.5)).toBe('1.5000');
  });

  it('should return dash for invalid values', () => {
    expect(formatQuantity(null)).toBe('-');
  });
});

describe('formatPnL', () => {
  it('should format positive PnL with plus sign', () => {
    const result = formatPnL(125.50);
    expect(result.value).toBe('+125.50');
    expect(result.className).toBe('positive');
  });

  it('should format negative PnL', () => {
    const result = formatPnL(-45.20);
    expect(result.value).toBe('-45.20');
    expect(result.className).toBe('negative');
  });

  it('should format zero as positive', () => {
    const result = formatPnL(0);
    expect(result.value).toBe('+0.00');
    expect(result.className).toBe('positive');
  });

  it('should handle invalid values', () => {
    const result = formatPnL(null);
    expect(result.value).toBe('-');
    expect(result.className).toBe('');
  });
});

describe('formatPercent', () => {
  it('should format decimal as percentage', () => {
    expect(formatPercent(0.05)).toBe('+5.00%');
    expect(formatPercent(-0.032)).toBe('-3.20%');
  });

  it('should handle zero', () => {
    expect(formatPercent(0)).toBe('+0.00%');
  });
});

describe('calculateSpread', () => {
  it('should calculate spread correctly', () => {
    const result = calculateSpread(42149, 42151);
    expect(result.absolute).toBe(2);
    expect(result.percent).toBeCloseTo(0.00474, 4);
  });

  it('should return null for invalid inputs', () => {
    expect(calculateSpread(null, 42151)).toBeNull();
    expect(calculateSpread(42149, null)).toBeNull();
    expect(calculateSpread(0, 42151)).toBeNull();
    expect(calculateSpread(-1, 42151)).toBeNull();
  });
});

describe('validateOrderParams', () => {
  it('should validate correct order params', () => {
    const result = validateOrderParams({
      side: 'BUY',
      symbol: 'BTCUSDT',
      qty: 0.001,
      price: 42000,
    });
    expect(result.valid).toBe(true);
    expect(result.errors).toHaveLength(0);
  });

  it('should reject invalid side', () => {
    const result = validateOrderParams({
      side: 'INVALID',
      symbol: 'BTCUSDT',
      qty: 0.001,
      price: 42000,
    });
    expect(result.valid).toBe(false);
    expect(result.errors).toContain('Side must be BUY or SELL');
  });

  it('should reject empty symbol', () => {
    const result = validateOrderParams({
      side: 'BUY',
      symbol: '',
      qty: 0.001,
      price: 42000,
    });
    expect(result.valid).toBe(false);
    expect(result.errors).toContain('Symbol is required');
  });

  it('should reject zero or negative quantity', () => {
    const result = validateOrderParams({
      side: 'BUY',
      symbol: 'BTCUSDT',
      qty: 0,
      price: 42000,
    });
    expect(result.valid).toBe(false);
    expect(result.errors).toContain('Quantity must be a positive number');
  });

  it('should reject zero or negative price', () => {
    const result = validateOrderParams({
      side: 'BUY',
      symbol: 'BTCUSDT',
      qty: 0.001,
      price: -100,
    });
    expect(result.valid).toBe(false);
    expect(result.errors).toContain('Price must be a positive number');
  });

  it('should collect multiple errors', () => {
    const result = validateOrderParams({
      side: '',
      symbol: '',
      qty: 0,
      price: 0,
    });
    expect(result.valid).toBe(false);
    expect(result.errors.length).toBeGreaterThan(1);
  });
});

describe('normalizeApiBase', () => {
  it('should remove trailing slashes', () => {
    expect(normalizeApiBase('http://localhost:8080/')).toBe('http://localhost:8080');
    expect(normalizeApiBase('http://localhost:8080///')).toBe('http://localhost:8080');
  });

  it('should trim whitespace', () => {
    expect(normalizeApiBase('  http://localhost:8080  ')).toBe('http://localhost:8080');
  });

  it('should handle empty/null input', () => {
    expect(normalizeApiBase('')).toBe('');
    expect(normalizeApiBase(null)).toBe('');
    expect(normalizeApiBase(undefined)).toBe('');
  });
});

describe('generateClientOrderId', () => {
  it('should generate ID with default prefix', () => {
    const id = generateClientOrderId();
    expect(id).toMatch(/^web-\d+$/);
  });

  it('should use custom prefix', () => {
    const id = generateClientOrderId('strat');
    expect(id).toMatch(/^strat-\d+$/);
  });

  it('should include timestamp in ID', () => {
    const before = Date.now();
    const id = generateClientOrderId();
    const after = Date.now();
    // Extract timestamp from ID
    const timestamp = parseInt(id.split('-')[1], 10);
    expect(timestamp).toBeGreaterThanOrEqual(before);
    expect(timestamp).toBeLessThanOrEqual(after);
  });
});
