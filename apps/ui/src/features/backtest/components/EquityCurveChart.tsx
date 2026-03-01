import {
  LineChart,
  Line,
  XAxis,
  YAxis,
  CartesianGrid,
  Tooltip,
  ResponsiveContainer,
  ReferenceLine,
} from 'recharts';
import type { EquityCurvePoint } from '@/shared/api/types';
import { formatCurrency } from '@/shared/utils';

interface EquityCurveChartProps {
  data: EquityCurvePoint[];
  initialCapital: number;
}

interface ChartDataPoint {
  date: string;
  equity: number;
  timestamp: string;
}

export function EquityCurveChart({ data, initialCapital }: EquityCurveChartProps) {
  if (!data || data.length === 0) {
    return (
      <div className="h-64 flex items-center justify-center text-text-muted">
        No equity curve data available
      </div>
    );
  }

  const chartData: ChartDataPoint[] = data.map((point) => ({
    date: new Date(point.timestamp).toLocaleDateString('en-US', {
      month: 'short',
      day: 'numeric',
    }),
    equity: point.equity,
    timestamp: point.timestamp,
  }));

  const minEquity = Math.min(...data.map((p) => p.equity));
  const maxEquity = Math.max(...data.map((p) => p.equity));
  const padding = (maxEquity - minEquity) * 0.1;

  return (
    <div className="h-64">
      <ResponsiveContainer width="100%" height="100%">
        <LineChart
          data={chartData}
          margin={{ top: 10, right: 30, left: 10, bottom: 0 }}
        >
          <CartesianGrid strokeDasharray="3 3" stroke="#e5e7eb" />
          <XAxis
            dataKey="date"
            tick={{ fontSize: 12, fill: '#6b7280' }}
            tickLine={{ stroke: '#e5e7eb' }}
            axisLine={{ stroke: '#e5e7eb' }}
            interval="preserveStartEnd"
          />
          <YAxis
            tickFormatter={(value: number) => formatCurrency(value)}
            tick={{ fontSize: 12, fill: '#6b7280' }}
            tickLine={{ stroke: '#e5e7eb' }}
            axisLine={{ stroke: '#e5e7eb' }}
            domain={[minEquity - padding, maxEquity + padding]}
            width={80}
          />
          <Tooltip
            formatter={(value) => [formatCurrency(Number(value)), 'Equity']}
            labelFormatter={(label) => `Date: ${String(label)}`}
            contentStyle={{
              backgroundColor: '#ffffff',
              border: '1px solid #e5e7eb',
              borderRadius: '6px',
              fontSize: '13px',
            }}
          />
          <ReferenceLine
            y={initialCapital}
            stroke="#9ca3af"
            strokeDasharray="5 5"
            label={{
              value: 'Initial',
              position: 'right',
              fill: '#9ca3af',
              fontSize: 11,
            }}
          />
          <Line
            type="monotone"
            dataKey="equity"
            stroke="#2563eb"
            strokeWidth={2}
            dot={false}
            activeDot={{ r: 4, fill: '#2563eb' }}
          />
        </LineChart>
      </ResponsiveContainer>
    </div>
  );
}
