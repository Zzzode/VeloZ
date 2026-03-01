import { test, expect } from '@playwright/test';

test.describe('Backtest', () => {
  test.beforeEach(async ({ page }) => {
    // Set up authenticated state
    await page.goto('/login');
    await page.getByLabel(/user|email|username/i).fill('test');
    await page.getByLabel(/password/i).fill('password');
    await page.getByRole('button', { name: /login|sign in/i }).click();
    await expect(page).toHaveURL(/\/(dashboard)?$/);
  });

  test('can navigate to backtest page', async ({ page }) => {
    // Navigate to backtest
    await page.getByRole('link', { name: /backtest/i }).click();

    // Should be on backtest page
    await expect(page).toHaveURL(/\/backtest/);
  });

  test('displays backtest configuration form', async ({ page }) => {
    await page.goto('/backtest');

    // Form elements should be visible
    await expect(page.getByLabel(/symbol/i)).toBeVisible();
    await expect(page.getByLabel(/strategy/i)).toBeVisible();
    await expect(page.getByLabel(/start date/i)).toBeVisible();
    await expect(page.getByLabel(/end date/i)).toBeVisible();
    await expect(page.getByLabel(/initial capital/i)).toBeVisible();
  });

  test('can configure and run a backtest', async ({ page }) => {
    await page.goto('/backtest');

    // Fill in backtest configuration
    await page.getByLabel(/symbol/i).fill('BTCUSDT');
    await page.getByLabel(/strategy/i).selectOption({ index: 1 });
    await page.getByLabel(/start date/i).fill('2024-01-01');
    await page.getByLabel(/end date/i).fill('2024-06-30');
    await page.getByLabel(/initial capital/i).fill('10000');

    // Optional: fill commission and slippage
    const commissionInput = page.getByLabel(/commission/i);
    if (await commissionInput.isVisible()) {
      await commissionInput.fill('0.001');
    }

    const slippageInput = page.getByLabel(/slippage/i);
    if (await slippageInput.isVisible()) {
      await slippageInput.fill('0.0005');
    }

    // Run backtest
    await page.getByRole('button', { name: /run backtest|start/i }).click();

    // Should show loading or results
    await expect(
      page.getByText(/running|queued|results|completed/i).first()
    ).toBeVisible({ timeout: 10000 });
  });

  test('validates backtest form', async ({ page }) => {
    await page.goto('/backtest');

    // Try to submit empty form
    await page.getByRole('button', { name: /run backtest|start/i }).click();

    // Should show validation errors
    await expect(page.getByText(/required|invalid/i).first()).toBeVisible();
  });

  test('displays backtest results', async ({ page }) => {
    await page.goto('/backtest');

    // If there are existing backtests, click on one
    const backtestItem = page.getByRole('row').or(page.getByRole('listitem')).first();
    if (await backtestItem.isVisible()) {
      await backtestItem.click();

      // Results should show key metrics
      await expect(
        page.getByText(/total return|sharpe ratio|max drawdown/i).first()
      ).toBeVisible();
    }
  });

  test('displays equity curve chart', async ({ page }) => {
    await page.goto('/backtest');

    // Click on an existing backtest if available
    const backtestItem = page.getByRole('row').or(page.getByRole('listitem')).first();
    if (await backtestItem.isVisible()) {
      await backtestItem.click();

      // Equity curve chart should be visible
      await expect(
        page.getByText(/equity curve/i).or(page.locator('svg').first())
      ).toBeVisible();
    }
  });

  test('displays trade list', async ({ page }) => {
    await page.goto('/backtest');

    // Click on an existing backtest if available
    const backtestItem = page.getByRole('row').or(page.getByRole('listitem')).first();
    if (await backtestItem.isVisible()) {
      await backtestItem.click();

      // Trade list should be visible
      await expect(
        page.getByText(/trades|trade list|entry|exit/i).first()
      ).toBeVisible();
    }
  });

  test('can compare multiple backtests', async ({ page }) => {
    await page.goto('/backtest');

    // Look for compare functionality
    const compareButton = page.getByRole('button', { name: /compare/i });
    if (await compareButton.isVisible()) {
      // Select multiple backtests for comparison
      const checkboxes = page.getByRole('checkbox');
      const count = await checkboxes.count();

      if (count >= 2) {
        await checkboxes.nth(0).check();
        await checkboxes.nth(1).check();
        await compareButton.click();

        // Comparison view should show
        await expect(
          page.getByText(/comparison|vs|side by side/i).first()
        ).toBeVisible();
      }
    }
  });

  test('can export backtest results', async ({ page }) => {
    await page.goto('/backtest');

    // Click on an existing backtest
    const backtestItem = page.getByRole('row').or(page.getByRole('listitem')).first();
    if (await backtestItem.isVisible()) {
      await backtestItem.click();

      // Look for export button
      const exportButton = page.getByRole('button', { name: /export|download|csv/i });
      if (await exportButton.isVisible()) {
        // Set up download handler
        const downloadPromise = page.waitForEvent('download');
        await exportButton.click();

        // Should trigger download
        const download = await downloadPromise;
        expect(download.suggestedFilename()).toMatch(/\.csv$/);
      }
    }
  });

  test('can delete a backtest', async ({ page }) => {
    await page.goto('/backtest');

    // Look for delete button on a backtest
    const deleteButton = page.getByRole('button', { name: /delete|remove/i }).first();
    if (await deleteButton.isVisible()) {
      await deleteButton.click();

      // Confirm deletion if dialog appears
      const confirmButton = page.getByRole('button', { name: /confirm|yes|delete/i });
      if (await confirmButton.isVisible()) {
        await confirmButton.click();
      }

      // Should show success message
      await expect(
        page.getByText(/deleted|removed|success/i).first()
      ).toBeVisible();
    }
  });
});
