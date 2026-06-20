#pragma once
#include "mini/game/MatchSettings.hpp"
#include <string>
#include <vector>

namespace mini
{

class OptionsMenu
{
public:
    OptionsMenu(int screenW, int screenH);

    enum class Result { None, Back, Confirmed };

    // Chiamato per ogni SDL_KEYDOWN (scancode)
    Result handleKey(int sdlScancode);

    // Chiamato per ogni SDL_TEXTINPUT (testo UTF-8 — per inserimento nome)
    void handleTextInput(const char* text);

    void render() const;

    [[nodiscard]] const MatchSettings& getSettings() const { return m_settings; }
    void setSettings(const MatchSettings& s);

private:
    int m_w, m_h;

    MatchSettings m_settings;
    UserPresets   m_presets;

    // ── Pagine ───────────────────────────────────────────────────────
    enum class Page { Main, SavePreset, ManagePresets, RenamePreset, LoadPreset };
    Page m_page        = Page::Main;
    int  m_selectedRow = 0;
    int  m_presetSlot  = 0;

    // ── Input testo per nomi preset ──────────────────────────────────
    std::string m_textInput;
    static constexpr int MAX_NAME = 24;

    // ── Righe modificabili (Main) ────────────────────────────────────
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

    // ── Handler per pagina ───────────────────────────────────────────
    Result handleMain(int sc);
    Result handleSavePreset(int sc);
    Result handleManagePresets(int sc);
    Result handleRenamePreset(int sc);
    Result handleLoadPreset(int sc);

    // ── Render per pagina ────────────────────────────────────────────
    void renderMain()          const;
    void renderSavePreset()    const;
    void renderManagePresets() const;
    void renderRenamePreset()  const;
    void renderLoadPreset()    const;

    // ── Helpers OpenGL 2D ────────────────────────────────────────────
    void begin2D() const;
    void end2D()   const;
    void drawRect(float x, float y, float w, float h,
                  float r, float g, float b, float a = 1.0f) const;
    void drawText(float x, float y, float scale, const char* text,
                  float r, float g, float b) const;
};

} // namespace mini