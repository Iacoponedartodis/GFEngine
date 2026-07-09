#pragma once
#include "mini/render/Ui2D.hpp"
#include <string>
#include <vector>

namespace mini
{

class HUD
{
public:
    HUD(int screenW, int screenH);

    // Stato command post mostrato in alto (proprietario + progresso cattura)
    struct PostStatus
    {
        std::string label;
        int   owner         = 0;   // 0 neutrale, 1 alleati, 2 nemici
        int   capturingTeam = 0;
        float progress01    = 0.0f;
    };
    void setPosts(const std::vector<PostStatus>& posts) { m_posts = posts; }

    // Avanza i timer degli effetti (hitmarker, toast). Chiamare col dt reale.
    void tick(float dt);

    // Feedback di mira/colpo (impostati da Application ogni frame):
    void setAimOnTarget(bool on) { m_aimOnTarget = on; }   // mirino su hitbox
    void hitmarker(bool kill);                             // colpo a segno
    void toast(const std::string& msg, float seconds = 2.5f); // messaggio a schermo

    // state: -1=Paused, 0=Playing, 1=Win, 2=Lose
    void render(float playerHp, float playerMaxHp, int state,
                float weaponHeat = 0.0f, bool overheated = false,
                const char* weaponName = nullptr,
                int team1Tickets = -1, int team2Tickets = -1,
                int aliveAllies = 0, int aliveEnemies = 0);

private:
    Ui2D m_ui;

    bool        m_aimOnTarget = false;
    float       m_hitTimer    = 0.0f;
    bool        m_hitWasKill  = false;
    float       m_toastTimer  = 0.0f;
    std::string m_toast;
    std::vector<PostStatus> m_posts;
};

} // namespace mini
