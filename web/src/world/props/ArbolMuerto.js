// src/world/props/ArbolMuerto.js
import * as THREE from 'three';

export class ArbolMuerto {
  constructor(position) {
    this.group = new THREE.Group();
    this.group.position.copy(position);

    const woodMat = new THREE.MeshLambertMaterial({
      color: 0x2a2018,
      flatShading: true
    });

    // Tronco
    const trunkGeo = new THREE.CylinderGeometry(0.12, 0.18, 2.5, 5);
    const trunk = new THREE.Mesh(trunkGeo, woodMat);
    trunk.position.y = 1.25;
    trunk.castShadow = true;
    this.group.add(trunk);

    // Ramas (aleatorias)
    const numBranches = 3 + Math.floor(Math.random() * 3);
    for (let i = 0; i < numBranches; i++) {
      const branchGeo = new THREE.CylinderGeometry(0.03, 0.06, 0.8, 5);
      const branch = new THREE.Mesh(branchGeo, woodMat);
      const angle = (Math.PI * 2 / numBranches) * i + Math.random() * 0.5;
      const height = 1.5 + Math.random() * 0.8;
      branch.position.set(
        Math.cos(angle) * 0.25,
        height,
        Math.sin(angle) * 0.25
      );
      branch.rotation.z = Math.PI / 3;
      branch.rotation.y = angle;
      branch.castShadow = true;
      this.group.add(branch);
    }
  }
}