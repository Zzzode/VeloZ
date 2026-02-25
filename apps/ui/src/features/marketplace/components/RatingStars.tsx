/**
 * Rating Stars Component
 * Displays star rating with optional review count
 */

import { Star } from 'lucide-react';

interface RatingStarsProps {
  rating: number;
  reviewCount?: number;
  showCount?: boolean;
  size?: 'sm' | 'md' | 'lg';
  className?: string;
}

export function RatingStars({
  rating,
  reviewCount,
  showCount = true,
  size = 'md',
  className = '',
}: RatingStarsProps) {
  const maxStars = 5;
  const fullStars = Math.floor(rating);
  const hasHalfStar = rating % 1 >= 0.5;

  const sizeClasses = {
    sm: { icon: 'w-3 h-3', text: 'text-xs', gap: 'gap-0.5' },
    md: { icon: 'w-4 h-4', text: 'text-sm', gap: 'gap-1' },
    lg: { icon: 'w-5 h-5', text: 'text-base', gap: 'gap-1' },
  };

  const { icon, text, gap } = sizeClasses[size];

  return (
    <div
      className={`flex items-center ${gap} ${className}`}
      role="img"
      aria-label={`${rating.toFixed(1)} out of ${maxStars} stars${reviewCount ? `, ${reviewCount} reviews` : ''}`}
    >
      <div className={`flex ${gap}`}>
        {Array.from({ length: maxStars }).map((_, index) => {
          const isFilled = index < fullStars;
          const isHalf = index === fullStars && hasHalfStar;

          return (
            <div key={index} className="relative">
              <Star
                className={`${icon} ${isFilled || isHalf ? 'text-warning fill-warning' : 'text-border'}`}
                aria-hidden="true"
              />
              {isHalf && (
                <div className="absolute inset-0 overflow-hidden w-1/2">
                  <Star
                    className={`${icon} text-warning fill-warning`}
                    aria-hidden="true"
                  />
                </div>
              )}
            </div>
          );
        })}
      </div>
      <span className={`${text} font-semibold text-warning`}>
        {rating.toFixed(1)}
      </span>
      {showCount && reviewCount !== undefined && (
        <span className={`${text} text-text-muted`}>
          ({reviewCount.toLocaleString()})
        </span>
      )}
    </div>
  );
}
