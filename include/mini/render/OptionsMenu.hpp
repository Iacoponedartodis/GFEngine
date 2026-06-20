#pragma once
#include "mini/game/MatchSettings.hpp"

namespace mini
{

class OptionsMenu
{
public:
    OptionsMenu(int screenW, int screenH);

    enum class Result { None, Back, Confirmed };
    Result handleKey(int sdlScancode);

    void render() const;

    [[nodiscard]] const MatchSettings& getSettings() const { return m_settings; }
    void setSettings(const MatchSettings& s);

private:
    int m_w, m_h;

    MatchSettings m_settings;
    UserPresets   m_presets;

    // ── Navigazione ──────────────────────────────────────────────────
    enum class Page { Main, SavePreset, LoadPreset };
    Page m_page = Page::Main;
    int  m_selectedRow = 0;

    // ── Pagina Main: righe modificabili ─────────────────────────────
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
    void buildRows();

    // ── Pagina preset ────────────────────────────────────────────────
    int  m_presetSlot = 0;   // slot selezionato (0-7)

    // ── Helpers render ───────────────────────────────────────────────
    void begin2D() const;
    void end2D()   const;
    void drawRect(float x, float y, float w, float h,
                  float r, float g, float b, float a = 1.0f) const;
    void drawText(float x, float y, float scale, const char* text,
                  float r, float g, float b) const;

    void renderMain()       const;
    void renderSavePreset() const;
    void renderLoadPreset() const;

    Result handleMain(int sc);
    Result handleSavePreset(int sc);
    Result handleLoadPreset(int sc);
};

} // namespace mini