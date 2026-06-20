#pragma once

namespace mini { class Mesh; class Texture; }

namespace mini
{

// Componente che fa sparare automaticamente un'entita' verso i nemici di team 1.
// AiSystem aggiorna il cooldown e genera proiettili.
struct AiComponent
{
    float    shootCooldown  = 0.0f;   // secondi rimanenti prima del prossimo sparo
    float    shootInterval  = 2.5f;   // secondi tra uno sparo e l'altro
    float    aggroRange     = 14.0f;  // raggio entro cui attaccare

    // Risorse per il proiettile (non-owning pointers)
    Mesh*    bulletMesh     = nullptr;
    Texture* bulletTexture  = nullptr;
    float    bulletR        = 1.0f;   // colore proiettile nemico (rosso)
    float    bulletG        = 0.25f;
    float    bulletB        = 0.1f;
};

} // namespace mini