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
#include "mini/game/ClassResolve.hpp"           // ADR-022: la classe vince
#include "mini/game/WeaponHandPose.hpp"          // LA formula della posa, condivisa con gli editor
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

    // Arma in mano = arma primaria EFFETTIVA (classres: la CLASSE vince sul
    // loadout dell'entità, ADR-022); weapon_display fornisce solo la POSA.
    // Due bug della stessa famiglia sono già passati da questa riga:
    //   2026-07-11 — weapon_display.id vinceva sul loadout (B1 Heavy: loadout
    //                E-5C, modello in mano E-5);
    //   2026-07-17 — il loadout dell'entità vinceva sulla CLASSE (unità che
    //                sparava l'arma della classe impugnando quella dell'entità).
    // Per questo la regola non è più scritta qui: sta in classres, una volta sola.
    std::string weaponId = classres::primaryWeaponId(*registry, *def);
    if (weaponId.empty()) weaponId = def->weaponDisplay.weaponId;
    if (weaponId.empty()) return out;

    const WeaponDef* wd = registry->getWeapon(weaponId);
    if (!wd || wd->meshPath.empty()) return out;

    auto it = meshCache->find(wd->meshPath);
    if (it == meshCache->end()) return out;
    out.mesh = it->second;

    // Mano del personaggio (model space): l'attach point è del PERSONAGGIO,
    // sempre dall'entità — è l'unica parte della posa che dipende da chi impugna.
    const auto& disp = def->weaponDisplay;
    glm::vec3 hand{0.0f};
    auto ap = def->attachPoints.find(disp.hand);
    if (ap == def->attachPoints.end())
        ap = def->attachPoints.find("right_hand");
    if (ap != def->attachPoints.end())
        hand = {ap->second[0], ap->second[1], ap->second[2]};

    // Posa in mano (KI #49): dall'ARMA se autorata (handScale>0), altrimenti dal
    // weapon_display legacy dell'entità (fallback di transizione). È ciò che
    // impedisce l'"arma per l'entità A a scala tarata per B": la posa segue
    // l'arma effettivamente impugnata, non l'id fisso sull'entità.
    const bool  wPose     = wd->handScale > 0.0f;
    const float poseScale = wPose ? wd->handScale : disp.scale;
    const glm::vec3 poseRot = wPose
        ? glm::vec3(wd->handRot[0], wd->handRot[1], wd->handRot[2])
        : glm::vec3(disp.rot[0], disp.rot[1], disp.rot[2]);
    const glm::vec3 off = wPose
        ? glm::vec3(wd->handOffset[0], wd->handOffset[1], wd->handOffset[2])
        : glm::vec3(disp.offset[0], disp.offset[1], disp.offset[2]);
    HandPose p;
    p.hand      = hand;
    p.offset    = off;
    p.rot       = poseRot;
    p.grip      = {wd->gripAttach[0], wd->gripAttach[1], wd->gripAttach[2]};
    p.scale     = poseScale;
    p.charScale = def->meshScale;
    p.baseRotX  = wd->meshRotX;
    p.baseRotY  = wd->meshRotY;
    out.local   = handLocal(p);   // formula condivisa: vedi HandPose sopra
    return out;
}

} // namespace mini::weaponattach
