#pragma once

namespace mini
{

class HUD
{
public:
    HUD(int screenW, int screenH);

    // state: -2=Paused, -1=FreeRoam, 0=Playing, 1=Win, 2=Lose
    void render(float playerHp, float playerMaxHp, int state,
                float weaponHeat = 0.0f, bool overheated = false,
                const char* weaponName = nullptr,
                int team1Tickets = -1, int team2Tickets = -1,
                int aliveAllies = 0, int aliveEnemies = 0);

private:
    int m_w, m_h;
    void begin2D();
    void end2D();
    void drawRect(float x, float y, float w, float h,
                  float r, float g, float b, float a = 1.0f);
    void drawText(float x, float y, float scale,
                  const char* text, float r, float g, float b);
};

} // namespace mini