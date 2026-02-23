import type { ReactElement, ReactNode } from 'react';
import { render, renderHook as rtlRenderHook } from '@testing-library/react';
import type { RenderOptions, RenderHookOptions, RenderHookResult } from '@testing-library/react';
import { QueryClient, QueryClientProvider } from '@tanstack/react-query';
import { BrowserRouter } from 'react-router-dom';

// Create a fresh QueryClient for each test
function createTestQueryClient() {
  return new QueryClient({
    defaultOptions: {
      queries: {
        retry: false,
        gcTime: 0,
        staleTime: 0,
      },
      mutations: {
        retry: false,
      },
    },
  });
}

interface CustomRenderOptions extends Omit<RenderOptions, 'wrapper'> {
  queryClient?: QueryClient;
}

function customRender(
  ui: ReactElement,
  options?: CustomRenderOptions
): ReturnType<typeof render> {
  const { queryClient, ...renderOptions } = options || {};

  const Wrapper = ({ children }: { children: ReactNode }) => {
    const client = queryClient || createTestQueryClient();
    return (
      <QueryClientProvider client={client}>
        <BrowserRouter>
          {children}
        </BrowserRouter>
      </QueryClientProvider>
    );
  };

  return render(ui, { wrapper: Wrapper, ...renderOptions });
}

// Custom renderHook with providers
function customRenderHook<Result, Props>(
  hook: (props: Props) => Result,
  options?: Omit<RenderHookOptions<Props>, 'wrapper'>
): RenderHookResult<Result, Props> {
  const Wrapper = ({ children }: { children: ReactNode }) => {
    const client = createTestQueryClient();
    return (
      <QueryClientProvider client={client}>
        <BrowserRouter>
          {children}
        </BrowserRouter>
      </QueryClientProvider>
    );
  };

  return rtlRenderHook(hook, { wrapper: Wrapper, ...options });
}

// Re-export everything from testing-library
export * from '@testing-library/react';
export { default as userEvent } from '@testing-library/user-event';

// Override render and renderHook with our custom versions
export { customRender as render, customRenderHook as renderHook };

// Export utilities
export { createTestQueryClient };
