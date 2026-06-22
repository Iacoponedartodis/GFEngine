#pragma once
#include "mini/game/data/DefinitionRegistry.hpp"
#include "mini/ecs/components/HitboxComponent.hpp"
#include <string>
#include <vector>

namespace editor
{

class HitboxEditor
{
public:
    HitboxEditor();
    void draw();

private:
    mini::DefinitionRegistry m_registry;
    std::string              m_selProfile;
    int                      m_selZone = -1;

    // Copia modificabile del profilo selezionato
    mini::HitboxProfile      m_edit;
    bool                     m_dirty = false;

    void drawProfileList();
    void drawZoneList();
    void drawZoneProperties();
    void drawVisualPreview();

    void saveProfile(const mini::HitboxProfile& p);
    void reload();

    // Aggiunge zona con valori default
    void addZone();
    void removeZone(int idx);
    void duplicateZone(int idx);
};

} // namespace editor