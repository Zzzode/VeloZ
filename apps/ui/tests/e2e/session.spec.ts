import { test, expect } from '@playwright/test';

test.describe('Session Management', () => {
  test('handles session timeout and recovery', async ({ page }) => {
    // Log in first
    await page.goto('/login');
    await page.getByLabel(/user|email|username/i).fill('test');
    await page.getByLabel(/password/i).fill('password');
    await page.getByRole('button', { name: /login|sign in/i }).click();
    await expect(page).toHaveURL(/\/(dashboard)?$/);

    // Simulate session expiry by clearing auth tokens
    await page.evaluate(() => {
      // Clear auth tokens from localStorage
      localStorage.removeItem('token');
      localStorage.removeItem('auth');
      localStorage.removeItem('user');
      // Clear any session storage
      sessionStorage.clear();
    });

    // Clear cookies
    await page.context().clearCookies();

    // Try to navigate to a protected page
    await page.goto('/trading');

    // Should redirect to login or show session expired message
    await expect(
      page.getByRole('heading', { name: /login|sign in/i }).or(
        page.getByText(/session expired|please log in|unauthorized/i)
      )
    ).toBeVisible({ timeout: 5000 });

    // User should be able to log in again
    await page.getByLabel(/user|email|username/i).fill('test');
    await page.getByLabel(/password/i).fill('password');
    await page.getByRole('button', { name: /login|sign in/i }).click();

    // Should be able to access protected pages again
    await expect(page).toHaveURL(/\/(dashboard)?$/);

    // Navigate to trading to verify full recovery
    await page.getByRole('link', { name: /trading|trade/i }).click();
    await expect(page).toHaveURL(/\/trading/);
  });
});
