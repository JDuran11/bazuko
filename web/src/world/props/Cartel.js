// src/world/props/Cartel.js
import * as THREE from 'three';
import { ProceduralTextures } from '../../utils/textures.js';

export class Cartel {
  constructor(position, rotation = 0, text = 'ZONA CERO') {
    this.group = new THREE.Group();
    this.group.position.copy(position);
    this.group.rotation.y = rotation;

    const metalMat = new THREE.MeshLambertMaterial({
      color: 0x2a2a30,
      flatShading: true
    });

    // Poste
    const poleGeo = new THREE.CylinderGeometry(0.05, 0.05, 2.5, 6);
    const pole = new THREE.Mesh(poleGeo, metalMat);
    pole.position.y = 1.25;
    pole.castShadow = true;
    this.group.add(pole);

    // Placa con textura dinámica
    const canvas = document.createElement('canvas');
    canvas.width = 128;
    canvas.height = 64;
    const ctx = canvas.getContext('2d');
    ctx.fillStyle = '#c9a227';
    ctx.fillRect(0, 0, 128, 64);
    ctx.fillStyle = '#1a1a1d';
    ctx.font = 'bold 18px monospace';
    ctx.textAlign = 'center';
    ctx.textBaseline = 'middle';
    ctx.fillText(text, 64, 32);

    const tex = new THREE.CanvasTexture(canvas);
    tex.magFilter = THREE.NearestFilter;
    tex.minFilter = THREE.NearestFilter;

    const plaqueGeo = new THREE.PlaneGeometry(0.8, 0.4);
    const plaqueMat = new THREE.MeshBasicMaterial({
      map: tex,
      side: THREE.DoubleSide
    });
    const plaque = new THREE.Mesh(plaqueGeo, plaqueMat);
    plaque.position.set(0, 2.3, 0);
    this.group.add(plaque);
  }
}