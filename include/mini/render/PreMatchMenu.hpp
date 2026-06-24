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

    // ── Liste dinamiche da DefinitionRegistry ────────────────────────
    struct WeaponEntry  { std::string id; std::string name; };
    struct AbilityEntry { std::string id; std::string name; std::string type; };

    void setWeaponList (const std::vector<WeaponEntry>&  weapons);
    void setAbilityList(const std::vector<AbilityEntry>& abilities);

    [[nodiscard]] const std::string& getSelectedWeaponId()    const;
    [[nodiscard]] int getSelectedWeapon() const { return m_weaponIdx; }

private:
    Ui2D m_ui;

    enum class Page
    {
        Root, Loadout, LoadoutAbilities, Rules,
        SavePreset, ManagePresets, RenamePreset, LoadPreset
    };
    Page m_page      = Page::Root;
    int  m_selectedRow = 0;
    int  m_presetSlot  = 0;

    MatchSettings m_settings;
    UserPresets   m_presets;

    // ── Armi ─────────────────────────────────────────────────────────
    int m_weaponIdx  = 0;
    int m_weapon2Idx = 0;   // arma secondaria (0 = nessuna)
    std::vector<WeaponEntry> m_weaponList;

    // ── Abilità & Gadget ─────────────────────────────────────────────
    std::vector<AbilityEntry> m_abilityList;
    int m_abilitySlot = 0;  // quale slot si sta modificando (0/1)
    int m_abilityIdx[2] = {0, 0}; // 0 = "(nessuna)"
    int m_gadgetIdx = 0;           // 0 = "(nessuno)"

    // ── Regole match ─────────────────────────────────────────────────
    struct Row
    {
        const char* label;
        bool isInt;
        int*   iVal;
        float* fVal;
        float step, minV, maxV;
    };
    std::vector<Row> m_rows;
    int m_rulesRow = 0;
    void buildRows();

    std::string m_textInput;
    static constexpr int MAX_NAME = 24;

    // ── Handler pagine ────────────────────────────────────────────────
    Result handleRoot           (int sc);
    Result handleLoadout        (int sc);
    Result handleLoadoutAbilities(int sc);
    Result handleRules          (int sc);
    Result handleSavePreset     (int sc);
    Result handleManagePresets  (int sc);
    Result handleRenamePreset   (int sc);
    Result handleLoadPreset     (int sc);

    // ── Render pagine ─────────────────────────────────────────────────
    void renderRoot            () const;
    void renderLoadout         () const;
    void renderLoadoutAbilities() const;
    void renderRules           () const;
    void renderSavePreset      () const;
    void renderManagePresets   () const;
    void renderRenamePreset    () const;
    void renderLoadPreset      () const;

    // ── Helpers ───────────────────────────────────────────────────────
    void syncLoadoutToSettings();   // copia gli idx in m_settings.primaryWeaponId ecc.
};

} // namespace mini
