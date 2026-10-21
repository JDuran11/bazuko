import { CALLES } from '../src/world/bogota/StreetLayout.js';
import assert from 'node:assert/strict';
import { mock, test } from 'node:test';
import * as THREE from 'three';
import * as textures from '../src/world/bogota/textures.js';
import { TrafficRoute } from '../src/world/bogota/TrafficRoute.js';

// Usa los carros reales, sustituyendo solo las texturas de Canvas por texturas vacías.
mock.module('../src/world/bogota/textures.js', { namedExports: { ...textures, radialSprite: () => new THREE.Texture() } });
const { TrafficSystem } = await import('../src/systems/TrafficSystem.js');
const { Car } = await import('../src/entities/Car.js');
const { Bogota } = await import('../src/world/districts/Bogota.js');

test('descartar peatones lejanos mantiene el mismo frenado en rectas y giros', () => {
  const route = new TrafficRoute(0, 30, 0, 30);
  const traffic = new TrafficSystem(new THREE.Scene(), [route], { carsPerRoute: 1 });
  const vehicle = traffic.vehicles[0], direction = new THREE.Vector3();
  const pedestrians = [];
  for (let x = -4; x <= 34; x += 2) for (let z = -4; z <= 34; z += 2) {
    pedestrians.push({ position: new THREE.Vector3(x, 0, z) });
  }
  for (let distance = 0; distance < route.length; distance += .75) {
    vehicle.distance = distance;
    vehicle.speed = 5;
    route.sample(distance, vehicle.car.position, direction);
    const actual = traffic._clearance(vehicle, null, pedestrians);
    const lookAhead = Math.max(5, vehicle.speed ** 2 / 10 + vehicle.speed * .2 + 2);
    let expected = Infinity;
    for (let ahead = 0; ahead <= lookAhead; ahead += .5) {
      route.sample(distance + ahead, traffic._probePosition, traffic._probeDirection);
      if (pedestrians.some(p => traffic._personInCar(p.position, vehicle.car))) {
        expected = Math.max(0, ahead - .5);
        break;
      }
    }
    assert.equal(actual, expected);
  }
  let probes = 0;
  const check = traffic._personInCar.bind(traffic);
  traffic._personInCar = (...args) => { probes++; return check(...args); };
  traffic._clearance(vehicle, null, [{ position: new THREE.Vector3(500, 0, 500) }]);
  assert.equal(probes, 0);
  traffic.dispose();
});

test('los circuitos son continuos y toda la carrocería permanece en la calzada', () => {
  const city = new Bogota(), map = city._layout();
  const position = new THREE.Vector3(), direction = new THREE.Vector3();
  for (const route of city.getTrafficRoutes()) {
    for (let distance = 0; distance < route.length; distance += 0.1) {
      route.sample(distance, position, direction);
      assert.ok(Math.abs(direction.length() - 1) < 1e-8);
      for (const forward of [-2.15, -1, 0, 1, 2.15]) for (const sideways of [-0.88, 0, 0.88]) {
        const x = position.x + direction.x * forward - direction.z * sideways;
        const z = position.z + direction.z * forward + direction.x * sideways;
        assert.equal(map[Math.round(z / 2)]?.[Math.round(x / 2)], 'road', `carro fuera del carril: ${x},${z}`);
      }
    }
    const first = new THREE.Vector3(), end = new THREE.Vector3();
    route.sample(0, first, direction);
    route.sample(route.length, end, direction);
    assert.ok(first.distanceTo(end) < 1e-8);
    route.sample(route.length - 0.001, end, direction);
    assert.ok(first.distanceTo(end) < 0.002);
  }
});

test('el tráfico recorre vueltas completas, gira las ruedas y mantiene la distancia', () => {
  const traffic = new TrafficSystem(new THREE.Scene(), [new TrafficRoute(14, 44, 26, 60)], { random: () => 0.7 });
  traffic.vehicles[1].cruiseSpeed = 1.4;
  const starts = traffic.vehicles.map(vehicle => vehicle.distance);
  let passedSeam = false;
  for (let frame = 0; frame < 1500; frame++) {
    const previous = traffic.vehicles.map(vehicle => vehicle.distance);
    traffic.update(0.1);
    for (let i = 0; i < traffic.vehicles.length; i++) {
      const vehicle = traffic.vehicles[i];
      if (vehicle.distance < previous[i]) passedSeam = true;
      for (const other of traffic.vehicles) {
        if (other === vehicle) continue;
        const gap = (other.distance - vehicle.distance + vehicle.route.length) % vehicle.route.length;
        assert.ok(gap >= (vehicle.car.length + other.car.length) / 2 + 1.8 - 1e-6);
      }
      assert.ok(vehicle.speed >= 0 && vehicle.speed <= vehicle.cruiseSpeed + 1e-6);
    }
  }
  assert.ok(passedSeam);
  for (let i = 0; i < traffic.vehicles.length; i++) {
    assert.notEqual(traffic.vehicles[i].distance, starts[i]);
    assert.ok(traffic.vehicles[i].car.vehicle.wheels[0].rotation.z < 0);
    assert.equal(traffic.vehicles[i].car.marker.visible, false);
    assert.equal(traffic.vehicles[i].car.steal(), null);
  }
  traffic.dispose();
});

test('los carros frenan ante el jugador o un peatón y reanudan la marcha', () => {
  for (const type of ['player', 'pedestrian']) {
    const traffic = new TrafficSystem(new THREE.Scene(), [new TrafficRoute(0, 30, 0, 34)], { carsPerRoute: 1, random: () => 0.5 });
    const vehicle = traffic.vehicles[0];
    vehicle.distance = 2;
    vehicle.speed = vehicle.cruiseSpeed;
    const actor = new THREE.Vector3(), direction = new THREE.Vector3();
    vehicle.route.sample(vehicle.distance + 9, actor, direction);
    const context = type === 'player' ? { player: actor } : { pedestrians: [{ position: actor }] };
    for (let frame = 0; frame < 200; frame++) traffic.update(0.1, context);
    assert.equal(vehicle.speed, 0);
    assert.ok(vehicle.car.position.distanceTo(actor) >= vehicle.car.length / 2 + 0.55);
    const stopped = vehicle.distance;
    for (let frame = 0; frame < 40; frame++) traffic.update(0.1);
    assert.ok(vehicle.distance > stopped + 3);
    traffic.dispose();
  }
});

test('el parqueo queda fuera de los circuitos y eliminar un carro conserva recursos compartidos', () => {
  const city = new Bogota();
  city.map = city._layout();
  const spots = city.getParkingSpots();
  assert.ok(spots.length >= 5);
  assert.ok(spots.every(spot => spot.z < (CALLES[0][0] - 2) * 2 || spot.z > (CALLES[1][1] + 2) * 2));
  const scene = new THREE.Scene();
  const first = new Car(scene, new THREE.Vector3());
  const second = new Car(scene, new THREE.Vector3(10, 0, 0));
  let sharedDisposed = false, bodyDisposed = false;
  first.vehicle.wheels[0].geometry.addEventListener('dispose', () => { sharedDisposed = true; });
  first.vehicle.group.children[0].geometry.addEventListener('dispose', () => { bodyDisposed = true; });
  first.dispose();
  assert.equal(sharedDisposed, false);
  assert.equal(bodyDisposed, true);
  assert.ok(scene.children.includes(second.group));
  assert.equal(second.marker.visible, true);
  assert.ok(second.steal());
  second.dispose();
});
