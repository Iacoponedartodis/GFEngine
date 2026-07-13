#pragma once
#include <glm/glm.hpp>

namespace mini
{
class Mesh;
class Texture;

struct MeshRendererComponent
{
    Mesh*    mesh    = nullptr;
    Texture* texture = nullptr;
    float    r           = 1.0f;
    float    g           = 1.0f;
    float    b           = 1.0f;
    float    meshOffsetY = 0.0f;
    // Rotazione VISIVA extra attorno a Y (gradi), applicata al render SENZA
    // toccare transform.ry. Per i veicoli: transform.ry è la direzione di
    // MARCIA; questo raddrizza il muso del modello indipendentemente, così
    // ruotare il modello non inverte la guida (19_Vehicles).
    float    yawOffsetDeg = 0.0f;
    bool     visible      = true;

    // Modello agganciato (es. arma in mano): disegnato componendo la matrice
    // modello dell'entità con attachLocal (posa nel model space del personaggio).
    Mesh*     attachMesh  = nullptr;
    glm::mat4 attachLocal = glm::mat4(1.0f);
};

} // namespace mini
