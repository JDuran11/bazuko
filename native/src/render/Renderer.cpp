#include "render/Renderer.h"

#include <algorithm>

#include <glm/gtc/matrix_transform.hpp>

#include "render/Frustum.h"

namespace bazuko {

namespace {

const char* kVertexSrc = R"(#version 450 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec3 aColor;
layout(location = 3) in vec2 aUv;
layout(location = 4) in float aHouse;

layout(std430, binding = 0) readonly buffer HouseFlags { uint flags[]; };

uniform mat4 uViewProj;
uniform mat4 uView;
uniform mat4 uModel;

out vec3 vNormal;
out vec3 vColor;
out vec2 vUv;
out float vDepth;
flat out uint vSeparate;

void main() {
    vec4 world = uModel * vec4(aPos, 1.0);
    vNormal = mat3(uModel) * aNormal;
    vColor = aColor;
    vUv = aUv;
    vDepth = -(uView * world).z;   // profundidad de vista, como la niebla de three.js
    int house = int(aHouse + 0.5);
    vSeparate = house > 0 ? flags[house] : 0u;  // la casa se dibuja por piezas: se descarta aquí
    gl_Position = uViewProj * world;
}
)";

// Iluminación difusa estilo three.js (hemisférica + direccional + ambiente) con ACES y niebla lineal.
const char* kFragmentSrc = R"(#version 450 core
in vec3 vNormal;
in vec3 vColor;
in vec2 vUv;
in float vDepth;
flat in uint vSeparate;

uniform sampler2D uMap;
uniform sampler2D uEmissiveMap;
uniform int uHasMap;
uniform int uHasEmissive;
uniform vec3 uTint;
uniform float uEmissiveScale;
uniform int uUnlit;
uniform int uNoFog;
uniform float uAlpha;
uniform vec3 uSunDir;        // hacia el sol
uniform vec3 uSunRadiance;   // color * intensidad
uniform vec3 uHemiSky;       // color * intensidad
uniform vec3 uHemiGround;
uniform vec3 uEnvAmbient;    // aproximación del IBL de RoomEnvironment
uniform vec3 uFogColor;      // sRGB, igual que el fondo
uniform float uFogNear;
uniform float uFogFar;
uniform float uExposure;

out vec4 outColor;

const float PI = 3.14159265359;

vec3 rrtAndOdtFit(vec3 v) {
    vec3 a = v * (v + 0.0245786) - 0.000090537;
    vec3 b = v * (0.983729 * v + 0.4329510) + 0.238081;
    return a / b;
}

vec3 acesFilmic(vec3 color) {
    const mat3 inputMat = mat3(vec3(0.59719, 0.07600, 0.02840), vec3(0.35458, 0.90834, 0.13383), vec3(0.04823, 0.01566, 0.83777));
    const mat3 outputMat = mat3(vec3(1.60475, -0.10208, -0.00327), vec3(-0.53108, 1.10813, -0.07276), vec3(-0.07367, -0.00605, 1.07602));
    color *= uExposure / 0.6;
    color = inputMat * color;
    color = rrtAndOdtFit(color);
    color = outputMat * color;
    return clamp(color, 0.0, 1.0);
}

vec3 linearToSrgb(vec3 c) {
    return mix(pow(c, vec3(0.41666)) * 1.055 - 0.055, c * 12.92, vec3(lessThanEqual(c, vec3(0.0031308))));
}

void main() {
    if (vSeparate != 0u) discard;
    vec4 base = vec4(vColor * uTint, 1.0);
    if (uHasMap != 0) base *= texture(uMap, vUv);

    vec3 n = normalize(vNormal);
    if (!gl_FrontFacing) n = -n;
    vec3 lit;
    if (uUnlit != 0) {
        lit = base.rgb;
    } else {
        vec3 hemi = mix(uHemiGround, uHemiSky, 0.5 + 0.5 * n.y);
        vec3 sun = max(dot(n, uSunDir), 0.0) * uSunRadiance;
        lit = base.rgb * (hemi + sun + uEnvAmbient) / PI;
    }
    if (uHasEmissive != 0) lit += texture(uEmissiveMap, vUv).rgb * uEmissiveScale;

    vec3 color = linearToSrgb(acesFilmic(lit));
    if (uNoFog == 0) color = mix(color, uFogColor, smoothstep(uFogNear, uFogFar, vDepth));
    outColor = vec4(color, uAlpha);
}
)";

