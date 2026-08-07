#pragma once
#include "viewport/FreeCameraViewport.hpp"
#include <string>
#include <vector>
#include <unordered_map>
#include "util/RigReader.hpp"
#include "framework/UndoStack.hpp"   // annullamento condiviso (doc 52 F2)
#include "mini/game/data/DefinitionRegistry.hpp"
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

    // Il CORPO dell'unita' (ADR-022). Niente moveSpeed (lo sovrascrive sempre il
    // profilo AI), niente damageScale (nessun consumatore), niente aiProfileId ne'
    // abilityIds: li decide la CLASSE e questo modulo non li edita piu'.
    std::string faction    = "neutral";
    float hp               = 80.0f;
    // ADR-022: la classe (professione) fornisce loadout+comportamento+abilita'
    // all'unita' che la referenzia. Vuota = valgono i campi legacy (additivo).
    std::string classId;
    std::string hitboxProfileId;
    // SOLA LETTURA: fallback quando non c'e' una classe + anteprima in mano.
    std::vector<std::string> weaponIds;

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
    // Lavoro non salvato (doc 52 F3). Prima lo teneva per sé: uscendo da GFEditor
    // con un'entità modificata le modifiche sparivano senza una domanda.
    [[nodiscard]] bool hasUnsavedChanges() const { return m_dirty; }
    [[nodiscard]] std::string unsavedWhat() const
    { return (m_sel >= 0 && m_sel < (int)m_entries.size())
             ? ("l'entita' \"" + m_entries[m_sel].id + "\"") : std::string("un'entita'"); }
    void savePending() { if (m_dirty) saveSelected(); }
    // Rilascia la cattura del mouse quando questo modulo cessa di essere attivo
    // (EditorApp lo chiama al cambio modulo): senza, il mouse resterebbe
    // invisibile e non liberabile. Vedi FreeCameraViewport::releaseMouseCapture.
    void releaseMouseCapture() { m_viewport.releaseMouseCapture(); }

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

    // ── ANNULLAMENTO (doc 52 F2) ─────────────────────────────────────────
    // Questo modulo NON aveva annullamento: si spostava una zona hitbox o un attach
    // point col gizmo e non c'era modo di tornare indietro. Non è stato scritto qui:
    // è la pila condivisa, la stessa del Map Editor e del tab strutture. Il costo
    // dell'adozione è questa dichiarazione più tre chiamate — che è esattamente la
    // promessa del framework di ADR-049.
    struct UndoState
    {
        std::unordered_map<std::string, AttachPointEntry> attach;
        std::vector<EntityEntry::InlineHitZone>           zones;
        std::string selAttach;
        int         selZone = -1;
        float       rotX = 0.0f, scale = 1.0f;
    };
    UndoStack<UndoState> m_undo;
    float m_clock = 0.0f;   // orologio del modulo, per la coalescenza dei gesti
    [[nodiscard]] UndoState snapshot() const
    { return { m_attachPoints, m_hitboxZones, m_selAttachPoint, m_selZone, m_rotX, m_scale }; }
    void applyUndoState(const UndoState& s);

    // ── Arma in mano (preview/posa) ──────────────────────────────────────
    std::string m_weaponId;                 // arma mostrata ("" = nessuna)
    std::string m_weaponMeshPath;           // mesh dell'arma (campo "assets/...")
    glm::vec3   m_weaponGrip   = {0,0,0};    // attach "right_hand" dell'arma
    // Correzione canonica dell'arma (mesh_rot_x/y del WeaponDef): l'anteprima
    // in mano DEVE applicarla come il runtime, altrimenti l'arma appare storta
    // in gioco rispetto all'editor.
    float       m_weaponBaseRotX = 0.0f;
    float       m_weaponBaseRotY = 0.0f;
    float       m_weaponScale  = 1.0f;
    glm::vec3   m_weaponRot    = {0,0,0};    // euler gradi
    glm::vec3   m_weaponOffset = {0,0,0};    // offset fine rispetto alla mano
    std::string m_weaponHandPoint = "right_hand"; // attach point del personaggio
    // KI #49: la posa (scala/rot/offset) viene dall'ARMA se autorata (hand_scale>0).
    // In quel caso è READ-ONLY qui (si edita nel Weapon Editor) e non va riscritta
    // sul weapon_display dell'entità. false = arma legacy senza posa → si edita qui.
    bool        m_weaponPoseFromWeapon = false;

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

    // ID disponibili per i dropdown. Niente liste per armi/AI/abilità dell'unità:
    // le decide la CLASSE e non si editano più qui (ADR-022).
    std::vector<std::string> m_availableClasses;   // ADR-022: professioni assegnabili
    std::vector<std::string> m_availableHitboxes;

    // Registry di sola lettura: serve a risolvere l'arma della CLASSE con
    // `classres`, la stessa funzione del runtime, invece di riscrivere la regola
    // nell'editor (è la duplicazione che ha fatto divergere runtime e display —
    // stesso principio di R4 sul VehicleEditor e di ADR-018).
    mini::DefinitionRegistry m_registry;

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
