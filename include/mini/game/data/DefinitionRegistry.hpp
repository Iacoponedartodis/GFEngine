#pragma once
#include "mini/game/data/Definitions.hpp"
#include "mini/ecs/components/HitboxComponent.hpp"
#include <unordered_map>
#include <string>
#include <optional>
#include <iostream>

namespace mini
{

// Registro globale di tutte le definizioni data-driven.
// Carica JSON all'avvio, espone accesso per ID.
// Progettato per essere passato per riferimento a tutti i sistemi che ne hanno bisogno.
class DefinitionRegistry
{
public:
    // Carica tutte le definizioni dalla struttura cartelle standard.
    // Chiama questa funzione una volta all'avvio, prima di creare il mondo.
    void loadAll(const std::string& dataRoot = "data");

    // Ricarica tutto (utile dall'editor).
    void reload(const std::string& dataRoot = "data") { loadAll(dataRoot); }

    // ── Accesso per ID ────────────────────────────────────────────────
    [[nodiscard]] const WeaponDef*    getWeapon  (const std::string& id) const;
    [[nodiscard]] const EnemyDef*     getEnemy   (const std::string& id) const;
    [[nodiscard]] const MapDef*       getMap     (const std::string& id) const;
    [[nodiscard]] const AiProfileDef*   getAiProfile  (const std::string& id) const;
    [[nodiscard]] const HitboxProfile*  getHitboxProfile(const std::string& id) const;

    // ── Accesso a tutte le definizioni (per l'editor) ─────────────────
    [[nodiscard]] const auto& weapons()    const { return m_weapons; }
    [[nodiscard]] const auto& enemies()    const { return m_enemies; }
    [[nodiscard]] const auto& maps()       const { return m_maps; }
    [[nodiscard]] const auto& aiProfiles()    const { return m_aiProfiles; }
    [[nodiscard]] const auto& hitboxProfiles() const { return m_hitboxProfiles; }

    [[nodiscard]] bool isLoaded() const { return m_loaded; }

private:
    std::unordered_map<std::string, WeaponDef>    m_weapons;
    std::unordered_map<std::string, EnemyDef>     m_enemies;
    std::unordered_map<std::string, MapDef>       m_maps;
    std::unordered_map<std::string, AiProfileDef>  m_aiProfiles;
    std::unordered_map<std::string, HitboxProfile>  m_hitboxProfiles;
    bool m_loaded = false;

    void loadWeapons      (const std::string& dir);
    void loadHitboxProfiles(const std::string& dir);
    void loadEnemies   (const std::string& dir);
    void loadMaps      (const std::string& dir);
    void loadAiProfiles(const std::string& dir);
};

} // namespace mini