#include "mini/core/Renderer.hpp"
#include "mini/core/Window.hpp"
#include "mini/platform/OpenGL.hpp"
#include "mini/render/Camera.hpp"
#include "mini/render/Shader.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <cstdint>
#include <iostream>

namespace mini
{

static void checkGL(const char* where)
{
    GLenum err;
    while ((err = glGetError()) != GL_NO_ERROR)
        std::cerr << "[GL Error] " << where << " -> 0x"
                  << std::hex << err << std::dec << std::endl;
}

// ============================================================
// GLSL 3.30 — vertex shader con matrice MVP
// ============================================================

static const char* k_vertSrc = R"glsl(
#version 330 compatibility
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aColor;

uniform mat4 uMVP;   // Model * View * Projection

out vec3 fragColor;

void main()
{
    gl_Position = uMVP * vec4(aPos, 1.0);
    fragColor   = aColor;
}
)glsl";

static const char* k_fragSrc = R"glsl(
#version 330 compatibility
in  vec3 fragColor;
out vec4 outColor;

void main()
{
    outColor = vec4(fragColor, 1.0);
}
)glsl";

// Triangolo in world space — colori Clone Wars
// Layout: [x, y, z,  r, g, b]
static const float k_verts[] = {
     0.0f,  0.7f,  0.0f,   1.00f, 0.55f, 0.05f,  // cima    — arancione
    -0.7f, -0.6f,  0.0f,   0.20f, 0.50f, 1.00f,  // sin     — blu
     0.7f, -0.6f,  0.0f,   1.00f, 1.00f, 1.00f,  // destra  — bianco
};

// ============================================================
// Costruttore / Distruttore
// ============================================================

Renderer::Renderer(Window& window)
    : m_window(window)
{
    while (glGetError() != GL_NO_ERROR) {}

    glViewport(0, 0, window.getWidth(), window.getHeight());
    checkGL("glViewport");

    glEnable(GL_DEPTH_TEST);

    // Camera: posizione leggermente sopra e davanti al triangolo
    const float aspect = static_cast<float>(window.getWidth())
                       / static_cast<float>(window.getHeight());
    m_camera = std::make_unique<Camera>(60.0f, aspect, 0.1f, 100.0f);
    m_camera->setPosition({0.0f, 0.3f, 2.5f});
    m_camera->lookAt({0.0f, 0.0f, 0.0f});

    m_shader = std::make_unique<Shader>(k_vertSrc, k_fragSrc);

    std::cout << "[Renderer] Camera e shader pronti." << std::endl;
    std::cout << "[Renderer] Pronto. Viewport "
              << window.getWidth() << "x" << window.getHeight() << std::endl;
}

Renderer::~Renderer()
{
    // m_vao/m_vbo sono 0 (client-side arrays) — niente da liberare
}

void Renderer::initTriangle()
{
    // Non usata in modalita' client-side arrays
}

Camera& Renderer::getCamera()
{
    return *m_camera;
}

// ============================================================
// Frame lifecycle
// ============================================================

void Renderer::beginFrame(const ClearColor& color)
{
    glClearColor(color.r, color.g, color.b, color.a);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void Renderer::render(float dt)
{
    m_elapsedTime += dt;

    // Matrice Model: lenta rotazione sull'asse Y per mostrare la 3D
    const glm::mat4 model = glm::rotate(
        glm::mat4(1.0f),
        m_elapsedTime * 0.5f,   // 0.5 rad/s
        glm::vec3(0.0f, 1.0f, 0.0f)
    );

    const glm::mat4 mvp = m_camera->getViewProjection() * model;

    m_shader->use();
    m_shader->setMat4("uMVP", glm::value_ptr(mvp));
    checkGL("setMat4 uMVP");

    // Client-side vertex arrays (workaround driver Intel — vedi note in Window.cpp)
    constexpr GLsizei stride = static_cast<GLsizei>(6 * sizeof(float));

    glEnableVertexAttribArray(0u);
    glEnableVertexAttribArray(1u);

    glVertexAttribPointer(0u, 3, GL_FLOAT, GL_FALSE, stride,
                          static_cast<const void*>(k_verts));
    glVertexAttribPointer(1u, 3, GL_FLOAT, GL_FALSE, stride,
                          static_cast<const void*>(k_verts + 3));

    glDrawArrays(GL_TRIANGLES, 0, 3);
    checkGL("glDrawArrays");

    glDisableVertexAttribArray(0u);
    glDisableVertexAttribArray(1u);
}

void Renderer::endFrame()
{
    m_window.swapBuffers();
}

} // namespace mini