#pragma once

namespace mini
{

// Componente che marca un'entita' come proiettile.
// CombatSystem usa questo per aggiornare la lifetime e rilevare collisioni.
struct BulletComponent
{
    float damage    = 25.0f; // danno inflitto all'impatto
    float lifetime  =  3.0f; // secondi rimanenti prima di scomparire
    int   ownerTeam =  1;    // i proiettili NON colpiscono entita' dello stesso team
};

} // namespace mini