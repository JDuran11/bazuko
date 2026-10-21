#pragma once

#include <map>
#include <utility>

#include "render/MeshBuilder.h"
#include "world/Bogota.h"

namespace bazuko {

constexpr double kChunkSize = 32;  // sectores de 32 m

struct Chunk {
    int cx, cz;
    MeshBuilder mesh;
};

// Altura de las cajas que representan los edificios (la fachada detallada llega con la fase 5).
double buildingHeight(Tile t);
// Color base de cada tipo de tile (lineal).
Rgb tileColor(Tile t);

// Suelo, bordillos y volúmenes de edificios, repartidos en sectores de 32 m.
std::vector<Chunk> buildWorldChunks(const Bogota& city);

}  // namespace bazuko
