// FreeCameraViewport.cpp
// Renderizza una scena 3D (griglia + modello + hitbox + bones + markers + gizmo)
// in un FBO mostrato via ImGui::Image.
//
// IMPORTANTE: usa CLIENT-SIDE ARRAYS senza VAO/VBO, identico a mini::Mesh::draw().

#include "viewport/FreeCameraViewport.hpp"
#include "util/FileDialog.hpp"
#include "mini/platform/OpenGL.hpp"
#include "mini/render/Shader.hpp"
#include "mini/render/Camera.hpp"
#include "mini/render/Model.hpp"
#include "mini/render/Mesh.hpp"

#include <imgui.h>
#include <SDL2/SDL.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <vector>
#include <string>
#include <cstdio>
#include <cmath>
#include <algorithm>

// ── Costanti FBO ──────────────────────────────────────────────────────────────
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
#ifndef GL_FRAMEBUFFER_BINDING
  #define GL_FRAMEBUFFER_BINDING      0x8CA6
#endif

typedef void   (*FN_GenFBO)   (GLsizei, GLuint*);
typedef void   (*FN_BindFBO)  (GLenum,  GLuint);
typedef void   (*FN_DelFBO)   (GLsizei, const GLuint*);
typedef void   (*FN_FBOTex)   (GLenum,  GLenum, GLenum, GLuint, GLint);
typedef void   (*FN_FBORBO)   (GLenum,  GLenum, GLenum, GLuint);
typedef GLenum (*FN_ChkFBO)   (GLenum);
typedef void   (*FN_GenRBO)   (GLsizei, GLuint*);
typedef void   (*FN_BindRBO)  (GLenum,  GLuint);
typedef void   (*FN_DelRBO)   (GLsizei, const GLuint*);
typedef void   (*FN_RBOSt)    (GLenum,  GLenum, GLsizei, GLsizei);

static FN_GenFBO  s_genFBO  = nullptr;
static FN_BindFBO s_bindFBO = nullptr;
static FN_DelFBO  s_delFBO  = nullptr;
static FN_FBOTex  s_fboTex  = nullptr;
static FN_FBORBO  s_fboRBO  = nullptr;
static FN_ChkFBO  s_chkFBO  = nullptr;
static FN_GenRBO  s_genRBO  = nullptr;
static FN_BindRBO s_bindRBO = nullptr;
static FN_DelRBO  s_delRBO  = nullptr;
static FN_RBOSt   s_rboSt   = nullptr;
static bool       s_fboLoaded = false;

typedef void (*FN_BindVAO)(GLuint);
static FN_BindVAO s_bindVAO = nullptr;

static void loadFBOFuncs()
{
    if (s_fboLoaded) return;
    s_fboLoaded = true;
    s_genFBO  = (FN_GenFBO)  SDL_GL_GetProcAddress("glGenFramebuffers");
    s_bindFBO = (FN_BindFBO) SDL_GL_GetProcAddress("glBindFramebuffer");
    s_delFBO  = (FN_DelFBO)  SDL_GL_GetProcAddress("glDeleteFramebuffers");
    s_fboTex  = (FN_FBOTex)  SDL_GL_GetProcAddress("glFramebufferTexture2D");
    s_fboRBO  = (FN_FBORBO)  SDL_GL_GetProcAddress("glFramebufferRenderbuffer");
    s_chkFBO  = (FN_ChkFBO)  SDL_GL_GetProcAddress("glCheckFramebufferStatus");
    s_genRBO  = (FN_GenRBO)  SDL_GL_GetProcAddress("glGenRenderbuffers");
    s_bindRBO = (FN_BindRBO) SDL_GL_GetProcAddress("glBindRenderbuffer");
    s_delRBO  = (FN_DelRBO)  SDL_GL_GetProcAddress("glDeleteRenderbuffers");
    s_rboSt   = (FN_RBOSt)   SDL_GL_GetProcAddress("glRenderbufferStorage");
    s_bindVAO = (FN_BindVAO) SDL_GL_GetProcAddress("glBindVertexArray");
}

// ── Shader sorgenti ───────────────────────────────────────────────────────────
static const char* k_vert = R"(
#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aCol;
uniform mat4 uVP;
out vec3 vCol;
void main() { gl_Position = uVP * vec4(aPos, 1.0); vCol = aCol; }
)";

static const char* k_frag = R"(
#version 330 core
in vec3 vCol;
out vec4 FragColor;
void main() { FragColor = vec4(vCol, 1.0); }
)";

