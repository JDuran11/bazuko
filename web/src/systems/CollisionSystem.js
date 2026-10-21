import * as THREE from 'three';

export class CollisionSystem {
  constructor() {
    this.colliders = []; // { box: THREE.Box3, mesh }
  }

  /** Registra un mesh como sólido. size opcional para hitbox custom. */
  add(mesh, size) {
    const box = size
      ? new THREE.Box3().setFromCenterAndSize(
          mesh.position.clone().setY(mesh.position.y + size.y / 2),
          size
        )
      : new THREE.Box3().setFromObject(mesh);
    this.colliders.push({ box, mesh });
  }

  clear() { this.colliders = []; }

  /**
   * Resuelve el movimiento del player (radio `radius`).
   * Devuelve nueva posición válida.
   */
  resolveMove(currentPos, delta, radius = 0.4, result = new THREE.Vector3()) {
    const pos = result.copy(currentPos);

    this._moveAxis(pos, radius, 'x', 'z', delta.x);
    this._moveAxis(pos, radius, 'z', 'x', delta.z);

    return pos;
  }

  _moveAxis(pos, radius, axis, other, delta) {
    if (delta === 0) return;
    const epsilon = 1e-7;
    const start = pos[axis];
    let end = start + delta;
    for (const { box } of this.colliders) {
      // El contacto tangencial no es penetración (incluye error de coma flotante).
      if (pos[other] <= box.min[other] - radius + epsilon ||
          pos[other] >= box.max[other] + radius - epsilon) continue;
      const min = box.min[axis] - radius;
      const max = box.max[axis] + radius;
      if (delta > 0 && start <= min + epsilon && end > min) {
        end = Math.max(start, min);
      } else if (delta < 0 && start >= max - epsilon && end < max) {
        end = Math.min(start, max);
      } else if (start > min + epsilon && start < max - epsilon) {
        // Una posición ya solapada puede salir por el lado más cercano,
        // pero nunca se corrige saltando al otro extremo de una manzana.
        if ((delta > 0 && start < (min + max) / 2) ||
            (delta < 0 && start > (min + max) / 2)) end = start;
      }
    }
    pos[axis] = end;
  }
}
