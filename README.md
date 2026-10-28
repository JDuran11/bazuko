# Bazuko

Juego de supervivencia ambientado en una Bogotá procedural. Dos versiones en un mismo repositorio:

- [`web/`](web/README.md): versión original en JavaScript + three.js (Vite).
- [`native/`](native/README.md): versión nativa en C++20 (SDL3 + OpenGL 4.5).

## Versión nativa (C++)

Reescritura completa del juego como aplicación de escritorio, sin navegador ni runtime de JavaScript.

### Stack

| Área | Tecnología |
|---|---|
| Lenguaje y build | C++20, CMake ≥ 3.25, Ninja (presets `debug` / `release`) |
| Ventana, entrada, contexto GL | SDL3 |
| Gráficos | OpenGL 4.5 (glad2), glm |
| Interfaz | Dear ImGui (backends SDL3 + OpenGL3), fuente Overpass |
| Audio | miniaudio |
| Modelos y texturas | cgltf, libwebp (`EXT_texture_webp`), stb |
| Datos y guardado | nlohmann/json |
| Pruebas | doctest |

### Arquitectura

El código se divide en bibliotecas con dependencias en una sola dirección:

```
bazuko_core    lógica pura (sin GL ni SDL): datos, simulación, mundo, cámara, iluminación
bazuko_gfx     gráficos en CPU: mallas, canvas 2D, texturas procedurales, glTF, personajes
bazuko_render  capa OpenGL: shaders, buffers, sombras, resolución adaptativa
bazuko_audio   motor de audio (miniaudio)
bazuko         ejecutable: bucle de juego, interfaz ImGui, modelos de entidades
```

```
native/src
  core/       bucle de cuadros, entrada, métricas, resolución adaptativa
  game/       datos, sistemas (necesidades, enfermedades, talentos, inventario, crafteo,
              guardado), entidades, interacción, lugares, textos de interfaz
  world/      calles, ciudad, colisiones AABB, tráfico, bus troncal, navegación de peatones
  render/     mallas, texturas procedurales, mundo visual, sombras, culling, renderer
  character/  glTF, retarget de animaciones, animador, ensamblado de piezas, LOD
  audio/      AudioEngine
  app/        Game, Ui, EntityModels
```

Decisiones de diseño:

- **Lógica separada del render.** `game/` y `world/` no incluyen GL ni SDL, por lo que la simulación se prueba
  en consola y es determinista (RNG sembrado, `Mulberry32` idéntico al del JS).
- **Eventos tipados.** Un `EventBus` basado en `std::variant` reemplaza los `CustomEvent` de `window`; la
  interfaz y el audio los consumen una vez por cuadro.
- **Paridad verificada.** Los scripts `native/tools/export_*.mjs` ejecutan el código JS original y vuelcan sus
  resultados a `native/data/*_reference.json`; los tests de C++ comparan contra esos números (mapa de 120×111
  tiles, colisiones, rutas de tráfico y bus, red peatonal, cámaras, iluminación por hora, etc.).
- **Congelado del mundo.** Menús, creador, muerte y sueño detienen la simulación sin detener el render.

### Render

- Shader forward con luz hemisférica, sol, dos luces puntuales reales (faroles, fuego), niebla lineal y
  tonemapping ACES con salida sRGB.
- Sombra del sol de 2048² ortográfica que sigue al jugador, PCF por hardware y centro ajustado a la rejilla de
  texels (sin temblor al caminar). Se omite de noche.
- Ciudad con texturas procedurales (asfalto, ladrillo, fachadas con ventanas emisivas, letreros con texto),
  agrupada por lotes de 32 m con culling por frustum.
- Visibilidad de edificios: en cámara isométrica las paredes que tapan al jugador se desvanecen; las casas
  afectadas se dibujan por piezas y el resto sale del lote agrupado.
- Silueta del jugador a través de las paredes y halos aditivos en las luces.
- Personajes con skinning en GPU, retarget de animaciones Mixamo → MakeHuman en carga, mezcla de
  idle/walk/run y ensamblado por piezas (pelo, barba, ropa, calzado) con tintes de piel y pelo.
