#pragma once

namespace mini { class Mesh; class Texture; }

namespace mini
{

enum class AiState : unsigned char { Patrol, Alert, Seek };

struct AiComponent
{
    // Combattimento
    float    shootCooldown  = 0.0f;
    float    shootInterval  = 2.5f;
    float    aggroRange     = 16.0f;
    Mesh*    bulletMesh     = nullptr;
    Texture* bulletTexture  = nullptr;
    float    bulletR = 1.0f, bulletG = 0.25f, bulletB = 0.1f;

    // Pattuglia
    float patrolAx = 0.0f, patrolAz = 0.0f;
    float patrolBx = 0.0f, patrolBz = 0.0f;
    float patrolSpeed = 2.0f;

    // Seek
    float seekSpeed      = 3.5f;
    float lastKnownX     = 0.0f;
    float lastKnownZ     = 0.0f;
    bool  hasLastKnown   = false;

    // Strafing
    float strafeTimer = 1.4f;
    float strafeSign  = 1.0f;

    // Gravità AI
    float velY       = 0.0f;

    // Se true: il nemico non si muove (solo rotazione + sparo)
    bool stationary  = false;

    // Anti-stuck: se la posizione non cambia per >1s, cambia direzione
    float stuckTimer = 0.0f;
    float prevX      = 0.0f;
    float prevZ      = 0.0f;

    // Stato interno
    AiState state      = AiState::Patrol;
    bool    goingToB   = true;
    float   alertTimer = 0.0f;
    float   seekTimer  = 0.0f;
};

} // namespace mini