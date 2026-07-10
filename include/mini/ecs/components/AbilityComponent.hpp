#pragma once
#include <string>
#include <vector>

namespace mini
{

// Stato runtime di una singola abilità su un'entità
struct AbilityState
{
    std::string abilityId;
    std::string type;         // "roll", ... (dal AbilityDef, risolto allo spawn)
    bool  active    = false;
    float cooldown  = 0.0f;   // tempo rimanente prima di poter riattivarsi
    float stateTime = 0.0f;   // tempo dall'attivazione corrente
    float param1    = 0.0f;   // es. velocità roll (m/s)
    float param2    = 0.0f;   // es. durata roll (s)
    float cooldownMax = 5.0f; // cooldown pieno dal AbilityDef
};

// Componente ECS: lista di abilità assegnate a un'entità
// Ogni entità può avere zero o più abilità (shield, roll, melee, ecc.)
struct AbilityComponent
{
    std::vector<AbilityState> states;  // una entry per ogni ability_id dell'EnemyDef

    [[nodiscard]] AbilityState* findAbility(const std::string& id)
    {
        for (auto& s : states) if (s.abilityId == id) return &s;
        return nullptr;
    }
    [[nodiscard]] const AbilityState* findAbility(const std::string& id) const
    {
        for (auto& s : states) if (s.abilityId == id) return &s;
        return nullptr;
    }
    [[nodiscard]] bool isActive(const std::string& id) const
    {
        auto* s = findAbility(id);
        return s && s->active;
    }
};

} // namespace mini