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
    // Pubblica (ADR-048): l'EDITOR carica i soli prefab per la lista di piazzamento e
    // l'anteprima, senza pagare `loadAll` e — soprattutto — senza un secondo parser che
    // divergerebbe da questo. Chiamata anche da `loadAll`, PRIMA delle mappe.
    void loadPrefabs(const std::string& dir);
    // Pubblica per lo stesso motivo dei prefab (ADR-055): l'editor strutture carica
    // i soli tipi, senza `loadAll` e senza un secondo parser che divergerebbe.
    void loadStructureTypes(const std::string& dir);
    void reload (const std::string& dataRoot = "data") { loadAll(dataRoot); }

    [[nodiscard]] const AbilityDef*    getAbility      (const std::string& id) const;
    [[nodiscard]] const WeaponDef*     getWeapon       (const std::string& id) const;
    [[nodiscard]] const AiProfileDef*  getAiProfile    (const std::string& id) const;
    [[nodiscard]] const EnemyDef*      getEnemy        (const std::string& id) const;
    [[nodiscard]] const EnemyDef*      getAlly         (const std::string& id) const;
    [[nodiscard]] const MapDef*        getMap          (const std::string& id) const;
    [[nodiscard]] const HitboxProfile* getHitboxProfile(const std::string& id) const;
    [[nodiscard]] const PlayerDef*     getPlayerDef    (const std::string& id) const;
    [[nodiscard]] const VehicleDef*    getVehicle      (const std::string& id) const;
    [[nodiscard]] const ClassDef*      getClass        (const std::string& id) const;
    [[nodiscard]] const CommanderDef*  getCommander    (const std::string& id) const;
    [[nodiscard]] const ObjectiveDef*  getObjective    (const std::string& id) const;
    [[nodiscard]] const MissionDef*    getMission      (const std::string& id) const;
    [[nodiscard]] const PrefabDef*     getPrefab       (const std::string& id) const;
    // Tutti i prefab (per l'editor: lista di piazzamento e validazione).
    [[nodiscard]] const std::unordered_map<std::string, PrefabDef>& prefabs() const { return m_prefabs; }
    // Tipi di struttura (ADR-055): libreria per il menu `+ Struttura` e per l'editor.
    [[nodiscard]] const std::unordered_map<std::string, StructureTypeDef>&
        structureTypes() const { return m_structureTypes; }
    // Solo per il COLLAUDO: inserisce/rimuove un tipo senza passare dal disco.
    // Serve a verificare l'espansione degli assemblaggi in un test headless senza
    // creare file veri, che poi resterebbero nella libreria dell'utente.
    void addStructureTypeForTest(const StructureTypeDef& t) { m_structureTypes[t.id] = t; }
    void removeStructureTypeForTest(const std::string& id) { m_structureTypes.erase(id); }

    [[nodiscard]] const StructureTypeDef* getStructureType(const std::string& id) const
    {
        auto it = m_structureTypes.find(id);
        return (it == m_structureTypes.end()) ? nullptr : &it->second;
    }

    [[nodiscard]] const auto& abilities()      const { return m_abilities; }
    [[nodiscard]] const auto& weapons()        const { return m_weapons; }
    [[nodiscard]] const auto& aiProfiles()     const { return m_aiProfiles; }
    [[nodiscard]] const auto& enemies()        const { return m_enemies; }
    [[nodiscard]] const auto& allies()         const { return m_allies; }
    [[nodiscard]] const auto& maps()           const { return m_maps; }
    [[nodiscard]] const auto& hitboxProfiles() const { return m_hitboxProfiles; }
    [[nodiscard]] const auto& playerDefs()     const { return m_playerDefs; }
    [[nodiscard]] const auto& vehicles()       const { return m_vehicles; }
    [[nodiscard]] const auto& classes()       const { return m_classes; }
    [[nodiscard]] const auto& commanders()     const { return m_commanders; }
    [[nodiscard]] const auto& objectives()     const { return m_objectives; }
    [[nodiscard]] const auto& missions()       const { return m_missions; }
    [[nodiscard]] const auto& unknownKeys()    const { return m_unknownKeys; }

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
    std::unordered_map<std::string, PrefabDef>     m_prefabs;   // ADR-048
    std::unordered_map<std::string, StructureTypeDef> m_structureTypes;   // ADR-055
    std::unordered_map<std::string, HitboxProfile> m_hitboxProfiles;
    std::unordered_map<std::string, PlayerDef>     m_playerDefs;
    std::unordered_map<std::string, ClassDef>      m_classes;      // doc 14
    std::unordered_map<std::string, CommanderDef>  m_commanders;   // ADR-044
    std::unordered_map<std::string, ObjectiveDef>  m_objectives;   // ADR-019
    std::unordered_map<std::string, MissionDef>    m_missions;     // ADR-019

    // Chiavi JSON che nessun loader legge, per file (ADR-018 "campi fantasma").
    // Popolata dai loader mentre il JSON e' ancora in mano: dopo il parsing
    // l'informazione non esiste piu' nel registry.
    std::unordered_map<std::string, std::vector<std::string>> m_unknownKeys;
    std::unordered_map<std::string, VehicleDef>    m_vehicles;
    bool m_loaded = false;

    void loadAbilities      (const std::string& dir);
    void loadWeapons        (const std::string& dir);
    void loadAiProfiles     (const std::string& dir);
    void loadEnemies        (const std::string& dir);
    void loadAllies         (const std::string& dir);
    void loadMaps           (const std::string& dir);
    void loadHitboxProfiles (const std::string& dir);
    void loadPlayerDefs     (const std::string& dir);
    void loadVehicles       (const std::string& dir);
    void loadClasses        (const std::string& dir);   // 14_ClassSystem
    void loadCommanders     (const std::string& dir);   // ADR-044
    void loadObjectives     (const std::string& dir);   // ADR-019
    void loadMissions       (const std::string& dir);   // ADR-019
};

} // namespace mini
