/**
 * MSW Server Setup
 * Configure mock server for Node.js testing environment
 */

import { setupServer } from 'msw/node';
import { handlers } from './handlers';

// Create the mock server with default handlers
export const server = setupServer(...handlers);

// Export for use in tests
export { handlers };
