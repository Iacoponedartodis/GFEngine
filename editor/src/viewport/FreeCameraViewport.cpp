#include "viewport/FreeCameraViewport.hpp"

#include <mini/platform/OpenGL.hpp>
#include <mini/render/Camera.hpp>
#include <mini/render/Shader.hpp>

#include <imgui.h>
#include <SDL2/SDL.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <vector>
#include <iostream>
#include <stdexcept>

namespace editor
{

// ── GLSL minimale per linee colorate ─────────────────────────────────────
static const char* k_vert = R"(
#version 330 core
layout(location=0) in vec3 aPos;
layout(location=1) in vec3 aCol;
uniform mat4 uVP;
out vec3 vCol;
void main() { gl_Position = uVP * vec4(aPos, 1.0); vCol = aCol; }
)";

static const char* k_frag = R"(
#version 330 core
in vec3 vCol;
out vec4 fragColor;
void main() { fragColor = vec4(vCol, 1.0); }
)";

FreeCameraViewport::FreeCameraViewport()
{
    // Camera inizialmente sopra la mappa, guardando verso il centro
    m_camera = std::make_unique<mini::Camera>(60.0f, 16.0f/9.0f, 0.1f, 500.0f);
    m_camera->setPosition({0.0f, 12.0f, 20.0f});
    m_camera->lookAt({0.0f, 0.0f, 0.0f});
    m_camera->setSpeed(m_camSpeed);

    m_shader = std::make_unique<mini::Shader>(k_vert, k_frag);
    buildGrid(40.0f, 20);

    // FBO iniziale (verrà ridimensionato al primo frame)
    resizeFBO(1, 1);
}

FreeCameraViewport::~FreeCameraViewport()
{
    if (m_fbo)      glDeleteFramebuffers(1, &m_fbo);
    if (m_colorTex) glDeleteTextures(1, &m_colorTex);
    if (m_depthRbo) glDeleteRenderbuffers(1, &m_depthRbo);
    if (m_gridVAO)  glDeleteVertexArrays(1, &m_gridVAO);
    if (m_gridVBO)  glDeleteBuffers(1, &m_gridVBO);
}

// ── Framebuffer ──────────────────────────────────────────────────────────
void FreeCameraViewport::resizeFBO(int w, int h)
{
    if (w == m_fbWidth && h == m_fbHeight) return;
    m_fbWidth = w; m_fbHeight = h;

    if (m_fbo) glDeleteFramebuffers(1,  &m_fbo);
    if (m_colorTex) glDeleteTextures(1, &m_colorTex);
    if (m_depthRbo) glDeleteRenderbuffers(1, &m_depthRbo);

    glGenFramebuffers(1, &m_fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);

    glGenTextures(1, &m_colorTex);
    glBindTexture(GL_TEXTURE_2D, m_colorTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, w, h, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_colorTex, 0);

    glGenRenderbuffers(1, &m_depthRbo);
    glBindRenderbuffer(GL_RENDERBUFFER, m_depthRbo);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, w, h);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, m_depthRbo);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        std::cerr << "[Viewport] FBO incompleto!" << std::endl;

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

// ── Griglia ──────────────────────────────────────────────────────────────
void FreeCameraViewport::buildGrid(float size, int div)
{
    std::vector<float> verts;
    float step = size / div;
    float half = size * 0.5f;

    auto line = [&](float x0,float y0,float z0, float x1,float y1,float z1,
                    float r, float g, float b)
    {
        verts.insert(verts.end(), {x0,y0,z0, r,g,b, x1,y1,z1, r,g,b});
    };

    // Linee griglia grigie
    for (int i = 0; i <= div; ++i)
    {
        float t = -half + i * step;
        bool axis = (i == div/2);
        float col = axis ? 0.5f : 0.22f;
        line(-half, 0, t,  half, 0, t,  col, col, col);
        line(t, 0, -half,  t, 0,  half, col, col, col);
    }

    // Assi X (rosso), Z (blu)
    line(-half, 0.001f, 0, half, 0.001f, 0, 0.8f, 0.2f, 0.2f);
    line(0, 0.001f, -half, 0, 0.001f, half, 0.2f, 0.4f, 0.9f);

    m_gridVertCount = (int)(verts.size() / 6);

    glGenVertexArrays(1, &m_gridVAO);
    glGenBuffers(1, &m_gridVBO);
    glBindVertexArray(m_gridVAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_gridVBO);
    glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(float), verts.data(), GL_STATIC_DRAW);

    const int stride = 6 * sizeof(float);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, (void*)(3*sizeof(float)));

    glBindVertexArray(0);
}

