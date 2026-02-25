/**
 * Monthly Returns Table Component
 * Displays monthly returns in a table format
 */

import { ChevronDown, ChevronUp } from 'lucide-react';
import { Card } from '@/shared/components';
import type { MonthlyReturn } from '../types';
import { useState } from 'react';

interface MonthlyReturnsTableProps {
  returns: MonthlyReturn[];
  initialShowCount?: number;
  className?: string;
}

export function MonthlyReturnsTable({
  returns,
  initialShowCount = 6,
  className = '',
}: MonthlyReturnsTableProps) {
  const [showAll, setShowAll] = useState(false);

  const displayedReturns = showAll ? returns : returns.slice(0, initialShowCount);
  const hasMore = returns.length > initialShowCount;

  // Calculate statistics
  const positiveMonths = returns.filter((r) => r.return_pct > 0).length;
  const negativeMonths = returns.filter((r) => r.return_pct < 0).length;
  const avgReturn = returns.reduce((sum, r) => sum + r.return_pct, 0) / returns.length;
  const bestMonth = returns.reduce((best, r) => (r.return_pct > best.return_pct ? r : best));
  const worstMonth = returns.reduce((worst, r) => (r.return_pct < worst.return_pct ? r : worst));

  const formatReturn = (value: number) => {
    const sign = value >= 0 ? '+' : '';
    return `${sign}${value.toFixed(1)}%`;
  };

  return (
    <Card title="Monthly Returns" subtitle="Historical monthly performance" className={className}>
      {/* Summary Stats */}
      <div className="grid grid-cols-2 md:grid-cols-4 gap-4 mb-4 pb-4 border-b border-border">
        <div>
          <p className="text-xs text-text-muted">Positive Months</p>
          <p className="text-lg font-bold text-success">{positiveMonths}</p>
        </div>
        <div>
          <p className="text-xs text-text-muted">Negative Months</p>
          <p className="text-lg font-bold text-danger">{negativeMonths}</p>
        </div>
        <div>
          <p className="text-xs text-text-muted">Average Return</p>
          <p className={`text-lg font-bold ${avgReturn >= 0 ? 'text-success' : 'text-danger'}`}>
            {formatReturn(avgReturn)}
          </p>
        </div>
        <div>
          <p className="text-xs text-text-muted">Best / Worst</p>
          <p className="text-sm">
            <span className="text-success font-bold">{formatReturn(bestMonth.return_pct)}</span>
            {' / '}
            <span className="text-danger font-bold">{formatReturn(worstMonth.return_pct)}</span>
          </p>
        </div>
      </div>

      {/* Returns Table */}
      <div className="overflow-x-auto">
        <table className="w-full">
          <thead>
            <tr className="text-left text-xs text-text-muted border-b border-border">
              <th className="pb-2 font-medium">Month</th>
              <th className="pb-2 font-medium text-right">Return</th>
              <th className="pb-2 font-medium">Performance</th>
            </tr>
          </thead>
          <tbody>
            {displayedReturns.map((item, index) => {
              const maxAbsReturn = Math.max(...returns.map((r) => Math.abs(r.return_pct)));
              const barWidth = (Math.abs(item.return_pct) / maxAbsReturn) * 100;

              return (
                <tr key={index} className="border-b border-border/50 last:border-0">
                  <td className="py-2 text-sm text-text">
                    {item.month} {item.year}
                  </td>
                  <td
                    className={`py-2 text-sm font-mono font-medium text-right ${
                      item.return_pct >= 0 ? 'text-success' : 'text-danger'
                    }`}
                  >
                    {formatReturn(item.return_pct)}
                  </td>
                  <td className="py-2 w-1/2">
                    <div className="flex items-center gap-2">
                      {item.return_pct < 0 && (
                        <div className="flex-1 flex justify-end">
                          <div
                            className="h-4 bg-danger/30 rounded-l"
                            style={{ width: `${barWidth}%` }}
                          />
                        </div>
                      )}
                      <div className="w-px h-4 bg-border" />
                      {item.return_pct >= 0 && (
                        <div className="flex-1">
                          <div
                            className="h-4 bg-success/30 rounded-r"
                            style={{ width: `${barWidth}%` }}
                          />
                        </div>
                      )}
                    </div>
                  </td>
                </tr>
              );
            })}
          </tbody>
        </table>
      </div>

      {/* Show More/Less */}
      {hasMore && (
        <button
          onClick={() => setShowAll(!showAll)}
          className="mt-4 w-full py-2 text-sm text-primary hover:text-primary-hover flex items-center justify-center gap-1"
        >
          {showAll ? (
            <>
              Show Less <ChevronUp className="w-4 h-4" />
            </>
          ) : (
            <>
              Show All {returns.length} Months <ChevronDown className="w-4 h-4" />
            </>
          )}
        </button>
      )}
    </Card>
  );
}
