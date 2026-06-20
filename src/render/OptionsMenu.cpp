#include "mini/render/OptionsMenu.hpp"
#include "mini/platform/OpenGL.hpp"

#include <stb_easy_font.h>
#include <SDL2/SDL.h>
#include <cstdio>
#include <cmath>
#include <algorithm>

namespace mini
{

// ─────────────────────────────────────────────────────────────────────────────

OptionsMenu::OptionsMenu(int screenW, int screenH)
    : m_w(screenW), m_h(screenH)
{
    buildRows();
}

void OptionsMenu::setSettings(const MatchSettings& s)
{
    m_settings = s;
    buildRows();
}

void OptionsMenu::buildRows()
{
    m_rows.clear();
    // { label, isInt, ptr_int, ptr_float, step, min, max }
    m_rows.push_back({ "Vite alleati  (team 1 tickets)", true,  &m_settings.team1Tickets, nullptr,     1,  1, 99 });
    m_rows.push_back({ "Vite nemici   (team 2 tickets)", true,  &m_settings.team2Tickets, nullptr,     1,  1, 99 });
    m_rows.push_back({ "AI alleate  (num unita team 1)", true,  &m_settings.team1AiCount, nullptr,     1,  0, 10 });
    m_rows.push_back({ "AI nemiche  (num unita team 2)", true,  &m_settings.team2AiCount, nullptr,     1,  0, 20 });
    m_rows.push_back({ "HP giocatore",                   false, nullptr, &m_settings.playerHp,       25, 25,500 });
    m_rows.push_back({ "Ritardo respawn (s)",            false, nullptr, &m_settings.respawnDelay,  0.5f, 0, 30 });
}

// ── Input ─────────────────────────────────────────────────────────────────────

OptionsMenu::Result OptionsMenu::handleKey(int sc)
{
    switch (m_page)
    {
    case Page::Main:       return handleMain(sc);
    case Page::SavePreset: return handleSavePreset(sc);
    case Page::LoadPreset: return handleLoadPreset(sc);
    }
    return Result::None;
}

OptionsMenu::Result OptionsMenu::handleMain(int sc)
{
    const int rowCount = (int)m_rows.size();

    if (sc == SDL_SCANCODE_UP || sc == SDL_SCANCODE_W)
    { m_selectedRow = (m_selectedRow - 1 + rowCount) % rowCount; return Result::None; }

    if (sc == SDL_SCANCODE_DOWN || sc == SDL_SCANCODE_S)
    { m_selectedRow = (m_selectedRow + 1) % rowCount; return Result::None; }

    Row& r = m_rows[m_selectedRow];
    auto clampApply = [&](float delta)
    {
        if (r.isInt)
        {
            int v = *r.iVal + (int)delta;
            *r.iVal = std::clamp(v, (int)r.minV, (int)r.maxV);
        }
        else
        {
            float v = *r.fVal + delta * r.step;
            *r.fVal = std::clamp(v, r.minV, r.maxV);
        }
    };

    if (sc == SDL_SCANCODE_RIGHT || sc == SDL_SCANCODE_D) clampApply(+1.0f);
    if (sc == SDL_SCANCODE_LEFT  || sc == SDL_SCANCODE_A) clampApply(-1.0f);

    // F5 = salva preset, F6 = carica preset
    if (sc == SDL_SCANCODE_F5)
    { m_page = Page::SavePreset; m_presetSlot = 0; return Result::None; }
    if (sc == SDL_SCANCODE_F6)
    { m_page = Page::LoadPreset; m_presetSlot = 0; return Result::None; }

    if (sc == SDL_SCANCODE_RETURN || sc == SDL_SCANCODE_KP_ENTER)
        return Result::Confirmed;

    if (sc == SDL_SCANCODE_ESCAPE || sc == SDL_SCANCODE_BACKSPACE)
        return Result::Back;

    return Result::None;
}

OptionsMenu::Result OptionsMenu::handleSavePreset(int sc)
{
    if (sc == SDL_SCANCODE_UP   || sc == SDL_SCANCODE_W)
    { m_presetSlot = (m_presetSlot - 1 + UserPresets::MAX) % UserPresets::MAX; return Result::None; }
    if (sc == SDL_SCANCODE_DOWN || sc == SDL_SCANCODE_S)
    { m_presetSlot = (m_presetSlot + 1) % UserPresets::MAX; return Result::None; }

    if (sc == SDL_SCANCODE_RETURN || sc == SDL_SCANCODE_KP_ENTER)
    {
        MatchSettings toSave = m_settings;
        char buf[32];
        std::snprintf(buf, sizeof(buf), "Preset %d", m_presetSlot + 1);
        toSave.presetName = buf;
        m_presets.save(toSave, m_presetSlot);
        m_page = Page::Main;
        return Result::None;
    }

    if (sc == SDL_SCANCODE_ESCAPE || sc == SDL_SCANCODE_BACKSPACE)
    { m_page = Page::Main; return Result::None; }

    return Result::None;
}

OptionsMenu::Result OptionsMenu::handleLoadPreset(int sc)
{
    if (sc == SDL_SCANCODE_UP   || sc == SDL_SCANCODE_W)
    { m_presetSlot = (m_presetSlot - 1 + UserPresets::MAX) % UserPresets::MAX; return Result::None; }
    if (sc == SDL_SCANCODE_DOWN || sc == SDL_SCANCODE_S)
    { m_presetSlot = (m_presetSlot + 1) % UserPresets::MAX; return Result::None; }

    if (sc == SDL_SCANCODE_RETURN || sc == SDL_SCANCODE_KP_ENTER)
    {
        const MatchSettings* p = m_presets.get(m_presetSlot);
        if (p) { m_settings = *p; buildRows(); }
        m_page = Page::Main;
        return Result::None;
    }

    if (sc == SDL_SCANCODE_ESCAPE || sc == SDL_SCANCODE_BACKSPACE)
    { m_page = Page::Main; return Result::None; }

    return Result::None;
}

// ── Rendering base ────────────────────────────────────────────────────────────

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

void OptionsMenu::drawText(float x, float y, float scale, const char* text,
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

// ── Render principale ─────────────────────────────────────────────────────────

void OptionsMenu::render() const
{
    begin2D();
    switch (m_page)
    {
    case Page::Main:       renderMain();       break;
    case Page::SavePreset: renderSavePreset(); break;
    case Page::LoadPreset: renderLoadPreset(); break;
    }
    end2D();
}

void OptionsMenu::renderMain() const
{
    const float W  = (float)m_w;
    const float cx = W * 0.5f;

    drawRect(0, 0, W, (float)m_h, 0.0f, 0.0f, 0.0f, 0.80f);
    drawText(cx - 120, 35, 3.2f, "IMPOSTAZIONI PARTITA", 0.95f, 0.85f, 0.3f);

    const float startY = 120.0f;
    const float rowH   = 40.0f;
    const float labelX = cx - 300;
    const float valueX = cx + 80;
    const float barX   = valueX + 70;
    const float barW   = 150.0f;

    for (int i = 0; i < (int)m_rows.size(); ++i)
    {
        const Row& row = m_rows[i];
        const float y  = startY + i * rowH;
        const bool  sel = (i == m_selectedRow);

        if (sel)
            drawRect(labelX - 12, y - 5, W - (labelX - 12) * 2, rowH - 4,
                     0.15f, 0.32f, 0.55f, 0.55f);

        float lr = sel ? 1.0f  : 0.80f;
        float lg = sel ? 0.95f : 0.80f;
        float lb = sel ? 0.50f : 0.80f;
        drawText(labelX, y + 5, 1.8f, row.label, lr, lg, lb);

        char valBuf[32];
        float curF = 0.0f;
        if (row.isInt) { std::snprintf(valBuf, sizeof(valBuf), "%d", *row.iVal); curF = (float)*row.iVal; }
        else           { std::snprintf(valBuf, sizeof(valBuf), "%.1f", *row.fVal); curF = *row.fVal; }

        drawText(valueX, y + 5, 1.9f, valBuf,
                 sel ? 1.0f : 0.90f, sel ? 1.0f : 0.90f, sel ? 0.4f : 0.90f);

        float pct = (row.maxV > row.minV) ? (curF - row.minV) / (row.maxV - row.minV) : 0.0f;
        pct = std::clamp(pct, 0.0f, 1.0f);
        drawRect(barX, y + 10, barW,        10, 0.12f, 0.12f, 0.12f);
        drawRect(barX, y + 10, barW * pct,  10,
                 sel ? 0.3f : 0.2f, sel ? 0.75f : 0.5f, sel ? 1.0f : 0.7f);

        if (sel)
        {
            drawText(valueX - 24, y + 5, 1.9f, "<", 1.0f, 0.8f, 0.2f);
            drawText(valueX + 55, y + 5, 1.9f, ">", 1.0f, 0.8f, 0.2f);
        }
    }

    // Legenda in basso
    const float ly = startY + m_rows.size() * rowH + 28;
    drawRect(0, ly - 8, W, 60, 0, 0, 0, 0.55f);
    drawText(cx - 290, ly,      1.6f, "SU/GIU = naviga   SX/DX = modifica valore", 0.6f, 0.6f, 0.6f);
    drawText(cx - 290, ly + 20, 1.6f, "INVIO = avvia partita   ESC = annulla", 0.6f, 0.6f, 0.6f);
    drawText(cx - 290, ly + 40, 1.6f, "F5 = salva preset   F6 = carica preset", 0.5f, 0.85f, 1.0f);
}

void OptionsMenu::renderSavePreset() const
{
    const float W  = (float)m_w;
    const float cx = W * 0.5f;

    drawRect(0, 0, W, (float)m_h, 0.0f, 0.0f, 0.0f, 0.88f);
    drawText(cx - 90, 35, 3.0f, "SALVA PRESET", 0.3f, 1.0f, 0.5f);
    drawText(cx - 160, 85, 1.7f, "Scegli slot con SU/GIU — INVIO per salvare", 0.7f, 0.7f, 0.7f);

    const float startY = 140.0f;
    const float rowH   = 38.0f;

    for (int i = 0; i < UserPresets::MAX; ++i)
    {
        const float y   = startY + i * rowH;
        const bool  sel = (i == m_presetSlot);
        const MatchSettings* p = m_presets.get(i);

        if (sel)
            drawRect(cx - 240, y - 4, 480, rowH - 4, 0.1f, 0.4f, 0.2f, 0.55f);

        char buf[64];
        if (p && !p->presetName.empty())
            std::snprintf(buf, sizeof(buf), "Slot %d: %s", i + 1, p->presetName.c_str());
        else
            std::snprintf(buf, sizeof(buf), "Slot %d: [vuoto]", i + 1);

        drawText(cx - 220, y + 5, 1.8f, buf,
                 sel ? 1.0f : 0.7f, sel ? 1.0f : 0.7f, sel ? 0.5f : 0.7f);
    }

    const float ly = startY + UserPresets::MAX * rowH + 20;
    drawText(cx - 120, ly, 1.6f, "ESC = torna indietro", 0.55f, 0.55f, 0.55f);
}

void OptionsMenu::renderLoadPreset() const
{
    const float W  = (float)m_w;
    const float cx = W * 0.5f;

    drawRect(0, 0, W, (float)m_h, 0.0f, 0.0f, 0.0f, 0.88f);
    drawText(cx - 90, 35, 3.0f, "CARICA PRESET", 0.3f, 0.7f, 1.0f);
    drawText(cx - 160, 85, 1.7f, "Scegli slot con SU/GIU — INVIO per caricare", 0.7f, 0.7f, 0.7f);

    const float startY = 140.0f;
    const float rowH   = 38.0f;

    for (int i = 0; i < UserPresets::MAX; ++i)
    {
        const float y   = startY + i * rowH;
        const bool  sel = (i == m_presetSlot);
        const MatchSettings* p = m_presets.get(i);

        if (sel)
            drawRect(cx - 240, y - 4, 480, rowH - 4, 0.1f, 0.25f, 0.5f, 0.55f);

        char buf[128];
        if (p && !p->presetName.empty())
        {
            std::snprintf(buf, sizeof(buf),
                "Slot %d: %s  [T1:%d T2:%d  AI-a:%d AI-n:%d  HP:%.0f]",
                i + 1, p->presetName.c_str(),
                p->team1Tickets, p->team2Tickets,
                p->team1AiCount, p->team2AiCount,
                p->playerHp);
        }
        else
            std::snprintf(buf, sizeof(buf), "Slot %d: [vuoto]", i + 1);

        drawText(cx - 230, y + 5, 1.6f, buf,
                 sel ? 1.0f : (p ? 0.75f : 0.4f),
                 sel ? 1.0f : (p ? 0.75f : 0.4f),
                 sel ? 0.5f : (p ? 0.75f : 0.4f));
    }

    const float ly = startY + UserPresets::MAX * rowH + 20;
    drawText(cx - 120, ly, 1.6f, "ESC = torna indietro", 0.55f, 0.55f, 0.55f);
}

} // namespace mini