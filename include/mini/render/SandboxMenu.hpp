#pragma once
#include "mini/render/Ui2D.hpp"
#include <string>
#include <utility>
#include <vector>

namespace mini
{

// Menu in-game della sandbox (17_SandboxTools). Overlay a due pagine:
//   Armi        — lista completa dal registry, slot primaria/secondaria.
//   Simulazione — battaglia AI-vs-AI completamente personalizzabile
//                 (modalità, truppe, ticket, respawn) con osservatore in volo.
// Per una PARTITA vera si usa la scorciatoia P → PreMatch classico
// (gestita da Application, non da questo menu).
// Navigazione: TAB apre/chiude, Q/E cambia pagina, SU/GIU seleziona,
// SIN/DES modifica i valori, INVIO attiva.
class SandboxMenu
{
public:
    SandboxMenu(int screenW, int screenH);

    enum class Result { None, Close, EquipWeapon, ToggleSim, RestartSandbox };

    void setWeapons(const std::vector<std::pair<std::string, std::string>>& idName);
    void setMaps   (const std::vector<std::pair<std::string, std::string>>& idName);
    [[nodiscard]] const std::string& selectedWeaponId() const;
    [[nodiscard]] const std::string& selectedMapId() const;
    [[nodiscard]] int weaponSlot() const { return m_weaponSlot; } // 0 primaria, 1 secondaria

    // Parametri della simulazione (letti da Application in ToggleSim)
    int   simModeIndex = 0;      // 0 Conquista, 1 Assalto, 2 Difesa
    int   allyCount    = 4;
    int   enemyCount   = 6;
    int   team1Tickets = 10;
    int   team2Tickets = 10;
    float respawnDelay = 4.0f;
    bool  simRunning   = false;

    Result handleKey(int sdlScancode);
    void   render() const;

private:
    Ui2D m_ui;
    int  m_page       = 0;  // 0 Armi, 1 Simulazione
    int  m_weaponSel  = 0;
    int  m_weaponSlot = 0;  // 0 primaria, 1 secondaria (SIN/DES)
    int  m_simSel     = 0;  // riga selezionata in Simulazione
    int  m_mapSel     = 0;  // mappa scelta (sim E riavvio sandbox)
    std::vector<std::pair<std::string, std::string>> m_weapons; // id, nome
    std::vector<std::pair<std::string, std::string>> m_maps;    // id, nome

    static constexpr int k_visibleWeapons = 14;
};

} // namespace mini
