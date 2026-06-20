#pragma once
#include "mini/game/MatchSettings.hpp"
#include <vector>
#include <string>

namespace mini
{

class OptionsMenu
{
public:
    OptionsMenu(int screenW, int screenH);

    // Risultato della pressione di un tasto (chiama ogni frame per ogni SDL_Event)
    enum class Result { None, Back, Confirmed };
    Result handleKey(int sdlScancode);

    void render() const;   // chiama begin2D/end2D internamente (OpenGL fixed-function)

    [[nodiscard]] const MatchSettings& getSettings() const { return m_settings; }
    void applyPreset(int index);   // 0-3 = preset built-in

private:
    int m_w, m_h;
    int m_selectedRow = 0;

    MatchSettings m_settings;

    // ── Righe configurabili ──────────────────────────────────────────
    struct Row
    {
        const char* label;
        bool        isInt;     // true = valore int, false = float
        int*        iVal;
        float*      fVal;
        float       step;
        float       minV;
        float       maxV;
    };

    std::vector<Row> m_rows;
    void buildRows();

    // ── Helpers rendering ────────────────────────────────────────────
    void begin2D() const;
    void end2D()   const;
    void drawRect(float x, float y, float w, float h,
                  float r, float g, float b, float a = 1.0f) const;
    void drawText(float x, float y, float scale,
                  const char* text,
                  float r, float g, float b) const;
};

} // namespace mini