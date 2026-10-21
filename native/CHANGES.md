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

Pendientes de fases siguientes (ya identificados): `sortValueMult` sin efecto, peso al fabricar,
ítems perdidos sin aviso al llenar el inventario, energía sin `decayMult`, autoguardado,
desaturación por ánimo, `CraftingStation` sin interacción.
