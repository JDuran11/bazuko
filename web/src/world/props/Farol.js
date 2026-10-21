// src/world/props/Farol.js
import * as THREE from 'three';
import { COLORS } from '../../utils/constants.js';

export class Farol {
  constructor(position, opts = {}) {
    this.group = new THREE.Group();
    this.group.position.copy(position);

    const metalMat = new THREE.MeshLambertMaterial({
      color: 0x2a2a30,
      flatShading: true
    });

    // Poste
    const poleGeo = new THREE.CylinderGeometry(0.06, 0.08, 4, 6);
    const pole = new THREE.Mesh(poleGeo, metalMat);
    pole.position.y = 2;
    pole.castShadow = true;
    this.group.add(pole);

    // Brazo
    const armGeo = new THREE.BoxGeometry(0.6, 0.08, 0.08);
    const arm = new THREE.Mesh(armGeo, metalMat);
    arm.position.set(0.3, 4, 0);
    this.group.add(arm);

    // Lámpara
    const lampGeo = new THREE.BoxGeometry(0.2, 0.15, 0.25);
    const lampMat = new THREE.MeshBasicMaterial({ color: COLORS.ACCENT_AMBER });
    const lamp = new THREE.Mesh(lampGeo, lampMat);
    lamp.position.set(0.6, 3.9, 0);
    this.group.add(lamp);

    // Luz (solo si no es de día — la controla el World)
    if (opts.addLight !== false) {
      this.light = new THREE.PointLight(COLORS.ACCENT_AMBER, 0, 8);
      this.light.position.set(0.6, 3.8, 0);
      this.group.add(this.light);
    }
  }

  /** Enciende/apaga la luz según hora */
  setNight(isNight) {
    if (this.light) {
      this.light.intensity = isNight ? 1.2 : 0;
    }
  }
}