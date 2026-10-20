import type { Config } from 'tailwindcss';

export default {
  content: ['./index.html', './src/**/*.{vue,ts}'],
  theme: {
    extend: {
      colors: {
        app: '#F6F7F9',
        surface: '#FFFFFF',
        viewer: '#0B1220',
        ink: '#111827',
        muted: '#6B7280',
        line: '#E5E7EB',
        focus: '#1F5A7A',
        primary: '#1F5A7A',
        'primary-hover': '#164A66',
        cad: '#000000',
        hl: '#38BDF8',
        success: '#15803D',
        warning: '#B45309',
        danger: '#B91C1C',
      },
      fontFamily: {
        sans: ['Inter', '-apple-system', 'BlinkMacSystemFont', 'PingFang SC', 'Microsoft YaHei', 'sans-serif'],
        mono: ['JetBrains Mono', 'Consolas', 'ui-monospace', 'monospace'],
      },
      boxShadow: {
        panel: '0 1px 2px rgba(17, 24, 39, 0.04), 0 1px 3px rgba(17, 24, 39, 0.06)',
        lift: '0 8px 24px -6px rgba(17, 24, 39, 0.18), 0 2px 6px rgba(17, 24, 39, 0.06)',
        drawer: '-8px 0 32px -8px rgba(17, 24, 39, 0.24)',
      },
      transitionTimingFunction: {
        industrial: 'cubic-bezier(0.2, 0.7, 0.25, 1)',
      },
      keyframes: {
        'fade-up': {
          '0%': { opacity: '0', transform: 'translateY(6px)' },
          '100%': { opacity: '1', transform: 'translateY(0)' },
        },
        shimmer: {
          '0%': { backgroundPosition: '-200% 0' },
          '100%': { backgroundPosition: '200% 0' },
        },
        'pulse-ring': {
          '0%': { boxShadow: '0 0 0 0 rgba(56, 189, 248, 0.5)' },
          '70%': { boxShadow: '0 0 0 8px rgba(56, 189, 248, 0)' },
          '100%': { boxShadow: '0 0 0 0 rgba(56, 189, 248, 0)' },
        },
      },
      animation: {
        'fade-up': 'fade-up 0.28s cubic-bezier(0.2, 0.7, 0.25, 1) both',
        shimmer: 'shimmer 1.6s linear infinite',
        'pulse-ring': 'pulse-ring 1.6s ease-out infinite',
      },
    },
  },
  plugins: [],
} satisfies Config;
