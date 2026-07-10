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

    // ── Log chat (17_SandboxTools): eventi di gioco leggibili ─────────
    void pushFeed(const std::string& msg);   // accoda un evento
    void toggleFeed() { m_feedOpen = !m_feedOpen; m_feedScroll = 0; } // tasto L
    [[nodiscard]] bool feedOpen() const { return m_feedOpen; }
    // Scroll dello storico (PAGSU/PAGGIU): +su verso i messaggi vecchi
    void scrollFeed(int lines);

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

    // Log chat: storico (cap fisso) + timer di visibilità delle righe recenti
    struct FeedLine { std::string text; float age = 0.0f; };
    std::vector<FeedLine> m_feed;      // più recente in coda
    bool  m_feedOpen   = false;
    int   m_feedScroll = 0;   // righe di offset dal fondo (0 = più recenti)
    static constexpr int   k_feedMax     = 200;  // storico conservato
    static constexpr int   k_feedRecent  = 4;    // righe sempre visibili
    static constexpr int   k_feedPanel   = 18;   // righe nel pannello aperto
    static constexpr float k_feedFadeSec = 6.0f; // durata righe recenti
};

} // namespace mini
