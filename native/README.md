# Bazuko nativo (C++)

Reescritura del juego web (`../web`) en C++20 con SDL3 + OpenGL 4.5, por fases.
El plan y el análisis del código original están en `CHANGES.md` y en el historial de ramas `fase-N-*`.

## Dependencias
- Compilador C++20, CMake >= 3.25, Ninja.
- Fedora: `sudo dnf install SDL3-devel mesa-libGL-devel git-lfs` (SDL3 se descarga y compila si no está).

## Datos de referencia
`data/game_data.json` y `data/world_reference.json` salen del código JS original:
```
node tools/export_data.mjs
(cd ../web && npm ci --ignore-scripts) && node tools/export_world.mjs
```

## Compilar y probar
```
cmake --preset debug
cmake --build --preset debug
ctest --preset debug
./build/debug/bazuko
```
Esc cierra la ventana.
