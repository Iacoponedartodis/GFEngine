// DefinitionRename.cpp — implementazione del comando di rinomina (ADR-010).
#include "util/DefinitionRename.hpp"
#include "util/JsonSave.hpp"

#include <nlohmann/json.hpp>
#include <filesystem>
#include <iostream>
#include <vector>

namespace fs = std::filesystem;
using json   = nlohmann::json;

namespace editor::rename
{

static const char* subdirOf(Category c)
{
    switch (c)
    {
    case Category::Weapon:        return "weapons";
    case Category::Enemy:         return "enemies";
    case Category::Ally:          return "allies";
    case Category::HitboxProfile: return "hitboxes";
    case Category::AiProfile:     return "ai";
    case Category::Ability:       return "abilities";
    case Category::Map:           return "maps";
    case Category::Character:     return "characters";
    case Category::Vehicle:       return "vehicles";
    case Category::Objective:     return "objectives";
    case Category::Mission:       return "missions";
    case Category::Class:         return "classes";
    }
    return "";
}

static bool isFilenameSafe(const std::string& s)
{
    if (s.empty()) return false;
    for (char c : s)
        if (c=='\\'||c=='/'||c==':'||c=='*'||c=='?'||c=='"'||c=='<'||c=='>'||c=='|')
            return false;
    return true;
}

// Sostituisce oldId→newId in una stringa JSON se combacia esattamente.
static bool patchString(json& j, const char* key,
                        const std::string& oldId, const std::string& newId)
{
    if (!j.contains(key) || !j[key].is_string()) return false;
    if (j[key].get<std::string>() != oldId) return false;
    j[key] = newId;
    return true;
}

// Sostituisce le occorrenze esatte in un array di stringhe.
static bool patchArray(json& j, const char* key,
                       const std::string& oldId, const std::string& newId)
{
    if (!j.contains(key) || !j[key].is_array()) return false;
    bool changed = false;
    for (auto& v : j[key])
        if (v.is_string() && v.get<std::string>() == oldId)
        { v = newId; changed = true; }
    return changed;
}

// Applica a un file entità (enemy/ally) i campi che riferiscono la categoria.
static bool patchEntityRefs(json& j, Category cat,
                            const std::string& oldId, const std::string& newId)
{
    bool ch = false;
    switch (cat)
    {
    case Category::Weapon:
        ch |= patchArray (j, "weapons", oldId, newId);
        ch |= patchString(j, "weapon",  oldId, newId); // legacy singolo
        if (j.contains("weapon_display") && j["weapon_display"].is_object())
            ch |= patchString(j["weapon_display"], "id", oldId, newId);
        break;
    case Category::HitboxProfile:
        ch |= patchString(j, "hitbox_profile", oldId, newId);
        break;
    case Category::AiProfile:
        ch |= patchString(j, "ai_profile", oldId, newId);
        break;
    case Category::Ability:
        ch |= patchArray(j, "abilities", oldId, newId);
        break;
    default: break;
    }
    return ch;
}

std::string renameDefinition(const std::string& dataDir, Category cat,
                             const std::string& oldId, const std::string& newId,
                             int* outUpdatedRefs)
{
    if (outUpdatedRefs) *outUpdatedRefs = 0;

    // ── a. Validazione ────────────────────────────────────────────────────
    if (!isFilenameSafe(newId))
        return "Nuovo id non valido (vuoto o caratteri vietati \\/:*?\"<>|).";
    if (newId == oldId)
        return "Il nuovo id coincide con quello attuale.";

    const fs::path folder  = fs::path(dataDir) / subdirOf(cat);
    const fs::path oldPath = folder / (oldId + ".json");
    const fs::path newPath = folder / (newId + ".json");

    std::error_code ec;
    if (!fs::exists(oldPath, ec))
        return "File di origine non trovato: " + oldPath.string();
    if (fs::exists(newPath, ec))
        return "Esiste gia' una definizione con id '" + newId + "'.";

    // ── b. Rinomina fisica del file ───────────────────────────────────────
    fs::rename(oldPath, newPath, ec);
    if (ec)
        return "Rinomina file fallita: " + ec.message();

    // ── c. Pulizia id deprecato nel file rinominato (ADR-001) ─────────────
    editor::jsonsave::saveJsonRMW(newPath.string(), [](json& j) {
        bool ch = false;
        if (j.contains("id"))         { j.erase("id");         ch = true; }
        if (j.contains("profile_id")) { j.erase("profile_id"); ch = true; }
        return ch;
    });

    // ── d. Sweep dei cross-reference (mappa esplicita per categoria) ──────
    int updated = 0;
    auto sweepDir = [&](const char* subdir, auto&& patchFile)
    {
        const fs::path dir = fs::path(dataDir) / subdir;
        if (!fs::exists(dir, ec)) return;
        for (auto& entry : fs::directory_iterator(dir, ec))
        {
            if (entry.path().extension() != ".json") continue;
            bool changed = false;
            editor::jsonsave::saveJsonRMW(entry.path().string(), [&](json& j) {
                changed = patchFile(j);
                return changed;
            });
            if (changed)
            {
                ++updated;
                std::cout << "[Rename] Aggiornato riferimento in: "
                          << entry.path().filename().string() << "\n";
            }
        }
    };

    switch (cat)
    {
    case Category::Weapon:
    case Category::HitboxProfile:
    case Category::AiProfile:
    case Category::Ability:
        sweepDir("enemies", [&](json& j){ return patchEntityRefs(j, cat, oldId, newId); });
        sweepDir("allies",  [&](json& j){ return patchEntityRefs(j, cat, oldId, newId); });
        break;
    case Category::Enemy:
        sweepDir("maps", [&](json& j){ return patchArray(j, "enemy_types", oldId, newId); });
        break;
    case Category::Ally:
        sweepDir("maps", [&](json& j){ return patchArray(j, "ally_types", oldId, newId); });
        break;
    case Category::Map:
        // Nessun cross-reference nei dati. R3 (2026-07-10): la mappa attiva
        // arriva da MatchSettings/PreMatch — i mode non hardcodano più id.
        // Resta "firebase" solo come FALLBACK di default: rinominarla è
        // sicuro per le partite avviate dal PreMatch, ma il sandbox boot
        // userebbe il fallback inesistente finché non selezioni una mappa.
        if (oldId == "firebase")
            std::cerr << "[Rename] AVVISO: 'firebase' e' il fallback di "
                         "default di MatchSettings.mapId.\n";
        break;
    case Category::Character:
        break;
    case Category::Vehicle:
        // maps: vehicle_spawns[].vehicle_id
        sweepDir("maps", [&](json& j){
            bool ch = false;
            if (j.contains("vehicle_spawns") && j["vehicle_spawns"].is_array())
                for (auto& vs : j["vehicle_spawns"])
                    ch |= patchString(vs, "vehicle_id", oldId, newId);
            return ch;
        });
        break;

    case Category::Objective:
        // missions: primary_objectives[] / optional_objectives[]
        sweepDir("missions", [&](json& j){
            bool ch = false;
            ch |= patchArray(j, "primary_objectives",  oldId, newId);
            ch |= patchArray(j, "optional_objectives", oldId, newId);
            return ch;
        });
        // objectives: activation.objective (dipendenza) e linked_objectives[].
        // Un obiettivo può referenziarne un altro: senza questo sweep, rinominare
        // un prerequisito lascerebbe l'obiettivo dipendente inattivabile PER SEMPRE
        // (il gate ADR-018 lo direbbe, ma il rename deve non romperlo affatto).
        sweepDir("objectives", [&](json& j){
            bool ch = false;
            if (j.contains("activation") && j["activation"].is_object())
                ch |= patchString(j["activation"], "objective", oldId, newId);
            ch |= patchArray(j, "linked_objectives", oldId, newId);
            return ch;
        });
        break;

    case Category::Class:
        // Nessun cross-ref dentro data/: la classe e' referenziata solo dai preset
        // utente e dal flag --class. Un preset con id stantio ricade su "(nessuna)".
        break;

    case Category::Mission:
        // Nessun cross-ref dentro data/: la missione è referenziata solo dai
        // preset utente (fuori da data/, KI #19) e dal flag --mission.
        break;
    }

    if (outUpdatedRefs) *outUpdatedRefs = updated;
    std::cout << "[Rename] '" << oldId << "' -> '" << newId << "' ("
              << subdirOf(cat) << ", " << updated << " riferimenti aggiornati)\n";
    return "";
}

} // namespace editor::rename
