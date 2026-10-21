import assert from 'node:assert/strict';
import { test } from 'node:test';
import * as THREE from 'three';
import { CollisionSystem } from '../src/systems/CollisionSystem.js';
import { Bogota } from '../src/world/districts/Bogota.js';

test('rozar una pared larga no transporta al otro extremo por redondeo', () => {
  const collisions = new CollisionSystem();
  const wall = new THREE.Object3D();
  wall.position.set(47, 0, 0);
  collisions.add(wall, new THREE.Vector3(96, 8, 2));
  const position = new THREE.Vector3(47, 0, -1.4);
  const delta = new THREE.Vector3(0.02, 0, -0.03);
  const result = collisions.resolveMove(position, delta);
  assert.ok(result.distanceTo(position) <= delta.length() + 1e-8);
  assert.ok(result.distanceTo(position.clone().add(delta)) < 1e-8);
});

test('las colisiones detienen pasos largos, permiten deslizar y no mueven al jugador quieto', () => {
  const c = new CollisionSystem();
  const wall = new THREE.Object3D();
  wall.position.set(5, 0, 5);
  c.add(wall, new THREE.Vector3(2, 8, 10));
  assert.deepEqual(c.resolveMove(new THREE.Vector3(0, 0, 5), new THREE.Vector3(20, 0, 1)).toArray(), [3.6, 0, 6]);
  assert.deepEqual(c.resolveMove(new THREE.Vector3(10, 0, 5), new THREE.Vector3(-20, 0, 0)).toArray(), [6.4, 0, 5]);
  const overlap = new THREE.Vector3(4, 0, 5);
  assert.deepEqual(c.resolveMove(overlap, new THREE.Vector3()), overlap);
  assert.ok(c.resolveMove(overlap, new THREE.Vector3(-0.1, 0, 0)).distanceTo(overlap) <= 0.100001);
});

test('el mapa real mantiene cada desplazamiento acotado junto a todas sus paredes', () => {
  const city = new Bogota(), c = new CollisionSystem();
  city._buildCollisions(city._layout(), c);
  const delta = new THREE.Vector3();
  for (const { box } of c.colliders) {
    for (const z of [box.min.z - 0.4, box.max.z + 0.4]) {
      for (const x of [box.min.x - 0.4, (box.min.x + box.max.x) / 2, box.max.x + 0.4]) {
        const start = new THREE.Vector3(x, 0, z);
        for (const dx of [-0.3, 0, 0.3]) for (const dz of [-0.3, 0, 0.3]) {
          delta.set(dx, 0, dz);
          const result = c.resolveMove(start, delta);
          assert.ok(result.distanceTo(start) <= delta.length() + 1e-7, `salto en ${start.toArray()}`);
          const inPlace = start.clone();
          c.resolveMove(inPlace, delta, 0.4, inPlace);
          assert.deepEqual(inPlace, result);
        }
      }
    }
  }
});
