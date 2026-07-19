#include "mini/render/HUD.hpp"

#include <cstdio>
#include <cmath>
#include <algorithm>

namespace mini
{

// ── Proiezione mondo→schermo della mappa di respawn (doc 30) ────────────────
// Definita UNA volta e usata sia dal render sia dal picking → render e click
// non possono mai divergere. Il pannello è funzione della sola dimensione
// schermo (costante nel frame), quindi ricalcolarlo in due punti è sicuro.
namespace
{
    struct RMProj
    {
        float ox = 0, oy = 0, scale = 1, minX = 0, minZ = 0;
        void to(float x, float z, float& sx, float& sy) const
        { sx = ox + (x - minX) * scale; sy = oy + (z - minZ) * scale; }
    };

    void rmPanel(int W, int H, float& px, float& py, float& pw, float& ph)
    {
        pw = (float)W * 0.5f; ph = (float)H * 0.54f;
        px = ((float)W - pw) * 0.5f; py = (float)H * 0.24f;
    }

    RMProj rmProj(const HUD::RespawnMap& r, int W, int H)
    {
        float px, py, pw, ph; rmPanel(W, H, px, py, pw, ph);
        const float worldW = std::max(0.001f, r.maxX - r.minX);
        const float worldD = std::max(0.001f, r.maxZ - r.minZ);
        const float scale  = std::min(pw / worldW, ph / worldD);
        const float ox = px + (pw - worldW * scale) * 0.5f;
        const float oy = py + (ph - worldD * scale) * 0.5f;
        return {ox, oy, scale, r.minX, r.minZ};
    }

    // ── Bottoni degli overlay Pausa / Fine partita (doc: mouse ovunque) ──
    // Una sola definizione dei rettangoli, usata da render e da overlayPick.
    struct OvBtn { float x, y, w, h; const char* label; HUD::OverlayAction act; };
    std::vector<OvBtn> overlayButtons(int W, int H, int state)
    {
        const float cx = (float)W * 0.5f, cy = (float)H * 0.5f;
        const float bw = 320.0f, bh = 40.0f, gap = 10.0f;
        std::vector<OvBtn> b;
        if (state == -1)   // Pausa
        {
            const char* L[5] = { "Riprendi", "Riavvia partita",
                                 "Respawn volontario", "Opzioni", "Menu principale" };
            const HUD::OverlayAction A[5] = {
                HUD::OverlayAction::Resume,  HUD::OverlayAction::Restart,
                HUD::OverlayAction::Respawn, HUD::OverlayAction::Options,
                HUD::OverlayAction::MainMenu };
            float y = cy - 40.0f;
            for (int i = 0; i < 5; ++i)
            { b.push_back({cx - bw * 0.5f, y, bw, bh, L[i], A[i]}); y += bh + gap; }
        }
        else if (state == 1 || state == 2)   // Win / Lose
        {
            b.push_back({cx - bw*0.5f, cy + 110.0f,               bw, bh,
                         "Ricomincia",      HUD::OverlayAction::Restart});
            b.push_back({cx - bw*0.5f, cy + 110.0f + bh + gap,    bw, bh,
                         "Menu principale", HUD::OverlayAction::MainMenu});
        }
        return b;
    }
}

HUD::HUD(int screenW, int screenH) : m_ui(screenW, screenH) {}

HUD::OverlayAction HUD::overlayPick(float mx, float my, int state) const
{
    for (const auto& b : overlayButtons(m_ui.width(), m_ui.height(), state))
        if (mx >= b.x && mx <= b.x + b.w && my >= b.y && my <= b.y + b.h)
            return b.act;
    return OverlayAction::None;
}

int HUD::respawnMapPick(float mx, float my) const
{
    const RespawnMap& r = m_respawnMap;
    if (!r.active) return -1;
    const RMProj p = rmProj(r, m_ui.width(), m_ui.height());
    int best = -1; float bestD2 = 16.0f * 16.0f;   // raggio di hit ~16 px
    for (int i = 0; i < (int)r.markers.size(); ++i)
    {
        float sx, sy; p.to(r.markers[i].x, r.markers[i].z, sx, sy);
        const float dx = mx - sx, dy = my - sy, d2 = dx * dx + dy * dy;
        if (d2 < bestD2) { bestD2 = d2; best = i; }
    }
    return best;
}

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
        m_ui.textCentered(cx, cy - 110, 4.5f, "PAUSA", 0.95f, 0.95f, 0.95f);
        for (const auto& b : overlayButtons(m_ui.width(), m_ui.height(), -1))
        {
            const bool hv = (m_mouseX >= b.x && m_mouseX <= b.x + b.w
                          && m_mouseY >= b.y && m_mouseY <= b.y + b.h);
            m_ui.rect(b.x, b.y, b.w, b.h, hv ? 0.14f : 0.08f,
                      hv ? 0.30f : 0.10f, hv ? 0.52f : 0.14f, hv ? 0.92f : 0.6f);
            m_ui.border(b.x, b.y, b.w, b.h, 0.3f, 0.4f, 0.6f);
            m_ui.textCentered(cx, b.y + 11, 2.0f, b.label,
                              hv ? 1.0f : 0.78f, hv ? 0.95f : 0.85f, hv ? 0.6f : 0.95f);
        }
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

