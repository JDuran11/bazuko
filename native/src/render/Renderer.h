#pragma once

#include <cstdint>
#include <memory>
#include <vector>

#include <glm/glm.hpp>

#include "game/CameraRig.h"
#include "game/Lighting.h"
#include "render/Gpu.h"
#include "render/MeshBuilder.h"
#include "render/VisualWorld.h"

namespace bazuko {

// Caja dinámica (jugador, carros, peatones...) definida en el espacio local de una entidad.
struct DynBox {
    glm::vec3 entityPos;
    float entityRotY;
    glm::vec3 localCenter;  // centro de la caja respecto a la entidad
    glm::vec3 size;
    Rgb color;
};

// Pieza de una casa que se dibuja suelta (alguna de sus partes está atenuada).
struct PartDraw {
    unsigned house;  // id de casa (1..N)
    int part;        // 0..3 paredes altas, 4 techo
    float opacity;
};

struct FrameInput {
    const CameraRig* camera = nullptr;
    double aspect = 16.0 / 9;
    Lighting lighting{};
    bool night = false;
    bool cullStatics = true;
    std::vector<DynBox> boxes;
    // Por id de casa (índice 0 = ninguna): 1 si sus partes altas se dibujan por separado.
    const std::vector<std::uint8_t>* houseSeparate = nullptr;
    std::vector<PartDraw> parts;
};

struct RenderStats {
    int drawCalls = 0;
    size_t triangles = 0;
    int batchesVisible = 0, batchesCulled = 0;
};

class Renderer {
public:
    Renderer();
    ~Renderer();

    // Sube texturas, materiales y mallas del distrito a la GPU.
    void uploadWorld(const VisualWorld& world);
    void resize(int width, int height);
    RenderStats render(const FrameInput& frame);
    // Lee el framebuffer (RGBA, de arriba a abajo).
    std::vector<unsigned char> readPixels(int& width, int& height) const;

private:
    struct GpuBatch {
        GpuMesh mesh;
        Aabb bounds;
        int material;
    };
    struct GpuHouse {
        std::vector<std::pair<int, GpuMesh>> parts[5];
    };
    void bindMaterial(const MaterialDef& m, bool night, float alpha);
    void drawBatches(const std::vector<GpuBatch>& list, const class Frustum* frustum, bool night, RenderStats& stats);

    std::unique_ptr<Shader> shader_;
    std::unique_ptr<GpuMesh> unitBox_;
    std::vector<GpuTexture> textures_;
    std::vector<MaterialDef> materials_;
    std::vector<GpuBatch> statics_, closed_;
    std::vector<GpuHouse> houses_;
    GpuUintBuffer houseFlags_;
    int width_ = 1, height_ = 1;
    bool cullEnabled_ = true;
};

}  // namespace bazuko