void FreeCameraViewport::drawGrid(const glm::mat4& vp)
{
    m_shader->use();
    glUniformMatrix4fv(glGetUniformLocation("uVP"), "uVP"), 1, GL_FALSE, glm::value_ptr(vp));
    glBindVertexArray(m_gridVAO);
    glDrawArrays(GL_LINES, 0, m_gridVertCount);
    glBindVertexArray(0);
}

// ── Render scena nell'FBO ────────────────────────────────────────────────
void FreeCameraViewport::renderScene()
{
    glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
    glViewport(0, 0, m_fbWidth, m_fbHeight);
    glEnable(GL_DEPTH_TEST);
    glClearColor(0.06f, 0.07f, 0.12f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    const glm::mat4 vp = m_camera->getViewProjection();
    drawGrid(vp);

    // TODO Stage 2: renderizza mappa e metadata overlay qui

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

// ── Input ────────────────────────────────────────────────────────────────
void FreeCameraViewport::tick(float dt)
{
    if (!m_focused) return;

    const Uint8* ks = SDL_GetKeyboardState(nullptr);
    m_camera->setSpeed(ks[SDL_SCANCODE_LSHIFT] ? m_camSpeed * 3.0f : m_camSpeed);

    m_camera->processKeyboard(
        ks[SDL_SCANCODE_W], ks[SDL_SCANCODE_S],
        ks[SDL_SCANCODE_A], ks[SDL_SCANCODE_D],
        ks[SDL_SCANCODE_E] || ks[SDL_SCANCODE_SPACE],
        ks[SDL_SCANCODE_Q] || ks[SDL_SCANCODE_LCTRL],
        dt
    );

    if (m_mouseCapture)
    {
        int dx = 0, dy = 0;
        SDL_GetRelativeMouseState(&dx, &dy);
        if (dx != 0 || dy != 0)
            m_camera->processMouse((float)dx, (float)dy, 0.15f);
    }
}

// ── Draw (nel pannello ImGui) ─────────────────────────────────────────────
void FreeCameraViewport::draw()
{
    // Dimensioni disponibili nella finestra ImGui
    const ImVec2 avail = ImGui::GetContentRegionAvail();
    const int w = (int)avail.x;
    const int h = (int)(avail.y - 32); // spazio per toolbar

    // ── Toolbar ───────────────────────────────────────────────────────
    ImGui::TextDisabled("WASD = muovi  |  E/Q = su/giu  |  Shift = veloce  |  RMB = guarda");
    ImGui::SameLine(avail.x - 140);

    const char* capLabel = m_mouseCapture ? "Rilascia mouse" : "Cattura mouse";
    if (ImGui::Button(capLabel))
    {
        m_mouseCapture = !m_mouseCapture;
        SDL_SetRelativeMouseMode(m_mouseCapture ? SDL_TRUE : SDL_FALSE);
    }

    // ── Viewport image ────────────────────────────────────────────────
    m_focused = ImGui::IsWindowFocused();

    if (w > 8 && h > 8)
    {
        // Aggiorna aspect ratio camera
        if (h > 0) m_camera->setAspect((float)w / (float)h);

        resizeFBO(w, h);
        renderScene();

        // ImGui::Image vuole texture ID come ImTextureID
        ImGui::Image((ImTextureID)(uintptr_t)m_colorTex, ImVec2((float)w, (float)h),
                     ImVec2(0,1), ImVec2(1,0)); // flip Y per OpenGL
    }
}

} // namespace editor