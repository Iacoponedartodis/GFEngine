#include "mini/render/Model.hpp"

#include <tiny_obj_loader.h>

#include <cmath>
#include <iostream>
#include <unordered_map>

namespace mini
{

// ============================================================
// Caricamento OBJ con tinyobjloader
// ============================================================

/*static*/ std::optional<Model> Model::loadFromObj(const char* path)
{
    tinyobj::ObjReaderConfig cfg;
    cfg.mtl_search_path = "";   // cerca .mtl nella stessa cartella del .obj
    cfg.triangulate     = true; // triangola automaticamente poligoni non-triangolari

    tinyobj::ObjReader reader;
    if (!reader.ParseFromFile(path, cfg))
    {
        std::cerr << "[Model] Errore parsing OBJ '" << path << "':\n"
                  << reader.Error() << std::endl;
        return std::nullopt;
    }

    if (!reader.Warning().empty())
        std::cout << "[Model] Warning OBJ: " << reader.Warning() << std::endl;

    const auto& attrib    = reader.GetAttrib();
    const auto& shapes    = reader.GetShapes();
    const auto& materials = reader.GetMaterials();

    Model model;
    model.m_path = path;

    // -------------------------------------------------------
    // Converte ogni shape in un Mesh
    // -------------------------------------------------------
    for (const auto& shape : shapes)
    {
        std::vector<Mesh::Vertex> verts;
        verts.reserve(shape.mesh.indices.size());

        std::size_t indexOffset = 0;

        for (std::size_t faceIdx = 0; faceIdx < shape.mesh.num_face_vertices.size(); ++faceIdx)
        {
            const int fv = static_cast<int>(shape.mesh.num_face_vertices[faceIdx]);

            // Colore dal materiale del triangolo (default bianco)
            glm::vec3 faceColor{1.0f, 1.0f, 1.0f};
            if (!materials.empty() && faceIdx < shape.mesh.material_ids.size())
            {
                const int matId = shape.mesh.material_ids[faceIdx];
                if (matId >= 0 && matId < static_cast<int>(materials.size()))
                {
                    const auto& m = materials[static_cast<std::size_t>(matId)];
                    faceColor = {m.diffuse[0], m.diffuse[1], m.diffuse[2]};
                }
            }

            // Calcola normale flat del triangolo (usata se il file non ha normali)
            glm::vec3 flatNormal{0.0f, 1.0f, 0.0f};
            if (fv >= 3)
            {
                const auto i0 = shape.mesh.indices[indexOffset + 0];
                const auto i1 = shape.mesh.indices[indexOffset + 1];
                const auto i2 = shape.mesh.indices[indexOffset + 2];

                const glm::vec3 p0{
                    attrib.vertices[3 * static_cast<std::size_t>(i0.vertex_index) + 0],
                    attrib.vertices[3 * static_cast<std::size_t>(i0.vertex_index) + 1],
                    attrib.vertices[3 * static_cast<std::size_t>(i0.vertex_index) + 2]
                };
                const glm::vec3 p1{
                    attrib.vertices[3 * static_cast<std::size_t>(i1.vertex_index) + 0],
                    attrib.vertices[3 * static_cast<std::size_t>(i1.vertex_index) + 1],
                    attrib.vertices[3 * static_cast<std::size_t>(i1.vertex_index) + 2]
                };
                const glm::vec3 p2{
                    attrib.vertices[3 * static_cast<std::size_t>(i2.vertex_index) + 0],
                    attrib.vertices[3 * static_cast<std::size_t>(i2.vertex_index) + 1],
                    attrib.vertices[3 * static_cast<std::size_t>(i2.vertex_index) + 2]
                };
                const glm::vec3 edge1 = p1 - p0;
                const glm::vec3 edge2 = p2 - p0;
                const glm::vec3 cross = glm::vec3{
                    edge1.y * edge2.z - edge1.z * edge2.y,
                    edge1.z * edge2.x - edge1.x * edge2.z,
                    edge1.x * edge2.y - edge1.y * edge2.x
                };
                const float len = std::sqrt(cross.x*cross.x + cross.y*cross.y + cross.z*cross.z);
                if (len > 1e-6f) flatNormal = {cross.x/len, cross.y/len, cross.z/len};
            }

            for (int v = 0; v < fv; ++v)
            {
                const tinyobj::index_t idx = shape.mesh.indices[indexOffset + static_cast<std::size_t>(v)];

                // Posizione
                glm::vec3 pos{
                    attrib.vertices[3 * static_cast<std::size_t>(idx.vertex_index) + 0],
                    attrib.vertices[3 * static_cast<std::size_t>(idx.vertex_index) + 1],
                    attrib.vertices[3 * static_cast<std::size_t>(idx.vertex_index) + 2]
                };

                // Normale (dal file se disponibile, altrimenti flat)
                glm::vec3 norm = flatNormal;
                if (idx.normal_index >= 0)
                {
                    norm = {
                        attrib.normals[3 * static_cast<std::size_t>(idx.normal_index) + 0],
                        attrib.normals[3 * static_cast<std::size_t>(idx.normal_index) + 1],
                        attrib.normals[3 * static_cast<std::size_t>(idx.normal_index) + 2]
                    };
                }

                // UV (0,0 se non presenti)
                glm::vec2 uv{0.0f, 0.0f};
                if (idx.texcoord_index >= 0)
                {
                    uv = {
                        attrib.texcoords[2 * static_cast<std::size_t>(idx.texcoord_index) + 0],
                        attrib.texcoords[2 * static_cast<std::size_t>(idx.texcoord_index) + 1]
                    };
                }

                verts.push_back({pos, norm, faceColor, uv});
            }

            indexOffset += static_cast<std::size_t>(fv);
        }

        if (!verts.empty())
            model.m_meshes.emplace_back(verts);
    }

    std::cout << "[Model] Caricato: '" << path << "' — "
              << model.m_meshes.size() << " sub-mesh, "
              << shapes.size() << " shape(s), "
              << materials.size() << " materiale(i)" << std::endl;

    return model;
}

// ============================================================
// Draw / utility
// ============================================================

void Model::draw() const
{
    for (const auto& mesh : m_meshes)
        mesh.draw();
}

bool        Model::isEmpty()      const { return m_meshes.empty(); }
std::size_t Model::getMeshCount() const { return m_meshes.size(); }
const std::string& Model::getPath() const { return m_path; }
const std::vector<Mesh>& Model::getMeshes() const { return m_meshes; }

} // namespace mini