#include "viewport/FreeCameraViewport.hpp"

// OpenGL.hpp DEVE stare prima: definisce GLsizei, GLuint, GLenum, ecc.
#include <mini/platform/OpenGL.hpp>
#include <SDL2/SDL.h>

// ── Costanti FBO (GL 3.0+, non in <GL/gl.h> su Windows) ─────────────────
#ifndef GL_FRAMEBUFFER
  #define GL_FRAMEBUFFER              0x8D40
#endif
#ifndef GL_COLOR_ATTACHMENT0
  #define GL_COLOR_ATTACHMENT0        0x8CE0
#endif
#ifndef GL_DEPTH_STENCIL_ATTACHMENT
  #define GL_DEPTH_STENCIL_ATTACHMENT 0x821A
#endif
#ifndef GL_DEPTH24_STENCIL8
  #define GL_DEPTH24_STENCIL8         0x88F0
#endif
#ifndef GL_RENDERBUFFER
  #define GL_RENDERBUFFER             0x8D41
#endif
#ifndef GL_FRAMEBUFFER_COMPLETE
  #define GL_FRAMEBUFFER_COMPLETE     0x8CD5
#endif

// ── Puntatori funzione FBO (caricati a runtime) ──────────────────────────
typedef void   (*PFBO_GenFBO)    (GLsizei, GLuint*);
typedef void   (*PFBO_BindFBO)   (GLenum,  GLuint);
typedef void   (*PFBO_DelFBO)    (GLsizei, const GLuint*);
typedef void   (*PFBO_FBOTex)    (GLenum, GLenum, GLenum, GLuint, GLint);
typedef void   (*PFBO_FBORBO)    (GLenum, GLenum, GLenum, GLuint);
typedef GLenum (*PFBO_CheckFBO)  (GLenum);
typedef void   (*PFBO_GenRBO)    (GLsizei, GLuint*);
typedef void   (*PFBO_BindRBO)   (GLenum,  GLuint);
typedef void   (*PFBO_DelRBO)    (GLsizei, const GLuint*);
typedef void   (*PFBO_RBOStore)  (GLenum, GLenum, GLsizei, GLsizei);

static PFBO_GenFBO   s_genFBO   = nullptr;
static PFBO_BindFBO  s_bindFBO  = nullptr;
static PFBO_DelFBO   s_delFBO   = nullptr;
static PFBO_FBOTex   s_fboTex   = nullptr;
static PFBO_FBORBO   s_fboRBO   = nullptr;
static PFBO_CheckFBO s_checkFBO = nullptr;
static PFBO_GenRBO   s_genRBO   = nullptr;
static PFBO_BindRBO  s_bindRBO  = nullptr;
static PFBO_DelRBO   s_delRBO   = nullptr;
static PFBO_RBOStore s_rboStore = nullptr;

static void loadFBOFunctions()
{
    s_genFBO   = (PFBO_GenFBO)  SDL_GL_GetProcAddress("glGenFramebuffers");
    s_bindFBO  = (PFBO_BindFBO) SDL_GL_GetProcAddress("glBindFramebuffer");
    s_delFBO   = (PFBO_DelFBO)  SDL_GL_GetProcAddress("glDeleteFramebuffers");
    s_fboTex   = (PFBO_FBOTex)  SDL_GL_GetProcAddress("glFramebufferTexture2D");
    s_fboRBO   = (PFBO_FBORBO)  SDL_GL_GetProcAddress("glFramebufferRenderbuffer");
    s_checkFBO = (PFBO_CheckFBO)SDL_GL_GetProcAddress("glCheckFramebufferStatus");
    s_genRBO   = (PFBO_GenRBO)  SDL_GL_GetProcAddress("glGenRenderbuffers");
    s_bindRBO  = (PFBO_BindRBO) SDL_GL_GetProcAddress("glBindRenderbuffer");
    s_delRBO   = (PFBO_DelRBO)  SDL_GL_GetProcAddress("glDeleteRenderbuffers");
    s_rboStore = (PFBO_RBOStore)SDL_GL_GetProcAddress("glRenderbufferStorage");

    if (!s_genFBO || !s_bindFBO)
        SDL_Log("[Viewport] WARN: FBO functions non caricate.");
    else
        SDL_Log("[Viewport] FBO functions OK.");
}