namespace editor
{

// ── Costruttore / Distruttore ─────────────────────────────────────────────────
FreeCameraViewport::FreeCameraViewport()
{
    loadFBOFuncs();

    m_shader = std::make_unique<mini::Shader>(k_vert, k_frag);
    if (!m_shader->isValid())
    {
        m_lastError = "Shader non compilato (vedi log SDL).";
        SDL_Log("[Viewport] Shader FALLITO");
    }
    else
    {
        SDL_Log("[Viewport] Shader OK (id=%u)", m_shader->getId());
    }

    m_camera = std::make_unique<mini::Camera>(60.0f, 16.0f/9.0f, 0.1f, 500.0f);
    m_camera->setPosition({0.0f, 8.0f, 14.0f});
    m_camera->lookAt({0.0f, 0.0f, 0.0f});
    m_camera->setSpeed(m_camSpeed);

    buildGrid(120.0f, 60);   // 120 m, passo 2 m: copre anche mappe grandi (2026-07-21)
    SDL_Log("[Viewport] Grid vertici=%d", m_gridVertCount);

    resizeFBO(4, 4);
}

FreeCameraViewport::~FreeCameraViewport()
{
    // Difensivo: se l'app si chiude (o il modulo viene distrutto) mentre la
    // cattura è attiva, il cursore resterebbe nascosto a livello di sistema.
    releaseMouseCapture();
    if (s_delFBO && m_fbo)      s_delFBO(1, &m_fbo);
    if (s_delRBO && m_depthRbo) s_delRBO(1, &m_depthRbo);
    if (m_colorTex) glDeleteTextures(1, &m_colorTex);
}

void FreeCameraViewport::releaseMouseCapture()
{
    if (!m_mouseCapture) return;
    m_mouseCapture = false;
    SDL_SetRelativeMouseMode(SDL_FALSE);
}

bool FreeCameraViewport::isReady() const
{
    return m_shader && m_shader->isValid() && m_fboOk;
}

// ── Grid ─────────────────────────────────────────────────────────────────────
void FreeCameraViewport::buildGrid(float size, int div)
{
    m_gridData.clear();
    float step = size / div, half = size * 0.5f;

    auto ln = [&](float x0,float y0,float z0,
                  float x1,float y1,float z1,
                  float r, float g, float b)
    {
        m_gridData.insert(m_gridData.end(),
            {x0,y0,z0, r,g,b, x1,y1,z1, r,g,b});
    };

    for (int i = 0; i <= div; ++i)
    {
        float t = -half + i * step;
        float c = (i == div/2) ? 0.55f : 0.30f;
        ln(-half, 0, t,  half, 0, t,  c, c, c);
        ln(t, 0, -half,  t, 0, half, c, c, c);
    }
    ln(-half, 0.02f, 0, half, 0.02f, 0,  0.95f, 0.25f, 0.25f);
    ln(0, 0.02f,-half, 0, 0.02f, half,  0.25f, 0.55f, 1.00f);

    m_gridVertCount = (int)(m_gridData.size() / 6);
}

// ── Draw generico (client-side arrays) ────────────────────────────────────────
void FreeCameraViewport::drawArray(const std::vector<float>& data, int count,
                                   unsigned int glMode, const glm::mat4& vp)
{
    if (count <= 0 || data.empty() || !m_shader || !m_shader->isValid()) return;

    m_shader->use();
    m_shader->setMat4("uVP", glm::value_ptr(vp));

    constexpr GLsizei stride = (GLsizei)(6 * sizeof(float));
    const float* base = data.data();

    glEnableVertexAttribArray(0u);
    glEnableVertexAttribArray(1u);
    glVertexAttribPointer(0u, 3, GL_FLOAT, GL_FALSE, stride, (const void*)(base + 0));
    glVertexAttribPointer(1u, 3, GL_FLOAT, GL_FALSE, stride, (const void*)(base + 3));

    glDrawArrays(glMode, 0, count);

    glDisableVertexAttribArray(1u);
    glDisableVertexAttribArray(0u);
}

// ── FBO ──────────────────────────────────────────────────────────────────────
void FreeCameraViewport::resizeFBO(int w, int h)
{
    // Stessa dimensione E l'FBO è valido → niente da fare.
    // Il `&& m_fboOk` è la RIPARAZIONE (2026-07-17): senza, un FBO invalidato
    // mentre il pannello ha dimensione stabile non veniva mai più ricostruito.
    // Un realloc fallito (intoppo del driver, primi frame, minimize/restore che
    // perde le risorse GL) lasciava `m_fboOk=false`; al frame dopo, stessa
    // dimensione → si usciva subito, e il viewport restava rotto fino a un
    // resize o al riavvio dell'editor. Ora ritenta finché non torna valido —
    // si auto-ripara al frame successivo invece di richiedere un restart.
    if (w == m_fbWidth && h == m_fbHeight && m_fboOk) return;

    // KI #17 (leak memoria editor): l'area disponibile del pannello può
    // OSCILLARE di pochi pixel tra frame (scrollbar, separatori) — prima
    // ricreava FBO+texture+RBO a ogni oscillazione (churn GL → memoria in
    // crescita continua). Ora la texture è allocata a multipli di 64 e viene
    // SOLO ingrandita; il pannello mostra la sub-regione via UV.
    m_fbWidth = w; m_fbHeight = h;
    if (w <= m_texWidth && h <= m_texHeight && m_fboOk)
        return;   // la texture allocata basta già: nessun realloc

    if (!s_genFBO || !s_bindFBO || !s_fboTex || !s_genRBO ||
        !s_bindRBO || !s_rboSt  || !s_fboRBO || !s_chkFBO)
    {
        m_lastError = "FBO non disponibile.";
        return;
    }

    const int aw = ((w + 63) / 64) * 64;
    const int ah = ((h + 63) / 64) * 64;
    // Log solo su un VERO cambio di dimensione allocata. Un ritenta-a-parità
    // (FBO rotto ma pannello stabile, vedi guardia sopra) non deve spammare il
    // log a ogni frame né far credere a un churn di crescita (KI #17).
    const bool sizeChanged = (aw != m_texWidth || ah != m_texHeight);
    m_texWidth = aw; m_texHeight = ah;
    if (sizeChanged)
        std::printf("[Viewport] Realloc FBO %dx%d (richiesti %dx%d)\n", aw, ah, w, h);
    else
        std::printf("[Viewport] Ricostruzione FBO invalidato %dx%d\n", aw, ah);
    m_fboOk = false;

    if (m_fbo)      { s_delFBO(1, &m_fbo);      m_fbo      = 0; }
    if (m_depthRbo) { s_delRBO(1, &m_depthRbo); m_depthRbo = 0; }
    if (m_colorTex) { glDeleteTextures(1, &m_colorTex); m_colorTex = 0; }

    s_genFBO(1, &m_fbo);
    s_bindFBO(GL_FRAMEBUFFER, m_fbo);

    glGenTextures(1, &m_colorTex);
    glBindTexture(GL_TEXTURE_2D, m_colorTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, m_texWidth, m_texHeight, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glBindTexture(GL_TEXTURE_2D, 0);
    s_fboTex(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_colorTex, 0);

    s_genRBO(1, &m_depthRbo);
    s_bindRBO(GL_RENDERBUFFER, m_depthRbo);
    s_rboSt(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, m_texWidth, m_texHeight);
    s_bindRBO(GL_RENDERBUFFER, 0);
    s_fboRBO(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT,
             GL_RENDERBUFFER, m_depthRbo);

    GLenum status = s_chkFBO(GL_FRAMEBUFFER);
    if (status == GL_FRAMEBUFFER_COMPLETE)
    {
        m_fboOk = true;
        m_lastError.clear();
    }
    else
    {
        m_lastError = "FBO incompleto (status=" + std::to_string(status) + ").";
    }

    s_bindFBO(GL_FRAMEBUFFER, 0);
}

// ── Render scene nell'FBO ─────────────────────────────────────────────────────
void FreeCameraViewport::renderScene()
{
    if (!m_fboOk || !s_bindFBO) return;

    GLint oldFBO = 0;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &oldFBO);
    GLint oldVP[4];
    glGetIntegerv(GL_VIEWPORT, oldVP);
    GLboolean blendOn = glIsEnabled(GL_BLEND);
    GLboolean depthOn = glIsEnabled(GL_DEPTH_TEST);

    if (s_bindVAO) s_bindVAO(0);

    s_bindFBO(GL_FRAMEBUFFER, m_fbo);
    glViewport(0, 0, m_fbWidth, m_fbHeight);
    glEnable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
    glClearColor(0.10f, 0.12f, 0.17f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    const glm::mat4 vp = m_camera->getViewProjection();

    drawArray(m_gridData, m_gridVertCount, GL_LINES, vp);

    if (m_modelVertCount > 0)
        drawArray(m_modelData, m_modelVertCount, GL_TRIANGLES, vp);

    if (m_attachVertCount > 0)
        drawArray(m_attachData, m_attachVertCount, GL_TRIANGLES, vp);

    // Facce piene PRIMA del wireframe: polygon offset spinge le facce leggermente
    // "indietro" così gli spigoli restano nitidi sopra (niente z-fighting). Opaco,
    // nessun blending → compat Intel intatta (ADR-003).
    if (m_showSolid && m_mapBoxFillVertCount > 0)
    {
        glEnable(GL_POLYGON_OFFSET_FILL);
        glPolygonOffset(1.0f, 1.0f);
        drawArray(m_mapBoxFillData, m_mapBoxFillVertCount, GL_TRIANGLES, vp);
        glPolygonOffset(0.0f, 0.0f);
        glDisable(GL_POLYGON_OFFSET_FILL);
    }

    glDepthFunc(GL_LEQUAL);
    drawArray(m_mapBoxData, m_mapBoxVertCount, GL_LINES, vp);

    // Bone lines (rispettano la profondità)
    if (m_boneLineCount > 0)
        drawArray(m_boneLineData, m_boneLineCount, GL_LINES, vp);

    // Elementi di editing SEMPRE visibili (anche attraverso i modelli):
    // hitbox, marker attach point, bone dots, foot marker.
    glDepthFunc(GL_ALWAYS);
    drawArray(m_boxData, m_boxVertCount, GL_LINES, vp);
    if (m_showFootMarker && !m_footMarkerData.empty())
        drawArray(m_footMarkerData, (int)(m_footMarkerData.size() / 6), GL_LINES, vp);
    if (m_boneDotCount > 0)
        drawArray(m_boneDotData, m_boneDotCount, GL_LINES, vp);
    if (m_markerCount > 0)
        drawArray(m_markerData, m_markerCount, GL_LINES, vp);

    glDepthFunc(GL_LESS);

    s_bindFBO(GL_FRAMEBUFFER, (GLuint)oldFBO);
    glViewport(oldVP[0], oldVP[1], oldVP[2], oldVP[3]);
    if (!depthOn) glDisable(GL_DEPTH_TEST);
    if (blendOn)  glEnable(GL_BLEND);
}

// ── Foot marker ───────────────────────────────────────────────────────────────
void FreeCameraViewport::setFootMarker(float y, bool show)
{
    m_showFootMarker = show;
    if (show && y != m_footMarkerY)
    {
        m_footMarkerY = y;
        buildFootMarker(y);
    }
    else if (!show)
    {
        m_footMarkerData.clear();
    }
}

void FreeCameraViewport::buildFootMarker(float y)
{
    m_footMarkerData.clear();
    constexpr int   SEGS = 32;
    constexpr float R    = 0.18f;
    constexpr float PI2  = 6.28318530f;
    constexpr float cr = 0.20f, cg = 1.00f, cb = 0.40f;

    for (int i = 0; i < SEGS; ++i)
    {
        float a0 = PI2 * i       / SEGS;
        float a1 = PI2 * (i + 1) / SEGS;
        float x0 = R * std::cos(a0), z0 = R * std::sin(a0);
        float x1 = R * std::cos(a1), z1 = R * std::sin(a1);
        m_footMarkerData.insert(m_footMarkerData.end(),
            {x0, y, z0, cr, cg, cb,
             x1, y, z1, cr, cg, cb});
    }
    float cross = R * 0.35f;
    m_footMarkerData.insert(m_footMarkerData.end(), {
        -cross, y, 0,  cr,cg,cb,  cross, y, 0,  cr,cg,cb,
        0, y, -cross,  cr,cg,cb,  0, y,  cross, cr,cg,cb,
    });
}

// ── Bones ─────────────────────────────────────────────────────────────────────
void FreeCameraViewport::setBoneData(const std::vector<JointData>& joints,
                                     float meshRotX, float meshScale)
{
    m_joints     = joints;
    m_jointRotX  = meshRotX;
    m_jointScale = meshScale;
    buildBoneData();
}

void FreeCameraViewport::clearBones()
{
    m_joints.clear();
    m_boneLineData.clear(); m_boneLineCount = 0;
    m_boneDotData.clear();  m_boneDotCount  = 0;
}

void FreeCameraViewport::setSelectedBone(const std::string& name)
{
    m_selBone = name;
    buildBoneData(); // rebuild to update colors
}

void FreeCameraViewport::buildBoneData()
{
    m_boneLineData.clear();
    m_boneDotData.clear();

    if (m_joints.empty()) { m_boneLineCount = 0; m_boneDotCount = 0; return; }

    glm::mat4 M = glm::rotate(glm::mat4(1.0f), glm::radians(m_jointRotX), {1,0,0})
                * glm::scale(glm::mat4(1.0f), {m_jointScale, m_jointScale, m_jointScale});

    auto addLine = [&](std::vector<float>& buf,
                       glm::vec3 a, glm::vec3 b,
                       float r, float g, float bv)
    {
        buf.insert(buf.end(), {a.x,a.y,a.z, r,g,bv,
                               b.x,b.y,b.z, r,g,bv});
    };

    constexpr float CR = 0.9f, CG = 0.8f, CB = 0.1f; // bone yellow
    constexpr float dot = 0.03f; // cross arm length

    for (int ji = 0; ji < (int)m_joints.size(); ++ji)
    {
        const auto& jd = m_joints[ji];
        bool sel = (jd.name == m_selBone);
        float r = sel ? 1.0f : CR;
        float g = sel ? 1.0f : CG;
        float b = sel ? 1.0f : CB;

        glm::vec3 wp = glm::vec3(M * glm::vec4(jd.modelPos, 1.0f));

        // Bone line to parent
        if (jd.parentIdx >= 0 && jd.parentIdx < (int)m_joints.size())
        {
            glm::vec3 pp = glm::vec3(M * glm::vec4(m_joints[jd.parentIdx].modelPos, 1.0f));
            addLine(m_boneLineData, pp, wp, r, g, b);
        }

        // Dot cross (3 axes)
        addLine(m_boneDotData, wp - glm::vec3(dot,0,0), wp + glm::vec3(dot,0,0), r, g, b);
        addLine(m_boneDotData, wp - glm::vec3(0,dot,0), wp + glm::vec3(0,dot,0), r, g, b);
        addLine(m_boneDotData, wp - glm::vec3(0,0,dot), wp + glm::vec3(0,0,dot), r, g, b);
    }

    m_boneLineCount = (int)(m_boneLineData.size() / 6);
    m_boneDotCount  = (int)(m_boneDotData.size()  / 6);
}

// ── Markers ──────────────────────────────────────────────────────────────────
void FreeCameraViewport::setMarkers(const std::vector<ViewportMarker>& markers)
{
    m_markers = markers;
    buildMarkerData();
}

void FreeCameraViewport::clearMarkers()
{
    m_markers.clear();
    m_markerData.clear();
    m_markerCount = 0;
}

void FreeCameraViewport::buildMarkerData()
{
    m_markerData.clear();

    auto addLine = [&](glm::vec3 a, glm::vec3 b, float r, float g, float bv) {
        m_markerData.insert(m_markerData.end(),
            {a.x,a.y,a.z, r,g,bv,  b.x,b.y,b.z, r,g,bv});
    };

    for (const auto& mk : m_markers)
    {
        float r = mk.r, g = mk.g, b = mk.b;
        const float arm = mk.selected ? 0.14f : 0.10f;
        const float h   = mk.selected ? 0.10f : 0.07f; // mezzo lato del cubetto
        if (mk.selected) { r = (r+0.3f<1.0f)?r+0.3f:1.0f; g=(g+0.3f<1.0f)?g+0.3f:1.0f; b=(b+0.3f<1.0f)?b+0.3f:1.0f; }
        glm::vec3 p = mk.pos;

        // Croce a 3 assi
        addLine({p.x-arm,p.y,p.z}, {p.x+arm,p.y,p.z}, r,g,b);
        addLine({p.x,p.y-arm,p.z}, {p.x,p.y+arm,p.z}, r,g,b);
        addLine({p.x,p.y,p.z-arm}, {p.x,p.y,p.z+arm}, r,g,b);

        // Cubetto wireframe attorno al punto → si legge come "oggetto"
        glm::vec3 c[8] = {
            {p.x-h,p.y-h,p.z-h},{p.x+h,p.y-h,p.z-h},{p.x+h,p.y+h,p.z-h},{p.x-h,p.y+h,p.z-h},
            {p.x-h,p.y-h,p.z+h},{p.x+h,p.y-h,p.z+h},{p.x+h,p.y+h,p.z+h},{p.x-h,p.y+h,p.z+h},
        };
        const int e[12][2] = {{0,1},{1,2},{2,3},{3,0},{4,5},{5,6},{6,7},{7,4},{0,4},{1,5},{2,6},{3,7}};
        for (auto& ed : e) addLine(c[ed[0]], c[ed[1]], r,g,b);
    }
    m_markerCount = (int)(m_markerData.size() / 6);
}

// ── Gizmo ─────────────────────────────────────────────────────────────────────
void FreeCameraViewport::setGizmoTarget(glm::vec3 pos, bool enabled)
{
    m_gizmoPos     = pos;
    m_gizmoEnabled = enabled;
}

bool FreeCameraViewport::popGizmoDelta(glm::vec3& outDelta)
{
    if (!m_gizmoDragged) return false;
    outDelta      = m_gizmoDelta;
    m_gizmoDelta  = {0,0,0};
    m_gizmoDragged = false;
    return true;
}

bool FreeCameraViewport::popGizmoRotDelta(glm::vec3& outEulerDeg)
{
    if (!m_gizmoRotDragged) return false;
    outEulerDeg      = m_gizmoRotDelta;
    m_gizmoRotDelta  = {0,0,0};
    m_gizmoRotDragged = false;
    return true;
}

bool FreeCameraViewport::popGizmoScaleDelta(glm::vec3& outDelta)
{
    if (!m_gizmoScaleDragged) return false;
    outDelta           = m_gizmoScaleDelta;
    m_gizmoScaleDelta  = {0,0,0};
    m_gizmoScaleDragged = false;
    return true;
}

// ── Pan ───────────────────────────────────────────────────────────────────────
void FreeCameraViewport::panCamera(float rightDelta, float upDelta)
{
    glm::vec3 fwd   = m_camera->getForward();
    glm::vec3 right = glm::normalize(glm::cross(fwd, {0,1,0}));
    glm::vec3 pos   = m_camera->getPosition();
    pos += right * rightDelta + glm::vec3(0,1,0) * upDelta;
    m_camera->setPosition(pos);
}

glm::vec3 FreeCameraViewport::groundFocusPoint(float fallbackDist) const
{
    const glm::vec3 p = m_camera->getPosition();
    const glm::vec3 f = m_camera->getForward();
    // Camera che guarda verso il basso: interseca il piano del suolo (y=0).
    if (f.y < -0.05f)
    {
        const float t = -p.y / f.y;
        if (t > 0.5f && t < 300.0f)
            return {p.x + f.x * t, 0.0f, p.z + f.z * t};
    }
    // Ripiego: davanti alla camera, sul piano orizzontale, a distanza fissa.
    glm::vec3 fh = {f.x, 0.0f, f.z};
    const float len = std::sqrt(fh.x * fh.x + fh.z * fh.z);
    if (len > 1e-4f) { fh.x /= len; fh.z /= len; }
    else             { fh = {0.0f, 0.0f, -1.0f}; }
    return {p.x + fh.x * fallbackDist, 0.0f, p.z + fh.z * fallbackDist};
}

// ── Click selection ───────────────────────────────────────────────────────────
std::string FreeCameraViewport::popClickedItem()
{
    std::string s = m_lastClickedItem;
    m_lastClickedItem.clear();
    return s;
}

// ── handleViewportClick ───────────────────────────────────────────────────────
void FreeCameraViewport::handleViewportClick()
{
    if (!m_imgClicked) return;
    m_imgClicked = false;
    // Click che ha afferrato il gizmo: è un trascinamento, non una selezione.
    if (m_gizmoActiveAxis >= 0) return;

    glm::mat4 vp = m_camera->getViewProjection();
    float cx = m_imgClickPos.x;
    float cy = m_imgClickPos.y;

    auto projectToScreen = [&](glm::vec3 worldPos) -> ImVec2 {
        glm::vec4 clip = vp * glm::vec4(worldPos, 1.0f);
        if (clip.w <= 0.0f) return {-9999, -9999};
        float nx = clip.x / clip.w;
        float ny = clip.y / clip.w;
        float sx = m_imgMin.x + (nx * 0.5f + 0.5f) * m_imgSize.x;
        float sy = m_imgMin.y + (1.0f - (ny * 0.5f + 0.5f)) * m_imgSize.y;
        return {sx, sy};
    };

    std::string bestName;
    float bestDist = 15.0f; // threshold in pixels

    glm::mat4 M = glm::rotate(glm::mat4(1.0f), glm::radians(m_jointRotX), {1,0,0})
                * glm::scale(glm::mat4(1.0f), {m_jointScale, m_jointScale, m_jointScale});

    for (const auto& jd : m_joints)
    {
        glm::vec3 wp = glm::vec3(M * glm::vec4(jd.modelPos, 1.0f));
        ImVec2 sp = projectToScreen(wp);
        float dx = sp.x - cx, dy = sp.y - cy;
        float d = std::sqrt(dx*dx + dy*dy);
        if (d < bestDist) { bestDist = d; bestName = jd.name; }
    }

    for (const auto& mk : m_markers)
    {
        ImVec2 sp = projectToScreen(mk.pos);
        float dx = sp.x - cx, dy = sp.y - cy;
        float d = std::sqrt(dx*dx + dy*dy);
        if (d < bestDist) { bestDist = d; bestName = mk.name; }
    }

    if (!bestName.empty())
    {
        // Check if it's a bone
        for (const auto& jd : m_joints)
            if (jd.name == bestName) { m_selBone = bestName; break; }
        m_lastClickedItem = bestName;
    }

    // ── Map box: ray-picking (selezione dal viewport) ──────────────────
    // Solo se nessun marker/bone è stato colto: quelli sono punti specifici e
    // hanno la precedenza. I box sono volumi → si testa un raggio dal pixel
    // cliccato contro ogni OBB e si prende il più vicino alla camera.
    if (bestName.empty() && !m_mapBoxes.empty())
    {
        const float ndcX = ((cx - m_imgMin.x) / m_imgSize.x) * 2.0f - 1.0f;
        const float ndcY = 1.0f - ((cy - m_imgMin.y) / m_imgSize.y) * 2.0f;
        const glm::mat4 invVP = glm::inverse(vp);
        glm::vec4 pNear = invVP * glm::vec4(ndcX, ndcY, -1.0f, 1.0f);
        glm::vec4 pFar  = invVP * glm::vec4(ndcX, ndcY,  1.0f, 1.0f);
        if (std::abs(pNear.w) > 1e-8f && std::abs(pFar.w) > 1e-8f)
        {
            const glm::vec3 o = glm::vec3(pNear) / pNear.w;
            const glm::vec3 f = glm::vec3(pFar)  / pFar.w;
            const glm::vec3 dir = glm::normalize(f - o);

            int   bestId = MapBoxDraw::kNoPick;
            float bestT  = 1e30f;
            for (const auto& b : m_mapBoxes)
            {
                if (b.pickId == MapBoxDraw::kNoPick) continue;
                // Origine+direzione nello spazio locale del box (undo traslazione
                // + rotazione Y). In setMapBoxes il corner è
                //   wx = cosR*lx + sinR*lz + bx ; wz = -sinR*lx + cosR*lz + bz
                // → inversa (R^T): lx = cosR*dx - sinR*dz ; lz = sinR*dx + cosR*dz
                const float cosR = std::cos(glm::radians(b.ry));
                const float sinR = std::sin(glm::radians(b.ry));
                const glm::vec3 lo = {
                    cosR*(o.x-b.x) - sinR*(o.z-b.z), o.y - b.y,
                    sinR*(o.x-b.x) + cosR*(o.z-b.z) };
                const glm::vec3 ld = {
                    cosR*dir.x - sinR*dir.z, dir.y, sinR*dir.x + cosR*dir.z };
                const glm::vec3 h = {b.sx*0.5f, b.sy*0.5f, b.sz*0.5f};

                float tmin = -1e30f, tmax = 1e30f; bool hit = true;
                for (int a = 0; a < 3; ++a)
                {
                    if (std::abs(ld[a]) < 1e-8f)
                    { if (lo[a] < -h[a] || lo[a] > h[a]) { hit = false; break; } }
                    else
                    {
                        const float inv = 1.0f / ld[a];
                        float t1 = (-h[a]-lo[a])*inv, t2 = (h[a]-lo[a])*inv;
                        if (t1 > t2) { const float tmp = t1; t1 = t2; t2 = tmp; }
                        // NB: confronti manuali, non std::max/min — <windows.h>
                        // (via editor) definisce le macro min/max e romperebbe.
                        if (t1 > tmin) tmin = t1;
                        if (t2 < tmax) tmax = t2;
                        if (tmin > tmax) { hit = false; break; }
                    }
                }
                if (!hit) continue;
                const float t = (tmin > 0.0f) ? tmin : tmax;   // camera dentro il box → tmax
                if (t > 0.0f && t < bestT) { bestT = t; bestId = b.pickId; }
            }
            if (bestId != MapBoxDraw::kNoPick)
            { m_clickedBoxId = bestId; m_hasClickedBox = true; }
        }
    }
}

bool FreeCameraViewport::popClickedMapBox(int& outPickId)
{
    if (!m_hasClickedBox) return false;
    outPickId = m_clickedBoxId;
    m_hasClickedBox = false;
    m_clickedBoxId  = MapBoxDraw::kNoPick;
    return true;
}

// ── drawMarkerLabels ──────────────────────────────────────────────────────────
// Disegna il nome di ogni marker (attach point) come testo nel viewport.
void FreeCameraViewport::drawMarkerLabels()
{
    if (m_markers.empty()) return;
    glm::mat4 vp = m_camera->getViewProjection();
    ImDrawList* dl = ImGui::GetWindowDrawList();

    for (const auto& mk : m_markers)
    {
        glm::vec4 clip = vp * glm::vec4(mk.pos, 1.0f);
        if (clip.w <= 0.0f) continue;
        float nx = clip.x / clip.w, ny = clip.y / clip.w;
        if (nx < -1.1f || nx > 1.1f || ny < -1.1f || ny > 1.1f) continue;
        float sx = m_imgMin.x + (nx * 0.5f + 0.5f) * m_imgSize.x;
        float sy = m_imgMin.y + (1.0f - (ny * 0.5f + 0.5f)) * m_imgSize.y;
        ImU32 col = mk.selected ? IM_COL32(255,255,160,255)
                                : IM_COL32(210,210,210,210);
        dl->AddText({sx + 8.0f, sy - 7.0f}, col, mk.name.c_str());
    }
}

// ── drawGizmoOverlay ──────────────────────────────────────────────────────────
// Gizmo a 3 modalità (Sposta/Ruota/Scala) disegnato come overlay ImGui sopra
// il viewport. Convenzioni: assi world X=rosso, Y=verde, Z=blu; asse attivo
// evidenziato in giallo; scorciatoie 1/2/3 con mouse sul viewport.
void FreeCameraViewport::drawGizmoOverlay()
{
    if (!m_gizmoEnabled) return;

    // ── Barra strumenti CLICCABILE (Sposta/Ruota/Scala) ──────────────────
    // Selezione del tool affidabile e scopribile, senza dipendere dalla
    // scorciatoia da tastiera (che richiede viewport in hover + mouse libero).
    // Ruota/Scala sono DISABILITATI se il target corrente non li supporta:
    // un pulsante grigio dice a colpo d'occhio "questo elemento non lo permette".
    {
        ImGui::SetCursorScreenPos(ImVec2(m_imgMin.x + 8.0f, m_imgMin.y + 8.0f));
        auto toolBtn = [&](const char* label, GizmoMode gm, bool enabled)
        {
            const bool active = (m_gizmoMode == gm);
            if (!enabled) ImGui::BeginDisabled();
            if (active) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.25f, 0.55f, 0.9f, 1.0f));
            if (ImGui::Button(label)) m_gizmoMode = gm;
            if (active) ImGui::PopStyleColor();
            if (!enabled) ImGui::EndDisabled();
        };
        // Raggruppo la barra così `IsItemHovered` (dopo EndGroup) copre l'intero
        // rettangolo: se il click cade qui, `draw()` annulla la selezione a raggio
        // sottostante — prima cliccare un pulsante cambiava modalità E selezionava
        // l'oggetto dietro in prospettiva (click-through, segnalato dall'utente).
        ImGui::BeginGroup();
        toolBtn("Sposta", GizmoMode::Translate, true);
        ImGui::SameLine();
        toolBtn("Ruota", GizmoMode::Rotate, m_gizmoCanRotate);
        ImGui::SameLine();
        toolBtn("Scala", GizmoMode::Scale, m_gizmoCanScale);
        ImGui::EndGroup();
        m_gizmoBarHovered = ImGui::IsItemHovered();
    }

