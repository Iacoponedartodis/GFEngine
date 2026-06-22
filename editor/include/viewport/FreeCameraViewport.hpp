#pragma once
#include <memory>
#include <glm/glm.hpp>

namespace mini { class Camera; }

namespace editor
{

class FreeCameraViewport
{
public:
    FreeCameraViewport();
    ~FreeCameraViewport();

    void tick(float dt);
    void draw();

private:
    // ── FBO ──────────────────────────────────────────────────────────
    unsigned int m_fbo=0, m_colorTex=0, m_depthRbo=0;
    int m_fbWidth=0, m_fbHeight=0;
    void resizeFBO(int w, int h);

    // ── Shader (compilato localmente, no mini::Shader) ────────────────
    unsigned int m_shaderProgram = 0;

    // ── Grid ─────────────────────────────────────────────────────────
    unsigned int m_gridVAO=0, m_gridVBO=0;
    int          m_gridVertCount=0;
    void buildGrid(float size, int div);
    void drawGrid(const glm::mat4& vp);
    void renderScene();

    // ── Camera ───────────────────────────────────────────────────────
    std::unique_ptr<mini::Camera> m_camera;
    float m_camSpeed  = 8.0f;
    bool  m_focused   = false;
    bool  m_mouseCapture = false;
    bool  m_tabWasDown   = false;
};

} // namespace editor