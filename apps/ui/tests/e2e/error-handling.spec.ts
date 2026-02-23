import { test, expect } from '@playwright/test';

test.describe('Error Handling', () => {
  test.beforeEach(async ({ page }) => {
    // Set up authenticated state
    await page.goto('/login');
    await page.getByLabel(/user|email|username/i).fill('test');
    await page.getByLabel(/password/i).fill('password');
    await page.getByRole('button', { name: /login|sign in/i }).click();
    await expect(page).toHaveURL(/\/(dashboard)?$/);
  });

  test('handles network errors gracefully', async ({ page }) => {
    // Navigate to trading page
    await page.goto('/trading');

    // Simulate network failure by blocking API requests
    await page.route('**/api/**', (route) => {
      route.abort('failed');
    });

    // Try to place an order (which will fail)
    const buyButton = page.getByRole('button', { name: /buy/i }).first();
    if (await buyButton.isVisible()) {
      await buyButton.click();

      // Fill form
      const qtyInput = page.getByLabel(/quantity|amount|qty/i);
      if (await qtyInput.isVisible()) {
        await qtyInput.fill('0.1');
      }

      const priceInput = page.getByLabel(/price/i);
      if (await priceInput.isVisible()) {
        await priceInput.fill('42000');
      }

      // Submit
      await page.getByRole('button', { name: /place order|submit/i }).click();

      // Should show error message (not crash)
      await expect(
        page.getByText(/error|failed|unable|network|connection/i).first()
      ).toBeVisible({ timeout: 5000 });
    }

    // Page should still be functional
    await expect(page.getByText(/trading|order/i).first()).toBeVisible();
  });

  test('handles 404 pages correctly', async ({ page }) => {
    // Navigate to non-existent page
    await page.goto('/non-existent-page-12345');

    // Should show 404 or redirect to a valid page
    await expect(
      page.getByText(/not found|404|page doesn't exist/i).or(
        page.getByText(/dashboard|home/i)
      )
    ).toBeVisible();

    // Should have navigation to go back
    const homeLink = page.getByRole('link', { name: /home|dashboard|back/i });
    if (await homeLink.isVisible()) {
      await homeLink.click();
      await expect(page).toHaveURL(/\/(dashboard)?$/);
    }
  });
});
