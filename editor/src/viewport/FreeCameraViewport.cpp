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
#include "mini/core/Telemetry.hpp"
#include "mini/game/MapMetrics.hpp"

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

    updateInfiniteGrid();   // fissa, ancorata al mondo, 1000 m di lato (changelog 186)
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
// ── GRIGLIA "INFINITA" (richiesta utente 2026-08-08) ────────────────────────
// La griglia era costruita UNA volta a 120 m con passo 2: su una mappa 300 × 200
// finisce, e oltre il bordo si costruisce senza riferimenti.
//
// "Infinita" in pratica significa due cose insieme:
//   · **segue la vista** — si ricostruisce centrata su dove si guarda, agganciata al
//     passo così le linee non strisciano mentre ci si sposta;
//   · **adatta il passo** — allontanandosi, un passo fisso diventerebbe una massa
//     grigia illeggibile e migliaia di linee inutili. Passi "tondi" (1/2/5 × 10ⁿ),
//     come sulle carte e come già fa la barra di scala.
// Il numero di linee resta LIMITATO: è la ragione per cui una griglia davvero
// infinita non si disegna, la si simula.
void FreeCameraViewport::updateInfiniteGrid()
{
    // FISSA e ANCORATA AL MONDO. Costruita una volta sola.
    //
    // Il primo tentativo (changelog 185) la faceva seguire la vista e adattare il
    // passo. Sulla carta era la soluzione "corretta"; all'uso era peggio del
    // problema: *"se si muove seguendo dove sto guardando fa solo casino e mi
    // confonde"*. E aveva ragione — cambiando il passo, TUTTE le linee saltano di
    // posto insieme, e una griglia che salta non è più un riferimento.
    //
    // Una griglia serve a dare un riferimento STABILE. Quindi: passo fisso di 2 m,
    // estensione 1000 m (la mappa grande pianificata è 300 × 200, quindi la copre
    // tre volte in ogni direzione), linee sempre negli stessi punti del mondo.
    // Costa ~12.000 float una volta sola: non vale la pena di essere furbi.
    if (!m_gridData.empty()) return;

    constexpr float kStep = 2.0f;
    constexpr float kHalf = 1000.0f;
    const int n = (int)(kHalf / kStep);

    auto ln = [&](float x0, float z0, float x1, float z1, float y,
                  float r, float g, float b) {
        m_gridData.insert(m_gridData.end(),
            {x0, y, z0, r, g, b, x1, y, z1, r, g, b});
    };
    for (int i = -n; i <= n; ++i)
    {
        const float o = i * kStep;
        // Una linea più chiara ogni 10 m: dà il senso della scala senza etichette.
        const float c = ((i % 5) == 0) ? 0.40f : 0.22f;
        ln(-kHalf, o, kHalf, o, 0.0f, c, c, c);
        ln(o, -kHalf, o, kHalf, 0.0f, c, c, c);
    }
    // Gli assi del mondo: rosso = X, blu = Z. Sono l'origine della mappa.
    ln(-kHalf, 0.0f, kHalf, 0.0f, 0.02f, 0.95f, 0.25f, 0.25f);
    ln(0.0f, -kHalf, 0.0f, kHalf, 0.02f, 0.25f, 0.55f, 1.00f);

    m_gridVertCount = (int)(m_gridData.size() / 6);
}

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

    // ── GUARDIA: il conteggio non può superare i dati (KI #98) ───────────
    // Con gli array client-side (ADR-003) è il DRIVER a leggere questa memoria
    // durante `glDrawArrays`. Se `count` promette più vertici di quanti ce ne
    // siano, la lettura oltre il limite avviene dentro la DLL del driver: un
    // access violation che ASan non può vedere, perché ASan strumenta il nostro
    // codice, non il driver. Qui la si intercetta PRIMA, con un nome.
    // Costo: un confronto fra interi per disegno.
    if ((std::size_t)count * 6u > data.size())
    {
        static bool s_told = false;   // una volta sola: non deve allagare il log
        if (!s_told)
        {
            s_told = true;
            std::fprintf(stderr,
                "[Viewport] DISEGNO RIFIUTATO: %d vertici richiesti ma solo %zu "
                "float disponibili (%zu vertici). Sarebbe stata una lettura oltre "
                "il limite fatta dal driver. Vedi KI #98.\n",
                count, data.size(), data.size() / 6u);
            mini::telemetry::setPhase("drawArray: conteggio oltre i dati (KI #98)");
        }
        return;
    }

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

    // s_delFBO/s_delRBO sono nella guardia perché più sotto vengono CHIAMATI:
    // erano gli unici due usati senza essere verificati, e il distruttore (che
    // li protegge con `s_delFBO &&`) mostra che nulli possono esserlo davvero.
    // Un puntatore a funzione nullo chiamato è un access violation identico a
    // quello che stiamo cercando — non un caso teorico.
    if (!s_genFBO || !s_bindFBO || !s_fboTex || !s_genRBO ||
        !s_bindRBO || !s_rboSt  || !s_fboRBO || !s_chkFBO ||
        !s_delFBO  || !s_delRBO)
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

    updateInfiniteGrid();   // costruita una volta sola: ritorna subito se già pronta
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

    // Navmesh SOPRA la geometria: è un'informazione di verifica, deve prevalere
    // sul disegno della mappa invece di finirci sotto.
    drawArray(m_navFillData, m_navFillVertCount, GL_TRIANGLES, vp);
    drawArray(m_navEdgeData, m_navEdgeVertCount, GL_LINES, vp);

    // Righello sopra tutto e SENZA test di profondità: una misura che sparisce
    // dietro un muro non misura niente.
    if (m_rulerVertCount > 0)
    {
        glDisable(GL_DEPTH_TEST);
        drawArray(m_rulerData, m_rulerVertCount, GL_LINES, vp);
        glEnable(GL_DEPTH_TEST);
    }

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

// ── MODALITÀ FACCIA e DISEGNA BOX (doc 53 L1) ────────────────────────────────
void FreeCameraViewport::setGizmoBounds(const glm::vec3& mn, const glm::vec3& mx, bool valid)
{
    m_boundsMin = mn; m_boundsMax = mx; m_boundsValid = valid;
}

bool FreeCameraViewport::popGizmoFaceDelta(int& outFace, float& outDelta)
{
    if (!m_faceHas) return false;
    outFace  = m_faceLast;
    outDelta = m_facePending;
    m_facePending = 0.0f;
    m_faceHas = false;
    return true;
}

