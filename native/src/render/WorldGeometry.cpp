#include "render/WorldGeometry.h"

#include <cmath>

namespace bazuko {

namespace {
constexpr float T = static_cast<float>(kTileSize);
constexpr float H = T / 2;

int chunkOf(double meters) { return static_cast<int>(std::floor(meters / kChunkSize)); }

// Variación pequeña y estable por posición, para que las manzanas no sean un solo color.
double jitter(int x, int z) {
    unsigned h = static_cast<unsigned>(x) * 73856093u ^ static_cast<unsigned>(z) * 19349663u;
    h ^= h >> 13;
    h *= 0x5bd1e995u;
    h ^= h >> 15;
    return (h & 1023) / 1023.0;
}
}  // namespace

double buildingHeight(Tile t) {
    switch (t) {
        case Tile::Wall: return 9;
        case Tile::Ruin: return 4.5;
        default: return 6.5;
    }
}

Rgb tileColor(Tile t) {
    switch (t) {
        case Tile::Road: return rgbFromHex(0x2b2d31);
        case Tile::Busway: return rgbFromHex(0x7a4a45);
        case Tile::Sidewalk: return rgbFromHex(0x9a958c);
        case Tile::Floor: return rgbFromHex(0x6c6c68);
        case Tile::Dirt: return rgbFromHex(0x6b5d4a);
        case Tile::Grass: return rgbFromHex(0x3e5a2c);
        case Tile::Plaza: return rgbFromHex(0xa89880);
        case Tile::Building: return rgbFromHex(0x7b6a5a);
        case Tile::Ruin: return rgbFromHex(0x4f4a45);
        case Tile::Wall: return rgbFromHex(0x3a3a40);
    }
    return {};
}

std::vector<Chunk> buildWorldChunks(const Bogota& city) {
    const TileMap& map = city.map();
    std::map<std::pair<int, int>, Chunk> chunks;
    auto chunkAt = [&](double mx, double mz) -> MeshBuilder& {
        const int cx = chunkOf(mx), cz = chunkOf(mz);
        auto it = chunks.try_emplace({cx, cz}, Chunk{cx, cz, {}}).first;
        return it->second.mesh;
    };
    const Rgb curb = rgbFromHex(0xd8d4c8);

    // Suelo: un cuadrado por tile a su cota, y bordillo donde el vecino queda más bajo.
    for (int z = 0; z < map.height(); ++z) {
        for (int x = 0; x < map.width(); ++x) {
            const Tile t = map.at(x, z);
            if (isSolid(t)) continue;
            const float cx = x * T, cz = z * T;
            const float y = static_cast<float>(groundHeight(t));
            MeshBuilder& mb = chunkAt(cx, cz);
            mb.quad({cx - H, y, cz - H}, {cx - H, y, cz + H}, {cx + H, y, cz + H}, {cx + H, y, cz - H}, {0, 1, 0}, tileColor(t));
            struct Side { int dx, dz; glm::vec2 a, b; glm::vec3 n; };
            const Side sides[4] = {{1, 0, {cx + H, cz - H}, {cx + H, cz + H}, {1, 0, 0}},
                                   {-1, 0, {cx - H, cz + H}, {cx - H, cz - H}, {-1, 0, 0}},
                                   {0, 1, {cx + H, cz + H}, {cx - H, cz + H}, {0, 0, 1}},
                                   {0, -1, {cx - H, cz - H}, {cx + H, cz - H}, {0, 0, -1}}};
            for (const Side& s : sides) {
                if (!map.inside(x + s.dx, z + s.dz)) continue;
                const Tile n = map.at(x + s.dx, z + s.dz);
                if (isSolid(n)) continue;
                const float ny = static_cast<float>(groundHeight(n));
                if (ny < y - 1e-4f) mb.wall(s.a, s.b, ny, y, s.n, curb);
            }
        }
    }

    // Edificios: tramos de tiles sólidos del mismo tipo unidos en una caja.
    for (int z = 0; z < map.height(); ++z) {
        int x = 0;
        while (x < map.width()) {
            const Tile t = map.at(x, z);
            if (!isSolid(t)) { ++x; continue; }
            int x2 = x;
            while (x2 + 1 < map.width() && map.at(x2 + 1, z) == t) ++x2;
            const float h = static_cast<float>(buildingHeight(t)) * (t == Tile::Wall ? 1.0f : 0.85f + 0.3f * static_cast<float>(jitter(x, z)));
            Rgb c = tileColor(t);
            const double shade = 0.85 + 0.3 * jitter(z, x);
            c = {c.r * shade, c.g * shade, c.b * shade};
            chunkAt((x + x2) * T / 2, z * T).box({x * T - H, 0, z * T - H}, {x2 * T + H, h, z * T + H}, c);
            x = x2 + 1;
        }
    }

    std::vector<Chunk> out;
    for (auto& [key, chunk] : chunks) out.push_back(std::move(chunk));
    return out;
}

}  // namespace bazuko
