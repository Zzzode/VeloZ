import { QueryProvider } from './providers';
import { ToastProvider, ErrorBoundary } from '@/shared/components';
import { AppRouter } from './Router';

export function App() {
  return (
    <ErrorBoundary>
      <QueryProvider>
        <ToastProvider>
          <AppRouter />
        </ToastProvider>
      </QueryProvider>
    </ErrorBoundary>
  );
}
