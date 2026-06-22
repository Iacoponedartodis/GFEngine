#include "viewport/FreeCameraViewport.hpp"

// OpenGL.hpp PRIMA: definisce GLuint, GLenum, ecc.
#include <mini/platform/OpenGL.hpp>
#include <SDL2/SDL.h>

// ── Costanti FBO ─────────────────────────────────────────────────────────
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
#ifndef GL_VERTEX_SHADER
  #define GL_VERTEX_SHADER            0x8B31
#endif
#ifndef GL_FRAGMENT_SHADER
  #define GL_FRAGMENT_SHADER          0x8B30
#endif
#ifndef GL_COMPILE_STATUS
  #define GL_COMPILE_STATUS           0x8B81
#endif
#ifndef GL_LINK_STATUS
  #define GL_LINK_STATUS              0x8B82
#endif
#ifndef GL_INFO_LOG_LENGTH
  #define GL_INFO_LOG_LENGTH          0x8B84
#endif
#ifndef GL_ARRAY_BUFFER
  #define GL_ARRAY_BUFFER             0x8892
#endif
#ifndef GL_STATIC_DRAW
  #define GL_STATIC_DRAW              0x88B4
#endif

// ── Tutti i puntatori FBO + Shader caricati localmente ───────────────────
typedef void   (*PFN_GenFBO)   (GLsizei,GLuint*);
typedef void   (*PFN_BindFBO)  (GLenum,GLuint);
typedef void   (*PFN_DelFBO)   (GLsizei,const GLuint*);
typedef void   (*PFN_FBOTex)   (GLenum,GLenum,GLenum,GLuint,GLint);
typedef void   (*PFN_FBORBO)   (GLenum,GLenum,GLenum,GLuint);
typedef GLenum (*PFN_CheckFBO) (GLenum);
typedef void   (*PFN_GenRBO)   (GLsizei,GLuint*);
typedef void   (*PFN_BindRBO)  (GLenum,GLuint);
typedef void   (*PFN_DelRBO)   (GLsizei,const GLuint*);
typedef void   (*PFN_RBOStore) (GLenum,GLenum,GLsizei,GLsizei);

typedef GLuint (*PFN_CreateShader)  (GLenum);
typedef void   (*PFN_ShaderSource)  (GLuint,GLsizei,const char**,const GLint*);
typedef void   (*PFN_CompileShader) (GLuint);
typedef void   (*PFN_GetShaderiv)   (GLuint,GLenum,GLint*);
typedef void   (*PFN_GetShaderLog)  (GLuint,GLsizei,GLsizei*,char*);
typedef GLuint (*PFN_CreateProgram) ();
typedef void   (*PFN_AttachShader)  (GLuint,GLuint);
typedef void   (*PFN_BindAttribLoc) (GLuint,GLuint,const char*);
typedef void   (*PFN_LinkProgram)   (GLuint);
typedef void   (*PFN_GetProgramiv)  (GLuint,GLenum,GLint*);
typedef void   (*PFN_GetProgramLog) (GLuint,GLsizei,GLsizei*,char*);
typedef void   (*PFN_UseProgram)    (GLuint);
typedef void   (*PFN_DeleteShader)  (GLuint);
typedef void   (*PFN_DeleteProgram) (GLuint);
typedef GLint  (*PFN_GetUniformLoc) (GLuint,const char*);
typedef void   (*PFN_Uniform4fv)    (GLint,GLsizei,const GLfloat*);
typedef void   (*PFN_UniformMat4)   (GLint,GLsizei,GLboolean,const GLfloat*);
typedef void   (*PFN_GenVAO)        (GLsizei,GLuint*);
typedef void   (*PFN_BindVAO)       (GLuint);
typedef void   (*PFN_DelVAO)        (GLsizei,const GLuint*);
typedef void   (*PFN_GenBuf)        (GLsizei,GLuint*);
typedef void   (*PFN_BindBuf)       (GLenum,GLuint);
typedef void   (*PFN_DelBuf)        (GLsizei,const GLuint*);
typedef void   (*PFN_BufData)       (GLenum,GLsizeiptr,const void*,GLenum);
typedef void   (*PFN_EnableVAttrib) (GLuint);
typedef void   (*PFN_VAttribPtr)    (GLuint,GLint,GLenum,GLboolean,GLsizei,const void*);