    // ── Scorciatoie modalità (solo viewport hover, mouse libero) ─────────
    if (ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows) && !m_mouseCapture)
    {
        if (ImGui::IsKeyPressed(ImGuiKey_1)) m_gizmoMode = GizmoMode::Translate;
        if (ImGui::IsKeyPressed(ImGuiKey_2) && m_gizmoCanRotate)
            m_gizmoMode = GizmoMode::Rotate;
        if (ImGui::IsKeyPressed(ImGuiKey_3) && m_gizmoCanScale)
            m_gizmoMode = GizmoMode::Scale;
    }

    // Modalità non consentita per il target corrente → ripiega su Sposta
    GizmoMode mode = m_gizmoMode;
    if (mode == GizmoMode::Rotate && !m_gizmoCanRotate) mode = GizmoMode::Translate;
    if (mode == GizmoMode::Scale  && !m_gizmoCanScale)  mode = GizmoMode::Translate;

    glm::mat4 vp = m_camera->getViewProjection();
    glm::vec4 clip = vp * glm::vec4(m_gizmoPos, 1.0f);
    if (clip.w <= 0.0f) return;

    float ndcX = clip.x / clip.w;
    float ndcY = clip.y / clip.w;
    if (ndcX < -1.1f || ndcX > 1.1f || ndcY < -1.1f || ndcY > 1.1f) return;

    float cx = m_imgMin.x + (ndcX * 0.5f + 0.5f) * m_imgSize.x;
    float cy = m_imgMin.y + (1.0f - (ndcY * 0.5f + 0.5f)) * m_imgSize.y;

    auto toScreen = [&](glm::vec3 world, bool& ok) -> ImVec2 {
        glm::vec4 c1 = vp * glm::vec4(world, 1.0f);
        if (c1.w <= 0.0f) { ok = false; return {0,0}; }
        ok = true;
        float nx = c1.x / c1.w, ny = c1.y / c1.w;
        return { (nx * 0.5f + 0.5f) * m_imgSize.x + m_imgMin.x,
                 (1.0f - (ny * 0.5f + 0.5f)) * m_imgSize.y + m_imgMin.y };
    };

    // Direzione schermo (unitaria) di un asse world dal centro gizmo
    auto projectAxis = [&](glm::vec3 dir) -> ImVec2 {
        bool ok = false;
        ImVec2 p = toScreen(m_gizmoPos + dir * 0.3f, ok);
        if (!ok) return {0,0};
        float dx = p.x - cx, dy = p.y - cy;
        float len = std::sqrt(dx*dx + dy*dy);
        if (len > 0.001f) { dx /= len; dy /= len; }
        return {dx, dy};
    };

    static const glm::vec3 axisWorld[3] = {{1,0,0},{0,1,0},{0,0,1}};
    static const ImU32 axColors[3] = {
        IM_COL32(235,70,70,230),   // X rosso
        IM_COL32(80,220,80,230),   // Y verde
        IM_COL32(80,130,255,230),  // Z blu
    };
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 mp = ImGui::GetMousePos();

    const float distToCamera = glm::length(m_gizmoPos - m_camera->getPosition());
    const float pixelToWorld = 2.0f * std::tan(glm::radians(30.0f))
                             * distToCamera / m_imgSize.y;

    // ── Etichetta modalità accanto al gizmo ───────────────────────────────
    {
        const char* modeName = (mode == GizmoMode::Translate) ? "Sposta [1]"
                             : (mode == GizmoMode::Rotate)    ? "Ruota [2]"
                                                              : "Scala [3]";
        dl->AddText({cx + 14.0f, cy + 12.0f}, IM_COL32(200,200,200,200), modeName);
    }

    // ════════════════════════ SPOSTA / SCALA (assi) ══════════════════════
    if (mode == GizmoMode::Translate || mode == GizmoMode::Scale)
    {
        constexpr float ARM = 60.0f;
        constexpr float TH  = 8.0f;

        ImVec2 axes[3];
        ImVec2 tips[3];
        for (int a = 0; a < 3; ++a)
        {
            axes[a] = projectAxis(axisWorld[a]);
            tips[a] = {cx + axes[a].x * ARM, cy + axes[a].y * ARM};
        }

        // Disegno
        for (int a = 0; a < 3; ++a)
        {
            ImU32 col = (m_gizmoActiveAxis == a) ? IM_COL32(255,255,0,255) : axColors[a];
            dl->AddLine({cx,cy}, tips[a], col, 2.5f);

            if (mode == GizmoMode::Translate)
            {
                ImVec2 perp = {-axes[a].y * TH, axes[a].x * TH};
                dl->AddTriangleFilled(tips[a],
                    {tips[a].x - axes[a].x*TH + perp.x, tips[a].y - axes[a].y*TH + perp.y},
                    {tips[a].x - axes[a].x*TH - perp.x, tips[a].y - axes[a].y*TH - perp.y},
                    col);
            }
            else // Scala: maniglie quadrate
            {
                dl->AddRectFilled({tips[a].x - 5, tips[a].y - 5},
                                  {tips[a].x + 5, tips[a].y + 5}, col);
            }
        }

        // Quadrato centrale: scala uniforme
        if (mode == GizmoMode::Scale)
        {
            ImU32 c = (m_gizmoActiveAxis == 3) ? IM_COL32(255,255,0,255)
                                               : IM_COL32(220,220,220,230);
            dl->AddRect({cx - 7, cy - 7}, {cx + 7, cy + 7}, c, 0, 0, 2.0f);
        }
        else
            dl->AddCircleFilled({cx,cy}, 4.0f, IM_COL32(220,220,220,220));

        // Hit test: punta O corpo dell'asse (distanza punto-segmento)
        auto distToSegment = [&](ImVec2 p, ImVec2 a, ImVec2 b) -> float {
            float vx = b.x-a.x, vy = b.y-a.y;
            float len2 = vx*vx + vy*vy;
            float t = len2 > 0 ? ((p.x-a.x)*vx + (p.y-a.y)*vy) / len2 : 0.0f;
            t = t < 0 ? 0 : (t > 1 ? 1 : t);
            float px = a.x + vx*t, py = a.y + vy*t;
            return std::hypot(p.x-px, p.y-py);
        };

        if (ImGui::IsMouseClicked(0) && m_gizmoActiveAxis < 0)
        {
            if (mode == GizmoMode::Scale
                && std::abs(mp.x-cx) < 9 && std::abs(mp.y-cy) < 9)
                m_gizmoActiveAxis = 3; // uniforme
            else
                for (int a = 0; a < 3; ++a)
                    if (distToSegment(mp, {cx,cy}, tips[a]) < 9.0f)
                    { m_gizmoActiveAxis = a; break; }
        }
        for (int a = 0; a < 3; ++a)
            if (distToSegment(mp, {cx,cy}, tips[a]) < 9.0f)
                ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeAll);

        // Drag
        if (m_gizmoActiveAxis >= 0)
        {
            if (ImGui::IsMouseDown(0))
            {
                ImVec2 md = ImGui::GetIO().MouseDelta;
                if (m_gizmoActiveAxis == 3) // scala uniforme
                {
                    float u = (md.x - md.y) * 0.5f * pixelToWorld;
                    m_gizmoScaleDelta += glm::vec3(u, u, u);
                    m_gizmoScaleDragged = true;
                }
                else
                {
                    // Proiezione del movimento mouse sulla direzione schermo
                    // dell'asse (entrambi in coordinate y-verso-il-basso).
                    float proj = md.x * axes[m_gizmoActiveAxis].x
                               + md.y * axes[m_gizmoActiveAxis].y;
                    glm::vec3 d = axisWorld[m_gizmoActiveAxis] * proj * pixelToWorld;
                    if (mode == GizmoMode::Translate)
                    { m_gizmoDelta += d; m_gizmoDragged = true; }
                    else
                    { m_gizmoScaleDelta += d; m_gizmoScaleDragged = true; }
                }
            }
            else m_gizmoActiveAxis = -1;
        }
    }

    // ════════════════════════════ RUOTA (anelli) ═════════════════════════
    else if (mode == GizmoMode::Rotate)
    {
        constexpr int   SEG    = 48;
        constexpr float RING_PX = 70.0f;
        const float worldR = RING_PX * pixelToWorld;

        // Basi ortogonali per il piano di ogni anello
        static const glm::vec3 basisU[3] = {{0,1,0},{1,0,0},{1,0,0}};
        static const glm::vec3 basisV[3] = {{0,0,1},{0,0,1},{0,1,0}};

        ImVec2 pts[3][SEG];
        bool   ptsOk[3] = {false,false,false};

        for (int a = 0; a < 3; ++a)
        {
            if (!m_gizmoRotAxes[a]) continue;
            bool allOk = true;
            for (int i = 0; i < SEG; ++i)
            {
                float t = (float)i / SEG * 2.0f * 3.1415926f;
                glm::vec3 w = m_gizmoPos
                            + worldR * (std::cos(t) * basisU[a] + std::sin(t) * basisV[a]);
                bool ok = false;
                pts[a][i] = toScreen(w, ok);
                if (!ok) { allOk = false; break; }
            }
            ptsOk[a] = allOk;
            if (!allOk) continue;

            ImU32 col = (m_gizmoActiveAxis == a) ? IM_COL32(255,255,0,255) : axColors[a];
            dl->AddPolyline(pts[a], SEG, col, ImDrawFlags_Closed,
                            (m_gizmoActiveAxis == a) ? 3.0f : 2.0f);
        }
        dl->AddCircleFilled({cx,cy}, 3.5f, IM_COL32(220,220,220,200));

        // Hit test: distanza minima dai campioni dell'anello
        if (ImGui::IsMouseClicked(0) && m_gizmoActiveAxis < 0)
        {
            float best = 10.0f; int bestA = -1;
            for (int a = 0; a < 3; ++a)
            {
                if (!ptsOk[a]) continue;
                for (int i = 0; i < SEG; ++i)
                {
                    float d = std::hypot(mp.x - pts[a][i].x, mp.y - pts[a][i].y);
                    if (d < best) { best = d; bestA = a; }
                }
            }
            if (bestA >= 0)
            {
                m_gizmoActiveAxis = bestA;
                m_gizmoPrevAngle  = std::atan2(mp.y - cy, mp.x - cx);
            }
        }

        // Drag: delta angolare del mouse attorno al centro
        if (m_gizmoActiveAxis >= 0 && m_gizmoActiveAxis < 3)
        {
            if (ImGui::IsMouseDown(0))
            {
                float ang = std::atan2(mp.y - cy, mp.x - cx);
                float d   = ang - m_gizmoPrevAngle;
                while (d >  3.1415926f) d -= 2.0f * 3.1415926f;
                while (d < -3.1415926f) d += 2.0f * 3.1415926f;
                m_gizmoPrevAngle = ang;

                // Segno: rotazione visiva oraria = negativa se l'asse punta
                // verso la camera (regola mano destra; schermo y-in-basso).
                glm::vec3 viewDir = glm::normalize(
                    m_camera->getPosition() - m_gizmoPos);
                float s = (glm::dot(axisWorld[m_gizmoActiveAxis], viewDir) >= 0.0f)
                          ? -1.0f : 1.0f;

                m_gizmoRotDelta[m_gizmoActiveAxis] += glm::degrees(d) * s;
                m_gizmoRotDragged = true;
            }
            else m_gizmoActiveAxis = -1;
        }
    }
}

