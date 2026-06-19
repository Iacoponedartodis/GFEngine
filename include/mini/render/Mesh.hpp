#pragma once

#include <glm/glm.hpp>
#include <vector>

namespace mini
{

// Mesh con layout interleaved: position + normal + color + UV per vertice.
// Usa client-side arrays (workaround driver Intel — Compatibility Profile).
// Attributi: location 0=pos, 1=normal, 2=color, 3=UV
class Mesh
{
public:
    struct Vertex
    {
        glm::vec3 position;
        glm::vec3 normal;
        glm::vec3 color;
        glm::vec2 uv;
    };

    explicit Mesh(const std::vector<Vertex>& vertices);

    // Disegna la mesh con gli attributi correnti
    void draw() const;

    // Factory: cubo unitario [-0.5, 0.5]^3 con normali e UV per faccia
    static Mesh createCube(const glm::vec3& color = {1.0f, 1.0f, 1.0f});

private:
    std::vector<float> m_data;         // dati interleaved (11 float per vertice)
    int                m_vertexCount = 0;
};

} // namespace mini