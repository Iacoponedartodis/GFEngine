#pragma once

#include <memory>

namespace mini
{

class Window;
class Shader;
class Camera;

struct ClearColor
{
    float r = 0.04f;
    float g = 0.04f;
    float b = 0.10f;
    float a = 1.0f;
};

class Renderer
{
public:
    explicit Renderer(Window& window);
    ~Renderer();

    Renderer(const Renderer&)            = delete;
    Renderer& operator=(const Renderer&) = delete;

    void beginFrame(const ClearColor& color = {});
    void render(float dt = 0.0f);   // dt usato per animazioni future
    void endFrame();

    // Accesso alla camera per muoverla da Application o da sistemi ECS
    [[nodiscard]] Camera& getCamera();

private:
    Window& m_window;

    std::unique_ptr<Shader> m_shader;
    std::unique_ptr<Camera> m_camera;

    unsigned int m_vao = 0;
    unsigned int m_vbo = 0;

    float m_elapsedTime = 0.0f; // per animazioni (rotazione demo)

    void initTriangle();
};

} // namespace mini