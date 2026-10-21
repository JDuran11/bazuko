import * as THREE from 'three';
import { articulatedBus, ARTICULATED_BUS_SIZE } from './props.js';

const ACCELERATION = 1.1, BRAKING = 4, CRUISE_SPEED = 5;
const STOP_SECONDS = 4, END_FADE_METERS = 6;

/** Servicio hacia el norte por el carril exclusivo, con relevo en los extremos. */
export class BusService {
  constructor({ x, startZ, endZ, stationZ, height = 0.02 }) {
    if (!(startZ > stationZ && stationZ > endZ)) throw new Error('La estación debe quedar dentro del recorrido del bus.');
    this.group = articulatedBus();
    this.group.userData.noMerge = true;
    this.group.name = 'Bus troncal Caracas';
    this.position = this.group.position;
    this.length = ARTICULATED_BUS_SIZE.length;
    this.width = ARTICULATED_BUS_SIZE.width;
    this.startZ = startZ;
    this.endZ = endZ;
    this.stationZ = stationZ;
    this.position.set(x, height, startZ);
    this.speed = 0;
    this.servedStation = false;
    this.dwellRemaining = 0;
    this.materials = new Map();
    this.group.traverse(mesh => {
      if (!mesh.isMesh) return;
      const material = mesh.material;
      this.materials.set(material, material.opacity);
    });
    this._updateVisibility();
  }

  _clearance(position) {
    // El frente es −Z. Incluye el radio de la persona y un margen ante el parachoques.
    if (Math.abs(position.x - this.position.x) > this.width / 2 + 0.55) return Infinity;
    if (position.z > this.position.z + this.length / 2 + 0.55) return Infinity;
    return Math.max(0, this.position.z - position.z - this.length / 2 - 0.8);
  }

  _updateVisibility() {
    // El relevo se desvanece en los extremos; nunca cruza los muros del perímetro.
    const opacity = THREE.MathUtils.clamp(Math.min(
      (this.startZ - this.position.z) / END_FADE_METERS,
      (this.position.z - this.endZ) / END_FADE_METERS), 0, 1);
    this.group.visible = opacity > 0;
    for (const [material, baseOpacity] of this.materials) {
      const transparent = opacity < 1;
      if (material.transparent !== transparent) { material.transparent = transparent; material.needsUpdate = true; }
      material.opacity = baseOpacity * opacity;
      material.depthWrite = !transparent;
    }
  }

  update(dt, { player = null, pedestrians = [] } = {}) {
    let remaining = Math.min(Math.max(dt, 0), 0.5);
    while (remaining > 1e-8) {
      const step = Math.min(remaining, 0.05);
      remaining -= step;
      if (this.dwellRemaining > 0) {
        this.dwellRemaining = Math.max(0, this.dwellRemaining - step);
        continue;
      }
      let clearance = this.position.z - this.endZ;
      if (!this.servedStation) clearance = Math.min(clearance, this.position.z - this.stationZ);
      if (player) clearance = Math.min(clearance, this._clearance(player));
      for (const npc of pedestrians) clearance = Math.min(clearance, this._clearance(npc.position));
      const desiredSpeed = Math.min(CRUISE_SPEED, Math.sqrt(2 * BRAKING * Math.max(0, clearance)));
      const rate = desiredSpeed < this.speed ? BRAKING : ACCELERATION;
      this.speed = THREE.MathUtils.clamp(desiredSpeed, this.speed - rate * step, this.speed + rate * step);
      const travel = Math.min(this.speed * step, Math.max(0, clearance));
      this.position.z -= travel;
      this.speed = travel / step;
      for (const wheel of this.group.userData.wheels) wheel.rotateY(travel / 0.5);
      if (!this.servedStation && this.position.z <= this.stationZ + 1e-8) {
        this.position.z = this.stationZ;
        this.servedStation = true;
        this.speed = 0;
        this.dwellRemaining = STOP_SECONDS;
      }
      if (this.position.z <= this.endZ + 1e-8) {
        this.position.z = this.startZ;
        this.servedStation = false;
        this.speed = 0;
      }
    }
    this._updateVisibility();
  }
}