// Irradiancia que aporta el entorno (RoomEnvironment) con environmentIntensity = 1.
constexpr float kEnvIrradiance = 1.5f;
constexpr float kExposure = 1.05f;

glm::vec3 v3(const Rgb& c, double scale = 1) {
    return {static_cast<float>(c.r * scale), static_cast<float>(c.g * scale), static_cast<float>(c.b * scale)};
}

}  // namespace

Renderer::Renderer() {
    shader_ = std::make_unique<Shader>(kVertexSrc, kFragmentSrc);
    MeshBuilder cube;
    cube.box({-0.5f, -0.5f, -0.5f}, {0.5f, 0.5f, 0.5f}, {1, 1, 1}, true);
    unitBox_ = std::make_unique<GpuMesh>(cube.vertices().data(), cube.vertices().size(), cube.indices().data(), cube.indices().size());
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    houseFlags_.upload(std::vector<std::uint8_t>(1, 0));
}

Renderer::~Renderer() = default;

void Renderer::uploadWorld(const VisualWorld& world) {
    textures_.clear();
    for (const Image& img : world.textures) textures_.emplace_back(img);
    materials_ = world.materials;
    auto upload = [](const std::vector<CpuBatch>& in, std::vector<GpuBatch>& out) {
        out.clear();
        for (const CpuBatch& b : in) {
            if (b.mesh.empty()) continue;
            out.push_back({GpuMesh(b.mesh.vertices().data(), b.mesh.vertices().size(), b.mesh.indices().data(), b.mesh.indices().size()),
                           b.bounds, b.material});
        }
    };
    upload(world.statics, statics_);
    upload(world.closed, closed_);
    houses_.clear();
    houses_.resize(world.houses.size());
    for (size_t h = 0; h < world.houses.size(); ++h) {
        for (int p = 0; p < 5; ++p) {
            const HousePart& part = p < 4 ? world.houses[h].walls[p] : world.houses[h].roof;
            for (const auto& [mat, mesh] : part.draws)
                houses_[h].parts[p].emplace_back(mat, GpuMesh(mesh.vertices().data(), mesh.vertices().size(), mesh.indices().data(), mesh.indices().size()));
        }
    }
    houseFlags_.upload(std::vector<std::uint8_t>(world.houses.size() + 1, 0));
}

void Renderer::resize(int width, int height) {
    width_ = std::max(1, width);
    height_ = std::max(1, height);
}

void Renderer::bindMaterial(const MaterialDef& m, bool night, float alpha) {
    const bool hasMap = m.map >= 0 && m.map < static_cast<int>(textures_.size());
    const bool hasEmissive = m.emissive >= 0 && m.emissive < static_cast<int>(textures_.size());
    if (hasMap) textures_[m.map].bind(0);
    if (hasEmissive) textures_[m.emissive].bind(1);
    shader_->set("uHasMap", hasMap ? 1 : 0);
    shader_->set("uHasEmissive", hasEmissive ? 1 : 0);
    shader_->set("uTint", v3(m.tint));
    shader_->set("uEmissiveScale", (m.nightEmissive && !night) ? 0.0f : m.emissiveIntensity);
    shader_->set("uUnlit", m.unlit ? 1 : 0);
    shader_->set("uNoFog", m.noFog ? 1 : 0);
    shader_->set("uAlpha", alpha);
    if (m.doubleSided == cullEnabled_) {
        cullEnabled_ = !m.doubleSided;
        if (cullEnabled_) glEnable(GL_CULL_FACE); else glDisable(GL_CULL_FACE);
    }
    if (m.polygonOffset) {
        glEnable(GL_POLYGON_OFFSET_FILL);
        glPolygonOffset(-2.0f, -2.0f);
    } else {
        glDisable(GL_POLYGON_OFFSET_FILL);
    }
}

void Renderer::drawBatches(const std::vector<GpuBatch>& list, const Frustum* frustum, bool night, RenderStats& stats) {
    for (const GpuBatch& b : list) {
        if (frustum && !frustum->intersects(b.bounds)) {
            ++stats.batchesCulled;
            continue;
        }
        bindMaterial(materials_[b.material], night, 1.0f);
        b.mesh.draw();
        ++stats.drawCalls;
        ++stats.batchesVisible;
        stats.triangles += b.mesh.triangles();
    }
}