// ── Barra di caricamento modello (modulo standalone) ──────────────────────────
void FreeCameraViewport::drawLoadBar()
{
    if (ImGui::Button("Sfoglia modello/mappa..."))
    {
        std::string picked = openFileDialog(
            "Modelli 3D\0*.glb;*.gltf;*.obj\0Tutti i file\0*.*\0\0",
            "assets/models");
        if (!picked.empty())
        {
            std::snprintf(m_loadPathBuf, sizeof(m_loadPathBuf), "%s", picked.c_str());
            loadModel(m_loadPathBuf, m_loadRotX, m_loadScale);
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Pulisci"))
    { clearModel(); m_loadPathBuf[0] = '\0'; }

    ImGui::SameLine();
    ImGui::SetNextItemWidth(70);
    if (ImGui::DragFloat("RotX", &m_loadRotX, 1.0f, -180.0f, 180.0f, "%.0f")
        && m_loadPathBuf[0])
        loadModel(m_loadPathBuf, m_loadRotX, m_loadScale);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(70);
    if (ImGui::DragFloat("Scala", &m_loadScale, 0.01f, 0.01f, 50.0f, "%.2f")
        && m_loadPathBuf[0])
        loadModel(m_loadPathBuf, m_loadRotX, m_loadScale);

    if (m_loadPathBuf[0])
    {
        ImGui::SameLine();
        ImGui::TextDisabled("%s", m_loadPathBuf);
    }
    ImGui::Separator();
}

// ── draw() ───────────────────────────────────────────────────────────────────
void FreeCameraViewport::draw(bool showLoadBar)
{
    if (showLoadBar) drawLoadBar();

    ImGui::TextDisabled(
        "Tasto destro = guarda + WASD/QE vola (Shift veloce, rotella = velocita')  |  "
        "Rotella = zoom  |  Tasto centrale = pan  |  1/2/3 = gizmo");
    ImGui::SameLine();
    ImGui::TextDisabled("  vel: %.0f", m_camSpeed);

    ImGui::Separator();

    m_focused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);

    if (!m_lastError.empty())
        ImGui::TextColored({1.0f, 0.3f, 0.3f, 1.0f}, "ERRORE: %s", m_lastError.c_str());
    if (!m_lastModelStatus.empty())
        ImGui::TextColored({0.5f, 0.9f, 0.5f, 1.0f}, "%s", m_lastModelStatus.c_str());

    const ImVec2 avail = ImGui::GetContentRegionAvail();
    const int w = (int)avail.x;
    const int h = (int)avail.y;
    if (w < 8 || h < 8) return;

    m_camera->setAspect((float)w / (float)h);
    resizeFBO(w, h);

    if (!m_fboOk)
    {
        ImGui::TextColored({1,0.4f,0.4f,1}, "FBO non valido.");
        return;
    }

    renderScene();

    // Sub-regione della texture (allocata a multipli di 64, KI #17): la
    // scena occupa l'angolo in basso a sinistra, flip verticale via UV.
    const float u1 = (m_texWidth  > 0) ? (float)w / (float)m_texWidth  : 1.0f;
    const float v1 = (m_texHeight > 0) ? (float)h / (float)m_texHeight : 1.0f;
    ImGui::Image(
        (ImTextureID)(uintptr_t)m_colorTex,
        ImVec2((float)w, (float)h),
        ImVec2(0, v1), ImVec2(u1, 0)
    );

    // Record image rect for picking
    m_imgMin  = ImGui::GetItemRectMin();
    m_imgSize = ImGui::GetItemRectSize();

    const bool imgHovered = ImGui::IsItemHovered();
    ImGuiIO& io = ImGui::GetIO();

    // ── Navigazione stile Unreal ──────────────────────────────────────
    // RMB tenuto: mouselook (+ WASD/QE in tick); rotella regola la velocità.
    // Senza RMB: rotella = dolly avanti/indietro; MMB drag = pan.
    if (imgHovered && ImGui::IsMouseDown(ImGuiMouseButton_Right))
        m_rmbLook = true;
    if (!ImGui::IsMouseDown(ImGuiMouseButton_Right))
        m_rmbLook = false;

    if (m_rmbLook)
    {
        if (io.MouseDelta.x != 0.0f || io.MouseDelta.y != 0.0f)
            m_camera->processMouse(io.MouseDelta.x, io.MouseDelta.y, 0.15f);
        ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeAll);

        if (io.MouseWheel != 0.0f)
        {
            m_camSpeed *= (1.0f + 0.15f * io.MouseWheel);
            if (m_camSpeed < 0.5f)  m_camSpeed = 0.5f;
            if (m_camSpeed > 80.f)  m_camSpeed = 80.f;
        }
    }
    else if (imgHovered)
    {
        if (io.MouseWheel != 0.0f)
        {
            glm::vec3 pos = m_camera->getPosition();
            pos += m_camera->getForward() * io.MouseWheel * (m_camSpeed * 0.25f);
            m_camera->setPosition(pos);
        }
        if (ImGui::IsMouseDragging(ImGuiMouseButton_Middle))
            panCamera(-io.MouseDelta.x * 0.02f, io.MouseDelta.y * 0.02f);
    }

    // Check click (selezione: solo LMB, mai durante la navigazione)
    if (ImGui::IsItemClicked(0) && !m_mouseCapture && !m_rmbLook)
    {
        m_imgClicked  = true;
        m_imgClickPos = ImGui::GetMousePos();
    }

    // Il gizmo PRIMA della selezione: se il click afferra un asse del gizmo,
    // `handleViewportClick` lo vede (m_gizmoActiveAxis >= 0) e NON seleziona un
    // oggetto dietro al gizmo (che rovinerebbe il trascinamento).
    drawGizmoOverlay();
    // Se il click è caduto sulla barra modalità (Sposta/Ruota/Scala) dell'overlay,
    // NON trattarlo come selezione a raggio: cambia solo la modalità del gizmo.
    if (m_gizmoBarHovered) m_imgClicked = false;
    handleViewportClick();
    drawMarkerLabels();
}

