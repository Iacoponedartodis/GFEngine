#pragma once
#include <string>
#include <vector>
#include <glm/glm.hpp>

namespace mini
{

// Una singola zona di hitbox (AABB locale relativo al pivot dell'entità)
struct HitZone
{
    std::string name;               // "head", "torso", "left_arm", ...
    glm::vec3   offset    = {0,0,0}; // offset dal pivot dell'entità (world-up = Y)
    glm::vec3   halfExtents = {0.2f, 0.3f, 0.2f};
    float       damageMultiplier = 1.0f;
    bool        debugVisible     = true;
};

// Profilo hitbox completo per un tipo di entità.
// Viene condiviso (via pointer) tra tutte le istanze dello stesso tipo.
struct HitboxProfile
{
    std::string          profileId;
    std::vector<HitZone> zones;
};

// Componente ECS: assegnato alle entità che hanno hitbox a zone.
// Il puntatore punta a un HitboxProfile caricato dal DefinitionRegistry
// (ownership nel registry, non nell'entità).
struct HitboxComponent
{
    const HitboxProfile* profile = nullptr;
};

} // namespace mini