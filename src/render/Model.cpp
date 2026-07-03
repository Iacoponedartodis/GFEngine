#include "mini/render/Model.hpp"

#include <tiny_obj_loader.h>

// tinygltf: solo dichiarazioni (implementazione in tinygltf_impl.cpp)
#include <stb_image.h>
#include <stb_image_write.h>
#define TINYGLTF_NO_INCLUDE_STB_IMAGE
#define TINYGLTF_NO_INCLUDE_STB_IMAGE_WRITE
#include <tiny_gltf.h>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

#include <cmath>
#include <cstring>
#include <functional>
#include <iostream>
#include <string_view>
#include <unordered_map>

namespace mini
{

// Trasformazione locale di un nodo glTF (matrice esplicita oppure TRS).
static glm::mat4 gltfNodeLocal(const tinygltf::Node& node)
{
    if (node.matrix.size() == 16)
    {
        glm::mat4 m(1.0f);
        for (int col = 0; col < 4; ++col)
            for (int row = 0; row < 4; ++row)
                m[col][row] = (float)node.matrix[col * 4 + row];
        return m;
    }
    glm::mat4 T(1.0f), R(1.0f), S(1.0f);
    if (node.translation.size() == 3)
        T = glm::translate(glm::mat4(1.0f),
            glm::vec3((float)node.translation[0], (float)node.translation[1],
                      (float)node.translation[2]));
    if (node.rotation.size() == 4)
        R = glm::mat4_cast(glm::quat((float)node.rotation[3], (float)node.rotation[0],
                                     (float)node.rotation[1], (float)node.rotation[2]));
    if (node.scale.size() == 3)
        S = glm::scale(glm::mat4(1.0f),
            glm::vec3((float)node.scale[0], (float)node.scale[1], (float)node.scale[2]));
    return T * R * S;
}

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

    // Helper: legge un float dal buffer di un accessor rispettando byteStride.
    // floatsPerElem = 3 per VEC3, 2 per VEC2. elemIdx = indice del vertice.
    // compIdx = indice del componente (0=x, 1=y, 2=z).
    auto readFloat = [&](int accIdx, size_t elemIdx, int compIdx) -> float
    {
        if (accIdx < 0) return 0.0f;
        const auto& acc = gltf.accessors[accIdx];
        const auto& bv  = gltf.bufferViews[acc.bufferView];
        const auto& buf = gltf.buffers[bv.buffer];
        // byteStride == 0 significa dense (stride = dimensione elemento)
        // Per VEC3 float: 12 byte. Per VEC2 float: 8 byte.
        int typeComp = (acc.type == TINYGLTF_TYPE_VEC3) ? 3 :
                       (acc.type == TINYGLTF_TYPE_VEC2) ? 2 : 4;
        size_t defaultStride = (size_t)(typeComp * sizeof(float));
        size_t stride = (bv.byteStride > 0) ? bv.byteStride : defaultStride;
        size_t byteOff = bv.byteOffset + acc.byteOffset + elemIdx * stride
                       + (size_t)compIdx * sizeof(float);
        if (byteOff + sizeof(float) > buf.data.size()) return 0.0f;
        float v; std::memcpy(&v, buf.data.data() + byteOff, sizeof(float)); return v;
    };

    // Helper mantenuto per compatibilità: restituisce count di un accessor
    auto accessorCount = [&](int accIdx) -> size_t {
        if (accIdx < 0) return 0;
        return gltf.accessors[accIdx].count;
    };

