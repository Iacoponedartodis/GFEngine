#pragma once
#include "viewport/FreeCameraViewport.hpp"
#include "util/RigReader.hpp"
#include "mini/game/data/DefinitionRegistry.hpp"   // anteprima in mano: entità + attach point
#include <string>
#include <vector>
#include <array>
#include <unordered_map>

namespace editor
{

class WeaponEditor
{
public:
    WeaponEditor();
    ~WeaponEditor() = default;

    void tick(float dt);
    void draw();
    // Rilascio cattura mouse al cambio modulo (FreeCameraViewport::releaseMouseCapture).
    void releaseMouseCapture() { m_viewport.releaseMouseCapture(); }

private:
    struct AttachPointEntry { float x=0, y=0, z=0; };

    struct WeaponEntry {
        std::string id;
        std::string name;
        std::string jsonPath;
        std::string meshPath;
        std::string projectileMeshPath;

        // Trasformazione mesh
        float meshRotX  = -90.0f;
        float meshRotY  = 0.0f;
        float meshScale =   0.8f;

        // Attach points (model space)
        std::unordered_map<std::string, AttachPointEntry> attachPoints;

        // Statistiche
        float damage         = 25.f;
        float fireRate       = 4.5f;
        float bulletSpeed    = 25.f;
        float bulletLifetime = 3.0f;
        float bulletScale    = 0.12f;
        std::array<float,3> bulletColor = {0.3f, 0.65f, 1.0f};
        float heatPerShot    = 0.12f;
        float cooldownRate   = 0.30f;
        float overheatPenalty= 2.0f;
        float effectiveRange = 20.f;
        float minRange       = 0.0f;
        float adsFov         = 35.f;   // FOV in mira (basso = più zoom)
        float spreadBase     = 0.02f;
        float spreadAds      = 0.005f;
        float spreadMove     = 0.06f;
        float spreadSprint   = 0.14f;
        float spreadJump     = 0.20f;
        std::string faction  = "neutral";

        // Posa in mano (KI #49): come l'arma sta impugnata, indipendente da chi.
        // handScale <= 0 = non autorata (le unità usano il weapon_display legacy).
        float handScale = 0.0f;
        std::array<float,3> handRot    = {0.0f, 0.0f, 0.0f};
        std::array<float,3> handOffset = {0.0f, 0.0f, 0.0f};
    };

    std::vector<WeaponEntry> m_weapons;
    int   m_sel   = -1;
    bool  m_dirty = false;

    FreeCameraViewport m_viewport;

    // Valori slider live (separati dall'entry per aggiornare il preview al volo)
    float m_rotX  = -90.0f;
    float m_rotY  = 0.0f;
    float m_scale =   0.8f;

    // Vista corrente: false = arma, true = proiettile
    bool m_showProjectileMesh = false;

    // ── Anteprima IN MANO (2026-08-02) ────────────────────────────────────
    // La posa in mano si tarava alla cieca: si scriveva `hand_scale` qui e la si
    // guardava nell'Entity Editor, o peggio avviando una partita. Con la scala
    // che compensa la dimensione NATIVA del mesh — 0.0015 per l'E-5C, 80 per lo
    // Z-6 — indovinarla senza vederla non è realistico: il DC-15X è rimasto
    // senza `hand_scale` e in partita appariva minuscolo. Qui si carica un
    // PERSONAGGIO come modello principale e l'arma come attachment, con la
    // stessa formula del runtime (`weaponattach::handLocal`): si trascina lo
    // slider e si vede.
    bool m_handPreview = false;
    int  m_handUnit    = 0;                 // indice in m_handUnits
    std::vector<std::string> m_handUnits;   // entità con un mesh (alleati + nemici)
    // Serve solo all'anteprima: mesh, scala e attach point del personaggio.
    // Ricaricato all'accensione dell'anteprima, non tenuto in sync di continuo.
    mini::DefinitionRegistry m_handRegistry;
    void refreshHandUnits();
    void updateHandPreview();

    // Attach points in editing
    std::unordered_map<std::string, AttachPointEntry> m_attachPoints;
    std::string m_selAttachPoint;

    // Ossa del rig (da tinygltf)
    std::vector<std::string> m_rigJoints;

    void loadWeapons();
    void selectWeapon(int idx);
    void saveSelected();
    void reloadPreview();
    bool browseForMesh(std::string& outPath);
    void loadRigJoints();

    // Marker + gizmo per gli attach point nel viewport 3D.
    void syncViewportMarkers();
    glm::mat4 weaponTransform() const;                    // rotX * scala (come loadModel)
    glm::vec3 toWorld(const glm::vec3& modelPos) const;
    glm::vec3 deltaToLocal(const glm::vec3& worldDelta) const;

    void drawList(float w);
    void drawMeshTab(float w);
    void drawStatsTab(float w);
    void drawViewport(float w);
};

} // namespace editor
