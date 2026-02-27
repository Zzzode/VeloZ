/**
 * Price chart component using Lightweight Charts
 */
import { useEffect, useRef, useState } from 'react';
import { createChart, CandlestickSeries } from 'lightweight-charts';
import type { IChartApi, ISeriesApi, CandlestickData, Time } from 'lightweight-charts';
import { Card, Spinner } from '@/shared/components';
import { useMarketStore } from '../store';

// Mock candlestick data generator for demo
function generateMockCandles(symbol: string, count: number): CandlestickData<Time>[] {
  const candles: CandlestickData<Time>[] = [];
  const now = Math.floor(Date.now() / 1000);
  const interval = 3600; // 1 hour in seconds

  // Base prices for different symbols
  const basePrices: Record<string, number> = {
    BTCUSDT: 45000,
    ETHUSDT: 2500,
    BNBUSDT: 300,
    SOLUSDT: 100,
    XRPUSDT: 0.5,
  };

  let price = basePrices[symbol] ?? 100;
  const volatility = price * 0.02; // 2% volatility

  for (let i = count; i >= 0; i--) {
    const time = (now - i * interval) as Time;
    const open = price;
    const change = (Math.random() - 0.5) * volatility;
    const close = open + change;
    const high = Math.max(open, close) + Math.random() * volatility * 0.5;
    const low = Math.min(open, close) - Math.random() * volatility * 0.5;

    candles.push({
      time,
      open,
      high,
      low,
      close,
    });

    price = close;
  }

  return candles;
}

interface PriceChartProps {
  height?: number;
}

export function PriceChart({ height = 400 }: PriceChartProps) {
  const containerRef = useRef<HTMLDivElement>(null);
  const chartRef = useRef<IChartApi | null>(null);
  const seriesRef = useRef<ISeriesApi<'Candlestick'> | null>(null);
  const [isLoading, setIsLoading] = useState(true);

  const selectedSymbol = useMarketStore((state) => state.selectedSymbol);
  const selectedTimeframe = useMarketStore((state) => state.selectedTimeframe);
  const currentPrice = useMarketStore((state) => state.prices[selectedSymbol]);

  // Initialize chart
  useEffect(() => {
    if (!containerRef.current) return;

    const chart = createChart(containerRef.current, {
      width: containerRef.current.clientWidth,
      height,
      layout: {
        background: { color: 'transparent' },
        textColor: '#6B7280',
      },
      grid: {
        vertLines: { color: '#E5E7EB' },
        horzLines: { color: '#E5E7EB' },
      },
      crosshair: {
        mode: 1,
      },
      rightPriceScale: {
        borderColor: '#E5E7EB',
      },
      timeScale: {
        borderColor: '#E5E7EB',
        timeVisible: true,
        secondsVisible: false,
      },
    });

    const candlestickSeries = chart.addSeries(CandlestickSeries, {
      upColor: '#10B981',
      downColor: '#EF4444',
      borderVisible: false,
      wickUpColor: '#10B981',
      wickDownColor: '#EF4444',
    });

    chartRef.current = chart;
    seriesRef.current = candlestickSeries;

    // Handle resize
    const handleResize = () => {
      if (containerRef.current && chartRef.current) {
        chartRef.current.applyOptions({
          width: containerRef.current.clientWidth,
        });
      }
    };

    window.addEventListener('resize', handleResize);

    return () => {
      window.removeEventListener('resize', handleResize);
      chart.remove();
      chartRef.current = null;
      seriesRef.current = null;
    };
  }, [height]);

  // Load data when symbol or timeframe changes
  useEffect(() => {
    if (!seriesRef.current) return;

    setIsLoading(true);

    // Simulate API call delay
    const timer = setTimeout(() => {
      const candles = generateMockCandles(selectedSymbol, 100);
      seriesRef.current?.setData(candles);
      chartRef.current?.timeScale().fitContent();
      setIsLoading(false);
    }, 500);

    return () => clearTimeout(timer);
  }, [selectedSymbol, selectedTimeframe]);

  // Update last candle with real-time price
  useEffect(() => {
    if (!seriesRef.current || !currentPrice) return;

    const now = Math.floor(Date.now() / 1000) as Time;
    const price = currentPrice.price;

    if (price === null || price === undefined) return;

    // Update the last candle
    seriesRef.current.update({
      time: now,
      open: price,
      high: price,
      low: price,
      close: price,
    });
  }, [currentPrice]);

  return (
    <Card
      title="Price Chart"
      subtitle={`${selectedSymbol} - ${selectedTimeframe.toUpperCase()}`}
      noPadding
    >
      <div className="relative" style={{ height }}>
        {isLoading && (
          <div className="absolute inset-0 flex items-center justify-center bg-background/80 z-10">
            <Spinner size="lg" />
          </div>
        )}
        <div ref={containerRef} className="w-full h-full" />
      </div>
    </Card>
  );
}
