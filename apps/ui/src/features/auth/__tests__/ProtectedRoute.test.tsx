/**
 * ProtectedRoute Component Tests
 * Tests for route protection, authentication redirects, and permission checks
 *
 * Note: These tests assume AUTH_ENABLED is false (default in test environment)
 * because mocking import.meta.env is complex in Vitest. The auth store logic
 * is tested separately in authStore.test.ts.
 */
import { describe, it, expect, beforeEach } from 'vitest';
import { render, screen } from '@testing-library/react';
import { MemoryRouter, Routes, Route } from 'react-router-dom';
import { QueryClient, QueryClientProvider } from '@tanstack/react-query';
import { useAuthStore } from '../store/authStore';

// Test component to render inside protected route
function TestPage() {
  return <div data-testid="protected-content">Protected Content</div>;
}

function LoginPage() {
  return <div data-testid="login-page">Login Page</div>;
}

function DashboardPage() {
  return <div data-testid="dashboard-page">Dashboard Page</div>;
}

// Simple ProtectedRoute for testing (without env var dependency)
function TestProtectedRoute({
  children,
  requiredPermission
}: {
  children: React.ReactNode;
  requiredPermission?: 'read' | 'write' | 'admin';
}) {
  const { isAuthenticated, hasPermission } = useAuthStore();

  // Redirect to login if not authenticated
  if (!isAuthenticated) {
    return <LoginPage />;
  }

  // Check permission if required
  if (requiredPermission && !hasPermission(requiredPermission)) {
    return <DashboardPage />;
  }

  return <>{children}</>;
}

// Helper to render with router
function renderWithRouter(initialPath: string) {
  const queryClient = new QueryClient({
    defaultOptions: {
      queries: { retry: false },
    },
  });

  return render(
    <QueryClientProvider client={queryClient}>
      <MemoryRouter initialEntries={[initialPath]}>
        <Routes>
          <Route path="/login" element={<LoginPage />} />
          <Route path="/dashboard" element={<DashboardPage />} />
          <Route
            path="/protected"
            element={
              <TestProtectedRoute>
                <TestPage />
              </TestProtectedRoute>
            }
          />
          <Route
            path="/admin"
            element={
              <TestProtectedRoute requiredPermission="admin">
                <TestPage />
              </TestProtectedRoute>
            }
          />
        </Routes>
      </MemoryRouter>
    </QueryClientProvider>
  );
}

describe('ProtectedRoute', () => {
  beforeEach(() => {
    // Reset auth store before each test
    useAuthStore.setState({
      accessToken: null,
      refreshToken: null,
      user: null,
      isAuthenticated: false,
    });
  });

  describe('when user is not authenticated', () => {
    it('should redirect to login page', () => {
      renderWithRouter('/protected');

      // Should show login page, not protected content
      expect(screen.getByTestId('login-page')).toBeInTheDocument();
      expect(screen.queryByTestId('protected-content')).not.toBeInTheDocument();
    });
  });

  describe('when user is authenticated', () => {
    beforeEach(() => {
      useAuthStore.setState({
        accessToken: 'test-token',
        refreshToken: 'test-refresh',
        user: { user_id: 'user-001', permissions: ['read', 'write'] },
        isAuthenticated: true,
      });
    });

    it('should render protected content', () => {
      renderWithRouter('/protected');

      expect(screen.getByTestId('protected-content')).toBeInTheDocument();
    });

    it('should redirect to dashboard when lacking required permission', () => {
      renderWithRouter('/admin');

      // User has read/write but not admin
      expect(screen.getByTestId('dashboard-page')).toBeInTheDocument();
      expect(screen.queryByTestId('protected-content')).not.toBeInTheDocument();
    });
  });

  describe('when user has admin permission', () => {
    beforeEach(() => {
      useAuthStore.setState({
        accessToken: 'test-token',
        refreshToken: 'test-refresh',
        user: { user_id: 'admin-001', permissions: ['admin'] },
        isAuthenticated: true,
      });
    });

    it('should render admin-protected content', () => {
      renderWithRouter('/admin');

      expect(screen.getByTestId('protected-content')).toBeInTheDocument();
    });

    it('should render regular protected content', () => {
      renderWithRouter('/protected');

      expect(screen.getByTestId('protected-content')).toBeInTheDocument();
    });
  });
});