            // Obiettivi della missione (ADR-019) — colonna a sinistra, sotto i
            // pannelli in alto. Un obiettivo che il giocatore non vede non
            // esiste: senza questo pannello il framework era un sistema isolato
            // (GDD 21.2). I primari sono in evidenza — sono quelli che decidono
            // la missione; gli opzionali restano leggibili ma defilati.
            if (!m_objectives.empty())
            {
                const float OX = 14.0f, OY = PY + PH + 34.0f, OW = 250.0f;
                const float rowH = 17.0f;
                const float boxH = 22.0f + rowH * (float)m_objectives.size();
                m_ui.rect(OX, OY, OW, boxH, 0.06f, 0.07f, 0.12f, 0.72f);
                m_ui.rect(OX, OY, 3, boxH, 0.95f, 0.80f, 0.35f);
                m_ui.text(OX + 10, OY + 4, 1.3f, "OBIETTIVI", 0.95f, 0.85f, 0.45f);

                float y = OY + 18.0f;
                for (const auto& o : m_objectives)
                {
                    // Il colore porta lo stato senza aggiungere testo: verde =
                    // fatto, rosso = fallito, chiaro = da fare.
                    float r = 0.88f, g = 0.92f, b = 1.0f;
                    if      (o.state == 1) { r = 0.45f; g = 0.90f; b = 0.55f; }
                    else if (o.state == 2) { r = 1.00f; g = 0.45f; b = 0.40f; }
                    else if (!o.primary)   { r = 0.62f; g = 0.66f; b = 0.75f; }
                    m_ui.text(OX + 10, y, o.primary ? 1.5f : 1.3f,
                              o.label.c_str(), r, g, b);
                    y += rowH;
                }
            }

            // Squadra: ordine corrente (ADR-020 Phase B) — sotto gli alleati,
            // stessa colonna. Un ordine non deve mai essere invisibile: se il
            // giocatore non vede cosa ha chiesto, non sta comandando niente.
            if (!m_squadOrder.empty())
            {
                const float SY = PY + PH + 4.0f;
                m_ui.rect(axr - PW, SY, PW, 22.0f, 0.06f, 0.16f, 0.14f, 0.78f);
                m_ui.rect(axr - PW, SY, 3, 22.0f, 0.30f, 0.85f, 0.60f);
                m_ui.text(axr - PW + 10, SY + 5, 1.5f, m_squadOrder.c_str(),
                          0.75f, 1.0f, 0.88f);
            }

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

            // Crosshair — ROSSA su hitbox nemica, VERDE su un compagno (feedback
            // per i comandi Revive/CoveringFire). Il surriscaldamento vince su
            // tutto (è uno stato dell'arma che il giocatore deve notare subito).
            const float arm = 10, gap = 4, thick = 1.5f;
            float cr = 0.9f, cg = 0.9f, cb = 0.9f;
            if (m_aimOnAlly)   { cr = 0.25f; cg = 0.9f; cb = 0.3f; }
            if (m_aimOnTarget) { cr = 1.0f; cg = 0.25f; cb = 0.2f; }
            if (overheated)    { cr = 1.0f; cg = 0.55f; cb = 0.1f; }
            m_ui.rect(cx-thick, cy-arm-gap, thick*2, arm, cr, cg, cb);
            m_ui.rect(cx-thick, cy+gap,     thick*2, arm, cr, cg, cb);
            m_ui.rect(cx-arm-gap, cy-thick, arm, thick*2, cr, cg, cb);
            m_ui.rect(cx+gap,     cy-thick, arm, thick*2, cr, cg, cb);

