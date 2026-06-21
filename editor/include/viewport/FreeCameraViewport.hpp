#pragma once

#include <memory>
#include <glm/glm.hpp>

namespace mini { class Camera; class Shader; }

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
    unsigned int m_fbo = 0;
    unsigned int m_colorTex = 0;
    unsigned int m_depthRbo = 0;
    int m_fbWidth = 0;
    int m_fbHeight = 0;

    void resizeFBO(int w, int h);
    void renderScene();

    std::unique_ptr<mini::Camera> m_camera;
    bool m_focused = false;
    bool m_mouseCapture = false;
    bool m_tabWasDown = false;
    float m_camSpeed = 8.0f;

    std::unique_ptr<mini::Shader> m_shader;
    unsigned int m_gridVAO = 0;
    unsigned int m_gridVBO = 0;
    int m_gridVertCount = 0;

    void buildGrid(float size, int divisions);
    void drawGrid(const glm::mat4& vp);
};

} // namespace editor