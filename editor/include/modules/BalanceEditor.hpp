#pragma once
#include "mini/game/data/DefinitionRegistry.hpp"
#include <string>
#include <filesystem>

namespace editor
{

// Modifica parametri armi, nemici e AI direttamente come JSON.
// I file vengono salvati in data/weapons/, data/enemies/, data/ai/.
class BalanceEditor
{
public:
    BalanceEditor();

    void draw(); // pannello ImGui principale

private:
    // ── Stato ────────────────────────────────────────────────────────
    enum class Tab { Weapons, Enemies, AI };
    Tab m_tab = Tab::Weapons;

    mini::DefinitionRegistry m_registry;
    bool m_dirty = false; // modifiche non salvate

    // ID selezionato in ogni tab
    std::string m_selWeapon;
    std::string m_selEnemy;
    std::string m_selAI;

    // ── Helper ────────────────────────────────────────────────────────
    void drawWeaponsTab();
    void drawEnemiesTab();
    void drawAITab();

    void saveWeapon(const mini::WeaponDef& w);
    void saveEnemy(const mini::EnemyDef& e);
    void saveAI(const mini::AiProfileDef& a);

    void reload();
};

} // namespace editor