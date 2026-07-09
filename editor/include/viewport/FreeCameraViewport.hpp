#pragma once
#include "mini/ecs/components/HitboxComponent.hpp"
#include "util/RigReader.hpp"
#include <imgui.h>
#include <memory>
#include <string>
#include <vector>
#include <glm/glm.hpp>

namespace mini { class Camera; class Shader; }

namespace editor
{

class FreeCameraViewport
{
public:
    FreeCameraViewport();
    ~FreeCameraViewport();

    void tick(float dt);
    // showLoadBar: se true mostra una barra "Sfoglia modello..." sopra il viewport
    void draw(bool showLoadBar = false);

    void loadModel(const std::string& path, float meshRotX = 0.0f, float meshScale = 1.0f);
    void clearModel();
    void setHitboxes(const std::vector<mini::HitZone>& zones, int selZone);

    // Modello "attaccato" (es. arma in mano) disegnato con una trasformazione
    // world arbitraria sopra il modello principale.
    void setAttachmentModel(const std::string& path, const glm::mat4& transform);
    void clearAttachmentModel();

    // Box della mappa da visualizzare come wireframe colorato.
    struct MapBoxDraw {
        float x, y, z, ry;
        float sx, sy, sz;
        float r, g, b;
        bool selected = false;
    };
    void setMapBoxes(const std::vector<MapBoxDraw>& boxes);
    void clearMapBoxes();

    // Marker visivo per un attach point
    void setFootMarker(float y, bool show);

    // Ritorna true se il sistema GL è operativo
    bool isReady() const;

    // ── Bones ────────────────────────────────────────────────────────────
    void setBoneData(const std::vector<JointData>& joints, float meshRotX, float meshScale);
    void clearBones();

    // ── Markers (world-space, pre-transformed by caller) ─────────────────
    struct ViewportMarker {
        std::string name;
        glm::vec3   pos        = {0,0,0};
        float r=1, g=1, b=0;
        bool  selected = false;
    };
    void setMarkers(const std::vector<ViewportMarker>& markers);
    void clearMarkers();

    // ── Bone selection ───────────────────────────────────────────────────
    void        setSelectedBone(const std::string& name);
    std::string getSelectedBone() const { return m_selBone; }

    // ── Gizmo ────────────────────────────────────────────────────────────
    // Tre modalità stile DCC: Sposta (frecce), Ruota (anelli), Scala (maniglie
    // quadrate + quadrato centrale per scala uniforme). Scorciatoie 1/2/3 con
    // mouse sul viewport (non in cattura).
    enum class GizmoMode { Translate, Rotate, Scale };

    void      setGizmoTarget(glm::vec3 pos, bool enabled);
    void      setGizmoMode(GizmoMode m)      { m_gizmoMode = m; }
    GizmoMode getGizmoMode() const           { return m_gizmoMode; }
    // Limita gli anelli di rotazione disponibili (es. solo Y per i box mappa).
    void setGizmoRotAxes(bool x, bool y, bool z)
    { m_gizmoRotAxes[0]=x; m_gizmoRotAxes[1]=y; m_gizmoRotAxes[2]=z; }
    // Abilita/disabilita le modalità non-translate (es. attach point: solo sposta).
    void setGizmoCanRotateScale(bool rotate, bool scale)
    { m_gizmoCanRotate = rotate; m_gizmoCanScale = scale; }

    bool popGizmoDelta(glm::vec3& outDelta);        // world, modalità Sposta
    bool popGizmoRotDelta(glm::vec3& outEulerDeg);  // delta euler (gradi) per asse
    bool popGizmoScaleDelta(glm::vec3& outDelta);   // world units per asse

    // ── Camera pan ───────────────────────────────────────────────────────
    void panCamera(float rightDelta, float upDelta);

    // ── Click selection ──────────────────────────────────────────────────
    std::string popClickedItem(); // nome dell'ultimo item cliccato (bone o marker)

private:
    // ── FBO ──────────────────────────────────────────────────────────
    unsigned int m_fbo=0, m_colorTex=0, m_depthRbo=0;
    int m_fbWidth=0, m_fbHeight=0;
    void resizeFBO(int w, int h);
    bool m_fboOk = false;

