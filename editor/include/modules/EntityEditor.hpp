#pragma once
#include "viewport/FreeCameraViewport.hpp"
#include <string>
#include <vector>
#include <unordered_map>
#include "util/RigReader.hpp"
#include <glm/glm.hpp>

namespace editor
{

// Attach point: posizione 3D nel model space
struct AttachPointEntry { float x=0, y=0, z=0; };

// Dati minimi per visualizzare/editare un'entità (nemico o alleato)
struct EntityEntry
{
    std::string id;
    std::string name;
    std::string jsonPath;
    std::string meshPath;
    float meshRotX  = 0.0f;
    float meshRotY  = 0.0f;
    float meshScale = 1.0f;
    bool  isAlly    = false;

    // Statistiche combat
    std::string faction    = "neutral";
    float hp               = 80.0f;
    float moveSpeed        = 4.0f;
    float damageScale      = 1.0f;
    std::string aiProfileId;
    std::string hitboxProfileId;
    std::vector<std::string> weaponIds;
    std::vector<std::string> abilityIds;

    // Attach points: "foot", "eye", "right_hand", ...
    std::unordered_map<std::string, AttachPointEntry> attachPoints;

    // Inline hitbox zones
    struct InlineHitZone {
        std::string name       = "zona";
        glm::vec3   offset     = {0,0,0};
        glm::vec3   halfExt    = {0.2f,0.3f,0.2f};
        float       damageMult = 1.0f;
        std::string boneName   = "";
        glm::vec3   eulerDeg   = {0,0,0};
        bool        debugVisible = true;   // flag del profilo runtime
    };
    std::vector<InlineHitZone> hitboxZones;

    // Arma mostrata in mano (posa, solo visuale editor/futuro render)
    std::string dispWeaponId;
    float       dispWeaponScale  = 1.0f;
    glm::vec3   dispWeaponRot     = {0,0,0};
    glm::vec3   dispWeaponOffset  = {0,0,0};
    std::string dispWeaponHand    = "right_hand";

    float footY() const {
        auto it = attachPoints.find("foot");
        return it != attachPoints.end() ? it->second.y : 0.0f;
    }
};

class EntityEditor
{
public:
    EntityEditor();
    void draw();
    void tick(float dt);

private:
    std::vector<EntityEntry> m_entries;
    int                      m_sel = -1;

    FreeCameraViewport m_viewport;

    // Valori correnti (slider live)
    float m_rotX    = 0.0f;
    float m_rotY    = 0.0f;
    float m_scale   = 1.0f;
    bool  m_dirty   = false;

    // Attach points dell'entità selezionata
    std::unordered_map<std::string, AttachPointEntry> m_attachPoints;
    std::string m_selAttachPoint;

    // Bone data per selezione corrente
    std::vector<JointData> m_joints;
    std::string            m_selBoneName;  // bone selezionato nella viewport

    // Joint names (solo nomi, per backward compat)
    std::vector<std::string> m_rigJoints;

    // Hitbox editing state
    std::vector<EntityEntry::InlineHitZone> m_hitboxZones;
    int m_selZone = -1;

    // ── Arma in mano (preview/posa) ──────────────────────────────────────
    std::string m_weaponId;                 // arma mostrata ("" = nessuna)
    std::string m_weaponMeshPath;           // mesh dell'arma (campo "assets/...")
    glm::vec3   m_weaponGrip   = {0,0,0};    // attach "right_hand" dell'arma
    float       m_weaponScale  = 1.0f;
    glm::vec3   m_weaponRot    = {0,0,0};    // euler gradi
    glm::vec3   m_weaponOffset = {0,0,0};    // offset fine rispetto alla mano
    std::string m_weaponHandPoint = "right_hand"; // attach point del personaggio

    void loadWeaponPreview();   // legge il JSON arma + carica mesh
    void updateWeaponTransform(); // ricalcola e applica la trasformazione

    // ── Trasformazione personaggio (coerente con marker/modello) ─────────
    // I punti (attach point, zone hitbox) sono in model space; il viewport
    // lavora in world space: M = rotX * scala uniforme.
    glm::mat4 charTransform() const;
    glm::vec3 toWorld(const glm::vec3& modelPos) const;
    glm::vec3 deltaToLocal(const glm::vec3& worldDelta) const;

    // Resizable panel widths
    float m_listW   = 180.0f;
    float m_centerW = 280.0f;

    // Gizmo target
    std::string m_gizmoTarget;

    // Rinomina (ADR-010): reload deferito al frame successivo
    std::string m_pendingSelectId;

    // ID disponibili per i dropdown delle statistiche
    std::vector<std::string> m_availableWeapons;
    std::vector<std::string> m_availableAI;
    std::vector<std::string> m_availableHitboxes;
    std::vector<std::string> m_availableAbilities;

    void loadEntries();
    void selectEntry(int idx);
    // Carica m_hitboxZones dal profilo indicato (vuoto se il file non esiste).
    void loadZonesFromProfile(const std::string& profileId);
    void saveSelected();
    void reloadPreview();
    void updateMarker();
    void loadRigJoints();
    void loadBones();
    void loadAvailableIds();
    void drawStatsPanel();
    void drawHitboxTab();
    void syncViewportMarkers();
};

} // namespace editor
