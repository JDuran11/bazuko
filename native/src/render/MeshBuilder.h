#pragma once

#include <cstdint>
#include <vector>

#include <glm/glm.hpp>

#include "core/MathTypes.h"

namespace bazuko {

// Vértices de 9 floats: posición, normal y color lineal.
class MeshBuilder {
public:
    static constexpr int kFloatsPerVertex = 9;

    void quad(const glm::vec3& a, const glm::vec3& b, const glm::vec3& c, const glm::vec3& d, const glm::vec3& normal,
              const Rgb& color);
    // Caja alineada a los ejes entre min y max; `bottom` añade la cara inferior.
    void box(const glm::vec3& min, const glm::vec3& max, const Rgb& color, bool bottom = false);
    // Cara vertical entre dos alturas a lo largo de un borde horizontal (para bordillos).
    void wall(const glm::vec2& from, const glm::vec2& to, float y0, float y1, const glm::vec3& normal, const Rgb& color);

    bool empty() const { return indices_.empty(); }
    const std::vector<float>& vertices() const { return vertices_; }
    const std::vector<std::uint32_t>& indices() const { return indices_; }
    size_t triangleCount() const { return indices_.size() / 3; }

private:
    void vertex(const glm::vec3& p, const glm::vec3& n, const Rgb& c);
    std::vector<float> vertices_;
    std::vector<std::uint32_t> indices_;
};

}  // namespace bazuko
