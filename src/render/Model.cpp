#include "mini/render/Model.hpp"

#include <tiny_obj_loader.h>

// tinygltf: solo dichiarazioni (implementazione in tinygltf_impl.cpp)
#include <stb_image.h>
#include <stb_image_write.h>
#define TINYGLTF_NO_INCLUDE_STB_IMAGE
#define TINYGLTF_NO_INCLUDE_STB_IMAGE_WRITE
#include <tiny_gltf.h>

#include <cmath>
#include <iostream>
#include <string_view>
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
// Caricamento glTF / GLB con tinygltf
// ============================================================

/*static*/ std::optional<Model> Model::loadFromGltf(const char* path)
{
    tinygltf::TinyGLTF loader;
    tinygltf::Model gltf;
    std::string err, warn;

    const std::string_view sv(path);
    const bool isBinary = sv.size() >= 4 && sv.substr(sv.size() - 4) == ".glb";

    bool ok = isBinary
        ? loader.LoadBinaryFromFile(&gltf, &err, &warn, path)
        : loader.LoadASCIIFromFile (&gltf, &err, &warn, path);

    if (!warn.empty()) std::cout << "[Model] glTF warning: " << warn << "\n";
    if (!ok)
    {
        std::cerr << "[Model] Errore caricamento glTF '" << path << "': " << err << "\n";
        return std::nullopt;
    }

    Model model;
    model.m_path = path;

    // Helper: legge il buffer raw di un accessor come span di float
    auto accessorData = [&](int accIdx) -> std::pair<const float*, size_t>
    {
        if (accIdx < 0) return {nullptr, 0};
        const auto& acc  = gltf.accessors[accIdx];
        const auto& bv   = gltf.bufferViews[acc.bufferView];
        const auto& buf  = gltf.buffers[bv.buffer];
        const float* ptr = reinterpret_cast<const float*>(
            buf.data.data() + bv.byteOffset + acc.byteOffset);
        return {ptr, acc.count};
    };

    for (const auto& mesh : gltf.meshes)
    {
        for (const auto& prim : mesh.primitives)
        {
            if (prim.mode != TINYGLTF_MODE_TRIANGLES) continue;

            auto [posPtr, posCount] = accessorData(
                prim.attributes.count("POSITION") ? prim.attributes.at("POSITION") : -1);
            if (!posPtr || posCount == 0) continue;

            auto [normPtr, normCount] = accessorData(
                prim.attributes.count("NORMAL") ? prim.attributes.at("NORMAL") : -1);
            auto [uvPtr,   uvCount  ] = accessorData(
                prim.attributes.count("TEXCOORD_0") ? prim.attributes.at("TEXCOORD_0") : -1);

            // Colore base dal materiale glTF
            glm::vec3 matColor{1.0f, 1.0f, 1.0f};
            if (prim.material >= 0)
            {
                const auto& mat = gltf.materials[prim.material];
                const auto& bf  = mat.pbrMetallicRoughness.baseColorFactor;
                if (bf.size() >= 3)
                    matColor = { (float)bf[0], (float)bf[1], (float)bf[2] };
            }

            std::vector<Mesh::Vertex> verts;

            // Indici (se presenti)
            if (prim.indices >= 0)
            {
                const auto& idxAcc = gltf.accessors[prim.indices];
                const auto& idxBv  = gltf.bufferViews[idxAcc.bufferView];
                const auto& idxBuf = gltf.buffers[idxBv.buffer];
                const uint8_t* idxBase = idxBuf.data.data() + idxBv.byteOffset + idxAcc.byteOffset;

                verts.reserve(idxAcc.count);
                for (size_t i = 0; i < idxAcc.count; ++i)
                {
                    uint32_t vi = 0;
                    if      (idxAcc.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT)
                        vi = reinterpret_cast<const uint16_t*>(idxBase)[i];
                    else if (idxAcc.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT)
                        vi = reinterpret_cast<const uint32_t*>(idxBase)[i];
                    else if (idxAcc.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE)
                        vi = idxBase[i];

                    glm::vec3 pos  = { posPtr[vi*3], posPtr[vi*3+1], posPtr[vi*3+2] };
                    glm::vec3 norm = (normPtr && vi < normCount)
                        ? glm::vec3{ normPtr[vi*3], normPtr[vi*3+1], normPtr[vi*3+2] }
                        : glm::vec3{ 0, 1, 0 };
                    glm::vec2 uv   = (uvPtr && vi < uvCount)
                        ? glm::vec2{ uvPtr[vi*2], uvPtr[vi*2+1] }
                        : glm::vec2{ 0, 0 };
                    verts.push_back({ pos, norm, matColor, uv });
                }
            }
            else
            {
                // Non-indexed
                verts.reserve(posCount);
                for (size_t vi = 0; vi < posCount; ++vi)
                {
                    glm::vec3 pos  = { posPtr[vi*3], posPtr[vi*3+1], posPtr[vi*3+2] };
                    glm::vec3 norm = (normPtr && vi < normCount)
                        ? glm::vec3{ normPtr[vi*3], normPtr[vi*3+1], normPtr[vi*3+2] }
                        : glm::vec3{ 0, 1, 0 };
                    glm::vec2 uv   = (uvPtr && vi < uvCount)
                        ? glm::vec2{ uvPtr[vi*2], uvPtr[vi*2+1] }
                        : glm::vec2{ 0, 0 };
                    verts.push_back({ pos, norm, matColor, uv });
                }
            }

            if (!verts.empty())
                model.m_meshes.emplace_back(verts);
        }
    }

    std::cout << "[Model] Caricato glTF: '" << path << "' — "
              << model.m_meshes.size() << " primitive(s)\n";
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