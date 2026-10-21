// src/entities/Bed.js
import * as THREE from 'three';
import { COLORS } from '../utils/constants.js';

export class Bed {
  constructor(scene, position) {
    this.scene = scene;
    this.cooldown = 0;

    this.group = new THREE.Group();
    this.group.position.copy(position);
    scene.add(this.group);

    // ===== Colchón =====
    const mattressGeo = new THREE.BoxGeometry(1.2, 0.25, 2);
    const mattressMat = new THREE.MeshLambertMaterial({
      color: 0x555560,
      flatShading: true
    });
    const mattress = new THREE.Mesh(mattressGeo, mattressMat);
    mattress.position.y = 0.15;
    mattress.castShadow = true;
    mattress.receiveShadow = true;
    this.group.add(mattress);

    // ===== Almohada =====
    const pillowGeo = new THREE.BoxGeometry(0.7, 0.15, 0.4);
    const pillowMat = new THREE.MeshLambertMaterial({
      color: 0x777780,
      flatShading: true
    });
    const pillow = new THREE.Mesh(pillowGeo, pillowMat);
    pillow.position.set(0, 0.32, -0.7);
    pillow.castShadow = true;
    this.group.add(pillow);

    // ===== Cartón debajo (ambiente callejero) =====
    const cardGeo = new THREE.BoxGeometry(1.3, 0.05, 2.1);
    const cardMat = new THREE.MeshLambertMaterial({
      color: 0x8a6a3a,
      flatShading: true
    });
    const card = new THREE.Mesh(cardGeo, cardMat);
    card.position.y = 0.02;
    card.receiveShadow = true;
    this.group.add(card);

    // ===== Manta raída =====
    const blanketGeo = new THREE.BoxGeometry(1.15, 0.08, 1.2);
    const blanketMat = new THREE.MeshLambertMaterial({
      color: 0x4a3a30,
      flatShading: true
    });
    const blanket = new THREE.Mesh(blanketGeo, blanketMat);
    blanket.position.set(0, 0.3, 0.4);
    blanket.castShadow = true;
    this.group.add(blanket);

    // ===== Marcador flotante (anillo cian) — ahora BIEN visible =====
    const markerGeo = new THREE.RingGeometry(1.0, 1.3, 24);
    const markerMat = new THREE.MeshBasicMaterial({
      color: COLORS.ACCENT_CYAN,
      side: THREE.DoubleSide,
      transparent: true,
      opacity: 0.9,
      depthTest: false,     // ← ignora el depth buffer
      depthWrite: false
    });
    this.marker = new THREE.Mesh(markerGeo, markerMat);
    this.marker.rotation.x = -Math.PI / 2;
    this.marker.position.y = 0.6;   // ← lo subimos para que flote sobre el colchón
    this.marker.renderOrder = 999;
    this.group.add(this.marker);

    // ===== Luz tenue =====
    this.light = new THREE.PointLight(0x2ab7ca, 0, 4);
    this.light.position.set(0, 0.5, 0);
    this.group.add(this.light);
  }

  get position() { return this.group.position; }

  update(dt) {
    if (this.marker) {
      this.marker.rotation.z += dt * 0.6;
      // Pulso de opacidad para que sea más llamativo
      this.marker.material.opacity = 0.7 + Math.sin(performance.now() * 0.003) * 0.2;
    }

    if (this.cooldown > 0) {
      this.cooldown -= dt;
    }
  }

  setNight(isNight) {
    if (this.light) {
      this.light.intensity = isNight ? 0.6 : 0;
    }
  }

  dispose() {
    this.scene.remove(this.group);
    this.group.traverse((o) => {
      o.geometry?.dispose?.();
      o.material?.dispose?.();
    });
  }
}