RenderStats Renderer::render(const FrameInput& f) {
    RenderStats stats;
    glViewport(0, 0, width_, height_);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    const Rgb bg = toSrgb(f.lighting.background);
    glClearColor(static_cast<float>(bg.r), static_cast<float>(bg.g), static_cast<float>(bg.b), 1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    const CameraRig& cam = *f.camera;
    const glm::vec3 eye(cam.position.x, cam.position.y, cam.position.z);
    const glm::vec3 target(cam.lookAt.x, cam.lookAt.y, cam.lookAt.z);
    const glm::mat4 view = glm::lookAt(eye, target, glm::vec3(0, 1, 0));
    const glm::mat4 proj = glm::perspective(glm::radians(static_cast<float>(kCameraFovDegrees)), static_cast<float>(f.aspect),
                                            static_cast<float>(kCameraNear), static_cast<float>(kCameraFar));
    const Frustum frustum = Frustum::fromViewProjection(proj * view);

    const Lighting& l = f.lighting;
    const Vec3 sun = sunDirection();
    shader_->use();
    shader_->set("uViewProj", proj * view);
    shader_->set("uView", view);
    shader_->set("uSunDir", glm::vec3(static_cast<float>(sun.x), static_cast<float>(sun.y), static_cast<float>(sun.z)));
    shader_->set("uSunRadiance", v3(l.sunColor, l.sunIntensity));
    shader_->set("uHemiSky", v3(l.hemiSky, l.hemiIntensity));
    shader_->set("uHemiGround", v3(l.hemiGround, l.hemiIntensity));
    shader_->set("uEnvAmbient", glm::vec3(static_cast<float>(l.envIntensity) * kEnvIrradiance));
    shader_->set("uFogColor", v3(toSrgb(l.background)));
    shader_->set("uFogNear", static_cast<float>(l.fogNear));
    shader_->set("uFogFar", static_cast<float>(l.fogFar));
    shader_->set("uExposure", kExposure);
    shader_->set("uMap", 0);
    shader_->set("uEmissiveMap", 1);
    shader_->set("uModel", glm::mat4(1.0f));

    // Casas dibujadas por piezas: sus vértices del lote agrupado se descartan en el shader.
    if (f.houseSeparate) houseFlags_.upload(*f.houseSeparate);
    houseFlags_.bind(0);

    cullEnabled_ = true;
    glEnable(GL_CULL_FACE);
    const Frustum* fr = f.cullStatics ? &frustum : nullptr;
    drawBatches(statics_, fr, f.night, stats);
    drawBatches(closed_, fr, f.night, stats);

    // Piezas sueltas opacas (casas con alguna pared atenuada).
    auto drawPart = [&](const PartDraw& p, float alpha) {
        for (const auto& [mat, mesh] : houses_[p.house - 1].parts[p.part]) {
            bindMaterial(materials_[mat], f.night, alpha);
            mesh.draw();
            ++stats.drawCalls;
            stats.triangles += mesh.triangles();
        }
    };
    for (const PartDraw& p : f.parts)
        if (p.opacity >= 1.0f && p.opacity > 0) drawPart(p, 1.0f);

    // Cajas dinámicas.
    MaterialDef plain;
    bindMaterial(plain, f.night, 1.0f);
    for (const DynBox& b : f.boxes) {
        glm::mat4 m = glm::translate(glm::mat4(1.0f), b.entityPos);
        m = glm::rotate(m, b.entityRotY, glm::vec3(0, 1, 0));
        m = glm::translate(m, b.localCenter);
        m = glm::scale(m, b.size);
        shader_->set("uModel", m);
        shader_->set("uTint", v3(b.color));
        unitBox_->draw();
        ++stats.drawCalls;
        stats.triangles += unitBox_->triangles();
    }
    shader_->set("uModel", glm::mat4(1.0f));

    // Piezas atenuadas: mezcla alfa sin escribir profundidad.
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);
    for (const PartDraw& p : f.parts)
        if (p.opacity < 1.0f && p.opacity > 0) drawPart(p, p.opacity);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    return stats;
}

std::vector<unsigned char> Renderer::readPixels(int& width, int& height) const {
    width = width_;
    height = height_;
    std::vector<unsigned char> px(static_cast<size_t>(width) * height * 4), flipped(px.size());
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, px.data());
    const size_t row = static_cast<size_t>(width) * 4;
    for (int y = 0; y < height; ++y) std::copy_n(px.data() + (height - 1 - y) * row, row, flipped.data() + y * row);
    return flipped;
}

}  // namespace bazuko
