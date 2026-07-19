#include "mini/render/SandboxMenu.hpp"
#include "mini/core/GameConfig.hpp"
#include <SDL2/SDL.h>
#include <algorithm>
#include <cstdio>

namespace mini
{

SandboxMenu::SandboxMenu(int w, int h) : m_ui(w, h) {}

void SandboxMenu::setWeapons(const std::vector<std::pair<std::string, std::string>>& idName)
{
    m_weapons = idName;
    m_weaponSel = 0;
}

const std::string& SandboxMenu::selectedWeaponId() const
{
    static const std::string empty;
    if (m_weaponSel < 0 || m_weaponSel >= (int)m_weapons.size()) return empty;
    return m_weapons[m_weaponSel].first;
}

void SandboxMenu::setMaps(const std::vector<std::pair<std::string, std::string>>& idName)
{
    m_maps = idName;
    if (m_mapSel >= (int)m_maps.size()) m_mapSel = 0;
}

const std::string& SandboxMenu::selectedMapId() const
{
    static const std::string fallback = "firebase";
    if (m_mapSel < 0 || m_mapSel >= (int)m_maps.size()) return fallback;
    return m_maps[m_mapSel].first;
}

SandboxMenu::Result SandboxMenu::handleKey(int sc)
{
    if (sc == SDL_SCANCODE_TAB || sc == SDL_SCANCODE_ESCAPE)
        return Result::Close;

    if (sc == SDL_SCANCODE_Q || sc == SDL_SCANCODE_E)
    { m_page = 1 - m_page; return Result::None; }

    const bool up    = (sc == SDL_SCANCODE_UP);
    const bool down  = (sc == SDL_SCANCODE_DOWN);
    const bool left  = (sc == SDL_SCANCODE_LEFT);
    const bool right = (sc == SDL_SCANCODE_RIGHT);
    const bool enter = (sc == SDL_SCANCODE_RETURN || sc == SDL_SCANCODE_KP_ENTER);

    if (m_page == 0) // ── Armi ─────────────────────────────────────────
    {
        const int n = (int)m_weapons.size();
        if (n == 0) return Result::None;
        if (up)   m_weaponSel = (m_weaponSel - 1 + n) % n;
        if (down) m_weaponSel = (m_weaponSel + 1) % n;
        if (left || right) m_weaponSlot = 1 - m_weaponSlot;
        if (enter) return Result::EquipWeapon;
    }
    else // ── Simulazione ──────────────────────────────────────────────
    {
        // Righe: 0 mappa, 1 modalità, 2 alleati, 3 nemici, 4 tkt alleati,
        //        5 tkt nemici, 6 respawn, 7 avvia/ferma sim,
        //        8 riavvia sandbox sulla mappa scelta
        constexpr int ROWS = 9;
        if (up)   m_simSel = (m_simSel - 1 + ROWS) % ROWS;
        if (down) m_simSel = (m_simSel + 1) % ROWS;

        const int dir = right ? +1 : (left ? -1 : 0);
        if (dir != 0)
        {
            const int nMaps = (int)m_maps.size();
            switch (m_simSel)
            {
            case 0: if (nMaps > 0)
                        m_mapSel = (m_mapSel + dir + nMaps) % nMaps;        break;
            case 1: simModeIndex = (simModeIndex + (dir > 0 ? 1 : 2)) % 3; break;
            case 2: allyCount    = std::clamp(allyCount  + dir, 1, config::MAX_AI_PER_TEAM); break;
            case 3: enemyCount   = std::clamp(enemyCount + dir, 1, config::MAX_AI_PER_TEAM); break;
            case 4: team1Tickets = std::clamp(team1Tickets + dir, 1, 50);  break;
            case 5: team2Tickets = std::clamp(team2Tickets + dir, 1, 50);  break;
            case 6: respawnDelay = std::clamp(respawnDelay + (float)dir, 0.0f, 30.0f); break;
            }
        }
        if (enter && m_simSel == 7) return Result::ToggleSim;
        if (enter && m_simSel == 8) return Result::RestartSandbox;
    }
    return Result::None;
}

// ── Mouse ─────────────────────────────────────────────────────────────────
// Geometrie identiche a render() (stesso PX/PY/PW/PH, y0, rowH).
SandboxMenu::Result SandboxMenu::handleMouse(float mx, float my, bool clicked)
{
    const float W = (float)m_ui.width(), H = (float)m_ui.height();
    const float PX = W * 0.5f - 260.0f, PY = 70.0f, PW = 520.0f, PH = H - 170.0f;

    // Tab pagine (ARMI / SIMULAZIONE) — solo su click
    for (int i = 0; i < 2; ++i)
    {
        const float tx = PX + 18.0f + (float)i * 160.0f;
        if (clicked && mx >= tx && mx <= tx + 150 && my >= PY + 10 && my <= PY + 34)
            m_page = i;
    }

    const float y0 = PY + 66.0f, rowH = 24.0f;

    if (m_page == 0)   // ── Armi ──────────────────────────────────────────
    {
        // Slot primaria/secondaria
        if (mx >= PX + 70 && mx <= PX + 260 && my >= y0 - 2 && my <= y0 + 18)
        { if (clicked) m_weaponSlot = 1 - m_weaponSlot; }

        const int n = (int)m_weapons.size();
        if (n > 0)
        {
            const float listY = y0 + 26.0f;
            const int first = std::max(0, std::min(m_weaponSel - k_visibleWeapons / 2,
                                                    n - k_visibleWeapons));
            const int last = std::min(n, first + k_visibleWeapons);
            for (int i = first; i < last; ++i)
            {
                const float y = listY + (float)(i - first) * rowH;
                if (mx < PX + 10 || mx > PX + 10 + (PW - 20)
                    || my < y - 3 || my > y - 3 + (rowH - 2)) continue;
                m_weaponSel = i;
                if (clicked) return Result::EquipWeapon;   // click = equipaggia
                break;
            }
        }
    }
    else               // ── Simulazione ───────────────────────────────────
    {
        const float rowsY = y0 + 46.0f;
        // Il valore "< N >" è a destra (PX+PW-200): split sul suo centro così la
        // freccia < diminuisce e > aumenta (come nei menu PreMatch).
        const int dir = (mx < PX + PW - 165.0f) ? -1 : +1;
        for (int i = 0; i < 7; ++i)
        {
            const float y = rowsY + (float)i * 30.0f;
            if (mx < PX + 10 || mx > PX + 10 + (PW - 20)
                || my < y - 4 || my > y - 4 + 24) continue;
            m_simSel = i;
            if (clicked)
            {
                const int nMaps = (int)m_maps.size();
                switch (i)
                {
                case 0: if (nMaps > 0) m_mapSel = (m_mapSel + dir + nMaps) % nMaps; break;
                case 1: simModeIndex = (simModeIndex + (dir > 0 ? 1 : 2)) % 3;      break;
                case 2: allyCount    = std::clamp(allyCount  + dir, 1, config::MAX_AI_PER_TEAM); break;
                case 3: enemyCount   = std::clamp(enemyCount + dir, 1, config::MAX_AI_PER_TEAM); break;
                case 4: team1Tickets = std::clamp(team1Tickets + dir, 1, 50); break;
                case 5: team2Tickets = std::clamp(team2Tickets + dir, 1, 50); break;
                case 6: respawnDelay = std::clamp(respawnDelay + (float)dir, 0.0f, 30.0f); break;
                }
            }
            return Result::None;
        }
        // Riga avvia/ferma sim (7) e riavvia sandbox (8)
        const float y7 = rowsY + 7.0f * 30.0f + 10.0f;
        if (mx >= PX + 10 && mx <= PX + 10 + (PW - 20) && my >= y7 - 4 && my <= y7 - 4 + 28)
        { m_simSel = 7; if (clicked) return Result::ToggleSim; return Result::None; }
        const float y8 = rowsY + 7.0f * 30.0f + 46.0f;
        if (mx >= PX + 10 && mx <= PX + 10 + (PW - 20) && my >= y8 - 4 && my <= y8 - 4 + 28)
        { m_simSel = 8; if (clicked) return Result::RestartSandbox; return Result::None; }
    }
    return Result::None;
}

void SandboxMenu::render() const
{
    const float W = (float)m_ui.width();
    const float H = (float)m_ui.height();

    m_ui.begin();

    const float PX = W * 0.5f - 260.0f, PY = 70.0f, PW = 520.0f, PH = H - 170.0f;
    m_ui.rect(PX, PY, PW, PH, 0.04f, 0.05f, 0.09f, 0.92f);
    m_ui.border(PX, PY, PW, PH, 0.35f, 0.45f, 0.65f);

    // Header pagine
    const char* pages[2] = {"ARMI", "SIMULAZIONE"};
    float tx = PX + 18.0f;
    for (int i = 0; i < 2; ++i)
    {
        const bool cur = (i == m_page);
        m_ui.text(tx, PY + 12, cur ? 2.0f : 1.6f, pages[i],
                  cur ? 1.0f : 0.5f, cur ? 0.85f : 0.5f, cur ? 0.4f : 0.55f);
        tx += 160.0f;
    }
    m_ui.text(PX + 18, PY + 38, 1.3f,
              "[TAB] chiudi   [Q]/[E] pagina   [SU]/[GIU] scegli   [SIN]/[DES] valore   [INVIO] ok",
              0.5f, 0.55f, 0.6f);

    const float y0 = PY + 66.0f, rowH = 24.0f;

    if (m_page == 0) // ── Armi ─────────────────────────────────────────
    {
        m_ui.text(PX + 18, y0, 1.6f, "Slot:", 0.6f, 0.65f, 0.7f);
        m_ui.text(PX + 70, y0, 1.6f,
                  m_weaponSlot == 0 ? "< PRIMARIA >" : "< SECONDARIA >",
                  1.0f, 0.85f, 0.4f);

        const int n = (int)m_weapons.size();
        if (n == 0)
        { m_ui.text(PX + 18, y0 + 26, 1.6f, "Nessun'arma nel registry.", 0.7f, 0.6f, 0.5f); }

        const float listY = y0 + 26.0f;
        int first = std::max(0, std::min(m_weaponSel - k_visibleWeapons / 2,
                                          n - k_visibleWeapons));
        const int last = std::min(n, first + k_visibleWeapons);
        for (int i = first; i < last; ++i)
        {
            const bool sel = (i == m_weaponSel);
            const float y = listY + (float)(i - first) * rowH;
            if (sel) m_ui.rect(PX + 10, y - 3, PW - 20, rowH - 2,
                               0.14f, 0.25f, 0.45f, 0.8f);
            m_ui.text(PX + 22, y, 1.6f, m_weapons[i].second.c_str(),
                      sel ? 1.0f : 0.7f, sel ? 0.95f : 0.7f, sel ? 0.55f : 0.72f);
        }
        if (n > k_visibleWeapons)
        {
            char buf[32];
            std::snprintf(buf, sizeof(buf), "%d / %d", m_weaponSel + 1, n);
            m_ui.text(PX + PW - 80, PY + 38, 1.3f, buf, 0.5f, 0.55f, 0.6f);
        }
        m_ui.text(PX + 18, PY + PH - 26, 1.4f,
                  "INVIO equipaggia nello slot scelto. [P] in gioco = partita vera (PreMatch).",
                  0.55f, 0.65f, 0.55f);
    }
    else // ── Simulazione ──────────────────────────────────────────────
    {
        m_ui.text(PX + 18, y0, 1.5f,
                  "Battaglia AI contro AI con te come osservatore neutrale", 0.75f, 0.78f, 0.82f);
        m_ui.text(PX + 18, y0 + 18, 1.5f,
                  "in volo libero (WASD + SPAZIO/CTRL). [L] log eventi.", 0.75f, 0.78f, 0.82f);

        static const char* kModes[3] = {"Conquista", "Assalto", "Difesa"};
        char buf[64];
        struct Row { const char* label; };
        const Row rows[7] = {{"Mappa"}, {"Modalita'"}, {"Alleati AI"}, {"Nemici AI"},
                             {"Ticket alleati"}, {"Ticket nemici"}, {"Respawn (s)"}};
        const float rowsY = y0 + 46.0f;
        for (int i = 0; i < 7; ++i)
        {
            const bool sel = (i == m_simSel);
            const float y = rowsY + (float)i * 30.0f;
            if (sel) m_ui.rect(PX + 10, y - 4, PW - 20, 24, 0.14f, 0.25f, 0.45f, 0.8f);
            m_ui.text(PX + 22, y, 1.6f, rows[i].label,
                      sel ? 1.0f : 0.7f, sel ? 0.95f : 0.7f, sel ? 0.55f : 0.72f);
            if (i == 0)
            {
                const char* mapName = (m_mapSel >= 0 && m_mapSel < (int)m_maps.size())
                                    ? m_maps[m_mapSel].second.c_str() : "firebase";
                std::snprintf(buf, sizeof(buf), "%s %s %s",
                              sel ? "<" : " ", mapName, sel ? ">" : " ");
            }
            else if (i == 1)
                std::snprintf(buf, sizeof(buf), "%s %s %s",
                              sel ? "<" : " ", kModes[simModeIndex], sel ? ">" : " ");
            else
            {
                const int v = (i == 2) ? allyCount : (i == 3) ? enemyCount
                            : (i == 4) ? team1Tickets : (i == 5) ? team2Tickets
                            : (int)respawnDelay;
                std::snprintf(buf, sizeof(buf), "%s %d %s",
                              sel ? "<" : " ", v, sel ? ">" : " ");
            }
            m_ui.text(PX + PW - 200, y, 1.6f, buf, 0.9f, 0.9f, 0.9f);
        }

        // Riga avvia/ferma sim
        {
            const bool sel = (m_simSel == 7);
            const float y = rowsY + 7.0f * 30.0f + 10.0f;
            m_ui.rect(PX + 10, y - 4, PW - 20, 28, simRunning ? 0.35f : 0.18f,
                      simRunning ? 0.18f : 0.35f, 0.2f, sel ? 0.95f : 0.6f);
            m_ui.text(PX + 22, y, 1.8f,
                      simRunning ? "INVIO: ferma la simulazione (torna sandbox)"
                                 : "INVIO: avvia la simulazione AI",
                      0.95f, 0.95f, 0.8f);
        }

        // Riga riavvia sandbox sulla mappa scelta
        {
            const bool sel = (m_simSel == 8);
            const float y = rowsY + 7.0f * 30.0f + 46.0f;
            m_ui.rect(PX + 10, y - 4, PW - 20, 28, 0.16f, 0.22f, 0.40f,
                      sel ? 0.95f : 0.6f);
            m_ui.text(PX + 22, y, 1.8f,
                      "INVIO: riavvia la SANDBOX sulla mappa scelta",
                      0.85f, 0.9f, 1.0f);
        }

        m_ui.text(PX + 18, PY + PH - 26, 1.4f,
                  "Per una partita vera: chiudi il menu e premi [P] (PreMatch classico).",
                  0.55f, 0.65f, 0.55f);
    }

    m_ui.end();
}

} // namespace mini
