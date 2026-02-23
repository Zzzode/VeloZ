import { Navigate, useLocation } from 'react-router-dom';
import { useAuthStore, type Permission } from '../store';

interface ProtectedRouteProps {
  children: React.ReactNode;
  requiredPermission?: Permission;
}

// Check if auth is enabled via environment variable
const AUTH_ENABLED = import.meta.env.VITE_AUTH_ENABLED === 'true';

export function ProtectedRoute({ children, requiredPermission }: ProtectedRouteProps) {
  const location = useLocation();
  const { isAuthenticated, hasPermission } = useAuthStore();

  // If auth is disabled, allow all access
  if (!AUTH_ENABLED) {
    return <>{children}</>;
  }

  // Redirect to login if not authenticated
  if (!isAuthenticated) {
    return <Navigate to="/login" state={{ from: location }} replace />;
  }

  // Check permission if required
  if (requiredPermission && !hasPermission(requiredPermission)) {
    return <Navigate to="/dashboard" replace />;
  }

  return <>{children}</>;
}