            // ── Ruota di comando (doc 26, livello 2) ─────────────────────
            // 3 settori attorno al mirino: Advance (alto), Regroup (basso-sx),
            // Hold (basso-dx). Il settore puntato dal mouse è evidenziato; si
            // impartisce al rilascio del tasto. Ordine = indice: 0/1/2.
            if (m_wheelOpen)
            {
                m_ui.rect(0, 0, W, H, 0.0f, 0.0f, 0.0f, 0.35f);   // vela di fondo
                const char* wlabels[3] = { "REGROUP", "HOLD", "ADVANCE" };
                const float R = 120.0f, bw = 108.0f, bh = 40.0f;
                // Posizioni dei 3 settori (x,y del centro box)
                const float px[3] = { cx - R,        cx + R,        cx        };
                const float py[3] = { cy + R*0.55f,  cy + R*0.55f,  cy - R    };
                for (int i = 0; i < 3; ++i)
                {
                    const bool on = (m_wheelSel == i);
                    const float bx = px[i] - bw*0.5f, by = py[i] - bh*0.5f;
                    if (on) m_ui.rect(bx, by, bw, bh, 0.18f, 0.42f, 0.20f, 0.95f);
                    else    m_ui.rect(bx, by, bw, bh, 0.08f, 0.10f, 0.14f, 0.85f);
                    m_ui.border(bx, by, bw, bh, on ? 0.4f : 0.5f,
                                                on ? 0.95f : 0.5f, on ? 0.4f : 0.55f);
                    m_ui.textCentered(px[i], py[i] - 6.0f, 1.7f, wlabels[i],
                                      on ? 0.85f : 0.7f, on ? 1.0f : 0.7f, on ? 0.85f : 0.72f);
                }
                m_ui.textCentered(cx, cy + R + 34.0f, 1.4f,
                                  "muovi il mouse e rilascia per impartire",
                                  0.7f, 0.75f, 0.8f);
            }

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

