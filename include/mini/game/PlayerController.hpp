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

    // ── Traccia di SESSIONE GIOCATA (ADR-050, doc 42 buco O3) ────────────
    // Tutte le mie misure vengono da simulazioni AI-vs-AI: del caso che conta
    // davvero — una partita giocata — non avevo **nessun** dato. Non so quanto
    // spara il giocatore, quanto si muove, quanto sta morto, quanti ordini dà.
    // Senza, ogni conclusione sul bilanciamento vale per una battaglia fra bot.
    // Contatori grezzi: l'aggregazione e la cadenza le decide chi li emette.
    struct SessionStats
    {
        int   shots      = 0;    // colpi sparati dal giocatore
        int   orders     = 0;    // ordini impartiti alla squadra
        float distance   = 0.0f; // metri percorsi (solo XZ)
        float timeAlive  = 0.0f; // secondi in piedi
        float timeDead   = 0.0f; // secondi da morto/in respawn
        float timeAds    = 0.0f; // secondi in mira
    };
    SessionStats session;
private:
    float m_lastX = 0.0f, m_lastZ = 0.0f;   // posizione al tick prima (distanza)
public:

    // ── Stat base del personaggio (14_ClassSystem / KI #35) ──────────
    // Popolate da PlayerDef (data/characters/<id>.json) al load. I DEFAULT sono
    // esattamente i valori che il gioco usava quando erano costanti: senza dati,
    // o con dati assenti, il comportamento è identico a prima — per costruzione.
    // Prima del 2026-07-15 questo tipo era autorato ma NON letto da nessuno: le
    // stat regolate nell'editor non avevano alcun effetto (KI #35).
    float    moveSpeed   = 5.0f;    // era config::PLAYER_SPEED
    float    jumpMult    = 1.0f;    // moltiplicatore su config::JUMP_IMPULSE
    float    sprintMult  = 1.65f;   // era la costante locale SPRINT_MULT
    float    armorRating = 1.0f;    // divisore del danno (1 = nessuna riduzione)

    // ── Armi (0=primaria, 1=secondaria) ──────────────────────────────
    // L'arma attiva è SEMPRE weapons[activeWeapon]: niente copia separata
    // (la copia desincronizzata azzerava il calore allo switch — KI #22).
    Weapon   weapons[2];      // slot primaria e secondaria
    [[nodiscard]] Weapon&       weapon()       { return weapons[activeWeapon]; }
    [[nodiscard]] const Weapon& weapon() const { return weapons[activeWeapon]; }

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