void FreeCameraViewport::setDrawBoxActive(bool on)
{
    m_drawActive   = on;
    m_drawDragging = false;
    m_drawHas      = false;
    // Il righello e il disegno vogliono lo stesso clic: due strumenti modali accesi
    // insieme sono un clic che fa due cose, cioè nessuna delle due in modo
    // prevedibile. Accendere l'uno spegne l'altro.
    if (on) setRulerActive(false);
}

bool FreeCameraViewport::popDrawnRect(glm::vec3& outMin, glm::vec3& outMax)
{
    if (!m_drawHas) return false;
    outMin = m_drawMin; outMax = m_drawMax;
    m_drawHas = false;
    return true;
}

bool FreeCameraViewport::popGridStepRequest(int& outDir)
{
    if (m_gridStepReq == 0) return false;
    outDir = m_gridStepReq;
    m_gridStepReq = 0;
    return true;
}

bool FreeCameraViewport::worldToScreen(const glm::vec3& w, ImVec2& out) const
{
    const glm::vec4 c = m_camera->getViewProjection() * glm::vec4(w, 1.0f);
    if (c.w <= 0.0001f) return false;
    const float nx = c.x / c.w, ny = c.y / c.w;
    out = { (nx * 0.5f + 0.5f) * m_imgSize.x + m_imgMin.x,
            (1.0f - (ny * 0.5f + 0.5f)) * m_imgSize.y + m_imgMin.y };
    return true;
}

// Le sei maniglie sulle facce dell'ingombro della selezione. Tirare una maniglia
// muove QUELLA faccia: la opposta resta ferma. È la differenza con la scala, che
// muove entrambe e costringe a ricentrare.
void FreeCameraViewport::drawFaceGizmo()
{
    if (!m_boundsValid) return;
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 mp = ImGui::GetMousePos();
    const glm::vec3 c = (m_boundsMin + m_boundsMax) * 0.5f;

    // Centro di ogni faccia e sua normale uscente.
    glm::vec3 fc[6], fn[6];
    fc[0] = {m_boundsMin.x, c.y, c.z};  fn[0] = {-1, 0, 0};
    fc[1] = {m_boundsMax.x, c.y, c.z};  fn[1] = { 1, 0, 0};
    fc[2] = {c.x, m_boundsMin.y, c.z};  fn[2] = { 0,-1, 0};
    fc[3] = {c.x, m_boundsMax.y, c.z};  fn[3] = { 0, 1, 0};
    fc[4] = {c.x, c.y, m_boundsMin.z};  fn[4] = { 0, 0,-1};
    fc[5] = {c.x, c.y, m_boundsMax.z};  fn[5] = { 0, 0, 1};

    static const ImU32 faceCol[6] = {
        IM_COL32(235, 70, 70,220), IM_COL32(235, 70, 70,220),   // X rosso
        IM_COL32( 80,220, 80,220), IM_COL32( 80,220, 80,220),   // Y verde
        IM_COL32( 80,130,255,220), IM_COL32( 80,130,255,220),   // Z blu
    };

    ImVec2 sp[6]; bool ok[6];
    for (int f = 0; f < 6; ++f) ok[f] = worldToScreen(fc[f], sp[f]);

    // Direzione SCHERMO della normale: serve a proiettare il movimento del mouse.
    ImVec2 sdir[6];
    for (int f = 0; f < 6; ++f)
    {
        sdir[f] = {0, 0};
        if (!ok[f]) continue;
        ImVec2 tip;
        if (!worldToScreen(fc[f] + fn[f] * 0.5f, tip)) continue;
        float dx = tip.x - sp[f].x, dy = tip.y - sp[f].y;
        const float len = std::sqrt(dx*dx + dy*dy);
        if (len > 0.001f) { dx /= len; dy /= len; sdir[f] = {dx, dy}; }
    }

    for (int f = 0; f < 6; ++f)
    {
        if (!ok[f]) continue;
        const bool act = (m_faceActive == f);
        const ImU32 col = act ? IM_COL32(255,255,0,255) : faceCol[f];
        dl->AddRectFilled({sp[f].x - 7, sp[f].y - 7}, {sp[f].x + 7, sp[f].y + 7}, col, 2.0f);
        dl->AddRect({sp[f].x - 7, sp[f].y - 7}, {sp[f].x + 7, sp[f].y + 7},
                    IM_COL32(20,20,20,200), 2.0f);
        // L'ultima faccia usata è quella su cui agiscono E/Q: se non si vede quale,
        // premere E diventa un tiro a indovinare.
        if (f == m_faceLast && !act)
            dl->AddRect({sp[f].x - 10, sp[f].y - 10}, {sp[f].x + 10, sp[f].y + 10},
                        IM_COL32(255,255,255,200), 3.0f, 0, 1.5f);
    }

    if (ImGui::IsMouseClicked(0) && m_faceActive < 0)
        for (int f = 0; f < 6; ++f)
            if (ok[f] && std::abs(mp.x - sp[f].x) < 9 && std::abs(mp.y - sp[f].y) < 9)
            { m_faceActive = f; m_faceLast = f; m_faceAccum = 0.0f; break; }

    for (int f = 0; f < 6; ++f)
        if (ok[f] && std::abs(mp.x - sp[f].x) < 9 && std::abs(mp.y - sp[f].y) < 9)
            ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeAll);

    if (m_faceActive >= 0)
    {
        if (ImGui::IsMouseDown(0))
        {
            const int f = m_faceActive;
            const float distToCamera = glm::length(fc[f] - m_camera->getPosition());
            const float pixelToWorld = 2.0f * std::tan(glm::radians(30.0f))
                                     * distToCamera / (m_imgSize.y > 1 ? m_imgSize.y : 1.0f);
            const ImVec2 md = ImGui::GetIO().MouseDelta;
            m_faceAccum += (md.x * sdir[f].x + md.y * sdir[f].y) * pixelToWorld;

            // AGGANCIO: si emette solo a passi interi di griglia. Emettere il grezzo
            // darebbe muri lunghi 3,47 m — e le fessure che ne nascono stanno sotto
            // la soglia di erosione del navmesh, cioè non si vedono e rompono.
            const float step = (m_rulerSnap > 0.001f) ? m_rulerSnap : 0.0f;
            if (step > 0.0f)
            {
                const float n = std::trunc(m_faceAccum / step);
                if (std::fabs(n) >= 1.0f)
                { m_facePending += n * step; m_faceAccum -= n * step; m_faceHas = true; }
            }
            else if (std::fabs(m_faceAccum) > 0.0001f)
            { m_facePending += m_faceAccum; m_faceAccum = 0.0f; m_faceHas = true; }
        }
        else { m_faceActive = -1; m_faceAccum = 0.0f; }
    }
}

