#pragma once

#include "mini/ecs/Entity.hpp"
#include "mini/game/Weapon.hpp"

#include <glm/glm.hpp>

namespace mini
{

class World;
class Camera;
class InputManager;
class Audio;
class Mesh;

class PlayerController
{
public:
    // ── Stato ────────────────────────────────────────────────────────
    EntityId entity     = 0;
    float    velY       = 0.0f;
    bool     onGround   = true;
    float    airVelX    = 0.0f;
    float    airVelZ    = 0.0f;
    float    prevHp     = 100.0f;
    bool     isDead     = false;
    float    respawnTimer = -1.0f;

    // ── Armi (0=primaria, 1=secondaria) ──────────────────────────────
    Weapon   weapon;          // arma attiva (punta a weapons[activeWeapon])
    Weapon   weapons[2];      // slot primaria e secondaria

    // ── Terza persona ─────────────────────────────────────────────────
    bool      thirdPerson  = false;
    float     tpsYaw       = 0.0f;
    float     tpsPitch     = -10.0f;
    float     tpsDistance  = 5.0f;
    float     tpsHeight    = 2.2f;
    glm::vec3 tpsPlayerPos = {0, 0.86f, 0};

    // ── Stato di gameplay ─────────────────────────────────────────────
    bool  isSprinting    = false;   // read-only (aggiornato da updateMovement)
    bool  isCrouching    = false;   // toggle su Crouch pressed
    bool  isAiming       = false;   // mouse destro tenuto
    bool  isRolling      = false;   // true durante schivata
    float rollTimer      = 0.0f;    // timer interno roll (secondi rimanenti)
    float rollVelX       = 0.0f;
    float rollVelZ       = 0.0f;
    int   activeWeapon   = 0;       // 0=primaria, 1=secondaria

    // ── Init ─────────────────────────────────────────────────────────
    void reset(EntityId playerEntity, float hp, const glm::vec3& spawnPos, Camera& cam);

    // ── Toggle TPS/FPS ────────────────────────────────────────────────
    // Sincronizza yaw/pitch con la camera corrente al momento del toggle.
    void toggleThirdPerson(Camera& cam);

    // ── Update (chiamato ogni frame in Playing) ───────────────────────
    void updateMovement(Camera& cam, const InputManager& input,
                        World& world, float elapsed);

    // Mouse in TPS: ruota l'orbita, non la camera FPS.
    // In FPS: delega a cam.processMouse().
    void processMouse(Camera& cam, float dx, float dy, float sensitivity = 0.1f);

    bool updateHealth(World& world, Audio& audio);

    bool updateShooting(World& world, Camera& cam, const InputManager& input,
                        Audio& audio, Mesh* bulletMesh, bool mouseCaptured);

    bool updateRespawn(World& world, Camera& cam,
                       float respawnDelay, const glm::vec3& spawnPos, float maxHp);
};

} // namespace mini
