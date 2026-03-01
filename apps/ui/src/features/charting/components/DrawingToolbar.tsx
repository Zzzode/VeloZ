/**
 * Drawing Toolbar Component
 * Tools for drawing on the chart
 */
import { Button } from '@/shared/components';
import { useChartStore } from '../store/chartStore';
import type { DrawingToolType } from '../types';
import {
  MousePointer2,
  TrendingUp,
  Minus,
  ArrowRight,
  GitBranch,
  Percent,
  Square,
  Type,
  Ruler,
  Trash2,
  X,
} from 'lucide-react';

// =============================================================================
// Types
// =============================================================================

interface DrawingToolbarProps {
  onClose: () => void;
}

interface ToolDefinition {
  type: DrawingToolType;
  label: string;
  icon: React.ReactNode;
}

// =============================================================================
// Constants
// =============================================================================

const DRAWING_TOOLS: ToolDefinition[] = [
  { type: 'cursor', label: 'Cursor', icon: <MousePointer2 className="w-4 h-4" /> },
  { type: 'trendLine', label: 'Trend Line', icon: <TrendingUp className="w-4 h-4" /> },
  { type: 'horizontalLine', label: 'Horizontal Line', icon: <Minus className="w-4 h-4" /> },
  { type: 'verticalLine', label: 'Vertical Line', icon: <Minus className="w-4 h-4 rotate-90" /> },
  { type: 'ray', label: 'Ray', icon: <ArrowRight className="w-4 h-4" /> },
  { type: 'channel', label: 'Channel', icon: <GitBranch className="w-4 h-4 rotate-90" /> },
  { type: 'fibonacci', label: 'Fibonacci', icon: <Percent className="w-4 h-4" /> },
  { type: 'rectangle', label: 'Rectangle', icon: <Square className="w-4 h-4" /> },
  { type: 'text', label: 'Text', icon: <Type className="w-4 h-4" /> },
  { type: 'measure', label: 'Measure', icon: <Ruler className="w-4 h-4" /> },
];

// =============================================================================
// Component
// =============================================================================

export function DrawingToolbar({ onClose }: DrawingToolbarProps) {
  const { activeDrawingTool, setActiveDrawingTool, clearAllDrawings, drawings } = useChartStore();

  return (
    <div className="bg-bg border border-border rounded-lg shadow-modal p-2">
      {/* Header */}
      <div className="flex items-center justify-between mb-2 pb-2 border-b border-border">
        <span className="text-sm font-medium text-text">Drawing Tools</span>
        <Button variant="secondary" size="sm" onClick={onClose} className="p-1">
          <X className="w-4 h-4" />
        </Button>
      </div>

      {/* Tools Grid */}
      <div className="grid grid-cols-5 gap-1">
        {DRAWING_TOOLS.map((tool) => (
          <Button
            key={tool.type}
            variant={activeDrawingTool === tool.type ? 'primary' : 'secondary'}
            size="sm"
            onClick={() => setActiveDrawingTool(tool.type)}
            className="flex flex-col items-center gap-1 p-2 h-auto"
            title={tool.label}
          >
            {tool.icon}
            <span className="text-[10px]">{tool.label}</span>
          </Button>
        ))}
      </div>

      {/* Clear All */}
      {drawings.length > 0 && (
        <div className="mt-2 pt-2 border-t border-border">
          <Button
            variant="secondary"
            size="sm"
            onClick={clearAllDrawings}
            className="w-full flex items-center justify-center gap-2 text-danger"
          >
            <Trash2 className="w-4 h-4" />
            Clear All ({drawings.length})
          </Button>
        </div>
      )}
    </div>
  );
}
