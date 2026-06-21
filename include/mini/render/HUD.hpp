#pragma once
#include "mini/render/Ui2D.hpp"

namespace mini
{

class HUD
{
public:
    HUD(int screenW, int screenH);

    // state: -1=Paused, 0=Playing, 1=Win, 2=Lose
    void render(float playerHp, float playerMaxHp, int state,
                float weaponHeat = 0.0f, bool overheated = false,
                const char* weaponName = nullptr,
                int team1Tickets = -1, int team2Tickets = -1,
                int aliveAllies = 0, int aliveEnemies = 0);

private:
    Ui2D m_ui;
};

} // namespace mini