// Trascinamento sul piano di lavoro: l'impronta in pianta di un box nuovo.
void FreeCameraViewport::drawBoxTool(bool hovered)
{
    if (!m_drawActive) return;
    const float step = (m_rulerSnap > 0.001f) ? m_rulerSnap : 0.0f;
    auto snap = [&](float v) { return step > 0.0f ? std::round(v / step) * step : v; };

    if (hovered)
    {
        const ImVec2 mp = ImGui::GetMousePos();
        glm::vec3 hit;
        if (screenToPlane(mp.x, mp.y, m_drawPlaneY, hit))
        {
            hit.x = snap(hit.x); hit.z = snap(hit.z); hit.y = m_drawPlaneY;
            if (ImGui::IsMouseClicked(ImGuiMouseButton_Left))
            { m_drawA = hit; m_drawB = hit; m_drawDragging = true; }
            else if (m_drawDragging) m_drawB = hit;
        }
    }
    if (m_drawDragging && !ImGui::IsMouseDown(ImGuiMouseButton_Left))
    {
        m_drawDragging = false;
        // Ternari e non std::min/max: <windows.h> definisce `min` e `max` come macro
        // e qui il compilatore le espande prima di vedere `std::`. È già costato due
        // volte in questo progetto.
        const float ax = m_drawA.x, bx = m_drawB.x, az = m_drawA.z, bz = m_drawB.z;
        m_drawMin = { (ax < bx ? ax : bx), m_drawPlaneY, (az < bz ? az : bz) };
        m_drawMax = { (ax > bx ? ax : bx), m_drawPlaneY, (az > bz ? az : bz) };
        // Un clic senza trascinamento non è un box da zero metri: è un clic. Si
        // scarta invece di creare geometria degenere che poi sparisce dal navmesh
        // senza spiegazione.
        const float w = m_drawMax.x - m_drawMin.x, d = m_drawMax.z - m_drawMin.z;
        m_drawHas = (w > 0.001f && d > 0.001f);
    }

    // Anteprima: il rettangolo e le sue misure, MENTRE si trascina. Il numero
    // durante il gesto è ciò che rende inutile misurare dopo.
    if (m_drawDragging)
    {
        const float ax = m_drawA.x, bx = m_drawB.x, az = m_drawA.z, bz = m_drawB.z;
        const float x0 = (ax < bx ? ax : bx), x1 = (ax > bx ? ax : bx);
        const float z0 = (az < bz ? az : bz), z1 = (az > bz ? az : bz);
        const glm::vec3 corner[4] = { {x0, m_drawPlaneY, z0}, {x1, m_drawPlaneY, z0},
                                      {x1, m_drawPlaneY, z1}, {x0, m_drawPlaneY, z1} };
        ImVec2 s[4]; bool allOk = true;
        for (int i = 0; i < 4; ++i) if (!worldToScreen(corner[i], s[i])) allOk = false;
        if (allOk)
        {
            ImDrawList* dl = ImGui::GetWindowDrawList();
            for (int i = 0; i < 4; ++i)
                dl->AddLine(s[i], s[(i + 1) % 4], IM_COL32(255, 210, 80, 240), 2.0f);
            char buf[64];
            std::snprintf(buf, sizeof(buf), "%.2f x %.2f m", x1 - x0, z1 - z0);
            const ImVec2 mid = { (s[0].x + s[2].x) * 0.5f, (s[0].y + s[2].y) * 0.5f };
            dl->AddText({mid.x + 8, mid.y}, IM_COL32(255, 230, 140, 255), buf);
        }
    }
}

// ── Pan ───────────────────────────────────────────────────────────────────────
void FreeCameraViewport::panCamera(float rightDelta, float upDelta)
{
    const glm::vec3 fwd = m_camera->getForward();
    // Guardando DRITTO in basso (vista dall'alto) `cross(fwd, {0,1,0})` è il vettore
    // nullo, e normalizzarlo dà NaN: la camera sparirebbe alla prima trascinata.
    // In quel caso lo "spostamento in su" nello schermo è −Z nel mondo, non +Y.
    if (std::fabs(fwd.y) > 0.99f)
    {
        glm::vec3 pos = m_camera->getPosition();
        pos.x += rightDelta;
        pos.z -= upDelta * (fwd.y < 0.0f ? 1.0f : -1.0f);
        m_camera->setPosition(pos);
        return;
    }
    glm::vec3 right = glm::normalize(glm::cross(fwd, {0,1,0}));
    glm::vec3 pos   = m_camera->getPosition();
    pos += right * rightDelta + glm::vec3(0,1,0) * upDelta;
    m_camera->setPosition(pos);
}

