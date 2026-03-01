import type { HTMLAttributes, ReactNode } from 'react';

export interface CardProps extends HTMLAttributes<HTMLDivElement> {
  title?: string;
  subtitle?: string;
  headerAction?: ReactNode;
  footer?: ReactNode;
  noPadding?: boolean;
}

export function Card({
  title,
  subtitle,
  headerAction,
  footer,
  noPadding = false,
  className = '',
  children,
  ...props
}: CardProps) {
  const hasHeader = title || subtitle || headerAction;

  return (
    <div
      className={`bg-background border border-border rounded-lg shadow-card ${className}`}
      {...props}
    >
      {hasHeader && (
        <div className="flex items-center justify-between px-4 py-3 border-b border-border">
          <div>
            {title && (
              <h3 className="text-lg font-semibold text-text">{title}</h3>
            )}
            {subtitle && (
              <p className="text-sm text-text-muted mt-0.5">{subtitle}</p>
            )}
          </div>
          {headerAction && <div>{headerAction}</div>}
        </div>
      )}
      <div className={noPadding ? '' : 'p-4'}>{children}</div>
      {footer && (
        <div className="px-4 py-3 border-t border-border bg-background-secondary rounded-b-lg">
          {footer}
        </div>
      )}
    </div>
  );
}
