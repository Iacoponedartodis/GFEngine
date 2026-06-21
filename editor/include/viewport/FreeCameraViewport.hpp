#pragma once
#include <memory>
#include <vector>
#include <glm/glm.hpp>

namespace mini { class Camera; class Mesh; class Shader; }
struct ImVec2;

namespace editor
{

// Viewport 3D con camera libera. Sostituisce il volo libero rimosso dal gioco.
//
// Funzionalità Stage 1:
//   - Framebuffer offscreen → ImGui::Image
//   - Camera free-fly con WASD + mouse (quando il pannello è focused)
//   - Griglia di riferimento a pavimento
//   - Rendering placeholder (cubo grid + future mesh mappa)
//
// Funzionalità future (Stage 2+):
//   - Caricamento mappa da MapDef
//   - Overlay cover/spawn/patrol point
//   - Selezione entità con ray-casting
//   - Gizmo (ImGuizmo)
class FreeCameraViewport
{
public:
    FreeCameraViewport();
    ~FreeCameraViewport();

    void tick(float dt);
    void draw(); // chiamato dentro ImGui window

private:
    // ── Framebuffer offscreen ─────────────────────────────────────────
    unsigned int m_fbo         = 0;
    unsigned int m_colorTex    = 0;
    unsigned int m_depthRbo    = 0;
    int          m_fbWidth     = 0;
    int          m_fbHeight    = 0;

    void resizeFBO(int w, int h);
    void renderScene();

    // ── Camera ────────────────────────────────────────────────────────
    std::unique_ptr<mini::Camera> m_camera;
    bool  m_focused       = false;
    bool  m_mouseCapture  = false;
    float m_camSpeed      = 8.0f;

    // ── Shader + geometry ─────────────────────────────────────────────
    std::unique_ptr<mini::Shader> m_shader;
    unsigned int m_gridVAO = 0;
    unsigned int m_gridVBO = 0;
    int          m_gridVertCount = 0;

    void buildGrid(float size, int divisions);
    void drawGrid(const glm::mat4& vp);

    // Input state
    bool m_keyW=false, m_keyS=false, m_keyA=false, m_keyD=false;
    bool m_keyQ=false, m_keyE=false; // su/giù
    int  m_mouseDX=0, m_mouseDY=0;

    void handleInput();
};

} // namespace editor