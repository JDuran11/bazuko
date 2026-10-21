# Pruebas de navegador

El proyecto usa [Playwright](https://playwright.dev/) para comprobar los flujos básicos del juego en Chromium.

## Preparación

```bash
npm install
npx playwright install chromium
```

## Ejecutar

```bash
npm run test:e2e          # ejecución headless
npm run test:e2e:headed   # muestra Chromium
npm run test:e2e:ui       # inspector interactivo de Playwright
npm run test:report       # abre el último informe HTML
```

La configuración levanta Vite automáticamente en `127.0.0.1:5173`. Los tests limpian `localStorage` antes de cada caso para que una partida guardada no afecte el resultado.
