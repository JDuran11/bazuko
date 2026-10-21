import * as THREE from 'three';
import { LightHalos } from './LightHalos.js';

/** Un número fijo de luces evita shaders con cientos de luces y recompilaciones al caminar. */
export class LocalLights {
  constructor(scene, count = 2) {
    this.scene = scene;
    this.sources = new Map();
    this.candidates = [];
    this.elapsed = 1;
    this.halos = new LightHalos(scene);
    this.pool = Array.from({ length: count }, () => {
      const light = new THREE.PointLight(0xffffff, 0, 1);
      light.userData.localLightProxy = true;
      scene.add(light);
      return light;
    });
  }

  update(dt, target, groundHeight, camera = null) {
    this.elapsed += dt;
    if (this.elapsed >= 1) {
      this.elapsed = 0;
      // También descubre fogones construidos durante la partida.
      const found = new Set();
      this.scene.traverse(light => {
        if (!light.isPointLight || light.userData.localLightProxy) return;
        found.add(light);
        if (!this.sources.has(light)) {
          this.sources.set(light, { light, mask: light.layers.mask, position: new THREE.Vector3(), score: 0 });
          light.layers.set(2); // fuera de las pasadas normal (0) y silueta (1)
        }
      });
      for (const [light, entry] of this.sources) if (!found.has(light)) {
        light.layers.mask = entry.mask;
        this.sources.delete(light);
      }
    }
    this.candidates.length = 0;
    for (const entry of this.sources.values()) {
      const light = entry.light;
      if (light.intensity <= 0) continue;
      let visible = true;
      for (let node = light; node; node = node.parent) if (!node.visible) { visible = false; break; }
      if (!visible) continue;
      light.getWorldPosition(entry.position);
      const distance = entry.position.distanceToSquared(target);
      if (distance > 80 * 80) continue;
      // La llama parpadea, pero no debe intercambiar las luces elegidas en cada oscilación.
      entry.peakIntensity = Math.max(entry.peakIntensity ?? 0, light.intensity);
      entry.score = entry.peakIntensity / (1 + distance);
      this.candidates.push(entry);
    }
    this.candidates.sort((a, b) => b.score - a.score);
    this.halos.update(this.candidates, target, groundHeight, camera);
    for (let i = 0; i < this.pool.length; i++) {
      const proxy = this.pool[i], source = this.candidates[i];
      // No ocultar las plazas vacías: mantener el número de luces del shader estable.
      proxy.intensity = source?.light.intensity ?? 0;
      if (!source) continue;
      proxy.position.copy(source.position);
      proxy.color.copy(source.light.color);
      proxy.distance = source.light.distance;
      proxy.decay = source.light.decay;
    }
  }

  dispose() {
    this.halos.dispose();
    for (const { light, mask } of this.sources.values()) light.layers.mask = mask;
    for (const light of this.pool) this.scene.remove(light);
    this.sources.clear();
  }
}
