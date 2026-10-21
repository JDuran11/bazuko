// src/systems/AudioSystem.js
import { Howl, Howler } from 'howler';

export class AudioSystem {
  constructor() {
    // Volumen maestro (0.0 a 1.0)
    Howler.volume(0.7);

    // Definir sonidos. Si el archivo no existe, Howl queda en silencio
    // pero NO crashea (maneja el error internamente).
    this.sounds = {
      pickup:   new Howl({ src: ['/audio/pickup.wav'],   volume: 0.6 }),
      treasure: new Howl({ src: ['/audio/treasure.wav'], volume: 0.9 }),
      sell:     new Howl({ src: ['/audio/sell.wav'],     volume: 0.8 }),
      full:     new Howl({ src: ['/audio/full.wav'],     volume: 0.5 }),
      empty:    new Howl({ src: ['/audio/empty.wav'],    volume: 0.4 }),
      steps:    new Howl({ src: ['/audio/steps.wav'],    volume: 0.5, loop: true }),
      boost:    new Howl({ src: ['/audio/boost.wav'],    volume: 0.6, loop: true }),
      ambient:  new Howl({ src: ['/audio/ambient.wav'],  volume: 0.25, loop: true })
    };

    // El audio es opcional: un archivo ausente o bloqueado no impide jugar.
    this.ready = Promise.all(Object.values(this.sounds).map(sound => new Promise(resolve => {
      if (sound.state() === 'loaded') { resolve(); return; }
      const done = () => {
        clearTimeout(timer);
        sound.off('load', done);
        sound.off('loaderror', done);
        resolve();
      };
      const timer = setTimeout(done, 5000);
      sound.once('load', done);
      sound.once('loaderror', done);
    })));

    this._stepsPlaying = false;
    this._boostPlaying = false;
    this._ambientStarted = false;

    // Desbloquear audio tras primer click (política de navegadores)
    const unlock = () => {
      if (Howler.ctx && Howler.ctx.state === 'suspended') {
        Howler.ctx.resume();
      }
      window.removeEventListener('click', unlock);
      window.removeEventListener('keydown', unlock);
    };
    window.addEventListener('click', unlock);
    window.addEventListener('keydown', unlock);
  }

  play(name) {
    const s = this.sounds[name];
    if (!s || s.playing()) return;
    s.play();
  }

  stop(name) {
    this.sounds[name]?.stop();
  }

  updateSteps(moving, running) {
    const s = this.sounds.steps;
    if (!s) return;

    if (moving && !this._stepsPlaying) {
      s.rate(running ? 1.4 : 1.0);
      s.play();
      this._stepsPlaying = true;
    } else if (!moving && this._stepsPlaying) {
      s.stop();
      this._stepsPlaying = false;
    }
  }

  updateBoost(active) {
    const s = this.sounds.boost;
    if (!s) return;

    if (active && !this._boostPlaying) {
      s.play();
      this._boostPlaying = true;
    }
    if (!active && this._boostPlaying) {
      s.stop();
      this._boostPlaying = false;
    }
  }

  startAmbient() {
    if (this._ambientStarted) return;
    this.sounds.ambient?.play();
    this._ambientStarted = true;
  }
}