// ── MISURE VISIBILI SUL VIEWPORT (richiesta utente 2026-08-06) ──────────────
// *"una funzione in più che aggiunge sulla griglia delle misure visibili in maniera
// chiara evidente"*. Il righello dice la distanza fra due punti scelti; questo dice
// la SCALA di ciò che si sta guardando, sempre, senza che tu debba chiedere.
//
// Due cose, entrambe come sulle carte geografiche:
//   · una BARRA DI SCALA con la sua lunghezza scritta ("50 m"), che è il modo
//     universale di dire quanto è grande ciò che si vede;
//   · le COORDINATE lungo i bordi, a passi "tondi" (1/2/5 × 10ⁿ), così si legge
//     dove si è e quanto dista una cosa dall'altra senza misurare.
// Disegnate in sovrimpressione con la draw list di ImGui: sono testo, e il testo
// nella scena 3D andrebbe ruotato, scalato e ridisegnato a ogni frame.
void FreeCameraViewport::drawMeasureOverlay()
{
    if (!m_showMeasures || m_imgSize.x < 80.0f || m_imgSize.y < 60.0f) return;

    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImU32 colLine = IM_COL32(255, 255, 255, 60);
    const ImU32 colText = IM_COL32(220, 235, 255, 220);
    const ImU32 colBar  = IM_COL32(255, 220, 120, 235);

    // Quanti metri copre il viewport in orizzontale: in ortografica è esatto; in
    // prospettiva si stima sul piano del suolo, e allora si mostra SOLO la barra di
    // scala (le coordinate ai bordi mentirebbero, perché la scala varia con la quota).
    float metersAcross = 0.0f;
    glm::vec3 pL, pR;
    const float midY = m_imgMin.y + m_imgSize.y * 0.5f;
    if (screenToPlane(m_imgMin.x + 2.0f, midY, 0.0f, pL)
     && screenToPlane(m_imgMin.x + m_imgSize.x - 2.0f, midY, 0.0f, pR))
        metersAcross = glm::length(glm::vec2(pR.x - pL.x, pR.z - pL.z));
    if (metersAcross < 0.01f || metersAcross > 100000.0f) return;

    // Passo "tondo": 1, 2 o 5 per una potenza di dieci. Un passo di 37,4 m sarebbe
    // tecnicamente corretto e illeggibile.
    auto niceStep = [](float rough) {
        const float p = std::pow(10.0f, std::floor(std::log10(rough)));
        const float n = rough / p;
        if (n < 1.5f) return 1.0f * p;
        if (n < 3.5f) return 2.0f * p;
        if (n < 7.5f) return 5.0f * p;
        return 10.0f * p;
    };

    // ── Barra di scala, in basso a sinistra ──────────────────────────────
    {
        const float target = metersAcross * 0.22f;      // ~un quinto della vista
        const float step   = niceStep(target);
        const float pxPerM = m_imgSize.x / metersAcross;
        const float barPx  = step * pxPerM;
        if (barPx > 20.0f && barPx < m_imgSize.x * 0.8f)
        {
            const float x0 = m_imgMin.x + 12.0f;
            const float y0 = m_imgMin.y + m_imgSize.y - 22.0f;
            dl->AddLine({x0, y0}, {x0 + barPx, y0}, colBar, 2.0f);
            dl->AddLine({x0, y0 - 5.0f}, {x0, y0 + 5.0f}, colBar, 2.0f);
            dl->AddLine({x0 + barPx, y0 - 5.0f}, {x0 + barPx, y0 + 5.0f}, colBar, 2.0f);
            char lbl[32];
            if (step >= 1.0f) std::snprintf(lbl, sizeof(lbl), "%.0f m", step);
            else              std::snprintf(lbl, sizeof(lbl), "%.1f m", step);
            // Ombra dietro il testo: sopra una scena chiara il bianco sparisce.
            dl->AddText({x0 + 3.0f, y0 - 19.0f}, IM_COL32(0,0,0,160), lbl);
            dl->AddText({x0 + 2.0f, y0 - 20.0f}, colBar, lbl);
        }
    }

    // Le coordinate ai bordi solo in ORTOGRAFICA: in prospettiva la scala cambia
    // con la profondità, quindi una tacca "ogni 10 m" sarebbe una bugia.
    if (!isOrtho()) return;

    const float step = niceStep(metersAcross / 10.0f);
    const float pxPerM = m_imgSize.x / metersAcross;
    if (step * pxPerM < 28.0f) return;   // troppo fitte per essere leggibili

    // Assi mostrati: dipendono da cosa si sta guardando.
    const bool topView = (m_viewMode == ViewMode::Top);
    // Estremi del mondo visibili, presi dagli angoli dell'immagine.
    glm::vec3 tl, br;
    if (!screenToPlane(m_imgMin.x, m_imgMin.y, 0.0f, tl)) return;
    if (!screenToPlane(m_imgMin.x + m_imgSize.x, m_imgMin.y + m_imgSize.y, 0.0f, br)) return;

    if (topView)
    {
        // X lungo il bordo superiore, Z lungo quello sinistro.
        // Ternari e non `std::min/max`: <windows.h> definisce `min` e `max` come
        // macro e li trasforma in errori di sintassi (già inciampato una volta).
        const float x0 = (tl.x < br.x) ? tl.x : br.x;
        const float x1 = (tl.x < br.x) ? br.x : tl.x;
        const float z0 = (tl.z < br.z) ? tl.z : br.z;
        const float z1 = (tl.z < br.z) ? br.z : tl.z;
        if (x1 - x0 < 0.01f || z1 - z0 < 0.01f) return;
        for (float x = std::ceil(x0 / step) * step; x <= x1; x += step)
        {
            const float sx = m_imgMin.x + (x - x0) / (x1 - x0) * m_imgSize.x;
            dl->AddLine({sx, m_imgMin.y}, {sx, m_imgMin.y + m_imgSize.y}, colLine, 1.0f);
            char lbl[24]; std::snprintf(lbl, sizeof(lbl), "%.0f", x);
            dl->AddText({sx + 3.0f, m_imgMin.y + 3.0f}, IM_COL32(0,0,0,160), lbl);
            dl->AddText({sx + 2.0f, m_imgMin.y + 2.0f}, colText, lbl);
        }
        for (float z = std::ceil(z0 / step) * step; z <= z1; z += step)
        {
            // In vista dall'alto "su" sullo schermo è −Z: l'asse va ribaltato, o le
            // etichette crescerebbero al contrario rispetto a ciò che si vede.
            const float sy = m_imgMin.y + (1.0f - (z - z0) / (z1 - z0)) * m_imgSize.y;
            dl->AddLine({m_imgMin.x, sy}, {m_imgMin.x + m_imgSize.x, sy}, colLine, 1.0f);
            char lbl[24]; std::snprintf(lbl, sizeof(lbl), "%.0f", z);
            dl->AddText({m_imgMin.x + 4.0f, sy + 3.0f}, IM_COL32(0,0,0,160), lbl);
            dl->AddText({m_imgMin.x + 3.0f, sy + 2.0f}, colText, lbl);
        }
    }
}

// Schermo → punto sul piano orizzontale y = planeY. Si sproietta la matrice, quindi
// vale sia in prospettiva sia in ortografica senza casi speciali.
bool FreeCameraViewport::screenToPlane(float sx, float sy, float planeY,
                                       glm::vec3& out) const
{
    if (m_imgSize.x < 1.0f || m_imgSize.y < 1.0f) return false;
    const float nx = ((sx - m_imgMin.x) / m_imgSize.x) * 2.0f - 1.0f;
    const float ny = 1.0f - ((sy - m_imgMin.y) / m_imgSize.y) * 2.0f;

    const glm::mat4 inv = glm::inverse(m_camera->getViewProjection());
    glm::vec4 p0 = inv * glm::vec4(nx, ny, -1.0f, 1.0f);
    glm::vec4 p1 = inv * glm::vec4(nx, ny,  1.0f, 1.0f);
    if (std::fabs(p0.w) < 1e-9f || std::fabs(p1.w) < 1e-9f) return false;
    p0 /= p0.w; p1 /= p1.w;

    const glm::vec3 a{p0}, b{p1};
    const glm::vec3 d = b - a;
    // Raggio parallelo al piano: nessuna intersezione (succede di lato/di fronte).
    if (std::fabs(d.y) < 1e-6f) return false;
    const float t = (planeY - a.y) / d.y;
    out = a + d * t;
    return true;
}