static PFN_GenFBO       s_genFBO    = nullptr; static PFN_BindFBO  s_bindFBO = nullptr;
static PFN_DelFBO       s_delFBO    = nullptr; static PFN_FBOTex   s_fboTex  = nullptr;
static PFN_FBORBO       s_fboRBO    = nullptr; static PFN_CheckFBO s_chkFBO  = nullptr;
static PFN_GenRBO       s_genRBO    = nullptr; static PFN_BindRBO  s_bindRBO = nullptr;
static PFN_DelRBO       s_delRBO    = nullptr; static PFN_RBOStore s_rboSt   = nullptr;
static PFN_CreateShader s_crSh      = nullptr; static PFN_ShaderSource s_shSrc= nullptr;
static PFN_CompileShader s_compSh   = nullptr; static PFN_GetShaderiv s_gShiv = nullptr;
static PFN_GetShaderLog s_gShLog    = nullptr; static PFN_CreateProgram s_crPr= nullptr;
static PFN_AttachShader s_attSh     = nullptr; static PFN_BindAttribLoc s_bAL = nullptr;
static PFN_LinkProgram  s_lnkPr     = nullptr; static PFN_GetProgramiv s_gPiv = nullptr;
static PFN_GetProgramLog s_gPLog    = nullptr; static PFN_UseProgram  s_usePr = nullptr;
static PFN_DeleteShader s_delSh     = nullptr; static PFN_DeleteProgram s_delPr= nullptr;
static PFN_GetUniformLoc s_gULoc    = nullptr; static PFN_UniformMat4 s_uMat4 = nullptr;
static PFN_GenVAO       s_genVAO    = nullptr; static PFN_BindVAO  s_bindVAO  = nullptr;
static PFN_DelVAO       s_delVAO    = nullptr; static PFN_GenBuf   s_genBuf   = nullptr;
static PFN_BindBuf      s_bindBuf   = nullptr; static PFN_DelBuf   s_delBuf   = nullptr;
static PFN_BufData      s_bufData   = nullptr;
static PFN_EnableVAttrib s_enVA     = nullptr; static PFN_VAttribPtr s_vAPtr   = nullptr;

#define GETGL(T, N) T N = (T)SDL_GL_GetProcAddress(#N)

