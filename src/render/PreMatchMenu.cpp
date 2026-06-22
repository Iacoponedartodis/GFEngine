#include "mini/render/PreMatchMenu.hpp"

#include "mini/game/Weapon.hpp"
#include <SDL2/SDL.h>
#include <cstdio>
#include <algorithm>
#include <cstring>

namespace mini
{

PreMatchMenu::PreMatchMenu(int screenW, int screenH)
    : m_ui(screenW, screenH)
{
    buildRows();
    while ((int)m_presets.list.size() < UserPresets::MAX)
        m_presets.list.push_back({});
}

void PreMatchMenu::setSettings(const MatchSettings& s)
{
    m_settings = s;
    buildRows();
}

void PreMatchMenu::buildRows()
{
    m_rows.clear();
    m_rows.push_back({"Vite alleati  (team 1 tickets)", true,  &m_settings.team1Tickets, nullptr,   1,   1,  99});
    m_rows.push_back({"Vite nemici    (team 2 tickets)", true,  &m_settings.team2Tickets, nullptr,   1,   1,  99});
    m_rows.push_back({"AI alleate  (num unita team 1)",  true,  &m_settings.team1AiCount, nullptr,   1,   0,  10});
    m_rows.push_back({"AI nemiche   (num unita team 2)", true,  &m_settings.team2AiCount, nullptr,   1,   0,  20});
    m_rows.push_back({"HP giocatore",                    false, nullptr, &m_settings.playerHp,       25,  25, 500});
    m_rows.push_back({"Ritardo respawn (s)",             false, nullptr, &m_settings.respawnDelay, 0.5f, 0.5f, 30});
}

void PreMatchMenu::handleTextInput(const char* text)
{
    if (m_page != Page::SavePreset && m_page != Page::RenamePreset) return;
    if ((int)m_textInput.size() >= MAX_NAME) return;
    m_textInput += text;
}

PreMatchMenu::Result PreMatchMenu::handleKey(int sc)
{
    switch (m_page)
    {
    case Page::Root:          return handleRoot(sc);
    case Page::Loadout:       return handleLoadout(sc);
    case Page::Rules:         return handleRules(sc);
    case Page::SavePreset:    return handleSavePreset(sc);
    case Page::ManagePresets: return handleManagePresets(sc);
    case Page::RenamePreset:  return handleRenamePreset(sc);
    case Page::LoadPreset:    return handleLoadPreset(sc);
    }
    return Result::None;
}

PreMatchMenu::Result PreMatchMenu::handleRoot(int sc)
{
    constexpr int ITEMS = 3;

    if (sc == SDL_SCANCODE_UP || sc == SDL_SCANCODE_W)
    {
        m_selectedRow = (m_selectedRow - 1 + ITEMS) % ITEMS;
        return Result::None;
    }
    if (sc == SDL_SCANCODE_DOWN || sc == SDL_SCANCODE_S)
    {
        m_selectedRow = (m_selectedRow + 1) % ITEMS;
        return Result::None;
    }

    if (sc == SDL_SCANCODE_RETURN || sc == SDL_SCANCODE_KP_ENTER)
    {
        if (m_selectedRow == 0) return Result::StartGame;
        if (m_selectedRow == 1) { m_page = Page::Loadout; return Result::None; }
        if (m_selectedRow == 2) { m_page = Page::Rules; m_rulesRow = 0; return Result::None; }
    }

    if (sc == SDL_SCANCODE_ESCAPE || sc == SDL_SCANCODE_BACKSPACE)
        return Result::Back;

    return Result::None;
}

PreMatchMenu::Result PreMatchMenu::handleLoadout(int sc)
{
    constexpr int N = 4;

    if (sc == SDL_SCANCODE_UP || sc == SDL_SCANCODE_W)
    {
        m_weaponIdx = (m_weaponIdx - 1 + N) % N;
        return Result::None;
    }
    if (sc == SDL_SCANCODE_DOWN || sc == SDL_SCANCODE_S)
    {
        m_weaponIdx = (m_weaponIdx + 1) % N;
        return Result::None;
    }

    if (sc == SDL_SCANCODE_RETURN || sc == SDL_SCANCODE_KP_ENTER ||
        sc == SDL_SCANCODE_ESCAPE || sc == SDL_SCANCODE_BACKSPACE)
    {
        m_page = Page::Root;
        return Result::None;
    }

    return Result::None;
}

PreMatchMenu::Result PreMatchMenu::handleRules(int sc)
{
    const int rowCount = (int)m_rows.size();

    if (sc == SDL_SCANCODE_UP || sc == SDL_SCANCODE_W)
    {
        m_rulesRow = (m_rulesRow - 1 + rowCount) % rowCount;
        return Result::None;
    }
    if (sc == SDL_SCANCODE_DOWN || sc == SDL_SCANCODE_S)
    {
        m_rulesRow = (m_rulesRow + 1) % rowCount;
        return Result::None;
    }

    Row& r = m_rows[m_rulesRow];
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
    if (sc == SDL_SCANCODE_LEFT || sc == SDL_SCANCODE_A) clampApply(-1.0f);

    if (sc == SDL_SCANCODE_F5)
    {
        m_page = Page::SavePreset;
        m_presetSlot = 0;
        m_textInput.clear();
        SDL_StartTextInput();
        return Result::None;
    }
    if (sc == SDL_SCANCODE_F6)
    {
        m_page = Page::LoadPreset;
        m_presetSlot = 0;
        return Result::None;
    }
    if (sc == SDL_SCANCODE_F7)
    {
        m_page = Page::ManagePresets;
        m_presetSlot = 0;
        return Result::None;
    }

    if (sc == SDL_SCANCODE_ESCAPE || sc == SDL_SCANCODE_BACKSPACE)
    {
        m_page = Page::Root;
        return Result::None;
    }

    return Result::None;
}

PreMatchMenu::Result PreMatchMenu::handleSavePreset(int sc)
{
    if (sc == SDL_SCANCODE_UP || sc == SDL_SCANCODE_W)
    {
        m_presetSlot = (m_presetSlot - 1 + UserPresets::MAX) % UserPresets::MAX;
        return Result::None;
    }
    if (sc == SDL_SCANCODE_DOWN || sc == SDL_SCANCODE_S)
    {
        m_presetSlot = (m_presetSlot + 1) % UserPresets::MAX;
        return Result::None;
    }

    if (sc == SDL_SCANCODE_BACKSPACE && !m_textInput.empty())
    {
        m_textInput.pop_back();
        return Result::None;
    }

    if (sc == SDL_SCANCODE_RETURN || sc == SDL_SCANCODE_KP_ENTER)
    {
        MatchSettings toSave = m_settings;
        if (m_textInput.empty())
        {
            char buf[32];
            std::snprintf(buf, sizeof(buf), "Preset %d", m_presetSlot + 1);
            toSave.presetName = buf;
        }
        else toSave.presetName = m_textInput;

        m_presets.save(toSave, m_presetSlot);
        SDL_StopTextInput();
        m_page = Page::Rules;
        return Result::None;
    }

    if (sc == SDL_SCANCODE_ESCAPE)
    {
        SDL_StopTextInput();
        m_page = Page::Rules;
        return Result::None;
    }

    return Result::None;
}

PreMatchMenu::Result PreMatchMenu::handleManagePresets(int sc)
{
    if (sc == SDL_SCANCODE_UP || sc == SDL_SCANCODE_W)
    {
        m_presetSlot = (m_presetSlot - 1 + UserPresets::MAX) % UserPresets::MAX;
        return Result::None;
    }
    if (sc == SDL_SCANCODE_DOWN || sc == SDL_SCANCODE_S)
    {
        m_presetSlot = (m_presetSlot + 1) % UserPresets::MAX;
        return Result::None;
    }

    if (sc == SDL_SCANCODE_RETURN || sc == SDL_SCANCODE_KP_ENTER)
    {
        const MatchSettings* p = m_presets.get(m_presetSlot);
        if (p) { m_settings = *p; buildRows(); }
        m_page = Page::Rules;
        return Result::None;
    }

    if (sc == SDL_SCANCODE_R)
    {
        const MatchSettings* p = m_presets.get(m_presetSlot);
        if (p)
        {
            m_textInput = p->presetName;
            m_page = Page::RenamePreset;
            SDL_StartTextInput();
        }
        return Result::None;
    }

    if (sc == SDL_SCANCODE_DELETE || sc == SDL_SCANCODE_X)
    {
        m_presets.remove(m_presetSlot);
        return Result::None;
    }

    if (sc == SDL_SCANCODE_ESCAPE || sc == SDL_SCANCODE_BACKSPACE)
    {
        m_page = Page::Rules;
        return Result::None;
    }

    return Result::None;
}

PreMatchMenu::Result PreMatchMenu::handleRenamePreset(int sc)
{
    if (sc == SDL_SCANCODE_BACKSPACE && !m_textInput.empty())
    {
        m_textInput.pop_back();
        return Result::None;
    }

    if (sc == SDL_SCANCODE_RETURN || sc == SDL_SCANCODE_KP_ENTER)
    {
        MatchSettings* p = m_presets.getMutable(m_presetSlot);
        if (p && !m_textInput.empty()) p->presetName = m_textInput;
        SDL_StopTextInput();
        m_page = Page::ManagePresets;
        return Result::None;
    }

    if (sc == SDL_SCANCODE_ESCAPE)
    {
        SDL_StopTextInput();
        m_page = Page::ManagePresets;
        return Result::None;
    }

    return Result::None;
}

PreMatchMenu::Result PreMatchMenu::handleLoadPreset(int sc)
{
    if (sc == SDL_SCANCODE_UP || sc == SDL_SCANCODE_W)
    {
        m_presetSlot = (m_presetSlot - 1 + UserPresets::MAX) % UserPresets::MAX;
        return Result::None;
    }
    if (sc == SDL_SCANCODE_DOWN || sc == SDL_SCANCODE_S)
    {
        m_presetSlot = (m_presetSlot + 1) % UserPresets::MAX;
        return Result::None;
    }

    if (sc == SDL_SCANCODE_RETURN || sc == SDL_SCANCODE_KP_ENTER)
    {
        const MatchSettings* p = m_presets.get(m_presetSlot);
        if (p) { m_settings = *p; buildRows(); }
        m_page = Page::Rules;
        return Result::None;
    }

    if (sc == SDL_SCANCODE_ESCAPE || sc == SDL_SCANCODE_BACKSPACE)
    {
        m_page = Page::Rules;
        return Result::None;
    }

    return Result::None;
}

void PreMatchMenu::render() const
{
    m_ui.begin();
    switch (m_page)
    {
    case Page::Root:          renderRoot(); break;
    case Page::Loadout:       renderLoadout(); break;
    case Page::Rules:         renderRules(); break;
    case Page::SavePreset:    renderSavePreset(); break;
    case Page::ManagePresets: renderManagePresets(); break;
    case Page::RenamePreset:  renderRenamePreset(); break;
    case Page::LoadPreset:    renderLoadPreset(); break;
    }
    m_ui.end();
}

void PreMatchMenu::renderRoot() const
{
    const float W = (float)m_ui.width();
    const float H = (float)m_ui.height();
    const float cx = W * 0.5f;
    const float cy = H * 0.5f;

    m_ui.rect(0, 0, W, H, 0.0f, 0.0f, 0.0f, 0.80f);

    m_ui.textCentered(cx, cy - 120.0f, 3.5f, "MENU PARTITA", 0.95f, 0.85f, 0.3f);

    struct Item { const char* label; float r, g, b; };
    const Item items[] = {
        { "Avvia partita",     0.3f, 1.0f, 0.45f },
        { "Personalizzazione", 0.85f, 0.75f, 0.4f },
        { "Regole di gioco",   0.5f, 0.85f, 1.0f },
    };
    constexpr int N = 3;

    const float startY = cy - 30.0f;
    const float rowH = 58.0f;

    for (int i = 0; i < N; ++i)
    {
        const float y = startY + i * rowH;
        const bool sel = (i == m_selectedRow);

        const float bx = cx - 220.0f;
        const float bw = 440.0f;
        const float bh = 44.0f;

        if (sel)
            m_ui.rect(bx, y - 4, bw, bh, 0.12f, 0.28f, 0.50f, 0.65f);
        else
            m_ui.rect(bx, y - 4, bw, bh, 0.08f, 0.08f, 0.10f, 0.45f);

        m_ui.rect(bx,          y - 4,      bw, 1, 0.25f, 0.35f, 0.55f);
        m_ui.rect(bx,          y + bh - 5, bw, 1, 0.25f, 0.35f, 0.55f);
        m_ui.rect(bx,          y - 4,      1, bh, 0.25f, 0.35f, 0.55f);
        m_ui.rect(bx + bw - 1, y - 4,      1, bh, 0.25f, 0.35f, 0.55f);

        float scale = sel ? 2.4f : 2.0f;
        float ir = sel ? items[i].r : items[i].r * 0.65f;
        float ig = sel ? items[i].g : items[i].g * 0.65f;
        float ib = sel ? items[i].b : items[i].b * 0.65f;

        m_ui.textCentered(cx, y + 10, scale, items[i].label, ir, ig, ib);
    }

    m_ui.text(cx - 187.0f, startY + N * rowH + 24.0f, 1.7f,
              "SU/GIU = naviga   INVIO = seleziona   ESC = indietro",
              0.50f, 0.50f, 0.50f);
}

void PreMatchMenu::renderLoadout() const
{
    const float W = (float)m_ui.width();
    const float H = (float)m_ui.height();
    const float cx = W * 0.5f;

    m_ui.rect(0, 0, W, H, 0.0f, 0.0f, 0.0f, 0.88f);
    m_ui.textCentered(cx, 30, 3.0f, "LOADOUT", 0.95f, 0.85f, 0.3f);

    const char* weapons[4] = {
        "Blaster Rifle",
        "Blaster Pistol",
        "Heavy Blaster",
        "Sniper Rifle"
    };

    const float startY = 120.0f;
    const float rowH = 50.0f;

    for (int i = 0; i < 4; ++i)
    {
        const float y = startY + i * rowH;
        const bool sel = (i == m_weaponIdx);

        if (sel)
            m_ui.rect(cx - 220, y - 4, 440, 40, 0.15f, 0.30f, 0.55f, 0.60f);

        m_ui.text(cx - 120, y + 4, sel ? 2.1f : 1.8f, weapons[i],
                  sel ? 1.0f : 0.75f,
                  sel ? 0.9f : 0.75f,
                  sel ? 0.4f : 0.75f);
    }

    m_ui.text(cx - 170, startY + 4 * rowH + 30, 1.6f,
              "SU/GIU = cambia arma   INVIO/ESC = torna",
              0.6f, 0.6f, 0.6f);
}

void PreMatchMenu::renderRules() const
{
    const float W = (float)m_ui.width();
    const float cx = W * 0.5f;

    m_ui.rect(0, 0, W, (float)m_ui.height(), 0.0f, 0.0f, 0.0f, 0.82f);
    m_ui.textCentered(cx, 28, 3.0f, "REGOLE DI GIOCO", 0.95f, 0.85f, 0.3f);

    const float startY = 95.0f;
    const float rowH = 40.0f;
    const float labelX = cx - 300;
    const float valueX = cx + 80;
    const float barX = valueX + 70;
    const float barW = 150.0f;

    for (int i = 0; i < (int)m_rows.size(); ++i)
    {
        const Row& row = m_rows[i];
        const float y = startY + i * rowH;
        const bool sel = (i == m_rulesRow);

        if (sel)
            m_ui.rect(labelX - 12, y - 5, W - (labelX - 12) * 2, rowH - 4,
                      0.15f, 0.32f, 0.55f, 0.55f);

        float lr = sel ? 1.0f : 0.80f;
        float lg = sel ? 0.95f : 0.80f;
        float lb = sel ? 0.50f : 0.80f;
        m_ui.text(labelX, y + 5, 1.8f, row.label, lr, lg, lb);

        char valBuf[32];
        float curF = 0.0f;
        if (row.isInt) { std::snprintf(valBuf, sizeof(valBuf), "%d", *row.iVal); curF = (float)*row.iVal; }
        else           { std::snprintf(valBuf, sizeof(valBuf), "%.1f", *row.fVal); curF = *row.fVal; }

        m_ui.text(valueX, y + 5, 1.9f, valBuf,
                  sel ? 1.0f : 0.90f, sel ? 1.0f : 0.90f, sel ? 0.4f : 0.90f);

        float pct = (row.maxV > row.minV) ? (curF - row.minV) / (row.maxV - row.minV) : 0.0f;
        pct = std::clamp(pct, 0.0f, 1.0f);
        m_ui.rect(barX, y + 10, barW, 10, 0.12f, 0.12f, 0.12f);
        m_ui.rect(barX, y + 10, barW * pct, 10,
                  sel ? 0.3f : 0.2f, sel ? 0.75f : 0.5f, sel ? 1.0f : 0.7f);

        if (sel)
        {
            m_ui.text(valueX - 24, y + 5, 1.9f, "<", 1.0f, 0.8f, 0.2f);
            m_ui.text(valueX + 55, y + 5, 1.9f, ">", 1.0f, 0.8f, 0.2f);
        }
    }

    const float ly = startY + m_rows.size() * rowH + 22;
    m_ui.rect(0, ly - 8, W, 72, 0, 0, 0, 0.55f);
    m_ui.text(cx - 290, ly,      1.6f, "SU/GIU = naviga   SX/DX = modifica valore", 0.6f, 0.6f, 0.6f);
    m_ui.text(cx - 290, ly + 18, 1.6f, "ESC = torna al menu partita", 0.6f, 0.6f, 0.6f);
    m_ui.text(cx - 290, ly + 36, 1.6f, "F5 = salva preset   F6 = carica preset   F7 = gestisci preset",
              0.5f, 0.85f, 1.0f);
}

void PreMatchMenu::renderSavePreset() const
{
    const float W = (float)m_ui.width();
    const float cx = W * 0.5f;

    m_ui.rect(0, 0, W, (float)m_ui.height(), 0.0f, 0.0f, 0.0f, 0.88f);
    m_ui.textCentered(cx, 28, 2.8f, "SALVA PRESET", 0.3f, 1.0f, 0.5f);
    m_ui.text(cx - 190, 68, 1.6f,
              "SU/GIU = slot   Scrivi nome   INVIO = salva   ESC = annulla",
              0.6f, 0.6f, 0.6f);

    const float bx = cx - 200, by = 98.0f;
    m_ui.rect(bx - 4, by - 4, 408, 32, 0.1f, 0.1f, 0.1f);
    m_ui.rect(bx - 2, by - 2, 404, 28, 0.18f, 0.18f, 0.22f);
    std::string shown = m_textInput + "|";
    m_ui.text(bx + 4, by + 4, 1.9f, shown.c_str(), 1.0f, 1.0f, 0.6f);

    const float startY = 146.0f, rowH = 38.0f;
    for (int i = 0; i < UserPresets::MAX; ++i)
    {
        const float y = startY + i * rowH;
        const bool sel = (i == m_presetSlot);
        const MatchSettings* p = m_presets.get(i);
        if (sel) m_ui.rect(cx - 240, y - 4, 480, rowH - 4, 0.1f, 0.4f, 0.2f, 0.5f);
        char buf[64];
        if (p) std::snprintf(buf, sizeof(buf), "Slot %d: %s", i + 1, p->presetName.c_str());
        else   std::snprintf(buf, sizeof(buf), "Slot %d: [vuoto]", i + 1);
        m_ui.text(cx - 220, y + 5, 1.8f, buf,
                  sel ? 1.0f : 0.65f, sel ? 1.0f : 0.65f, sel ? 0.5f : 0.65f);
    }
}

void PreMatchMenu::renderManagePresets() const
{
    const float W = (float)m_ui.width();
    const float cx = W * 0.5f;

    m_ui.rect(0, 0, W, (float)m_ui.height(), 0.0f, 0.0f, 0.0f, 0.88f);
    m_ui.textCentered(cx, 28, 2.8f, "GESTIONE PRESET", 0.8f, 0.6f, 1.0f);

    const float startY = 80.0f, rowH = 44.0f;
    for (int i = 0; i < UserPresets::MAX; ++i)
    {
        const float y = startY + i * rowH;
        const bool sel = (i == m_presetSlot);
        const MatchSettings* p = m_presets.get(i);
        if (sel) m_ui.rect(cx - 290, y - 4, 580, rowH - 4, 0.2f, 0.15f, 0.4f, 0.55f);

        char line1[80], line2[80];
        if (p)
        {
            std::snprintf(line1, sizeof(line1), "Slot %d: %s", i + 1, p->presetName.c_str());
            std::snprintf(line2, sizeof(line2),
                "  T1:%d vite  T2:%d vite  AI-a:%d  AI-n:%d  HP:%.0f  Respawn:%.1fs",
                p->team1Tickets, p->team2Tickets,
                p->team1AiCount, p->team2AiCount,
                p->playerHp, p->respawnDelay);
        }
        else { std::snprintf(line1, sizeof(line1), "Slot %d: [vuoto]", i + 1); line2[0] = '\0'; }

        m_ui.text(cx - 270, y + 3, 1.9f, line1,
                  sel ? 1.0f : (p ? 0.85f : 0.35f),
                  sel ? 0.85f : (p ? 0.85f : 0.35f),
                  sel ? 1.0f  : (p ? 0.85f : 0.35f));
        if (p) m_ui.text(cx - 265, y + 22, 1.4f, line2, 0.55f, 0.7f, 0.55f);
    }

    const float ly = startY + UserPresets::MAX * rowH + 16;
    m_ui.rect(0, ly - 6, W, 56, 0, 0, 0, 0.55f);
    m_ui.text(cx - 280, ly,      1.6f, "INVIO = carica nelle impostazioni   ESC = indietro", 0.6f, 0.6f, 0.6f);
    m_ui.text(cx - 280, ly + 18, 1.6f, "R = rinomina   X o CANC = elimina", 0.8f, 0.5f, 0.5f);
}

void PreMatchMenu::renderRenamePreset() const
{
    const float W = (float)m_ui.width();
    const float H = (float)m_ui.height();
    const float cx = W * 0.5f;

    m_ui.rect(0, 0, W, H, 0.0f, 0.0f, 0.0f, 0.88f);
    m_ui.textCentered(cx, H * 0.35f - 40, 2.8f, "RINOMINA PRESET", 1.0f, 0.7f, 0.3f);

    const MatchSettings* p = m_presets.get(m_presetSlot);
    if (p)
    {
        char info[64];
        std::snprintf(info, sizeof(info), "Slot %d — nome attuale: %s",
                      m_presetSlot + 1, p->presetName.c_str());
        m_ui.text(cx - 175, H * 0.35f, 1.7f, info, 0.7f, 0.7f, 0.7f);
    }
    m_ui.text(cx - 130, H * 0.35f + 28, 1.6f, "Scrivi il nuovo nome:", 0.75f, 0.75f, 0.75f);

    const float bx = cx - 200, by = H * 0.35f + 55;
    m_ui.rect(bx - 4, by - 4, 408, 32, 0.1f, 0.1f, 0.1f);
    m_ui.rect(bx - 2, by - 2, 404, 28, 0.18f, 0.18f, 0.22f);
    std::string shown = m_textInput + "|";
    m_ui.text(bx + 4, by + 4, 1.9f, shown.c_str(), 1.0f, 1.0f, 0.6f);
    m_ui.text(cx - 165, by + 46, 1.6f, "INVIO = conferma   ESC = annulla", 0.55f, 0.55f, 0.55f);
}

void PreMatchMenu::renderLoadPreset() const
{
    const float W = (float)m_ui.width();
    const float cx = W * 0.5f;

    m_ui.rect(0, 0, W, (float)m_ui.height(), 0.0f, 0.0f, 0.0f, 0.88f);
    m_ui.text(cx - 85, 28, 2.8f, "CARICA PRESET", 0.3f, 0.7f, 1.0f);
    m_ui.text(cx - 165, 68, 1.6f, "SU/GIU = naviga   INVIO = carica   ESC = annulla",
              0.6f, 0.6f, 0.6f);

    const float startY = 104.0f, rowH = 44.0f;
    for (int i = 0; i < UserPresets::MAX; ++i)
    {
        const float y = startY + i * rowH;
        const bool sel = (i == m_presetSlot);
        const MatchSettings* p = m_presets.get(i);
        if (sel) m_ui.rect(cx - 290, y - 4, 580, rowH - 4, 0.1f, 0.25f, 0.5f, 0.55f);

        char line1[80], line2[80];
        if (p)
        {
            std::snprintf(line1, sizeof(line1), "Slot %d: %s", i + 1, p->presetName.c_str());
            std::snprintf(line2, sizeof(line2),
                "  T1:%d vite  T2:%d vite  AI-a:%d  AI-n:%d  HP:%.0f  Respawn:%.1fs",
                p->team1Tickets, p->team2Tickets,
                p->team1AiCount, p->team2AiCount,
                p->playerHp, p->respawnDelay);
        }
        else { std::snprintf(line1, sizeof(line1), "Slot %d: [vuoto]", i + 1); line2[0] = '\0'; }

        m_ui.text(cx - 270, y + 3, 1.9f, line1,
                  sel ? 1.0f : (p ? 0.75f : 0.35f),
                  sel ? 1.0f : (p ? 0.75f : 0.35f),
                  sel ? 0.5f : (p ? 0.75f : 0.35f));
        if (p) m_ui.text(cx - 265, y + 22, 1.4f, line2, 0.5f, 0.65f, 0.5f);
    }
}

} // namespace mini