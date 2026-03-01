import { test, expect } from '@playwright/test';

test.describe('Strategy Management', () => {
  test.beforeEach(async ({ page }) => {
    // Set up authenticated state
    await page.goto('/login');
    await page.getByLabel(/user|email|username/i).fill('test');
    await page.getByLabel(/password/i).fill('password');
    await page.getByRole('button', { name: /login|sign in/i }).click();
    await expect(page).toHaveURL(/\/(dashboard)?$/);
  });

  test('displays strategy list and can view strategy details', async ({ page }) => {
    // Navigate to strategies page
    await page.getByRole('link', { name: /strategies/i }).click();
    await expect(page).toHaveURL(/\/strategies/);

    // Should display list of strategies
    await expect(page.getByText(/strategies/i).first()).toBeVisible();

    // Should show strategy cards or list items
    const strategyItems = page.getByRole('article').or(
      page.locator('[class*="strategy"]').or(page.locator('[class*="card"]'))
    );

    // Wait for strategies to load
    await expect(strategyItems.first()).toBeVisible({ timeout: 5000 });

    // Click on a strategy to view details
    await strategyItems.first().click();

    // Should show strategy details (name, status, parameters)
    await expect(
      page.getByText(/parameters|configuration|settings|details/i).first()
    ).toBeVisible();
  });

  test('can start and stop a strategy', async ({ page }) => {
    await page.goto('/strategies');

    // Wait for strategies to load
    await page.waitForTimeout(1000);

    // Find a strategy card
    const strategyCard = page.locator('[class*="card"]').first();

    if (await strategyCard.isVisible()) {
      // Look for start/stop button
      const startButton = page.getByRole('button', { name: /start|run|activate/i }).first();
      const stopButton = page.getByRole('button', { name: /stop|pause|deactivate/i }).first();

      if (await startButton.isVisible()) {
        // Start the strategy
        await startButton.click();

        // Should show running state or success message
        await expect(
          page.getByText(/running|started|active|success/i).first()
        ).toBeVisible({ timeout: 5000 });
      } else if (await stopButton.isVisible()) {
        // Stop the strategy
        await stopButton.click();

        // Should show stopped state or success message
        await expect(
          page.getByText(/stopped|paused|inactive|success/i).first()
        ).toBeVisible({ timeout: 5000 });
      }
    }

    // Verify strategy state badges are displayed
    await expect(
      page.getByText(/running|stopped|paused/i).first()
    ).toBeVisible();
  });
});