static bool s_glLoaded = false;
static void loadAllGLFuncs()
{
    if (s_glLoaded) return; s_glLoaded = true;
    s_genFBO  = (PFN_GenFBO)  SDL_GL_GetProcAddress("glGenFramebuffers");
    s_bindFBO = (PFN_BindFBO) SDL_GL_GetProcAddress("glBindFramebuffer");
    s_delFBO  = (PFN_DelFBO)  SDL_GL_GetProcAddress("glDeleteFramebuffers");
    s_fboTex  = (PFN_FBOTex)  SDL_GL_GetProcAddress("glFramebufferTexture2D");
    s_fboRBO  = (PFN_FBORBO)  SDL_GL_GetProcAddress("glFramebufferRenderbuffer");
    s_chkFBO  = (PFN_CheckFBO)SDL_GL_GetProcAddress("glCheckFramebufferStatus");
    s_genRBO  = (PFN_GenRBO)  SDL_GL_GetProcAddress("glGenRenderbuffers");
    s_bindRBO = (PFN_BindRBO) SDL_GL_GetProcAddress("glBindRenderbuffer");
    s_delRBO  = (PFN_DelRBO)  SDL_GL_GetProcAddress("glDeleteRenderbuffers");
    s_rboSt   = (PFN_RBOStore)SDL_GL_GetProcAddress("glRenderbufferStorage");
    s_crSh    = (PFN_CreateShader)  SDL_GL_GetProcAddress("glCreateShader");
    s_shSrc   = (PFN_ShaderSource)  SDL_GL_GetProcAddress("glShaderSource");
    s_compSh  = (PFN_CompileShader) SDL_GL_GetProcAddress("glCompileShader");
    s_gShiv   = (PFN_GetShaderiv)   SDL_GL_GetProcAddress("glGetShaderiv");
    s_gShLog  = (PFN_GetShaderLog)  SDL_GL_GetProcAddress("glGetShaderInfoLog");
    s_crPr    = (PFN_CreateProgram) SDL_GL_GetProcAddress("glCreateProgram");
    s_attSh   = (PFN_AttachShader)  SDL_GL_GetProcAddress("glAttachShader");
    s_bAL     = (PFN_BindAttribLoc) SDL_GL_GetProcAddress("glBindAttribLocation");
    s_lnkPr   = (PFN_LinkProgram)   SDL_GL_GetProcAddress("glLinkProgram");
    s_gPiv    = (PFN_GetProgramiv)  SDL_GL_GetProcAddress("glGetProgramiv");
    s_gPLog   = (PFN_GetProgramLog) SDL_GL_GetProcAddress("glGetProgramInfoLog");
    s_usePr   = (PFN_UseProgram)    SDL_GL_GetProcAddress("glUseProgram");
    s_delSh   = (PFN_DeleteShader)  SDL_GL_GetProcAddress("glDeleteShader");
    s_delPr   = (PFN_DeleteProgram) SDL_GL_GetProcAddress("glDeleteProgram");
    s_gULoc   = (PFN_GetUniformLoc) SDL_GL_GetProcAddress("glGetUniformLocation");
    s_uMat4   = (PFN_UniformMat4)   SDL_GL_GetProcAddress("glUniformMatrix4fv");
    s_genVAO  = (PFN_GenVAO)        SDL_GL_GetProcAddress("glGenVertexArrays");
    s_bindVAO = (PFN_BindVAO)       SDL_GL_GetProcAddress("glBindVertexArray");
    s_delVAO  = (PFN_DelVAO)        SDL_GL_GetProcAddress("glDeleteVertexArrays");
    s_genBuf  = (PFN_GenBuf)        SDL_GL_GetProcAddress("glGenBuffers");
    s_bindBuf = (PFN_BindBuf)       SDL_GL_GetProcAddress("glBindBuffer");
    s_delBuf  = (PFN_DelBuf)        SDL_GL_GetProcAddress("glDeleteBuffers");
    s_bufData = (PFN_BufData)       SDL_GL_GetProcAddress("glBufferData");
    s_enVA    = (PFN_EnableVAttrib) SDL_GL_GetProcAddress("glEnableVertexAttribArray");
    s_vAPtr   = (PFN_VAttribPtr)    SDL_GL_GetProcAddress("glVertexAttribPointer");

    SDL_Log("[Viewport] GL functions loaded: FBO=%s Shader=%s VAO=%s",
        s_genFBO?"OK":"FAIL", s_crSh?"OK":"FAIL", s_genVAO?"OK":"FAIL");
}

// ── Compile shader con error log ─────────────────────────────────────────
static GLuint compileShader(GLenum type, const char* src)
{
    if (!s_crSh || !s_shSrc || !s_compSh || !s_gShiv) return 0;
    GLuint sh = s_crSh(type);
    s_shSrc(sh, 1, &src, nullptr);
    s_compSh(sh);
    GLint ok = 0; s_gShiv(sh, GL_COMPILE_STATUS, &ok);
    if (!ok && s_gShLog) {
        GLint len=0; s_gShiv(sh, GL_INFO_LOG_LENGTH, &len);
        if (len>0) { char* log=new char[len]; s_gShLog(sh,len,nullptr,log);
            SDL_Log("[Viewport] Shader error: %s", log); delete[] log; }
        return 0;
    }
    return sh;
}

static GLuint buildProgram(const char* vs, const char* fs)
{
    GLuint v = compileShader(GL_VERTEX_SHADER, vs);
    GLuint f = compileShader(GL_FRAGMENT_SHADER, fs);
    if (!v || !f || !s_crPr) return 0;
    GLuint p = s_crPr();
    // Bind attrib locations BEFORE link
    if (s_bAL) { s_bAL(p, 0, "aPos"); s_bAL(p, 1, "aCol"); }
    s_attSh(p, v); s_attSh(p, f);
    if (s_lnkPr) s_lnkPr(p);
    GLint ok=0; if (s_gPiv) s_gPiv(p, GL_LINK_STATUS, &ok);
    if (!ok && s_gPLog) {
        GLint len=0; s_gPiv(p, GL_INFO_LOG_LENGTH, &len);
        if (len>0) { char* log=new char[len]; s_gPLog(p,len,nullptr,log);
            SDL_Log("[Viewport] Program link error: %s", log); delete[] log; }
        if (s_delPr) s_delPr(p); return 0;
    }
    if (s_delSh) { s_delSh(v); s_delSh(f); }
    SDL_Log("[Viewport] Shader program %u OK", p);
    return p;
}