// ── tick() ────────────────────────────────────────────────────────────────────
void FreeCameraViewport::tick(float dt)
{
    if (!m_focused) return;
    const Uint8* ks = SDL_GetKeyboardState(nullptr);
    bool tabNow = ks[SDL_SCANCODE_TAB] != 0;
    if (tabNow && !m_tabWasDown)
    {
        m_mouseCapture = !m_mouseCapture;
        SDL_SetRelativeMouseMode(m_mouseCapture ? SDL_TRUE : SDL_FALSE);
    }
    m_tabWasDown = tabNow;

    // Il volo WASD è attivo SOLO durante la navigazione (RMB tenuto o TAB
    // capture) e mai mentre si digita in un campo di testo: prima la camera
    // si muoveva appena la finestra era focused, rendendo l'editing caotico.
    const bool flying = (m_mouseCapture || m_rmbLook)
                      && !ImGui::GetIO().WantTextInput;
    if (flying)
    {
        m_camera->setSpeed(ks[SDL_SCANCODE_LSHIFT] ? m_camSpeed*3.0f : m_camSpeed);
        m_camera->processKeyboard(
            ks[SDL_SCANCODE_W]!=0, ks[SDL_SCANCODE_S]!=0,
            ks[SDL_SCANCODE_A]!=0, ks[SDL_SCANCODE_D]!=0,
            ks[SDL_SCANCODE_E] || ks[SDL_SCANCODE_SPACE],
            ks[SDL_SCANCODE_Q] || ks[SDL_SCANCODE_LCTRL], dt);
    }
    if (m_mouseCapture)
    {
        int dx=0, dy=0;
        SDL_GetRelativeMouseState(&dx, &dy);
        if (dx || dy) m_camera->processMouse((float)dx, (float)dy, 0.15f);
    }
}

