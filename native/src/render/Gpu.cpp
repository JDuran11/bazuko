#include "render/Gpu.h"

#include <algorithm>
#include <stdexcept>
#include <utility>

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

constexpr GLenum kTextureMaxAnisotropy = 0x84FE;      // GL_TEXTURE_MAX_ANISOTROPY (core en 4.6)
constexpr GLenum kMaxTextureMaxAnisotropy = 0x84FF;

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
    const GLsizei stride = 12 * sizeof(float);
    // posición, normal, color, uv, id de casa
    const struct { GLint size; size_t offset; } attrs[5] = {{3, 0}, {3, 3}, {3, 6}, {2, 9}, {1, 11}};
    for (GLuint i = 0; i < 5; ++i) {
        glEnableVertexAttribArray(i);
        glVertexAttribPointer(i, attrs[i].size, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(attrs[i].offset * sizeof(float)));
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

GpuTexture::GpuTexture(const Image& img) {
    // La fila 0 de la imagen es el borde superior; GL espera v = 0 abajo, así que se invierten las filas.
    std::vector<std::uint8_t> flipped(img.rgba.size());
    const size_t row = static_cast<size_t>(img.width) * 4;
    for (int y = 0; y < img.height; ++y) std::copy_n(img.rgba.data() + (img.height - 1 - y) * row, row, flipped.data() + y * row);

    glGenTextures(1, &id_);
    glBindTexture(GL_TEXTURE_2D, id_);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, img.srgb ? GL_SRGB8_ALPHA8 : GL_RGBA8, img.width, img.height, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                 flipped.data());
    glGenerateMipmap(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    const GLint wrap = img.repeat ? GL_REPEAT : GL_CLAMP_TO_EDGE;
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrap);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrap);
    GLfloat maxAniso = 0;
    glGetFloatv(kMaxTextureMaxAnisotropy, &maxAniso);
    if (glGetError() == GL_NO_ERROR && maxAniso > 1) glTexParameterf(GL_TEXTURE_2D, kTextureMaxAnisotropy, std::min(8.0f, maxAniso));
}

GpuTexture::GpuTexture(GpuTexture&& o) noexcept : id_(std::exchange(o.id_, 0)) {}

GpuTexture::~GpuTexture() {
    if (id_) glDeleteTextures(1, &id_);
}

void GpuTexture::bind(GLuint unit) const {
    glActiveTexture(GL_TEXTURE0 + unit);
    glBindTexture(GL_TEXTURE_2D, id_);
}

GpuUintBuffer::~GpuUintBuffer() {
    if (id_) glDeleteBuffers(1, &id_);
}

void GpuUintBuffer::upload(const std::vector<std::uint8_t>& values) {
    std::vector<GLuint> data(values.begin(), values.end());
    if (!id_) glGenBuffers(1, &id_);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, id_);
    if (data.size() > capacity_) {
        glBufferData(GL_SHADER_STORAGE_BUFFER, static_cast<GLsizeiptr>(data.size() * sizeof(GLuint)), data.data(), GL_DYNAMIC_DRAW);
        capacity_ = data.size();
    } else {
        glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, static_cast<GLsizeiptr>(data.size() * sizeof(GLuint)), data.data());
    }
}

void GpuUintBuffer::bind(GLuint binding) const { glBindBufferBase(GL_SHADER_STORAGE_BUFFER, binding, id_); }

}  // namespace bazuko