void FreeCameraViewport::setRulerActive(bool on)
{
    m_rulerActive = on;
    m_rulerHasA = false;
    m_rulerFrozen = false;
    m_rulerData.clear(); m_rulerVertCount = 0;
}

// Linea A→B più due crocette agli estremi: gli estremi sono l'informazione, perché
// dicono ESATTAMENTE cosa si sta misurando.
void FreeCameraViewport::buildRulerGeometry()
{
    m_rulerData.clear();
    if (!m_rulerActive || !m_rulerHasA) { m_rulerVertCount = 0; return; }

    const float r = 0.25f, lift = 0.05f;
    auto put = [&](glm::vec3 p, float cr, float cg, float cb) {
        m_rulerData.insert(m_rulerData.end(),
                           {p.x, p.y + lift, p.z, cr, cg, cb});
    };
    auto cross = [&](glm::vec3 c) {
        put({c.x - r, c.y, c.z}, 1.0f, 0.85f, 0.25f);
        put({c.x + r, c.y, c.z}, 1.0f, 0.85f, 0.25f);
        put({c.x, c.y, c.z - r}, 1.0f, 0.85f, 0.25f);
        put({c.x, c.y, c.z + r}, 1.0f, 0.85f, 0.25f);
    };
    put(m_rulerA, 1.0f, 0.85f, 0.25f);
    put(m_rulerB, 1.0f, 0.85f, 0.25f);
    cross(m_rulerA);
    cross(m_rulerB);
    m_rulerVertCount = (int)(m_rulerData.size() / 6);
}

// ── Viste ortografiche (doc 50 M3) ──────────────────────────────────────────
// In prospettiva non si misura, si stima: una lunghezza sullo schermo non
// corrisponde a una lunghezza nel mondo. È il motivo per cui il righello di Unreal
// funziona SOLO in ortografica e per cui Hammer/Radiant lavorano su viste
// ortografiche. La vista si àncora a ciò che si stava guardando, così cambiare modo
// non fa perdere il posto.
void FreeCameraViewport::setViewMode(ViewMode m)
{
    if (m == m_viewMode) return;

    // Uscendo dalla PROSPETTIVA se ne conserva lo stato. Senza, tornandoci ci si
    // ritrovava la camera dove l'aveva messa la vista ortografica — **200 m in
    // aria**, a inquadrare il nulla, senza un modo ovvio di rimettersi a posto.
    // È il difetto segnalato dall'utente, ed è anche il motivo per cui ogni editor
    // 3D conserva la vista prospettica invece di ricalcolarla.
    if (m_viewMode == ViewMode::Perspective)
    {
        m_perspPos   = m_camera->getPosition();
        m_perspYaw   = m_camera->getYaw();
        m_perspPitch = m_camera->getPitch();
        m_perspSaved = true;
    }

    m_viewMode = m;
    if (m == ViewMode::Perspective)
    {
        m_camera->setOrthographic(false);
        if (m_perspSaved)
        {
            m_camera->setPosition(m_perspPos);
            // `lookAt` ricostruisce yaw/pitch da una direzione: la si ricava dagli
            // angoli salvati, così si torna esattamente a com'era.
            const float yr = glm::radians(m_perspYaw), pr = glm::radians(m_perspPitch);
            const glm::vec3 dir{ std::cos(yr) * std::cos(pr),
                                 std::sin(pr),
                                 std::sin(yr) * std::cos(pr) };
            m_camera->lookAt(m_perspPos + dir, {0.0f, 1.0f, 0.0f});
        }
        return;
    }

    // Il CENTRO da inquadrare: il contenuto se c'è, altrimenti ciò che si stava
    // guardando. `groundFocusPoint` usa un ripiego a distanza fissa quando la camera
    // non punta verso il basso, e quel ripiego è la seconda causa dei salti nel
    // vuoto — quindi non ci si affida più a lui da solo.
    glm::vec3 center = groundFocusPoint();
    float half = m_camera->getOrthoHalfHeight();
    if (m_contentValid)
    {
        center = (m_contentMin + m_contentMax) * 0.5f;
        half   = frameHalfHeightFor(m_contentMin, m_contentMax, m);
    }

    m_camera->setOrthographic(true, half);
    applyOrthoPlacement(m, center);
}

// In ortografica la DISTANZA non cambia l'immagine (proiezione parallela): serve
// solo a stare fuori dalla geometria. Non va però confusa col piano di taglio:
// mettendola a 500 con `far` = 500, il contenuto cadeva **esattamente oltre** il
// piano lontano e spariva. Con l'intervallo simmetrico [-far, +far] della
// proiezione, una distanza modesta lascia dentro tutto ciò che sta davanti E
// dietro alla camera. È il difetto che il collaudo ha trovato.
void FreeCameraViewport::applyOrthoPlacement(ViewMode m, const glm::vec3& center)
{
    const float dist = 100.0f;
    switch (m)
    {
        case ViewMode::Top:   // dall'alto: "su" sullo schermo = −Z nel mondo
            m_camera->setPosition({center.x, center.y + dist, center.z});
            m_camera->lookAt(center, {0.0f, 0.0f, -1.0f});
            break;
        case ViewMode::Front: // da −Z verso +Z
            m_camera->setPosition({center.x, center.y, center.z - dist});
            m_camera->lookAt(center, {0.0f, 1.0f, 0.0f});
            break;
        case ViewMode::Side:  // da −X verso +X
            m_camera->setPosition({center.x - dist, center.y, center.z});
            m_camera->lookAt(center, {0.0f, 1.0f, 0.0f});
            break;
        default: break;
    }
}

