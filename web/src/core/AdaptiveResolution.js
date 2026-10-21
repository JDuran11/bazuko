/** Ventanas largas e histéresis para no cambiar resolución por un tirón aislado. */
export class AdaptiveResolution {
  constructor(deviceRatio = 1) {
    this.max = Math.min(deviceRatio || 1, 1.5);
    this.min = Math.min(this.max, 0.75);
    this.ratio = this.max;
    this.resetWindow();
  }

  resetWindow() { this.elapsed = 0; this.frames = 0; this.fastWindows = 0; }

  sample(frameMs) {
    if (!Number.isFinite(frameMs) || frameMs <= 0) return false;
    this.elapsed += frameMs;
    this.frames++;
    if (this.elapsed < 3000) return false;
    const fps = this.frames * 1000 / this.elapsed;
    this.elapsed = this.frames = 0;
    const before = this.ratio;
    if (fps < 45) {
      this.ratio = Math.max(this.min, this.ratio - 0.15);
      this.fastWindows = 0;
    } else if (fps > 57) {
      if (++this.fastWindows >= 4) {
        this.ratio = Math.min(this.max, this.ratio + 0.15);
        this.fastWindows = 0;
      }
    } else this.fastWindows = 0;
    return this.ratio !== before;
  }
}
