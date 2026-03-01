import {
  AreaChart,
  Area,
  XAxis,
  YAxis,
  CartesianGrid,
  Tooltip,
  ResponsiveContainer,
} from 'recharts';
import type { DrawdownPoint } from '@/shared/api/types';

interface DrawdownChartProps {
  data: DrawdownPoint[];
}

interface ChartDataPoint {
  date: string;
  drawdown: number;
  timestamp: string;
}

export function DrawdownChart({ data }: DrawdownChartProps) {
  if (!data || data.length === 0) {
    return (
      <div className="h-48 flex items-center justify-center text-text-muted">
        No drawdown data available
      </div>
    );
  }

  const chartData: ChartDataPoint[] = data.map((point) => ({
    date: new Date(point.timestamp).toLocaleDateString('en-US', {
      month: 'short',
      day: 'numeric',
    }),
    drawdown: point.drawdown * 100, // Convert to percentage
    timestamp: point.timestamp,
  }));

  const minDrawdown = Math.min(...chartData.map((p) => p.drawdown));

  return (
    <div className="h-48">
      <ResponsiveContainer width="100%" height="100%">
        <AreaChart
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
            tickFormatter={(value: number) => `${value.toFixed(1)}%`}
            tick={{ fontSize: 12, fill: '#6b7280' }}
            tickLine={{ stroke: '#e5e7eb' }}
            axisLine={{ stroke: '#e5e7eb' }}
            domain={[minDrawdown * 1.1, 0]}
            width={60}
          />
          <Tooltip
            formatter={(value) => [`${Number(value).toFixed(2)}%`, 'Drawdown']}
            labelFormatter={(label) => `Date: ${String(label)}`}
            contentStyle={{
              backgroundColor: '#ffffff',
              border: '1px solid #e5e7eb',
              borderRadius: '6px',
              fontSize: '13px',
            }}
          />
          <Area
            type="monotone"
            dataKey="drawdown"
            stroke="#dc2626"
            fill="#fecaca"
            fillOpacity={0.6}
          />
        </AreaChart>
      </ResponsiveContainer>
    </div>
  );
}
