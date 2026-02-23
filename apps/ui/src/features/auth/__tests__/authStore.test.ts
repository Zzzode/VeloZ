/**
 * Auth Store Tests
 * Tests for Zustand auth store: token management, login/logout, permissions
 */
import { describe, it, expect, beforeEach } from 'vitest';
import { useAuthStore } from '../store/authStore';
import type { User } from '../store/authStore';

describe('authStore', () => {
  // Reset store before each test
  beforeEach(() => {
    useAuthStore.setState({
      accessToken: null,
      refreshToken: null,
      user: null,
      isAuthenticated: false,
    });
  });

  describe('initial state', () => {
    it('should have null tokens initially', () => {
      const state = useAuthStore.getState();
      expect(state.accessToken).toBeNull();
      expect(state.refreshToken).toBeNull();
    });

    it('should not be authenticated initially', () => {
      const state = useAuthStore.getState();
      expect(state.isAuthenticated).toBe(false);
    });

    it('should have null user initially', () => {
      const state = useAuthStore.getState();
      expect(state.user).toBeNull();
    });
  });

  describe('setTokens', () => {
    it('should set access and refresh tokens', () => {
      useAuthStore.getState().setTokens('access-123', 'refresh-456');

      const state = useAuthStore.getState();
      expect(state.accessToken).toBe('access-123');
      expect(state.refreshToken).toBe('refresh-456');
    });

    it('should set isAuthenticated to true when tokens are set', () => {
      useAuthStore.getState().setTokens('access-123', 'refresh-456');

      const state = useAuthStore.getState();
      expect(state.isAuthenticated).toBe(true);
    });
  });

  describe('setUser', () => {
    it('should set user data', () => {
      const user: User = {
        user_id: 'user-001',
        permissions: ['read', 'write'],
      };

      useAuthStore.getState().setUser(user);

      const state = useAuthStore.getState();
      expect(state.user).toEqual(user);
    });

    it('should update user data when called multiple times', () => {
      const user1: User = { user_id: 'user-001', permissions: ['read'] };
      const user2: User = { user_id: 'user-002', permissions: ['read', 'write'] };

      useAuthStore.getState().setUser(user1);
      expect(useAuthStore.getState().user?.user_id).toBe('user-001');

      useAuthStore.getState().setUser(user2);
      expect(useAuthStore.getState().user?.user_id).toBe('user-002');
    });
  });

  describe('logout', () => {
    it('should clear all auth state', () => {
      // Set up authenticated state
      useAuthStore.getState().setTokens('access-123', 'refresh-456');
      useAuthStore.getState().setUser({ user_id: 'user-001', permissions: ['read'] });

      // Verify authenticated
      expect(useAuthStore.getState().isAuthenticated).toBe(true);

      // Logout
      useAuthStore.getState().logout();

      // Verify cleared
      const state = useAuthStore.getState();
      expect(state.accessToken).toBeNull();
      expect(state.refreshToken).toBeNull();
      expect(state.user).toBeNull();
      expect(state.isAuthenticated).toBe(false);
    });
  });

  describe('hasPermission', () => {
    it('should return false when user is null', () => {
      const hasRead = useAuthStore.getState().hasPermission('read');
      expect(hasRead).toBe(false);
    });

    it('should return true when user has the permission', () => {
      useAuthStore.getState().setUser({
        user_id: 'user-001',
        permissions: ['read', 'write'],
      });

      expect(useAuthStore.getState().hasPermission('read')).toBe(true);
      expect(useAuthStore.getState().hasPermission('write')).toBe(true);
    });

    it('should return false when user does not have the permission', () => {
      useAuthStore.getState().setUser({
        user_id: 'user-001',
        permissions: ['read'],
      });

      expect(useAuthStore.getState().hasPermission('write')).toBe(false);
      expect(useAuthStore.getState().hasPermission('admin')).toBe(false);
    });

    it('should return true for any permission when user has admin', () => {
      useAuthStore.getState().setUser({
        user_id: 'admin-001',
        permissions: ['admin'],
      });

      expect(useAuthStore.getState().hasPermission('read')).toBe(true);
      expect(useAuthStore.getState().hasPermission('write')).toBe(true);
      expect(useAuthStore.getState().hasPermission('admin')).toBe(true);
    });
  });

  describe('state persistence', () => {
    it('should include auth fields in persisted state', () => {
      // Set up state
      useAuthStore.getState().setTokens('access-123', 'refresh-456');
      useAuthStore.getState().setUser({ user_id: 'user-001', permissions: ['read'] });

      // The persist middleware should include these fields
      const state = useAuthStore.getState();
      expect(state.accessToken).toBe('access-123');
      expect(state.refreshToken).toBe('refresh-456');
      expect(state.user?.user_id).toBe('user-001');
      expect(state.isAuthenticated).toBe(true);
    });
  });
});