// Quanta altezza inquadrare per contenere un ingombro, con un margine del 10%:
// dipende dal modo, perché ogni vista guarda una coppia di assi diversa.
float FreeCameraViewport::frameHalfHeightFor(const glm::vec3& mn, const glm::vec3& mx,
                                             ViewMode m) const
{
    const glm::vec3 size = mx - mn;
    // L'aspect della CAMERA, non quello del pannello: è quello che proietta davvero.
    // Prenderlo dall'immagine sembrava equivalente, ma nel primo frame (e nel
    // collaudo headless) l'immagine non è ancora stata disegnata e il valore era zero.
    const float aspect = m_camera->getAspect() > 0.01f ? m_camera->getAspect() : 1.6f;
    float wNeeded = 1.0f, hNeeded = 1.0f;
    switch (m)
    {
        case ViewMode::Top:   wNeeded = size.x; hNeeded = size.z; break;
        case ViewMode::Front: wNeeded = size.x; hNeeded = size.y; break;
        case ViewMode::Side:  wNeeded = size.z; hNeeded = size.y; break;
        default: break;
    }
    // Serve contenere sia in altezza sia in larghezza: si prende il vincolo peggiore.
    const float halfByH = hNeeded * 0.5f;
    const float halfByW = (aspect > 0.01f) ? (wNeeded * 0.5f / aspect) : halfByH;
    float half = (halfByH > halfByW) ? halfByH : halfByW;
    half *= 1.10f;                       // margine: la mappa non tocca i bordi
    if (half < 2.0f) half = 2.0f;
    return half;
}

// "Inquadra tutto": il comando che ogni editor 3D ha perché perdersi è normale.
void FreeCameraViewport::frameContent()
{
    if (!m_contentValid) return;
    const glm::vec3 center = (m_contentMin + m_contentMax) * 0.5f;
    if (isOrtho())
    {
        m_camera->setOrthoHalfHeight(frameHalfHeightFor(m_contentMin, m_contentMax, m_viewMode));
        applyOrthoPlacement(m_viewMode, center);
        return;
    }
    // In prospettiva: indietreggia lungo la direzione attuale quanto basta.
    const glm::vec3 size = m_contentMax - m_contentMin;
    float radius = 0.5f * std::sqrt(size.x*size.x + size.y*size.y + size.z*size.z);
    if (radius < 1.0f) radius = 1.0f;
    const float d = radius / std::tan(glm::radians(m_camera->getFov() * 0.5f)) * 1.1f;
    const glm::vec3 dir = m_camera->getForward();
    m_camera->setPosition(center - dir * d);
    m_camera->lookAt(center, {0.0f, 1.0f, 0.0f});
}

// ── PORTAMI LÌ (doc 53 L5) ───────────────────────────────────────────────────
// Inquadra UN punto, senza toccare l'ingombro del contenuto (che serve a `F` per
// inquadrare tutto). Un elenco di problemi in cui bisogna poi cercare a mano
// l'elemento segnalato è un elenco che si smette di usare: su una mappa 300 × 200
// "il box 147" non è un indirizzo, è un enigma.
void FreeCameraViewport::focusOn(const glm::vec3& center, float radius)
{
    if (radius < 1.0f) radius = 1.0f;
    if (isOrtho())
    {
        // In ortografica si sposta l'inquadratura, non ci si avvicina: avvicinarsi
        // non vuol dire niente quando la proiezione è parallela.
        m_camera->setOrthoHalfHeight(radius * 2.5f);
        applyOrthoPlacement(m_viewMode, center);
        return;
    }
    const float d = radius / std::tan(glm::radians(m_camera->getFov() * 0.5f)) * 2.0f;
    // Si arriva da una direzione un po' dall'alto invece che dalla direzione attuale:
    // se la telecamera guardava esattamente in orizzontale, "indietreggiare" la
    // lascerebbe dentro un muro — e l'elemento segnalato resterebbe invisibile.
    const glm::vec3 dir = glm::normalize(glm::vec3(0.45f, -0.55f, 0.70f));
    m_camera->setPosition(center - dir * d);
    m_camera->lookAt(center, {0.0f, 1.0f, 0.0f});
}

