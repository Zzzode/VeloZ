import { test, expect } from '@playwright/test';

test.describe('Dashboard', () => {
  test.beforeEach(async ({ page }) => {
    // Set up authenticated state
    await page.goto('/login');
    await page.getByLabel(/user|email|username/i).fill('test');
    await page.getByLabel(/password/i).fill('password');
    await page.getByRole('button', { name: /login|sign in/i }).click();
    await expect(page).toHaveURL(/\/(dashboard)?$/);
  });

  test('displays dashboard with key metrics and real-time updates', async ({ page }) => {
    await page.goto('/dashboard');

    // Dashboard should show key sections
    await expect(page.getByText(/dashboard|overview/i).first()).toBeVisible();

    // Should display engine status
    await expect(
      page.getByText(/engine|status|running|stopped/i).first()
    ).toBeVisible();

    // Should display account/portfolio information
    await expect(
      page.getByText(/account|balance|portfolio|positions/i).first()
    ).toBeVisible();

    // Should display strategies section
    await expect(
      page.getByText(/strategies|active/i).first()
    ).toBeVisible();

    // Verify real-time update capability - check for connection status indicator
    await expect(
      page.getByText(/connected|live|real-time/i).or(
        page.locator('[class*="status"]').first()
      )
    ).toBeVisible({ timeout: 5000 });
  });
});
