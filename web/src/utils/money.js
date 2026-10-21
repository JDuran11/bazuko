// src/utils/money.js
// Formato de pesos colombianos: 12500 → "$ 12.500"

const fmt = new Intl.NumberFormat('es-CO', { maximumFractionDigits: 0 });

export const cop = (n) => `$ ${fmt.format(Math.round(n ?? 0))}`;