// ── includes standard ─────────────────────────────────────────────────────
#include "viewport/FreeCameraViewport.hpp"
#include <mini/render/Camera.hpp>
#include <imgui.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <vector>
#include <iostream>

namespace editor
{

static const char* k_vert = R"(
#version 130
attribute vec3 aPos;
attribute vec3 aCol;
uniform mat4 uVP;
varying vec3 vCol;
void main() { gl_Position = uVP * vec4(aPos, 1.0); vCol = aCol; }
)";

static const char* k_frag = R"(
#version 130
varying vec3 vCol;
void main() { gl_FragColor = vec4(vCol, 1.0); }
)";

FreeCameraViewport::FreeCameraViewport()
{
    loadAllGLFuncs();

    m_camera = std::make_unique<mini::Camera>(60.0f, 16.0f/9.0f, 0.1f, 500.0f);
    m_camera->setPosition({0.0f, 8.0f, 14.0f});
    m_camera->lookAt({0.0f, 0.0f, 0.0f});
    m_camera->setSpeed(m_camSpeed);

    m_shaderProgram = buildProgram(k_vert, k_frag);
    buildGrid(40.0f, 20);
    resizeFBO(4, 4);
}

FreeCameraViewport::~FreeCameraViewport()
{
    if (m_fbo && s_delFBO)      s_delFBO(1, &m_fbo);
    if (m_depthRbo && s_delRBO) s_delRBO(1, &m_depthRbo);
    if (m_colorTex)             glDeleteTextures(1, &m_colorTex);
    if (m_gridVAO && s_delVAO)  s_delVAO(1, &m_gridVAO);
    if (m_gridVBO && s_delBuf)  s_delBuf(1, &m_gridVBO);
    if (m_shaderProgram && s_delPr) s_delPr(m_shaderProgram);
}

