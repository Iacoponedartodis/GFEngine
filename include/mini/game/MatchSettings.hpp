#pragma once

// NOMINMAX impedisce a windows.h di definire le macro min/max
// che confliggono con std::min/std::max nel resto del codice
#ifdef _WIN32
  #define NOMINMAX
  #define WIN32_LEAN_AND_MEAN
  #include <windows.h>
#endif

#include <string>
#include <vector>
#include <fstream>
#include <iostream>
#include <cstdio>

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

    // ── Persistenza JSON in data/presets/match/ ──────────────────────────
    static constexpr const char* PRESET_DIR = "data/presets/match";

    static void ensureDir()
    {
#ifdef _WIN32
        CreateDirectoryA("data",              nullptr);
        CreateDirectoryA("data\\presets",     nullptr);
        CreateDirectoryA("data\\presets\\match", nullptr);
#else
        ::mkdir("data",               0755);
        ::mkdir("data/presets",       0755);
        ::mkdir("data/presets/match", 0755);
#endif
    }

    static std::string slotPath(int slot)
    {
        return std::string(PRESET_DIR) + "/slot_" + std::to_string(slot) + ".json";
    }

    void saveToFile() const
    {
        ensureDir();
        for (int i = 0; i < MAX; ++i)
        {
            std::string path = slotPath(i);
            if (list[i].presetName.empty())
            {
                std::remove(path.c_str());
                continue;
            }
            std::ofstream f(path);
            if (!f.is_open())
            {
                std::cerr << "[Presets] Impossibile scrivere: " << path << "\n";
                continue;
            }
            f << "{\n"
              << "  \"name\": \"" << list[i].presetName << "\",\n"
              << "  \"team1_tickets\": " << list[i].team1Tickets << ",\n"
              << "  \"team2_tickets\": " << list[i].team2Tickets << ",\n"
              << "  \"team1_ai_count\": " << list[i].team1AiCount << ",\n"
              << "  \"team2_ai_count\": " << list[i].team2AiCount << ",\n"
              << "  \"player_hp\": " << list[i].playerHp << ",\n"
              << "  \"respawn_delay\": " << list[i].respawnDelay << "\n"
              << "}\n";
            std::cout << "[Presets] Salvato slot " << i << ": "
                      << list[i].presetName << "\n";
        }
    }

    void loadFromFile()
    {
        list.resize(MAX);
        ensureDir();

        // Helper: estrae valore da JSON minimale
        auto val = [](const std::string& content, const std::string& key) -> std::string
        {
            auto pos = content.find("\"" + key + "\"");
            if (pos == std::string::npos) return "";
            pos = content.find(':', pos);
            if (pos == std::string::npos) return "";
            ++pos;
            while (pos < content.size() &&
                   (content[pos] == ' ' || content[pos] == '\t')) ++pos;
            if (pos < content.size() && content[pos] == '"')
            {
                ++pos;
                auto end = content.find('"', pos);
                return content.substr(pos, end - pos);
            }
            auto end = pos;
            while (end < content.size() &&
                   content[end] != ',' && content[end] != '\n' &&
                   content[end] != '}') ++end;
            return content.substr(pos, end - pos);
        };

        int loaded = 0;
        for (int i = 0; i < MAX; ++i)
        {
            std::ifstream f(slotPath(i));
            if (!f.is_open()) { list[i] = {}; continue; }

            std::string content((std::istreambuf_iterator<char>(f)),
                                 std::istreambuf_iterator<char>());

            list[i].presetName = val(content, "name");
            if (list[i].presetName.empty()) { list[i] = {}; continue; }

            try
            {
                list[i].team1Tickets = std::stoi(val(content, "team1_tickets"));
                list[i].team2Tickets = std::stoi(val(content, "team2_tickets"));
                list[i].team1AiCount = std::stoi(val(content, "team1_ai_count"));
                list[i].team2AiCount = std::stoi(val(content, "team2_ai_count"));
                list[i].playerHp     = std::stof(val(content, "player_hp"));
                list[i].respawnDelay = std::stof(val(content, "respawn_delay"));
            }
            catch (...) { list[i] = {}; continue; }
            ++loaded;
        }
        if (loaded > 0)
            std::cout << "[Presets] Caricati " << loaded << " preset da "
                      << PRESET_DIR << "\n";
    }
};

} // namespace mini