// ── loadModel() ───────────────────────────────────────────────────────────────
void FreeCameraViewport::loadModel(const std::string& path,
                                   float meshRotX, float meshScale,
                                   float meshRotY)
{
    m_modelData.clear();
    m_modelVertCount = 0;
    m_lastModelStatus.clear();

    if (path.empty())
    {
        m_lastModelStatus = "Nessun percorso specificato.";
        return;
    }

    std::optional<mini::Model> mdl;
    const size_t len = path.size();
    bool isGltf = (len >= 4 &&
        (path.substr(len-4)==".glb" || (len>=5 && path.substr(len-5)==".gltf")));
    if (isGltf)
        mdl = mini::Model::loadFromGltf(path.c_str());
    else
        mdl = mini::Model::loadFromObj(path.c_str());

    if (!mdl || mdl->isEmpty())
    {
        m_lastModelStatus = "Modello non trovato: " + path;
        return;
    }

    glm::mat4 correction = glm::rotate(glm::mat4(1.0f),
                                        glm::radians(meshRotY), {0,1,0})
                         * glm::rotate(glm::mat4(1.0f),
                                        glm::radians(meshRotX), {1,0,0});
    correction = glm::scale(correction, glm::vec3(meshScale));

    for (const auto& mesh : mdl->getMeshes())
    {
        const auto& raw   = mesh.getVertexData();
        const int   count = mesh.getVertexCount();
        m_modelData.reserve(m_modelData.size() + (size_t)count * 6);
        for (int i = 0; i < count; ++i)
        {
            const float* v = raw.data() + i * 11;
            glm::vec4 wp = correction * glm::vec4(v[0], v[1], v[2], 1.0f);
            float cr = v[6], cg = v[7], cb = v[8];
            if (cr > 0.99f && cg > 0.99f && cb > 0.99f)
            { cr = 0.75f; cg = 0.75f; cb = 0.78f; }
            m_modelData.insert(m_modelData.end(), {wp.x, wp.y, wp.z, cr, cg, cb});
        }
    }

    m_modelVertCount = (int)(m_modelData.size() / 6);
    if (m_modelVertCount > 0)
        m_lastModelStatus = "Modello caricato: " + std::to_string(m_modelVertCount) + " vertici";
    else
        m_lastModelStatus = "Modello vuoto (0 vertici): " + path;
}

