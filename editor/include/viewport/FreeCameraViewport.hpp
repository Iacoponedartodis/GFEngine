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

    void loadModel(const std::string& path, float meshRotX = 0.0f, float meshScale = 1.0f,
                   float meshRotY = 0.0f);
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
        // Identificatore OPACO per il picking dal viewport: il chiamante ci mette
        // il proprio codice di selezione, il viewport lo restituisce quando il
        // box è cliccato (vedi popClickedMapBox). kNoPick = non selezionabile.
        static constexpr int kNoPick = 0x7fffffff;
        int pickId = kNoPick;
    };
    void setMapBoxes(const std::vector<MapBoxDraw>& boxes);
    void clearMapBoxes();

    // ── Modo di vista (doc 50 M3) ────────────────────────────────────────
    // Prospettiva per navigare, ORTOGRAFICA per misurare: in prospettiva una
    // lunghezza sullo schermo non corrisponde a una lunghezza nel mondo.
    enum class ViewMode : int { Perspective = 0, Top, Front, Side };
    void setViewMode(ViewMode m);
    [[nodiscard]] ViewMode viewMode() const { return m_viewMode; }
    [[nodiscard]] bool isOrtho() const { return m_viewMode != ViewMode::Perspective; }
    // Ingombro del CONTENUTO, dato da chi lo conosce (il modulo). Serve a inquadrare
    // qualcosa invece di puntare a caso: cambiare vista senza sapere dov'è la roba
    // è il modo sicuro di inquadrare il vuoto.
    void setContentBounds(const glm::vec3& mn, const glm::vec3& mx);
    void frameContent();   // "inquadra tutto" — il rimedio universale al perdersi
    // Inquadra UN punto (doc 53 L5, "portami lì"): non tocca l'ingombro del
    // contenuto, quindi `F` continua a inquadrare tutta la mappa.
    void focusOn(const glm::vec3& center, float radius = 4.0f);
    // Sola lettura, per il collaudo: permette di verificare che dopo un cambio di
    // vista il contenuto sia DAVVERO inquadrato, invece di dichiararlo.
    [[nodiscard]] const mini::Camera& camera() const { return *m_camera; }

    // ── RIGHELLO LIBERO (doc 50 M4) ──────────────────────────────────────
    // Due clic nel viewport, con aggancio alla griglia. Il righello che c'era
    // misurava solo fra DUE ELEMENTI SELEZIONATI, quindi non poteva misurare uno
    // spazio VUOTO — cioè il caso vero: la larghezza di un varco, la luce di un
    // passaggio, la distanza fra due muri. Come in Unreal, dà il meglio in
    // ortografica, dove una lunghezza sullo schermo è una lunghezza nel mondo.
    void setRulerActive(bool on);
    [[nodiscard]] bool rulerActive() const { return m_rulerActive; }
    void setGridSnap(float s) { m_rulerSnap = (s > 0.001f) ? s : 0.0f; }
    // Converte un punto dello SCHERMO in un punto sul piano orizzontale y = planeY.
    // Vale sia in prospettiva sia in ortografica: si sproietta la matrice.
    [[nodiscard]] bool screenToPlane(float sx, float sy, float planeY,
                                     glm::vec3& out) const;

    // ── Overlay NAVMESH (doc 47) ─────────────────────────────────────────
    // Le superfici che l'AI può davvero calpestare, disegnate come sono: è la
    // sola cosa che non mente, perché fra i box e il navmesh ci sono erosione,
    // sfoltimento dei cigli e area minima di regione. Triangoli già in coordinate
    // mondo, con il colore deciso dal chiamante (verde = raggiungibile dallo
    // spawn, rosso = isola).
    struct NavTriDraw
    {
        float ax, ay, az, bx, by, bz, cx, cy, cz;
        float r, g, b;
    };
    void setNavMesh(const std::vector<NavTriDraw>& tris);
    void clearNavMesh();
    // Facce piene ombreggiate oltre al wireframe: rende le SUPERFICI visibili
    // (muri/piattaforme/cover non più solo linee). Opaco + spigoli sopra: nessun
    // blending → zero rischio compat Intel (ADR-003).
    void setShowSolid(bool s) { m_showSolid = s; }
    bool showSolid() const    { return m_showSolid; }

    // Selezione dal viewport (ray-picking, doc: "selezione oggetti dalle
    // viewport"): se dall'ultima chiamata è stato cliccato un box con pickId
    // valido, ritorna true e scrive il pickId del più vicino colpito dal ray.
    bool popClickedMapBox(int& outPickId);

    // Marker visivo per un attach point
    void setFootMarker(float y, bool show);

    // Ritorna true se il sistema GL è operativo
    bool isReady() const;

    // Rilascia la cattura del mouse (Tab) se attiva, riportando il cursore
    // visibile. Idempotente. DEVE essere chiamata quando questo viewport smette
    // di essere il modulo attivo: SDL_SetRelativeMouseMode è uno stato GLOBALE e
    // il tick() — che è l'unico a poterlo spegnere col Tab — non gira più quando
    // il modulo non è attivo, lasciando il mouse invisibile e non liberabile.
    void releaseMouseCapture();

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
    // `Face` è il gesto primario di costruzione secondo tre riferimenti su quattro
    // (CubeGrid di Unreal, TrenchBroom, "A Simpler 3D Level Editor"): si afferra una
    // FACCIA e la si tira. Non è "scala": la faccia opposta resta ferma, quindi si
    // allunga un muro senza doverlo anche ricentrare. Con la scala ogni allungamento
    // costa due gesti — ed è il motivo per cui costruire qui costava sei gesti a box.
    enum class GizmoMode { Translate, Rotate, Scale, Face };

    void      setGizmoTarget(glm::vec3 pos, bool enabled);
    void      setGizmoMode(GizmoMode m)      { m_gizmoMode = m; }
    GizmoMode getGizmoMode() const           { return m_gizmoMode; }
    // Limita gli anelli di rotazione disponibili (es. solo Y per i box mappa).
    void setGizmoRotAxes(bool x, bool y, bool z)
    { m_gizmoRotAxes[0]=x; m_gizmoRotAxes[1]=y; m_gizmoRotAxes[2]=z; }
    // Abilita/disabilita le modalità non-translate (es. attach point: solo sposta).
    void setGizmoCanRotateScale(bool rotate, bool scale)
    { m_gizmoCanRotate = rotate; m_gizmoCanScale = scale; }

    // Il gizmo è afferrato adesso? Serve a chi tiene una pila di annullamento:
    // la fotografia va presa quando il gesto COMINCIA, non a ogni delta.
    [[nodiscard]] bool gizmoDragging() const { return m_gizmoActiveAxis >= 0; }
    bool popGizmoDelta(glm::vec3& outDelta);        // world, modalità Sposta
    bool popGizmoRotDelta(glm::vec3& outEulerDeg);  // delta euler (gradi) per asse
    bool popGizmoScaleDelta(glm::vec3& outDelta);   // world units per asse

    // ── MODALITÀ FACCIA (doc 53 L1) ──────────────────────────────────────
    // L'ingombro della selezione, da chi lo conosce: serve a mettere le sei
    // maniglie sulle facce. Senza, il gizmo non saprebbe dove sono.
    void setGizmoBounds(const glm::vec3& mn, const glm::vec3& mx, bool valid);
    // Faccia: 0=-X 1=+X 2=-Y 3=+Y 4=-Z 5=+Z. `outDelta` è positivo VERSO L'ESTERNO
    // (la faccia si allontana dal centro) ed è sempre un multiplo del passo di
    // griglia: una costruzione che non si aggancia produce fessure sotto la soglia
    // di erosione del navmesh, cioè difetti che non si vedono.
    bool popGizmoFaceDelta(int& outFace, float& outDelta);
    [[nodiscard]] int activeFace() const { return m_faceLast; }

    // ── DISEGNA UN BOX (doc 53 L1) ───────────────────────────────────────
    // Trascinamento sul piano di lavoro: definisce l'impronta in pianta. È il gesto
    // con cui nasce un muro in Hammer e in CubeGrid — uno, invece dei quattro di
    // "crea a misura fissa, poi tre campi numerici".
    void setDrawBoxActive(bool on);
    [[nodiscard]] bool drawBoxActive() const { return m_drawActive; }
    void setDrawPlaneY(float y) { m_drawPlaneY = y; }
    [[nodiscard]] float drawPlaneY() const { return m_drawPlaneY; }
    // Rettangolo appena disegnato, in coordinate mondo sul piano di lavoro.
    bool popDrawnRect(glm::vec3& outMin, glm::vec3& outMax);

    // Ctrl+rotella ha chiesto di cambiare il passo di griglia: +1 più grande,
    // -1 più piccolo. Il viewport non conosce i passi ammessi — li decide il
    // modulo, che è anche quello che li salva.
    bool popGridStepRequest(int& outDir);

    // ── Camera pan ───────────────────────────────────────────────────────
    void panCamera(float rightDelta, float upDelta);

    // Punto sul suolo (y=0) davanti alla camera: dove creare i NUOVI oggetti,
    // invece che sempre al centro mappa. Se la camera non guarda in basso,
    // ripiega a distanza fissa davanti a sé. Così un oggetto nasce già dove
    // stai guardando, non da trascinare dal centro (richiesta utente).
    glm::vec3 groundFocusPoint(float fallbackDist = 12.0f) const;

    // ── Click selection ──────────────────────────────────────────────────
    std::string popClickedItem(); // nome dell'ultimo item cliccato (bone o marker)

