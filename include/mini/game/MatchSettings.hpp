#pragma once
#include <string>
#include <vector>

namespace mini
{

struct MatchSettings
{
    // ── Partita ───────────────────────────────────────────────────────
    int   team1Tickets  = 5;     // vite alleati (giocatore incluso)
    int   team2Tickets  = 10;    // vite nemici

    // ── Unità ────────────────────────────────────────────────────────
    int   team1AiCount  = 1;     // quante AI alleate spawna ConquestMode
    int   team2AiCount  = 6;     // quante AI nemiche spawna ConquestMode

    // ── Giocatore ────────────────────────────────────────────────────
    float playerHp      = 100.0f;
    float respawnDelay  = 4.0f;

    // ── Nome preset (vuoto = non salvato) ────────────────────────────
    std::string presetName;
};

// ── Preset salvati dall'utente (tenuti in memoria durante la sessione) ──
struct UserPresets
{
    static constexpr int MAX = 8;
    std::vector<MatchSettings> list;

    void save(const MatchSettings& s, int slot)
    {
        if (slot < 0 || slot >= MAX) return;
        while ((int)list.size() <= slot) list.push_back({});
        list[slot] = s;
    }

    const MatchSettings* get(int slot) const
    {
        if (slot < 0 || slot >= (int)list.size()) return nullptr;
        return &list[slot];
    }
};

} // namespace mini