#pragma once
#include <string>
#include <vector>
#include <fstream>
#include <cstdio>
#include <iostream>

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

    UserPresets()
    {
        list.resize(MAX);
        loadFromFile(); // carica all'avvio
    }

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

    // ── Persistenza su file ──────────────────────────────────────────
    static constexpr const char* FILENAME = "presets.cfg";

    void saveToFile() const
    {
        std::ofstream f(FILENAME);
        if (!f.is_open()) return;
        for (int i = 0; i < MAX; ++i)
        {
            const auto& s = list[i];
            if (s.presetName.empty())
            {
                f << "---\n"; // slot vuoto
            }
            else
            {
                f << s.presetName << "\n"
                  << s.team1Tickets << " " << s.team2Tickets << " "
                  << s.team1AiCount << " " << s.team2AiCount << " "
                  << s.playerHp << " " << s.respawnDelay << "\n";
            }
        }
        f.close();
        std::cout << "[Presets] Salvati su " << FILENAME << std::endl;
    }

    void loadFromFile()
    {
        list.resize(MAX);
        std::ifstream f(FILENAME);
        if (!f.is_open()) return;

        for (int i = 0; i < MAX; ++i)
        {
            std::string line;
            if (!std::getline(f, line)) break;

            // Rimuovi \r se presente
            if (!line.empty() && line.back() == '\r') line.pop_back();

            if (line == "---" || line.empty())
            {
                list[i] = {};
                continue;
            }

            list[i].presetName = line;
            std::string vals;
            if (std::getline(f, vals))
            {
                if (!vals.empty() && vals.back() == '\r') vals.pop_back();
                std::sscanf(vals.c_str(), "%d %d %d %d %f %f",
                    &list[i].team1Tickets, &list[i].team2Tickets,
                    &list[i].team1AiCount, &list[i].team2AiCount,
                    &list[i].playerHp, &list[i].respawnDelay);
            }
        }
        std::cout << "[Presets] Caricati da " << FILENAME << std::endl;
    }
};

} // namespace mini