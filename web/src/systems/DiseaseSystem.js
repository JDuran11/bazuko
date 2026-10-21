// src/systems/DiseaseSystem.js
import {
  DISEASE_BASE_CHANCE,
  DISEASE_SORT_MULT,
  DISEASE_HEALTH_DRAIN,
  DISEASE_MENTAL_DRAIN,
  DISEASE_DURATION
} from '../utils/constants.js';

const DISEASES = {
  gripe: {
    id: 'gripe',
    name: 'Gripe',
    emoji: '🤧',
    healthDrain: DISEASE_HEALTH_DRAIN,
    mentalDrain: DISEASE_MENTAL_DRAIN * 0.5,
    duration: DISEASE_DURATION
  },
  infeccion: {
    id: 'infeccion',
    name: 'Infección',
    emoji: '🦠',
    healthDrain: DISEASE_HEALTH_DRAIN * 1.8,
    mentalDrain: DISEASE_MENTAL_DRAIN,
    duration: DISEASE_DURATION * 1.2
  },
  intoxication: {
    id: 'intoxication',
    name: 'Intoxicación',
    emoji: '🤢',
    healthDrain: DISEASE_HEALTH_DRAIN * 0.8,
    mentalDrain: DISEASE_MENTAL_DRAIN * 1.5,
    duration: DISEASE_DURATION * 0.8
  }
};

export class DiseaseSystem {
  constructor() {
    this.active = {}; // { id: { timeLeft } }
  }

  get hasAny() { return Object.keys(this.active).length > 0; }

  get list() {
    return Object.entries(this.active).map(([id, d]) => ({
      ...DISEASES[id],
      timeLeft: d.timeLeft
    }));
  }

  /**
   * Intenta infectar al jugador al recoger basura.
   * @param {boolean} sorted - si se seleccionó la basura (más riesgo)
   * @param {number} resistanceMult - mult de resistencia (talentos)
   * @returns {object|null} enfermedad contraída o null
   */
  tryInfect(sorted = false, resistanceMult = 1) {
    let chance = DISEASE_BASE_CHANCE;
    if (sorted) chance *= DISEASE_SORT_MULT;
    chance *= resistanceMult;

    if (Math.random() > chance) return null;

    const ids = Object.keys(DISEASES);
    const id = ids[Math.floor(Math.random() * ids.length)];

    // No apilar la misma enfermedad
    if (this.active[id]) {
      this.active[id].timeLeft = DISEASES[id].duration;
      return null;
    }

    this.active[id] = { timeLeft: DISEASES[id].duration };
    window.dispatchEvent(new CustomEvent('disease', {
      detail: { name: DISEASES[id].name, emoji: DISEASES[id].emoji }
    }));
    return DISEASES[id];
  }

  /** Cura todas las enfermedades (antibiótico) */
  cureAll() {
    this.active = {};
  }

  /** Cura una enfermedad específica */
  cure(id) {
    delete this.active[id];
  }

  update(dt) {
    for (const id of Object.keys(this.active)) {
      this.active[id].timeLeft -= dt;
      if (this.active[id].timeLeft <= 0) {
        delete this.active[id];
        window.dispatchEvent(new CustomEvent('disease-cured', {
          detail: { name: DISEASES[id]?.name ?? id }
        }));
      }
    }
  }

  /** Drenaje total de salud por segundo */
  get healthDrain() {
    return Object.entries(this.active).reduce(
      (s, [id]) => s + (DISEASES[id]?.healthDrain ?? 0), 0
    );
  }

  /** Drenaje total de salud mental por segundo */
  get mentalDrain() {
    return Object.entries(this.active).reduce(
      (s, [id]) => s + (DISEASES[id]?.mentalDrain ?? 0), 0
    );
  }

  /** Efectos agregados de todas las enfermedades activas */
  getAggregatedEffects() {
    let healthDrain = 0;
    let mentalDrain = 0;
    let speedMultiplier = 1;
    let hungerDrainMultiplier = 1;
    let energyDrainMultiplier = 1;
    let hygieneDrain = 0;
    let canEat = true;

    for (const id of Object.keys(this.active)) {
      const def = DISEASES[id];
      if (!def) continue;
      healthDrain += def.healthDrain ?? 0;
      mentalDrain += def.mentalDrain ?? 0;
      if (def.speedMultiplier) speedMultiplier *= def.speedMultiplier;
      if (def.hungerDrainMultiplier) hungerDrainMultiplier *= def.hungerDrainMultiplier;
      if (def.energyDrainMultiplier) energyDrainMultiplier *= def.energyDrainMultiplier;
      if (def.hygieneDrain) hygieneDrain += def.hygieneDrain;
      if (def.canEat === false) canEat = false;
    }

    return {
      healthDrain: healthDrain || this.healthDrain,
      mentalDrain: mentalDrain || this.mentalDrain,
      speedMultiplier,
      hungerDrainMultiplier,
      energyDrainMultiplier,
      hygieneDrain,
      canEat
    };
  }

  serialize() { return { active: this.active }; }
  deserialize(d) { this.active = d?.active ?? {}; }
}
