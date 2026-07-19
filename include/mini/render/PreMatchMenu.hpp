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
    // Mouse: hover (clicked=false) evidenzia la riga sotto il cursore; click
    // (clicked=true) attiva i bottoni e, sulle righe a valore, regola (metà
    // sinistra = −, destra = +) come le frecce. Stessa `Result` di handleKey.
    Result handleMouse(float mx, float my, bool clicked);
    void handleTextInput(const char* text);
    void render() const;

    [[nodiscard]] const MatchSettings& getSettings() const { return m_settings; }
    void setSettings(const MatchSettings& s);

    // ── Liste dinamiche da DefinitionRegistry ────────────────────────
    struct WeaponEntry  { std::string id; std::string name; };
    struct AbilityEntry { std::string id; std::string name; std::string type; };
    struct MapEntry     { std::string id; std::string name; };
    // La missione porta con sé mappa e modalità: sono SUE (MissionDef, doc 25) e
    // servono qui per aggiornare a vista le righe Mappa/Modalità quando la si
    // sceglie — il menu non deve mai mostrare una mappa diversa da quella che si
    // giocherà davvero.
    struct MissionEntry { std::string id; std::string name; std::string mapId; std::string modeId; };
    // Nessun ClassEntry/setClassList/getSelectedClassId: il giocatore NON sceglie
    // una classe (GDD 11.3, ADR-022). Senza il metodo la regola è STRUTTURALE —
    // la sola esistenza di un setter inviterebbe a rimettere la riga.

    void setWeaponList (const std::vector<WeaponEntry>&  weapons);
    void setAbilityList(const std::vector<AbilityEntry>& abilities);
    void setMapList    (const std::vector<MapEntry>&     maps);   // R3
    void setMissionList(const std::vector<MissionEntry>& missions);   // ADR-019

    [[nodiscard]] const std::string& getSelectedWeaponId()    const;
    [[nodiscard]] const std::string& getSelectedMapId()       const;
    [[nodiscard]] const std::string& getSelectedMissionId()   const;
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

    // ── Mappe (R3): nomi persistenti per le righe enum ───────────────
    std::vector<MapEntry>     m_mapList;
    std::vector<std::string>  m_mapNames;
    std::vector<const char*>  m_mapNamePtrs;

    // ── Missioni (ADR-019): stesso pattern delle mappe.
    //    L'indice vive QUI e non in MatchSettings: è stato di UI, e KI #20 ha già
    //    insegnato che l'indice è fragile — quello che si persiste è l'ID.
    //    Indice 0 = "(nessuna)" → partita libera: il default resta il
    //    comportamento storico.
    std::vector<MissionEntry> m_missionList;
    std::vector<std::string>  m_missionNames;
    std::vector<const char*>  m_missionNamePtrs;
    int m_missionIdx = 0;

    // Allinea le righe Mappa/Modalità alla missione scelta (se ne impone una).
    void syncRowsToMission();

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
        // Riga "enum": se names != nullptr mostra names[*iVal] al posto del
        // numero (usato per la Modalita di gioco).
        const char* const* names = nullptr;
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
    void applyPreset(const MatchSettings& p);   // preset → settings + risolve id in indici UI
};

} // namespace mini
