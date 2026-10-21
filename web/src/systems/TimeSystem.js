export class TimeSystem {
  constructor() { this.acc = 0; }
  update(dt, clock) { this.acc += dt; /* hooks futuros: eventos nocturnos */ }
}