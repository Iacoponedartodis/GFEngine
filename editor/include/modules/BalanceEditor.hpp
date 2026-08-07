#pragma once
#include "mini/game/data/DefinitionRegistry.hpp"
#include "mini/game/data/GameplayBalance.hpp"   // ADR-043: bilanciamento globale
#include <string>
#include <filesystem>

namespace editor
{

class BalanceEditor
{
public:
    BalanceEditor();
    void draw();
    // Lavoro non salvato (doc 52 F3). Qui NON si offre il salvataggio automatico:
    // il Balance Editor scrive per singola definizione (arma, profilo, abilità) e
    // non ha un "salva tutto" — offrirlo salverebbe una parte e sembrerebbe tutto.
    // Meglio avvisare e lasciar tornare indietro.
    [[nodiscard]] bool hasUnsavedChanges() const { return m_dirty; }
    [[nodiscard]] std::string unsavedWhat() const { return "valori di bilanciamento"; }

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

    // Bilanciamento globale data-driven (ADR-043): rianimazione + rete di
    // comunicazione. Vive in data/config/gameplay.json, non più in constexpr.
    mini::GameplayBalance m_gameplay;

    // ── Tab draw ─────────────────────────────────────────────────────
    void drawWeaponsTab();
    void drawAITab();
    void drawMapsTab();
    void drawPlayerDefTab();
    void drawAbilitiesTab();
    void drawGameplayTab();
    void drawCommandoTab();   // ADR-041 §4 / ADR-044: CommanderDef + rete comunicazione

    // ── Salvataggio ──────────────────────────────────────────────────
    void saveWeapon   (const mini::WeaponDef&     w);
    void saveAI       (const mini::AiProfileDef&  a);
    void saveMap      (const mini::MapDef&         m);
    void savePlayerDef(const mini::PlayerDef&     p);
    void saveAbility  (const mini::AbilityDef&    a);
    void saveGameplay ();
    void saveCommander(const mini::CommanderDef& c);
    std::string m_selCommander;

    void reload();
};

} // namespace editor
