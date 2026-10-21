// src/world/tiles/RoadGenerator.js

/**
 * Generador de distrito urbano con sentido urbanístico.
 *
 * Patrón repetido por eje (período = 8 tiles), empezando en el tile 1
 * (el tile 0 y el último son el muro perimetral):
 *
 *   p=0  andén
 *   p=1..4  edificio (4 tiles)
 *   p=5  andén
 *   p=6..7  calle (2 tiles)
 *
 * Con size = 32 (o 40, 48... = 8k + 8) queda exacto: 4x4 manzanas, 3 calles
 * por eje y andén en los bordes. Cada edificio está SIEMPRE rodeado de
 * andén, y los andenes SIEMPRE tocan una calle -> todo es transitable
 * y los edificios nunca invaden la calle.
 *
 * Convención de tiles:
 *   'road'  = calle horizontal (corre a lo largo de X) e intersecciones
 *   'roadV' = calle vertical (corre a lo largo de Y)
 *   map[y][x]  (y = fila = eje Z del mundo)
 */

const PERIOD = 8;
const BUILDING_TYPES = ['building', 'buildingBrick', 'buildingLit'];
const WALKABLE = new Set(['sidewalk', 'dirt', 'floor', 'road', 'roadV']);

// Manzanas (bx, by) que se convierten en plazas libres
const PLAZAS = {
  player: { bx: 0, by: 0 }, // spawn (izq-arriba)
  stall:  { bx: 3, by: 0 }, // puesto de venta (der-arriba)
  bed:    { bx: 0, by: 3 }, // cama / refugio (izq-abajo)
  dealer: { bx: 3, by: 3 }  // rincón del dealer (der-abajo)
};

/** Clasifica un índice 1D según el patrón: 'wall' | 'walk' | 'bldg' | 'road' */
function axisKind(i, size) {
  if (i <= 0 || i >= size - 1) return 'wall';
  const p = (i - 1) % PERIOD;
  if (p === 0 || p === 5) return 'walk';
  if (p >= 1 && p <= 4) return 'bldg';
  return 'road';
}

/** Esquina superior-izquierda (tiles) del núcleo 4x4 de la manzana (bx, by) */
function blockOrigin(bx, by) {
  return { x: 2 + bx * PERIOD, y: 2 + by * PERIOD };
}

/** Tile transitable central de una plaza (dentro de la manzana) */
function plazaCenter(bx, by) {
  const o = blockOrigin(bx, by);
  return { x: o.x + 1, z: o.y + 1 };
}

/**
 * Posiciones (en índices de tile) donde colocar jugador, puesto, cama y dealer.
 * Si tu mundo usa el centro del tile, suma +0.5 a x y z.
 */
export function getSpawnPoints() {
  return {
    player: plazaCenter(PLAZAS.player.bx, PLAZAS.player.by),
    stall:  plazaCenter(PLAZAS.stall.bx,  PLAZAS.stall.by),
    bed:    plazaCenter(PLAZAS.bed.bx,    PLAZAS.bed.by),
    dealer: plazaCenter(PLAZAS.dealer.bx, PLAZAS.dealer.by)
  };
}

