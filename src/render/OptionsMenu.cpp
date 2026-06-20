#include "mini/render/OptionsMenu.hpp"
#include "mini/platform/OpenGL.hpp"

#include <stb_easy_font.h>
#include <SDL2/SDL.h>
#include <cstdio>
#include <cmath>
#include <algorithm>

namespace mini
{

// ── Costruzione ─────────────────────────────────────────────────────────────

OptionsMenu::OptionsMenu(int screenW, int screenH)
    : m_w(screenW), m_h(screenH)
{
    buildRows();
}

void OptionsMenu::buildRows()
{
    m_rows.clear();

    m_rows.push_back({ "Vite alleati  (team 1 tickets)", true,  &m_settings.team1Tickets,   nullptr,  1,    1,   99 });
    m_rows.push_back({ "Vite nemici   (team 2 tickets)", true,  &m_settings.team2Tickets,   nullptr,  1,    1,   99 });
    m_rows.push_back({ "Ritardo respawn (s)",            false, nullptr, &m_settings.respawnDelay,   0.5f, 0,   30 });
    m_rows.push_back({ "Velocita AI",                    false, nullptr, &m_settings.aiSpeed,        0.5f, 1,   12 });
    m_rows.push_back({ "Cadenza fuoco AI (s)",           false, nullptr, &m_settings.aiFireInterval, 0.2f, 0.2f, 5 });
    m_rows.push_back({ "Raggio vista AI",                false, nullptr, &m_settings.aiRange,        1.0f, 4,   40 });
    m_rows.push_back({ "HP giocatore",                   false, nullptr, &m_settings.playerHp,       25,   25, 500 });
    m_rows.push_back({ "Velocita giocatore",             false, nullptr, &m_settings.playerSpeed,    0.5f,  1,  15 });
}

// ── Preset ───────────────────────────────────────────────────────────────────

void OptionsMenu::applyPreset(int index)
{
    auto presets = getBuiltinPresets();
    if (index < 0 || index >= (int)presets.size()) return;
    m_settings = presets[index].settings;   // <-- .settings, non .s
    buildRows();
}

// ── Input ────────────────────────────────────────────────────────────────────

OptionsMenu::Result OptionsMenu::handleKey(int sc)
{
    const int rowCount = (int)m_rows.size();

    if (sc == SDL_SCANCODE_UP || sc == SDL_SCANCODE_W)
    {
        m_selectedRow = (m_selectedRow - 1 + rowCount) % rowCount;
        return Result::None;
    }
    if (sc == SDL_SCANCODE_DOWN || sc == SDL_SCANCODE_S)
    {
        m_selectedRow = (m_selectedRow + 1) % rowCount;
        return Result::None;
    }

    Row& r = m_rows[m_selectedRow];
    auto clampApply = [&](float delta)
    {
        if (r.isInt)
        {
            int newVal = *r.iVal + (int)delta;
            *r.iVal = std::clamp(newVal, (int)r.minV, (int)r.maxV);
        }
        else
        {
            float newVal = *r.fVal + delta * r.step;
            *r.fVal = std::clamp(newVal, r.minV, r.maxV);
        }
    };

    if (sc == SDL_SCANCODE_RIGHT || sc == SDL_SCANCODE_D) clampApply(+1.0f);
    if (sc == SDL_SCANCODE_LEFT  || sc == SDL_SCANCODE_A) clampApply(-1.0f);

    if (sc == SDL_SCANCODE_F1) { applyPreset(0); return Result::None; }
    if (sc == SDL_SCANCODE_F2) { applyPreset(1); return Result::None; }
    if (sc == SDL_SCANCODE_F3) { applyPreset(2); return Result::None; }
    if (sc == SDL_SCANCODE_F4) { applyPreset(3); return Result::None; }

    if (sc == SDL_SCANCODE_RETURN || sc == SDL_SCANCODE_KP_ENTER)
        return Result::Confirmed;

    if (sc == SDL_SCANCODE_ESCAPE || sc == SDL_SCANCODE_BACKSPACE)
        return Result::Back;

    return Result::None;
}

// ── Rendering ─────────────────────────────────────────────────────────────────

void OptionsMenu::begin2D() const
{
    glUseProgram(0);
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glMatrixMode(GL_PROJECTION); glPushMatrix(); glLoadIdentity();
    glOrtho(0.0, m_w, m_h, 0.0, -1.0, 1.0);
    glMatrixMode(GL_MODELVIEW);  glPushMatrix(); glLoadIdentity();
}

void OptionsMenu::end2D() const
{
    glMatrixMode(GL_MODELVIEW);  glPopMatrix();
    glMatrixMode(GL_PROJECTION); glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
}

void OptionsMenu::drawRect(float x, float y, float w, float h,
                            float r, float g, float b, float a) const
{
    glColor4f(r, g, b, a);
    glBegin(GL_QUADS);
    glVertex2f(x,   y);   glVertex2f(x+w, y);
    glVertex2f(x+w, y+h); glVertex2f(x,   y+h);
    glEnd();
}

void OptionsMenu::drawText(float x, float y, float scale,
                            const char* text,
                            float r, float g, float b) const
{
    static char buf[131072];
    int quads = stb_easy_font_print(0, 0, const_cast<char*>(text),
                                    nullptr, buf, sizeof(buf));
    glPushMatrix();
    glTranslatef(x, y, 0.0f);
    glScalef(scale, scale, 1.0f);
    glColor3f(r, g, b);
    glEnableClientState(GL_VERTEX_ARRAY);
    glVertexPointer(3, GL_FLOAT, 16, buf);
    glDrawArrays(GL_QUADS, 0, quads * 4);
    glDisableClientState(GL_VERTEX_ARRAY);
    glPopMatrix();
}

void OptionsMenu::render() const
{
    begin2D();

    const float W  = (float)m_w;
    const float H  = (float)m_h;
    const float cx = W * 0.5f;

    // Sfondo semi-trasparente
    drawRect(0, 0, W, H, 0.0f, 0.0f, 0.0f, 0.78f);

    // Titolo
    drawText(cx - 110, 38, 3.5f, "OPZIONI PARTITA", 0.95f, 0.85f, 0.3f);

    // Preset rapidi
    auto presets = getBuiltinPresets();   // <-- funzione libera corretta
    float px = cx - 220;
    drawText(px, 88, 1.6f, "Preset rapidi:", 0.7f, 0.7f, 0.7f);
    for (int i = 0; i < (int)presets.size(); ++i)
    {
        char buf[64];
        // presets[i].name  +  F1/F2/F3/F4
        std::snprintf(buf, sizeof(buf), "F%d=%s", i + 1, presets[i].name);
        drawText(px + 115.0f + i * 115.0f, 88, 1.6f, buf, 0.5f, 0.85f, 1.0f);
    }

    // Righe opzioni
    const float startY = 130.0f;
    const float rowH   = 36.0f;
    const float labelX = cx - 290;
    const float valueX = cx + 80;
    const float barX   = valueX + 70;
    const float barW   = 160.0f;

    for (int i = 0; i < (int)m_rows.size(); ++i)
    {
        const Row& row = m_rows[i];
        const float y  = startY + i * rowH;
        const bool  sel = (i == m_selectedRow);

        if (sel)
            drawRect(labelX - 10, y - 4, W - (labelX - 10) * 2, rowH - 4,
                     0.18f, 0.35f, 0.55f, 0.55f);

        float lr = sel ? 1.0f : 0.80f;
        float lg = sel ? 0.95f : 0.80f;
        float lb = sel ? 0.55f : 0.80f;
        drawText(labelX, y + 4, 1.7f, row.label, lr, lg, lb);

        char valBuf[32];
        float curF = 0.0f;
        if (row.isInt)
        {
            std::snprintf(valBuf, sizeof(valBuf), "%d", *row.iVal);
            curF = (float)*row.iVal;
        }
        else
        {
            std::snprintf(valBuf, sizeof(valBuf), "%.1f", *row.fVal);
            curF = *row.fVal;
        }

        float vr = sel ? 1.0f  : 0.90f;
        float vg = sel ? 1.0f  : 0.90f;
        float vb = sel ? 0.4f  : 0.90f;
        drawText(valueX, y + 4, 1.8f, valBuf, vr, vg, vb);

        float pct = (row.maxV > row.minV)
                    ? (curF - row.minV) / (row.maxV - row.minV)
                    : 0.0f;
        pct = std::clamp(pct, 0.0f, 1.0f);
        drawRect(barX, y + 8, barW,        10, 0.15f, 0.15f, 0.15f);
        drawRect(barX, y + 8, barW * pct,  10,
                 sel ? 0.3f : 0.25f,
                 sel ? 0.75f : 0.55f,
                 sel ? 1.0f  : 0.75f);

        if (sel)
        {
            drawText(valueX - 22, y + 4, 1.8f, "<", 1.0f, 0.8f, 0.2f);
            drawText(valueX + 52, y + 4, 1.8f, ">", 1.0f, 0.8f, 0.2f);
        }
    }

    // Legenda tasti
    const float legendY = startY + m_rows.size() * rowH + 22;
    drawRect(0, legendY - 6, W, 56, 0, 0, 0, 0.5f);
    drawText(cx - 270, legendY,      1.6f,
             "SU/GIU = naviga   |   SX/DX = cambia valore",
             0.65f, 0.65f, 0.65f);
    drawText(cx - 230, legendY + 22, 1.6f,
             "INVIO = conferma e torna   |   ESC = annulla",
             0.65f, 0.65f, 0.65f);

    end2D();
}

} // namespace mini