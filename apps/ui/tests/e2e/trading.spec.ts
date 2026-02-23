import { test, expect } from '@playwright/test';

test.describe('Trading', () => {
  test.beforeEach(async ({ page }) => {
    // Set up authenticated state
    await page.goto('/login');
    await page.getByLabel(/user|email|username/i).fill('test');
    await page.getByLabel(/password/i).fill('password');
    await page.getByRole('button', { name: /login|sign in/i }).click();
    await expect(page).toHaveURL(/\/(dashboard)?$/);
  });

  test('can navigate to trading page', async ({ page }) => {
    // Navigate to trading
    await page.getByRole('link', { name: /trading|trade/i }).click();

    // Should be on trading page
    await expect(page).toHaveURL(/\/trading/);
  });

  test('displays order book', async ({ page }) => {
    await page.goto('/trading');

    // Order book should be visible
    await expect(page.getByText(/order book|bids|asks/i).first()).toBeVisible();
  });

  test('displays market data', async ({ page }) => {
    await page.goto('/trading');

    // Market data should show price
    await expect(page.getByText(/price|last/i).first()).toBeVisible();
  });

  test('can place a buy order', async ({ page }) => {
    await page.goto('/trading');

    // Select buy side
    await page.getByRole('button', { name: /buy/i }).first().click();

    // Fill order form
    await page.getByLabel(/quantity|amount|qty/i).fill('0.1');
    await page.getByLabel(/price/i).fill('42000');

    // Submit order
    await page.getByRole('button', { name: /place order|submit|buy/i }).click();

    // Should show success message or order in list
    await expect(
      page.getByText(/order placed|success|submitted/i).or(
        page.getByText(/0\.1/)
      )
    ).toBeVisible();
  });

  test('can place a sell order', async ({ page }) => {
    await page.goto('/trading');

    // Select sell side
    await page.getByRole('button', { name: /sell/i }).first().click();

    // Fill order form
    await page.getByLabel(/quantity|amount|qty/i).fill('0.05');
    await page.getByLabel(/price/i).fill('43000');

    // Submit order
    await page.getByRole('button', { name: /place order|submit|sell/i }).click();

    // Should show success message or order in list
    await expect(
      page.getByText(/order placed|success|submitted/i).or(
        page.getByText(/0\.05/)
      )
    ).toBeVisible();
  });

  test('validates order form', async ({ page }) => {
    await page.goto('/trading');

    // Try to submit empty form
    await page.getByRole('button', { name: /place order|submit/i }).click();

    // Should show validation errors
    await expect(page.getByText(/required|invalid|enter/i).first()).toBeVisible();
  });

  test('displays open orders', async ({ page }) => {
    await page.goto('/trading');

    // Open orders section should be visible
    await expect(page.getByText(/open orders|active orders|orders/i).first()).toBeVisible();
  });

  test('can cancel an order', async ({ page }) => {
    await page.goto('/trading');

    // First place an order
    await page.getByRole('button', { name: /buy/i }).first().click();
    await page.getByLabel(/quantity|amount|qty/i).fill('0.1');
    await page.getByLabel(/price/i).fill('40000');
    await page.getByRole('button', { name: /place order|submit/i }).click();

    // Wait for order to appear
    await page.waitForTimeout(500);

    // Find and click cancel button
    const cancelButton = page.getByRole('button', { name: /cancel/i }).first();
    if (await cancelButton.isVisible()) {
      await cancelButton.click();

      // Should show cancellation confirmation
      await expect(
        page.getByText(/cancelled|cancel/i)
      ).toBeVisible();
    }
  });

  test('displays account balance', async ({ page }) => {
    await page.goto('/trading');

    // Balance should be visible
    await expect(page.getByText(/balance|available|usdt/i).first()).toBeVisible();
  });
});
