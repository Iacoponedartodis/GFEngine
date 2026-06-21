#include "mini/core/Renderer.hpp"
#include "mini/core/Window.hpp"
#include "mini/platform/OpenGL.hpp"
#include "mini/render/Camera.hpp"
#include "mini/render/Mesh.hpp"
#include "mini/render/Shader.hpp"
#include "mini/render/Texture.hpp"

#include <SDL2/SDL.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <iostream>

namespace mini
{

static void checkGL(const char* w)
{
    GLenum err;
    while ((err = glGetError()) != GL_NO_ERROR)
        std::cerr << "[GL Error] " << w << " -> 0x" << std::hex << err << std::dec << std::endl;
}

// ============================================================
// GLSL — Blinn-Phong + texture + color tint per entita'
// ============================================================

static const char* k_vertSrc = R"glsl(
#version 330 compatibility
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec3 aColor;
layout(location = 3) in vec2 aUV;

uniform mat4 uMVP;
uniform mat4 uModel;

out vec3 fragWorldPos;
out vec3 fragNormal;
out vec3 fragColor;
out vec2 fragUV;

void main()
{
    gl_Position  = uMVP * vec4(aPos, 1.0);
    fragWorldPos = vec3(uModel * vec4(aPos, 1.0));
    fragNormal   = mat3(transpose(inverse(uModel))) * aNormal;
    fragColor    = aColor;
    fragUV       = aUV;
}
)glsl";

static const char* k_fragSrc = R"glsl(
#version 330 compatibility
in vec3 fragWorldPos;
in vec3 fragNormal;
in vec3 fragColor;
in vec2 fragUV;

uniform sampler2D uAlbedo;
uniform int       uUseTexture;
uniform vec3      uColorTint;   // moltiplicatore colore per-entita'

uniform vec3 uLightDir;
uniform vec3 uLightColor;
uniform vec3 uCameraPos;

out vec4 outColor;

void main()
{
    vec3 base = (uUseTexture != 0)
              ? texture(uAlbedo, fragUV).rgb * uColorTint
              : fragColor * uColorTint;

    vec3 n       = normalize(fragNormal);
    vec3 viewDir = normalize(uCameraPos - fragWorldPos);

    vec3 ambient  = 0.45 * uLightColor;
    float diff    = max(dot(n, uLightDir), 0.0);
    vec3 diffuse  = diff * uLightColor;
    vec3 halfDir  = normalize(uLightDir + viewDir);
    float spec    = pow(max(dot(n, halfDir), 0.0), 24.0);
    vec3 specular = 0.4 * spec * uLightColor;

    outColor = vec4((ambient + diffuse + specular) * base, 1.0);
}
)glsl";

// ============================================================
// Costruttore / Distruttore
// ============================================================

Renderer::Renderer(Window& window)
    : m_window(window)
{
    while (glGetError() != GL_NO_ERROR) {}

    glViewport(0, 0, window.getWidth(), window.getHeight());
    glEnable(GL_DEPTH_TEST);

    const float aspect = static_cast<float>(window.getWidth())
                       / static_cast<float>(window.getHeight());
    m_camera = std::make_unique<Camera>(60.0f, aspect, 0.1f, 100.0f);
    m_camera->setPosition({0.0f, 1.0f, 5.0f});
    m_camera->lookAt({0.0f, 0.0f, 0.0f});

    m_shader = std::make_unique<Shader>(k_vertSrc, k_fragSrc);

    checkGL("Renderer init");
    std::cout << "[Renderer] Pronto. Viewport "
              << window.getWidth() << "x" << window.getHeight() << std::endl;
}

Renderer::~Renderer() = default;

Camera& Renderer::getCamera() { return *m_camera; }

// ============================================================
// Frame lifecycle
// ============================================================

void Renderer::beginFrame(const ClearColor& col)
{
    // Aggiorna viewport alle dimensioni correnti della finestra
    int w = 0, h = 0;
    SDL_GL_GetDrawableSize(SDL_GL_GetCurrentWindow(), &w, &h);
    if (w > 0 && h > 0)
    {
        glViewport(0, 0, w, h);
        m_camera->setAspect((float)w / (float)h);
    }
    glClearColor(col.r, col.g, col.b, col.a);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void Renderer::drawMesh(const Mesh&     mesh,
                         const Texture*  texture,
                         const glm::mat4& model,
                         const glm::vec3& colorTint)
{
    const glm::mat4 mvp      = m_camera->getViewProjection() * model;
    const glm::vec3 lightDir = glm::normalize(glm::vec3{0.4f, 0.8f, 0.5f});
    const glm::vec3& camPos  = m_camera->getPosition();

    m_shader->use();
    m_shader->setMat4("uMVP",        glm::value_ptr(mvp));
    m_shader->setMat4("uModel",      glm::value_ptr(model));
    m_shader->setVec3("uColorTint",  colorTint.x, colorTint.y, colorTint.z);
    m_shader->setVec3("uLightDir",   lightDir.x,  lightDir.y,  lightDir.z);
    m_shader->setVec3("uLightColor", 1.0f, 0.95f, 0.85f);
    m_shader->setVec3("uCameraPos",  camPos.x,    camPos.y,    camPos.z);

    if (texture)
    {
        texture->bind(0);
        m_shader->setInt("uAlbedo",     0);
        m_shader->setInt("uUseTexture", 1);
    }
    else
    {
        m_shader->setInt("uUseTexture", 0);
    }

    mesh.draw();
    checkGL("drawMesh");
}

void Renderer::endFrame()
{
    m_window.swapBuffers();
}

} // namespace mini