#pragma once
#include "mini/game/data/Definitions.hpp"
#include "mini/ecs/components/HitboxComponent.hpp"
#include <unordered_map>
#include <string>
#include <vector>
#include <iostream>

namespace mini
{

class DefinitionRegistry
{
public:
    void loadAll(const std::string& dataRoot = "data");
    void reload (const std::string& dataRoot = "data") { loadAll(dataRoot); }

    [[nodiscard]] const AbilityDef*    getAbility      (const std::string& id) const;
    [[nodiscard]] const WeaponDef*     getWeapon       (const std::string& id) const;
    [[nodiscard]] const AiProfileDef*  getAiProfile    (const std::string& id) const;
    [[nodiscard]] const EnemyDef*      getEnemy        (const std::string& id) const;
    [[nodiscard]] const EnemyDef*      getAlly         (const std::string& id) const;
    [[nodiscard]] const MapDef*        getMap          (const std::string& id) const;
    [[nodiscard]] const HitboxProfile* getHitboxProfile(const std::string& id) const;
    [[nodiscard]] const PlayerDef*     getPlayerDef    (const std::string& id) const;

    [[nodiscard]] const auto& abilities()      const { return m_abilities; }
    [[nodiscard]] const auto& weapons()        const { return m_weapons; }
    [[nodiscard]] const auto& aiProfiles()     const { return m_aiProfiles; }
    [[nodiscard]] const auto& enemies()        const { return m_enemies; }
    [[nodiscard]] const auto& allies()         const { return m_allies; }
    [[nodiscard]] const auto& maps()           const { return m_maps; }
    [[nodiscard]] const auto& hitboxProfiles() const { return m_hitboxProfiles; }
    [[nodiscard]] const auto& playerDefs()     const { return m_playerDefs; }

    // Filtra armi per fazione (Neutral = tutte)
    [[nodiscard]] std::vector<const WeaponDef*> weaponsForFaction(Faction f) const
    {
        std::vector<const WeaponDef*> out;
        for (auto& [id, w] : m_weapons)
            if (f == Faction::Neutral || w.faction == f) out.push_back(&w);
        return out;
    }

    [[nodiscard]] bool isLoaded() const { return m_loaded; }

private:
    std::unordered_map<std::string, AbilityDef>    m_abilities;
    std::unordered_map<std::string, WeaponDef>     m_weapons;
    std::unordered_map<std::string, AiProfileDef>  m_aiProfiles;
    std::unordered_map<std::string, EnemyDef>      m_enemies;
    std::unordered_map<std::string, EnemyDef>      m_allies;   // stessa struct, team=1
    std::unordered_map<std::string, MapDef>        m_maps;
    std::unordered_map<std::string, HitboxProfile> m_hitboxProfiles;
    std::unordered_map<std::string, PlayerDef>     m_playerDefs;
    bool m_loaded = false;

    void loadAbilities      (const std::string& dir);
    void loadWeapons        (const std::string& dir);
    void loadAiProfiles     (const std::string& dir);
    void loadEnemies        (const std::string& dir);
    void loadAllies         (const std::string& dir);
    void loadMaps           (const std::string& dir);
    void loadHitboxProfiles (const std::string& dir);
    void loadPlayerDefs     (const std::string& dir);
};

} // namespace mini
