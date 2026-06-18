#pragma once

#include <memory>

namespace mini
{

class Window;
class Shader;

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

    // Chiama all'inizio di ogni frame: clear colore + depth
    void beginFrame(const ClearColor& color = {});

    // Disegna la scena. Per ora: triangolo demo.
    // In futuro: iterera' sulle entita' renderizzabili.
    void render();

    // Chiama alla fine: swap buffer
    void endFrame();

private:
    Window& m_window;

    std::unique_ptr<Shader> m_shader;
    unsigned int            m_vao = 0;
    unsigned int            m_vbo = 0;

    void initTriangle();
};

} // namespace mini