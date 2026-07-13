#pragma once

#include "mini/ecs/Entity.hpp"   // EntityId (target cachato per il time-slicing)

namespace mini { class Mesh; class Texture; }

namespace mini
{

enum class AiState : unsigned char {
    Patrol,   // nessun contatto — cammina tra waypoint
    Alert,    // LOS attivo — spara e strafea
    Hunt,     // no LOS ma conosce lastKnown — va verso il bersaglio
    Search    // raggiunto lastKnown ma nessuno — cerca in punti random
};

struct AiComponent
{
    float    shootCooldown  = 0.0f;
    float    shootInterval  = 2.5f;
    float    aggroRange     = 16.0f;
    Mesh*    bulletMesh     = nullptr;
    Texture* bulletTexture  = nullptr;
    float    bulletR = 1.0f, bulletG = 0.25f, bulletB = 0.1f;

    // Statistiche proiettile — popolate da WeaponDef al momento dello spawn
    float    bulletSpeed    = 8.0f;
    float    bulletDamage   = 20.0f;
    float    bulletLifetime = 5.0f;

    // ── Modello arma (da WeaponDef): cadenza reale + surriscaldamento ──
    // fireInterval <= 0 → usa il legacy shootInterval del profilo AI.
    float    fireInterval    = 0.0f;   // 1 / fire_rate dell'arma
    float    heat            = 0.0f;   // 0..1
    float    heatPerShot     = 0.0f;   // 0 = arma senza surriscaldamento
    float    cooldownRate    = 0.30f;  // raffreddamento al secondo
    float    overheatPenalty = 2.0f;   // pausa forzata a overheat (s)
    bool     overheated      = false;

    float patrolAx = 0, patrolAz = 0;
    float patrolBx = 0, patrolBz = 0;
    float patrolSpeed = 2.0f;

    // Sosta ai waypoint di pattuglia (secondi). > del capture_time dei
    // command post → l'AI resta nell'area abbastanza da catturarli.
    // 0 = comportamento legacy (inversione immediata).
    float patrolDwell = 0.0f;
    float waitTimer   = 0.0f;

    // ── Dal profilo AI (AiProfileDef, risolto allo spawn) ─────────────
    bool  jumpEnabled  = true;   // salto anti-ostacolo quando bloccata
    float accuracy     = 0.55f;  // 1 = colpo perfetto; <1 = dispersione
    float reactionTime = 0.4f;   // ritardo primo colpo all'acquisizione

    // ── Comportamento tattico dal profilo (16_AiBehavior) ─────────────
    float aggression      = 0.65f; // 1 = chiude la distanza, 0 = tiene il raggio
    float retreatHpThresh = 0.0f;  // frazione HP sotto cui si disimpegna (0 = mai)
    float coverPreference = 0.0f;  // probabilità di fase evasiva a fine peek
    float peekMin = 0.6f, peekMax = 1.1f; // durata finestra di fuoco (s)
    float hideMin = 0.8f, hideMax = 1.8f; // durata finestra evasiva (s)
    float flankChance     = 0.0f;  // probabilità di approccio laterale in Hunt

    // Roll attivo (abilità "roll", 16 est.): scatto in corso
    float rollTimer = 0.0f;
    float rollVX = 0.0f, rollVZ = 0.0f;

    // Stato runtime peek/hide + flank
    // Cover point scelto per la fase evasiva (18_AiMapConsumption)
    bool  hasCover = false;
    float coverX = 0.0f, coverZ = 0.0f;
    float searchTimer = 0.0f;   // tempo in Search: oltre il limite → Patrol
    float exposeTimer = 0.0f;   // countdown della fase corrente
    bool  evading     = false;  // true = fase evasiva (non spara)
    bool  flankActive = false;  // sta raggiungendo il punto di fiancheggiamento
    float flankX = 0.0f, flankZ = 0.0f;

    float seekSpeed      = 3.5f;
    float lastKnownX     = 0.0f;
    float lastKnownZ     = 0.0f;
    bool  hasLastKnown   = false;

    float strafeTimer = 1.4f;
    float strafeSign  = 1.0f;
    float velY        = 0.0f;
    bool  stationary  = false;

    float stuckTimer  = 0.0f;
    float prevX       = 0.0f;
    float prevZ       = 0.0f;

    // Search: punto random sulla mappa dove l'AI sta cercando
    float searchX     = 0.0f;
    float searchZ     = 0.0f;

    // Alert→Hunt timer: quanto tempo resta in Alert senza LOS prima di passare a Hunt
    float alertTimer  = 0.0f;

    AiState state     = AiState::Patrol;
    bool    goingToB  = true;

    // ── Time-slicing sensing (Fase 4) ─────────────────────────────────
    // La ricerca target O(N²) + LOS gira solo 1 tick su AI_SENSE_INTERVAL
    // (scaglionata per entità); fra un sensing e l'altro l'AI riusa questo
    // bersaglio cachato per mirare/muoversi. 0 = nessun bersaglio.
    EntityId targetEntity = 0;
};

} // namespace mini