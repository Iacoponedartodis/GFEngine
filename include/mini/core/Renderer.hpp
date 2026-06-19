#pragma once

#include <memory>
#include <glm/glm.hpp>

namespace mini
{

class Window;
class Shader;
class Camera;
class Mesh;
class Texture;

struct ClearColor { float r=0.04f, g=0.04f, b=0.10f, a=1.0f; };

// Renderer: API di disegno multi-oggetto.
// Usa beginFrame / drawMesh (N volte) / endFrame ogni frame.
class Renderer
{
public:
    explicit Renderer(Window& window);
    ~Renderer();

    Renderer(const Renderer&)            = delete;
    Renderer& operator=(const Renderer&) = delete;

    // Inizio frame: clear color+depth
    void beginFrame(const ClearColor& color = {});

    // Disegna una singola mesh con la sua matrice model e un colore tint.
    // texture == nullptr: usa il vertex color della mesh.
    void drawMesh(const Mesh&    mesh,
                  const Texture* texture,
                  const glm::mat4& modelMatrix,
                  const glm::vec3& colorTint = {1.0f, 1.0f, 1.0f});

    // Fine frame: swap buffer
    void endFrame();

    [[nodiscard]] Camera& getCamera();

private:
    Window& m_window;
    std::unique_ptr<Shader> m_shader;
    std::unique_ptr<Camera> m_camera;
};

} // namespace mini