void FreeCameraViewport::clearModel()
{
    m_modelData.clear();
    m_modelVertCount = 0;
    m_lastModelStatus.clear();
}

// ── setAttachmentModel() ───────────────────────────────────────────────────────
void FreeCameraViewport::setAttachmentModel(const std::string& path,
                                            const glm::mat4& transform)
{
    m_attachData.clear();
    m_attachVertCount = 0;
    if (path.empty()) return;

    std::optional<mini::Model> mdl;
    const size_t len = path.size();
    bool isGltf = (len >= 4 &&
        (path.substr(len-4)==".glb" || (len>=5 && path.substr(len-5)==".gltf")));
    if (isGltf) mdl = mini::Model::loadFromGltf(path.c_str());
    else        mdl = mini::Model::loadFromObj(path.c_str());

    if (!mdl || mdl->isEmpty()) return;

    const glm::mat3 nrm = glm::transpose(glm::inverse(glm::mat3(transform)));
    (void)nrm;

    for (const auto& mesh : mdl->getMeshes())
    {
        const auto& raw   = mesh.getVertexData();
        const int   count = mesh.getVertexCount();
        m_attachData.reserve(m_attachData.size() + (size_t)count * 6);
        for (int i = 0; i < count; ++i)
        {
            const float* v = raw.data() + i * 11;
            glm::vec4 wp = transform * glm::vec4(v[0], v[1], v[2], 1.0f);
            // Colore arma: grigio metallico leggermente caldo
            float cr = 0.55f, cg = 0.55f, cb = 0.58f;
            m_attachData.insert(m_attachData.end(), {wp.x, wp.y, wp.z, cr, cg, cb});
        }
    }
    m_attachVertCount = (int)(m_attachData.size() / 6);
}

