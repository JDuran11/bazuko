#pragma once

#include <cmath>
#include <cstdint>

#include "core/Rng.h"

namespace bazuko {

struct Vec3 {
    double x = 0, y = 0, z = 0;
};

inline double distance2D(const Vec3& a, const Vec3& b) { return std::hypot(a.x - b.x, a.z - b.z); }

// Math.round de JavaScript: las mitades suben (2.5 -> 3, -2.5 -> -2).
inline double jsRound(double v) { return std::floor(v + 0.5); }

constexpr double kPi = 3.14159265358979323846;

// Igual que mulberry32 de textures.js: permite reproducir en C++ las secuencias del juego web.
class Mulberry32Rng : public Rng {
public:
    explicit Mulberry32Rng(std::uint32_t seed) : s_(seed) {}
    double next() override {
        s_ += 0x6d2b79f5u;
        std::uint32_t t = (s_ ^ (s_ >> 15)) * (1u | s_);
        t = (t + (t ^ (t >> 7)) * (61u | t)) ^ t;
        return static_cast<double>(t ^ (t >> 14)) / 4294967296.0;
    }

private:
    std::uint32_t s_;
};

}  // namespace bazuko