void FreeCameraViewport::resizeFBO(int w, int h)
{
    if (w == m_fbWidth && h == m_fbHeight) return;
    if (!s_genFBO) return;
    m_fbWidth = w; m_fbHeight = h;
    if (m_fbo)      { s_delFBO(1,&m_fbo);     m_fbo=0; }
    if (m_depthRbo) { s_delRBO(1,&m_depthRbo); m_depthRbo=0; }
    if (m_colorTex) { glDeleteTextures(1,&m_colorTex); m_colorTex=0; }
    s_genFBO(1,&m_fbo); s_bindFBO(GL_FRAMEBUFFER, m_fbo);
    glGenTextures(1,&m_colorTex); glBindTexture(GL_TEXTURE_2D, m_colorTex);
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,w,h,0,GL_RGBA,GL_UNSIGNED_BYTE,nullptr);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    s_fboTex(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,m_colorTex,0);
    s_genRBO(1,&m_depthRbo); s_bindRBO(GL_RENDERBUFFER,m_depthRbo);
    s_rboSt(GL_RENDERBUFFER,GL_DEPTH24_STENCIL8,w,h);
    s_fboRBO(GL_FRAMEBUFFER,GL_DEPTH_STENCIL_ATTACHMENT,GL_RENDERBUFFER,m_depthRbo);
    if (s_chkFBO(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        SDL_Log("[Viewport] FBO incompleto!");
    s_bindFBO(GL_FRAMEBUFFER,0); glBindTexture(GL_TEXTURE_2D,0);
}

void FreeCameraViewport::buildGrid(float size, int div)
{
    std::vector<float> verts;
    float step=size/div, half=size*0.5f;
    auto ln=[&](float x0,float y0,float z0,float x1,float y1,float z1,float r,float g,float b)
    { verts.insert(verts.end(),{x0,y0,z0,r,g,b,x1,y1,z1,r,g,b}); };
    for(int i=0;i<=div;++i){
        float t=-half+i*step; float c=(i==div/2)?0.6f:0.28f;
        ln(-half,0,t,half,0,t,c,c,c); ln(t,0,-half,t,0,half,c,c,c);
    }
    ln(-half,0.02f,0,half,0.02f,0, 0.90f,0.20f,0.20f); // X rosso
    ln(0,0.02f,-half,0,0.02f,half, 0.20f,0.45f,0.95f); // Z blu
    m_gridVertCount=(int)(verts.size()/6);
    if(!s_genVAO||!s_genBuf) return;
    s_genVAO(1,&m_gridVAO); s_genBuf(1,&m_gridVBO);
    s_bindVAO(m_gridVAO);
    s_bindBuf(GL_ARRAY_BUFFER,m_gridVBO);
    s_bufData(GL_ARRAY_BUFFER,verts.size()*sizeof(float),verts.data(),GL_STATIC_DRAW);
    s_enVA(0); s_vAPtr(0,3,GL_FLOAT,GL_FALSE,6*sizeof(float),(void*)0);
    s_enVA(1); s_vAPtr(1,3,GL_FLOAT,GL_FALSE,6*sizeof(float),(void*)(3*sizeof(float)));
    s_bindVAO(0);
}

void FreeCameraViewport::drawGrid(const glm::mat4& vp)
{
    if (!m_shaderProgram || !m_gridVAO || !s_usePr) return;
    s_usePr(m_shaderProgram);
    GLint loc = s_gULoc ? s_gULoc(m_shaderProgram,"uVP") : -1;
    if (loc>=0 && s_uMat4) s_uMat4(loc,1,GL_FALSE,glm::value_ptr(vp));
    s_bindVAO(m_gridVAO);
    glDrawArrays(GL_LINES,0,m_gridVertCount);
    s_bindVAO(0);
    s_usePr(0);
}

void FreeCameraViewport::renderScene()
{
    if (!m_fbo || !s_bindFBO) return;
    GLint oldVP[4]; glGetIntegerv(GL_VIEWPORT,oldVP);
    s_bindFBO(GL_FRAMEBUFFER,m_fbo);
    glViewport(0,0,m_fbWidth,m_fbHeight);
    glDisable(GL_DEPTH_TEST);
    glClearColor(0.08f,0.10f,0.16f,1.0f);
    glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
    drawGrid(m_camera->getViewProjection());
    s_bindFBO(GL_FRAMEBUFFER,0);
    glViewport(oldVP[0],oldVP[1],oldVP[2],oldVP[3]);
}

void FreeCameraViewport::tick(float dt)
{
    if (!m_focused) return;
    const Uint8* ks = SDL_GetKeyboardState(nullptr);
    bool tabNow = ks[SDL_SCANCODE_TAB]!=0;
    if (tabNow && !m_tabWasDown) {
        m_mouseCapture=!m_mouseCapture;
        SDL_SetRelativeMouseMode(m_mouseCapture?SDL_TRUE:SDL_FALSE);
    }
    m_tabWasDown=tabNow;
    m_camera->setSpeed(ks[SDL_SCANCODE_LSHIFT]?m_camSpeed*3.0f:m_camSpeed);
    m_camera->processKeyboard(
        ks[SDL_SCANCODE_W]!=0, ks[SDL_SCANCODE_S]!=0,
        ks[SDL_SCANCODE_A]!=0, ks[SDL_SCANCODE_D]!=0,
        ks[SDL_SCANCODE_E]||ks[SDL_SCANCODE_SPACE],
        ks[SDL_SCANCODE_Q]||ks[SDL_SCANCODE_LCTRL], dt);
    if (m_mouseCapture) {
        int dx=0,dy=0; SDL_GetRelativeMouseState(&dx,&dy);
        if (dx||dy) m_camera->processMouse((float)dx,(float)dy,0.15f);
    }
}

void FreeCameraViewport::draw()
{
    const ImVec2 avail = ImGui::GetContentRegionAvail();
    ImGui::TextDisabled(m_mouseCapture
        ? "TAB = rilascia mouse  |  WASD = muovi  |  E/Q = su/giu  |  Shift = veloce"
        : "TAB = cattura mouse   |  WASD = muovi  |  E/Q = su/giu  |  Shift = veloce");
    ImGui::Separator();

    m_focused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);

    const ImVec2 avail2 = ImGui::GetContentRegionAvail();
    const int w = (int)avail2.x;
    const int h = (int)avail2.y;

    if (w>8 && h>8) {
        m_camera->setAspect((float)w/(float)h);
        resizeFBO(w,h);
        renderScene();
        if (m_colorTex)
            ImGui::Image((ImTextureID)(uintptr_t)m_colorTex,
                ImVec2((float)w,(float)h),ImVec2(0,1),ImVec2(1,0));
        else
            ImGui::TextColored({1,0.4f,0.4f,1},"FBO non disponibile.");
    }
}

} // namespace editor