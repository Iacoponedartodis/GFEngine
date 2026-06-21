#pragma once
#include "mini/render/Ui2D.hpp"
#include <string>
#include <vector>

namespace mini
{

struct VersionEntry
{
    std::string label;       // "Stable", "Dev", etc.
    std::string description; // "Build 0.1.3 — 21 Jun 2026"
    std::string configPath;  // path al profilo config (futuro)
};

class LauncherScreen
{
public:
    LauncherScreen(int screenW, int screenH);

    enum class Result { None, Launch, Quit };

    Result handleKey(int sdlScancode);
    void   render() const;

    [[nodiscard]] int getSelectedVersion() const { return m_selected; }

    // Carica versioni (per ora hardcoded, in futuro da versions.json)
    void loadVersions();

private:
    Ui2D m_ui;
    std::vector<VersionEntry> m_versions;
    int m_selected = 0;
};

} // namespace mini