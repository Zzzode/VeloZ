/**
 * Order Form Component
 * Form for placing market/limit orders
 */

import { useCallback, type FormEvent } from 'react';
import { Button, Input, Card } from '@/shared/components';
import { useTradingStore } from '../store';
import { usePlaceOrder } from '../hooks';
import type { OrderSide } from '@/shared/api/types';

export function OrderForm() {
  const {
    selectedSymbol,
    orderFormSide,
    orderFormPrice,
    orderFormQty,
    orderBook,
    setOrderFormSide,
    setOrderFormPrice,
    setOrderFormQty,
  } = useTradingStore();

  const { placeOrder, isLoading } = usePlaceOrder();

  const handleSideChange = useCallback(
    (side: OrderSide) => {
      setOrderFormSide(side);
      // Auto-fill price from order book
      if (side === 'BUY' && orderBook.asks.length > 0) {
        setOrderFormPrice(orderBook.asks[0].price.toString());
      } else if (side === 'SELL' && orderBook.bids.length > 0) {
        setOrderFormPrice(orderBook.bids[0].price.toString());
      }
    },
    [orderBook, setOrderFormSide, setOrderFormPrice]
  );

  const handleSubmit = useCallback(
    async (e: FormEvent) => {
      e.preventDefault();

      const price = parseFloat(orderFormPrice);
      const qty = parseFloat(orderFormQty);

      if (isNaN(price) || price <= 0) {
        return;
      }
      if (isNaN(qty) || qty <= 0) {
        return;
      }

      try {
        await placeOrder({
          side: orderFormSide,
          symbol: selectedSymbol,
          price,
          qty,
        });
      } catch {
        // Error handled in hook
      }
    },
    [orderFormSide, orderFormPrice, orderFormQty, selectedSymbol, placeOrder]
  );

  const isValid =
    orderFormPrice &&
    orderFormQty &&
    parseFloat(orderFormPrice) > 0 &&
    parseFloat(orderFormQty) > 0;

  return (
    <Card title="Place Order" subtitle={selectedSymbol}>
      <form onSubmit={handleSubmit} className="space-y-4">
        {/* Side Toggle */}
        <div className="grid grid-cols-2 gap-2">
          <button
            type="button"
            onClick={() => handleSideChange('BUY')}
            className={`py-2.5 px-4 rounded-md font-medium transition-colors ${
              orderFormSide === 'BUY'
                ? 'bg-success text-white'
                : 'bg-background-secondary text-text-muted hover:bg-gray-100'
            }`}
          >
            Buy
          </button>
          <button
            type="button"
            onClick={() => handleSideChange('SELL')}
            className={`py-2.5 px-4 rounded-md font-medium transition-colors ${
              orderFormSide === 'SELL'
                ? 'bg-danger text-white'
                : 'bg-background-secondary text-text-muted hover:bg-gray-100'
            }`}
          >
            Sell
          </button>
        </div>

        {/* Price Input */}
        <Input
          label="Price"
          type="number"
          step="0.01"
          min="0"
          placeholder="0.00"
          value={orderFormPrice}
          onChange={(e) => setOrderFormPrice(e.target.value)}
        />

        {/* Quantity Input */}
        <Input
          label="Quantity"
          type="number"
          step="0.0001"
          min="0"
          placeholder="0.0000"
          value={orderFormQty}
          onChange={(e) => setOrderFormQty(e.target.value)}
        />

        {/* Order Summary */}
        {isValid && (
          <div className="p-3 bg-background-secondary rounded-md">
            <div className="flex justify-between text-sm">
              <span className="text-text-muted">Total</span>
              <span className="font-medium">
                {(parseFloat(orderFormPrice) * parseFloat(orderFormQty)).toFixed(2)} USDT
              </span>
            </div>
          </div>
        )}

        {/* Submit Button */}
        <Button
          type="submit"
          variant={orderFormSide === 'BUY' ? 'success' : 'danger'}
          fullWidth
          isLoading={isLoading}
          disabled={!isValid || isLoading}
        >
          {orderFormSide === 'BUY' ? 'Buy' : 'Sell'} {selectedSymbol}
        </Button>
      </form>
    </Card>
  );
}
