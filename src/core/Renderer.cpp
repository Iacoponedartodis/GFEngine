#include "mini/core/Renderer.hpp"
#include "mini/core/Window.hpp"
#include "mini/platform/OpenGL.hpp"
#include "mini/render/Shader.hpp"

#include <cstdint>   // uintptr_t
#include <iostream>

namespace mini
{

// ============================================================
// Debug: controlla e stampa eventuali errori GL dopo ogni call critica
// ============================================================

static void checkGL(const char* where)
{
    GLenum err;
    while ((err = glGetError()) != GL_NO_ERROR)
    {
        std::cerr << "[GL Error] " << where
                  << " -> 0x" << std::hex << err << std::dec
                  << std::endl;
    }
}

// ============================================================
// GLSL — hardcoded per ora, asset system in futuro
// ============================================================

static const char* k_vertSrc = R"glsl(
#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aColor;
out vec3 fragColor;
void main()
{
    gl_Position = vec4(aPos, 1.0);
    fragColor   = aColor;
}
)glsl";

static const char* k_fragSrc = R"glsl(
#version 330 core
in  vec3 fragColor;
out vec4 outColor;
void main()
{
    outColor = vec4(fragColor, 1.0);
}
)glsl";

// Triangolo grande e visibile — colori Clone Wars
// Layout interleaved: [x, y, z,  r, g, b]
static const float k_verts[] = {
     0.0f,  0.7f, 0.0f,   1.00f, 0.55f, 0.05f,  // cima    — arancione
    -0.7f, -0.6f, 0.0f,   0.20f, 0.50f, 1.00f,  // sin     — blu
     0.7f, -0.6f, 0.0f,   1.00f, 1.00f, 1.00f,  // destra  — bianco
};

// ============================================================
// Costruttore / Distruttore
// ============================================================

Renderer::Renderer(Window& window)
    : m_window(window)
{
    // Pulisci eventuali errori pre-esistenti prima di iniziare
    while (glGetError() != GL_NO_ERROR) {}

    glViewport(0, 0, window.getWidth(), window.getHeight());
    checkGL("glViewport");

    m_shader = std::make_unique<Shader>(k_vertSrc, k_fragSrc);
    initTriangle();

    std::cout << "[Renderer] Pronto. Viewport "
              << window.getWidth() << "x" << window.getHeight() << std::endl;
}

Renderer::~Renderer()
{
    if (m_vbo != 0) glDeleteBuffers(1, &m_vbo);
    if (m_vao != 0) glDeleteVertexArrays(1, &m_vao);
}

// ============================================================
// Setup VAO / VBO — con controllo errori su ogni call
// ============================================================

void Renderer::initTriangle()
{
    while (glGetError() != GL_NO_ERROR) {}  // clear state

    glGenVertexArrays(1, &m_vao);
    checkGL("glGenVertexArrays");

    glGenBuffers(1, &m_vbo);
    checkGL("glGenBuffers");

    glBindVertexArray(m_vao);
    checkGL("glBindVertexArray");

    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    checkGL("glBindBuffer");

    glBufferData(GL_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(sizeof(k_verts)),
                 k_verts,
                 GL_STATIC_DRAW);
    checkGL("glBufferData");

    // stride: 6 float per vertice = 24 byte
    // offset attr 0: 0 byte dall'inizio (posizione)
    // offset attr 1: 12 byte (3 float) dall'inizio (colore)
    constexpr GLsizei    stride = static_cast<GLsizei>(6 * sizeof(float));
    constexpr uintptr_t  kOff1  = static_cast<uintptr_t>(3 * sizeof(float));

    glVertexAttribPointer(0u, 3, GL_FLOAT, GL_FALSE, stride, nullptr);
    checkGL("glVertexAttribPointer(0)");
    glEnableVertexAttribArray(0u);
    checkGL("glEnableVertexAttribArray(0)");

    glVertexAttribPointer(1u, 3, GL_FLOAT, GL_FALSE, stride,
                          reinterpret_cast<const void*>(kOff1));
    checkGL("glVertexAttribPointer(1)");
    glEnableVertexAttribArray(1u);
    checkGL("glEnableVertexAttribArray(1)");

    glBindVertexArray(0u);
    checkGL("glBindVertexArray unbind");

    std::cout << "[Renderer] VAO=" << m_vao
              << " VBO=" << m_vbo << " configurati." << std::endl;
}

// ============================================================
// Frame lifecycle
// ============================================================

void Renderer::beginFrame(const ClearColor& color)
{
    glClearColor(color.r, color.g, color.b, color.a);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void Renderer::render()
{
    m_shader->use();
    checkGL("glUseProgram");

    glBindVertexArray(m_vao);
    checkGL("glBindVertexArray render");

    glDrawArrays(GL_TRIANGLES, 0, 3);
    checkGL("glDrawArrays");

    glBindVertexArray(0u);
}

void Renderer::endFrame()
{
    m_window.swapBuffers();
}

} // namespace mininamespace mini