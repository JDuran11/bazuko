import assert from 'node:assert/strict';
import { test } from 'node:test';
import * as THREE from 'three';
import { Bogota } from '../src/world/districts/Bogota.js';
import { BusService } from '../src/world/bogota/BusService.js';
import { CARACAS } from '../src/world/bogota/StreetLayout.js';

test('el bus completo recorre la troncal sin invadir estación, separadores, andenes ni muros', () => {
  const city = new Bogota(), map = city._layout(), route = city.getBusRoute();
  const stationX = (CARACAS.busway[0] + CARACAS.busway[1]);
  const bus = new BusService(route);
  assert.ok(route.x - bus.width / 2 > stationX + 1.6);
  assert.ok(route.x + bus.width / 2 < CARACAS.busway[1] * 2 + 1 - 0.15);
  for(let z=route.endZ;z<=route.startZ;z+=0.1) {
    for(let along=-bus.length/2;along<=bus.length/2;along+=0.5) for(const side of [-bus.width/2,0,bus.width/2]) {
      const tile=map[Math.round((z+along)/2)]?.[Math.round((route.x+side)/2)];
      assert.ok(['road','busway'].includes(tile), `${route.x+side},${z+along}: ${tile}`);
    }
  }
  city.group.add(bus.group);
  city._mergeStatic();
  assert.equal(bus.group.parent,city.group);
  assert.equal(bus.group.userData.wheels.length,8);
});

test('el servicio avanza, para cuatro segundos en la estación y repite el recorrido sin giro brusco visible', () => {
  const city=new Bogota(), bus=new BusService(city.getBusRoute());
  const orientation=bus.group.quaternion.clone(), wheel=bus.group.userData.wheels[0].quaternion.clone();
  let dwellSeconds=0, arrived=false, departed=false, relayed=false;
  for(let i=0;i<2400;i++) {
    const before=bus.position.z;
    bus.update(0.05);
    if(bus.dwellRemaining>0) {
      arrived=true;
      dwellSeconds+=0.05;
      assert.equal(bus.position.z,bus.stationZ);
      assert.equal(bus.speed,0);
    }
    if(arrived && bus.position.z<bus.stationZ-1) departed=true;
    assert.ok(bus.group.quaternion.equals(orientation));
    if(bus.position.z>before) {
      assert.equal(bus.group.visible,false);
      assert.equal(bus.materials.keys().next().value.opacity,0);
      relayed=true;
      break;
    }
    assert.ok(before-bus.position.z<=5*0.05+1e-8);
  }
  assert.ok(arrived && departed && relayed);
  assert.ok(dwellSeconds>=3.95 && dwellSeconds<=4.1);
  assert.ok(!bus.group.userData.wheels[0].quaternion.equals(wheel));
});

test('el bus frena ante el jugador y los peatones, y vuelve a avanzar cuando despejan el carril', () => {
  for(const actor of ['player','pedestrian']) {
    const bus=new BusService(new Bogota().getBusRoute());
    for(let i=0;i<60;i++) bus.update(0.05);
    const person=new THREE.Vector3(bus.position.x,0,bus.position.z-bus.length/2-10);
    const context=actor==='player'?{player:person}:{pedestrians:[{position:person}]};
    for(let i=0;i<200;i++) bus.update(0.05,context);
    assert.ok(bus.speed<1e-8);
    assert.ok(bus.position.z-bus.length/2>=person.z+0.79);
    const stopped=bus.position.z;
    for(let i=0;i<40;i++) bus.update(0.05);
    assert.ok(bus.position.z<stopped-1);
  }
});
