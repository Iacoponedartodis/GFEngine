#include "mini/render/HUD.hpp"

#include <cstdio>
#include <cmath>

namespace mini
{

HUD::HUD(int screenW, int screenH) : m_ui(screenW, screenH) {}

void HUD::tick(float dt)
{
    if (m_hitTimer   > 0.0f) m_hitTimer   -= dt;
    if (m_toastTimer > 0.0f) m_toastTimer -= dt;
}

void HUD::hitmarker(bool kill)
{
    m_hitTimer   = kill ? 0.45f : 0.18f;
    m_hitWasKill = kill;
}

void HUD::toast(const std::string& msg, float seconds)
{
    m_toast      = msg;
    m_toastTimer = seconds;
}

void HUD::render(float playerHp, float playerMaxHp, int state,
                 float weaponHeat, bool overheated, const char* weaponName,
                 int team1Tickets, int team2Tickets,
                 int aliveAllies, int aliveEnemies)
{
    m_ui.begin();

    const float W = (float)m_ui.width(), H = (float)m_ui.height();
    const float cx = W * 0.5f, cy = H * 0.5f;

    if (state == -1) // Paused
    {
        m_ui.rect(0, 0, W, H, 0, 0, 0, 0.65f);
        m_ui.text(cx - 75,  cy - 90, 4.5f, "PAUSA",                   0.95f, 0.95f, 0.95f);
        m_ui.text(cx - 105, cy - 20, 2.3f, "ESC  =  Riprendi",        0.75f, 0.85f, 0.95f);
        m_ui.text(cx - 105, cy + 12, 2.3f, "R    =  Riavvia partita", 0.75f, 0.85f, 0.95f);
        m_ui.text(cx - 105, cy + 44, 2.3f, "K    =  Respawn volontario",     0.75f, 0.85f, 0.95f);
        m_ui.text(cx - 105, cy + 76, 2.3f, "O    =  Opzioni",         0.75f, 0.85f, 0.95f);
        m_ui.text(cx - 105, cy +108, 2.3f, "Q    =  Menu principale",  0.75f, 0.85f, 0.95f);
    }
    else // Playing / Win / Lose
    {
        const float PAD = 14;

        // ── Barra HP (basso sinistra) ────────────────────────────────
        const float BW = 220, BH = 18;
        const float BY = H - PAD - BH;
        m_ui.rect(PAD, BY, BW, BH, 0.15f, 0.08f, 0.08f);

        float pct = (playerMaxHp > 0) ? playerHp / playerMaxHp : 0;
        if (pct < 0) pct = 0; if (pct > 1) pct = 1;
        float fr = (pct < 0.5f) ? 1.0f : 2.0f - pct * 2;
        float fg = (pct > 0.5f) ? 1.0f : pct * 2;
        m_ui.rect(PAD, BY, BW * pct, BH, fr, fg, 0.05f);
        m_ui.border(PAD, BY, BW, BH, 0.7f, 0.7f, 0.7f);

        char hpBuf[32];
        std::snprintf(hpBuf, sizeof(hpBuf), "HP  %d / %d",
                      (int)(playerHp > 0 ? playerHp : 0), (int)playerMaxHp);
        m_ui.text(PAD, BY - 16, 1.6f, hpBuf, 0.9f, 0.9f, 0.9f);

        // ── Ticket + contatori (alto centro) ─────────────────────────
        if (state == 0 && team1Tickets >= 0)
        {
            char tBuf[96];
            std::snprintf(tBuf, sizeof(tBuf),
                "ALLEATI  %d vivi  %d ticket   |   NEMICI  %d vivi  %d ticket",
                aliveAllies, team1Tickets, aliveEnemies, team2Tickets);
            m_ui.rect(cx - 260, 8, 520, 22, 0, 0, 0, 0.55f);
            m_ui.text(cx - 250, 12, 1.6f, tBuf, 0.85f, 0.85f, 0.85f);
        }

        // ── Barra calore + crosshair (solo in Playing) ───────────────
        if (state == 0)
        {
            const float HBW = 180, HBH = 14;
            const float HBX = W - PAD - HBW;
            const float HBY = H - PAD - HBH;

            m_ui.rect(HBX, HBY, HBW, HBH, 0.10f, 0.08f, 0.08f);

            float hr, hg, hb;
            if (overheated)
            {
                float blink = std::fmod(weaponHeat * 10.0f, 2.0f) < 1.0f ? 1.0f : 0.4f;
                hr = blink; hg = 0.1f; hb = 0.05f;
            }
            else
            {
                hr = (weaponHeat < 0.5f) ? weaponHeat * 2.0f : 1.0f;
                hg = (weaponHeat < 0.5f) ? 1.0f : 1.0f - (weaponHeat - 0.5f) * 2.0f;
                hb = 0.05f;
            }
            m_ui.rect(HBX, HBY, HBW * weaponHeat, HBH, hr, hg, hb);
            m_ui.border(HBX, HBY, HBW, HBH, 0.5f, 0.5f, 0.5f);

            if (weaponName)
            {
                char wBuf[64];
                if (overheated)
                    std::snprintf(wBuf, sizeof(wBuf), "%s  [OVERHEAT]", weaponName);
                else
                    std::snprintf(wBuf, sizeof(wBuf), "%s  %d%%", weaponName, (int)(weaponHeat * 100));
                m_ui.text(HBX, HBY - 16, 1.5f, wBuf,
                          overheated ? 1.0f : 0.85f,
                          overheated ? 0.3f : 0.85f,
                          overheated ? 0.2f : 0.85f);
            }

            // Crosshair — ROSSA quando il mirino è su una hitbox nemica reale
            const float arm = 10, gap = 4, thick = 1.5f;
            float cr = 0.9f, cg = 0.9f, cb = 0.9f;
            if (m_aimOnTarget) { cr = 1.0f; cg = 0.25f; cb = 0.2f; }
            if (overheated)    { cr = 1.0f; cg = 0.55f; cb = 0.1f; }
            m_ui.rect(cx-thick, cy-arm-gap, thick*2, arm, cr, cg, cb);
            m_ui.rect(cx-thick, cy+gap,     thick*2, arm, cr, cg, cb);
            m_ui.rect(cx-arm-gap, cy-thick, arm, thick*2, cr, cg, cb);
            m_ui.rect(cx+gap,     cy-thick, arm, thick*2, cr, cg, cb);

            // Hitmarker — 4 tacche diagonali per ~0.2s dopo un colpo a segno
            // (giallo = colpito, rosso = eliminato)
            if (m_hitTimer > 0.0f)
            {
                const float d = 9.0f, s = 4.0f;
                float hr = 1.0f, hg = 0.85f, hb = 0.25f;
                if (m_hitWasKill) { hg = 0.2f; hb = 0.15f; }
                m_ui.rect(cx-d-s, cy-d-s, s, s, hr, hg, hb);
                m_ui.rect(cx+d,   cy-d-s, s, s, hr, hg, hb);
                m_ui.rect(cx-d-s, cy+d,   s, s, hr, hg, hb);
                m_ui.rect(cx+d,   cy+d,   s, s, hr, hg, hb);
            }
        }

        // ── Stato command post (alto centro): quadrato colorato per
        //    proprietario + lettera + barra di cattura ──────────────────
        if (!m_posts.empty() && (state == 0 || state == -1))
        {
            const float box = 30.0f, gapP = 10.0f;
            const float totW = (float)m_posts.size() * box
                             + (float)(m_posts.size() - 1) * gapP;
            float px = cx - totW * 0.5f;
            for (const auto& p : m_posts)
            {
                float r = 0.55f, g = 0.55f, b = 0.55f;          // neutrale
                if (p.owner == 1) { r = 0.25f; g = 0.50f; b = 1.00f; }
                if (p.owner == 2) { r = 1.00f; g = 0.25f; b = 0.25f; }
                m_ui.rect(px, 8, box, box, r*0.35f, g*0.35f, b*0.35f, 0.85f);
                m_ui.rect(px, 8, box, 3, r, g, b);              // bordo alto
                const char letter[2] = { p.label.empty() ? '?' : p.label[0], 0 };
                m_ui.text(px + 10, 14, 2.0f, letter, r, g, b);

                // Barra di cattura (colore del team che sta catturando)
                if (p.capturingTeam != 0 && p.progress01 > 0.01f)
                {
                    float cr2 = (p.capturingTeam == 1) ? 0.25f : 1.0f;
                    float cb2 = (p.capturingTeam == 1) ? 1.0f  : 0.25f;
                    m_ui.rect(px, 8 + box + 2, box, 4, 0.1f, 0.1f, 0.1f);
                    m_ui.rect(px, 8 + box + 2, box * p.progress01, 4,
                              cr2, 0.3f, cb2);
                }
                px += box + gapP;
            }
        }

        // Toast (messaggi tipo "game_state.json salvato") — alto centro
        if (m_toastTimer > 0.0f && !m_toast.empty())
        {
            const float tw = (float)m_toast.size() * 9.0f;
            m_ui.rect(cx - tw*0.5f - 8, 52, tw + 16, 26, 0.05f, 0.05f, 0.08f, 0.8f);
            m_ui.text(cx - tw*0.5f, 58, 1.6f, m_toast.c_str(), 0.95f, 0.9f, 0.5f);
        }

        // ── Overlay Win/Lose ─────────────────────────────────────────
        if (state == 1)
        {
            m_ui.rect(0, 0, W, H, 0, 0.22f, 0, 0.52f);
            m_ui.text(cx - 215, cy - 55, 5.0f, "MISSIONE COMPLETATA!", 0.2f, 1.0f, 0.3f);
            m_ui.text(cx - 170, cy + 15, 2.3f, "Tutti i nemici eliminati.", 0.8f, 0.95f, 0.8f);
            m_ui.text(cx - 155, cy + 50, 2.3f, "R = ricomincia  |  Q = menu", 0.7f, 0.9f, 0.7f);
        }
        else if (state == 2)
        {
            m_ui.rect(0, 0, W, H, 0.32f, 0, 0, 0.52f);
            m_ui.text(cx - 115, cy - 55, 5.0f, "GAME OVER", 1.0f, 0.2f, 0.15f);
            m_ui.text(cx - 125, cy + 15, 2.3f, "Sei stato eliminato.", 0.95f, 0.75f, 0.75f);
            m_ui.text(cx - 155, cy + 50, 2.3f, "R = ricomincia  |  Q = menu", 0.9f, 0.6f, 0.6f);
        }
    }

    m_ui.end();
}

} // namespace mini