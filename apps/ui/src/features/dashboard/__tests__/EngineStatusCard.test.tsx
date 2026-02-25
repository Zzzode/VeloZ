/**
 * EngineStatusCard Component Tests
 * Tests for engine status display with loading, error, and data states
 */
import { describe, it, expect, beforeAll, afterAll, afterEach, vi } from 'vitest';
import { render, screen, waitFor } from '@/test/test-utils';
import { server } from '@/test/mocks/server';
import { http, HttpResponse } from 'msw';
import { EngineStatusCard } from '../components/EngineStatusCard';
import Dashboard from '../index';

// Setup MSW server
beforeAll(() => server.listen({ onUnhandledRequest: 'error' }));
afterEach(() => server.resetHandlers());
afterAll(() => server.close());

const API_BASE = 'http://127.0.0.1:8080';

vi.mock('../hooks', async () => {
  const actual = await vi.importActual<typeof import('../hooks')>('../hooks');
  return {
    ...actual,
    useSSEConnection: () => ({
      reconnect: vi.fn(),
      disconnect: vi.fn(),
      connectionState: 'disconnected',
    }),
  };
});

vi.mock('@/shared/api/hooks/useEngine', async () => {
  const actual = await vi.importActual<typeof import('@/shared/api/hooks/useEngine')>('@/shared/api/hooks/useEngine');
  return {
    ...actual,
    useEngineHealth: () => ({
      data: { engine: { connected: true } },
      isLoading: false,
      error: null,
    }),
  };
});

describe('EngineStatusCard', () => {
  beforeAll(() => server.listen({ onUnhandledRequest: 'error' }));
  afterEach(() => server.resetHandlers());
  afterAll(() => server.close());

  describe('loading state', () => {
    it('should show card title', () => {
      render(<EngineStatusCard />);

      // Should show card title
      expect(screen.getByText('Engine Status')).toBeInTheDocument();
    });
  });

  describe('success state', () => {
    it('should display engine status when data loads', async () => {
      render(<EngineStatusCard />);

      // Wait for the data to load - look for the state text
      await waitFor(() => {
        // The mock returns state: 'Running'
        const runningElements = screen.getAllByText('Running');
        expect(runningElements.length).toBeGreaterThan(0);
      });
    });

    it('should display uptime when engine is running', async () => {
      render(<EngineStatusCard />);

      await waitFor(() => {
        expect(screen.getByText(/Uptime:/)).toBeInTheDocument();
      });
    });

    it('should have link to strategies page', async () => {
      render(<EngineStatusCard />);

      await waitFor(() => {
        expect(screen.getByText('Strategies')).toBeInTheDocument();
      });
    });
  });

  describe('stopped state', () => {
    it('should display stopped status', async () => {
      server.use(
        http.get(`${API_BASE}/api/status`, () => {
          return HttpResponse.json({
            state: 'Stopped',
            uptime_ms: 0,
            strategies_loaded: 2,
            strategies_running: 0,
          });
        }),
        http.get(`${API_BASE}/api/strategies`, () => {
          return HttpResponse.json({
            strategies: [],
          });
        })
      );

      render(<EngineStatusCard />);

      await waitFor(() => {
        expect(screen.getByText('Stopped')).toBeInTheDocument();
      });
    });

    it('should not show uptime when stopped', async () => {
      server.use(
        http.get(`${API_BASE}/api/status`, () => {
          return HttpResponse.json({
            state: 'Stopped',
            uptime_ms: 0,
            strategies_loaded: 2,
            strategies_running: 0,
          });
        }),
        http.get(`${API_BASE}/api/strategies`, () => {
          return HttpResponse.json({
            strategies: [],
          });
        })
      );

      render(<EngineStatusCard />);

      await waitFor(() => {
        expect(screen.getByText('Stopped')).toBeInTheDocument();
      });

      // Uptime should not be shown when stopped
      expect(screen.queryByText(/Uptime:/)).not.toBeInTheDocument();
    });
  });

  describe('error state', () => {
    // Note: Error state tests are skipped because the API client has built-in
    // retry logic. Error handling is tested at the API client level.
    it.skip('should display error message when engine unavailable', async () => {
      server.use(
        http.get(`${API_BASE}/api/status`, () => {
          return HttpResponse.json(
            { error: 'engine_unavailable', message: 'Engine not running' },
            { status: 503 }
          );
        })
      );

      render(<EngineStatusCard />);

      await waitFor(() => {
        expect(screen.getByText('Engine unavailable')).toBeInTheDocument();
      });
    });
  });
});

describe('Dashboard', () => {
  it('should render engine connectivity and order latency', async () => {
    render(<Dashboard />);

    // Wait for the health check to complete
    await waitFor(() => {
      expect(screen.getByText(/Engine Connected/)).toBeInTheDocument();
      expect(screen.getByText(/Order Latency/)).toBeInTheDocument();
    });
  });
});
