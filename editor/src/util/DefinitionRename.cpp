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
        // Nessun cross-reference nei dati. ATTENZIONE (ADR-008 residuo): i
        // game mode caricano ancora la mappa "firebase" per id hardcoded.
        if (oldId == "firebase")
            std::cerr << "[Rename] AVVISO: i game mode caricano 'firebase' "
                         "hardcoded — rinominarla rompe l'avvio partita.\n";
        break;
    case Category::Character:
        break;
    }

    if (outUpdatedRefs) *outUpdatedRefs = updated;
    std::cout << "[Rename] '" << oldId << "' -> '" << newId << "' ("
              << subdirOf(cat) << ", " << updated << " riferimenti aggiornati)\n";
    return "";
}

} // namespace editor::rename
