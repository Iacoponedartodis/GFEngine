#include "mini/render/HUD.hpp"

#include <cstdio>
#include <cmath>
#include <algorithm>

namespace mini
{

HUD::HUD(int screenW, int screenH) : m_ui(screenW, screenH) {}

void HUD::tick(float dt)
{
    if (m_hitTimer   > 0.0f) m_hitTimer   -= dt;
    if (m_toastTimer > 0.0f) m_toastTimer -= dt;
    for (auto& l : m_feed) l.age += dt;
}

void HUD::pushFeed(const std::string& msg)
{
    m_feed.push_back({msg, 0.0f});
    if ((int)m_feed.size() > k_feedMax)
        m_feed.erase(m_feed.begin(), m_feed.begin() + ((int)m_feed.size() - k_feedMax));
    // Se l'utente sta leggendo indietro, tieni ferma la vista sui messaggi
    // vecchi anche quando ne arrivano di nuovi.
    if (m_feedScroll > 0)
        m_feedScroll = std::min(m_feedScroll + 1,
                                std::max(0, (int)m_feed.size() - k_feedPanel));
}

void HUD::scrollFeed(int lines)
{
    if (!m_feedOpen) return;
    m_feedScroll = std::clamp(m_feedScroll + lines,
                              0, std::max(0, (int)m_feed.size() - k_feedPanel));
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

        // ── Ticket + contatori: pannelli fazione AI LATI del centro,
        //    così i post (al centro) non coprono mai le scritte ────────
        if (state == 0 && team1Tickets >= 0)
        {
            // Spazio riservato al centro per la barra dei command post
            const float postBox = 30.0f, postGap = 10.0f;
            const float postW = m_posts.empty() ? 0.0f
                : (float)m_posts.size() * postBox
                + (float)(m_posts.size() - 1) * postGap;
            const float half = postW * 0.5f + (m_posts.empty() ? 0.0f : 26.0f);

            const float PW = 195.0f, PH = 34.0f, PY = 6.0f;
            char buf[48];

            // Alleati (blu) — a sinistra del centro
            const float axr = cx - half;             // bordo destro pannello
            m_ui.rect(axr - PW, PY, PW, PH, 0.06f, 0.10f, 0.22f, 0.78f);
            m_ui.rect(axr - PW, PY, 3, PH, 0.25f, 0.50f, 1.0f);
            m_ui.text(axr - PW + 10, PY + 3, 1.3f, "ALLEATI", 0.55f, 0.70f, 1.0f);
            std::snprintf(buf, sizeof(buf), "%d vivi   %d ticket",
                          aliveAllies, team1Tickets);
            m_ui.text(axr - PW + 10, PY + 17, 1.6f, buf, 0.9f, 0.93f, 1.0f);

            // Nemici (rosso) — a destra del centro
            const float exl = cx + half;             // bordo sinistro pannello
            m_ui.rect(exl, PY, PW, PH, 0.22f, 0.07f, 0.06f, 0.78f);
            m_ui.rect(exl + PW - 3, PY, 3, PH, 1.0f, 0.30f, 0.25f);
            m_ui.text(exl + 10, PY + 3, 1.3f, "NEMICI", 1.0f, 0.60f, 0.55f);
            std::snprintf(buf, sizeof(buf), "%d vivi   %d ticket",
                          aliveEnemies, team2Tickets);
            m_ui.text(exl + 10, PY + 17, 1.6f, buf, 1.0f, 0.92f, 0.9f);
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
                float mkR = 1.0f, mkG = 0.85f, mkB = 0.25f;
                if (m_hitWasKill) { mkG = 0.2f; mkB = 0.15f; }
                m_ui.rect(cx-d-s, cy-d-s, s, s, mkR, mkG, mkB);
                m_ui.rect(cx+d,   cy-d-s, s, s, mkR, mkG, mkB);
                m_ui.rect(cx-d-s, cy+d,   s, s, mkR, mkG, mkB);
                m_ui.rect(cx+d,   cy+d,   s, s, mkR, mkG, mkB);
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

        // ── Log chat (17_SandboxTools) — basso sinistra, sopra la barra HP.
        //    Chiusa: ultime righe recenti che sfumano. Aperta (L): pannello
        //    con lo storico recente. ─────────────────────────────────────
        {
            const float lineH = 15.0f, x0 = PAD;
            if (m_feedOpen)
            {
                const int total = (int)m_feed.size();
                const int n     = std::min(total, k_feedPanel);
                // Finestra spostata indietro di m_feedScroll righe dal fondo
                const int end   = std::max(n, total - m_feedScroll);
                const float panelH = (float)k_feedPanel * lineH + 26.0f;
                const float y0 = H - 60.0f - panelH;
                m_ui.rect(x0 - 6, y0 - 6, 470, panelH + 12, 0.03f, 0.04f, 0.06f, 0.82f);
                char hdr[80];
                if (m_feedScroll > 0)
                    std::snprintf(hdr, sizeof(hdr),
                        "LOG EVENTI   [L] chiudi  [PAGSU/PAGGIU] scorri  (-%d)", m_feedScroll);
                else
                    std::snprintf(hdr, sizeof(hdr),
                        "LOG EVENTI   [L] chiudi  [PAGSU/PAGGIU] scorri");
                m_ui.text(x0, y0, 1.4f, hdr, 0.6f, 0.75f, 0.95f);
                for (int i = 0; i < n; ++i)
                {
                    const auto& l = m_feed[end - n + i];
                    m_ui.text(x0, y0 + 20.0f + (float)i * lineH, 1.25f,
                              l.text.c_str(), 0.85f, 0.87f, 0.9f);
                }
            }
            else if (state == 0)
            {
                // Righe recenti (fade), dalla più vecchia in alto
                int shown = 0;
                const int n = (int)m_feed.size();
                for (int i = std::max(0, n - k_feedRecent); i < n; ++i)
                {
                    const auto& l = m_feed[i];
                    if (l.age > k_feedFadeSec) continue;
                    const float a = (l.age < k_feedFadeSec - 1.5f)
                                  ? 0.85f
                                  : 0.85f * (k_feedFadeSec - l.age) / 1.5f;
                    const float y = H - 66.0f
                        - (float)(n - 1 - i) * lineH;
                    // Ui2D::text non ha alpha: il fade scurisce il colore
                    m_ui.text(x0, y, 1.25f, l.text.c_str(), 0.8f*a, 0.83f*a, 0.88f*a);
                    ++shown;
                }
                (void)shown;
            }
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