- Personaje procedural liviano de primitivas como alternativa (`--procedural`).

### Optimización

- **Resolución adaptativa:** la escena se dibuja en un FBO multimuestreado a 0,75×–1,5× según el tiempo de
  cuadro (ventanas de 3 s) y se amplía con un triángulo de pantalla completa.
- **LOD de personajes:** modelo completo a < 7 m, liviano a > 10 m y sin sombra propia; animación cada 3
  cuadros pasados 20 m, congelada pasados 40 m y sin animar fuera de cámara.
- **Memoria:** mallas, texturas y esqueletos se comparten entre personajes con las mismas piezas; no se
  decodifican los mapas normales de 4096²; las texturas de personaje se reducen a 1024 px y se libera su copia
  en CPU al subirlas a la GPU.
- **Lotes estáticos:** el mundo se fusiona por chunk; los lotes que no proyectan sombra se excluyen de esa pasada.
- **Carga:** pantalla con progreso por etapas y tolerancia a archivos ausentes.
- **Resultado** (Release, Intel UHD): ~8 ms por cuadro con 72 personajes y ~1.270 dibujos, arranque ~8 s,
  ~283 MB de RAM.

### Mejoras y correcciones respecto a la versión web

Detalle en [`native/CHANGES.md`](native/CHANGES.md).

**Datos y jugabilidad**
- 5 de las 6 recetas de crafteo apuntaban a ítems inexistentes y nunca se podían fabricar; ahora usan ítems reales.
- Las curas (vacuna, pomada, antiparasitario, carbón activo) usaban ids sin enfermedad asociada y se gastaban
  sin curar; el catálogo tiene ahora 6 enfermedades coherentes con sus curas.
- El talento «Superviviente» (−40 % enfermedad) aumentaba el riesgo un 60 %; igual con el origen *desplazado*.
- El crafteo no descontaba el peso de los ingredientes y fallaba con la mochila llena.
- «Selección Experta» no multiplicaba el valor de lo seleccionado y la higiene baja no aumentaba el riesgo de
  enfermar al recoger.
- Los objetos que no cabían en la mochila se perdían en silencio; ahora se avisa.
- La mesa de crafteo ahora es interactuable y la caja de colisión de la tienda coincide con su modelo.
- El aviso de talento desbloqueado salía duplicado.

**Guardado**
- Escritura atómica (archivo temporal + rename), autoguardado cada 60 s, al dormir y al salir.
- Un archivo ilegible se aparta como `*.corrupt` y se arranca una partida nueva en vez de fallar.
- Migra las partidas v1 y v2 del juego web.

**Visual**
- El césped, casi negro por el tinte sobre la textura de tierra, ahora se ve verde.
- Sombra estable (sin temblor) y silueta del jugador sin falsos contornos entre sus partes.

### Compilar y ejecutar

```sh
cd native
export PATH=$HOME/.local/bin:$PATH      # si cmake/ninja se instalaron con pip --user
cmake --preset release && cmake --build --preset release
./build/release/bazuko --new            # partida nueva con creador de personaje

cmake --preset debug && cmake --build --preset debug
./build/debug/bazuko_tests              # pruebas
```

Requiere SDL3, libwebp y cabeceras de Mesa (Fedora: `sudo dnf install SDL3-devel libwebp-devel mesa-libGL-devel`).
Los audios de `web/public/audio` están en Git LFS: `git lfs pull`. Controles y opciones en
[`native/README.md`](native/README.md).

### Limitaciones conocidas

- Sin mapas de relieve ni IBL real (se aproximan con color base y ambiente constante).
- Solo existen las animaciones idle/walk/run; buscar, pedir, comer y dormir se ven como idle.
- Los carteles de la tienda no llevan texto; el creador de personaje no tiene vista 3D propia.
- Si se elimina `web/`: mover `web/public/models` y `web/public/audio` a `native/assets/` y ajustar `.gitattributes`.
