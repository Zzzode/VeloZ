/**
 * Shared API Client Instance
 * Singleton API client for use across all hooks
 */

import { createApiClient } from '../api-client';
import { createSSEClient, VelozSSEClient } from '../sse-client';
import { createMarketWsClient, createOrderWsClient, VelozMarketWsClient, VelozOrderWsClient } from '../ws-client';
import type { VelozApiClient } from '../api-client';

// API base URL from environment
export const API_BASE = import.meta.env.VITE_API_BASE || 'http://127.0.0.1:8080';

// Singleton instances
let apiClientInstance: VelozApiClient | null = null;
let sseClientInstance: VelozSSEClient | null = null;
let marketWsClientInstance: VelozMarketWsClient | null = null;
let orderWsClientInstance: VelozOrderWsClient | null = null;

/**
 * Get the shared API client instance
 */
export function getApiClient(): VelozApiClient {
  if (!apiClientInstance) {
    apiClientInstance = createApiClient(API_BASE);
  }
  return apiClientInstance;
}

/**
 * Get the shared SSE client instance
 */
export function getSSEClient(): VelozSSEClient {
  if (!sseClientInstance) {
    sseClientInstance = createSSEClient(API_BASE);
  }
  return sseClientInstance!;
}

/**
 * Get the shared Market WebSocket client instance
 */
export function getMarketWsClient(): VelozMarketWsClient {
  if (!marketWsClientInstance) {
    marketWsClientInstance = createMarketWsClient(API_BASE);
  }
  return marketWsClientInstance!;
}

/**
 * Get the shared Order WebSocket client instance
 */
export function getOrderWsClient(): VelozOrderWsClient {
  if (!orderWsClientInstance) {
    orderWsClientInstance = createOrderWsClient(API_BASE);
  }
  return orderWsClientInstance!;
}

/**
 * Reset all client instances (useful for testing or logout)
 */
export function resetClients(): void {
  if (sseClientInstance) {
    sseClientInstance.disconnect();
    sseClientInstance = null;
  }
  if (marketWsClientInstance) {
    marketWsClientInstance.disconnect();
    marketWsClientInstance = null;
  }
  if (orderWsClientInstance) {
    orderWsClientInstance.disconnect();
    orderWsClientInstance = null;
  }
  if (apiClientInstance) {
    apiClientInstance.clearTokens();
    apiClientInstance = null;
  }
}
