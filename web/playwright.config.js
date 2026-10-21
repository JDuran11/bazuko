import { defineConfig, devices } from '@playwright/test';

/**
 * Pruebas de navegador del vertical slice de Habitante de Calle.
 * El servidor lo levanta Playwright y lo cierra al terminar la suite.
 */
export default defineConfig({
  testDir: './tests/e2e',
  timeout: 180_000,
  expect: { timeout: 15_000 },
  // El juego crea un mundo Three.js y varios modelos por proceso; un worker
  // evita que las pruebas compitan por WebGL y memoria en máquinas de desarrollo.
  fullyParallel: false,
  forbidOnly: !!process.env.CI,
  retries: process.env.CI ? 2 : 0,
  workers: 1,
  reporter: process.env.CI ? 'github' : 'html',
  use: {
    baseURL: 'http://127.0.0.1:5173',
    trace: 'on-first-retry',
    screenshot: 'only-on-failure',
    video: 'retain-on-failure',
    viewport: { width: 1280, height: 800 },
    // Three.js necesita WebGL en Chromium headless. SwiftShader lo hace
    // reproducible en CI y en máquinas sin GPU dedicada.
    launchOptions: {
      args: ['--use-angle=swiftshader', '--enable-unsafe-swiftshader']
    }
  },
  projects: [
    {
      name: 'chromium',
      use: { ...devices['Desktop Chrome'] }
    }
  ],
  webServer: {
    command: 'npm run dev -- --host 127.0.0.1',
    url: 'http://127.0.0.1:5173',
    reuseExistingServer: !process.env.CI,
    timeout: 120_000
  }
});
