#pragma once

#include "mini/render/Mesh.hpp"

#include <optional>
#include <string>
#include <vector>

namespace mini
{

// Modello 3D caricato da file OBJ.
// Puo' contenere piu' sub-mesh (uno per shape/material nel file OBJ).
// Usa tinyobjloader internamente.
class Model
{
public:
    // Carica un file .obj (con eventuale .mtl nella stessa cartella).
    // Ritorna nullopt se il file non esiste o non e' parsabile.
    static std::optional<Model> loadFromObj(const char* path);

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