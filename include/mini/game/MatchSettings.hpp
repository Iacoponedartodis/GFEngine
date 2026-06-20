#pragma once
#include <string>
#include <vector>

namespace mini
{

struct MatchSettings
{
    int   team1Tickets  = 5;
    int   team2Tickets  = 10;
    int   team1AiCount  = 1;
    int   team2AiCount  = 6;
    float playerHp      = 100.0f;
    float respawnDelay  = 4.0f;
    std::string presetName;
};

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

    void remove(int slot)
    {
        if (slot < 0 || slot >= (int)list.size()) return;
        list[slot].presetName.clear();
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
};

} // namespace mini