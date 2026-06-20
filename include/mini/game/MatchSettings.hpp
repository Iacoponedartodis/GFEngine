#pragma once
#include <array>

namespace mini
{

struct MatchSettings
{
    // ── Squadre ──────────────────────────────────────────────────────
    int   team1Tickets   = 5;
    int   team2Tickets   = 10;
    float respawnDelay   = 4.0f;

    // ── AI ───────────────────────────────────────────────────────────
    float aiSpeed        = 3.5f;
    float aiFireInterval = 1.8f;
    float aiRange        = 12.0f;

    // ── Giocatore ────────────────────────────────────────────────────
    float playerHp    = 100.0f;
    float playerSpeed = 5.0f;
};

// ── Preset built-in (fuori dalla struct, evita problema MSVC) ────────

struct MatchPreset
{
    const char*   name;
    MatchSettings settings;
};

inline std::array<MatchPreset, 4> getBuiltinPresets()
{
    std::array<MatchPreset, 4> presets;

    presets[0].name                    = "Tutorial";
    presets[0].settings.team1Tickets   = 10;
    presets[0].settings.team2Tickets   = 4;
    presets[0].settings.respawnDelay   = 5.0f;
    presets[0].settings.aiSpeed        = 2.5f;
    presets[0].settings.aiFireInterval = 2.8f;
    presets[0].settings.aiRange        = 8.0f;
    presets[0].settings.playerHp       = 150.0f;
    presets[0].settings.playerSpeed    = 5.0f;

    presets[1].name                    = "Standard";
    presets[1].settings.team1Tickets   = 5;
    presets[1].settings.team2Tickets   = 10;
    presets[1].settings.respawnDelay   = 4.0f;
    presets[1].settings.aiSpeed        = 3.5f;
    presets[1].settings.aiFireInterval = 1.8f;
    presets[1].settings.aiRange        = 12.0f;
    presets[1].settings.playerHp       = 100.0f;
    presets[1].settings.playerSpeed    = 5.0f;

    presets[2].name                    = "Hardcore";
    presets[2].settings.team1Tickets   = 3;
    presets[2].settings.team2Tickets   = 20;
    presets[2].settings.respawnDelay   = 6.0f;
    presets[2].settings.aiSpeed        = 5.0f;
    presets[2].settings.aiFireInterval = 1.2f;
    presets[2].settings.aiRange        = 16.0f;
    presets[2].settings.playerHp       = 75.0f;
    presets[2].settings.playerSpeed    = 5.0f;

    presets[3].name                    = "Horde";
    presets[3].settings.team1Tickets   = 1;
    presets[3].settings.team2Tickets   = 50;
    presets[3].settings.respawnDelay   = 0.0f;
    presets[3].settings.aiSpeed        = 4.5f;
    presets[3].settings.aiFireInterval = 1.5f;
    presets[3].settings.aiRange        = 14.0f;
    presets[3].settings.playerHp       = 200.0f;
    presets[3].settings.playerSpeed    = 6.0f;

    return presets;
}

} // namespace mini