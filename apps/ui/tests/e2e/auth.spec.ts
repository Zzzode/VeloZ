import { test, expect } from '@playwright/test';

test.describe('Authentication', () => {
  test.beforeEach(async ({ page }) => {
    // Clear any existing auth state
    await page.context().clearCookies();
    await page.evaluate(() => localStorage.clear());
  });

  test('shows login page for unauthenticated users', async ({ page }) => {
    await page.goto('/');

    // Should redirect to login or show login form
    await expect(page.getByRole('heading', { name: /login|sign in/i })).toBeVisible();
  });

  test('can log in with valid credentials', async ({ page }) => {
    await page.goto('/login');

    // Fill in credentials
    await page.getByLabel(/user|email|username/i).fill('test');
    await page.getByLabel(/password/i).fill('password');

    // Submit form
    await page.getByRole('button', { name: /login|sign in/i }).click();

    // Should redirect to dashboard after successful login
    await expect(page).toHaveURL(/\/(dashboard)?$/);
  });

  test('shows error for invalid credentials', async ({ page }) => {
    await page.goto('/login');

    // Fill in invalid credentials
    await page.getByLabel(/user|email|username/i).fill('invalid');
    await page.getByLabel(/password/i).fill('wrongpassword');

    // Submit form
    await page.getByRole('button', { name: /login|sign in/i }).click();

    // Should show error message
    await expect(page.getByText(/invalid|incorrect|error/i)).toBeVisible();
  });

  test('can log out', async ({ page }) => {
    // First log in
    await page.goto('/login');
    await page.getByLabel(/user|email|username/i).fill('test');
    await page.getByLabel(/password/i).fill('password');
    await page.getByRole('button', { name: /login|sign in/i }).click();

    // Wait for dashboard
    await expect(page).toHaveURL(/\/(dashboard)?$/);

    // Find and click logout button
    await page.getByRole('button', { name: /logout|sign out/i }).click();

    // Should redirect to login
    await expect(page.getByRole('heading', { name: /login|sign in/i })).toBeVisible();
  });

  test('protected routes redirect to login', async ({ page }) => {
    // Try to access protected route
    await page.goto('/dashboard');

    // Should redirect to login
    await expect(page.getByRole('heading', { name: /login|sign in/i })).toBeVisible();
  });

  test('remembers auth state on page refresh', async ({ page }) => {
    // Log in
    await page.goto('/login');
    await page.getByLabel(/user|email|username/i).fill('test');
    await page.getByLabel(/password/i).fill('password');
    await page.getByRole('button', { name: /login|sign in/i }).click();

    // Wait for dashboard
    await expect(page).toHaveURL(/\/(dashboard)?$/);

    // Refresh page
    await page.reload();

    // Should still be on dashboard (auth state preserved)
    await expect(page).toHaveURL(/\/(dashboard)?$/);
  });
});
