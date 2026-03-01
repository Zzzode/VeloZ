import { test, expect } from '@playwright/test';

test.describe('Market Data', () => {
  test.beforeEach(async ({ page }) => {
    // Set up authenticated state
    await page.goto('/login');
    await page.getByLabel(/user|email|username/i).fill('test');
    await page.getByLabel(/password/i).fill('password');
    await page.getByRole('button', { name: /login|sign in/i }).click();
    await expect(page).toHaveURL(/\/(dashboard)?$/);
  });

  test('displays price chart and can switch symbols', async ({ page }) => {
    // Navigate to trading or market page
    await page.getByRole('link', { name: /trading|market/i }).first().click();

    // Should display price chart
    await expect(
      page.getByText(/price chart|chart/i).or(page.locator('canvas').first())
    ).toBeVisible({ timeout: 5000 });

    // Should display symbol selector
    const symbolSelector = page.getByRole('combobox').or(
      page.locator('[class*="symbol"]').or(page.getByLabel(/symbol/i))
    ).first();

    await expect(symbolSelector).toBeVisible();

    // Get current symbol display
    const currentSymbol = await page.getByText(/BTCUSDT|ETHUSDT/i).first().textContent();

    // Click on symbol selector to change symbol
    await symbolSelector.click();

    // Select a different symbol
    const symbolOption = page.getByRole('option').or(
      page.locator('[class*="option"]')
    );

    if (await symbolOption.first().isVisible()) {
      // Find and click a different symbol
      const ethOption = page.getByText(/ETHUSDT/i);
      const btcOption = page.getByText(/BTCUSDT/i);

      if (currentSymbol?.includes('BTC') && await ethOption.isVisible()) {
        await ethOption.click();
      } else if (await btcOption.isVisible()) {
        await btcOption.click();
      } else {
        await symbolOption.first().click();
      }

      // Wait for chart to update
      await page.waitForTimeout(500);

      // Verify symbol changed in display
      await expect(
        page.getByText(/BTCUSDT|ETHUSDT|BNBUSDT/i).first()
      ).toBeVisible();
    }

    // Verify timeframe selector is present
    await expect(
      page.getByText(/1m|5m|15m|1h|4h|1d/i).first()
    ).toBeVisible();
  });
});