export function generateDistrict(size = 32, rng = Math.random) {
  if ((size - 8) % PERIOD !== 0 || size < 24) {
    console.warn(`[RoadGenerator] size=${size} no encaja en el patrón (usa 32, 40, 48...).`);
  }

  const map = Array.from({ length: size }, () =>
    Array.from({ length: size }, () => 'sidewalk')
  );

  // ===== 1. Trama base: calles, andenes y manzanas =====
  for (let y = 0; y < size; y++) {
    for (let x = 0; x < size; x++) {
      const kx = axisKind(x, size);
      const ky = axisKind(y, size);

      if (kx === 'wall' || ky === 'wall') {
        map[y][x] = 'building';            // muro perimetral
      } else if (kx === 'road' && ky === 'road') {
        map[y][x] = 'road';                // intersección
      } else if (kx === 'road') {
        map[y][x] = 'roadV';               // calle que corre en Y
      } else if (ky === 'road') {
        map[y][x] = 'road';                // calle que corre en X
      } else if (kx === 'bldg' && ky === 'bldg') {
        map[y][x] = 'building';            // se estiliza en el paso 2
      } else {
        map[y][x] = 'sidewalk';            // andén alrededor de la manzana
      }
    }
  }

  // ===== 2. Estilo por edificio (cada manzana = 2 edificios de 4x2) =====
  const blocks = Math.floor((size - 2) / PERIOD);
  for (let by = 0; by < blocks; by++) {
    for (let bx = 0; bx < blocks; bx++) {
      const o = blockOrigin(bx, by);
      for (let half = 0; half < 2; half++) {
        const r = rng();
        const style = r < 0.55 ? BUILDING_TYPES[0]
                    : r < 0.85 ? BUILDING_TYPES[1]
                    : BUILDING_TYPES[2];
        for (let dy = 0; dy < 2; dy++) {
          for (let dx = 0; dx < 4; dx++) {
            map[o.y + half * 2 + dy][o.x + dx] = style;
          }
        }
      }
    }
  }

  // ===== 3. Plazas: spawn, puesto de venta, cama, dealer =====
  for (const [key, { bx, by }] of Object.entries(PLAZAS)) {
    const o = blockOrigin(bx, by);
    for (let dy = 0; dy < 4; dy++) {
      for (let dx = 0; dx < 4; dx++) {
        const x = o.x + dx, y = o.y + dy;
        if (key === 'dealer') {
          map[y][x] = (dx + dy) % 2 === 0 ? 'dirt' : 'floor';
        } else {
          map[y][x] = (x + y) % 5 === 0 ? 'dirt' : 'sidewalk';
        }
      }
    }
  }

  // ===== 4. Validación: todo lo transitable debe ser alcanzable =====
  validateConnectivity(map, size);

  return map;
}

/** Flood fill desde el spawn; verifica que puesto, cama y dealer sean alcanzables. */
function validateConnectivity(map, size) {
  const sp = getSpawnPoints();
  const seen = Array.from({ length: size }, () => Array(size).fill(false));
  const stack = [[sp.player.x, sp.player.z]];
  seen[sp.player.z][sp.player.x] = true;

  while (stack.length) {
    const [x, y] = stack.pop();
    for (const [dx, dy] of [[1, 0], [-1, 0], [0, 1], [0, -1]]) {
      const nx = x + dx, ny = y + dy;
      if (nx < 0 || ny < 0 || nx >= size || ny >= size) continue;
      if (seen[ny][nx] || !WALKABLE.has(map[ny][nx])) continue;
      seen[ny][nx] = true;
      stack.push([nx, ny]);
    }
  }

  const ok = (p) => seen[p.z][p.x];
  if (!ok(sp.stall) || !ok(sp.bed) || !ok(sp.dealer)) {
    console.error('[RoadGenerator] ¡Puesto, cama o dealer NO alcanzables desde el spawn!');
  }

  let unreachable = 0;
  for (let y = 0; y < size; y++)
    for (let x = 0; x < size; x++)
      if (WALKABLE.has(map[y][x]) && !seen[y][x]) unreachable++;
  if (unreachable > 0) {
    console.error(`[RoadGenerator] ${unreachable} tiles transitables aislados.`);
  }
}

/**
 * Props: solo en tiles transitables (andenes/tierra), nunca en calles,
 * edificios ni sobre los puntos de spawn.
 */
export function getPropSpawnPoints(map, size) {
  const points = [];
  const sp = getSpawnPoints();
  const keep = Object.values(sp);
  const isSpecial = (x, z) =>
    keep.some(p => Math.abs(p.x - x) <= 1 && Math.abs(p.z - z) <= 1);
  const isFree = (x, z) => {
    const t = map[z]?.[x];
    return t === 'sidewalk' || t === 'dirt' || t === 'floor';
  };

  // Faroles en las esquinas de andén de cada manzana
  const blocks = Math.floor((size - 2) / PERIOD);
  for (let by = 0; by < blocks; by++) {
    for (let bx = 0; bx < blocks; bx++) {
      const o = blockOrigin(bx, by);
      const corners = [
        [o.x - 1, o.y - 1], [o.x + 4, o.y - 1],
        [o.x - 1, o.y + 4], [o.x + 4, o.y + 4]
      ];
      for (const [x, z] of corners) {
        if (isFree(x, z) && !isSpecial(x, z)) points.push({ x, z, type: 'farol' });
      }
    }
  }

  // Props aleatorios (basura, cajas, etc.) solo en andén/tierra
  for (let z = 1; z < size - 1; z++) {
    for (let x = 1; x < size - 1; x++) {
      if (!isFree(x, z) || isSpecial(x, z)) continue;
      if (Math.random() < 0.02) points.push({ x, z, type: null });
    }
  }

  return points;
}