void FreeCameraViewport::setContentBounds(const glm::vec3& mn, const glm::vec3& mx)
{
    m_contentMin = mn; m_contentMax = mx;
    m_contentValid = (mx.x >= mn.x && mx.y >= mn.y && mx.z >= mn.z);
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
        ImGui::SameLine();
        // "Faccia" è disponibile dove lo è la scala: sono due modi di cambiare le
        // misure, e ciò che non si può scalare non si può nemmeno tirare.
        toolBtn("Faccia", GizmoMode::Face, m_gizmoCanScale && m_boundsValid);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Tira una FACCIA: la opposta resta ferma.\n"
                              "E = tira fuori, Q = spingi dentro, di un passo di griglia.");
        ImGui::EndGroup();
        m_gizmoBarHovered = ImGui::IsItemHovered();
    }

    // Le scorciatoie 1/2/3 per la modalità del gizmo sono state RIMOSSE su richiesta
    // dell'utente (2026-08-08): *"è un impiccio ed è una scorciatoia che non utilizzo
    // quasi mai, tanto i pulsanti sono sempre facilmente raggiungibili"*.
    // Rimosse e non disattivate con un'opzione: un tasto che intercetta 1/2/3 mentre
    // si lavora è un ostacolo, e un interruttore per spegnerlo sarebbe un'altra cosa
    // da scoprire. I pulsanti della modalità restano in cima al viewport.

    // Modalità non consentita per il target corrente → ripiega su Sposta
    GizmoMode mode = m_gizmoMode;
    if (mode == GizmoMode::Rotate && !m_gizmoCanRotate) mode = GizmoMode::Translate;
    if (mode == GizmoMode::Scale  && !m_gizmoCanScale)  mode = GizmoMode::Translate;
    if (mode == GizmoMode::Face   && (!m_gizmoCanScale || !m_boundsValid))
        mode = GizmoMode::Translate;
    if (mode == GizmoMode::Face) { drawFaceGizmo(); return; }

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

    // ── Selettore del modo di vista (doc 50 M3) ──────────────────────────
    // In cima al viewport perché è un cambio di STRUMENTO, non un'opzione: si passa
    // in ortografica quando si deve misurare, e ci si torna di continuo.
    {
        struct Btn { const char* label; ViewMode mode; const char* tip; };
        static const Btn btns[] = {
            { "Prosp",  ViewMode::Perspective, "Prospettiva: per navigare e giudicare gli spazi." },
            { "Alto",   ViewMode::Top,   "Dall'alto, ortografica: la pianta. Qui le lunghezze\n"
                                         "sullo schermo SONO lunghezze nel mondo." },
            { "Fronte", ViewMode::Front, "Da davanti, ortografica: le quote e le altezze." },
            { "Lato",   ViewMode::Side,  "Di lato, ortografica: profondita' e dislivelli." },
        };
        for (const auto& b : btns)
        {
            const bool on = (m_viewMode == b.mode);
            if (on) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.20f, 0.45f, 0.75f, 1.0f));
            if (ImGui::SmallButton(b.label)) setViewMode(b.mode);
            if (on) ImGui::PopStyleColor();
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", b.tip);
            ImGui::SameLine();
        }
        // "Inquadra tutto": il rimedio al perdersi, presente in ogni editor 3D
        // proprio perché perdersi è normale. Anche col tasto F.
        if (ImGui::SmallButton(m_showMeasures ? "Misure ON" : "Misure OFF"))
            m_showMeasures = !m_showMeasures;
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Barra di scala e coordinate ai bordi, sempre in vista.\n"
                              "Le coordinate compaiono solo in ortografica: in\n"
                              "prospettiva la scala cambia con la profondita' e una\n"
                              "tacca \"ogni 10 m\" sarebbe una bugia.");
        ImGui::SameLine();
        if (ImGui::SmallButton("Inquadra")) frameContent();
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Riporta la camera a inquadrare TUTTA la mappa.\n"
                              "Scorciatoia: F. Da usare ogni volta che ti perdi.");
        ImGui::SameLine();

        // ── Righello (M4) ────────────────────────────────────────────────
        if (m_rulerActive)
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.75f, 0.55f, 0.15f, 1.0f));
        if (ImGui::SmallButton("Righello")) setRulerActive(!m_rulerActive);
        if (m_rulerActive) ImGui::PopStyleColor();
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Due clic sul terreno: misura la distanza fra due punti\n"
                              "QUALSIASI, anche nel vuoto — la larghezza di un varco,\n"
                              "la luce di un passaggio. Aggancio alla griglia.\n"
                              "Il terzo clic ricomincia. In ortografica e' piu' preciso.");
        ImGui::SameLine();

        if (isOrtho())
        {
            // L'ampiezza inquadrata, in metri: è la scala della vista, e senza di
            // essa "quanto sto guardando" resta una sensazione.
            ImGui::TextColored(ImVec4(0.55f, 0.85f, 1.0f, 1.0f),
                               "| inquadratura %.0f m di altezza",
                               m_camera->getOrthoHalfHeight() * 2.0f);
        }
        else ImGui::TextDisabled("| vel: %.0f", m_camSpeed);

        // La misura, con il confronto normativo detto invece che lasciato a mente.
        if (m_rulerActive && m_rulerHasA)
        {
            const glm::vec3 d = m_rulerB - m_rulerA;
            const float horiz = std::sqrt(d.x * d.x + d.z * d.z);
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.25f, 1.0f),
                               "|  %.2f m   (dX %.2f  dZ %.2f)", horiz, d.x, d.z);
            const char* verdict = nullptr;
            if (horiz > 0.01f)
            {
                if (horiz < mini::mapmetrics::DOOR_WIDTH)      verdict = "sotto la porta (1,80)";
                else if (horiz < mini::mapmetrics::CORRIDOR_MIN) verdict = "sotto il corridoio (2,40)";
            }
            if (verdict)
            {
                ImGui::SameLine();
                ImGui::TextColored(ImVec4(0.95f, 0.65f, 0.30f, 1.0f), "%s", verdict);
            }
        }
    }

    if (isOrtho())
        ImGui::TextDisabled(
            "Tasto destro o centrale = sposta  |  Rotella = ingrandisci");
    else
        ImGui::TextDisabled(
            "Tasto destro = guarda + WASD/QE vola (Shift veloce, rotella = velocita')  |  "
            "Rotella = zoom  |  Tasto centrale = pan");

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

    drawMeasureOverlay();

    const bool imgHovered = ImGui::IsItemHovered();
    ImGuiIO& io = ImGui::GetIO();

    // ── Navigazione stile Unreal ──────────────────────────────────────
    // RMB tenuto: mouselook (+ WASD/QE in tick); rotella regola la velocità.
    // Senza RMB: rotella = dolly avanti/indietro; MMB drag = pan.
    // ── In ORTOGRAFICA: si sposta e si ingrandisce, non si ruota ─────────
    // Ruotare una vista ortografica assiale la disallinea dagli assi, e con essa
    // perde senso l'unica cosa per cui esiste: misurare. Quindi il tasto destro
    // TRASCINA invece di girare, e la rotella cambia l'inquadratura invece della
    // velocità di volo.
    // ── Ctrl+rotella = PASSO DI GRIGLIA, in tutte le viste ────────────────
    // Prima di ogni altro uso della rotella, e la CONSUMA: se cambiasse anche lo
    // zoom, un gesto solo farebbe due cose e nessuna delle due in modo prevedibile.
    // È la convenzione di CubeGrid e TrenchBroom, ed è il parametro che si cambia
    // più spesso costruendo (grande per le stanze, piccolo per la rifinitura).
    if (imgHovered && io.KeyCtrl && io.MouseWheel != 0.0f)
    {
        m_gridStepReq = (io.MouseWheel > 0.0f) ? 1 : -1;
        io.MouseWheel = 0.0f;
    }

    if (isOrtho())
    {
        m_rmbLook = false;
        if (imgHovered)
        {
            const float h = m_camera->getOrthoHalfHeight();
            if (io.MouseWheel != 0.0f)
                m_camera->setOrthoHalfHeight(h * (1.0f - 0.15f * io.MouseWheel));
            // Lo spostamento è proporzionale allo zoom: a mappa intera si copre
            // molta distanza, da vicino si rifinisce. Il fattore lega i pixel ai
            // metri inquadrati, così il gesto "segue" il cursore a ogni scala.
            const float k = h / 300.0f;
            if (ImGui::IsMouseDragging(ImGuiMouseButton_Right)
             || ImGui::IsMouseDragging(ImGuiMouseButton_Middle))
            {
                panCamera(-io.MouseDelta.x * k, io.MouseDelta.y * k);
                ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeAll);
            }
        }
    }
    else
    {
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
    }   // fine ramo prospettiva

    // ── RIGHELLO (doc 50 M4) ─────────────────────────────────────────────
    // Primo clic fissa A, il secondo congela B. Finché B non è congelato segue il
    // cursore, così la misura si legge MENTRE si cerca il punto, non dopo.
    if (m_rulerActive && imgHovered)
    {
        const ImVec2 mp = ImGui::GetMousePos();
        glm::vec3 hit;
        if (screenToPlane(mp.x, mp.y, 0.0f, hit))
        {
            if (m_rulerSnap > 0.001f)
            {
                hit.x = std::round(hit.x / m_rulerSnap) * m_rulerSnap;
                hit.z = std::round(hit.z / m_rulerSnap) * m_rulerSnap;
            }
            if (!m_rulerFrozen)
            {
                if (!m_rulerHasA) m_rulerA = hit;
                m_rulerB = hit;
            }
            if (ImGui::IsMouseClicked(ImGuiMouseButton_Left))
            {
                if (!m_rulerHasA) { m_rulerA = hit; m_rulerB = hit; m_rulerHasA = true; }
                else if (!m_rulerFrozen) { m_rulerB = hit; m_rulerFrozen = true; }
                else { m_rulerA = hit; m_rulerB = hit; m_rulerFrozen = false; }
            }
        }
    }
    if (m_rulerActive) buildRulerGeometry();

    // Disegna box. La soppressione del clic di selezione sta più sotto, DOPO che
    // `m_imgClicked` è stato posato: qui verrebbe sovrascritta subito.
    drawBoxTool(imgHovered);

    // F = inquadra tutto. Mai mentre si scrive in un campo, o "F" diventerebbe
    // un salto di camera invece di una lettera.
    if (imgHovered && !ImGui::GetIO().WantTextInput
        && ImGui::IsKeyPressed(ImGuiKey_F, false))
        frameContent();

    // ── E / Q: spingi e tira la faccia attiva di UN passo ─────────────────
    // Costruzione da tastiera, ripetibile e misurabile: tre pressioni = tre passi,
    // esatti. Col mouse la stessa cosa richiede di mirare una maniglia.
    // Solo quando NON si sta volando: in volo E/Q sono salita e discesa, ed erano
    // lì prima. Un tasto che cambia significato senza dirlo è peggio di due tasti.
    {
        const bool flying = m_mouseCapture || m_rmbLook;
        if (imgHovered && !flying && !io.WantTextInput && m_gizmoEnabled
            && m_gizmoMode == GizmoMode::Face && m_boundsValid)
        {
            const float step = (m_rulerSnap > 0.001f) ? m_rulerSnap : 0.5f;
            if (ImGui::IsKeyPressed(ImGuiKey_E, false))
            { m_facePending += step;  m_faceHas = true; }
            if (ImGui::IsKeyPressed(ImGuiKey_Q, false))
            { m_facePending -= step;  m_faceHas = true; }
        }
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
    // Con "Disegna box" acceso il clic sinistro APPARTIENE allo strumento: se
    // selezionasse anche, ogni rettangolo tracciato cambierebbe la selezione sotto,
    // e il pannello di destra mostrerebbe un oggetto a caso a fine gesto.
    if (m_drawActive) m_imgClicked = false;
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
        const auto& raw = mesh.getVertexData();
        // Il conteggio dichiarato non può promettere più vertici di quanti ne
        // contenga `raw`: sotto c'è `raw.data() + i*11`, cioè una lettura fuori
        // limite del NOSTRO codice se i due divergono. Oggi il costruttore da
        // `vector<Vertex>` li tiene allineati, ma è un'invariante che nessuno
        // impone — e `Mesh(vector<float>, int)` si fida di chi lo chiama.
        const int avail = (int)(raw.size() / 11u);
        // Confronto esplicito e non `std::min`: <windows.h> definisce `min` come
        // macro e la trasformerebbe in un errore di sintassi.
        const int declared = mesh.getVertexCount();
        const int count = (declared < avail) ? declared : avail;
        if (count < declared)
            std::fprintf(stderr,
                "[Viewport] modello incoerente: %d vertici dichiarati, %d "
                "disponibili — uso i disponibili (KI #98).\n", declared, avail);
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

// ── Overlay navmesh ──────────────────────────────────────────────────────────
// I triangoli arrivano già in coordinate mondo e già colorati: qui si aggiunge
// solo un rialzo di 4 cm per non finire dentro il pavimento, e si costruiscono
// gli spigoli, che sono ciò che rende leggibile dove il navmesh **finisce** —
// il bordo è l'informazione, non il riempimento.
void FreeCameraViewport::setNavMesh(const std::vector<NavTriDraw>& tris)
{
    constexpr float kLift = 0.04f;
    m_navFillData.clear(); m_navEdgeData.clear();
    m_navFillData.reserve(tris.size() * 18);
    m_navEdgeData.reserve(tris.size() * 36);

    auto put = [](std::vector<float>& v, float x, float y, float z,
                  float r, float g, float b)
    { v.push_back(x); v.push_back(y + kLift); v.push_back(z);
      v.push_back(r); v.push_back(g); v.push_back(b); };

    for (const auto& t : tris)
    {
        put(m_navFillData, t.ax, t.ay, t.az, t.r, t.g, t.b);
        put(m_navFillData, t.bx, t.by, t.bz, t.r, t.g, t.b);
        put(m_navFillData, t.cx, t.cy, t.cz, t.r, t.g, t.b);
        // Spigoli più scuri della faccia: si distinguono i poligoni fra loro.
        const float er = t.r * 0.45f, eg = t.g * 0.45f, eb = t.b * 0.45f;
        put(m_navEdgeData, t.ax, t.ay, t.az, er, eg, eb);
        put(m_navEdgeData, t.bx, t.by, t.bz, er, eg, eb);
        put(m_navEdgeData, t.bx, t.by, t.bz, er, eg, eb);
        put(m_navEdgeData, t.cx, t.cy, t.cz, er, eg, eb);
        put(m_navEdgeData, t.cx, t.cy, t.cz, er, eg, eb);
        put(m_navEdgeData, t.ax, t.ay, t.az, er, eg, eb);
    }
    m_navFillVertCount = (int)(m_navFillData.size() / 6);
    m_navEdgeVertCount = (int)(m_navEdgeData.size() / 6);
}

void FreeCameraViewport::clearNavMesh()
{
    m_navFillData.clear(); m_navFillVertCount = 0;
    m_navEdgeData.clear(); m_navEdgeVertCount = 0;
}

} // namespace editor
