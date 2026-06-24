#pragma once

#include "mini/render/Mesh.hpp"

#include <optional>
#include <string>
#include <vector>

namespace mini
{

// Modello 3D. Supporta OBJ (tinyobjloader) e glTF/GLB (tinygltf).
// Puo' contenere piu' sub-mesh.
class Model
{
public:
    // Carica un file .obj (con eventuale .mtl nella stessa cartella).
    static std::optional<Model> loadFromObj(const char* path);

    // Carica un file .gltf (ASCII) o .glb (binario).
    // Blender: File > Esporta > glTF 2.0 (con opzione "GLB" per file unico).
    static std::optional<Model> loadFromGltf(const char* path);

    // Disegna tutti i sub-mesh
    void draw() const;

    [[nodiscard]] bool        isEmpty()      const;
    [[nodiscard]] std::size_t getMeshCount() const;
    [[nodiscard]] const std::string& getPath() const;

    // Accesso diretto ai sub-mesh (utile per assegnare materiali diversi)
    [[nodiscard]] const std::vector<Mesh>& getMeshes() const;

private:
    std::vector<Mesh> m_meshes;
    std::string       m_path;
};

} // namespace mini