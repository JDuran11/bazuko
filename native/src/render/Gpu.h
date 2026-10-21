#pragma once

#include <string>

#include <glad/gl.h>
#include <glm/glm.hpp>

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
    void set(const char* name, const glm::vec3& v) const { glUniform3fv(uniform(name), 1, &v.x); }
    void set(const char* name, const glm::mat4& m) const { glUniformMatrix4fv(uniform(name), 1, GL_FALSE, &m[0][0]); }

private:
    GLuint program_ = 0;
};

// Malla estática en la GPU: VAO + VBO + IBO con el formato de MeshBuilder.
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

}  // namespace bazuko
