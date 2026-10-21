# Cambios respecto al juego web

Correcciones de lógica aplicadas al portar. El código de `web/` no se modifica: las
correcciones de datos viven en `tools/export_data.mjs`, que regenera `data/game_data.json`
(`node tools/export_data.mjs`).

## Fase 1 · datos
| # | Problema en la versión web | Corrección nativa |
|---|---|---|
| 1 | 5 de 6 recetas apuntaban a ítems inexistentes (`arma`, `mochila`, `curita`, `rifa`, `hierba`, y los ingredientes `alambre`, `tela`, `plastico`); `craft()` devolvía `null`. | Quedan 3 recetas con ítems reales: antibiótico (pastilla + botella), vendajes (1 lona → 3) y carbón activo (2 tablas → 1). **Decisión de diseño revisable.** El exportador falla si una receta usa un ítem inexistente. |
| 2 | Las curas usaban ids que no existían como enfermedad (`tetanos`, `sarna`, `parasitos`) y `intoxicacion` ≠ `intoxication`: vacuna, pomada, antiparasitario y carbón se gastaban sin curar. | El catálogo tiene 6 enfermedades (`gripe`, `infeccion`, `intoxicacion`, `tetanos`, `sarna`, `parasitos`) y el exportador falla si una cura no tiene enfermedad. Valores de las 3 nuevas: derivados de los de las existentes (ver el exportador). |
| 3 | El talento `surv2` ("-40 % enfermedad") usaba la clave `diseaseResist`, que se sumaba (1 + 0.6) y aumentaba el riesgo un 60 %. | Pasa a `diseaseResistMult` (0.6) y se multiplica. Igual para el origen *desplazado* (0.7). |

## Fase 2 · simulación
| # | Problema en la versión web | Corrección nativa |
|---|---|---|
| 4 | `CraftingSystem.craft` comprobaba el peso del resultado sin descontar los ingredientes que se iban a consumir: con la mochila llena no se podía fabricar nada. | Se descuenta el peso de los ingredientes (`CraftingSystem::craft`). |
| 5 | El guardado no tenía autoguardado y `SaveSystem.load` no tenía `try/catch` (JSON corrupto o `localStorage` bloqueado rompía el arranque); `clock` ausente lanzaba. | `SaveSystem` v3: escritura atómica (tmp + rename), `AutoSaver` cada 60 s de juego, un archivo ilegible se mueve a `save.json.corrupt` y se arranca partida nueva, campos ausentes toman su valor por defecto. Lee también las partidas v1/v2 del juego web (v1: dólares×250; `intoxication`→`intoxicacion`; ítems que ya no existen se descartan). |
| 6 | `talent-unlock` se mostraba dos veces (lo emitían el sistema y el clic del HUD). | Solo `TalentSystem::unlock` emite el evento. |
| 7 | Los eventos eran `CustomEvent` sobre `window`. | `EventBus` tipado (`std::variant`); la UI/audio consumen con `drain()` por frame. |

Sin cambio, aunque el análisis lo marcó como posible bug:
- La energía no usa `decayMult`: el talento *Estómago de Hierro* dice "hambre y sed", así que es lo descrito.

Pendientes de fases siguientes (ya identificados):
- Fase 8: `sortValueMult` sin efecto (se aplicará al valor de lo seleccionado), aviso al perder ítems por inventario lleno, multiplicador por higiene baja en la probabilidad de enfermar (`StatsSystem::getDiseaseChanceMultiplier` ya existe pero nadie lo usa en la versión web), `CraftingStation` sin interacción, `player.speedMult` muerto.
- Fase 10: desaturación por ánimo bajo (`getDesaturation` ya está portado).