// ── Altri include ─────────────────────────────────────────────────────────
#include <mini/render/Camera.hpp>
#include <mini/render/Shader.hpp>

#include <imgui.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <vector>
#include <iostream>

namespace editor
{

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
    loadFBOFunctions();

    m_camera = std::make_unique<mini::Camera>(60.0f, 16.0f/9.0f, 0.1f, 500.0f);
    m_camera->setPosition({0.0f, 12.0f, 20.0f});
    m_camera->lookAt({0.0f, 0.0f, 0.0f});
    m_camera->setSpeed(m_camSpeed);

    m_shader = std::make_unique<mini::Shader>(k_vert, k_frag);
    buildGrid(40.0f, 20);
    resizeFBO(4, 4);
}

FreeCameraViewport::~FreeCameraViewport()
{
    if (m_fbo && s_delFBO)      s_delFBO(1, &m_fbo);
    if (m_depthRbo && s_delRBO) s_delRBO(1, &m_depthRbo);
    if (m_colorTex) glDeleteTextures(1, &m_colorTex);
    if (m_gridVAO)  glDeleteVertexArrays(1, &m_gridVAO);
    if (m_gridVBO)  glDeleteBuffers(1, &m_gridVBO);
}

void FreeCameraViewport::resizeFBO(int w, int h)
{
    if (w == m_fbWidth && h == m_fbHeight) return;
    if (!s_genFBO) return;
    m_fbWidth = w; m_fbHeight = h;

    if (m_fbo)      { s_delFBO(1, &m_fbo);      m_fbo      = 0; }
    if (m_depthRbo) { s_delRBO(1, &m_depthRbo);  m_depthRbo = 0; }
    if (m_colorTex) { glDeleteTextures(1, &m_colorTex); m_colorTex = 0; }

    s_genFBO(1, &m_fbo);
    s_bindFBO(GL_FRAMEBUFFER, m_fbo);

    glGenTextures(1, &m_colorTex);
    glBindTexture(GL_TEXTURE_2D, m_colorTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    s_fboTex(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_colorTex, 0);

    s_genRBO(1, &m_depthRbo);
    s_bindRBO(GL_RENDERBUFFER, m_depthRbo);
    s_rboStore(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, w, h);
    s_fboRBO(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, m_depthRbo);

    if (s_checkFBO(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        std::cerr << "[Viewport] FBO incompleto!\n";

    s_bindFBO(GL_FRAMEBUFFER, 0);
    glBindTexture(GL_TEXTURE_2D, 0);
}

void FreeCameraViewport::buildGrid(float size, int div)
{
    std::vector<float> verts;
    float step = size / div, half = size * 0.5f;
    auto line = [&](float x0,float y0,float z0, float x1,float y1,float z1, float r,float g,float b)
    { verts.insert(verts.end(), {x0,y0,z0,r,g,b, x1,y1,z1,r,g,b}); };

    for (int i = 0; i <= div; ++i) {
        float t = -half + i * step;
        float c = (i == div/2) ? 0.55f : 0.25f;
        line(-half, 0, t, half, 0, t, c,c,c);
        line(t, 0, -half, t, 0, half, c,c,c);
    }
    line(-half,0.01f,0, half,0.01f,0, 0.85f,0.25f,0.25f);
    line(0,0.01f,-half, 0,0.01f,half, 0.25f,0.45f,0.90f);
    m_gridVertCount = (int)(verts.size() / 6);

    glGenVertexArrays(1, &m_gridVAO); glGenBuffers(1, &m_gridVBO);
    glBindVertexArray(m_gridVAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_gridVBO);
    glBufferData(GL_ARRAY_BUFFER, verts.size()*sizeof(float), verts.data(), GL_STATIC_DRAW);
    glEnableVertexAttribArray(0); glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,6*sizeof(float),(void*)0);
    glEnableVertexAttribArray(1); glVertexAttribPointer(1,3,GL_FLOAT,GL_FALSE,6*sizeof(float),(void*)(3*sizeof(float)));
    glBindVertexArray(0);
}

void FreeCameraViewport::drawGrid(const glm::mat4& vp)
{
    if (!m_shader || !m_gridVAO) return;
    m_shader->use();
    glUniformMatrix4fv(glGetUniformLocation(m_shader->getId(),"uVP"),1,GL_FALSE,glm::value_ptr(vp));
    glBindVertexArray(m_gridVAO);
    glDrawArrays(GL_LINES, 0, m_gridVertCount);
    glBindVertexArray(0);
    glUseProgram(0);
}

void FreeCameraViewport::renderScene()
{
    if (!m_fbo || !s_bindFBO) return;
    s_bindFBO(GL_FRAMEBUFFER, m_fbo);
    glViewport(0, 0, m_fbWidth, m_fbHeight);
    glEnable(GL_DEPTH_TEST);
    glClearColor(0.10f, 0.12f, 0.18f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    drawGrid(m_camera->getViewProjection());
    glDisable(GL_DEPTH_TEST);
    s_bindFBO(GL_FRAMEBUFFER, 0);
}

void FreeCameraViewport::tick(float dt)
{
    if (!m_focused) return;
    const Uint8* ks = SDL_GetKeyboardState(nullptr);
    m_camera->setSpeed(ks[SDL_SCANCODE_LSHIFT] ? m_camSpeed*3.0f : m_camSpeed);
    m_camera->processKeyboard(
        ks[SDL_SCANCODE_W], ks[SDL_SCANCODE_S],
        ks[SDL_SCANCODE_A], ks[SDL_SCANCODE_D],
        ks[SDL_SCANCODE_E] || ks[SDL_SCANCODE_SPACE],
        ks[SDL_SCANCODE_Q] || ks[SDL_SCANCODE_LCTRL], dt);
    if (m_mouseCapture) {
        int dx=0, dy=0;
        SDL_GetRelativeMouseState(&dx, &dy);
        if (dx||dy) m_camera->processMouse((float)dx,(float)dy,0.15f);
    }
}

void FreeCameraViewport::draw()
{
    const ImVec2 avail = ImGui::GetContentRegionAvail();
    const int w = (int)avail.x;
    const int h = (int)(avail.y - 30);

    ImGui::TextDisabled("WASD = muovi  |  E/Q = su/giu  |  Shift = veloce");
    ImGui::SameLine(avail.x - 150.0f);
    if (ImGui::Button(m_mouseCapture ? "Rilascia mouse" : "Cattura mouse")) {
        m_mouseCapture = !m_mouseCapture;
        SDL_SetRelativeMouseMode(m_mouseCapture ? SDL_TRUE : SDL_FALSE);
    }

    m_focused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);

    if (w > 8 && h > 8) {
        m_camera->setAspect((float)w / (float)h);
        resizeFBO(w, h);
        renderScene();
        // Reset GL state per ImGui
        glUseProgram(0);
        glBindVertexArray(0);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glDisableVertexAttribArray(0);
        glDisableVertexAttribArray(1);
        if (m_colorTex)
            ImGui::Image((ImTextureID)(uintptr_t)m_colorTex,
                         ImVec2((float)w,(float)h),
                         ImVec2(0.0f,1.0f), ImVec2(1.0f,0.0f));
        else
            ImGui::TextColored(ImVec4(1,0.4f,0.4f,1), "FBO non inizializzato.");
    }
}

} // namespace editor