/**
 * VeloZ Baseline Tailwind Configuration
 *
 * Extracted from existing UI (apps/ui/index.html and apps/ui/css/*.css)
 * This file serves as the foundation for the React UI redesign.
 *
 * COPY THIS FILE TO: apps/ui/tailwind.config.js
 *
 * ASSUMPTIONS FOR UI-DESIGNER TO VALIDATE:
 * 1. Light theme as default (existing UI uses light theme)
 * 2. System font stack for body text, monospace for numbers/prices
 * 3. 10px border radius as standard for cards/inputs
 * 4. Color palette derived from CSS custom properties
 * 5. Spacing scale based on observed padding/margin values
 *
 * @version 1.0.0
 * @date 2026-02-23
 */

/** @type {import('tailwindcss').Config} */
export default {
  content: [
    "./index.html",
    "./src/**/*.{js,ts,jsx,tsx}",
  ],
  theme: {
    extend: {
      // =================================================================
      // COLOR PALETTE
      // Extracted from :root CSS custom properties in index.html
      // =================================================================
      colors: {
        // Primary brand color (used for buttons, active states)
        primary: {
          DEFAULT: '#111827',  // --color-primary (dark gray/black)
          hover: '#1f2937',
          light: '#374151',
        },

        // Semantic colors
        success: {
          DEFAULT: '#059669',  // --color-success (green)
          light: '#d1fae5',    // Badge background
          dark: '#065f46',     // Badge text
        },
        warning: {
          DEFAULT: '#d97706',  // --color-warning (amber)
          light: '#fef3c7',    // Badge background
          dark: '#92400e',     // Badge text
        },
        danger: {
          DEFAULT: '#dc2626',  // --color-danger (red)
          light: '#fee2e2',    // Badge background
          dark: '#991b1b',     // Badge text
        },
        info: {
          DEFAULT: '#2563eb',  // --color-info (blue)
          light: '#dbeafe',    // Badge/highlight background
          dark: '#1d4ed8',     // Badge text
        },

        // Background colors
        background: {
          DEFAULT: '#ffffff',      // --color-bg (main background)
          secondary: '#f9fafb',    // --color-bg-secondary (cards, elevated)
          tertiary: '#f3f4f6',     // Subtle backgrounds
        },

        // Border colors
        border: {
          DEFAULT: '#e5e7eb',      // --color-border
          light: '#f1f5f9',        // --color-border-light (table rows)
          input: '#d1d5db',        // Input borders
        },

        // Text colors
        text: {
          DEFAULT: '#111827',      // --color-text (primary text)
          muted: '#6b7280',        // --color-text-muted (secondary text)
          light: '#9ca3af',        // Placeholder, disabled text
        },

        // Trading-specific colors (for order book, positions)
        buy: {
          DEFAULT: '#059669',      // Same as success
          light: '#d1fae5',
          dark: '#065f46',
        },
        sell: {
          DEFAULT: '#dc2626',      // Same as danger
          light: '#fee2e2',
          dark: '#991b1b',
        },

        // Strategy type colors (from strategies.css)
        strategy: {
          momentum: '#7c3aed',
          'momentum-bg': '#ede9fe',
          'mean-reversion': '#2563eb',
          'mean-reversion-bg': '#dbeafe',
          grid: '#059669',
          'grid-bg': '#d1fae5',
          'market-making': '#d97706',
          'market-making-bg': '#fef3c7',
          'trend-following': '#db2777',
          'trend-following-bg': '#fce7f3',
        },
      },

      // =================================================================
      // TYPOGRAPHY
      // Font families extracted from CSS
      // =================================================================
      fontFamily: {
        // System font stack (body text)
        sans: [
          'ui-sans-serif',
          'system-ui',
          '-apple-system',
          'Segoe UI',
          'Roboto',
          'Helvetica',
          'Arial',
          'Apple Color Emoji',
          'Segoe UI Emoji',
          'sans-serif',
        ],
        // Monospace for prices, quantities, code
        mono: [
          'ui-monospace',
          'SFMono-Regular',
          'Menlo',
          'Monaco',
          'Consolas',
          'Liberation Mono',
          'Courier New',
          'monospace',
        ],
      },

      // Font sizes (based on observed values)
      fontSize: {
        'xxs': ['10px', { lineHeight: '14px' }],   // Labels, stat labels
        'xs': ['11px', { lineHeight: '16px' }],    // Uppercase labels
        'sm': ['12px', { lineHeight: '18px' }],    // Table headers, badges
        'base': ['13px', { lineHeight: '20px' }],  // Table cells, body
        'md': ['14px', { lineHeight: '22px' }],    // Form inputs, buttons
        'lg': ['16px', { lineHeight: '24px' }],    // Card titles, strategy names
        'xl': ['18px', { lineHeight: '28px' }],    // Section headers
        '2xl': ['20px', { lineHeight: '30px' }],   // PnL values
        '3xl': ['24px', { lineHeight: '32px' }],   // Large stats
        '4xl': ['28px', { lineHeight: '36px' }],   // Hero values
      },

      // Font weights
      fontWeight: {
        normal: '400',
        medium: '500',
        semibold: '600',
        bold: '700',
      },

      // =================================================================
      // SPACING
      // Based on observed padding/margin values in CSS
      // =================================================================
      spacing: {
        // Fine-grained spacing
        '0.5': '2px',
        '1': '4px',
        '1.5': '6px',
        '2': '8px',
        '2.5': '10px',
        '3': '12px',
        '4': '16px',
        '5': '20px',
        '6': '24px',
        '8': '32px',
        '10': '40px',
        '12': '48px',
        '16': '64px',
        '20': '80px',
        '24': '96px',
      },

      // =================================================================
      // BORDER RADIUS
      // Extracted from CSS (border-radius values)
      // =================================================================
      borderRadius: {
        'none': '0',
        'sm': '4px',       // Small buttons, badges
        'DEFAULT': '6px',  // Inputs
        'md': '8px',       // Cards, containers
        'lg': '10px',      // Main cards (observed: border-radius: 10px)
        'xl': '12px',      // Modals, dialogs
        'full': '9999px',  // Pills, badges
      },

      // =================================================================
      // BOX SHADOW
      // Extracted from CSS hover states and modals
      // =================================================================
      boxShadow: {
        'sm': '0 1px 2px rgba(0, 0, 0, 0.05)',
        'DEFAULT': '0 4px 12px rgba(0, 0, 0, 0.08)',  // Card hover
        'md': '0 4px 12px rgba(0, 0, 0, 0.1)',
        'lg': '0 4px 12px rgba(0, 0, 0, 0.15)',       // Toast, modals
        'focus-primary': '0 0 0 2px rgba(17, 24, 39, 0.2)',
        'focus-danger': '0 0 0 2px rgba(220, 38, 38, 0.2)',
        'focus-info': '0 0 0 2px rgba(37, 99, 235, 0.2)',
      },

      // =================================================================
      // TRANSITIONS
      // Based on observed transition values
      // =================================================================
      transitionDuration: {
        '150': '150ms',    // Button hover, opacity
        '200': '200ms',    // Background color, depth bars
        '300': '300ms',    // Animations, slide-in
      },

      // =================================================================
      // Z-INDEX
      // Layering system for modals, toasts, dropdowns
      // =================================================================
      zIndex: {
        'dropdown': '100',
        'sticky': '200',
        'modal-backdrop': '1000',
        'modal': '1100',
        'toast': '1200',
        'tooltip': '1300',
      },

      // =================================================================
      // BREAKPOINTS
      // Standard responsive breakpoints
      // =================================================================
      screens: {
        'xs': '480px',     // Mobile landscape
        'sm': '640px',     // Small tablets
        'md': '768px',     // Tablets
        'lg': '1024px',    // Small desktops
        'xl': '1200px',    // Container max-width
        '2xl': '1400px',   // Large desktops
      },

      // =================================================================
      // CONTAINER
      // Max-width for main content
      // =================================================================
      container: {
        center: true,
        padding: {
          DEFAULT: '16px',
          sm: '24px',
          lg: '32px',
        },
        screens: {
          sm: '640px',
          md: '768px',
          lg: '1024px',
          xl: '1200px',  // Matches existing .container max-width
        },
      },

      // =================================================================
      // ANIMATIONS
      // Keyframe animations from CSS
      // =================================================================
      keyframes: {
        'spin': {
          to: { transform: 'rotate(360deg)' },
        },
        'pulse': {
          '0%, 100%': { opacity: '1' },
          '50%': { opacity: '0.5' },
        },
        'flash-new': {
          '0%': { backgroundColor: 'rgba(37, 99, 235, 0.3)' },
          '100%': { backgroundColor: 'transparent' },
        },
        'flash-update': {
          '0%': { backgroundColor: 'rgba(234, 179, 8, 0.2)' },
          '100%': { backgroundColor: 'transparent' },
        },
        'slide-in': {
          from: { transform: 'translateX(100%)', opacity: '0' },
          to: { transform: 'translateX(0)', opacity: '1' },
        },
        'fade-in': {
          from: { opacity: '0' },
          to: { opacity: '1' },
        },
      },
      animation: {
        'spin': 'spin 0.8s linear infinite',
        'spin-fast': 'spin 0.6s linear infinite',
        'pulse': 'pulse 2s ease-in-out infinite',
        'pulse-fast': 'pulse 1s ease-in-out infinite',
        'flash-new': 'flash-new 0.3s ease',
        'flash-update': 'flash-update 0.2s ease',
        'slide-in': 'slide-in 0.3s ease',
        'fade-in': 'fade-in 0.2s ease',
      },
    },
  },

  // =================================================================
  // PLUGINS
  // =================================================================
  plugins: [
    // Add forms plugin for better form styling
    // require('@tailwindcss/forms'),

    // Custom plugin for trading-specific utilities
    function({ addUtilities }) {
      addUtilities({
        // Monospace numbers (for prices, quantities)
        '.font-tabular': {
          'font-variant-numeric': 'tabular-nums',
        },
        // Truncate with ellipsis
        '.truncate-id': {
          'max-width': '120px',
          'overflow': 'hidden',
          'text-overflow': 'ellipsis',
          'white-space': 'nowrap',
        },
        // Depth bar backgrounds (order book)
        '.depth-bar-bid': {
          'background': 'rgba(5, 150, 105, 0.15)',
        },
        '.depth-bar-ask': {
          'background': 'rgba(220, 38, 38, 0.15)',
        },
      });
    },
  ],
};
