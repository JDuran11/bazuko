#include "render/MeshBuilder.h"

namespace bazuko {

void MeshBuilder::vertex(const glm::vec3& p, const glm::vec3& n, const Rgb& c) {
    for (float f : {p.x, p.y, p.z, n.x, n.y, n.z, static_cast<float>(c.r), static_cast<float>(c.g), static_cast<float>(c.b)})
        vertices_.push_back(f);
}

void MeshBuilder::quad(const glm::vec3& a, const glm::vec3& b, const glm::vec3& c, const glm::vec3& d,
                       const glm::vec3& n, const Rgb& color) {
    const auto base = static_cast<std::uint32_t>(vertices_.size() / kFloatsPerVertex);
    vertex(a, n, color);
    vertex(b, n, color);
    vertex(c, n, color);
    vertex(d, n, color);
    for (auto i : {0u, 1u, 2u, 0u, 2u, 3u}) indices_.push_back(base + i);
}

void MeshBuilder::box(const glm::vec3& lo, const glm::vec3& hi, const Rgb& color, bool bottom) {
    // Cada cara se emite con el sentido antihorario visto desde fuera.
    quad({lo.x, hi.y, lo.z}, {lo.x, hi.y, hi.z}, {hi.x, hi.y, hi.z}, {hi.x, hi.y, lo.z}, {0, 1, 0}, color);       // +y
    quad({hi.x, lo.y, lo.z}, {hi.x, hi.y, lo.z}, {hi.x, hi.y, hi.z}, {hi.x, lo.y, hi.z}, {1, 0, 0}, color);       // +x
    quad({lo.x, lo.y, hi.z}, {lo.x, hi.y, hi.z}, {lo.x, hi.y, lo.z}, {lo.x, lo.y, lo.z}, {-1, 0, 0}, color);      // −x
    quad({lo.x, lo.y, hi.z}, {hi.x, lo.y, hi.z}, {hi.x, hi.y, hi.z}, {lo.x, hi.y, hi.z}, {0, 0, 1}, color);       // +z
    quad({hi.x, lo.y, lo.z}, {lo.x, lo.y, lo.z}, {lo.x, hi.y, lo.z}, {hi.x, hi.y, lo.z}, {0, 0, -1}, color);      // −z
    if (bottom) quad({lo.x, lo.y, lo.z}, {hi.x, lo.y, lo.z}, {hi.x, lo.y, hi.z}, {lo.x, lo.y, hi.z}, {0, -1, 0}, color);
}

void MeshBuilder::wall(const glm::vec2& from, const glm::vec2& to, float y0, float y1, const glm::vec3& n,
                       const Rgb& color) {
    quad({from.x, y0, from.y}, {to.x, y0, to.y}, {to.x, y1, to.y}, {from.x, y1, from.y}, n, color);
}

}  // namespace bazuko
