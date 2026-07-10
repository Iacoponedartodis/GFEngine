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
    std::string m_selAI;
    std::string m_selMap;
    std::string m_selPlayerDef;
    std::string m_selAbility;

    // Buffer persistenti (evita copia ogni frame)
    mini::EnemyDef  m_editAlly;
    mini::PlayerDef m_editPlayerDef;

    // ── Tab draw ─────────────────────────────────────────────────────
    void drawWeaponsTab();
    void drawAITab();
    void drawMapsTab();
    void drawPlayerDefTab();
    void drawAbilitiesTab();

    // ── Salvataggio ──────────────────────────────────────────────────
    void saveWeapon   (const mini::WeaponDef&     w);
    void saveAI       (const mini::AiProfileDef&  a);
    void saveMap      (const mini::MapDef&         m);
    void savePlayerDef(const mini::PlayerDef&     p);
    void saveAbility  (const mini::AbilityDef&    a);

    void reload();
};

} // namespace editor
