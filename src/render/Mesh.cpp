#include "mini/render/Mesh.hpp"
#include "mini/platform/OpenGL.hpp"

namespace mini
{

// ============================================================
// Costruttore: interleave pos(3)+normal(3)+color(3)+UV(2) = 11 float
// ============================================================

Mesh::Mesh(const std::vector<Vertex>& vertices)
    : m_vertexCount(static_cast<int>(vertices.size()))
{
    m_data.reserve(vertices.size() * 11u);
    for (const auto& v : vertices)
    {
        m_data.push_back(v.position.x);
        m_data.push_back(v.position.y);
        m_data.push_back(v.position.z);
        m_data.push_back(v.normal.x);
        m_data.push_back(v.normal.y);
        m_data.push_back(v.normal.z);
        m_data.push_back(v.color.r);
        m_data.push_back(v.color.g);
        m_data.push_back(v.color.b);
        m_data.push_back(v.uv.x);
        m_data.push_back(v.uv.y);
    }
}

// ============================================================
// Draw — client-side arrays, stride 44 byte (11 float)
// ============================================================

void Mesh::draw() const
{
    if (m_data.empty()) return;

    constexpr GLsizei stride = static_cast<GLsizei>(11 * sizeof(float));
    const float* base = m_data.data();

    glEnableVertexAttribArray(0u);
    glEnableVertexAttribArray(1u);
    glEnableVertexAttribArray(2u);
    glEnableVertexAttribArray(3u);

    glVertexAttribPointer(0u, 3, GL_FLOAT, GL_FALSE, stride, static_cast<const void*>(base + 0));  // position
    glVertexAttribPointer(1u, 3, GL_FLOAT, GL_FALSE, stride, static_cast<const void*>(base + 3));  // normal
    glVertexAttribPointer(2u, 3, GL_FLOAT, GL_FALSE, stride, static_cast<const void*>(base + 6));  // color
    glVertexAttribPointer(3u, 2, GL_FLOAT, GL_FALSE, stride, static_cast<const void*>(base + 9));  // UV

    glDrawArrays(GL_TRIANGLES, 0, m_vertexCount);

    glDisableVertexAttribArray(3u);
    glDisableVertexAttribArray(2u);
    glDisableVertexAttribArray(1u);
    glDisableVertexAttribArray(0u);
}

// ============================================================
// Factory: cubo con UV [0,1] per ogni faccia
// ============================================================

/*static*/ Mesh Mesh::createCube(const glm::vec3& color)
{
    const glm::vec3 lbd{-0.5f,-0.5f,-0.5f}, rbd{ 0.5f,-0.5f,-0.5f};
    const glm::vec3 rtd{ 0.5f, 0.5f,-0.5f}, ltd{-0.5f, 0.5f,-0.5f};
    const glm::vec3 lbf{-0.5f,-0.5f, 0.5f}, rbf{ 0.5f,-0.5f, 0.5f};
    const glm::vec3 rtf{ 0.5f, 0.5f, 0.5f}, ltf{-0.5f, 0.5f, 0.5f};

    std::vector<Vertex> verts;
    verts.reserve(36);

    // Aggiunge una faccia quad (2 triangoli CCW) con UV [0,1]^2
    auto face = [&](glm::vec3 v0, glm::vec3 v1, glm::vec3 v2, glm::vec3 v3, glm::vec3 n)
    {
        verts.push_back({v0, n, color, {0.0f, 0.0f}});
        verts.push_back({v1, n, color, {1.0f, 0.0f}});
        verts.push_back({v2, n, color, {1.0f, 1.0f}});
        verts.push_back({v0, n, color, {0.0f, 0.0f}});
        verts.push_back({v2, n, color, {1.0f, 1.0f}});
        verts.push_back({v3, n, color, {0.0f, 1.0f}});
    };

    face(lbf, rbf, rtf, ltf, { 0, 0, 1});   // Front  +Z
    face(rbd, lbd, ltd, rtd, { 0, 0,-1});   // Back   -Z
    face(rbf, rbd, rtd, rtf, { 1, 0, 0});   // Right  +X
    face(lbd, lbf, ltf, ltd, {-1, 0, 0});   // Left   -X
    face(ltf, rtf, rtd, ltd, { 0, 1, 0});   // Top    +Y
    face(lbd, rbd, rbf, lbf, { 0,-1, 0});   // Bottom -Y

    return Mesh(verts);
}

} // namespace mini