    // ── Shader ───────────────────────────────────────────────────────
    std::unique_ptr<mini::Shader> m_shader;

    // ── Geometria (client-side arrays, 6 float/vert: pos3 + col3) ─────
    std::vector<float> m_gridData;      int m_gridVertCount    = 0;
    std::vector<float> m_modelData;     int m_modelVertCount   = 0;
    std::vector<float> m_attachData;    int m_attachVertCount  = 0;
    std::vector<float> m_boxData;       int m_boxVertCount     = 0;
    std::vector<float> m_mapBoxData;    int m_mapBoxVertCount  = 0;

    void buildGrid(float size, int div);
    void drawArray(const std::vector<float>& data, int count,
                   unsigned int glMode, const glm::mat4& vp);
    void renderScene();

    // ── Camera ───────────────────────────────────────────────────────
    std::unique_ptr<mini::Camera> m_camera;
    float m_camSpeed    = 8.0f;
    bool  m_focused     = false;
    bool  m_mouseCapture= false;
    bool  m_tabWasDown  = false;
    bool  m_rmbLook     = false;   // navigazione stile Unreal: RMB tenuto = guarda+vola

    // ── Diagnostica ──────────────────────────────────────────────────
    std::string m_lastError;
    std::string m_lastModelStatus;

    // ── Barra caricamento (modulo standalone) ────────────────────────
    char  m_loadPathBuf[1024] = "";
    float m_loadRotX  = 0.0f;
    float m_loadScale = 1.0f;
    void  drawLoadBar();

    // ── Marker attach point ───────────────────────────────────────────
    bool  m_showFootMarker = false;
    float m_footMarkerY    = 0.0f;
    std::vector<float> m_footMarkerData;
    void  buildFootMarker(float y);

    // ── Bones ────────────────────────────────────────────────────────
    std::vector<JointData>       m_joints;
    std::string                  m_selBone;
    float                        m_jointRotX  = 0.0f;
    float                        m_jointScale = 1.0f;
    std::vector<float>           m_boneLineData;  int m_boneLineCount = 0;
    std::vector<float>           m_boneDotData;   int m_boneDotCount  = 0;

    void buildBoneData();

    // ── Markers ──────────────────────────────────────────────────────
    std::vector<ViewportMarker>  m_markers;
    std::vector<float>           m_markerData;   int m_markerCount = 0;

    void buildMarkerData();

    // ── Gizmo ────────────────────────────────────────────────────────
    bool      m_gizmoEnabled    = false;
    glm::vec3 m_gizmoPos        = {0,0,0};
    GizmoMode m_gizmoMode       = GizmoMode::Translate;
    bool      m_gizmoRotAxes[3] = {true, true, true};
    bool      m_gizmoCanRotate  = true;
    bool      m_gizmoCanScale   = true;

    bool      m_gizmoDragged    = false;
    glm::vec3 m_gizmoDelta      = {0,0,0};
    bool      m_gizmoRotDragged = false;
    glm::vec3 m_gizmoRotDelta   = {0,0,0};
    bool      m_gizmoScaleDragged = false;
    glm::vec3 m_gizmoScaleDelta = {0,0,0};

    int       m_gizmoActiveAxis = -1;  // 0=X,1=Y,2=Z, 3=uniforme (solo Scala)
    float     m_gizmoPrevAngle  = 0.0f; // angolo mouse precedente (Ruota)

    // ── Viewport image rect (per picking + gizmo) ────────────────────
    ImVec2    m_imgMin   = {0,0};
    ImVec2    m_imgSize  = {1,1};
    bool      m_imgClicked  = false;
    ImVec2    m_imgClickPos = {0,0};

    // ── Selection state ──────────────────────────────────────────────
    std::string m_selMarker;
    std::string m_lastClickedItem;

    void drawGizmoOverlay();
    void drawMarkerLabels();   // etichette testo dei marker (attach point ecc.)
    void handleViewportClick();
};

} // namespace editor
