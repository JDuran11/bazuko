// src/world/props/Semaforo.js
import * as THREE from 'three';

export class Semaforo {
  constructor(position, rotation = 0) {
    this.group = new THREE.Group();
    this.group.position.copy(position);
    this.group.rotation.y = rotation;

    const metalMat = new THREE.MeshLambertMaterial({
      color: 0x1a1a20,
      flatShading: true
    });

    // Poste
    const poleGeo = new THREE.CylinderGeometry(0.08, 0.1, 4.5, 6);
    const pole = new THREE.Mesh(poleGeo, metalMat);
    pole.position.y = 2.25;
    pole.castShadow = true;
    this.group.add(pole);

    // Brazo
    const armGeo = new THREE.BoxGeometry(0.7, 0.1, 0.1);
    const arm = new THREE.Mesh(armGeo, metalMat);
    arm.position.set(0.35, 4.4, 0);
    this.group.add(arm);

    // Caja
    const boxGeo = new THREE.BoxGeometry(0.25, 0.7, 0.25);
    const box = new THREE.Mesh(boxGeo, metalMat);
    box.position.set(0.7, 4.2, 0);
    box.castShadow = true;
    this.group.add(box);

    // Luces (rojo, amarillo, verde)
    const lightColors = [0xd02020, 0xc9a227, 0x20a020];
    this.lights = [];
    for (let i = 0; i < 3; i++) {
      const lightGeo = new THREE.CircleGeometry(0.08, 8);
      const lightMat = new THREE.MeshBasicMaterial({ color: lightColors[i] });
      const light = new THREE.Mesh(lightGeo, lightMat);
      light.position.set(0.84, 4.4 - i * 0.2, 0);
      light.rotation.y = Math.PI / 2;
      this.group.add(light);
      this.lights.push(light);
    }

    // Estado
    this._current = 0; // rojo
    this._timer = 0;
    this._updateAppearance();
  }

  update(dt) {
    this._timer += dt;
    if (this._timer > 3) {
      this._timer = 0;
      this._current = (this._current + 1) % 3;
      this._updateAppearance();
    }
  }

  _updateAppearance() {
    for (let i = 0; i < this.lights.length; i++) {
      const active = i === this._current;
      this.lights[i].material.color.setHex(
        [0xd02020, 0xc9a227, 0x20a020][i]
      );
      this.lights[i].material.opacity = active ? 1 : 0.25;
      this.lights[i].material.transparent = true;
    }
  }
}