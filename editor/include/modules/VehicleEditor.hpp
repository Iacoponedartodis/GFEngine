#pragma once
#include "viewport/FreeCameraViewport.hpp"
#include "framework/ModuleShell.hpp"   // layout condiviso (ADR-049)
#include "framework/AssetBrowser.hpp"  // ciclo di vita asset (ADR-049 R1)
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
    // Rilascio cattura mouse al cambio modulo (FreeCameraViewport::releaseMouseCapture).
    void releaseMouseCapture() { m_viewport.releaseMouseCapture(); }
    // Lavoro non salvato (doc 52 F3).
    [[nodiscard]] bool hasUnsavedChanges() const { return m_dirty; }
    [[nodiscard]] std::string unsavedWhat() const { return "un veicolo"; }
    void savePending() { if (m_dirty) saveSelected(); }

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
    editor::ModuleShell m_shell{190.0f, 300.0f};   // ADR-049
    editor::AssetBrowser m_browser;                // crea/duplica/rinomina/elimina

    void loadEntries();
    void selectEntry(int idx);
    void syncFromBrowser();   // allinea la selezione locale a quella del browser (per ID)
    void saveSelected();
    void updateViewport();

    void drawList();
    void drawProperties();
};

} // namespace editor
