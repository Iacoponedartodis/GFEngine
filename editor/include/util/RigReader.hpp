#pragma once
#include <string>
#include <vector>
#include <glm/glm.hpp>

namespace editor
{

// Legge i nomi dei joint (ossa) da un file GLB/GLTF usando tinygltf.
// Ritorna una lista vuota se il file non contiene skin o non è leggibile.
std::vector<std::string> readGlbJointNames(const std::string& glbPath);

// Dati estesi per un joint: nome, posizione model-space (bind pose), indice padre
struct JointData {
    std::string name;
    glm::vec3   modelPos  = {0,0,0}; // posizione in model space (bind pose)
    int         parentIdx = -1;       // indice nel vettore restituito, -1 = radice
};

// Legge joint + gerarchia + posizioni model-space da un file GLB.
// Se il file non ha skin, restituisce solo i nomi con pos={0,0,0}.
std::vector<JointData> readGlbJointData(const std::string& glbPath);

} // namespace editor