        // ── Mappa top-down di selezione respawn (doc 30, Phase 1, BF2005) ──
        //    Mentre si è a terra: vista dall'alto della mappa con i punti di
        //    respawn cliccabili. Proiezione in `rmProj` (coerente col picking).
        if (state == 0 && m_respawnMap.active)
        {
            const RespawnMap& r = m_respawnMap;
            m_ui.rect(0, 0, W, H, 0.30f, 0.03f, 0.03f, 0.5f);   // vela "a terra"
            m_ui.textCentered(cx, 40.0f, 3.0f, "SEI A TERRA", 1.0f, 0.42f, 0.36f);

            const bool ready = (r.secondsLeft <= 0.05f);
            if (ready)
                m_ui.textCentered(cx, 84.0f, 1.6f,
                    "Clicca un punto sulla mappa o premi INVIO per schierarti",
                    0.55f, 1.0f, 0.6f);
            else
            {
                char cd[72];
                std::snprintf(cd, sizeof(cd),
                    "Scegli il punto — rientro disponibile fra %.0fs",
                    std::ceil(r.secondsLeft));
                m_ui.textCentered(cx, 84.0f, 1.5f, cd, 0.9f, 0.86f, 0.8f);
            }

            float px, py, pw, ph; rmPanel(m_ui.width(), m_ui.height(), px, py, pw, ph);
            m_ui.rect(px, py, pw, ph, 0.06f, 0.08f, 0.11f, 0.92f);   // sfondo mappa
            m_ui.border(px, py, pw, ph, 0.40f, 0.55f, 0.75f, 1.5f);
            const RMProj proj = rmProj(r, m_ui.width(), m_ui.height());

            // Pareti/geometria come rettangoli tenui (contesto, non cliccabili)
            for (const auto& w2 : r.walls)
            {
                float sx, sy;
                proj.to(w2.x - w2.sx * 0.5f, w2.z - w2.sz * 0.5f, sx, sy);
                m_ui.rect(sx, sy, w2.sx * proj.scale, w2.sz * proj.scale,
                          0.16f, 0.18f, 0.22f, 0.9f);
            }

            // Luogo di morte (orientamento)
            if (r.hasDeath)
            {
                float sx, sy; proj.to(r.deathX, r.deathZ, sx, sy);
                m_ui.rect(sx - 3, sy - 3, 6, 6, 0.72f, 0.72f, 0.74f, 0.95f);
                m_ui.textCentered(sx, sy + 7.0f, 1.0f, "caduto",
                                  0.72f, 0.72f, 0.74f);
            }

            const int hover = respawnMapPick(r.mouseX, r.mouseY);

            // Marker dei punti di respawn disponibili (verde alleato)
            for (int i = 0; i < (int)r.markers.size(); ++i)
            {
                float sx, sy; proj.to(r.markers[i].x, r.markers[i].z, sx, sy);
                const bool on = (i == r.sel);
                const bool hv = (i == hover);
                const float half = on ? 8.0f : 6.0f;
                const float cr = on ? 0.45f : 0.30f;
                const float cg = on ? 1.00f : 0.85f;
                const float cb = on ? 0.55f : 0.45f;
                m_ui.rect(sx - half, sy - half, half * 2, half * 2, cr, cg, cb);
                if (on || hv)
                    m_ui.border(sx - half - 2, sy - half - 2,
                                half * 2 + 4, half * 2 + 4, 0.9f, 1.0f, 0.9f, 1.5f);
                m_ui.textCentered(sx, sy - half - 15.0f, (on || hv) ? 1.3f : 1.0f,
                                  r.markers[i].label.c_str(),
                                  (on || hv) ? 0.9f : 0.70f,
                                  (on || hv) ? 1.0f : 0.85f,
                                  (on || hv) ? 0.9f : 0.72f);
            }

            m_ui.textCentered(cx, py + ph + 18.0f, 1.2f,
                              "Click sulla mappa  oppure  A/D per scegliere",
                              0.70f, 0.75f, 0.82f);
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
        if (state == 1 || state == 2)
        {
            const bool win = (state == 1);
            if (win) m_ui.rect(0, 0, W, H, 0,     0.22f, 0,     0.52f);
            else     m_ui.rect(0, 0, W, H, 0.32f, 0,     0,     0.52f);

            if (win)
                m_ui.text(cx - 215, cy - 130, 5.0f, "MISSIONE COMPLETATA!", 0.2f, 1.0f, 0.3f);
            else
                m_ui.text(cx - 115, cy - 130, 5.0f, "GAME OVER", 1.0f, 0.2f, 0.15f);

            // ── Debrief (doc 25 / GDD 9.6) ────────────────────────────
            // Il vecchio testo qui era CABLATO ("Tutti i nemici eliminati." /
            // "Sei stato eliminato.") ed era diventato falso: si vince anche
            // completando una missione e si perde anche fallendo un obiettivo.
            // Ora si raccontano i fatti veri. Nessun voto: è l'insieme dei
            // fattori a dire com'è andata.
            if (!m_debrief.empty())
            {
                float y = cy - 70;
                for (const auto& line : m_debrief)
                {
                    if (win) m_ui.text(cx - 180, y, 2.0f, line.c_str(), 0.85f, 0.97f, 0.85f);
                    else     m_ui.text(cx - 180, y, 2.0f, line.c_str(), 0.97f, 0.82f, 0.80f);
                    y += 26.0f;
                }
            }
            else if (win)
                m_ui.text(cx - 170, cy - 70, 2.3f, "Obiettivi raggiunti.", 0.8f, 0.95f, 0.8f);
            else
                m_ui.text(cx - 125, cy - 70, 2.3f, "Sei stato eliminato.", 0.95f, 0.75f, 0.75f);

            for (const auto& b : overlayButtons(m_ui.width(), m_ui.height(), state))
            {
                const bool hv = (m_mouseX >= b.x && m_mouseX <= b.x + b.w
                              && m_mouseY >= b.y && m_mouseY <= b.y + b.h);
                m_ui.rect(b.x, b.y, b.w, b.h, hv ? 0.16f : 0.09f,
                          hv ? 0.30f : 0.11f, hv ? 0.42f : 0.13f, hv ? 0.92f : 0.62f);
                m_ui.border(b.x, b.y, b.w, b.h, 0.4f, 0.5f, 0.55f);
                m_ui.textCentered(cx, b.y + 11, 2.0f, b.label,
                                  hv ? 1.0f : 0.82f, hv ? 1.0f : 0.88f, hv ? 0.85f : 0.82f);
            }
        }
    }

    m_ui.end();
}

} // namespace mini