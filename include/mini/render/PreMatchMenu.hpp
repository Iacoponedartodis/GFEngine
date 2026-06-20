#pragma once
#include "mini/game/MatchSettings.hpp"
#include <string>
#include <vector>

namespace mini
{

// ── Menu pre-partita: aperto da ENTER in FreeRoam ────────────────────────────
// Voce 0: Avvia partita
// Voce 1: Regole di gioco e preset  →  sotto-pagina con le impostazioni
class PreMatchMenu
{
public:
    PreMatchMenu(int screenW, int screenH);

    enum class Result { None, StartGame, Back };

    Result handleKey(int sdlScancode);
    void   handleTextInput(const char* text);
    void   render() const;

    [[nodiscard]] const MatchSettings& getSettings() const { return m_settings; }
    void setSettings(const MatchSettings& s);

private:
    int m_w, m_h;

    // ── Pagine ───────────────────────────────────────────────────────
    enum class Page { Root, Rules, SavePreset, ManagePresets, RenamePreset, LoadPreset };
    Page m_page        = Page::Root;
    int  m_selectedRow = 0;   // usato in Root (0=Avvia, 1=Regole)
    int  m_presetSlot  = 0;

    // ── Impostazioni correnti ────────────────────────────────────────
    MatchSettings m_settings;
    UserPresets   m_presets;

    // ── Righe modificabili (Rules) ───────────────────────────────────
    struct Row
    {
        const char* label;
        bool        isInt;
        int*        iVal;
        float*      fVal;
        float       step;
        float       minV;
        float       maxV;
    };
    std::vector<Row> m_rows;
    int m_rulesRow = 0;
    void buildRows();

    // ── Input testo per nomi preset ──────────────────────────────────
    std::string m_textInput;
    static constexpr int MAX_NAME = 24;

    // ── Handler ──────────────────────────────────────────────────────
    Result handleRoot(int sc);
    Result handleRules(int sc);
    Result handleSavePreset(int sc);
    Result handleManagePresets(int sc);
    Result handleRenamePreset(int sc);
    Result handleLoadPreset(int sc);

    // ── Render ───────────────────────────────────────────────────────
    void renderRoot()          const;
    void renderRules()         const;
    void renderSavePreset()    const;
    void renderManagePresets() const;
    void renderRenamePreset()  const;
    void renderLoadPreset()    const;

    // ── OpenGL 2D helpers ────────────────────────────────────────────
    void begin2D() const;
    void end2D()   const;
    void drawRect(float x, float y, float w, float h,
                  float r, float g, float b, float a = 1.0f) const;
    void drawText(float x, float y, float scale, const char* text,
                  float r, float g, float b) const;
};

} // namespace mini