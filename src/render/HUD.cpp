#include "mini/render/HUD.hpp"
#include "mini/platform/OpenGL.hpp"

#include <stb_easy_font.h>
#include <cstdio>
#include <cmath>

namespace mini
{

HUD::HUD(int screenW, int screenH) : m_w(screenW), m_h(screenH) {}

void HUD::begin2D()
{
    glUseProgram(0);
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glMatrixMode(GL_PROJECTION); glPushMatrix(); glLoadIdentity();
    glOrtho(0.0, m_w, m_h, 0.0, -1.0, 1.0);
    glMatrixMode(GL_MODELVIEW);  glPushMatrix(); glLoadIdentity();
}

void HUD::end2D()
{
    glMatrixMode(GL_MODELVIEW);  glPopMatrix();
    glMatrixMode(GL_PROJECTION); glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
}

void HUD::drawRect(float x, float y, float w, float h,
                   float r, float g, float b, float a)
{
    glColor4f(r, g, b, a);
    glBegin(GL_QUADS);
    glVertex2f(x, y);     glVertex2f(x+w, y);
    glVertex2f(x+w, y+h); glVertex2f(x, y+h);
    glEnd();
}

void HUD::drawText(float x, float y, float scale,
                   const char* text, float r, float g, float b)
{
    static char buf[131072];
    int quads = stb_easy_font_print(0, 0, const_cast<char*>(text), nullptr, buf, sizeof(buf));
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

void HUD::render(float playerHp, float playerMaxHp, int state,
                 float weaponHeat, bool overheated, const char* weaponName,
                 int team1Tickets, int team2Tickets)
{
    begin2D();

    const float W = (float)m_w, H = (float)m_h;
    const float cx = W * 0.5f, cy = H * 0.5f;

    if (state == -1) // FreeRoam
    {
        // Banda istruzioni — due righe
        drawRect(0, H - 62, W, 62, 0, 0, 0, 0.65f);
        drawText(cx - 220, H - 54, 1.9f,
                 "ENTER = avvia partita   |   O = regole di gioco e preset   |   ESC = esci",
                 0.85f, 0.85f, 0.85f);

        if (weaponName)
        {
            char selBuf[128];
            std::snprintf(selBuf, sizeof(selBuf),
                "Arma: %s   |   1=Rifle  2=Pistol  3=Heavy  4=Sniper", weaponName);
            drawText(cx - 230, H - 27, 1.8f, selBuf, 0.6f, 0.8f, 1.0f);
        }
    }
    else if (state == -2) // Paused
    {
        drawRect(0, 0, W, H, 0, 0, 0, 0.65f);
        drawText(cx - 75,  cy - 90, 4.5f, "PAUSA",              0.95f, 0.95f, 0.95f);
        drawText(cx - 105, cy - 20, 2.3f, "ESC  =  Riprendi",       0.75f, 0.85f, 0.95f);
        drawText(cx - 105, cy + 12, 2.3f, "R    =  Riavvia partita", 0.75f, 0.85f, 0.95f);
        drawText(cx - 105, cy + 44, 2.3f, "F    =  Volo libero",     0.75f, 0.85f, 0.95f);
        drawText(cx - 105, cy + 76, 2.3f, "Q    =  Esci dal gioco",  0.75f, 0.85f, 0.95f);
    }
    else // Playing / Win / Lose
    {
        const float PAD = 14;

        // ── Barra HP (basso sinistra) ────────────────────────────────
        const float BW = 220, BH = 18;
        const float BY = H - PAD - BH;
        drawRect(PAD, BY, BW, BH, 0.15f, 0.08f, 0.08f);

        float pct = (playerMaxHp > 0) ? playerHp / playerMaxHp : 0;
        if (pct < 0) pct = 0; if (pct > 1) pct = 1;
        float fr = (pct < 0.5f) ? 1.0f : 2.0f - pct * 2;
        float fg = (pct > 0.5f) ? 1.0f : pct * 2;
        drawRect(PAD, BY, BW * pct, BH, fr, fg, 0.05f);

        drawRect(PAD - 1, BY - 1, BW + 2, 1,      0.7f, 0.7f, 0.7f);
        drawRect(PAD - 1, BY + BH, BW + 2, 1,     0.7f, 0.7f, 0.7f);
        drawRect(PAD - 1, BY - 1, 1, BH + 2,      0.7f, 0.7f, 0.7f);
        drawRect(PAD + BW, BY - 1, 1, BH + 2,     0.7f, 0.7f, 0.7f);

        char hpBuf[32];
        std::snprintf(hpBuf, sizeof(hpBuf), "HP  %d / %d",
                      (int)(playerHp > 0 ? playerHp : 0), (int)playerMaxHp);
        drawText(PAD, BY - 16, 1.6f, hpBuf, 0.9f, 0.9f, 0.9f);

        // ── Ticket display (alto centro) ─────────────────────────────
        if (state == 0 && team1Tickets >= 0)
        {
            char tBuf[64];
            std::snprintf(tBuf, sizeof(tBuf), "ALLEATI %d  |  NEMICI %d",
                          team1Tickets, team2Tickets);
            drawRect(cx - 120, 8, 240, 22, 0, 0, 0, 0.55f);
            drawText(cx - 110, 12, 1.7f, tBuf, 0.85f, 0.85f, 0.85f);
        }

        // ── Barra calore (basso destra) ──────────────────────────────
        if (state == 0)
        {
            const float HBW = 180, HBH = 14;
            const float HBX = W - PAD - HBW;
            const float HBY = H - PAD - HBH;

            drawRect(HBX, HBY, HBW, HBH, 0.10f, 0.08f, 0.08f);

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
            drawRect(HBX, HBY, HBW * weaponHeat, HBH, hr, hg, hb);

            drawRect(HBX - 1, HBY - 1, HBW + 2, 1,     0.5f, 0.5f, 0.5f);
            drawRect(HBX - 1, HBY + HBH, HBW + 2, 1,   0.5f, 0.5f, 0.5f);
            drawRect(HBX - 1, HBY - 1, 1, HBH + 2,     0.5f, 0.5f, 0.5f);
            drawRect(HBX + HBW, HBY - 1, 1, HBH + 2,   0.5f, 0.5f, 0.5f);

            if (weaponName)
            {
                char wBuf[64];
                if (overheated)
                    std::snprintf(wBuf, sizeof(wBuf), "%s  [OVERHEAT]", weaponName);
                else
                    std::snprintf(wBuf, sizeof(wBuf), "%s  %d%%", weaponName, (int)(weaponHeat * 100));
                drawText(HBX, HBY - 16, 1.5f, wBuf,
                         overheated ? 1.0f : 0.85f,
                         overheated ? 0.3f : 0.85f,
                         overheated ? 0.2f : 0.85f);
            }

            // ── Crosshair ────────────────────────────────────────────
            const float arm = 10, gap = 4, thick = 1.5f;
            float cr = 0.9f, cg = 0.9f, cb = 0.9f;
            if (overheated) { cr = 1.0f; cg = 0.3f; cb = 0.2f; }
            drawRect(cx - thick, cy - arm - gap, thick * 2, arm, cr, cg, cb);
            drawRect(cx - thick, cy + gap,       thick * 2, arm, cr, cg, cb);
            drawRect(cx - arm - gap, cy - thick, arm, thick * 2, cr, cg, cb);
            drawRect(cx + gap,       cy - thick, arm, thick * 2, cr, cg, cb);
        }

        // ── Overlay Win/Lose ─────────────────────────────────────────
        if (state == 1)
        {
            drawRect(0, 0, W, H, 0, 0.22f, 0, 0.52f);
            drawText(cx - 215, cy - 55, 5.0f, "MISSIONE COMPLETATA!", 0.2f, 1.0f, 0.3f);
            drawText(cx - 170, cy + 15, 2.3f, "Tutti i nemici eliminati.", 0.8f, 0.95f, 0.8f);
            drawText(cx - 155, cy + 50, 2.3f, "R = ricomincia  |  F = volo libero", 0.7f, 0.9f, 0.7f);
        }
        else if (state == 2)
        {
            drawRect(0, 0, W, H, 0.32f, 0, 0, 0.52f);
            drawText(cx - 115, cy - 55, 5.0f, "GAME OVER", 1.0f, 0.2f, 0.15f);
            drawText(cx - 125, cy + 15, 2.3f, "Sei stato eliminato.", 0.95f, 0.75f, 0.75f);
            drawText(cx - 155, cy + 50, 2.3f, "R = ricomincia  |  F = volo libero", 0.9f, 0.6f, 0.6f);
        }
    }

    end2D();
}

} // namespace mini