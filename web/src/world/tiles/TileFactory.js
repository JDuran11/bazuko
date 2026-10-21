// src/world/tiles/TileFactory.js
import * as THREE from 'three';
import { TILE_SIZE } from '../../utils/constants.js';
import { ProceduralTextures } from '../../utils/textures.js';

const _geoCache = {};
const _matCache = {};

function geo(key, factory) {
  if (!_geoCache[key]) _geoCache[key] = factory();
  return _geoCache[key];
}

function mat(key, factory) {
  if (!_matCache[key]) _matCache[key] = factory();
  return _matCache[key];
}

function texFor(name) {
  switch (name) {
    case 'asphalt':   return ProceduralTextures.asphalt();
    case 'concrete':  return ProceduralTextures.concrete();
    case 'brick':     return ProceduralTextures.brick();
    case 'road':      return ProceduralTextures.road();
    case 'roadV':     return ProceduralTextures.roadV();
    case 'sidewalk':  return ProceduralTextures.sidewalk();
    case 'windowLit': return ProceduralTextures.windowLit();
    case 'dirt':      return ProceduralTextures.dirt();
    default:          return ProceduralTextures.white();
  }
}

export const TileFactory = {
  create(type) {
    switch (type) {
      case 'floor':
      case 'sidewalk':  return this._sidewalk();
      case 'road':      return this._road('road');
      case 'roadV':     return this._road('roadV');
      case 'dirt':      return this._dirt();
      case 'building':  return this._building();
      case 'buildingBrick': return this._buildingBrick();
      case 'buildingLit':   return this._buildingLit();
      case 'barrel':    return this._barrel();
      case 'trash':     return this._trash();
      default:          return this._sidewalk();
    }
  },

  _sidewalk() {
    const g = geo('sidewalk', () => new THREE.BoxGeometry(TILE_SIZE, 0.2, TILE_SIZE));
    const m = mat('sidewalk', () => new THREE.MeshLambertMaterial({
      map: texFor('sidewalk'),
      color: 0x9a9a9a,
      flatShading: true
    }));
    const mesh = new THREE.Mesh(g, m);
    mesh.receiveShadow = true;
    return mesh;
  },

  _road(textureName) {
    const key = `road-${textureName}`;
    const g = geo(key, () => new THREE.BoxGeometry(TILE_SIZE, 0.1, TILE_SIZE));
    const m = mat(key, () => new THREE.MeshLambertMaterial({
      map: texFor(textureName),
      color: 0x9a9a9a,
      flatShading: true
    }));
    const mesh = new THREE.Mesh(g, m);
    mesh.receiveShadow = true;
    return mesh;
  },

  _dirt() {
    const g = geo('dirt', () => new THREE.BoxGeometry(TILE_SIZE, 0.15, TILE_SIZE));
    const m = mat('dirt', () => new THREE.MeshLambertMaterial({
      map: texFor('dirt'),
      color: 0x9a9a9a,
      flatShading: true
    }));
    const mesh = new THREE.Mesh(g, m);
    mesh.receiveShadow = true;
    return mesh;
  },

  _building() {
    const g = geo('building', () => new THREE.BoxGeometry(TILE_SIZE, 8, TILE_SIZE));
    const m = mat('building', () => new THREE.MeshLambertMaterial({
      map: texFor('concrete'),
      color: 0x9a9a9a,
      flatShading: true
    }));
    const mesh = new THREE.Mesh(g, m);
    mesh.position.y = 4;
    mesh.castShadow = true;
    mesh.receiveShadow = true;
    return mesh;
  },

  _buildingBrick() {
    const g = geo('buildingBrick', () => new THREE.BoxGeometry(TILE_SIZE, 6, TILE_SIZE));
    const m = mat('buildingBrick', () => new THREE.MeshLambertMaterial({
      map: texFor('brick'),
      color: 0xaaaaaa,
      flatShading: true
    }));
    const mesh = new THREE.Mesh(g, m);
    mesh.position.y = 3;
    mesh.castShadow = true;
    mesh.receiveShadow = true;
    return mesh;
  },

  _buildingLit() {
    const g = geo('buildingLit', () => new THREE.BoxGeometry(TILE_SIZE, 10, TILE_SIZE));
    const m = mat('buildingLit', () => new THREE.MeshLambertMaterial({
      map: texFor('windowLit'),
      color: 0xaaaaaa,
      emissive: 0x2a2a20,
      flatShading: true
    }));
    const mesh = new THREE.Mesh(g, m);
    mesh.position.y = 5;
    mesh.castShadow = true;
    mesh.receiveShadow = true;
    return mesh;
  },

  _barrel() {
    const g = geo('barrel', () => new THREE.CylinderGeometry(0.5, 0.5, 1.2, 8));
    const m = mat('barrel', () => new THREE.MeshLambertMaterial({
      color: 0x5a3a1a,
      flatShading: true
    }));
    const mesh = new THREE.Mesh(g, m);
    mesh.position.y = 0.6;
    mesh.castShadow = true;
    return mesh;
  },

  _trash() {
    const g = geo('trash', () => new THREE.BoxGeometry(1.2, 0.6, 1.2));
    const m = mat('trash', () => new THREE.MeshLambertMaterial({
      color: 0x4a4a2a,
      flatShading: true
    }));
    const mesh = new THREE.Mesh(g, m);
    mesh.position.y = 0.4;
    mesh.castShadow = true;
    return mesh;
  },

  dispose() {
    for (const g of Object.values(_geoCache)) g.dispose?.();
    for (const m of Object.values(_matCache)) m.dispose?.();
  }
};