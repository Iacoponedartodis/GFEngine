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

// Gestisce il movimento del giocatore, la gravità, il salto,
// l'air control e lo sparo. Estratto da Application.cpp.
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

    // ── Arma ─────────────────────────────────────────────────────────
    Weapon   weapon;

    // ── Init ─────────────────────────────────────────────────────────
    void reset(EntityId playerEntity, float hp, const glm::vec3& spawnPos, Camera& cam);

    // ── Update (chiamato ogni frame in Playing) ──────────────────────

    // Movimento + gravità + salto. Aggiorna la posizione della camera.
    void updateMovement(Camera& cam, const InputManager& input,
                        World& world, float elapsed);

    // Sincronizza la posizione della camera con l'entità giocatore
    // e controlla se il giocatore è stato colpito/ucciso.
    // Ritorna true se il giocatore è appena morto QUESTO frame.
    bool updateHealth(World& world, Audio& audio);

    // Spara creando entità proiettile. Ritorna true se ha sparato.
    bool updateShooting(World& world, Camera& cam, const InputManager& input,
                        Audio& audio, Mesh* bulletMesh, bool mouseCaptured);

    // Aggiorna il timer di respawn. Ritorna true se il giocatore è appena rinato.
    bool updateRespawn(World& world, Camera& cam,
                       float respawnDelay, const glm::vec3& spawnPos, float maxHp);
};

} // namespace mini