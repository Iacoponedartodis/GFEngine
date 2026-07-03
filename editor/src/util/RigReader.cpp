// RigReader.cpp
// Legge lo scheletro (ossa/joint) da un file GLB con tinygltf.
// Gestisce due casi:
//   1. Modelli con skin  → usa skin.joints (rig classico).
//   2. Modelli senza skin → usa la gerarchia dei nodi come scheletro
//      (es. droidi con parti rigide collegate da nodi-osso con matrice).
// In entrambi i casi calcola posizioni model-space reali percorrendo
// la gerarchia, così le ossa si allineano alla mesh.

#include "util/RigReader.hpp"

// tinygltf_impl.cpp nella build fornisce l'implementazione; qui solo dichiarazioni
#include <tiny_gltf.h>
#include <functional>
#include <unordered_map>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

namespace editor
{

// ── Trasformazione locale di un nodo glTF (matrice esplicita oppure TRS) ──────
static glm::mat4 nodeLocalTransform(const tinygltf::Node& node)
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

// ── Calcola i JointData (nome + pos world + parent) per un modello caricato ────
static std::vector<JointData> computeJoints(const tinygltf::Model& model)
{
    std::vector<JointData> result;
    const int N = (int)model.nodes.size();
    if (N == 0) return result;

    // Mappa figlio → padre
    std::vector<int> parent(N, -1);
    for (int ni = 0; ni < N; ++ni)
        for (int child : model.nodes[ni].children)
            if (child >= 0 && child < N) parent[child] = ni;

    // World transform di ogni nodo (cache ricorsiva)
    std::vector<glm::mat4> world(N, glm::mat4(1.0f));
    std::vector<char> done(N, 0);
    std::function<glm::mat4(int)> getW = [&](int i) -> glm::mat4 {
        if (i < 0) return glm::mat4(1.0f);
        if (done[i]) return world[i];
        world[i] = getW(parent[i]) * nodeLocalTransform(model.nodes[i]);
        done[i] = 1;
        return world[i];
    };
    for (int i = 0; i < N; ++i) getW(i);

    // Insieme dei nodi-osso da mostrare
    std::vector<int> jointNodes;
    if (!model.skins.empty())
    {
        // Rig classico: usa l'ordine dello skin
        for (int ji : model.skins[0].joints)
            if (ji >= 0 && ji < N) jointNodes.push_back(ji);
    }
    else
    {
        // Nessuno skin: i nodi strutturali (con figli) fungono da ossa.
        for (int ni = 0; ni < N; ++ni)
            if (!model.nodes[ni].name.empty() && !model.nodes[ni].children.empty())
                jointNodes.push_back(ni);
    }
    // Ultima risorsa: tutti i nodi con nome
    if (jointNodes.empty())
        for (int ni = 0; ni < N; ++ni)
            if (!model.nodes[ni].name.empty()) jointNodes.push_back(ni);

    // Mappa nodeIdx → indice nel risultato
    std::unordered_map<int, int> nodeToRes;
    for (int k = 0; k < (int)jointNodes.size(); ++k)
        nodeToRes[jointNodes[k]] = k;

    result.resize(jointNodes.size());
    for (int k = 0; k < (int)jointNodes.size(); ++k)
    {
        const int ni = jointNodes[k];
        JointData jd;
        jd.name = model.nodes[ni].name.empty()
                ? ("joint_" + std::to_string(k)) : model.nodes[ni].name;
        jd.modelPos = glm::vec3(world[ni][3]);

        // Padre = antenato più vicino che è anch'esso un osso mostrato
        int p = parent[ni];
        while (p >= 0 && !nodeToRes.count(p)) p = parent[p];
        jd.parentIdx = (p >= 0 && nodeToRes.count(p)) ? nodeToRes[p] : -1;

        result[k] = jd;
    }
    return result;
}

// ── API pubblica ──────────────────────────────────────────────────────────────
std::vector<JointData> readGlbJointData(const std::string& glbPath)
{
    std::vector<JointData> result;
    if (glbPath.empty()) return result;

    tinygltf::Model model;
    tinygltf::TinyGLTF loader;
    std::string err, warn;
    if (!loader.LoadBinaryFromFile(&model, &err, &warn, glbPath)) return result;

    return computeJoints(model);
}

std::vector<std::string> readGlbJointNames(const std::string& glbPath)
{
    std::vector<std::string> names;
    for (const auto& jd : readGlbJointData(glbPath))
        names.push_back(jd.name);
    return names;
}

} // namespace editor