    // Processa una primitiva applicando la trasformazione world del nodo.
    // Le posizioni vengono trasformate dalla matrice; le normali dalla
    // normal matrix (inverse-transpose della parte 3x3).
    auto processPrimitive = [&](const tinygltf::Primitive& prim, const glm::mat4& world)
    {
        if (prim.mode != TINYGLTF_MODE_TRIANGLES) return;

        int posAcc  = prim.attributes.count("POSITION")   ? prim.attributes.at("POSITION")   : -1;
        int normAcc = prim.attributes.count("NORMAL")     ? prim.attributes.at("NORMAL")     : -1;
        int uvAcc   = prim.attributes.count("TEXCOORD_0") ? prim.attributes.at("TEXCOORD_0") : -1;

        size_t posCount = accessorCount(posAcc);
        if (posCount == 0) return;

        const glm::mat3 normalMat = glm::transpose(glm::inverse(glm::mat3(world)));

        // Colore base dal materiale glTF
        glm::vec3 matColor{1.0f, 1.0f, 1.0f};
        if (prim.material >= 0)
        {
            const auto& mat = gltf.materials[prim.material];
            const auto& bf  = mat.pbrMetallicRoughness.baseColorFactor;
            if (bf.size() >= 3)
                matColor = { (float)bf[0], (float)bf[1], (float)bf[2] };
        }

        auto readPos  = [&](size_t vi) {
            glm::vec4 p = world * glm::vec4(readFloat(posAcc, vi, 0),
                                            readFloat(posAcc, vi, 1),
                                            readFloat(posAcc, vi, 2), 1.0f);
            return glm::vec3(p);
        };
        auto readNorm = [&](size_t vi) -> glm::vec3 {
            if (normAcc < 0) return {0,1,0};
            glm::vec3 n = normalMat * glm::vec3(readFloat(normAcc, vi, 0),
                                                readFloat(normAcc, vi, 1),
                                                readFloat(normAcc, vi, 2));
            float len = glm::length(n);
            return len > 1e-6f ? n / len : glm::vec3(0,1,0);
        };
        auto readUV   = [&](size_t vi) -> glm::vec2 {
            if (uvAcc < 0) return {0,0};
            return { readFloat(uvAcc, vi, 0), readFloat(uvAcc, vi, 1) };
        };

        std::vector<Mesh::Vertex> verts;

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

                verts.push_back({ readPos(vi), readNorm(vi), matColor, readUV(vi) });
            }
        }
        else
        {
            verts.reserve(posCount);
            for (size_t vi = 0; vi < posCount; ++vi)
                verts.push_back({ readPos(vi), readNorm(vi), matColor, readUV(vi) });
        }

        if (!verts.empty())
            model.m_meshes.emplace_back(verts);
    };

    // Traversa la gerarchia dei nodi accumulando le trasformazioni.
    std::function<void(int, const glm::mat4&)> traverse =
        [&](int nodeIdx, const glm::mat4& parent)
    {
        if (nodeIdx < 0 || nodeIdx >= (int)gltf.nodes.size()) return;
        const auto& node = gltf.nodes[nodeIdx];
        glm::mat4 world = parent * gltfNodeLocal(node);
        if (node.mesh >= 0 && node.mesh < (int)gltf.meshes.size())
        {
            // glTF: per le mesh skinnate i vertici sono già nello spazio di
            // bind (world); la trasformazione del nodo NON va applicata.
            // Per le mesh statiche, invece, la posizione è data dalla gerarchia.
            const glm::mat4 meshXf = (node.skin >= 0) ? glm::mat4(1.0f) : world;
            for (const auto& prim : gltf.meshes[node.mesh].primitives)
                processPrimitive(prim, meshXf);
        }
        for (int child : node.children)
            traverse(child, world);
    };

    // Radici: dalla scena di default se valida, altrimenti tutti i nodi root.
    bool traversed = false;
    if (gltf.defaultScene >= 0 && gltf.defaultScene < (int)gltf.scenes.size())
    {
        for (int root : gltf.scenes[gltf.defaultScene].nodes)
            traverse(root, glm::mat4(1.0f));
        traversed = true;
    }
    else if (!gltf.scenes.empty())
    {
        for (int root : gltf.scenes[0].nodes)
            traverse(root, glm::mat4(1.0f));
        traversed = true;
    }

    // Fallback: nessuna scena → processa tutte le mesh senza trasformazione.
    if (!traversed || model.m_meshes.empty())
    {
        model.m_meshes.clear();
        for (const auto& mesh : gltf.meshes)
            for (const auto& prim : mesh.primitives)
                processPrimitive(prim, glm::mat4(1.0f));
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

std::optional<Mesh> Model::merged() const
{
    if (m_meshes.empty()) return std::nullopt;
    if (m_meshes.size() == 1) return m_meshes.front();

    std::vector<float> data;
    int totalVerts = 0;
    for (const auto& m : m_meshes)
    {
        const auto& d = m.getVertexData();
        data.insert(data.end(), d.begin(), d.end());
        totalVerts += m.getVertexCount();
    }
    return Mesh(std::move(data), totalVerts);
}

} // namespace mini