void FreeCameraViewport::clearAttachmentModel()
{
    m_attachData.clear();
    m_attachVertCount = 0;
}

// ── setHitboxes() ─────────────────────────────────────────────────────────────
void FreeCameraViewport::setHitboxes(const std::vector<mini::HitZone>& zones,
                                     int selZone)
{
    m_boxData.clear();
    m_boxData.reserve(zones.size() * 24 * 6);

    auto addEdge = [&](glm::vec3 a, glm::vec3 b, float r, float g, float bv)
    {
        m_boxData.insert(m_boxData.end(), {a.x,a.y,a.z, r,g,bv,
                                           b.x,b.y,b.z, r,g,bv});
    };

    for (int zi = 0; zi < (int)zones.size(); ++zi)
    {
        const auto& z   = zones[zi];
        const bool  sel = (zi == selZone);
        const float mult = z.damageMultiplier;

        float r, g, b;
        if      (mult >= 2.0f) { r=1.0f;  g=0.30f; b=0.30f; }
        else if (mult >= 1.0f) { r=1.0f;  g=0.85f; b=0.30f; }
        else                   { r=0.40f; g=0.50f; b=1.00f; }

        if (sel) {
            r = (r + 0.25f > 1.0f) ? 1.0f : r + 0.25f;
            g = (g + 0.25f > 1.0f) ? 1.0f : g + 0.25f;
            b = (b + 0.25f > 1.0f) ? 1.0f : b + 0.25f;
        }

        glm::vec3 c = {z.offset.x,      z.offset.y,      z.offset.z};
        glm::vec3 e = {z.halfExtents.x, z.halfExtents.y, z.halfExtents.z};

        // Rotazione locale della zona (eulerDeg, ordine Y*X*Z)
        glm::mat3 R(1.0f);
        if (z.eulerDeg.x != 0.0f || z.eulerDeg.y != 0.0f || z.eulerDeg.z != 0.0f)
        {
            glm::mat4 rm = glm::rotate(glm::mat4(1.0f), glm::radians(z.eulerDeg.y), {0,1,0})
                         * glm::rotate(glm::mat4(1.0f), glm::radians(z.eulerDeg.x), {1,0,0})
                         * glm::rotate(glm::mat4(1.0f), glm::radians(z.eulerDeg.z), {0,0,1});
            R = glm::mat3(rm);
        }

        glm::vec3 corners[8];
        {
            const glm::vec3 local[8] = {
                {-e.x,-e.y,-e.z}, { e.x,-e.y,-e.z}, { e.x, e.y,-e.z}, {-e.x, e.y,-e.z},
                {-e.x,-e.y, e.z}, { e.x,-e.y, e.z}, { e.x, e.y, e.z}, {-e.x, e.y, e.z},
            };
            for (int k = 0; k < 8; ++k) corners[k] = c + R * local[k];
        }
        const int edges[12][2] = {
            {0,1},{1,2},{2,3},{3,0},
            {4,5},{5,6},{6,7},{7,4},
            {0,4},{1,5},{2,6},{3,7}
        };
        for (auto& edge : edges)
            addEdge(corners[edge[0]], corners[edge[1]], r, g, b);
    }

    m_boxVertCount = (int)(m_boxData.size() / 6);
}

// ── setMapBoxes() ─────────────────────────────────────────────────────────────
void FreeCameraViewport::setMapBoxes(const std::vector<MapBoxDraw>& boxes)
{
    m_mapBoxes = boxes;   // conservata per il ray-picking (popClickedMapBox)
    m_mapBoxData.clear();
    m_mapBoxData.reserve(boxes.size() * 24 * 6);
    m_mapBoxFillData.clear();
    m_mapBoxFillData.reserve(boxes.size() * 36 * 6);   // 6 facce × 2 tri × 3 vert

    auto pushEdge = [&](glm::vec3 a, glm::vec3 b, float r, float g, float bv) {
        m_mapBoxData.insert(m_mapBoxData.end(), {a.x,a.y,a.z,r,g,bv, b.x,b.y,b.z,r,g,bv});
    };
    // Faccia piena = 2 triangoli. `shade` fa una finta luce (alto più chiaro, sotto
    // più scuro) così i volumi si leggono senza normali/illuminazione nello shader.
    auto pushFace = [&](glm::vec3 a, glm::vec3 b, glm::vec3 c, glm::vec3 d,
                        float r, float g, float bv, float shade) {
        const float sr = r * shade, sg = g * shade, sb = bv * shade;
        m_mapBoxFillData.insert(m_mapBoxFillData.end(), {
            a.x,a.y,a.z,sr,sg,sb,  b.x,b.y,b.z,sr,sg,sb,  c.x,c.y,c.z,sr,sg,sb,
            a.x,a.y,a.z,sr,sg,sb,  c.x,c.y,c.z,sr,sg,sb,  d.x,d.y,d.z,sr,sg,sb });
    };

    for (const auto& box : boxes)
    {
        float r = box.r, g = box.g, bv = box.b;
        if (box.selected) {
            r  = (r  * 1.8f + 0.25f) < 1.0f ? (r  * 1.8f + 0.25f) : 1.0f;
            g  = (g  * 1.8f + 0.25f) < 1.0f ? (g  * 1.8f + 0.25f) : 1.0f;
            bv = (bv * 1.8f + 0.25f) < 1.0f ? (bv * 1.8f + 0.25f) : 1.0f;
        }

        float hx = box.sx * 0.5f, hy = box.sy * 0.5f, hz = box.sz * 0.5f;
        float cosR = std::cos(glm::radians(box.ry));
        float sinR = std::sin(glm::radians(box.ry));

        auto corner = [&](float lx, float ly, float lz) -> glm::vec3 {
            float wx = cosR * lx + sinR * lz + box.x;
            float wz =-sinR * lx + cosR * lz + box.z;
            return {wx, ly + box.y, wz};
        };

        glm::vec3 c[8] = {
            corner(-hx,-hy,-hz), corner( hx,-hy,-hz),
            corner( hx, hy,-hz), corner(-hx, hy,-hz),
            corner(-hx,-hy, hz), corner( hx,-hy, hz),
            corner( hx, hy, hz), corner(-hx, hy, hz),
        };
        const int edges[12][2] = {
            {0,1},{1,2},{2,3},{3,0},
            {4,5},{5,6},{6,7},{7,4},
            {0,4},{1,5},{2,6},{3,7}
        };
        for (auto& e : edges) pushEdge(c[e[0]], c[e[1]], r, g, bv);

        // Facce piene ombreggiate (culling OFF nel pipeline → winding indifferente).
        pushFace(c[3], c[2], c[6], c[7], r, g, bv, 1.00f);   // top    (+y) chiaro
        pushFace(c[0], c[1], c[5], c[4], r, g, bv, 0.50f);   // bottom (-y) scuro
        pushFace(c[0], c[1], c[2], c[3], r, g, bv, 0.82f);   // front  (-z)
        pushFace(c[4], c[5], c[6], c[7], r, g, bv, 0.82f);   // back   (+z)
        pushFace(c[0], c[4], c[7], c[3], r, g, bv, 0.65f);   // left   (-x)
        pushFace(c[1], c[5], c[6], c[2], r, g, bv, 0.65f);   // right  (+x)
    }

    m_mapBoxVertCount = (int)(m_mapBoxData.size() / 6);
    m_mapBoxFillVertCount = (int)(m_mapBoxFillData.size() / 6);
}

void FreeCameraViewport::clearMapBoxes()
{
    m_mapBoxes.clear();
    m_mapBoxData.clear();
    m_mapBoxVertCount = 0;
    m_mapBoxFillData.clear();
    m_mapBoxFillVertCount = 0;
}

} // namespace editor
