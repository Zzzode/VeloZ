import { createBrowserRouter, RouterProvider, Navigate } from 'react-router-dom';
import { lazy, Suspense } from 'react';
import { MainLayout, LoadingSpinner } from '@/shared/components';
import { ProtectedRoute, Login } from '@/features/auth';

// Lazy load feature modules
const Dashboard = lazy(() => import('@/features/dashboard'));
const Trading = lazy(() => import('@/features/trading'));
const Market = lazy(() => import('@/features/market'));
const Strategies = lazy(() => import('@/features/strategies'));
const Backtest = lazy(() => import('@/features/backtest'));
const Settings = lazy(() => import('@/features/settings'));

const router = createBrowserRouter([
  {
    path: '/login',
    element: <Login />,
  },
  {
    path: '/',
    element: (
      <ProtectedRoute>
        <MainLayout />
      </ProtectedRoute>
    ),
    children: [
      {
        index: true,
        element: <Navigate to="/dashboard" replace />,
      },
      {
        path: 'dashboard',
        element: (
          <Suspense fallback={<LoadingSpinner />}>
            <Dashboard />
          </Suspense>
        ),
      },
      {
        path: 'trading',
        element: (
          <Suspense fallback={<LoadingSpinner />}>
            <Trading />
          </Suspense>
        ),
      },
      {
        path: 'market',
        element: (
          <Suspense fallback={<LoadingSpinner />}>
            <Market />
          </Suspense>
        ),
      },
      {
        path: 'strategies',
        element: (
          <Suspense fallback={<LoadingSpinner />}>
            <Strategies />
          </Suspense>
        ),
      },
      {
        path: 'strategies/:id',
        element: (
          <Suspense fallback={<LoadingSpinner />}>
            <Strategies />
          </Suspense>
        ),
      },
      {
        path: 'backtest',
        element: (
          <Suspense fallback={<LoadingSpinner />}>
            <Backtest />
          </Suspense>
        ),
      },
      {
        path: 'settings',
        element: (
          <Suspense fallback={<LoadingSpinner />}>
            <Settings />
          </Suspense>
        ),
      },
    ],
  },
  {
    path: '*',
    element: <Navigate to="/dashboard" replace />,
  },
]);

export function AppRouter() {
  return <RouterProvider router={router} />;
}
