#include "render/Renderer.h"

#include <algorithm>

#include <glm/gtc/matrix_transform.hpp>

#include "render/WorldGeometry.h"

namespace bazuko {

namespace {

const char* kVertexSrc = R"(#version 450 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec3 aColor;

uniform mat4 uViewProj;
uniform mat4 uView;
uniform mat4 uModel;

out vec3 vNormal;
out vec3 vColor;
out float vDepth;

void main() {
    vec4 world = uModel * vec4(aPos, 1.0);
    vNormal = mat3(uModel) * aNormal;
    vColor = aColor;
    vDepth = -(uView * world).z;   // profundidad de vista, como la niebla de three.js
    gl_Position = uViewProj * world;
}
)";

// Iluminación difusa estilo three.js (hemisférica + direccional + ambiente) con ACES y niebla lineal.
const char* kFragmentSrc = R"(#version 450 core
in vec3 vNormal;
in vec3 vColor;
in float vDepth;

uniform vec3 uTint;
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
    vec3 n = normalize(vNormal);
    vec3 hemi = mix(uHemiGround, uHemiSky, 0.5 + 0.5 * n.y);
    vec3 sun = max(dot(n, uSunDir), 0.0) * uSunRadiance;
    vec3 albedo = vColor * uTint;
    vec3 lit = albedo * (hemi + sun + uEnvAmbient) / PI;

    vec3 color = linearToSrgb(acesFilmic(lit));
    float fog = smoothstep(uFogNear, uFogFar, vDepth);
    outColor = vec4(mix(color, uFogColor, fog), 1.0);
}
)";

// Intensidad de irradiancia que aporta el entorno (RoomEnvironment) con environmentIntensity = 1.
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
    unitBox_ = std::make_unique<GpuMesh>(cube.vertices().data(), cube.vertices().size(), cube.indices().data(),
                                         cube.indices().size());
    glEnable(GL_DEPTH_TEST);
}

Renderer::~Renderer() = default;

void Renderer::uploadWorld(const Bogota& city) {
    chunks_.clear();
    for (const Chunk& c : buildWorldChunks(city)) {
        if (c.mesh.empty()) continue;
        chunks_.push_back({GpuMesh(c.mesh.vertices().data(), c.mesh.vertices().size(), c.mesh.indices().data(),
                                   c.mesh.indices().size())});
    }
}

void Renderer::resize(int width, int height) {
    width_ = std::max(1, width);
    height_ = std::max(1, height);
}

RenderStats Renderer::render(const FrameInput& f) {
    RenderStats stats;
    glViewport(0, 0, width_, height_);
    const Rgb bg = toSrgb(f.lighting.background);
    glClearColor(static_cast<float>(bg.r), static_cast<float>(bg.g), static_cast<float>(bg.b), 1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    const CameraRig& cam = *f.camera;
    const glm::vec3 eye(cam.position.x, cam.position.y, cam.position.z);
    const glm::vec3 target(cam.lookAt.x, cam.lookAt.y, cam.lookAt.z);
    const glm::mat4 view = glm::lookAt(eye, target, glm::vec3(0, 1, 0));
    const glm::mat4 proj = glm::perspective(glm::radians(static_cast<float>(kCameraFovDegrees)), static_cast<float>(f.aspect),
                                            static_cast<float>(kCameraNear), static_cast<float>(kCameraFar));

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

    shader_->set("uModel", glm::mat4(1.0f));
    shader_->set("uTint", glm::vec3(1.0f));
    for (const StaticChunk& c : chunks_) {
        c.mesh.draw();
        ++stats.drawCalls;
        stats.triangles += c.mesh.triangles();
    }

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
