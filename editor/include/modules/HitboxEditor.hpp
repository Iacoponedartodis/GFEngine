#pragma once
#include "mini/game/data/DefinitionRegistry.hpp"
#include "mini/ecs/components/HitboxComponent.hpp"
#include "viewport/FreeCameraViewport.hpp"
#include "util/RigReader.hpp"
#include <string>
#include <vector>

namespace editor
{

class HitboxEditor
{
public:
    HitboxEditor();
    void draw();
    void tick(float dt);

private:
    mini::DefinitionRegistry m_registry;
    std::string              m_selProfile;
    int                      m_selZone = -1;

    // Copia modificabile del profilo selezionato
    mini::HitboxProfile      m_edit;
    bool                     m_dirty = false;

    // ── Viewport 3D: modello + ossa + hitbox sovrapposte ─────────────────
    FreeCameraViewport       m_viewport;
    std::string              m_modelPath;     // mesh corrente (campo JSON)
    std::vector<JointData>   m_joints;
    bool                     m_show2DViews = false;

    void drawProfileList();
    void drawZoneList();
    void drawZoneProperties();
    void drawVisualPreview();
    void drawViewport();

    // Carica un modello per l'anteprima (mesh path relativo "assets/...").
    void loadModelForProfile();
    void setModel(const std::string& meshField);
    // Sincronizza le hitbox + ossa nella viewport 3D.
    void syncViewport();

    void saveProfile(const mini::HitboxProfile& p);
    void reload();

    // Aggiunge zona con valori default
    void addZone();
    void removeZone(int idx);
    void duplicateZone(int idx);
};

} // namespace editor

