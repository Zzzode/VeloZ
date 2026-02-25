/**
 * Quick Order Panel Component
 * Compact order entry panel for chart-based trading
 */
import { useState, useEffect } from 'react';
import { Button, Input, Select } from '@/shared/components';
import { useChartStore } from '../store/chartStore';
import { usePlaceOrder } from '@/features/trading/hooks';

// =============================================================================
// Types
// =============================================================================

type OrderSide = 'buy' | 'sell';
type OrderType = 'limit' | 'market';

interface QuickOrderPanelProps {
  currentPrice: number;
  className?: string;
}

// =============================================================================
// Component
// =============================================================================

export function QuickOrderPanel({ currentPrice, className = '' }: QuickOrderPanelProps) {
  const { symbol, settings } = useChartStore();
  const { placeOrder, isLoading } = usePlaceOrder();

  const [side, setSide] = useState<OrderSide>('buy');
  const [orderType, setOrderType] = useState<OrderType>('limit');
  const [price, setPrice] = useState(currentPrice);
  const [quantity, setQuantity] = useState(0.01);

  // Update price when current price changes (for market orders)
  useEffect(() => {
    if (orderType === 'market') {
      setPrice(currentPrice);
    }
  }, [currentPrice, orderType]);

  if (!settings.enableChartTrading) {
    return null;
  }

  const total = price * quantity;

  const handleSubmit = async () => {
    try {
      await placeOrder({
        symbol,
        side: side.toUpperCase() as 'BUY' | 'SELL',
        qty: quantity,
        price: orderType === 'limit' ? price : currentPrice,
      });
    } catch {
      // Error is handled by the hook
    }
  };

  const handlePercentage = (pct: number) => {
    // This would normally calculate based on available balance
    // For now, just multiply current quantity
    setQuantity(Number((quantity * (pct / 100)).toFixed(8)));
  };

  return (
    <div className={`bg-white border border-gray-200 rounded-lg p-4 ${className}`}>
      {/* Header */}
      <div className="flex items-center justify-between mb-4">
        <h3 className="font-semibold text-gray-900">Quick Order</h3>
        <span className="text-sm text-gray-500">{symbol}</span>
      </div>

      {/* Current Price */}
      <div className="text-center mb-4">
        <span className="text-2xl font-bold text-gray-900">
          {currentPrice.toLocaleString(undefined, {
            minimumFractionDigits: 2,
            maximumFractionDigits: 2,
          })}
        </span>
      </div>

      {/* Side Toggle */}
      <div className="grid grid-cols-2 gap-2 mb-4">
        <Button
          variant={side === 'buy' ? 'primary' : 'secondary'}
          onClick={() => setSide('buy')}
          className={side === 'buy' ? 'bg-green-600 hover:bg-green-700' : ''}
        >
          Buy
        </Button>
        <Button
          variant={side === 'sell' ? 'primary' : 'secondary'}
          onClick={() => setSide('sell')}
          className={side === 'sell' ? 'bg-red-600 hover:bg-red-700' : ''}
        >
          Sell
        </Button>
      </div>

      {/* Order Type */}
      <div className="mb-4">
        <label className="block text-sm text-gray-500 mb-1">Order Type</label>
        <Select
          options={[
            { value: 'limit', label: 'Limit' },
            { value: 'market', label: 'Market' },
          ]}
          value={orderType}
          onChange={(e) => setOrderType(e.target.value as OrderType)}
        />
      </div>

      {/* Price (for limit orders) */}
      {orderType === 'limit' && (
        <div className="mb-4">
          <label className="block text-sm text-gray-500 mb-1">Price</label>
          <Input
            type="number"
            value={price}
            onChange={(e) => setPrice(parseFloat(e.target.value) || 0)}
            step="0.01"
          />
        </div>
      )}

      {/* Quantity */}
      <div className="mb-4">
        <label className="block text-sm text-gray-500 mb-1">Amount</label>
        <Input
          type="number"
          value={quantity}
          onChange={(e) => setQuantity(parseFloat(e.target.value) || 0)}
          step="0.001"
        />
        <div className="flex gap-1 mt-2">
          {[25, 50, 75, 100].map((pct) => (
            <Button
              key={pct}
              variant="secondary"
              size="sm"
              onClick={() => handlePercentage(pct)}
              className="flex-1"
            >
              {pct}%
            </Button>
          ))}
        </div>
      </div>

      {/* Total */}
      <div className="flex justify-between items-center mb-4 p-2 bg-gray-50 rounded">
        <span className="text-sm text-gray-500">Total</span>
        <span className="font-semibold text-gray-900">
          ${total.toLocaleString(undefined, { minimumFractionDigits: 2, maximumFractionDigits: 2 })}
        </span>
      </div>

      {/* Submit Button */}
      <Button
        variant="primary"
        onClick={handleSubmit}
        disabled={isLoading || quantity <= 0}
        className={`w-full ${
          side === 'buy' ? 'bg-green-600 hover:bg-green-700' : 'bg-red-600 hover:bg-red-700'
        }`}
      >
        {isLoading ? 'Placing...' : `${side === 'buy' ? 'Buy' : 'Sell'} ${symbol}`}
      </Button>
    </div>
  );
}
