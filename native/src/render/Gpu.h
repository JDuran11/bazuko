#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include <glad/gl.h>
#include <glm/glm.hpp>

#include "render/Image.h"

namespace bazuko {

// Programa de shaders (vértice + fragmento). Lanza std::runtime_error con el log si falla.
class Shader {
public:
    Shader(const std::string& vertexSrc, const std::string& fragmentSrc);
    ~Shader();
    Shader(const Shader&) = delete;
    Shader& operator=(const Shader&) = delete;

    void use() const { glUseProgram(program_); }
    GLint uniform(const char* name) const;
    void set(const char* name, float v) const { glUniform1f(uniform(name), v); }
    void set(const char* name, int v) const { glUniform1i(uniform(name), v); }
    void set(const char* name, const glm::vec3& v) const { glUniform3fv(uniform(name), 1, &v.x); }
    void set(const char* name, const glm::mat4& m) const { glUniformMatrix4fv(uniform(name), 1, GL_FALSE, &m[0][0]); }

private:
    GLuint program_ = 0;
};

// Malla estática en la GPU: VAO + VBO + IBO con el formato de MeshBuilder (12 floats por vértice).
class GpuMesh {
public:
    GpuMesh(const float* vertices, size_t floatCount, const unsigned* indices, size_t indexCount);
    ~GpuMesh();
    GpuMesh(const GpuMesh&) = delete;
    GpuMesh& operator=(const GpuMesh&) = delete;
    GpuMesh(GpuMesh&& o) noexcept;

    void draw() const;
    size_t triangles() const { return indexCount_ / 3; }

private:
    GLuint vao_ = 0, vbo_ = 0, ibo_ = 0;
    size_t indexCount_ = 0;
};

// Textura 2D con mipmaps y filtrado anisotrópico. Las sRGB se decodifican al muestrear.
class GpuTexture {
public:
    explicit GpuTexture(const Image& image);
    ~GpuTexture();
    GpuTexture(const GpuTexture&) = delete;
    GpuTexture& operator=(const GpuTexture&) = delete;
    GpuTexture(GpuTexture&& o) noexcept;

    void bind(GLuint unit) const;

private:
    GLuint id_ = 0;
};

// Buffer de almacenamiento (SSBO) de enteros sin signo.
class GpuUintBuffer {
public:
    GpuUintBuffer() = default;
    ~GpuUintBuffer();
    GpuUintBuffer(const GpuUintBuffer&) = delete;
    GpuUintBuffer& operator=(const GpuUintBuffer&) = delete;

    void upload(const std::vector<std::uint8_t>& values);  // un uint por elemento
    void bind(GLuint binding) const;

private:
    GLuint id_ = 0;
    size_t capacity_ = 0;
};

}  // namespace bazuko
