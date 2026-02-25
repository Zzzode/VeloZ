/**
 * Indicator Overlay Component
 * Renders technical indicators on the chart
 */
import { useEffect, useRef, useMemo } from 'react';
import { LineSeries, type IChartApi, type ISeriesApi, type Time } from 'lightweight-charts';
import { useChartStore, selectVisibleIndicators } from '../store/chartStore';
import { calculateIndicator } from '../indicators';
import type { IndicatorConfig, IndicatorResult } from '../types';

// =============================================================================
// Types
// =============================================================================

interface IndicatorOverlayProps {
  chart: IChartApi | null;
}

interface IndicatorSeries {
  id: string;
  series: ISeriesApi<'Line'>[];
}

// =============================================================================
// Component
// =============================================================================

export function IndicatorOverlay({ chart }: IndicatorOverlayProps) {
  const seriesMapRef = useRef<Map<string, IndicatorSeries>>(new Map());

  const { candles } = useChartStore();
  const indicators = useChartStore(selectVisibleIndicators);

  // Calculate indicator values
  const indicatorValues = useMemo(() => {
    if (candles.length === 0) return new Map<string, IndicatorResult[]>();

    const values = new Map<string, IndicatorResult[]>();

    for (const indicator of indicators) {
      if (indicator.overlay) {
        const results = calculateIndicator(indicator.type, candles, indicator.params);
        values.set(indicator.id, results);
      }
    }

    return values;
  }, [candles, indicators]);

  // Update chart series
  useEffect(() => {
    if (!chart) return;

    const currentIds = new Set(indicators.filter((i) => i.overlay).map((i) => i.id));
    const existingIds = new Set(seriesMapRef.current.keys());

    // Remove series for removed indicators
    for (const id of existingIds) {
      if (!currentIds.has(id)) {
        const indicatorSeries = seriesMapRef.current.get(id);
        if (indicatorSeries) {
          for (const series of indicatorSeries.series) {
            chart.removeSeries(series);
          }
          seriesMapRef.current.delete(id);
        }
      }
    }

    // Add or update series for current indicators
    for (const indicator of indicators) {
      if (!indicator.overlay) continue;

      const values = indicatorValues.get(indicator.id);
      if (!values || values.length === 0) continue;

      let indicatorSeries = seriesMapRef.current.get(indicator.id);

      // Determine number of lines needed
      const sampleValue = values[0];
      const lineKeys = Object.keys(sampleValue).filter((k) => k !== 'time');

      if (!indicatorSeries) {
        // Create new series
        const series: ISeriesApi<'Line'>[] = [];

        for (let i = 0; i < lineKeys.length; i++) {
          const lineSeries = chart.addSeries(LineSeries, {
            color: getLineColor(indicator, i),
            lineWidth: 2,
            priceScaleId: 'right',
            lastValueVisible: false,
            priceLineVisible: false,
          });
          series.push(lineSeries);
        }

        indicatorSeries = { id: indicator.id, series };
        seriesMapRef.current.set(indicator.id, indicatorSeries);
      }

      // Update data for each line
      for (let i = 0; i < lineKeys.length; i++) {
        const key = lineKeys[i];
        const lineData = values.map((v) => ({
          time: v.time as Time,
          value: v[key],
        }));

        if (indicatorSeries.series[i]) {
          indicatorSeries.series[i].setData(lineData);
        }
      }
    }
  }, [chart, indicators, indicatorValues]);

  // Cleanup on unmount
  useEffect(() => {
    return () => {
      if (chart) {
        for (const indicatorSeries of seriesMapRef.current.values()) {
          for (const series of indicatorSeries.series) {
            try {
              chart.removeSeries(series);
            } catch {
              // Series might already be removed
            }
          }
        }
        seriesMapRef.current.clear();
      }
    };
  }, [chart]);

  return null; // This component doesn't render anything directly
}

// =============================================================================
// Helper Functions
// =============================================================================

function getLineColor(indicator: IndicatorConfig, lineIndex: number): string {
  const baseColor = indicator.color || '#2196F3';

  // For multi-line indicators, generate variations
  if (lineIndex === 0) return baseColor;

  // Generate lighter/darker variations
  const variations = [
    baseColor,
    adjustColor(baseColor, 30),
    adjustColor(baseColor, -30),
    adjustColor(baseColor, 60),
  ];

  return variations[lineIndex % variations.length];
}

function adjustColor(hex: string, amount: number): string {
  const num = parseInt(hex.replace('#', ''), 16);
  const r = Math.min(255, Math.max(0, (num >> 16) + amount));
  const g = Math.min(255, Math.max(0, ((num >> 8) & 0x00ff) + amount));
  const b = Math.min(255, Math.max(0, (num & 0x0000ff) + amount));
  return `#${((r << 16) | (g << 8) | b).toString(16).padStart(6, '0')}`;
}
