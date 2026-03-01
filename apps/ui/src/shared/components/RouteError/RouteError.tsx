import { useRouteError, isRouteErrorResponse } from 'react-router-dom';
import { AlertTriangle, RefreshCw } from 'lucide-react';
import { Button } from '../Button';

export function RouteError() {
  const error = useRouteError();
  
  let errorMessage = 'An unexpected error occurred';
  if (isRouteErrorResponse(error)) {
    errorMessage = `${error.status} ${error.statusText}`;
  } else if (error instanceof Error) {
    errorMessage = error.message;
  } else if (typeof error === 'string') {
    errorMessage = error;
  }

  const handleReload = () => {
    window.location.reload();
  };

  return (
    <div className="flex flex-col items-center justify-center min-h-screen p-8 bg-background">
      <div className="flex items-center justify-center w-16 h-16 rounded-full bg-danger/10 mb-4">
        <AlertTriangle className="w-8 h-8 text-danger" />
      </div>
      <h2 className="text-xl font-semibold text-text mb-2">
        Application Error
      </h2>
      <p className="text-text-muted text-center max-w-md mb-6 font-mono text-sm bg-background-secondary p-4 rounded border border-border">
        {errorMessage}
      </p>
      <div className="flex gap-3">
        <Button
          variant="primary"
          onClick={handleReload}
          leftIcon={<RefreshCw className="w-4 h-4" />}
        >
          Reload Application
        </Button>
      </div>
    </div>
  );
}
