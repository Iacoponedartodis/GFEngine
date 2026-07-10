#pragma once
#include <SDL2/SDL.h>

#include <nlohmann/json.hpp>

#include <string>
#include <algorithm>
#include <vector>
#include <filesystem>
#include <fstream>
#include <iostream>

namespace mini
{

// Modalità selezionabili nel PreMatch (indici stabili per UI/preset, ADR-014)
inline const char* const* matchModeNames()
{
    static const char* names[] = { "Conquista", "Assalto", "Difesa" };
    return names;
}
inline const char* matchModeId(int index)
{
    switch (index) { case 1: return "assault"; case 2: return "defense";
                     default: return "conquest"; }
}
constexpr int MATCH_MODE_COUNT = 3;

struct MatchSettings
{
    // ── Modalità (0=Conquista, 1=Assalto, 2=Difesa) ───────────────────
    int   modeIndex     = 0;

    // ── Mappa attiva (R3: niente più "firebase" hardcoded nei mode) ───
    // Id risolto da Application dalla lista registry (mapIndex = riga UI).
    int         mapIndex = 0;
    std::string mapId    = "firebase";

    // ── Regole partita ────────────────────────────────────────────────
    int   team1Tickets  = 5;
    int   team2Tickets  = 10;
    int   team1AiCount  = 1;
    int   team2AiCount  = 6;
    float playerHp      = 100.0f;
    float respawnDelay  = 4.0f;
    std::string presetName;

    // ── Loadout giocatore ─────────────────────────────────────────────
    std::string primaryWeaponId;           // ID arma primaria (da registry)
    std::string secondaryWeaponId;         // ID arma secondaria (vuoto = nessuna)
    std::vector<std::string> abilityIds;   // abilità attive scelte (max 2)
    std::string gadgetId;                  // gadget passivo scelto (max 1)
};

struct UserPresets
{
    static constexpr int MAX = 8;
    std::vector<MatchSettings> list;

    UserPresets() { list.resize(MAX); loadFromFile(); }

    void save(const MatchSettings& s, int slot)
    {
        if (slot < 0 || slot >= MAX) return;
        list[slot] = s;
        saveToFile();
    }

    void remove(int slot)
    {
        if (slot < 0 || slot >= (int)list.size()) return;
        list[slot].presetName.clear();
        saveToFile();
    }

    const MatchSettings* get(int slot) const
    {
        if (slot < 0 || slot >= (int)list.size()) return nullptr;
        if (list[slot].presetName.empty()) return nullptr;
        return &list[slot];
    }

    MatchSettings* getMutable(int slot)
    {
        if (slot < 0 || slot >= (int)list.size()) return nullptr;
        if (list[slot].presetName.empty()) return nullptr;
        return &list[slot];
    }

    // ── Persistenza JSON in <exe>/user_presets/match/ ────────────────────
    // FUORI da data/: il post-build CMake azzera e ricopia la data/ di
    // output a ogni build (KI #19 — i preset in data/ venivano distrutti).
    // Percorso basato sull'exe (SDL_GetBasePath) -- funziona da qualsiasi CWD.
    static std::string exeDir()
    {
        char* base = SDL_GetBasePath();
        std::string dir = (base ? base : "./");
        SDL_free(base);
        return dir;
    }

    static std::string getPresetDir()
    { return exeDir() + "user_presets/match"; }

    // Vecchia posizione (pre KI #19), letta solo in migrazione.
    static std::string legacySlotPath(int slot)
    {
        return exeDir() + "data/presets/match/slot_"
             + std::to_string(slot) + ".json";
    }

    static void ensureDir()
    {
        std::error_code ec;
        std::filesystem::create_directories(getPresetDir(), ec);
        if (ec)
            std::cerr << "[Presets] Impossibile creare " << getPresetDir()
                      << ": " << ec.message() << "\n";
    }

    static std::string slotPath(int slot)
    {
        return getPresetDir() + "/slot_" + std::to_string(slot) + ".json";
    }

