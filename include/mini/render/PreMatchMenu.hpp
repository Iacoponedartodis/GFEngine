#pragma once

#include "mini/game/MatchSettings.hpp"
#include "mini/game/Weapon.hpp"
#include "mini/render/Ui2D.hpp"

#include <string>
#include <vector>

namespace mini
{

class PreMatchMenu
{
public:
    PreMatchMenu(int screenW, int screenH);

    enum class Result { None, StartGame, Back };

    Result handleKey(int sdlScancode);
    void handleTextInput(const char* text);
    void render() const;

    [[nodiscard]] const MatchSettings& getSettings() const { return m_settings; }
    void setSettings(const MatchSettings& s);

    // Lista armi dinamica da DefinitionRegistry
    struct WeaponEntry { std::string id; std::string name; };
    void setWeaponList(const std::vector<WeaponEntry>& weapons);
    [[nodiscard]] const std::string& getSelectedWeaponId() const;
    [[nodiscard]] int getSelectedWeapon() const { return m_weaponIdx; }

private:
    Ui2D m_ui;

    enum class Page { Root, Loadout, Rules, SavePreset, ManagePresets, RenamePreset, LoadPreset };
    Page m_page = Page::Root;
    int m_selectedRow = 0;
    int m_presetSlot = 0;

    MatchSettings m_settings;
    UserPresets m_presets;

    int m_weaponIdx = 0;
    std::vector<WeaponEntry> m_weaponList;

    struct Row
    {
        const char* label;
        bool isInt;
        int* iVal;
        float* fVal;
        float step;
        float minV;
        float maxV;
    };

    std::vector<Row> m_rows;
    int m_rulesRow = 0;
    void buildRows();

    std::string m_textInput;
    static constexpr int MAX_NAME = 24;

    Result handleRoot(int sc);
    Result handleLoadout(int sc);
    Result handleRules(int sc);
    Result handleSavePreset(int sc);
    Result handleManagePresets(int sc);
    Result handleRenamePreset(int sc);
    Result handleLoadPreset(int sc);

    void renderRoot() const;
    void renderLoadout() const;
    void renderRules() const;
    void renderSavePreset() const;
    void renderManagePresets() const;
    void renderRenamePreset() const;
    void renderLoadPreset() const;
};

} // namespace mini