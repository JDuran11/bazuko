// src/utils/textures.js
import * as THREE from 'three';

const cache = new Map();

/** Crea un canvas con tamaño dado y devuelve su contexto 2D */
function makeCanvas(size = 64) {
  const c = document.createElement('canvas');
  c.width = size;
  c.height = size;
  return { canvas: c, ctx: c.getContext('2d') };
}

/** Aplica ruido pixelado sobre el canvas */
function addNoise(ctx, size, intensity = 20) {
  const img = ctx.getImageData(0, 0, size, size);
  const d = img.data;
  for (let i = 0; i < d.length; i += 4) {
    const n = (Math.random() - 0.5) * intensity;
    d[i]     = Math.max(0, Math.min(255, d[i] + n));
    d[i + 1] = Math.max(0, Math.min(255, d[i + 1] + n));
    d[i + 2] = Math.max(0, Math.min(255, d[i + 2] + n));
  }
  ctx.putImageData(img, 0, 0);
}

/** Devuelve una THREE.CanvasTexture con NearestFilter (PSX look) */
function finish(canvas, repeat = 1) {
  const tex = new THREE.CanvasTexture(canvas);
  tex.magFilter = THREE.NearestFilter;
  tex.minFilter = THREE.NearestFilter;
  tex.generateMipmaps = false;
  tex.wrapS = tex.wrapT = THREE.RepeatWrapping;
  tex.repeat.set(repeat, repeat);
  return tex;
}

export const ProceduralTextures = {
  /** Asfalto oscuro con grietas */
  asphalt(size = 64) {
    const key = `asphalt-${size}`;
    if (cache.has(key)) return cache.get(key);

    const { canvas, ctx } = makeCanvas(size);
    ctx.fillStyle = '#2a2a2d';
    ctx.fillRect(0, 0, size, size);
    addNoise(ctx, size, 30);

    // Grietas
    ctx.strokeStyle = '#1a1a1d';
    ctx.lineWidth = 1;
    for (let i = 0; i < 6; i++) {
      ctx.beginPath();
      ctx.moveTo(Math.random() * size, Math.random() * size);
      ctx.lineTo(Math.random() * size, Math.random() * size);
      ctx.stroke();
    }

    const tex = finish(canvas);
    cache.set(key, tex);
    return tex;
  },

  /** Concreto gris para edificios */
  concrete(size = 64) {
    const key = `concrete-${size}`;
    if (cache.has(key)) return cache.get(key);

    const { canvas, ctx } = makeCanvas(size);
    ctx.fillStyle = '#4a4a52';
    ctx.fillRect(0, 0, size, size);
    addNoise(ctx, size, 25);

    // Manchas de humedad
    for (let i = 0; i < 3; i++) {
      ctx.fillStyle = `rgba(20,20,30,${0.15 + Math.random() * 0.2})`;
      ctx.beginPath();
      ctx.arc(Math.random() * size, Math.random() * size, 5 + Math.random() * 8, 0, Math.PI * 2);
      ctx.fill();
    }

    const tex = finish(canvas);
    cache.set(key, tex);
    return tex;
  },

  /** Ladrillo para variedad de edificios */
  brick(size = 64) {
    const key = `brick-${size}`;
    if (cache.has(key)) return cache.get(key);

    const { canvas, ctx } = makeCanvas(size);
    ctx.fillStyle = '#3a2a28';
    ctx.fillRect(0, 0, size, size);

    // Filas de ladrillos
    const brickH = 8;
    const brickW = 16;
    for (let y = 0; y < size; y += brickH) {
      const offset = (y / brickH) % 2 === 0 ? 0 : brickW / 2;
      for (let x = -brickW; x < size; x += brickW) {
        ctx.fillStyle = `rgb(${70 + Math.random() * 20}, ${45 + Math.random() * 15}, ${40 + Math.random() * 10})`;
        ctx.fillRect(x + offset + 1, y + 1, brickW - 2, brickH - 2);
      }
    }

    const tex = finish(canvas);
    cache.set(key, tex);
    return tex;
  },

  /** Calle con línea central amarilla */
  road(size = 64) {
    const key = `road-${size}`;
    if (cache.has(key)) return cache.get(key);

    const { canvas, ctx } = makeCanvas(size);
    ctx.fillStyle = '#2a2a2d';
    ctx.fillRect(0, 0, size, size);
    addNoise(ctx, size, 25);

    // Línea central (discontinua)
    ctx.fillStyle = '#a08820';
    for (let y = 4; y < size; y += 12) {
      ctx.fillRect(size / 2 - 1, y, 2, 6);
    }

    const tex = finish(canvas);
    cache.set(key, tex);
    return tex;
  },

  /** Calle vertical (rotada) */
  roadV(size = 64) {
    const key = `roadV-${size}`;
    if (cache.has(key)) return cache.get(key);

    const { canvas, ctx } = makeCanvas(size);
    ctx.fillStyle = '#2a2a2d';
    ctx.fillRect(0, 0, size, size);
    addNoise(ctx, size, 25);

    ctx.fillStyle = '#a08820';
    for (let x = 4; x < size; x += 12) {
      ctx.fillRect(x, size / 2 - 1, 6, 2);
    }

    const tex = finish(canvas);
    cache.set(key, tex);
    return tex;
  },

  /** Acera (concreto con junta) */
  sidewalk(size = 64) {
    const key = `sidewalk-${size}`;
    if (cache.has(key)) return cache.get(key);

    const { canvas, ctx } = makeCanvas(size);
    ctx.fillStyle = '#5a5a60';
    ctx.fillRect(0, 0, size, size);
    addNoise(ctx, size, 20);

    // Junta cada 32px
    ctx.strokeStyle = '#3a3a40';
    ctx.lineWidth = 1;
    ctx.beginPath();
    ctx.moveTo(size / 2, 0); ctx.lineTo(size / 2, size);
    ctx.moveTo(0, size / 2); ctx.lineTo(size, size / 2);
    ctx.stroke();

    const tex = finish(canvas);
    cache.set(key, tex);
    return tex;
  },

  /** Ventana iluminada para edificios */
  windowLit(size = 32) {
    const key = `windowLit-${size}`;
    if (cache.has(key)) return cache.get(key);

    const { canvas, ctx } = makeCanvas(size);
    ctx.fillStyle = '#2a2a2d';
    ctx.fillRect(0, 0, size, size);

    // Grid de ventanas
    const w = 6, h = 8, gap = 4;
    for (let y = gap; y < size - h; y += h + gap) {
      for (let x = gap; x < size - w; x += w + gap) {
        const lit = Math.random() < 0.4;
        ctx.fillStyle = lit ? '#c9a227' : '#1a1a20';
        ctx.fillRect(x, y, w, h);
      }
    }

    const tex = finish(canvas);
    cache.set(key, tex);
    return tex;
  },

  /** Césped muerto / tierra */
  dirt(size = 64) {
    const key = `dirt-${size}`;
    if (cache.has(key)) return cache.get(key);

    const { canvas, ctx } = makeCanvas(size);
    ctx.fillStyle = '#3a3028';
    ctx.fillRect(0, 0, size, size);
    addNoise(ctx, size, 35);

    const tex = finish(canvas);
    cache.set(key, tex);
    return tex;
  },

  /** Vacío: textura blanca para debug */
  white(size = 16) {
    const key = `white-${size}`;
    if (cache.has(key)) return cache.get(key);
    const { canvas, ctx } = makeCanvas(size);
    ctx.fillStyle = '#fff';
    ctx.fillRect(0, 0, size, size);
    const tex = finish(canvas);
    cache.set(key, tex);
    return tex;
  }
};