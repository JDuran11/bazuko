// src/data/origins.js
// ¿Cómo llegó el personaje a la calle? Cada origen da una ventaja pequeña.

export const ORIGINS = {
  reciclador: {
    name: 'Reciclador',
    desc: 'Llevas años en el reciclaje y sabes qué sirve: encuentras un objeto más en cada bolsa.',
    effects: { extraLoot: 1 }
  },
  desplazado: {
    name: 'Desplazado',
    desc: 'Llegaste del campo huyendo de la violencia. Aguantas: te enfermas 30 % menos.',
    effects: { diseaseResist: 0.7 }
  },
  migrante: {
    name: 'Migrante',
    desc: 'Cruzaste la frontera buscando trabajo. Tienes labia: cuando pides te dan 30 % más.',
    effects: { begMult: 1.3 }
  },
  endeudado: {
    name: 'Gota a gota',
    desc: 'Lo perdiste todo por un préstamo gota a gota. Sabes negociar: vendes 15 % más caro.',
    effects: { sellMult: 1.15 }
  }
};

/** Efecto del origen elegido, o `base` si no aplica */
export function originEffect(originId, key, base = 1) {
  return ORIGINS[originId]?.effects[key] ?? base;
}
