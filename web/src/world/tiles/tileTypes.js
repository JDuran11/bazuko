// src/world/tiles/tileTypes.js

export const TILE_TYPES = {
  // Suelo transitable
  floor:    { id: 'floor',    solid: false, height: 0.2, texture: 'sidewalk' },
  road:     { id: 'road',     solid: false, height: 0.1, texture: 'road'    },
  roadV:    { id: 'roadV',    solid: false, height: 0.1, texture: 'roadV'   },
  sidewalk: { id: 'sidewalk', solid: false, height: 0.2, texture: 'sidewalk'},
  dirt:     { id: 'dirt',     solid: false, height: 0.15,texture: 'dirt'    },

  // Sólidos
  building: { id: 'building', solid: true,  height: 8,   texture: 'concrete'},
  buildingBrick: { id: 'buildingBrick', solid: true, height: 6, texture: 'brick' },
  buildingLit:   { id: 'buildingLit',   solid: true, height: 10, texture: 'windowLit' },

  // Props con colisión
  barrel:   { id: 'barrel',   solid: true,  height: 1.2, texture: null },
  trash:    { id: 'trash',    solid: false, height: 0.6, texture: null }
};

/** Devuelve true si el tile bloquea movimiento */
export function isSolid(typeId) {
  return TILE_TYPES[typeId]?.solid ?? false;
}