private:
    // ── FBO ──────────────────────────────────────────────────────────
    unsigned int m_fbo=0, m_colorTex=0, m_depthRbo=0;
    int m_fbWidth=0, m_fbHeight=0;    // area visibile richiesta dal pannello
    int m_texWidth=0, m_texHeight=0;  // texture allocata (multipli di 64, solo crescita)
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
    std::vector<float> m_mapBoxFillData; int m_mapBoxFillVertCount = 0; // facce piene
    // Overlay navmesh: facce + spigoli. Disegnato DOPO i box e leggermente alzato,
    // così si vede sopra il pavimento senza z-fighting.
    std::vector<float> m_navFillData; int m_navFillVertCount = 0;
    std::vector<float> m_navEdgeData; int m_navEdgeVertCount = 0;
    bool m_showSolid = true;   // default: superfici visibili (richiesta utente)
    ViewMode m_viewMode = ViewMode::Perspective;
    // Stato della vista prospettica, conservato mentre si sta in ortografica: senza,
    // tornandoci la camera restava dove l'aveva messa l'ortografica (centinaia di
    // metri in aria) a inquadrare il nulla.
    bool      m_perspSaved = false;
    glm::vec3 m_perspPos{0.0f};
    float     m_perspYaw = -90.0f, m_perspPitch = 0.0f;
    // Ingombro del contenuto, comunicato dal modulo.
    bool      m_contentValid = false;
    glm::vec3 m_contentMin{0.0f}, m_contentMax{0.0f};
    // Misure sempre in vista: barra di scala + coordinate ai bordi.
    bool  m_showMeasures = true;
    void  drawMeasureOverlay();
    void  applyOrthoPlacement(ViewMode m, const glm::vec3& center);
    [[nodiscard]] float frameHalfHeightFor(const glm::vec3& mn, const glm::vec3& mx,
                                           ViewMode m) const;
    // Righello: A fissato col primo clic, B segue il cursore fino al secondo.
    bool      m_rulerActive = false;
    bool      m_rulerHasA   = false;
    bool      m_rulerFrozen = false;   // due punti fissati: il risultato resta leggibile
    glm::vec3 m_rulerA{0.0f}, m_rulerB{0.0f};
    float     m_rulerSnap   = 0.0f;    // 0 = nessun aggancio
    std::vector<float> m_rulerData; int m_rulerVertCount = 0;
    void buildRulerGeometry();

    void buildGrid(float size, int div);
    // Griglia che segue la vista e adatta il passo: si ricostruisce solo quando la
    // cella o il passo cambiano, quindi costa nulla nei frame fermi.
    void updateInfiniteGrid();
    float m_gridStep = -1.0f, m_gridCx = 0.0f, m_gridCz = 0.0f;
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

    // Ray-picking dei map box (selezione dal viewport): lista conservata per il
    // test + esito dell'ultimo click.
    std::vector<MapBoxDraw> m_mapBoxes;
    int  m_clickedBoxId  = MapBoxDraw::kNoPick;
    bool m_hasClickedBox = false;
    bool m_gizmoBarHovered = false;  // click sui pulsanti modalità → non selezionare dietro

    // ── Modalità FACCIA (doc 53 L1) ──────────────────────────────────────
    bool      m_boundsValid = false;
    glm::vec3 m_boundsMin{0.0f}, m_boundsMax{0.0f};
    int       m_faceActive  = -1;    // faccia afferrata adesso (0..5), -1 = nessuna
    int       m_faceLast    = 3;     // ultima usata; +Y perché "alza il muro" è il caso comune
    float     m_faceAccum   = 0.0f;  // metri accumulati non ancora emessi (sotto un passo)
    float     m_facePending = 0.0f;  // metri già agganciati, in attesa che il modulo li prenda
    bool      m_faceHas     = false;

    // ── Disegna un box ───────────────────────────────────────────────────
    bool      m_drawActive  = false;
    bool      m_drawDragging = false;
    float     m_drawPlaneY  = 0.0f;
    glm::vec3 m_drawA{0.0f}, m_drawB{0.0f};
    bool      m_drawHas     = false;   // rettangolo pronto da ritirare
    glm::vec3 m_drawMin{0.0f}, m_drawMax{0.0f};

    int  m_gridStepReq = 0;          // Ctrl+rotella: +1/-1, azzerato quando ritirato
    // Proiezione mondo → schermo, condivisa da gizmo e sovrapposizioni.
    [[nodiscard]] bool worldToScreen(const glm::vec3& w, ImVec2& out) const;
    void drawFaceGizmo();
    void drawBoxTool(bool hovered);

    void drawGizmoOverlay();
    void drawMarkerLabels();   // etichette testo dei marker (attach point ecc.)
    void handleViewportClick();
};

} // namespace editor
