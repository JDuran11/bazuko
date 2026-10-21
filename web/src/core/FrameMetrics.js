/** Mide intervalos reales de RAF, nunca el dt limitado de la simulación. */
export class FrameMetrics {
  constructor() { this.reset(); }

  reset() {
    this.previous = null;
    this.frames = 0;
    this.elapsed = 0;
    this.current = this.min = this.max = this.average = 0;
    this.frameMs = 0;
  }

  sample(now, active = true) {
    if (!active) { this.previous = null; return false; }
    const previous = this.previous;
    this.previous = now;
    if (previous === null || now <= previous) return false;
    this.frameMs = now - previous;
    this.current = 1000 / this.frameMs;
    this.frames++;
    this.elapsed += this.frameMs;
    this.min = this.frames === 1 ? this.current : Math.min(this.min, this.current);
    this.max = Math.max(this.max, this.current);
    this.average = this.frames * 1000 / this.elapsed;
    return true;
  }
}