    void saveToFile() const
    {
        ensureDir();
        for (int i = 0; i < MAX; ++i)
        {
            const std::string path = slotPath(i);
            if (list[i].presetName.empty())
            {
                std::error_code ec;
                std::filesystem::remove(path, ec);
                continue;
            }
            nlohmann::json j;
            const MatchSettings& s = list[i];
            j["name"]             = s.presetName;
            j["team1_tickets"]    = s.team1Tickets;
            j["team2_tickets"]    = s.team2Tickets;
            j["team1_ai_count"]   = s.team1AiCount;
            j["team2_ai_count"]   = s.team2AiCount;
            j["player_hp"]        = s.playerHp;
            j["respawn_delay"]    = s.respawnDelay;
            j["mode_index"]       = s.modeIndex;
            // Mappa per ID (KI #20): l'indice nella lista ordinata cambiava
            // significato aggiungendo/rinominando una mappa.
            j["map_id"]           = s.mapId;
            // Loadout (KI #20: prima non era persistito affatto)
            j["primary_weapon"]   = s.primaryWeaponId;
            j["secondary_weapon"] = s.secondaryWeaponId;
            j["abilities"]        = s.abilityIds;
            j["gadget"]           = s.gadgetId;

            std::ofstream f(path);
            if (!f.is_open())
            {
                std::cerr << "[Presets] Impossibile scrivere: " << path << "\n";
                continue;
            }
            f << j.dump(2) << "\n";
            std::cout << "[Presets] Salvato slot " << i << ": "
                      << s.presetName << "\n";
        }
    }

    void loadFromFile()
    {
        list.resize(MAX);
        ensureDir();

        int loaded = 0;
        for (int i = 0; i < MAX; ++i)
        {
            list[i] = {};

            // Nuova posizione; se assente, migrazione dalla legacy in data/
            std::ifstream f(slotPath(i));
            if (!f.is_open()) f.open(legacySlotPath(i));
            if (!f.is_open()) continue;

            nlohmann::json j;
            try { f >> j; }
            catch (const std::exception& e)
            {
                std::cerr << "[Presets] Slot " << i << " corrotto: "
                          << e.what() << "\n";
                continue;
            }

            MatchSettings& s = list[i];
            s.presetName = j.value("name", std::string());
            if (s.presetName.empty()) { s = {}; continue; }

            s.team1Tickets = j.value("team1_tickets", s.team1Tickets);
            s.team2Tickets = j.value("team2_tickets", s.team2Tickets);
            s.team1AiCount = j.value("team1_ai_count", s.team1AiCount);
            s.team2AiCount = j.value("team2_ai_count", s.team2AiCount);
            s.playerHp     = j.value("player_hp", s.playerHp);
            s.respawnDelay = j.value("respawn_delay", s.respawnDelay);
            s.modeIndex    = std::clamp(j.value("mode_index", 0),
                                        0, MATCH_MODE_COUNT - 1);

            // Mappa: preferisci l'id; i file legacy hanno solo map_index
            // (risolto/clampato da PreMatchMenu al caricamento del preset).
            s.mapId    = j.value("map_id", std::string());
            s.mapIndex = std::max(0, j.value("map_index", 0));

            s.primaryWeaponId   = j.value("primary_weapon", std::string());
            s.secondaryWeaponId = j.value("secondary_weapon", std::string());
            s.gadgetId          = j.value("gadget", std::string());
            s.abilityIds.clear();
            if (j.contains("abilities") && j["abilities"].is_array())
                for (const auto& a : j["abilities"])
                    if (a.is_string()) s.abilityIds.push_back(a.get<std::string>());

            ++loaded;
        }
        if (loaded > 0)
        {
            std::cout << "[Presets] Caricati " << loaded << " preset da "
                      << getPresetDir() << "\n";
            // Persisti subito nella nuova posizione (completa la migrazione
            // dalla legacy data/, che la prossima build cancellerà comunque).
            saveToFile();
        }
    }
};

} // namespace mini