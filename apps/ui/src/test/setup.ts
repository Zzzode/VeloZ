import '@testing-library/jest-dom/vitest';
import { cleanup } from '@testing-library/react';
import { afterEach, afterAll, beforeAll, vi } from 'vitest';
import React from 'react';
import { server } from './mocks/server';

// Start MSW server before all tests
beforeAll(() => {
  server.listen({ onUnhandledRequest: 'error' });
});

// Reset handlers after each test
afterEach(() => {
  cleanup();
  server.resetHandlers();
});

// Close MSW server after all tests
afterAll(() => {
  server.close();
});

// Mock window.matchMedia
Object.defineProperty(window, 'matchMedia', {
  writable: true,
  value: vi.fn().mockImplementation((query: string) => ({
    matches: false,
    media: query,
    onchange: null,
    addListener: vi.fn(),
    removeListener: vi.fn(),
    addEventListener: vi.fn(),
    removeEventListener: vi.fn(),
    dispatchEvent: vi.fn(),
  })),
});

// Mock ResizeObserver
class ResizeObserverMock {
  observe = vi.fn();
  unobserve = vi.fn();
  disconnect = vi.fn();
}

window.ResizeObserver = ResizeObserverMock;

// Mock IntersectionObserver
class IntersectionObserverMock {
  readonly root: Element | null = null;
  readonly rootMargin: string = '';
  readonly thresholds: ReadonlyArray<number> = [];
  observe = vi.fn();
  unobserve = vi.fn();
  disconnect = vi.fn();
  takeRecords = vi.fn().mockReturnValue([]);
}

window.IntersectionObserver = IntersectionObserverMock;

// Mock localStorage
const localStorageMock = {
  getItem: vi.fn(),
  setItem: vi.fn(),
  removeItem: vi.fn(),
  clear: vi.fn(),
  length: 0,
  key: vi.fn(),
};

Object.defineProperty(window, 'localStorage', {
  value: localStorageMock,
});

// Mock scrollTo
window.scrollTo = vi.fn();

// Mock Recharts ResponsiveContainer
vi.mock('recharts', async (importOriginal) => {
  const OriginalModule = await importOriginal<typeof import('recharts')>();
  return {
    ...OriginalModule,
    ResponsiveContainer: ({ children }: { children: React.ReactElement }) => 
      React.createElement('div', { style: { width: 800, height: 800 } },
        React.isValidElement(children)
          // eslint-disable-next-line @typescript-eslint/no-explicit-any
          ? React.cloneElement(children, { width: 800, height: 800 } as any)
          : children
      ),
  };
});

// Mock Web Animations API
if (typeof Element !== 'undefined') {
  Element.prototype.animate = vi.fn().mockReturnValue({
    finished: Promise.resolve(),
    cancel: vi.fn(),
    finish: vi.fn(),
    play: vi.fn(),
    pause: vi.fn(),
    commitStyles: vi.fn(),
    currentTime: 0,
    effect: null,
    id: '',
    oncancel: null,
    onfinish: null,
    onremove: null,
    pending: false,
    playbackRate: 1,
    playState: 'idle',
    replaceState: 'active',
    startTime: 0,
    timeline: null,
    reverse: vi.fn(),
    addEventListener: vi.fn(),
    removeEventListener: vi.fn(),
    dispatchEvent: vi.fn(),
  });

  Element.prototype.getAnimations = vi.fn().mockReturnValue([]);
}

// Suppress console errors in tests (optional, can be removed if you want to see errors)
// vi.spyOn(console, 'error').mockImplementation(() => {});
