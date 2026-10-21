// src/world/props/Hidrante.js
import * as THREE from 'three';

export class Hidrante {
  constructor(position) {
    this.group = new THREE.Group();
    this.group.position.copy(position);

    const redMat = new THREE.MeshLambertMaterial({
      color: 0x8a1a1a,
      flatShading: true
    });

    // Cuerpo
    const bodyGeo = new THREE.CylinderGeometry(0.15, 0.18, 0.7, 8);
    const body = new THREE.Mesh(bodyGeo, redMat);
    body.position.y = 0.35;
    body.castShadow = true;
    this.group.add(body);

    // Tapa
    const capGeo = new THREE.CylinderGeometry(0.2, 0.15, 0.1, 8);
    const cap = new THREE.Mesh(capGeo, redMat);
    cap.position.y = 0.75;
    this.group.add(cap);

    // Boquillas laterales
    const nozzleGeo = new THREE.CylinderGeometry(0.06, 0.06, 0.15, 6);
    for (const angle of [0, Math.PI]) {
      const nozzle = new THREE.Mesh(nozzleGeo, redMat);
      nozzle.rotation.z = Math.PI / 2;
      nozzle.position.set(Math.cos(angle) * 0.15, 0.4, Math.sin(angle) * 0.15);
      this.group.add(nozzle);
    }
  }
}