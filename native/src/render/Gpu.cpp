#include "render/Gpu.h"

#include <stdexcept>
#include <utility>
#include <vector>

namespace bazuko {

namespace {

GLuint compile(GLenum type, const std::string& src) {
    const GLuint s = glCreateShader(type);
    const char* p = src.c_str();
    glShaderSource(s, 1, &p, nullptr);
    glCompileShader(s);
    GLint ok = 0;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        GLint len = 0;
        glGetShaderiv(s, GL_INFO_LOG_LENGTH, &len);
        std::vector<char> log(static_cast<size_t>(len) + 1);
        glGetShaderInfoLog(s, len, nullptr, log.data());
        glDeleteShader(s);
        throw std::runtime_error(std::string(type == GL_VERTEX_SHADER ? "vertex shader: " : "fragment shader: ") + log.data());
    }
    return s;
}

}  // namespace

Shader::Shader(const std::string& vs, const std::string& fs) {
    const GLuint v = compile(GL_VERTEX_SHADER, vs), f = compile(GL_FRAGMENT_SHADER, fs);
    program_ = glCreateProgram();
    glAttachShader(program_, v);
    glAttachShader(program_, f);
    glLinkProgram(program_);
    glDeleteShader(v);
    glDeleteShader(f);
    GLint ok = 0;
    glGetProgramiv(program_, GL_LINK_STATUS, &ok);
    if (!ok) {
        GLint len = 0;
        glGetProgramiv(program_, GL_INFO_LOG_LENGTH, &len);
        std::vector<char> log(static_cast<size_t>(len) + 1);
        glGetProgramInfoLog(program_, len, nullptr, log.data());
        throw std::runtime_error(std::string("link: ") + log.data());
    }
}

Shader::~Shader() {
    if (program_) glDeleteProgram(program_);
}

GLint Shader::uniform(const char* name) const { return glGetUniformLocation(program_, name); }

GpuMesh::GpuMesh(const float* vertices, size_t floatCount, const unsigned* indices, size_t indexCount)
    : indexCount_(indexCount) {
    glGenVertexArrays(1, &vao_);
    glGenBuffers(1, &vbo_);
    glGenBuffers(1, &ibo_);
    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(floatCount * sizeof(float)), vertices, GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo_);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLsizeiptr>(indexCount * sizeof(unsigned)), indices, GL_STATIC_DRAW);
    const GLsizei stride = 9 * sizeof(float);
    for (GLuint i = 0; i < 3; ++i) {
        glEnableVertexAttribArray(i);
        glVertexAttribPointer(i, 3, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(i * 3 * sizeof(float)));
    }
    glBindVertexArray(0);
}

GpuMesh::GpuMesh(GpuMesh&& o) noexcept
    : vao_(std::exchange(o.vao_, 0)), vbo_(std::exchange(o.vbo_, 0)), ibo_(std::exchange(o.ibo_, 0)),
      indexCount_(o.indexCount_) {}

GpuMesh::~GpuMesh() {
    if (ibo_) glDeleteBuffers(1, &ibo_);
    if (vbo_) glDeleteBuffers(1, &vbo_);
    if (vao_) glDeleteVertexArrays(1, &vao_);
}

void GpuMesh::draw() const {
    glBindVertexArray(vao_);
    glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(indexCount_), GL_UNSIGNED_INT, nullptr);
}

}  // namespace bazuko
