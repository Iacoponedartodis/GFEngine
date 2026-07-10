#pragma once
#include "viewport/FreeCameraViewport.hpp"
#include "mini/game/data/Definitions.hpp"
#include <string>
#include <vector>

namespace editor
{

// ── VehicleEditor (19_Vehicles) ─────────────────────────────────────────
// Modulo dedicato ai veicoli: lista/creazione/rinomina, statistiche,
// modello 3D con anteprima nel viewport e box di collisione visualizzato.
// Hitbox a zone e attach point per i veicoli: Fase B (come EntityEditor).
class VehicleEditor
{
public:
    VehicleEditor();
    ~VehicleEditor() = default;

    void tick(float dt);
    void draw();

private:
    struct VehicleEntry
    {
        std::string id;
        std::string jsonPath;
        mini::VehicleDef def;
    };

    std::vector<VehicleEntry> m_entries;
    int  m_sel   = -1;
    bool m_dirty = false;
    std::string m_pendingSelectId;   // riselezione post-rinomina/creazione

    FreeCameraViewport m_viewport;
    float m_listW = 190.0f;

    void loadEntries();
    void selectEntry(int idx);
    void saveSelected();
    void updateViewport();

    void drawList();
    void drawProperties();
};

} // namespace editor
