#pragma once
#include "mini/game/data/DefinitionRegistry.hpp"
#include <string>
#include <filesystem>

namespace editor
{

class BalanceEditor
{
public:
    BalanceEditor();
    void draw();

private:
    mini::DefinitionRegistry m_registry;
    bool m_dirty = false;

    // Selezioni correnti per ogni tab
    std::string m_selWeapon;
    std::string m_selEnemy;
    std::string m_selAI;
    std::string m_selMap;
    std::string m_selAlly;
    std::string m_selPlayerDef;

    // Buffer persistenti (evita copia ogni frame)
    mini::EnemyDef  m_editAlly;
    mini::PlayerDef m_editPlayerDef;

    // ── Tab draw ─────────────────────────────────────────────────────
    void drawWeaponsTab();
    void drawEnemiesTab();
    void drawAITab();
    void drawMapsTab();
    void drawAlliesTab();
    void drawPlayerDefTab();

    // ── Salvataggio ──────────────────────────────────────────────────
    void saveWeapon   (const mini::WeaponDef&     w);
    void saveEnemy    (const mini::EnemyDef&      e);
    void saveAI       (const mini::AiProfileDef&  a);
    void saveMap      (const mini::MapDef&         m);
    void saveAlly     (const mini::EnemyDef&      e);
    void savePlayerDef(const mini::PlayerDef&     p);

    void reload();
};

} // namespace editor
