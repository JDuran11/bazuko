#pragma once

#include <memory>
#include <vector>

#include <glm/glm.hpp>

#include "game/CameraRig.h"
#include "game/Lighting.h"
#include "render/Gpu.h"
#include "render/MeshBuilder.h"
#include "world/Bogota.h"

namespace bazuko {

// Caja dinámica (jugador, carros, peatones...) definida en el espacio local de una entidad.
struct DynBox {
    glm::vec3 entityPos;
    float entityRotY;
    glm::vec3 localCenter;  // centro de la caja respecto a la entidad
    glm::vec3 size;
    Rgb color;
};

struct FrameInput {
    const CameraRig* camera;
    double aspect;
    Lighting lighting;
    std::vector<DynBox> boxes;
};

struct RenderStats {
    int drawCalls = 0;
    size_t triangles = 0;
};

class Renderer {
public:
    Renderer();
    ~Renderer();

    // Sube la geometría estática del distrito a la GPU.
    void uploadWorld(const Bogota& city);
    void resize(int width, int height);
    RenderStats render(const FrameInput& frame);
    // Lee el framebuffer (RGBA, de arriba a abajo).
    std::vector<unsigned char> readPixels(int& width, int& height) const;

private:
    struct StaticChunk {
        GpuMesh mesh;
    };
    std::unique_ptr<Shader> shader_;
    std::unique_ptr<GpuMesh> unitBox_;
    std::vector<StaticChunk> chunks_;
    int width_ = 1, height_ = 1;
};

}  // namespace bazuko
