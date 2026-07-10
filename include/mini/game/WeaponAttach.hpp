#pragma once
// WeaponAttach — risolve l'arma "in mano" di un'entità a partire dai metadata
// autorati nell'editor: attach point della mano (EnemyDef.attachPoints),
// grip dell'arma (WeaponDef.gripAttach) e posa (EnemyDef.weaponDisplay).
// Stessa formula dell'anteprima nell'Entity Editor:
//   local = T(mano + offset) * R(rot) * S(scala) * T(-grip)
// La matrice risultante è nel MODEL SPACE del personaggio: al render va
// composta con la matrice modello dell'entità.

#include "mini/game/data/Definitions.hpp"
#include "mini/game/game_modes/IGameMode.hpp"   // MeshCache
#include "mini/game/data/DefinitionRegistry.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace mini::weaponattach
{

struct Resolved
{
    Mesh*     mesh = nullptr;       // nullptr = niente arma da mostrare
    glm::mat4 local = glm::mat4(1.0f);
};

inline Resolved resolve(const DefinitionRegistry* registry,
                        const MeshCache* meshCache,
                        const EnemyDef* def)
{
    Resolved out;
    if (!registry || !meshCache || !def) return out;

    // Arma da mostrare: weapon_display.id, fallback arma primaria
    std::string weaponId = def->weaponDisplay.weaponId;
    if (weaponId.empty()) weaponId = def->primaryWeaponId();
    if (weaponId.empty()) return out;

    const WeaponDef* wd = registry->getWeapon(weaponId);
    if (!wd || wd->meshPath.empty()) return out;

    auto it = meshCache->find(wd->meshPath);
    if (it == meshCache->end()) return out;
    out.mesh = it->second;

    // Mano del personaggio (model space)
    glm::vec3 hand{0.0f};
    auto ap = def->attachPoints.find(def->weaponDisplay.hand);
    if (ap == def->attachPoints.end())
        ap = def->attachPoints.find("right_hand");
    if (ap != def->attachPoints.end())
        hand = {ap->second[0], ap->second[1], ap->second[2]};

    const auto& disp = def->weaponDisplay;
    const glm::vec3 off  {disp.offset[0], disp.offset[1], disp.offset[2]};
    const glm::vec3 grip {wd->gripAttach[0], wd->gripAttach[1], wd->gripAttach[2]};

    glm::mat4 R = glm::rotate(glm::mat4(1.0f), glm::radians(disp.rot[1]), {0,1,0})
                * glm::rotate(glm::mat4(1.0f), glm::radians(disp.rot[0]), {1,0,0})
                * glm::rotate(glm::mat4(1.0f), glm::radians(disp.rot[2]), {0,0,1});

    // La matrice local viene composta con la trasformazione del PERSONAGGIO
    // (che include la sua mesh_scale): l'arma NON deve ereditarla, altrimenti
    // su modelli scalati (es. clone 0.011) diventa microscopica. Compensa.
    const float charScale = (def->meshScale > 0.0001f) ? def->meshScale : 1.0f;
    const float effScale  = disp.scale / charScale;

    // mesh_rot_y del WeaponDef: raddrizzamento base del GLB attorno al grip
    // (0 per le armi esistenti = invariato; la POSA resta in weapon_display)
    glm::mat4 baseFix(1.0f);
    if (wd->meshRotY != 0.0f)
        baseFix = glm::rotate(glm::mat4(1.0f), glm::radians(wd->meshRotY), {0,1,0});

    out.local = glm::translate(glm::mat4(1.0f), hand + off)
              * R
              * glm::scale(glm::mat4(1.0f), glm::vec3(effScale))
              * baseFix
              * glm::translate(glm::mat4(1.0f), -grip);
    return out;
}

} // namespace mini::weaponattach
