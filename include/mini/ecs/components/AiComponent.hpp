#pragma once

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

    float patrolAx = 0, patrolAz = 0;
    float patrolBx = 0, patrolBz = 0;
    float patrolSpeed = 2.0f;

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
};

} // namespace mini