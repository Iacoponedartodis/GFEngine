// MapEditor.cpp
// Modulo GFEditor per la modifica visuale della geometria delle mappe.
// Layout: [lista box | viewport 3D | pannello proprietà]
// Salva/carica da data/maps/<id>.json, campo "geometry".

#include "util/DataPath.hpp"
#include "modules/MapEditor.hpp"
#include "util/FileDialog.hpp"
#include "util/UiWidgets.hpp"
#include "util/JsonSave.hpp"
#include "util/DefinitionRename.hpp"
#include "util/StartupOptions.hpp"
#include "framework/Dialogs.hpp"     // finestre modali condivise (doc 52 F4)
#include "mini/game/StructureJson.hpp" // una sola lettura/scrittura della ricetta
#include "mini/render/Camera.hpp"      // posizione telecamera per "Prova da qui"
// Esposizione mostrata al designer (ADR-033): si riusa la STESSA funzione del
// runtime invece di duplicarne la regola nell'editor.
#include "mini/game/data/Definitions.hpp"
#include "mini/game/ai/WorldIntel.hpp"
// Salute tattica (doc 41 B4): STESSE regole del gate `--validate`, mai una copia.
#include "mini/game/data/ContentValidation.hpp"
// Stesse costanti del runtime per la visuale verticale (KI #83): l'editor deve
// misurare con lo stesso modello con cui il gioco combatte, non con uno suo.
#include "mini/core/GameConfig.hpp"

#include <imgui.h>
#include <SDL2/SDL.h>

#include <nlohmann/json.hpp>
#include <fstream>
#include <filesystem>
#include <algorithm>
#include <chrono>
#include <cstring>
#include <cstdio>
#include <cmath>

namespace fs = std::filesystem;
using json   = nlohmann::json;

namespace editor
{

// ── Helpers ───────────────────────────────────────────────────────────────────
// R8 chiuso: unica risoluzione in util/DataPath. Questo modulo era una delle
// quattro copie col controllo DEBOLE (solo "la cartella esiste"): ora usa quello
// forte (`data/weapons`) come tutti gli altri.
static std::string getDataDir() { return editor::datapath::root(); }

// ── Ctor ─────────────────────────────────────────────────────────────────────
MapEditor::MapEditor()
{
    // Id veicoli per il combo degli spawn (id = filename stem, ADR-001)
    {
        std::error_code ec;
        fs::path folder = fs::path(getDataDir()) / "vehicles";
        if (fs::exists(folder, ec))
            for (auto& entry : fs::directory_iterator(folder, ec))
                if (entry.path().extension() == ".json")
                    m_vehicleIds.push_back(entry.path().stem().string());
        std::sort(m_vehicleIds.begin(), m_vehicleIds.end());
    }
    // Prefab (ADR-048): caricati col loader del RUNTIME, non con un parser dell'editor
    // — un secondo parser divergerebbe al primo campo aggiunto.
    {
        m_prefabReg.loadPrefabs(getDataDir());
        for (const auto& [id, def] : m_prefabReg.prefabs()) m_prefabIds.push_back(id);
        std::sort(m_prefabIds.begin(), m_prefabIds.end());
    }
    // Tipi di struttura (ADR-055): stesso principio dei prefab — il loader del
    // runtime, non un secondo parser.
    {
        m_prefabReg.loadStructureTypes(getDataDir());
        refreshStructTypeIds();
        m_typeResolver = [this](const std::string& id) {
            return m_prefabReg.getStructureType(id);
        };
        // `--struct-tab <id>`: apre subito il tab, così il percorso si può eseguire
        // e verificare da riga di comando invece che solo dichiarare.
        if (editor::startup::g_structTabSet)
        {
            openStructTab(editor::startup::g_structTab);
            if (!m_structTabs.empty()) checkStructType(m_structTabs.back());
        }
    }
    // CommanderDef per il combo del comandante (ADR-044: fuori dalle classi)
    {
        std::error_code ec;
        fs::path folder = fs::path(getDataDir()) / "commanders";
        if (fs::exists(folder, ec))
            for (auto& entry : fs::directory_iterator(folder, ec))
                if (entry.path().extension() == ".json")
                    m_commanderIds.push_back(entry.path().stem().string());
        std::sort(m_commanderIds.begin(), m_commanderIds.end());
    }

    loadMaps();
    if (!m_mapList.empty())
        loadMap(m_mapList[0].id);
}

// ── tick ─────────────────────────────────────────────────────────────────────
void MapEditor::tick(float dt)
{
    // `m_activeTab` lo scrivono i `BeginTabItem`, che in certi frame NON vengono
    // eseguiti (finestra ridotta, barra dei tab che non si apre): il valore resta
    // quello vecchio. Se resta puntato a un tab struttura che non c'è più, la
    // viewport della mappa smette di avanzare e non rilascia mai il mouse — cioè
    // l'editor si blocca. La verità qui è quante schede esistono davvero.
    if (m_structTabs.empty() || m_activeTab > (int)m_structTabs.size() || m_activeTab < 0)
        m_activeTab = 0;

    tickAutosave(dt);

    // ── Gizmo sulla PARTE selezionata di un assemblaggio ─────────────────
    // Senza, le parti si potevano solo digitare a numeri: un assemblaggio non era
    // componibile, era compilabile. Sposta/Ruota/Scala come in mappa, con la scala
    // che agisce sulle dimensioni del box o sulla misura principale della primitiva.
    if (m_activeTab > 0 && m_activeTab <= (int)m_structTabs.size())
    {
        auto& tb = m_structTabs[m_activeTab - 1];

        // ── Selezione e gizmo dal componente CONDIVISO (doc 52 F1) ───────
        // Qui prima c'erano ~50 righe che rifacevano a mano ciò che il Map Editor
        // già faceva: leggere il picking, tirare i tre delta del gizmo, riposizionare
        // il gizmo. Ora il modulo dichiara solo COME si legge e si scrive una parte.
        ViewportEditing::Ops ops;
        auto partPtr = [&tb](int i) -> mini::StructurePart* {
            return (i >= 0 && i < (int)tb.parts().size()) ? &tb.parts()[i] : nullptr;
        };
        auto pos = [&](int i) {
            const auto* p = partPtr(i);
            if (!p) return glm::vec3{0.0f};
            return p->isBox ? glm::vec3{p->box.x, p->box.y, p->box.z}
                            : glm::vec3{p->prim.x, p->prim.y, p->prim.z};
        };
        ops.valid  = [&](int i) { return partPtr(i) != nullptr; };
        // Il gizmo al baricentro della selezione: con una parte sola coincide con lei.
        ops.anchor = [&](const std::vector<int>& sel) {
            glm::vec3 c{0.0f};
            for (int i : sel) c += pos(i);
            return sel.empty() ? c : c / (float)sel.size();
        };
        ops.move = [&](const std::vector<int>& sel, const glm::vec3& d) {
            for (int i : sel)
            {
                auto* p = partPtr(i); if (!p) continue;
                float* x = p->isBox ? &p->box.x : &p->prim.x;
                float* y = p->isBox ? &p->box.y : &p->prim.y;
                float* z = p->isBox ? &p->box.z : &p->prim.z;
                *x += d.x; *y += d.y; *z += d.z;
            }
        };
        ops.rotate = [&](const std::vector<int>& sel, const glm::vec3& euler) {
            // Politica del MODULO: qui ogni parte gira su sé stessa attorno a Y.
            // Solo Y perché un `MapGeometryBox` ha solo `ry`: fingere gli altri assi
            // darebbe un gizmo che si muove senza che cambi nulla.
            for (int i : sel)
            {
                auto* p = partPtr(i); if (!p) continue;
                float* r = p->isBox ? &p->box.ry : &p->prim.ry;
                *r += euler.y;
                while (*r >  180.0f) *r -= 360.0f;
                while (*r < -180.0f) *r += 360.0f;
            }
        };
        ops.scale = [&](const std::vector<int>& sel, const glm::vec3& d) {
            for (int i : sel)
            {
                auto* p = partPtr(i); if (!p) continue;
                // Un RIFERIMENTO non si scala: le sue misure sono dell'altra
                // struttura. Scalarlo qui darebbe frecce che si muovono e geometria
                // che non cambia — la stessa bugia dei parametri finti nel pannello.
                if (p->isRef()) continue;
                if (p->isBox)
                {
                    p->box.sx = (p->box.sx + d.x < 0.05f) ? 0.05f : p->box.sx + d.x;
                    p->box.sy = (p->box.sy + d.y < 0.05f) ? 0.05f : p->box.sy + d.y;
                    p->box.sz = (p->box.sz + d.z < 0.05f) ? 0.05f : p->box.sz + d.z;
                }
                else scalePrimitivePart(p->prim, d);
            }
        };
        // La fotografia per l'annullamento: UNA per gesto, non una per delta.
        ops.beginGesture = [&]() { tb.undo.push(tb.snapshot(m_selPart), "gizmo", m_editorClock); };

        // La selezione del tab è un indice singolo: si presta al componente come
        // insieme di uno e si riprende com'è tornato. Il giorno in cui servirà
        // selezionare più parti, il componente è già pronto.
        std::vector<int> sel;
        if (m_selPart >= 0) sel.push_back(m_selPart);
        const bool ch = m_structEdit.tick(m_structVp, sel, ops);
        m_selPart = sel.empty() ? -1 : sel.back();
        if (ch) { tb.dirty = true; rebuildStructTabPreview(tb); }
    }

    // Solo la viewport del tab ATTIVO avanza: due telecamere che si muovono insieme
    // significa che tornando al tab Mappa la vista è cambiata da sola.
    if (m_activeTab == 0) m_viewport.tick(dt);
    else                  m_structVp.tick(dt);
    m_editorClock += dt;   // orologio per la coalescenza dell'undo
    if (m_savedFlash > 0.0f) m_savedFlash -= dt;   // "Salvato" sfuma da solo

    // Gizmo Sposta: applica lo spostamento a TUTTA la selezione (G3).
    glm::vec3 delta;
    if (m_viewport.popGizmoDelta(delta))
    {
        // Un trascinamento produce un delta per frame: `pushUndo` li fonde in una
        // sola voce (stessa etichetta entro 0,6 s), così "annulla" riporta all'inizio
        // del gesto e non indietro di un pixel.
        pushUndo("gizmo");
        for (int code : selectionCodes()) applyMove(code, delta);
        m_dirty = true;
        updateViewport(!m_viewport.gizmoDragging());
    }

    applyGizmoRotateScale();
    applyFaceDrag();

    // Il box disegnato col trascinamento sul piano di lavoro.
    if (glm::vec3 mn, mx; m_viewport.popDrawnRect(mn, mx)) createDrawnBox(mn, mx);

    // L'ingombro della selezione va comunicato a ogni frame: è dove il viewport
    // posa le sei maniglie della modalità Faccia, e una selezione che cambia senza
    // che le maniglie la seguano è un gizmo che agisce su qualcos'altro.
    {
        glm::vec3 bmn{0.0f}, bmx{0.0f};
        // Due righe e non una: passare `selectionBounds(bmn, bmx)` come argomento
        // accanto a `bmn`/`bmx` lascerebbe l'ordine di valutazione al compilatore,
        // e le maniglie finirebbero su un ingombro non ancora scritto.
        const bool okb = selectionBounds(bmn, bmx);
        m_viewport.setGizmoBounds(bmn, bmx, okb);
    }

    // ── I DERIVATI NON SI RICALCOLANO DURANTE IL TRASCINAMENTO (doc 51 §1) ──
    // `recomputeExposure` è O(n²) sulle posizioni tattiche: a 1500 posizioni costa
    // 21-34 ms, e girava a OGNI FRAME del trascinamento. Il risultato intermedio non
    // lo guarda nessuno — si guarda dove finisce l'oggetto — quindi si paga una
    // volta, al rilascio. È il caso peggiore misurato, e sparisce tutto.
    const bool dragNow = m_viewport.gizmoDragging();
    if (m_wasDragging && !dragNow) updateViewport(true);
    m_wasDragging = dragNow;

    // ── Passo di griglia con Ctrl+rotella (CubeGrid, TrenchBroom) ─────────
    // Cambiare il passo è il gesto più frequente della costruzione — grande per le
    // stanze, piccolo per la rifinitura — e finora richiedeva di andare a cercare un
    // combo nella barra. Ctrl+rotella è la convenzione di entrambi i riferimenti.
    if (int dir = 0; m_viewport.popGridStepRequest(dir))
    {
        static const float steps[] = { 0.10f, 0.25f, 0.50f, 1.00f, 2.00f, 4.00f, 8.00f };
        constexpr int N = (int)(sizeof(steps) / sizeof(steps[0]));
        int cur = 2;
        for (int i = 0; i < N; ++i)
            if (std::fabs(m_gridSnap - steps[i]) < 0.001f) { cur = i; break; }
        cur += dir;
        if (cur < 0) cur = 0;
        if (cur >= N) cur = N - 1;
        m_gridSnap = steps[cur];
        m_viewport.setGridSnap(m_gridSnap);
    }
}

// ── Modello di SELEZIONE MULTIPLA (G3) ───────────────────────────────────────
// Un solo insieme di codici. Il PRIMARIO è l'ultimo della lista: è ciò che il
// pannello proprietà mostra, e con un solo elemento tutto si comporta come prima.
int MapEditor::primaryCode() const
{
    if (!m_multiSel.empty()) return m_multiSel.back();
    if (m_selStruct >= 0) return -6000 - m_selStruct;
    return m_selBox;
}

std::vector<int> MapEditor::selectionCodes() const
{
    if (!m_multiSel.empty()) return m_multiSel;
    const int c = primaryCode();
    if (c == -1) return {};
    return { c };
}

// ── IL GATE DEI DATI, DA DENTRO IL MAP EDITOR (doc 53 L5) ────────────────────
// Lo STESSO `validateContent` di `--validate` e del pannello Validazione: nessuna
// seconda analisi, o prima o poi darebbe un verdetto diverso dal gioco — è il
// difetto che è già costato di più (changelog 77, due verità sullo stesso mondo).
//
// Filtrato su QUESTA mappa: costruendo interessa sapere se ho appena rotto quella
// che ho davanti, non l'inventario completo. Il resto resta nel pannello
// Validazione, che è il posto giusto per guardare tutto.
//
// SU RICHIESTA e non a ogni frame: ricarica l'intero registry, e non è un costo da
// pagare mentre si trascina un box.
void MapEditor::runDataGate()
{
    m_gateLines.clear();
    m_gateRun = true;
    mini::DefinitionRegistry reg;
    reg.loadAll(getDataDir());
    const std::string mine = "maps/" + m_mapId + ".json";
    for (const auto& d : mini::validateContent(reg, getDataDir()))
    {
        // Le voci di QUESTA mappa, più quelle delle strutture: una composita rotta
        // si vede in mappa, ed è lì che si va a correggerla.
        const bool isMine = (d.file == mine);
        const bool isStruct = (d.file.rfind("structures/", 0) == 0);
        if (!isMine && !isStruct) continue;
        const int sev = (d.severity == mini::telemetry::Level::Error) ? 1 : 0;
        // Il SUGGERIMENTO fa parte della voce: una diagnosi senza l'azione è rumore
        // (è la regola di doc 21, e il motivo per cui `Diagnostic` ha quel campo).
        std::string line = d.file + " — " + d.message;
        if (!d.suggestion.empty()) line += "\n    → " + d.suggestion;
        m_gateLines.push_back({sev, std::move(line)});
    }
    std::stable_sort(m_gateLines.begin(), m_gateLines.end(),
                     [](const auto& a, const auto& b) { return a.first > b.first; });
}

// ── TUTTI I PROBLEMI IN UN POSTO SOLO (richiesta utente 2026-08-11) ──────────
// Erano tre: la salute tattica nel pannello di sinistra, l'esito del navmesh
// scritto in barra, il gate dei dati sotto l'elenco. Tre presentazioni diverse
// della stessa domanda ("cosa non va?"), e quella in barra restava rossa in
// permanenza — anche per 24 triangoli su 484. Un avviso sempre acceso smette di
// essere un avviso e diventa arredamento.
//
// Qui si RACCOLGONO, non si ricalcolano: le tre analisi restano dove sono (e
// restano condivise col gate). Questa funzione le legge e basta — una quarta
// analisi "per la finestra" sarebbe la quarta verità sullo stesso mondo.
std::vector<MapEditor::Problem> MapEditor::collectProblems() const
{
    std::vector<Problem> out;

    // 1) Salute tattica + geometria (analyzeTacticalHealth, condivisa con --validate)
    for (const auto& is : m_issues)
    {
        Problem p;
        p.sev   = is.sev;
        p.group = mini::tacticalDefectKindName((mini::TacticalDefect::Kind)is.kind);
        p.text  = is.text;
        p.sel   = is.sel;
        glm::vec3 w;
        if (codePosition(is.sel, w)) { p.hasPos = true; p.pos = w; }
        out.push_back(std::move(p));
    }

    // 2) Navmesh: tre cause distinte, tre gruppi distinti. Tradurle tutte in
    // "isole" era il difetto dell'indicatore precedente.
    if (m_navBuilt && !m_navStale)
    {
        for (const auto& isl : m_navReport.islands)
        {
            Problem p;
            // La GRAVITÀ dalla dimensione, non dall'esistenza: una chiazza sotto
            // l'area minima di una regione Recast (2,56 m²) è un angolo di
            // geometria, non una zona tagliata fuori. Chiamarle tutte "problema"
            // è il motivo per cui l'indicatore restava rosso per sempre.
            // Pavimento chiuso sotto un cubo: irraggiungibile per costruzione, non
            // un difetto. Resta in elenco — sparire sarebbe una bugia — ma come
            // avviso e con la ragione scritta, così non manda a caccia di niente.
            const bool under = (isl.covered >= 0.70f);
            p.sev   = (!under && isl.area >= 6.0f) ? 1 : 0;
            p.group = under
                ? "Pavimento chiuso sotto un ostacolo (normale, non un difetto)"
                : "Isole del navmesh (superficie da cui non si arriva allo spawn)";
            char b[240];
            if (under)
            {
                std::snprintf(b, sizeof(b),
                              "%.1f m2 a %.0f, %.0f, %.0f — %.0f%% sotto un ostacolo: "
                              "e' il terreno chiuso sotto un cubo, nessuno deve andarci",
                              isl.area, isl.center.x, isl.center.y, isl.center.z,
                              isl.covered * 100.0f);
                p.text = b;
                p.hasPos = true; p.pos = isl.center;
                p.radius = std::max(6.0f, std::sqrt(isl.area) * 1.4f);
                p.needsNav = true;
                out.push_back(std::move(p));
                continue;
            }
            // L'AREA, e basta. Il numero di triangoli l'ho tolto dal testo perché
            // MENTE: il navmesh è grossolano (una mappa intera sta in ~500
            // triangoli), quindi "24 triangoli" possono essere 400 m². Mi ci sono
            // cascato io stesso chiamandole "schegge" — erano zone da 40 m².
            std::snprintf(b, sizeof(b),
                          "%.1f m2 attorno a %.0f, %.0f, %.0f — %s",
                          isl.area, isl.center.x, isl.center.y, isl.center.z,
                          isl.area >= 6.0f
                            ? "una zona vera in cui l'AI puo' stare ma non arrivare: "
                              "manca un collegamento"
                            : "un angolo dove due superfici si incontrano senza "
                              "collegarsi (l'erosione toglie 0,40 m per lato)");
            p.text = b;
            p.hasPos = true; p.pos = isl.center;
            // Inquadratura proporzionata: una zona da 40 m² con raggio fisso 8 si
            // vedrebbe solo in parte, e "non c'è niente qui" è la conclusione.
            p.radius = std::max(6.0f, std::sqrt(isl.area) * 1.4f);
            p.needsNav = true;
            out.push_back(std::move(p));
        }
        for (int i : m_navReport.badPositions)
        {
            if (i < 0 || i >= (int)m_positions.size()) continue;
            Problem p;
            p.sev = 1;
            p.group = "Posizioni tattiche che il navmesh non raggiunge";
            char b[160];
            std::snprintf(b, sizeof(b), "posizione %d (%s) a %.1f, %.1f: nessuna AI "
                          "ci arriva → lavoro sprecato e un buco nella copertura",
                          i + 1, m_positions[i].role.c_str(),
                          m_positions[i].x, m_positions[i].z);
            p.text = b;
            p.sel = -1000 - i;   // stesso schema di codePosition
            p.needsNav = true;
            p.hasPos = true;
            p.pos = {m_positions[i].x, m_positions[i].y, m_positions[i].z};
            out.push_back(std::move(p));
        }
        for (int i : m_navReport.badPosts)
        {
            if (i < 0 || i >= (int)m_posts.size()) continue;
            Problem p;
            p.sev = 1;
            p.group = "Command post che il navmesh non raggiunge";
            char b[160];
            std::snprintf(b, sizeof(b), "post '%s' a %.1f, %.1f: INCATTURABILE — una "
                          "missione che lo chiede e' incompletabile",
                          m_posts[i].label, m_posts[i].x, m_posts[i].z);
            p.text = b;
            p.sel = -10 - i;
            p.needsNav = true;
            p.hasPos = true;
            p.pos = {m_posts[i].x, m_posts[i].y, m_posts[i].z};
            out.push_back(std::move(p));
        }
    }

    // 3) Gate dei dati (solo se l'utente l'ha chiesto: ricarica il registry)
    for (const auto& l : m_gateLines)
    {
        Problem p;
        p.sev = l.first;
        p.group = "Dati e riferimenti (stesso controllo di --validate)";
        p.text = l.second;
        out.push_back(std::move(p));
    }
    return out;
}

void MapEditor::drawProblemsWindow()
{
    if (!m_showProblems) return;
    ImGui::SetNextWindowSize(ImVec2(720, 460), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Problemi della mappa", &m_showProblems))
    { ImGui::End(); return; }

    const auto probs = collectProblems();
    int nProb = 0;
    for (const auto& p : probs) if (p.sev == 1) ++nProb;

    if (probs.empty())
        ImGui::TextColored({0.45f, 0.85f, 0.50f, 1.0f},
                           "Nessun problema noto su questa mappa.");
    else
        ImGui::Text("%d problemi, %d avvisi", nProb, (int)probs.size() - nProb);
    ImGui::TextDisabled("Clicca una voce: seleziona l'elemento e ti porta li'.");

    ImGui::SameLine();
    if (ImGui::SmallButton(m_navStale ? "Verifica navmesh" : "Ri-verifica navmesh"))
        validateNavmesh();
    ImGui::SameLine();
    if (ImGui::SmallButton(m_gateRun ? "Ricontrolla i dati" : "Controlla i dati"))
        runDataGate();
    if (m_navBuilt && m_navStale)
    {
        ImGui::SameLine();
        ImGui::TextColored({0.90f, 0.75f, 0.35f, 1.0f}, "(navmesh da ri-verificare)");
    }
    ImGui::Separator();

    // Raggruppato per tipo, come la salute tattica: un elenco lungo e
    // indifferenziato si smette di leggere. I gruppi con PROBLEMI si aprono da soli.
    std::vector<std::string> groups;
    for (const auto& p : probs)
        if (std::find(groups.begin(), groups.end(), p.group) == groups.end())
            groups.push_back(p.group);

    ImGui::BeginChild("##problist", ImVec2(0, 0), true);
    for (const auto& g : groups)
    {
        int gp = 0, gw = 0;
        for (const auto& p : probs) if (p.group == g) { if (p.sev == 1) ++gp; else ++gw; }
        char hdr[224];
        std::snprintf(hdr, sizeof(hdr), "%s — %d###g_%s", g.c_str(), gp + gw, g.c_str());
        if (gp > 0) ImGui::SetNextItemOpen(true, ImGuiCond_Once);
        ImGui::PushStyleColor(ImGuiCol_Text, gp > 0 ? ImVec4(0.95f, 0.60f, 0.50f, 1.0f)
                                                    : ImVec4(0.85f, 0.82f, 0.55f, 1.0f));
        const bool open = ImGui::TreeNode(hdr);
        ImGui::PopStyleColor();
        if (!open) continue;
        int k = 0;
        for (const auto& p : probs)
        {
            if (p.group != g) continue;
            ImGui::PushID(k++);
            ImGui::PushStyleColor(ImGuiCol_Text, p.sev == 1 ? ImVec4(0.95f, 0.60f, 0.50f, 1.0f)
                                                            : ImVec4(0.85f, 0.82f, 0.55f, 1.0f));
            // ── NIENTE `SameLine` DOPO UN `Selectable` A PIENA LARGHEZZA ──
            // La prima versione faceva `Selectable("##row", …, ImVec2(0, h))` +
            // `SameLine(0,0)` + `TextWrapped`. Un `Selectable` con larghezza 0
            // occupa TUTTA la riga, quindi `SameLine` portava il cursore al bordo
            // destro e a `TextWrapped` restava una larghezza di ~0: una parola per
            // riga, poi una lettera per riga — migliaia di righe per voce.
            // L'editor lampeggiava e si bloccava, e l'utente ha dovuto chiuderlo da
            // Gestione attività. **Non è un caso limite: succedeva sempre, e di più
            // ingrandendo la finestra** (più voci visibili contemporaneamente).
            //
            // Il testo sta DENTRO l'etichetta: ImGui la ritaglia al bordo senza
            // mandare a capo, quindi il costo è fisso qualunque sia la lunghezza. Il
            // messaggio intero si legge nel suggerimento, dove il wrapping è sano.
            char row[320];
            std::snprintf(row, sizeof(row), "%s %s", p.sev == 1 ? "!" : "-",
                          p.text.c_str());
            const bool clicked = ImGui::Selectable(row);
            ImGui::PopStyleColor();
            if (ImGui::IsItemHovered())
            {
                ImGui::BeginTooltip();
                ImGui::PushTextWrapPos(520.0f);   // larghezza ESPLICITA: mai 0
                ImGui::TextUnformatted(p.text.c_str());
                ImGui::PopTextWrapPos();
                ImGui::EndTooltip();
            }
            if (clicked)
            {
                if (p.sel != -1) { m_multiSel.clear(); setSelection(p.sel, false); }
                // Un'isola non è un ELEMENTO: non si può selezionare, quindi
                // arrivando sul posto non c'è niente di evidenziato e sembra che la
                // telecamera sia andata nel vuoto (segnalato dall'utente).
                // Si accende l'overlay del navmesh: il rosso È l'isola.
                if (p.needsNav && !m_showNav) m_showNav = true;
                if (p.hasPos) m_viewport.focusOn(p.pos, p.radius);
                updateViewport();
            }
            ImGui::PopID();
            ImGui::Separator();
        }
        ImGui::TreePop();
    }
    ImGui::EndChild();
    ImGui::End();
}

void MapEditor::setSelection(int code, bool additive)
{
    if (additive)
    {
        // Ctrl+click: aggiunge o toglie, come in qualunque gestore di file. Se
        // l'insieme è vuoto si parte da ciò che era già selezionato, altrimenti
        // il primo Ctrl+click perderebbe la selezione corrente.
        if (m_multiSel.empty() && primaryCode() != -1)
            m_multiSel.push_back(primaryCode());
        auto it = std::find(m_multiSel.begin(), m_multiSel.end(), code);
        if (it != m_multiSel.end()) m_multiSel.erase(it);
        else                        m_multiSel.push_back(code);
    }
    else
    {
        m_multiSel.clear();
        if (code != -1) m_multiSel.push_back(code);
    }
    // Il primario governa il pannello proprietà: si tiene allineato all'ultimo.
    const int p = m_multiSel.empty() ? -1 : m_multiSel.back();
    if (p <= -6000) { m_selStruct = -6000 - p; m_selBox = -1; }
    else            { m_selStruct = -1;        m_selBox = p;  }
    if (m_selBox <= -300 && m_selBox > -400) m_selRoutePt = 0;
    updateViewport();
}

// Posizione di un elemento, se ne ha una. Serve alla rotazione di gruppo (per il
// baricentro) e all'evidenziazione.
bool MapEditor::codePosition(int code, glm::vec3& out) const
{
    auto set = [&](float x, float y, float z) { out = {x, y, z}; return true; };
    if (code <= -6000 && (-6000 - code) < (int)m_structures.size())
    { const auto& s = m_structures[-6000 - code]; return set(s.x, s.y, s.z); }
    if (code >= 0 && code < (int)m_boxes.size())
    { const auto& b = m_boxes[code]; return set(b.x, b.y, b.z); }
    if (code == -2) return set(m_spawnTeam1[0], m_spawnTeam1[1], m_spawnTeam1[2]);
    if (code == -3) return set(m_spawnTeam2[0], m_spawnTeam2[1], m_spawnTeam2[2]);
    if (code <= -10 && code > -100 && (-10 - code) < (int)m_posts.size())
    { const auto& p = m_posts[-10 - code]; return set(p.x, p.y, p.z); }
    if (code <= -200 && code > -300 && (-200 - code) < (int)m_dangers.size())
    { const auto& d = m_dangers[-200 - code]; return set(d.x, d.y, d.z); }
    if (code <= -400 && code > -500 && (-400 - code) < (int)m_vehSpawns.size())
    { const auto& v = m_vehSpawns[-400 - code]; return set(v.x, 0.0f, v.z); }
    if (code <= -500 && code > -1000 && (-500 - code) < (int)m_targets.size())
    { const auto& t = m_targets[-500 - code]; return set(t.x, t.y, t.z); }
    if (code <= -1000 && code > -2000 && (-1000 - code) < (int)m_positions.size())
    { const auto& p = m_positions[-1000 - code]; return set(p.x, p.y, p.z); }
    if (code <= -2000 && code > -3000 && (-2000 - code) < (int)m_sectors.size())
    { const auto& s = m_sectors[-2000 - code]; return set(s.x, 0.0f, s.z); }
    if (code <= -4000 && code > -5000 && (-4000 - code) < (int)m_prefabInsts.size())
    { const auto& p = m_prefabInsts[-4000 - code]; return set(p.x, p.y, p.z); }
    return false;
}

// Orientamento di un elemento, se ne ha uno (nullptr = non ruotabile).
float* MapEditor::codeYaw(int code)
{
    if (code <= -6000 && (-6000 - code) < (int)m_structures.size())
        return &m_structures[-6000 - code].ry;
    if (code >= 0 && code < (int)m_boxes.size())
        return &m_boxes[code].ry;
    if (code <= -400 && code > -500 && (-400 - code) < (int)m_vehSpawns.size())
        return &m_vehSpawns[-400 - code].ry;
    if (code <= -500 && code > -1000 && (-500 - code) < (int)m_targets.size())
        return &m_targets[-500 - code].ry;
    if (code <= -1000 && code > -2000 && (-1000 - code) < (int)m_positions.size())
        return &m_positions[-1000 - code].facing;
    if (code <= -4000 && code > -5000 && (-4000 - code) < (int)m_prefabInsts.size())
        return &m_prefabInsts[-4000 - code].ry;
    return nullptr;
}

// Elimina TUTTI gli elementi selezionati. Si raccolgono gli indici per contenitore
// e si cancella in ordine DECRESCENTE: cancellare in avanti invaliderebbe gli indici
// successivi e si finirebbe per eliminare l'elemento sbagliato.
void MapEditor::deleteSelection()
{
    const auto codes = selectionCodes();
    if (codes.empty()) return;
    pushUndo("elimina selezione");

    std::vector<int> boxes, structs, positions, posts, dangers, targets,
                     sectors, vehicles, prefabs;
    for (int c : codes)
    {
        if      (c >= 0)                          boxes.push_back(c);
        else if (c <= -6000)                      structs.push_back(-6000 - c);
        else if (c <= -4000 && c > -5000)         prefabs.push_back(-4000 - c);
        else if (c <= -2000 && c > -3000)         sectors.push_back(-2000 - c);
        else if (c <= -1000 && c > -2000)         positions.push_back(-1000 - c);
        else if (c <= -500  && c > -1000)         targets.push_back(-500 - c);
        else if (c <= -400  && c > -500)          vehicles.push_back(-400 - c);
        else if (c <= -200  && c > -300)          dangers.push_back(-200 - c);
        else if (c <= -10   && c > -100)          posts.push_back(-10 - c);
        // spawn, route e comandante restano fuori: sono singoli o liste di punti,
        // e "eliminarli in blocco" non è un'operazione sensata.
    }
    auto eraseAll = [](auto& vec, std::vector<int>& idx) {
        std::sort(idx.begin(), idx.end(), std::greater<int>());
        idx.erase(std::unique(idx.begin(), idx.end()), idx.end());
        for (int i : idx) if (i >= 0 && i < (int)vec.size()) vec.erase(vec.begin() + i);
    };
    eraseAll(m_boxes, boxes);            eraseAll(m_structures, structs);
    eraseAll(m_prefabInsts, prefabs);    eraseAll(m_sectors, sectors);
    eraseAll(m_positions, positions);    eraseAll(m_targets, targets);
    eraseAll(m_vehSpawns, vehicles);     eraseAll(m_dangers, dangers);
    eraseAll(m_posts, posts);

    m_multiSel.clear();
    m_selBox = -1; m_selStruct = -1;
    m_dirty = true;
    rebuildStructurePreview();
    updateViewport();
}

// Sposta di `delta` l'elemento identificato dal CODICE di selezione.
// Estratta dalla catena di `tick` perché ora serve a più chiamanti (uno per elemento
// selezionato): con la selezione multipla la stessa logica andava ripetuta, ed è il
// tipo di duplicazione che diverge al primo tipo aggiunto.
void MapEditor::applyMove(int code, const glm::vec3& delta)
{
    {
        // Le STRUTTURE si spostano come tutto il resto (ADR-053). Il gizmo muove la
        // RICETTA e i box si rigenerano: è il motivo per cui una scala si sposta
        // intera, gradini compresi, invece che uno alla volta.
        if (code <= -6000 && (-6000 - code) < (int)m_structures.size())
        {
            auto& s = m_structures[-6000 - code];
            s.x += delta.x; s.y += delta.y; s.z += delta.z;
            rebuildStructurePreview();
        }
        else if (code >= 0 && code < (int)m_boxes.size())
        {
            auto& b = m_boxes[code];
            b.x += delta.x; b.y += delta.y; b.z += delta.z;
        }
        else if (code == -2)
        { m_spawnTeam1[0]+=delta.x; m_spawnTeam1[1]+=delta.y; m_spawnTeam1[2]+=delta.z; }
        else if (code == -3)
        { m_spawnTeam2[0]+=delta.x; m_spawnTeam2[1]+=delta.y; m_spawnTeam2[2]+=delta.z; }
        else if (code <= -10 && code > -100
                 && (-10 - code) < (int)m_posts.size())
        {
            auto& p = m_posts[-10 - code];
            p.x += delta.x; p.y += delta.y; p.z += delta.z;
        }
        else if (code <= -200 && code > -300
                 && (-200 - code) < (int)m_dangers.size())
        {
            auto& d = m_dangers[-200 - code];
            d.x += delta.x; d.y += delta.y; d.z += delta.z;
        }
        else if (code <= -300 && code > -400
                 && (-300 - code) < (int)m_routes.size())
        {
            auto& r = m_routes[-300 - code];
            if (m_selRoutePt >= 0 && m_selRoutePt < (int)r.points.size())
            {
                r.points[m_selRoutePt][0] += delta.x;
                r.points[m_selRoutePt][1] += delta.y;
                r.points[m_selRoutePt][2] += delta.z;
            }
        }
        else if (code <= -400 && code > -500
                 && (-400 - code) < (int)m_vehSpawns.size())
        {
            auto& v = m_vehSpawns[-400 - code];
            v.x += delta.x; v.z += delta.z;
        }
        else if (code <= -500 && code > -1000
                 && (-500 - code) < (int)m_targets.size())
        {
            auto& t = m_targets[-500 - code];
            t.x += delta.x; t.y += delta.y; t.z += delta.z;
            if (t.y < 0.0f) t.y = 0.0f;   // non sotto il suolo
        }
        else if (code <= -2000 && (-2000 - code) < (int)m_sectors.size())   // ADR-034
        {
            auto& s = m_sectors[-2000 - code];
            s.x += delta.x; s.z += delta.z;
        }
        else if (code <= -1000 && code > -2000
                 && (-1000 - code) < (int)m_positions.size())   // ADR-030
        {
            auto& p = m_positions[-1000 - code];
            p.x += delta.x; p.y += delta.y; p.z += delta.z;
        }
        else if (code <= -3000 && code > -3100
                 && (-3000 - code) < (int)m_spawnPoints1.size())   // multi-spawn team1
        {
            auto& p = m_spawnPoints1[-3000 - code];
            p[0] += delta.x; p[1] += delta.y; p[2] += delta.z;
        }
        else if (code <= -3100 && code > -3200
                 && (-3100 - code) < (int)m_spawnPoints2.size())   // multi-spawn team2
        {
            auto& p = m_spawnPoints2[-3100 - code];
            p[0] += delta.x; p[1] += delta.y; p[2] += delta.z;
        }
        else if (code <= -4000 && (-4000 - code) < (int)m_prefabInsts.size())
        {   // Istanza di prefab (ADR-048): si sposta l'ISTANZA, il contenuto la segue.
            auto& p = m_prefabInsts[-4000 - code];
            p.x += delta.x; p.y += delta.y; p.z += delta.z;
        }
        else if (code == kSelCommander && m_commander.exists)   // ADR-041
        {
            m_commander.x += delta.x; m_commander.z += delta.z;
        }
    }
}

// Rotazione e scala del gizmo. Restano legate all'elemento PRIMARIO (l'ultimo
// selezionato): ruotare o scalare un gruppo eterogeneo non ha un significato unico —
// un raggio, un'altezza e un `facing` non si scalano allo stesso modo. La rotazione
// DI GRUPPO esiste ed è gestita a parte, sotto, per i soli elementi con posizione.
// ── COSTRUZIONE: TIRARE UNA FACCIA (doc 53 L1) ───────────────────────────────
// Il gesto primario di costruzione in CubeGrid, TrenchBroom e in ogni editor di
// blockout: si afferra una faccia e la si tira. La differenza con la scala è tutta
// qui: **la faccia opposta resta ferma**. Allungare un muro con la scala costa due
// gesti (scala, poi ricentra) e un errore di mezza misura ogni volta; tirando la
// faccia ne costa uno ed è esatto.
//
// L'ingombro è quello della SELEZIONE: con più elementi si tira l'insieme, e ognuno
// cresce della sua parte. È il comportamento di TrenchBroom sull'estrusione multipla.
bool MapEditor::selectionBounds(glm::vec3& mn, glm::vec3& mx) const
{
    bool any = false;
    auto grow = [&](const glm::vec3& c, const glm::vec3& h) {
        const glm::vec3 lo = c - h, hi = c + h;
        if (!any) { mn = lo; mx = hi; any = true; return; }
        mn.x = std::min(mn.x, lo.x); mn.y = std::min(mn.y, lo.y); mn.z = std::min(mn.z, lo.z);
        mx.x = std::max(mx.x, hi.x); mx.y = std::max(mx.y, hi.y); mx.z = std::max(mx.z, hi.z);
    };
    for (int code : selectionCodes())
    {
        if (code >= 0 && code < (int)m_boxes.size())
        {
            const auto& b = m_boxes[code];
            // Ingombro CON la rotazione: ignorarla darebbe maniglie fuori posto
            // proprio sui muri storti, che sono quelli su cui serve di più.
            const float a = b.ry * 3.14159265f / 180.0f;
            const float c = std::fabs(std::cos(a)), s = std::fabs(std::sin(a));
            grow({b.x, b.y, b.z}, { (b.sx*0.5f)*c + (b.sz*0.5f)*s, b.sy*0.5f,
                                    (b.sx*0.5f)*s + (b.sz*0.5f)*c });
        }
        else if (code <= -6000 && (-6000 - code) < (int)m_structures.size())
        {
            std::vector<mini::MapGeometryBox> boxes;
            expandStructureAt(-6000 - code, boxes);
            for (const auto& b : boxes)
                grow({b.x, b.y, b.z}, {b.sx*0.5f, b.sy*0.5f, b.sz*0.5f});
        }
    }
    return any;
}

void MapEditor::applyFaceDrag()
{
    int face = 0; float delta = 0.0f;
    if (!m_viewport.popGizmoFaceDelta(face, delta)) return;
    applyFaceDelta(face, delta);
}

// Separata da chi la invoca perché è la parte con l'invariante da collaudare — la
// faccia opposta non si muove — e senza mouse non si può collaudare un trascinamento.
void MapEditor::applyFaceDelta(int face, float delta)
{
    if (face < 0 || face > 5) return;
    if (std::fabs(delta) < 0.0001f) return;

    const int axis = face / 2;              // 0=X 1=Y 2=Z
    const float sign = (face % 2 == 0) ? -1.0f : 1.0f;   // -X/-Y/-Z oppure +X/+Y/+Z

    // Una voce di annullamento per gesto: la coalescenza di `pushUndo` fonde i delta
    // consecutivi con la stessa etichetta, come per il gizmo.
    pushUndo("tira faccia");
    bool touched = false;

    for (int code : selectionCodes())
    {
        if (code >= 0 && code < (int)m_boxes.size())
        {
            auto& b = m_boxes[code];
            float* size = (axis == 0) ? &b.sx : (axis == 1) ? &b.sy : &b.sz;
            float* pos  = (axis == 0) ? &b.x  : (axis == 1) ? &b.y  : &b.z;
            // La faccia si sposta di `delta` verso l'esterno: la dimensione cresce
            // di `delta` e il centro si sposta di metà, così l'altra faccia non si
            // muove di un millimetro.
            float ns = *size + delta;
            if (ns < 0.05f) ns = 0.05f;      // un box a spessore zero sparisce dal navmesh
            const float applied = ns - *size;
            *size = ns;
            *pos += sign * applied * 0.5f;
            touched = true;
        }
        else if (code <= -6000 && (-6000 - code) < (int)m_structures.size())
        {
            // Su una struttura parametrica si tira la MISURA giusta, non i box:
            // una scala allungata resta una scala a norma (ADR-053). Riusa la
            // stessa regola del gizmo di scala — una seconda regola qui darebbe
            // due comportamenti diversi per lo stesso gesto.
            glm::vec3 d{0.0f};
            d[axis] = delta;
            scalePrimitivePart(m_structures[-6000 - code], d);
            touched = true;
        }
    }
    if (!touched) return;
    m_dirty = true;
    rebuildStructurePreview();
    updateViewport(!m_viewport.gizmoDragging());
}

// Il box appena disegnato col trascinamento: l'impronta viene dal gesto, l'altezza
// dal campo "Altezza" della barra. Due numeri decisi prima, uno disegnato: è il
// motivo per cui un muro costa un gesto invece di sei.
void MapEditor::createDrawnBox(const glm::vec3& mn, const glm::vec3& mx)
{
    pushUndo("disegna box");
    BoxEntry b;
    b.sx = mx.x - mn.x;
    b.sz = mx.z - mn.z;
    b.sy = (m_drawHeight > 0.05f) ? m_drawHeight : 0.05f;
    b.x  = (mn.x + mx.x) * 0.5f;
    b.z  = (mn.z + mx.z) * 0.5f;
    // La Y di un box è il CENTRO: il piano di lavoro è la sua BASE, quindi mezza
    // altezza sopra. Sbagliarlo interra metà muro — ed è la convenzione che ha già
    // prodotto "piccole differenze inspiegabili" (nota nel pannello delle parti).
    b.y  = m_drawPlaneY + b.sy * 0.5f;
    std::snprintf(b.type, sizeof(b.type), "%s", "wall");
    m_boxes.push_back(b);
    m_multiSel.clear();
    m_selBox    = (int)m_boxes.size() - 1;
    m_selStruct = -1;
    m_dirty = true;
    updateViewport();
}

// ── PRECISIONE (doc 53 L2) ───────────────────────────────────────────────────
void MapEditor::moveSelectionBy(const glm::vec3& d)
{
    if (selectionCodes().empty()) return;
    pushUndo("sposta di");
    for (int code : selectionCodes()) applyMove(code, d);
    m_dirty = true;
    rebuildStructurePreview();
    updateViewport();
}

// Allinea: porta tutti gli elementi allo stesso bordo (min/centro/max) sull'asse.
// Fatto a mano costa una lettura di coordinate per elemento e un errore ogni tanto;
// e un muro disallineato di 3 cm produce una fessura che il navmesh non attraversa.
void MapEditor::alignSelection(int axis, int mode)
{
    const auto codes = selectionCodes();
    if (codes.size() < 2 || axis < 0 || axis > 2) return;

    // Il bersaglio si calcola sui bordi, non sui centri: allineare "a filo muro" è
    // ciò che serve costruendo, e con i soli centri due box di spessore diverso
    // restano sfalsati.
    auto extent = [&](int code, float& lo, float& hi) -> bool {
        glm::vec3 p;
        if (!codePosition(code, p)) return false;
        float half = 0.0f;
        if (code >= 0 && code < (int)m_boxes.size())
        {
            const auto& b = m_boxes[code];
            half = (axis == 0) ? b.sx * 0.5f : (axis == 1) ? b.sy * 0.5f : b.sz * 0.5f;
        }
        lo = p[axis] - half; hi = p[axis] + half;
        return true;
    };

    float tgt = 0.0f; bool any = false;
    for (int code : codes)
    {
        float lo, hi;
        if (!extent(code, lo, hi)) continue;
        const float v = (mode == 0) ? lo : (mode == 2) ? hi : (lo + hi) * 0.5f;
        if (!any) { tgt = v; any = true; }
        else if (mode == 0) tgt = std::min(tgt, v);
        else if (mode == 2) tgt = std::max(tgt, v);
        else                tgt += v;
    }
    if (!any) return;
    if (mode == 1)
    {
        int n = 0;
        for (int code : codes) { float a, b2; if (extent(code, a, b2)) ++n; }
        if (n > 0) tgt /= (float)n;
    }

    pushUndo("allinea");
    for (int code : codes)
    {
        float lo, hi;
        if (!extent(code, lo, hi)) continue;
        const float v = (mode == 0) ? lo : (mode == 2) ? hi : (lo + hi) * 0.5f;
        glm::vec3 d{0.0f};
        d[axis] = tgt - v;
        applyMove(code, d);
    }
    m_dirty = true;
    rebuildStructurePreview();
    updateViewport();
}

// Distribuisci: spazio uguale fra gli elementi sull'asse, tenendo fermi i due
// estremi. Serve per pilastri, finestre, barricate in fila.
void MapEditor::distributeSelection(int axis)
{
    auto codes = selectionCodes();
    if (codes.size() < 3 || axis < 0 || axis > 2) return;

    std::vector<std::pair<float,int>> ord;
    for (int code : codes)
    { glm::vec3 p; if (codePosition(code, p)) ord.push_back({ p[axis], code }); }
    if (ord.size() < 3) return;
    std::sort(ord.begin(), ord.end(),
              [](const auto& a, const auto& b) { return a.first < b.first; });

    const float a = ord.front().first, b = ord.back().first;
    const float stepv = (b - a) / (float)(ord.size() - 1);
    pushUndo("distribuisci");
    for (std::size_t i = 1; i + 1 < ord.size(); ++i)
    {
        glm::vec3 d{0.0f};
        d[axis] = (a + stepv * (float)i) - ord[i].first;
        applyMove(ord[i].second, d);
    }
    m_dirty = true;
    rebuildStructurePreview();
    updateViewport();
}

// ── APPOGGIA / ACCOSTA (doc 53 L2) ───────────────────────────────────────────
// Sposta la selezione lungo un asse finché tocca la geometria che le sta davanti,
// senza compenetrare e senza lasciare fessura. È il gesto che l'occhio non sa fare:
// "sembra appoggiato" a 3 cm da terra è indistinguibile da appoggiato, e quei 3 cm
// stanno sotto la soglia di erosione del navmesh — la superficie sparisce e non si
// capisce perché. Con l'aggancio alla griglia il problema si sposta ma non muore:
// due box costruiti su griglie diverse (o uno ruotato) restano sfalsati.
//
// Si muove come CORPO UNICO, non elemento per elemento: spostare ognuno fino al suo
// contatto smonterebbe la forma di ciò che si è selezionato.
int MapEditor::snapSelectionToSurface(int axis, int dir)
{
    if (axis < 0 || axis > 2 || (dir != 1 && dir != -1)) return 0;
    glm::vec3 selMin, selMax;
    if (!selectionBounds(selMin, selMax)) return 0;

    // Indici della selezione, per non misurare la distanza da sé stessi.
    const auto codes = selectionCodes();
    std::vector<int> selBoxes, selStructs;
    for (int c : codes)
    {
        if (c >= 0) selBoxes.push_back(c);
        else if (c <= -6000) selStructs.push_back(-6000 - c);
    }
    auto isSelBox = [&](int i) {
        return std::find(selBoxes.begin(), selBoxes.end(), i) != selBoxes.end();
    };

    // Tutti gli ostacoli candidati, in coordinate mondo.
    struct AABB { float mn[3], mx[3]; };
    std::vector<AABB> obstacles;
    auto addBox = [&](float x, float y, float z, float sx, float sy, float sz, float ry) {
        const float a = ry * 3.14159265f / 180.0f;
        const float c = std::fabs(std::cos(a)), s = std::fabs(std::sin(a));
        const float hx = (sx * 0.5f) * c + (sz * 0.5f) * s;
        const float hz = (sx * 0.5f) * s + (sz * 0.5f) * c;
        AABB b;
        b.mn[0] = x - hx; b.mx[0] = x + hx;
        b.mn[1] = y - sy * 0.5f; b.mx[1] = y + sy * 0.5f;
        b.mn[2] = z - hz; b.mx[2] = z + hz;
        obstacles.push_back(b);
    };
    for (int i = 0; i < (int)m_boxes.size(); ++i)
    {
        if (isSelBox(i)) continue;
        const auto& b = m_boxes[i];
        addBox(b.x, b.y, b.z, b.sx, b.sy, b.sz, b.ry);
    }
    for (int i = 0; i < (int)m_structures.size(); ++i)
    {
        if (std::find(selStructs.begin(), selStructs.end(), i) != selStructs.end()) continue;
        std::vector<mini::MapGeometryBox> bx;
        expandStructureAt(i, bx);
        for (const auto& b : bx) addBox(b.x, b.y, b.z, b.sx, b.sy, b.sz, b.ry);
    }
    // Il SUOLO conta come superficie: appoggiare a terra è il caso più frequente,
    // e senza di esso "Appoggia giù" non farebbe niente su una mappa vuota.
    if (axis == 1 && dir < 0)
    { AABB g; g.mn[0] = -1e6f; g.mx[0] = 1e6f; g.mn[1] = -1e6f; g.mx[1] = 0.0f;
      g.mn[2] = -1e6f; g.mx[2] = 1e6f; obstacles.push_back(g); }

    const int u = (axis + 1) % 3, v = (axis + 2) % 3;   // i due assi trasversali
    const float selLo[3] = { selMin.x, selMin.y, selMin.z };
    const float selHi[3] = { selMax.x, selMax.y, selMax.z };

    float best = 0.0f; bool found = false;
    for (const auto& o : obstacles)
    {
        // Deve stare DAVANTI sulla proiezione trasversale, altrimenti non lo si
        // incontrerebbe muovendosi: un box di fianco non è una superficie d'appoggio.
        if (o.mx[u] <= selLo[u] + 0.001f || o.mn[u] >= selHi[u] - 0.001f) continue;
        if (o.mx[v] <= selLo[v] + 0.001f || o.mn[v] >= selHi[v] - 0.001f) continue;
        // Distanza da percorrere per arrivare a contatto, nel verso richiesto.
        const float d = (dir > 0) ? (o.mn[axis] - selHi[axis])
                                  : (o.mx[axis] - selLo[axis]);
        if (dir > 0 ? (d < -0.001f) : (d > 0.001f)) continue;   // è dietro
        if (!found || std::fabs(d) < std::fabs(best)) { best = d; found = true; }
    }
    if (!found || std::fabs(best) < 0.0001f) return 0;

    pushUndo("appoggia");
    glm::vec3 delta{0.0f};
    delta[axis] = best;
    for (int c : codes) applyMove(c, delta);
    m_dirty = true;
    rebuildStructurePreview();
    updateViewport();
    return (int)codes.size();
}

void MapEditor::applyGizmoRotateScale()
{
    // Gizmo Ruota (solo asse Y): box mappa (ry), cover point (facing), veicolo (ry).
    // ADR-025: i marker metadata con un campo di orientamento sono ruotabili.
    glm::vec3 rotDelta;
    if (m_viewport.popGizmoRotDelta(rotDelta))
    {
        auto wrap = [](float a) { while (a > 180.0f) a -= 360.0f; while (a < -180.0f) a += 360.0f; return a; };
        // ── ROTAZIONE DI GRUPPO (G3) ──────────────────────────────────────
        // Con più elementi selezionati la rotazione deve far ORBITARE ognuno
        // attorno al baricentro comune, oltre a girarlo su sé stesso. Applicare
        // solo il proprio yaw farebbe "girare sul posto" ogni pezzo, ed è
        // esattamente ciò che NON serve quando si ruota un edificio.
        if (selectionCodes().size() > 1)
        {
            pushUndo("gizmo-rot-gruppo");
            glm::vec3 c(0.0f); int n = 0;
            for (int code : selectionCodes())
            { glm::vec3 p; if (codePosition(code, p)) { c += p; ++n; } }
            if (n > 0)
            {
                c /= (float)n;
                const float rad = rotDelta.y * 3.14159265f / 180.0f;
                const float cs = std::cos(rad), sn = std::sin(rad);
                for (int code : selectionCodes())
                {
                    glm::vec3 p;
                    if (!codePosition(code, p)) continue;
                    const float dx = p.x - c.x, dz = p.z - c.z;
                    // Stessa convenzione di rotazione dei prefab e delle strutture.
                    const glm::vec3 moved = { c.x + dx * cs + dz * sn - p.x, 0.0f,
                                              c.z - dx * sn + dz * cs - p.z };
                    applyMove(code, moved);
                    if (float* yaw = codeYaw(code)) *yaw = wrap(*yaw + rotDelta.y);
                }
                rebuildStructurePreview();
                m_dirty = true; updateViewport(!m_viewport.gizmoDragging());
            }
            return;
        }
        // Struttura: ruota la RICETTA, e con lei la direzione di salita. Una scala
        // ruotata resta una scala — è il vantaggio di orientare l'intento invece
        // che quindici box.
        if (m_selStruct >= 0 && m_selStruct < (int)m_structures.size())
        {
            auto& s = m_structures[m_selStruct];
            s.ry = wrap(s.ry + rotDelta.y);
            pushUndo("gizmo-rot");
            m_dirty = true; rebuildStructurePreview(); updateViewport(!m_viewport.gizmoDragging());
        }
        else if (m_selBox >= 0 && m_selBox < (int)m_boxes.size())
        {
            auto& b = m_boxes[m_selBox];
            b.ry = wrap(b.ry + rotDelta.y);
            m_dirty = true; updateViewport(!m_viewport.gizmoDragging());
        }
        else if (m_selBox <= -400 && m_selBox > -500
                 && (-400 - m_selBox) < (int)m_vehSpawns.size())
        {
            auto& v = m_vehSpawns[-400 - m_selBox];
            v.ry = wrap(v.ry + rotDelta.y);
            m_dirty = true; updateViewport(!m_viewport.gizmoDragging());
        }
        else if (m_selBox <= -500 && m_selBox > -1000
                 && (-500 - m_selBox) < (int)m_targets.size())
        {
            auto& t = m_targets[-500 - m_selBox];
            t.ry = wrap(t.ry + rotDelta.y);
            m_dirty = true; updateViewport(!m_viewport.gizmoDragging());
        }
        else if (m_selBox <= -1000 && m_selBox > -2000
                 && (-1000 - m_selBox) < (int)m_positions.size())   // ADR-030
        {
            auto& p = m_positions[-1000 - m_selBox];
            p.facing = wrap(p.facing + rotDelta.y);
            m_dirty = true; updateViewport(!m_viewport.gizmoDragging());
        }
        else if (m_selBox <= -4000 && (-4000 - m_selBox) < (int)m_prefabInsts.size())
        {   // Istanza di prefab (ADR-048): ruota tutto il contenuto con sé.
            auto& p = m_prefabInsts[-4000 - m_selBox];
            p.ry = wrap(p.ry + rotDelta.y);
            m_dirty = true; updateViewport(!m_viewport.gizmoDragging());
        }
    }

    // Gizmo Scala: box (dimensioni per asse), post/danger (raggio uniforme).
    // ADR-025: i marker metadata con un raggio sono scalabili (delta.x → radius).
    glm::vec3 scaleDelta;
    if (m_viewport.popGizmoScaleDelta(scaleDelta))
    {
        // Struttura: la scala del gizmo agisce sui PARAMETRI che hanno senso per il
        // tipo, non su un box. X = larghezza (scala/rampa) o lunghezza (muro),
        // Y = dislivello o altezza, Z = profondità della piattaforma.
        // I GRADINI NON SI ROMPONO: allargare una scala non tocca l'alzata, e
        // alzarla aggiunge gradini invece di renderli più ripidi.
        if (m_selStruct >= 0 && m_selStruct < (int)m_structures.size())
        {
            auto& s = m_structures[m_selStruct];
            auto grow = [](float& v, float d, float lo) { v += d; if (v < lo) v = lo; };
            switch (s.kind)
            {
            case mini::StructureKind::Stair:
            case mini::StructureKind::Ramp:
                grow(s.width, scaleDelta.x, 0.5f);
                grow(s.rise,  scaleDelta.y, 0.1f);
                grow(s.tread, scaleDelta.z, mini::mapmetrics::STAIR_TREAD);
                break;
            case mini::StructureKind::Wall:
                grow(s.length,    scaleDelta.x, 0.5f);
                grow(s.height,    scaleDelta.y, 0.2f);
                grow(s.thickness, scaleDelta.z, 0.05f);
                break;
            case mini::StructureKind::Platform:
                grow(s.sizeX, scaleDelta.x, 1.0f);
                grow(s.y,     scaleDelta.y, s.baseY + 0.1f);
                grow(s.sizeZ, scaleDelta.z, 1.0f);
                break;
            }
            pushUndo("gizmo-scale");
            m_dirty = true; rebuildStructurePreview(); updateViewport(!m_viewport.gizmoDragging());
        }
        else if (m_selBox >= 0 && m_selBox < (int)m_boxes.size())
        {
            auto& b = m_boxes[m_selBox];
            b.sx += scaleDelta.x; if (b.sx < 0.1f) b.sx = 0.1f;
            b.sy += scaleDelta.y; if (b.sy < 0.1f) b.sy = 0.1f;
            b.sz += scaleDelta.z; if (b.sz < 0.1f) b.sz = 0.1f;
            m_dirty = true; updateViewport(!m_viewport.gizmoDragging());
        }
        else if (m_selBox <= -10 && m_selBox > -100
                 && (-10 - m_selBox) < (int)m_posts.size())
        {
            auto& p = m_posts[-10 - m_selBox];
            p.radius += scaleDelta.x; if (p.radius < 0.5f) p.radius = 0.5f;
            m_dirty = true; updateViewport(!m_viewport.gizmoDragging());
        }
        else if (m_selBox <= -200 && m_selBox > -300
                 && (-200 - m_selBox) < (int)m_dangers.size())
        {
            auto& d = m_dangers[-200 - m_selBox];
            d.radius += scaleDelta.x; if (d.radius < 0.5f) d.radius = 0.5f;
            m_dirty = true; updateViewport();
        }
        else if (m_selBox <= -2000 && (-2000 - m_selBox) < (int)m_sectors.size())   // ADR-034
        {
            auto& s = m_sectors[-2000 - m_selBox];
            s.radius += scaleDelta.x; if (s.radius < 2.0f) s.radius = 2.0f;
            m_dirty = true; updateViewport();
        }
        else if (m_selBox <= -500 && m_selBox > -1000
                 && (-500 - m_selBox) < (int)m_targets.size())
        {
            auto& t = m_targets[-500 - m_selBox];
            t.scale += scaleDelta.x * 0.4f; if (t.scale < 0.2f) t.scale = 0.2f;
            m_dirty = true; updateViewport();
        }
        else if (m_selBox == kSelCommander && m_commander.exists)   // ADR-041: raggio leash
        {
            m_commander.leashRadius += scaleDelta.x;
            if (m_commander.leashRadius < 0.0f) m_commander.leashRadius = 0.0f;
            m_dirty = true; updateViewport();
        }
    }
}

// ── snap ─────────────────────────────────────────────────────────────────────
float MapEditor::snap(float v) const
{
    if (m_gridSnap <= 0.0f) return v;
    return std::round(v / m_gridSnap) * m_gridSnap;
}

// ── loadMaps ─────────────────────────────────────────────────────────────────
void MapEditor::loadMaps()
{
    m_mapList.clear();
    fs::path folder = getDataDir() + "/maps";
    if (!fs::exists(folder)) return;
    for (auto& entry : fs::directory_iterator(folder))
    {
        if (entry.path().extension() != ".json") continue;
        std::ifstream f(entry.path());
        if (!f) continue;
        json j;
        try { f >> j; } catch (...) { continue; }
        MapEntry me;
        me.id   = entry.path().stem().string();   // ADR-001: MAI dal contenuto (KI #21/#84)
        me.path = entry.path().string();
        m_mapList.push_back(me);
    }
}

// ── loadMap ──────────────────────────────────────────────────────────────────
void MapEditor::loadMap(const std::string& id)
{
    auto it = std::find_if(m_mapList.begin(), m_mapList.end(),
                           [&](const MapEntry& e){ return e.id == id; });
    if (it == m_mapList.end()) return;

    std::ifstream f(it->path);
    if (!f) return;
    json j;
    try { f >> j; } catch (...) { return; }

    m_mapId      = id;
    m_mapJsonPath = it->path;
    // Il nome visualizzato NON si carica più in un campo editabile: è l'id (2026-08-02).
    // Esisteva una casella separata che lo cambiava senza rinominare il file, creando
    // due nomi divergenti per la stessa mappa.
    m_boxes.clear();
    m_posts.clear();
    m_positions.clear();
    m_sectors.clear();
    m_dangers.clear();
    m_routes.clear();
    m_vehSpawns.clear();
    m_targets.clear();
    m_spawnPoints1.clear();
    m_spawnPoints2.clear();
    m_prefabInsts.clear();
    m_selRoutePt = 0;
    m_selBox = -1;

    // Istanze di prefab (ADR-048): si carica il RIFERIMENTO, non i dati espansi —
    // quelli li rigenera il motore al load, così aggiornare il prefab aggiorna tutte
    // le istanze e non possono diventare stale.
    if (j.contains("prefabs") && j["prefabs"].is_array())
        for (auto& pi : j["prefabs"])
        {
            if (!pi.contains("id")) continue;
            PrefabInstEntry e;
            e.id = pi["id"].get<std::string>();
            e.x  = pi.value("x", 0.0f); e.y = pi.value("y", 0.0f);
            e.z  = pi.value("z", 0.0f); e.ry = pi.value("ry", 0.0f);
            m_prefabInsts.push_back(std::move(e));
        }

    if (j.contains("spawn_team1") && j["spawn_team1"].size() >= 3)
        m_spawnTeam1 = {j["spawn_team1"][0], j["spawn_team1"][1], j["spawn_team1"][2]};
    if (j.contains("spawn_team2") && j["spawn_team2"].size() >= 3)
        m_spawnTeam2 = {j["spawn_team2"][0], j["spawn_team2"][1], j["spawn_team2"][2]};
    // Multi-spawn (opzionale): array di punti [x,y,z] per fazione.
    for (const char* key : {"spawn_points_team1", "spawn_points_team2"})
    {
        if (!j.contains(key) || !j[key].is_array()) continue;
        auto& out = (std::string(key).back() == '1') ? m_spawnPoints1 : m_spawnPoints2;
        for (auto& pt : j[key])
            if (pt.is_array() && pt.size() >= 3)
                out.push_back({(float)pt[0], (float)pt[1], (float)pt[2]});
    }

    // Comandante strategico (ADR-024/041): uno per mappa, campo `commander`.
    m_commander = CommanderEntry{};
    if (j.contains("commander") && j["commander"].is_object())
    {
        auto& c = j["commander"];
        m_commander.exists      = true;
        m_commander.unit        = c.value("unit", std::string());
        m_commander.x           = c.value("x", 0.0f);
        m_commander.z           = c.value("z", 0.0f);
        m_commander.leashRadius = c.value("leash_radius", 0.0f);
    }

    // Primitive parametriche (ADR-053): si legge la RICETTA. I box che ne derivano
    // si rigenerano subito sotto e non stanno in `m_boxes` — non vanno né salvati né
    // modificati a mano, altrimenti si perde il legame con la ricetta.
    m_structures.clear();
    m_selStruct = -1;
    if (j.contains("structures") && j["structures"].is_array())
    {
        for (auto& s : j["structures"])
        {
            // UNA sola lettura della ricetta, condivisa col registry
            // (mini::structjson). Qui c'era il secondo lettore, e gli mancava il
            // campo : una composita perdeva il legame col tipo e al
            // salvataggio successivo la perdita diventava permanente.
            m_structures.push_back(mini::structjson::fromJson(s));
        }
    }

    if (j.contains("geometry") && j["geometry"].is_array())
    {
        for (auto& gb : j["geometry"])
        {
            // I box derivati da una primitiva non si salvano MAI (ADR-033/053).
            // Se ne trovo uno in un file scritto male, lo scarto: altrimenti al
            // prossimo salvataggio verrebbe congelato accanto a quello rigenerato.
            if (gb.value("from_structure", false)) continue;
            BoxEntry b;
            b.x  = gb.value("x",  0.f);
            b.y  = gb.value("y",  0.f);
            b.z  = gb.value("z",  0.f);
            b.ry = gb.value("ry", 0.f);
            b.sx = gb.value("sx", 2.f);
            b.sy = gb.value("sy", 2.f);
            b.sz = gb.value("sz", 2.f);
            b.r  = gb.value("r",  0.35f);
            b.g  = gb.value("g",  0.32f);
            b.b  = gb.value("b",  0.28f);
            b.isCollider = gb.value("collider", true);

            std::string type  = gb.value("type",  std::string("wall"));
            std::string label = gb.value("label", std::string(""));
            std::strncpy(b.type,  type.c_str(),  sizeof(b.type)  - 1);
            std::strncpy(b.label, label.c_str(), sizeof(b.label) - 1);

            m_boxes.push_back(b);
        }
    }

    if (j.contains("command_posts") && j["command_posts"].is_array())
    {
        for (auto& cp : j["command_posts"])
        {
            PostEntry p;
            std::string lbl = cp.value("label", std::string("Post"));
            std::strncpy(p.label, lbl.c_str(), sizeof(p.label) - 1);
            p.x           = cp.value("x", 0.f);
            p.y           = cp.value("y", 0.f);
            p.z           = cp.value("z", 0.f);
            p.radius      = cp.value("radius", 4.f);
            p.team        = cp.value("team", 0);
            p.captureTime = cp.value("capture_time", 8.f);
            m_posts.push_back(p);
        }
    }

    // Bersagli strategici (doc 25, DestroyTarget)
    if (j.contains("strategic_targets") && j["strategic_targets"].is_array())
    {
        for (auto& st : j["strategic_targets"])
        {
            TargetEntry t;
            std::string lbl = st.value("label", std::string("Bersaglio"));
            std::strncpy(t.label, lbl.c_str(), sizeof(t.label) - 1);
            t.x  = st.value("x", 0.f);
            t.z  = st.value("z", 0.f);
            t.y  = st.value("y", 0.f);   // altezza sopra il suolo (0 = a terra)
            t.hp = st.value("hp", 300.f);
            t.ry    = st.value("ry", 0.f);
            t.team  = st.value("team", 2);
            t.scale = st.value("mesh_scale", 1.f);
            t.halfX = st.value("half_x", 0.f);
            t.halfY = st.value("half_y", 0.f);
            t.halfZ = st.value("half_z", 0.f);
            // Ruolo (doc 34): "comms" = torre di comunicazione della sua fazione.
            const std::string rl = st.value("role", std::string("generic"));
            t.role = (rl == "comms") ? 1 : (rl == "control") ? 2 : 0;
            t.priority     = st.value("priority", 0.5f);       // doc 35
            t.engageRadius = st.value("engage_radius", 0.0f);
            m_targets.push_back(t);
        }
    }

    // ── Map Metadata (15_MapMetadata) ────────────────────────────────────
    // Posizioni tattiche (ADR-030): chiave nuova + MIGRAZIONE delle due legacy.
    // Salvando si riscrive solo `tactical_positions` e le legacy spariscono.
    auto readPos = [&](const nlohmann::json& p, const char* roleKey,
                       const char* defRole, float defProtection)
    {
        PositionEntry e;
        e.x          = p.value("x", 0.f);
        e.y          = p.value("y", 0.5f);
        e.z          = p.value("z", 0.f);
        e.facing     = p.value("facing_deg", 0.f);
        e.role       = p.value(roleKey, std::string(defRole));
        if (e.role.empty()) e.role = defRole;
        e.height     = p.value("height", 1.f);
        e.protection = p.value("protection", defProtection);
        e.canShoot   = p.value("can_shoot", true);
        e.importance = p.value("importance", 0.5f);
        e.radius     = p.value("radius", 4.f);
        e.fireArc    = p.value("fire_arc_deg", 120.f);   // ADR-031
        e.fireRange  = p.value("fire_range", 25.f);
        m_positions.push_back(e);
    };
    if (j.contains("tactical_positions") && j["tactical_positions"].is_array())
        for (auto& p : j["tactical_positions"]) readPos(p, "role", "cover", 0.5f);
    if (j.contains("cover_points") && j["cover_points"].is_array())
        for (auto& p : j["cover_points"]) readPos(p, "role", "cover", 0.5f);
    if (j.contains("tactical_points") && j["tactical_points"].is_array())
        for (auto& p : j["tactical_points"]) readPos(p, "type", "vantage", 0.0f);

    if (j.contains("sectors") && j["sectors"].is_array())   // ADR-034
        for (auto& s : j["sectors"])
        {
            SectorEntry e;
            e.label      = s.value("label", std::string("Settore"));
            e.x          = s.value("x", 0.f);
            e.z          = s.value("z", 0.f);
            e.radius     = s.value("radius", 12.f);
            e.importance = s.value("importance", 0.5f);
            m_sectors.push_back(e);
        }
    if (j.contains("danger_zones") && j["danger_zones"].is_array())
    {
        for (auto& dz : j["danger_zones"])
        {
            DangerEntry d;
            d.x      = dz.value("x", 0.f);
            d.y      = dz.value("y", 0.f);
            d.z      = dz.value("z", 0.f);
            d.radius = dz.value("radius", 4.f);
            d.level  = dz.value("danger_level", 0.5f);
            m_dangers.push_back(d);
        }
    }
    if (j.contains("patrol_routes") && j["patrol_routes"].is_array())
    {
        for (auto& pr : j["patrol_routes"])
        {
            RouteEntry r;
            std::string rid = pr.value("id", std::string("route"));
            std::strncpy(r.id, rid.c_str(), sizeof(r.id) - 1);
            if (pr.contains("points") && pr["points"].is_array())
                for (auto& pt : pr["points"])
                    if (pt.is_array() && pt.size() >= 3)
                        r.points.push_back({(float)pt[0], (float)pt[1], (float)pt[2]});
            m_routes.push_back(std::move(r));
        }
    }

    if (j.contains("vehicle_spawns") && j["vehicle_spawns"].is_array())
    {
        for (auto& vs : j["vehicle_spawns"])
        {
            VehicleSpawnEntry v;
            v.vehicleId = vs.value("vehicle_id", std::string(""));
            v.x  = vs.value("x", 0.f);
            v.z  = vs.value("z", 0.f);
            v.ry = vs.value("ry", 0.f);
            m_vehSpawns.push_back(std::move(v));
        }
    }

    m_dirty = false;
    // La pila di undo appartiene al DOCUMENTO: tenerla fra due mappe diverse
    // significherebbe poter "annullare" la mappa A dentro la mappa B.
    m_hist.clear();
    rebuildStructurePreview();   // ADR-053: i box derivati esistono solo qui
    updateViewport();
}

// ── saveMap ───────────────────────────────────────────────────────────────────
bool MapEditor::saveMap(const std::string& overridePath)
{
    // Il percorso alternativo serve al salvataggio automatico, che scrive la STESSA
    // serializzazione altrove. Un secondo scrittore per la copia di recupero avrebbe
    // salvato qualcosa di diverso dal file vero — cioè un recupero che non recupera.
    const std::string target = overridePath.empty() ? m_mapJsonPath : overridePath;
    if (target.empty()) return false;

    // saveJsonRMW (ADR-010): unico canale di scrittura JSON dell'editor.
    return editor::jsonsave::saveJsonRMW(target, [&](json& j) {
    j.erase("id"); // deprecato: id = nome file (ADR-001)
    // Nome visualizzato: se vuoto usa l'id, così il campo non resta un residuo
    // vecchio (era la causa del "il rename non cambia il nome", 2026-07-21).
    // UN SOLO NOME (2026-08-02): il nome visualizzato è l'id. Non esiste più un modo
    // separato di cambiarlo — si rinomina, e cambia ovunque. Se un file vecchio aveva
    // un `name` divergente, il primo salvataggio lo riallinea.
    j["name"] = m_mapId;
    j["spawn_team1"] = {m_spawnTeam1[0], m_spawnTeam1[1], m_spawnTeam1[2]};
    j["spawn_team2"] = {m_spawnTeam2[0], m_spawnTeam2[1], m_spawnTeam2[2]};
    // Multi-spawn: scrivi l'array se ci sono punti, altrimenti RIMUOVI il campo (RMW:
    // niente residuo che distribuirebbe comunque le AI).
    auto writeSpawnPts = [&](const char* key, const std::vector<std::array<float,3>>& pts) {
        if (pts.empty()) { j.erase(key); return; }
        json arr = json::array();
        for (const auto& p : pts) arr.push_back({p[0], p[1], p[2]});
        j[key] = arr;
    };
    writeSpawnPts("spawn_points_team1", m_spawnPoints1);
    writeSpawnPts("spawn_points_team2", m_spawnPoints2);

    // Istanze di prefab (ADR-048): si scrivono SOLO i riferimenti. I box e le posizioni
    // che ne derivano NON vanno mai salvati: sono dati derivati, e scriverli
    // significherebbe congelare una copia che diverge appena il prefab cambia.
    if (m_prefabInsts.empty()) j.erase("prefabs");
    else
    {
        json arr = json::array();
        for (const auto& p : m_prefabInsts)
            arr.push_back({{"id", p.id}, {"x", p.x}, {"y", p.y}, {"z", p.z}, {"ry", p.ry}});
        j["prefabs"] = arr;
    }

    // Primitive parametriche (ADR-053): si scrive SOLO la ricetta, mai i box che ne
    // derivano — stessa disciplina dei prefab poco sopra.
    if (m_structures.empty()) j.erase("structures");
    else
    {
        // UNA sola scrittura della ricetta, condivisa col lettore
        // (mini::structjson): un campo aggiunto di la' compare qui senza che
        // nessuno debba ricordarsene.
        json arr = json::array();
        for (const auto& st : m_structures) arr.push_back(mini::structjson::toJson(st));
        j["structures"] = arr;
    }

    json geom = json::array();
    for (const auto& b : m_boxes)
    {
        json gb;
        gb["x"]        = b.x;  gb["y"]  = b.y;  gb["z"]  = b.z;
        gb["ry"]       = b.ry;
        gb["sx"]       = b.sx; gb["sy"] = b.sy; gb["sz"] = b.sz;
        gb["r"]        = b.r;  gb["g"]  = b.g;  gb["b"]  = b.b;
        gb["type"]     = b.type;
        gb["label"]    = b.label;
        gb["collider"] = b.isCollider;
        geom.push_back(gb);
    }
    j["geometry"] = geom;

    json postsArr = json::array();
    for (const auto& p : m_posts)
    {
        json cp;
        cp["label"]        = p.label;
        cp["x"] = p.x;  cp["y"] = p.y;  cp["z"] = p.z;
        cp["radius"]       = p.radius;
        cp["team"]         = p.team;
        cp["capture_time"] = p.captureTime;
        postsArr.push_back(cp);
    }
    j["command_posts"] = postsArr;

    // Comandante strategico (ADR-024/041): uno per mappa. Se non è autorato si
    // RIMUOVE il campo (RMW: non lasciare un residuo che spawnerebbe comunque).
    if (m_commander.exists && !m_commander.unit.empty())
    {
        json c;
        c["unit"] = m_commander.unit;
        c["x"] = m_commander.x;  c["z"] = m_commander.z;
        c["leash_radius"] = m_commander.leashRadius;
        j["commander"] = c;
    }
    else j.erase("commander");

    // Bersagli strategici (doc 25, DestroyTarget)
    json targetsArr = json::array();
    for (const auto& t : m_targets)
    {
        json st;
        st["label"] = t.label;
        st["x"] = t.x;  st["z"] = t.z;
        st["y"] = t.y;                  // altezza sopra il suolo (0 = a terra)
        st["hp"] = t.hp;
        st["ry"]         = t.ry;
        st["team"]       = t.team;
        st["mesh_scale"] = t.scale;
        st["half_x"] = t.halfX;  st["half_y"] = t.halfY;  st["half_z"] = t.halfZ;
        st["role"]   = (t.role == 1) ? "comms"            // doc 34
                     : (t.role == 2) ? "control"          // doc 36
                                     : "generic";
        st["priority"]      = t.priority;                 // doc 35
        st["engage_radius"] = t.engageRadius;
        targetsArr.push_back(st);
    }
    j["strategic_targets"] = targetsArr;

    // ── Map Metadata (15_MapMetadata) ────────────────────────────────────
    // Posizioni tattiche unificate (ADR-030). Si scrive SOLO la chiave nuova e si
    // cancellano le legacy: aprire+salvare una mappa la migra definitivamente.
    json posArr = json::array();
    for (const auto& p : m_positions)
    {
        json o;
        o["x"] = p.x;  o["y"] = p.y;  o["z"] = p.z;
        o["facing_deg"] = p.facing;
        o["role"]       = p.role;
        o["height"]     = p.height;
        o["protection"] = p.protection;
        o["can_shoot"]  = p.canShoot;
        o["importance"] = p.importance;
        o["radius"]     = p.radius;
        o["fire_arc_deg"] = p.fireArc;     // ADR-031
        o["fire_range"]   = p.fireRange;
        posArr.push_back(o);
    }
    j["tactical_positions"] = posArr;
    j.erase("cover_points");
    j.erase("tactical_points");

    json secArr = json::array();   // ADR-034
    for (const auto& s : m_sectors)
    {
        json o;
        o["label"]      = s.label;
        o["x"] = s.x;  o["z"] = s.z;
        o["radius"]     = s.radius;
        o["importance"] = s.importance;
        secArr.push_back(o);
    }
    j["sectors"] = secArr;

    json dangerArr = json::array();
    for (const auto& d : m_dangers)
    {
        json dz;
        dz["x"] = d.x;  dz["y"] = d.y;  dz["z"] = d.z;
        dz["radius"]       = d.radius;
        dz["danger_level"] = d.level;
        dangerArr.push_back(dz);
    }
    j["danger_zones"] = dangerArr;

    json routeArr = json::array();
    for (const auto& r : m_routes)
    {
        json pr;
        pr["id"] = r.id;
        json pts = json::array();
        for (const auto& pt : r.points)
            pts.push_back({pt[0], pt[1], pt[2]});
        pr["points"] = pts;
        routeArr.push_back(pr);
    }
    j["patrol_routes"] = routeArr;

    json vehArr = json::array();
    for (const auto& v : m_vehSpawns)
    {
        json vs;
        vs["vehicle_id"] = v.vehicleId;
        vs["x"] = v.x;  vs["z"] = v.z;  vs["ry"] = v.ry;
        vehArr.push_back(vs);
    }
    j["vehicle_spawns"] = vehArr;

    m_dirty = false;
    return true;
    });
}

// ── addBox ────────────────────────────────────────────────────────────────────
void MapEditor::addBox()
{
    pushUndo("addBox");
    BoxEntry b;
    // Nasce davanti alla camera (dove stai guardando), non al centro mappa.
    const glm::vec3 fp = m_viewport.groundFocusPoint();
    b.x = snap(fp.x); b.y = 1.0f; b.z = snap(fp.z);
    std::strncpy(b.type, "wall", sizeof(b.type) - 1);
    std::strncpy(b.label, "Nuovo Box", sizeof(b.label) - 1);
    m_boxes.push_back(b);
    setSelection((int)m_boxes.size() - 1, /*additive=*/false);   // vedi addStructure
    m_dirty  = true;
    updateViewport();
}

// ── duplicateBox ─────────────────────────────────────────────────────────────
// Restituisce l'INDICE della copia. Non è un dettaglio: la copia viene inserita
// ACCANTO all'originale (così nella lista sta vicino a lui), quindi *non* è
// l'ultima del vettore — e chi lo dava per scontato spostava un'altra box.
int MapEditor::duplicateBox(int idx)
{
    if (idx < 0 || idx >= (int)m_boxes.size()) return -1;
    BoxEntry b = m_boxes[idx];
    b.x += 1.0f;
    m_boxes.insert(m_boxes.begin() + idx + 1, b);
    m_selBox = idx + 1;
    m_dirty  = true;
    updateViewport();
    return idx + 1;
}

// ── duplicateSelected (F4, doc 39) ────────────────────────────────────────────
// Duplica l'elemento selezionato QUALUNQUE sia il tipo, copiando TUTTI i campi
// autorati (ruolo/arco/gittata di una posizione, raggio di un settore, ecc.):
// l'authoring dei metadata era laborioso perché ogni nuovo elemento partiva dai
// default e andava ri-regolato. Ora si autora una volta e si duplica in serie.
// Copia spostata di +2 in XZ per non sovrapporre. Spawn e comandante (unici) no.
int MapEditor::duplicateOne(int code)
{
    const float off = 2.0f;
    if (code >= 0 && code < (int)m_boxes.size())
    { return duplicateBox(code); }
    else if (code <= -10 && code > -100)
    {
        int i = -10 - code;
        if (i < 0 || i >= (int)m_posts.size()) return -1;
        PostEntry p = m_posts[i]; p.x += off; p.z += off;
        m_posts.push_back(p);
        m_selBox = -10 - ((int)m_posts.size() - 1);
    }
    else if (code <= -200 && code > -300)
    {
        int i = -200 - code;
        if (i < 0 || i >= (int)m_dangers.size()) return -1;
        DangerEntry d = m_dangers[i]; d.x += off; d.z += off;
        m_dangers.push_back(d);
        m_selBox = -200 - ((int)m_dangers.size() - 1);
    }
    else if (code <= -300 && code > -400)
    {
        int i = -300 - code;
        if (i < 0 || i >= (int)m_routes.size()) return -1;
        RouteEntry r = m_routes[i];
        std::snprintf(r.id, sizeof(r.id), "route_%d", (int)m_routes.size() + 1);
        for (auto& pt : r.points) { pt[0] += off; pt[2] += off; }
        m_routes.push_back(std::move(r));
        m_selBox = -300 - ((int)m_routes.size() - 1);
        m_selRoutePt = 0;
    }
    else if (code <= -400 && code > -500)
    {
        int i = -400 - code;
        if (i < 0 || i >= (int)m_vehSpawns.size()) return -1;
        VehicleSpawnEntry v = m_vehSpawns[i]; v.x += off; v.z += off;
        m_vehSpawns.push_back(v);
        m_selBox = -400 - ((int)m_vehSpawns.size() - 1);
    }
    else if (code <= -500 && code > -1000)
    {
        int i = -500 - code;
        if (i < 0 || i >= (int)m_targets.size()) return -1;
        TargetEntry t = m_targets[i]; t.x += off; t.z += off;
        m_targets.push_back(t);
        m_selBox = -500 - ((int)m_targets.size() - 1);
    }
    else if (code <= -1000 && code > -2000)
    {
        int i = -1000 - code;
        if (i < 0 || i >= (int)m_positions.size()) return -1;
        PositionEntry p = m_positions[i]; p.x += off; p.z += off;
        m_positions.push_back(p);
        m_selBox = -1000 - ((int)m_positions.size() - 1);
    }
    else if (code <= -6000 && (-6000 - code) < (int)m_structures.size())
    {   // Struttura parametrica: si duplica la RICETTA, i box si rigenerano.
        mini::StructureDef s = m_structures[-6000 - code];
        s.x += off; s.z += off;
        m_structures.push_back(s);
        m_selStruct = (int)m_structures.size() - 1;
        rebuildStructurePreview();
    }
    else if (code <= -2000 && code > -3000)
    {
        int i = -2000 - code;
        if (i < 0 || i >= (int)m_sectors.size()) return -1;
        SectorEntry s = m_sectors[i]; s.x += off; s.z += off;
        m_sectors.push_back(s);
        m_selBox = -2000 - ((int)m_sectors.size() - 1);
    }
    else return -1;   // spawn team1/2 e comandante: unici, non duplicabili

    m_dirty = true;
    // Ogni ramo non-box ha appena scritto il codice della copia su `m_selBox`
    // (le strutture su `m_selStruct`): è quello il codice da restituire. Chi duplica
    // deve SAPERE dove è finita la copia, non dedurlo dalla dimensione del vettore —
    // era proprio quella deduzione a far spostare l'elemento sbagliato.
    if (code <= -6000) return -6000 - ((int)m_structures.size() - 1);
    return m_selBox;
}

// ── ARRAY: N copie con offset progressivo (doc 47 E4) ────────────────────────
// La differenza con "Duplica" ripetuto è che l'offset è **progressivo**: la copia
// i-esima sta a i × offset dall'originale, quindi una fila resta allineata invece
// di accumulare l'errore di dodici trascinamenti a mano.
void MapEditor::makeArray()
{
    const auto codes = selectionCodes();
    if (codes.empty() || m_arrayCount < 1) return;
    pushUndo("array");

    // Codici CORRENTI degli originali. Non sono costanti: inserire una copia in
    // mezzo a `m_boxes` fa scalare di uno tutti gli indici superiori, quindi i
    // codici raccolti all'inizio puntano altrove già dalla seconda copia. Si
    // aggiornano dopo ogni inserimento — è lo stesso motivo per cui
    // `deleteSelection` cancella in ordine decrescente.
    std::vector<int> cur = codes;

    for (int k = 1; k <= m_arrayCount; ++k)
    {
        const glm::vec3 off = { m_arrayOff[0] * (float)k,
                                m_arrayOff[1] * (float)k,
                                m_arrayOff[2] * (float)k };
        const float yaw = m_arrayYawStep * (float)k;
        for (std::size_t ci = 0; ci < cur.size(); ++ci)
        {
            const int c = cur[ci];
            // `duplicateOne` mette la copia a +2/+2 di default: la si riporta
            // sull'originale e poi si applica l'offset voluto, così l'unico
            // spostamento è quello dichiarato.
            //
            // Il codice della copia lo DICE `duplicateOne`. Prima lo si deduceva
            // ("sarà l'ultima del vettore"), ma le box si inseriscono ACCANTO
            // all'originale: la deduzione puntava a un'altra box, che veniva
            // spostata e ruotata al posto della copia. Non una funzione inerte —
            // una funzione che danneggiava un elemento sano (segnalato dall'utente).
            const int newCode = duplicateOne(c);
            if (newCode == -1) continue;
            glm::vec3 src, dst;
            if (codePosition(c, src) && codePosition(newCode, dst))
                applyMove(newCode, src + off - dst);
            if (yaw != 0.0f) if (float* y = codeYaw(newCode)) *y += yaw;

            // Una box inserita all'indice `newCode` fa salire di uno ogni box con
            // indice >= newCode. I codici negativi (post, posizioni, strutture...)
            // si accodano sempre, quindi non ne sono toccati.
            if (newCode >= 0)
                for (int& x : cur) if (x >= newCode) ++x;
        }
    }
    m_multiSel.clear();
    m_dirty = true;
    rebuildStructurePreview();
    updateViewport();
}

bool MapEditor::filtersActive() const
{
    for (bool b : m_showType) if (!b) return true;
    // Anche i marcatori nascosti contano: l'asterisco esiste perché una mappa che
    // sembra vuota si spieghi da sé invece di far cercare l'elemento perduto.
    return m_hideAboveY < 999.0f || !m_showStructures
        || !m_showPositions || !m_showAreas || !m_showRoutes || !m_showGamePoints;
}

// Duplica TUTTA la selezione (G3).
// ATTENZIONE: le copie NON si accodano tutte. Una box viene inserita ACCANTO al suo
// originale, quindi ogni indice superiore scala di uno e i codici raccolti prima del
// ciclo puntano all'elemento sbagliato dalla seconda copia in poi. Il commento che
// stava qui affermava il contrario ed era falso: si duplicavano box diverse da quelle
// selezionate.
void MapEditor::duplicateSelected()
{
    const auto codes = selectionCodes();
    if (codes.empty()) return;
    pushUndo("duplica");
    std::vector<int> cur = codes;
    for (std::size_t i = 0; i < cur.size(); ++i)
    {
        const int nc = duplicateOne(cur[i]);
        if (nc >= 0)
            for (std::size_t j = i + 1; j < cur.size(); ++j)
                if (cur[j] >= nc) ++cur[j];
    }
    // La selezione NON segue le copie: restare sugli originali renderebbe il
    // secondo "duplica" una copia della copia, che non è mai ciò che si vuole.
    // Si tiene il primario sull'ultima copia creata, come nel caso singolo.
    m_multiSel.clear();
    m_dirty = true;
    rebuildStructurePreview();
    updateViewport();
}

// ── deleteBox ─────────────────────────────────────────────────────────────────
void MapEditor::deleteBox(int idx)
{
    if (idx < 0 || idx >= (int)m_boxes.size()) return;
    pushUndo("elimina");
    m_boxes.erase(m_boxes.begin() + idx);
    m_selBox = std::min(m_selBox, (int)m_boxes.size() - 1);
    m_dirty  = true;
    updateViewport();
}

// ── savePrefabFromZone (ADR-048) ─────────────────────────────────────────────
// Promuove ad ASSET ciò che è stato costruito nella mappa: prende i box e le posizioni
// tattiche entro un raggio dal focus del viewport, li converte in coordinate LOCALI e
// scrive `data/prefabs/<id>.json`. È il flusso naturale dell'authoring — si costruisce
// il pezzo dove lo si vede, poi lo si rende riusabile — e senza di esso il sistema
// prefab sarebbe monco: si potevano piazzare solo asset scritti a mano nel JSON.
// Cosa finirebbe nel prefab ORA. UNA sola funzione, usata sia dall'anteprima nel
// viewport sia dal salvataggio: se fossero due, l'utente vedrebbe evidenziata una cosa
// e ne otterrebbe un'altra — il tipo di divergenza che questo progetto paga sempre caro.
// Regola: il RAGGIO fa la selezione grossolana, i ritocchi manuali (Shift+click) la
// correggono; una volta toccato a mano, comanda la lista manuale.
void MapEditor::prefabZoneCollect(std::vector<int>& boxes,
                                  std::vector<int>& positions) const
{
    boxes.clear(); positions.clear();
    if (m_prefabPickManual)
    {
        boxes     = m_prefabPickBoxes;
        positions = m_prefabPickPositions;
        return;
    }
    const float r2 = m_newPrefabRadius * m_newPrefabRadius;
    for (int i = 0; i < (int)m_boxes.size(); ++i)
    {
        const float dx = m_boxes[i].x - m_prefabZoneX, dz = m_boxes[i].z - m_prefabZoneZ;
        if (dx*dx + dz*dz <= r2) boxes.push_back(i);
    }
    for (int i = 0; i < (int)m_positions.size(); ++i)
    {
        const float dx = m_positions[i].x - m_prefabZoneX, dz = m_positions[i].z - m_prefabZoneZ;
        if (dx*dx + dz*dz <= r2) positions.push_back(i);
    }
}

bool MapEditor::savePrefabFromZone(std::string& err)
{
    const std::string id = m_newPrefabId;
    if (id.empty()) { err = "Serve un nome (id = nome del file, ADR-001)."; return false; }
    for (char c : id)
        if (!(std::isalnum((unsigned char)c) || c == ' ' || c == '_' || c == '-'))
        { err = "Usa lettere, numeri, spazio, _ o -"; return false; }
    if (m_prefabReg.getPrefab(id))
    { err = "Esiste gia' un prefab con questo nome."; return false; }

    // Centro CONGELATO all'apertura del popup, non il focus corrente: se seguisse la
    // telecamera, ciò che era evidenziato cambierebbe mentre si compila il nome.
    const glm::vec3 c{m_prefabZoneX, 0.0f, m_prefabZoneZ};

    // STESSA selezione mostrata nel viewport (raggio + ritocchi manuali).
    std::vector<int> boxIdx, posIdx;
    prefabZoneCollect(boxIdx, posIdx);

    nlohmann::json coll = nlohmann::json::array();
    nlohmann::json tact = nlohmann::json::array();
    for (int i : boxIdx)
    {
        const auto& b = m_boxes[i];
        coll.push_back({{"x", b.x - c.x}, {"y", b.y}, {"z", b.z - c.z}, {"ry", b.ry},
                        {"sx", b.sx}, {"sy", b.sy}, {"sz", b.sz},
                        {"collider", b.isCollider},
                        {"r", b.r}, {"g", b.g}, {"b", b.b}});
    }
    for (int i : posIdx)
    {
        const auto& p = m_positions[i];
        tact.push_back({{"x", p.x - c.x}, {"y", p.y}, {"z", p.z - c.z},
                        {"facing_deg", p.facing}, {"role", p.role},
                        {"height", p.height}, {"protection", p.protection},
                        {"can_shoot", p.canShoot}, {"importance", p.importance},
                        {"radius", p.radius},
                        {"fire_arc_deg", p.fireArc}, {"fire_range", p.fireRange}});
    }
    if (coll.empty() && tact.empty())
    { err = "Nessun elemento selezionato."; return false; }

    nlohmann::json pj;
    pj["name"]      = id;
    pj["collision"] = coll;
    pj["tactical"]  = tact;

    const std::string path = (fs::path(getDataDir()) / "prefabs" / (id + ".json")).string();
    std::error_code ec;
    fs::create_directories(fs::path(getDataDir()) / "prefabs", ec);
    if (!editor::jsonsave::saveJsonRMW(path, [&](nlohmann::json& j) { j = pj; return true; }))
    { err = "Scrittura fallita: " + path; return false; }

    // Ricarica il registry: il nuovo prefab dev'essere subito piazzabile.
    // Stesso punto usato dall'eliminazione: una via sola per stare allineati al disco.
    reloadPrefabAssets();

    if (m_newPrefabConsume)
    {
        // Gli elementi assorbiti escono dalla mappa e tornano come ISTANZA: così non
        // restano duplicati (una copia "cotta" nella mappa + una dal prefab) che poi
        // divergerebbero. Si rimuove dal fondo per non invalidare gli indici.
        for (auto it = boxIdx.rbegin(); it != boxIdx.rend(); ++it)
            m_boxes.erase(m_boxes.begin() + *it);
        for (auto it = posIdx.rbegin(); it != posIdx.rend(); ++it)
            m_positions.erase(m_positions.begin() + *it);
        PrefabInstEntry inst;
        inst.id = id; inst.x = c.x; inst.y = 0.0f; inst.z = c.z; inst.ry = 0.0f;
        m_prefabInsts.push_back(inst);
        m_selBox = -4000 - ((int)m_prefabInsts.size() - 1);
    }
    m_newPrefabId[0] = '\0';
    m_prefabZoneMode = false;         // esce dalla modalità selezione
    m_prefabPickManual = false;
    m_prefabPickBoxes.clear(); m_prefabPickPositions.clear();
    m_dirty = true;
    updateViewport();
    return true;
}

// ── recomputeExposure (ADR-033) ───────────────────────────────────────────────
// Costruisce un MapDef temporaneo dai dati dell'editor e chiama la STESSA funzione
// del runtime: la regola dell'esposizione esiste in un posto solo. Si ricalcola a
// ogni modifica (updateViewport), non a ogni frame.
void MapEditor::recomputeExposure()
{
    mini::MapDef tmp;
    tmp.geometry.reserve(m_boxes.size());
    for (const auto& b : m_boxes)
    {
        mini::MapGeometryBox g;
        g.x = b.x; g.y = b.y; g.z = b.z; g.ry = b.ry;
        g.sx = b.sx; g.sy = b.sy; g.sz = b.sz;
        g.collider = b.isCollider;
        tmp.geometry.push_back(g);
    }
    tmp.tacticalPositions.reserve(m_positions.size());
    for (const auto& p : m_positions)
    {
        mini::TacticalPositionDef t;
        t.x = p.x; t.y = p.y; t.z = p.z;
        t.facingDeg = p.facing; t.role = p.role;
        t.height = p.height; t.protection = p.protection; t.canShoot = p.canShoot;
        t.importance = p.importance; t.radius = p.radius;
        t.fireArcDeg = p.fireArc; t.fireRange = p.fireRange;
        tmp.tacticalPositions.push_back(t);
    }
    mini::worldintel::buildTacticalLinks(tmp);
    m_exposure = tmp.positionExposure;

    // ── VISUALE VERTICALE (KI #83) ───────────────────────────────────────────
    // Quante posizioni a QUOTA DIVERSA vede ciascuna posizione, con lo stesso modello
    // di combattimento del runtime: origine agli OCCHI, bersaglio al CORPO
    // ([[combat-los-eye-height]]), e la stessa `hasLineOfFire`.
    // A cosa serve: è stato misurato che l'AI ingaggia tutto ciò che vede a quota
    // diversa e spara — il collo di bottiglia è che quasi non VEDE, perché le
    // piattaforme piene occludono verso il basso oltre il proprio bordo. Questo numero
    // rende quel limite visibile QUI, mentre si autora, invece che a tentativi in
    // partita: 0 = posizione elevata inutile per battere chi sta sotto.
    // La LOS si calcola solo sulle coppie cross-quota (il filtro sul dislivello viene
    // PRIMA), quindi il costo resta una frazione di quello dei link.
    // Copertura dall'alto (doc 41 B3): stessa funzione del runtime, calcolata sul
    // MapDef temporaneo → l'editor e il gioco dicono la stessa cosa.
    m_overhead.assign(m_positions.size(), 0);
    for (size_t i = 0; i < tmp.tacticalPositions.size() && i < m_overhead.size(); ++i)
    {
        const auto& p = tmp.tacticalPositions[i];
        m_overhead[i] = mini::worldintel::hasOverheadCover(
            tmp, p.x, p.y, p.z, mini::config::OVERHEAD_PROBE_HEIGHT) ? 1 : 0;
    }

    m_vertSight.assign(m_positions.size(), 0);
    m_vertPairs.assign(m_positions.size(), 0);
    for (size_t i = 0; i < tmp.tacticalPositions.size(); ++i)
    {
        const auto& a = tmp.tacticalPositions[i];
        for (size_t j = 0; j < tmp.tacticalPositions.size(); ++j)
        {
            if (i == j) continue;
            const auto& b = tmp.tacticalPositions[j];
            if (std::fabs(a.y - b.y) <= mini::config::VERTICAL_ENGAGE_DY) continue;
            ++m_vertPairs[i];
            if (mini::worldintel::hasLineOfFire(
                    tmp, a.x, a.y + mini::config::COMBAT_EYE_HEIGHT, a.z,
                         b.x, b.y + mini::config::AI_HALF_Y,          b.z))
                ++m_vertSight[i];
        }
    }

    // ── SALUTE TATTICA (doc 41 B4) ───────────────────────────────────────────
    // Aggrega in un elenco unico i difetti che prima si scoprivano solo ispezionando
    // un elemento per volta. Non inventa regole nuove: usa i dati derivati già
    // calcolati qui sopra (grafo, esposizione, visuale verticale) + due controlli di
    // coerenza. Ogni voce porta il codice di selezione → è cliccabile.
    // Regole CONDIVISE col gate `--validate` (ADR-018): stanno in ContentValidation,
    // mai duplicate qui — due copie divergerebbero al primo ritocco di soglia.
    // I settori vanno copiati nel MapDef temporaneo, altrimenti la regola "settore
    // senza posizioni" non avrebbe nulla su cui lavorare.
    tmp.sectors.reserve(m_sectors.size());
    for (const auto& s : m_sectors)
    {
        mini::SectorDef sd;
        sd.label = s.label; sd.x = s.x; sd.z = s.z;
        sd.radius = s.radius; sd.importance = s.importance;
        tmp.sectors.push_back(sd);
    }
    m_issues.clear();
    for (const auto& d : mini::analyzeTacticalHealth(tmp))
    {
        // Codice di selezione per banda: geometria = indice diretto in `m_boxes`
        // (tmp.geometry è costruito da lì, stesso ordine), posizioni −1000, settori −2000.
        const int sel =
              (d.target == mini::TacticalDefect::Target::Position) ? (-1000 - d.index)
            : (d.target == mini::TacticalDefect::Target::Geometry) ? d.index
            :                                                        (-2000 - d.index);
        m_issues.push_back({sel, d.severity, (int)d.kind, d.text});
    }
    // Difetto specifico dell'EDITOR: la visuale verticale è un dato che vive qui
    // (m_vertSight/m_vertPairs), quindi la regola resta qui — è l'unica.
    for (size_t i = 0; i < m_vertPairs.size() && i < m_vertSight.size(); ++i)
        if (m_vertPairs[i] > 0 && m_vertSight[i] == 0)
        {
            char buf[160];
            std::snprintf(buf, sizeof(buf), "[%s %d] cieca verso le altre quote (0/%d)",
                          i < tmp.tacticalPositions.size()
                              ? tmp.tacticalPositions[i].role.c_str() : "pos",
                          (int)i + 1, m_vertPairs[i]);
            m_issues.push_back({-1000 - (int)i, 1,
                                (int)mini::TacticalDefect::Kind::BlindVertical, buf});
        }
    std::stable_sort(m_issues.begin(), m_issues.end(),
                     [](const TacticalIssue& a, const TacticalIssue& b) { return a.sev > b.sev; });
}

// ── VALIDAZIONE NAVMESH (doc 47) ─────────────────────────────────────────────
// Costruisce il navmesh VERO, con lo stesso `NavManager` del motore, sullo stato
// che stai editando ADESSO — box a mano **più** i box generati dalle primitive,
// perché è quello che il gioco caricherà.
//
// Perché serve, in una riga: il gate sui dati non può vedere la voxelizzazione.
// Su Training Ground dice "0 problemi" mentre un intero recinto è irraggiungibile
// (KI #97): rampe da 1,50 m che, fra sfoltimento dei cigli, erosione del raggio
// agente e area minima di regione, non diventano navmesh. L'unico modo di
// accorgersene prima di giocare è **guardarlo**.
std::size_t MapEditor::geometryFingerprint() const
{
    // Hash grossolano ma sufficiente: conta e coordinate di tutto ciò che entra
    // nel navmesh. Non deve essere crittografico, deve solo cambiare quando la
    // mappa cambia.
    std::size_t h = m_boxes.size() * 1315423911u + m_structPreview.size() * 2654435761u;
    auto mix = [&h](float v) {
        const auto q = (std::size_t)(long long)(v * 100.0f);
        h ^= q + 0x9e3779b97f4a7c15ull + (h << 6) + (h >> 2);
    };
    for (const auto& b : m_boxes)
    { mix(b.x); mix(b.y); mix(b.z); mix(b.sx); mix(b.sy); mix(b.sz); mix(b.ry);
      mix(b.isCollider ? 1.0f : 0.0f); }
    for (const auto& g : m_structPreview)
    { mix(g.x); mix(g.y); mix(g.z); mix(g.sx); mix(g.sy); mix(g.sz); mix(g.ry); }
    for (float v : m_spawnTeam1) mix(v);
    return h;
}

void MapEditor::validateNavmesh()
{
    const auto t0 = std::chrono::steady_clock::now();
    m_navReport = NavReport{};
    m_navTris.clear();

    // Stato corrente → MapDef, compresi i box DERIVATI dalle primitive.
    mini::MapDef tmp;
    tmp.geometry.reserve(m_boxes.size() + m_structPreview.size());
    for (const auto& b : m_boxes)
    {
        mini::MapGeometryBox g;
        g.x = b.x; g.y = b.y; g.z = b.z; g.ry = b.ry;
        g.sx = b.sx; g.sy = b.sy; g.sz = b.sz;
        g.collider = b.isCollider;
        g.type = mini::parseBoxType(b.type);
        tmp.geometry.push_back(g);
    }
    for (const auto& g : m_structPreview) tmp.geometry.push_back(g);
    tmp.spawnTeam1 = m_spawnTeam1;
    tmp.spawnTeam2 = m_spawnTeam2;

    const mini::NavBuildStats st = m_nav.build(tmp);
    m_navReport.polys = st.polyCount;
    m_navBuilt = st.ok;
    m_navStale = false;
    if (!st.ok)
    {
        m_navReport.buildSeconds =
            std::chrono::duration<float>(std::chrono::steady_clock::now() - t0).count();
        m_viewport.clearNavMesh();
        return;
    }

    std::vector<mini::NavManager::DebugTri> tris;
    int nComp = 0;
    m_nav.debugTriangles(tris, &nComp);
    m_navReport.components = nComp;

    // La componente dello SPAWN alleato è il continente; tutto il resto è isola.
    // Non "la più grande": ciò che conta è da dove si parte davvero.
    const glm::vec3 spawn = {m_spawnTeam1[0], m_spawnTeam1[1], m_spawnTeam1[2]};
    m_navReport.mainComponent = m_nav.componentAt(spawn);

    // Dimensione e posizione di ogni isola: senza, "10 isole" non si può usare —
    // dieci schegge da tre triangoli e dieci stanze scollegate sono due mappe
    // diversissime. Si accumula per componente e si ordina per dimensione.
    std::vector<std::pair<int, glm::vec3>> acc(nComp > 0 ? nComp : 0, {0, glm::vec3(0.0f)});
    std::vector<float> area(nComp > 0 ? nComp : 0, 0.0f);
    // I triangoli di ogni isola, per chiedere a `navcheck` quanta della sua
    // superficie sta SOTTO un ostacolo. Stessa funzione di `--navcheck`: se la
    // classificazione vivesse in due posti, l'editor e la riga di comando
    // direbbero cose diverse sulla stessa mappa.
    std::vector<std::vector<std::array<glm::vec3,3>>> isleTris(nComp > 0 ? nComp : 0);

    m_navTris.reserve(tris.size());
    for (const auto& t : tris)
    {
        const bool ok = (t.component == m_navReport.mainComponent);
        if (!ok) ++m_navReport.islandTris;
        if (!ok && t.component >= 0 && t.component < (int)acc.size())
        {
            acc[t.component].first += 1;
            acc[t.component].second += (t.a + t.b + t.c) / 3.0f;
            area[t.component] += 0.5f * glm::length(glm::cross(t.b - t.a, t.c - t.a));
            isleTris[t.component].push_back({t.a, t.b, t.c});
        }
        FreeCameraViewport::NavTriDraw d;
        d.ax = t.a.x; d.ay = t.a.y; d.az = t.a.z;
        d.bx = t.b.x; d.by = t.b.y; d.bz = t.b.z;
        d.cx = t.c.x; d.cy = t.c.y; d.cz = t.c.z;
        if (ok) { d.r = 0.25f; d.g = 0.85f; d.b = 0.40f; }   // si arriva
        else    { d.r = 0.95f; d.g = 0.30f; d.b = 0.25f; }   // isola
        m_navTris.push_back(d);
    }
    for (std::size_t c = 0; c < acc.size(); ++c)
        if (acc[c].first > 0)
        {
            NavReport::Island isl;
            isl.tris   = acc[c].first;
            isl.area   = area[c];
            isl.center = acc[c].second / (float)acc[c].first;
            // Pavimento chiuso sotto un cubo: navmesh legittimo e irraggiungibile,
            // ma NON un difetto da correggere. Distinguerlo evita di mandare
            // l'autore a caccia di un problema che non c'è (osservazione
            // dell'utente su Warfare Ground: *"erano semplicemente il terreno che
            // stava sotto dei cubi"*).
            isl.covered = mini::navcheck::coveredFraction(tmp.geometry, isleTris[c]);
            m_navReport.islands.push_back(isl);
        }
    std::sort(m_navReport.islands.begin(), m_navReport.islands.end(),
              [](const auto& x, const auto& y) { return x.area > y.area; });

    // Elementi autorati che il navmesh non raggiunge. Sono i due che contano:
    // una posizione tattica irraggiungibile è lavoro sprecato **e** un buco
    // invisibile; un command post irraggiungibile è un obiettivo incatturabile.
    for (int i = 0; i < (int)m_positions.size(); ++i)
    {
        const auto& p = m_positions[i];
        if (!m_nav.isReachable(spawn, {p.x, p.y, p.z})) m_navReport.badPositions.push_back(i);
    }
    for (int i = 0; i < (int)m_posts.size(); ++i)
    {
        const auto& p = m_posts[i];
        if (!m_nav.isReachable(spawn, {p.x, p.y, p.z})) m_navReport.badPosts.push_back(i);
    }

    m_navReport.buildSeconds =
        std::chrono::duration<float>(std::chrono::steady_clock::now() - t0).count();
    m_navFingerprint = geometryFingerprint();
    m_showNav = true;
    updateViewport(/*recomputeDerived=*/false);
}

// I tetti derivano DIRETTAMENTE dagli intervalli usati in `codePosition`/`codeYaw`/
// `applyMove`: post `-10…-100`, pericoli `-200…-300`, percorsi `-300…-400`, veicoli
// `-400…-500`, bersagli `-500…-1000`, posizioni `-1000…-2000`, settori `-2000…-3000`,
// multi-spawn `-3000/-3100`, prefab `-4000…-5000`. Se un intervallo cambia, questa
// tabella va cambiata con lui — il collaudo lo verifica.
std::vector<MapEditor::CapacityInfo> MapEditor::capacityReport() const
{
    return {
        { "command post",       (int)m_posts.size(),        90   },
        { "zone di pericolo",   (int)m_dangers.size(),      100  },
        { "percorsi",           (int)m_routes.size(),       100  },
        { "spawn veicoli",      (int)m_vehSpawns.size(),    100  },
        { "bersagli",           (int)m_targets.size(),      500  },
        { "posizioni tattiche", (int)m_positions.size(),    1000 },
        { "settori",            (int)m_sectors.size(),      1000 },
        { "multi-spawn T1",     (int)m_spawnPoints1.size(), 100  },
        { "multi-spawn T2",     (int)m_spawnPoints2.size(), 100  },
        { "istanze prefab",     (int)m_prefabInsts.size(),  1000 },
    };
}

// ── SALVATAGGIO AUTOMATICO DI RECUPERO (doc 51) ─────────────────────────────
// FUORI da `data/maps/`: quella cartella viene scandita per costruire l'elenco
// delle mappe, e una copia lì dentro diventerebbe una mappa fantasma selezionabile
// e modificabile per sbaglio.
std::string MapEditor::unsavedSummary() const
{
    std::string s;
    if (m_dirty) s = "la mappa \"" + m_mapId + "\"";
    int n = 0;
    for (const auto& t : m_structTabs) if (t.dirty) ++n;
    if (n > 0)
    {
        if (!s.empty()) s += " e ";
        s += std::to_string(n);
        s += (n == 1) ? " tipo di struttura" : " tipi di struttura";
    }
    return s;
}

std::string MapEditor::autosaveDir()
{
    // `dir()` (con lo slash) e non `root()`: senza, il percorso diventava
    // `.../data_autosave/` — una cartella sorella creata per errore di
    // concatenazione. Dentro `data/` ma FUORI da `data/maps/`, che è il vincolo
    // vero: dentro maps/ una copia diventerebbe una mappa fantasma nell'elenco.
    return editor::datapath::dir() + "_autosave/";
}

void MapEditor::tickAutosave(float dt)
{
    // Solo se c'è davvero qualcosa da perdere e la mappa ha un nome.
    if (!m_dirty || m_mapId.empty()) { m_autosaveTimer = 0.0f; return; }
    m_autosaveTimer += dt;
    if (m_autosaveTimer < kAutosaveSeconds) return;
    m_autosaveTimer = 0.0f;

    std::error_code ec;
    fs::create_directories(autosaveDir(), ec);
    const std::string path = autosaveDir() + m_mapId + ".json";
    if (saveMap(path))
    {
        // NON si azzera `m_dirty`: la copia di recupero non è il salvataggio.
        // Azzerarlo farebbe credere che il lavoro sia al sicuro nel file vero.
        m_lastAutosave = path;
        std::printf("[MapEditor] copia di recupero: %s\n", path.c_str());
    }
}

// ════════════════════════════════════════════════════════════════════════════
// COLLAUDO HEADLESS DELLE OPERAZIONI
// ════════════════════════════════════════════════════════════════════════════
// Perché esiste: "serie" ha spostato per settimane un elemento SBAGLIATO, e se ne
// è accorto l'utente. Il difetto era banale — `makeArray` deduceva il codice della
// copia dalla dimensione del vettore, mentre le box si inseriscono accanto
// all'originale — ma nessuno poteva accorgersene senza mouse.
//
// Non è un framework di test: sono controlli su invarianti che si possono scrivere
// in dieci righe e che, se avessero girato, avrebbero fermato il difetto al primo
// build. Il criterio per aggiungerne uno è "questo si è già rotto una volta".
int MapEditor::selfTest()
{
    int failed = 0;
    auto check = [&](bool ok, const char* what) {
        std::printf("  [%s] %s\n", ok ? "OK  " : "FALL", what);
        if (!ok) ++failed;
    };

    // Stato sintetico e deterministico: tre box a distanza nota. Niente mappa
    // reale — un collaudo che dipende dal contenuto dell'utente fallisce quando
    // l'utente modifica la mappa, e allora lo si smette di guardare.
    // RICOSTRUITO da zero prima di ogni caso, non "troncato": un `resize` lascia
    // in testa le copie del caso precedente, e il banco di prova comincia a
    // misurare sé stesso invece del codice. È lo stesso errore che ha già fatto
    // sbagliare tre diagnosi su questo progetto.
    auto reset = [&]() {
        m_boxes.clear(); m_structures.clear(); m_multiSel.clear();
        m_hist.clear();
        m_selBox = -1; m_selStruct = -1;
        for (int i = 0; i < 3; ++i)
        {
            BoxEntry b; b.x = (float)i * 10.0f; b.y = 1.0f; b.z = 0.0f;
            std::snprintf(b.label, sizeof(b.label), "box%d", i);
            m_boxes.push_back(b);
        }
    };
    reset();
    const std::size_t base = m_boxes.size();

    // ── SERIE su una sola box ────────────────────────────────────────────
    // L'invariante che era rotto: le copie stanno a k*offset dall'originale, e
    // NESSUN altro elemento si muove.
    m_multiSel.clear(); m_selBox = 0; m_selStruct = -1;
    m_arrayCount = 3; m_arrayOff[0] = 4.0f; m_arrayOff[1] = 0.0f; m_arrayOff[2] = 0.0f;
    m_arrayYawStep = 0.0f;
    const float otherXBefore = m_boxes[2].x;
    makeArray();

    check(m_boxes.size() == base + 3, "serie: crea esattamente 3 copie");
    {
        // Le copie di box0 sono quelle a x = 4, 8, 12: si contano, senza dipendere
        // da DOVE finiscono nella lista (è proprio l'assunzione che ci ha fregato).
        int found = 0;
        for (int k = 1; k <= 3; ++k)
            for (const auto& b : m_boxes)
                if (std::fabs(b.x - 4.0f * (float)k) < 0.001f
                    && std::fabs(b.z) < 0.001f) { ++found; break; }
        check(found == 3, "serie: le copie stanno a k x offset dall'originale");
    }
    {
        // Il difetto reale: un elemento NON coinvolto veniva trascinato via.
        bool intact = false;
        for (const auto& b : m_boxes)
            if (std::strcmp(b.label, "box2") == 0)
                intact = (std::fabs(b.x - otherXBefore) < 0.001f);
        check(intact, "serie: non sposta elementi fuori dalla selezione");
    }

    // ── SERIE con rotazione ──────────────────────────────────────────────
    reset();
    m_multiSel.clear(); m_selBox = 0;
    m_arrayCount = 2; m_arrayYawStep = 30.0f;
    makeArray();
    {
        int rotated = 0;
        for (const auto& b : m_boxes)
            if (std::fabs(b.ry - 30.0f) < 0.001f || std::fabs(b.ry - 60.0f) < 0.001f)
                ++rotated;
        check(rotated == 2, "serie: applica la rotazione progressiva");
    }
    m_arrayYawStep = 0.0f;

    // ── DUPLICA su selezione multipla ────────────────────────────────────
    // L'inserimento accanto all'originale scala gli indici: duplicando due box si
    // duplicavano box diverse da quelle scelte.
    reset();
    m_multiSel.clear();
    m_multiSel.push_back(0);
    m_multiSel.push_back(2);
    duplicateSelected();
    check(m_boxes.size() == base + 2, "duplica multiplo: crea 2 copie");
    {
        int c0 = 0, c2 = 0;
        for (const auto& b : m_boxes)
        {
            if (std::strcmp(b.label, "box0") == 0) ++c0;
            if (std::strcmp(b.label, "box2") == 0) ++c2;
        }
        check(c0 == 2 && c2 == 2, "duplica multiplo: duplica le box SELEZIONATE");
    }

    // ── ANNULLA ──────────────────────────────────────────────────────────
    doUndo();
    check(m_boxes.size() == base, "annulla: riporta al conteggio precedente");

    // ── TETTI DEI CODICI (KI #100) ───────────────────────────────────────
    // Che la tabella dei tetti dica il vero, e non diverga dagli intervalli veri
    // usati da `codePosition`. Si riempie fino a UNO OLTRE il tetto e si verifica
    // che l'ultimo indice lecito risolva ancora come posizione tattica, mentre
    // quello successivo cade nell'intervallo dei settori — cioè che il tetto
    // dichiarato sia esattamente quello reale.
    reset();
    {
        const auto caps = capacityReport();
        int posLimit = 0;
        for (const auto& c : caps)
            if (std::strcmp(c.name, "posizioni tattiche") == 0) posLimit = c.limit;
        check(posLimit > 0, "tetti: la tabella conosce le posizioni tattiche");

        m_positions.clear();
        for (int i = 0; i < posLimit; ++i)
        {
            PositionEntry p; p.x = (float)i; p.y = 0.0f; p.z = 0.0f;
            m_positions.push_back(p);
        }
        glm::vec3 tmp;
        check(codePosition(-1000 - (posLimit - 1), tmp),
              "tetti: l'ultimo indice lecito risolve ancora");
        // Un indice oltre il tetto produce -2000, che è l'inizio dei SETTORI:
        // se un giorno gli intervalli cambiano, questo controllo se ne accorge.
        check((-1000 - posLimit) == -2000,
              "tetti: il tetto dichiarato coincide con l'intervallo reale");

        m_positions.clear();
        for (int i = 0; i < posLimit + 1; ++i) m_positions.push_back(PositionEntry{});
        bool flagged = false;
        for (const auto& c : capacityReport())
            if (std::strcmp(c.name, "posizioni tattiche") == 0) flagged = (c.used > c.limit);
        check(flagged, "tetti: il superamento viene segnalato");
        m_positions.clear();
    }

    // ── ASSEMBLAGGI (ADR-056) ────────────────────────────────────────────
    // La trasformazione delle parti è il punto in cui un assemblaggio può sbagliare
    // in silenzio: se la convenzione di rotazione qui non è quella delle primitive,
    // le parti ruotano al CONTRARIO e il difetto si vede solo ruotando l'istanza.
    {
        mini::StructureTypeDef ty;
        ty.kind = mini::StructureKind::Wall;
        mini::StructurePart p;
        p.isBox = true;
        p.box.x = 4.0f; p.box.y = 1.0f; p.box.z = 0.0f;   // 4 m lungo +X, in locale
        p.box.sx = 1.0f; p.box.sy = 2.0f; p.box.sz = 1.0f;
        ty.parts.push_back(p);

        check(mini::mapstructures::isAssembly(ty), "assemblaggio: riconosciuto come tale");

        // Istanza senza rotazione: la parte trasla e basta.
        mini::StructureDef inst;
        inst.x = 10.0f; inst.y = 0.0f; inst.z = 20.0f; inst.ry = 0.0f;
        std::vector<mini::MapGeometryBox> out;
        mini::mapstructures::expandInstance(inst, &ty, out);
        check(out.size() == 1, "assemblaggio: una parte -> un box");
        const bool moved = !out.empty()
            && std::fabs(out[0].x - 14.0f) < 0.001f
            && std::fabs(out[0].z - 20.0f) < 0.001f
            && std::fabs(out[0].y -  1.0f) < 0.001f;
        check(moved, "assemblaggio: la posa dell'istanza si applica alla parte");

        // Con ry = 90 la convenzione di `detail::Frame` è
        //   wx = ox + lx·cos + lz·sin ,  wz = oz − lx·sin + lz·cos
        // quindi una parte a (4, 0) locale, con origine (10, 20), finisce a
        //   x = 10 + 4·0 + 0·1 = 10 ,  z = 20 − 4·1 + 0·0 = 16.
        // Il valore ESATTO e non solo la distanza: col segno sbagliato la parte
        // finirebbe a z = 24, cioè dall'altra parte — un difetto che si vede solo
        // ruotando, e che a occhio sembra "quasi giusto".
        out.clear();
        inst.ry = 90.0f;
        mini::mapstructures::expandInstance(inst, &ty, out);
        const bool sameConvention = !out.empty()
            && std::fabs(out[0].x - 10.0f) < 0.01f
            && std::fabs(out[0].z - 16.0f) < 0.01f
            && std::fabs(out[0].ry - 90.0f) < 0.01f;   // la parte ruota CON l'insieme
        check(sameConvention,
              "assemblaggio: la rotazione usa la convenzione delle primitive");

        // Senza parti si torna al comportamento di ADR-053: nessuna regressione per
        // i tipi gia' scritti.
        mini::StructureTypeDef simple;
        simple.kind = mini::StructureKind::Wall;
        out.clear();
        mini::StructureDef inst2; inst2.kind = mini::StructureKind::Wall;
        inst2.length = 4.0f;
        mini::mapstructures::expandInstance(inst2, &simple, out);
        std::vector<mini::MapGeometryBox> plain;
        mini::mapstructures::expand(inst2, plain);
        check(out.size() == plain.size() && !plain.empty(),
              "tipo senza parti: identico al comportamento di prima");
    }

    // ── ASSEMBLAGGIO IN MAPPA: espansione e giro salva→ricarica ──────────
    // Due difetti veri, trovati dall'utente e non da me, che questo controllo
    // avrebbe fermato al primo build:
    //   1. l'anteprima espandeva con `expand` invece di `expandInstance`, quindi una
    //      composita appariva come la sola primitiva di base;
    //   2. il caricatore di mappe dell'editor non rileggeva il campo `type`: al
    //      salvataggio successivo il legame col tipo spariva PER SEMPRE.
    // Il secondo è perdita di dati, non un difetto di visualizzazione.
    reset();
    {
        // Un tipo composito sintetico, registrato dove l'editor lo cerca davvero.
        mini::StructureTypeDef ty;
        ty.id = "_selftest_asm"; ty.label = "asm"; ty.kind = mini::StructureKind::Wall;
        for (int k = 0; k < 3; ++k)
        {
            mini::StructurePart p;
            p.isBox = true;
            p.box.x = (float)k * 3.0f; p.box.y = 1.0f; p.box.z = 0.0f;
            p.box.sx = 1.0f; p.box.sy = 2.0f; p.box.sz = 1.0f;
            ty.parts.push_back(p);
        }
        m_prefabReg.addStructureTypeForTest(ty);

        mini::StructureDef inst;
        inst.kind = mini::StructureKind::Wall;
        inst.type = "_selftest_asm";
        inst.x = 5.0f; inst.z = 7.0f;
        m_structures.clear();
        m_structures.push_back(inst);

        std::vector<mini::MapGeometryBox> out;
        expandStructureAt(0, out);
        check(out.size() == 3, "assemblaggio in mappa: si espande in TUTTE le parti");

        rebuildStructurePreview();
        check(m_structPreview.size() == 3,
              "anteprima: mostra l'assemblaggio intero, non la sola primitiva");

        // Il giro completo con la serializzazione VERA — quella che usano davvero
        // `saveMap` e `loadMap`, non una sua copia scritta qui. Una copia avrebbe
        // verificato il contratto e non il codice: è la trappola del banco che
        // misura sé stesso, e su questo progetto ha già falsato tre diagnosi.
        const mini::StructureDef back = mini::structjson::fromJson(
                                            mini::structjson::toJson(inst));
        check(back.type == "_selftest_asm",
              "salva->ricarica: il legame col TIPO sopravvive al giro");
        check(back.kind == inst.kind
              && std::fabs(back.x - inst.x) < 0.001f
              && std::fabs(back.z - inst.z) < 0.001f,
              "salva->ricarica: la ricetta torna identica");

        // E che l'istanza ricaricata si espanda ancora nell'assemblaggio intero:
        // è l'invariante che l'utente ha visto rompersi ("appare solo una scala").
        m_structures.clear();
        m_structures.push_back(back);
        std::vector<mini::MapGeometryBox> after;
        expandStructureAt(0, after);
        check(after.size() == 3,
              "salva->ricarica: dopo il giro si espande ancora INTERO");

        m_structures.clear();
        m_structPreview.clear();
        m_prefabReg.removeStructureTypeForTest("_selftest_asm");
    }

    // ── RIFERIMENTI FRA COMPOSITE (ADR-056 rivisto) ──────────────────────
    // L'annidamento era VIETATO, e il divieto è caduto su richiesta esplicita
    // ("preferirei le lasciassi normali"). Il motivo del divieto però resta vero: da
    // qui in poi due strutture possono contenersi a vicenda, cioè un'espansione
    // infinita — l'editor che si pianta senza dire niente. Queste verifiche sono il
    // prezzo del permesso, non un extra.
    reset();
    {
        mini::StructureTypeDef leaf;
        leaf.id = "_st_leaf"; leaf.label = "leaf"; leaf.kind = mini::StructureKind::Wall;
        leaf.verified = true;
        for (int k = 0; k < 2; ++k)
        {
            mini::StructurePart p;
            p.isBox = true;
            p.box.x = (float)k * 2.0f; p.box.y = 1.0f; p.box.z = 0.0f;
            p.box.sx = 1.0f; p.box.sy = 2.0f; p.box.sz = 1.0f;
            leaf.parts.push_back(p);
        }
        m_prefabReg.addStructureTypeForTest(leaf);

        // Un tipo che CONTIENE il precedente, spostato di 10 in x.
        mini::StructureTypeDef host;
        host.id = "_st_host"; host.label = "host"; host.kind = mini::StructureKind::Wall;
        {
            mini::StructurePart r;
            r.isBox = false; r.refType = "_st_leaf";
            r.prim.x = 10.0f; r.prim.z = 0.0f;
            host.parts.push_back(r);
        }
        m_prefabReg.addStructureTypeForTest(host);

        std::vector<mini::MapGeometryBox> out;
        expandTypeForEdit(host, out);
        check(out.size() == 2, "riferimento: espande le parti dell'altra struttura");
        check(out.empty() || std::fabs(out[0].x - 10.0f) < 0.001f,
              "riferimento: la posa della parte si applica al sottotipo");

        // Il giro su disco: `ref` è il quarto campo della storia, e i tre precedenti
        // erano arrivati in un lettore su due. Si controlla con il serializzatore
        // VERO, non con una copia scritta qui.
        const mini::StructurePart back = mini::structjson::partFromJson(
                                             mini::structjson::partToJson(host.parts[0]));
        check(back.isRef() && back.refType == "_st_leaf",
              "salva->ricarica: il riferimento sopravvive al giro");
        check(std::fabs(back.prim.x - 10.0f) < 0.001f,
              "salva->ricarica: la posa del riferimento torna identica");

        // ESPLODI: la geometria non si deve muovere di un millimetro. Se esplodere
        // spostasse le parti, sarebbe uno strumento che rompe la mappa mentre la
        // aiuta — e ce ne si accorgerebbe solo guardando.
        {
            std::vector<mini::MapGeometryBox> exploded;
            for (const auto& sp : leaf.parts)
            {
                const auto w = mini::mapstructures::transformPart(
                    sp, host.parts[0].prim.x, host.parts[0].prim.y,
                    host.parts[0].prim.z, host.parts[0].prim.ry);
                if (w.isBox) exploded.push_back(w.box);
            }
            bool same = (exploded.size() == out.size());
            for (std::size_t k = 0; same && k < out.size(); ++k)
                same = std::fabs(exploded[k].x - out[k].x) < 0.001f
                    && std::fabs(exploded[k].y - out[k].y) < 0.001f
                    && std::fabs(exploded[k].z - out[k].z) < 0.001f;
            check(same, "esplodi: le parti restano ESATTAMENTE dov'erano");
        }

        // CICLO: leaf comincia a contenere host. Senza guardia, espansione infinita.
        {
            mini::StructureTypeDef loopLeaf = leaf;
            mini::StructurePart r;
            r.isBox = false; r.refType = "_st_host";
            loopLeaf.parts.push_back(r);
            m_prefabReg.addStructureTypeForTest(loopLeaf);

            check(mini::mapstructures::assemblyUses(host, "_st_host", m_typeResolver),
                  "ciclo: l'uso indiretto viene riconosciuto PRIMA di inserirlo");

            std::vector<mini::MapGeometryBox> loopOut;
            expandTypeForEdit(host, loopOut);   // deve TERMINARE
            check(loopOut.size() < 100,
                  "ciclo: l'espansione termina invece di girare all'infinito");
            m_prefabReg.addStructureTypeForTest(leaf);   // ripristina
        }

        // PROFONDITÀ: quanti livelli porta con sé un tipo.
        check(mini::mapstructures::assemblyDepth(host, m_typeResolver) == 1,
              "profondita': un riferimento a una foglia vale un livello");
        check(mini::mapstructures::assemblyDepth(leaf, m_typeResolver) == 0,
              "profondita': solo box e primitive valgono zero");

        // ── ESPLODI IN MAPPA: stessa geometria, elementi separati ────────
        m_structures.clear(); m_boxes.clear();
        {
            mini::StructureDef inst;
            inst.kind = mini::StructureKind::Wall;
            inst.type = "_st_leaf";
            inst.x = 3.0f; inst.z = -4.0f;
            m_structures.push_back(inst);
            std::vector<mini::MapGeometryBox> pre;
            expandStructureAt(0, pre);

            explodeStructure(0);
            check(m_structures.empty() && m_boxes.size() == pre.size(),
                  "esplodi in mappa: l'istanza diventa i suoi elementi");
            bool same = (m_boxes.size() == pre.size());
            for (std::size_t k = 0; same && k < pre.size(); ++k)
                same = std::fabs(m_boxes[k].x - pre[k].x) < 0.001f
                    && std::fabs(m_boxes[k].y - pre[k].y) < 0.001f
                    && std::fabs(m_boxes[k].z - pre[k].z) < 0.001f;
            check(same, "esplodi in mappa: la geometria non si sposta");

            // E il contrario: annullare deve rimettere l'istanza com'era.
            doUndo();
            check(m_structures.size() == 1 && m_boxes.empty(),
                  "esplodi in mappa: si annulla con Ctrl+Z");
        }

        // ── MODIFICA DI UNA SOLA ISTANZA (parti locali) ──────────────────
        // L'invariante che conta: quattro copie dello stesso tipo, se ne modifica
        // UNA, e le altre tre non si accorgono di niente. È esattamente ciò che
        // l'utente ha chiesto, ed è anche ciò che un errore di indice romperebbe in
        // silenzio — modificando la struttura sbagliata (KI #100).
        m_structures.clear(); m_boxes.clear();
        {
            for (int k = 0; k < 4; ++k)
            {
                mini::StructureDef inst;
                inst.kind = mini::StructureKind::Wall;
                inst.type = "_st_leaf";
                inst.x = (float)k * 20.0f;
                m_structures.push_back(inst);
            }
            std::vector<mini::MapGeometryBox> ref0;
            expandStructureAt(0, ref0);

            // Si modifica SOLO la terza: una parte in meno.
            openInstanceTab(2);
            const bool opened = !m_structTabs.empty()
                && m_structTabs.back().target == StructTab::Target::Instance;
            check(opened, "istanza: il tab si apre legato a QUELLA struttura");
            if (opened)
            {
                auto& tab = m_structTabs.back();
                check((int)tab.def.parts.size() == (int)leaf.parts.size(),
                      "istanza: si parte dalle parti del tipo, non dal vuoto");
                tab.def.parts.pop_back();
                applyInstanceTab(tab);

                check(m_structures[2].isModifiedInstance(),
                      "istanza: la modifica si segna sull'istanza");
                std::vector<mini::MapGeometryBox> got2;
                expandStructureAt(2, got2);
                check(got2.size() + 1 == ref0.size(),
                      "istanza: la modifica si VEDE nell'espansione");
                for (int k : {0, 1, 3})
                {
                    std::vector<mini::MapGeometryBox> other;
                    expandStructureAt(k, other);
                    check(other.size() == ref0.size(),
                          "istanza: le altre copie NON sono toccate");
                }
                // Il tipo in libreria non deve essersi mosso di un byte.
                check(m_prefabReg.getStructureType("_st_leaf")
                      && m_prefabReg.getStructureType("_st_leaf")->parts.size()
                         == leaf.parts.size(),
                      "istanza: il tipo di libreria resta intatto");

                // Giro su disco con il serializzatore VERO: se `local_parts` non
                // sopravvive, la modifica sparisce al primo salva→ricarica — cioè il
                // difetto peggiore possibile, perché si scopre il giorno dopo.
                const mini::StructureDef back = mini::structjson::fromJson(
                                                    mini::structjson::toJson(m_structures[2]));
                check(back.isModifiedInstance()
                      && back.localParts.size() == m_structures[2].localParts.size(),
                      "istanza: salva->ricarica conserva le parti modificate");

                // Ripristino: torna a seguire il tipo.
                m_structures[2].localParts.clear();
                std::vector<mini::MapGeometryBox> restored;
                expandStructureAt(2, restored);
                check(restored.size() == ref0.size(),
                      "istanza: ripristinando torna identica al tipo");

                m_structTabs.pop_back();
            }
        }

        // ── ISOLA E MODIFICA una parte-riferimento ───────────────────────
        {
            StructTab tab;
            tab.def = host;
            tab.isolated = -1;
            check(tab.parts().size() == host.parts.size(),
                  "isolamento: fuori si vedono le parti della struttura");
            // Si entra: le parti locali nascono da quelle del tipo riferito.
            tab.def.parts[0].localParts = leaf.parts;
            tab.isolated = 0;
            check((int)tab.parts().size() == (int)leaf.parts.size(),
                  "isolamento: dentro si vedono le parti della copia");
            tab.parts().pop_back();
            tab.isolated = -1;
            check(tab.def.parts[0].isModifiedRef(),
                  "isolamento: alla chiusura la parte risulta modificata");

            std::vector<mini::MapGeometryBox> iso;
            expandTypeForEdit(tab.def, iso);
            check(iso.size() + 1 == 2u,
                  "isolamento: l'espansione usa le parti locali, non il tipo");

            const mini::StructurePart backp = mini::structjson::partFromJson(
                                                  mini::structjson::partToJson(tab.def.parts[0]));
            check(backp.isModifiedRef() && backp.localParts.size() == 1,
                  "isolamento: salva->ricarica conserva la copia modificata");
        }

        m_structures.clear(); m_boxes.clear();
        m_prefabReg.removeStructureTypeForTest("_st_leaf");
        m_prefabReg.removeStructureTypeForTest("_st_host");
    }

    // ── ORIGINE DI UN ASSEMBLAGGIO ───────────────────────────────────────
    // L'origine è il perno di rotazione e il punto del gizmo in mappa. Un
    // assemblaggio costruito "verso destra" finiva con l'origine fuori da sé —
    // segnalato dall'utente su una torre. Qui si verifica che il centraggio la
    // riporti al centro SENZA cambiare la forma.
    {
        StructTab tb;
        tb.def.kind = mini::StructureKind::Wall;
        // Tre box in fila da x=10 a x=20: centro a 15, quindi origine fuori di 15 m.
        for (int k = 0; k < 3; ++k)
        {
            mini::StructurePart p;
            p.isBox = true;
            p.box.x = 10.0f + (float)k * 5.0f; p.box.y = 1.0f; p.box.z = 4.0f;
            p.box.sx = 1.0f; p.box.sy = 2.0f; p.box.sz = 1.0f;
            tb.def.parts.push_back(p);
        }
        check(std::fabs(assemblyOriginOffset(tb) - std::sqrt(15.0f*15.0f + 4.0f*4.0f)) < 0.01f,
              "origine: lo scostamento si misura in pianta");

        // La FORMA prima e dopo: le distanze fra le parti non devono cambiare.
        std::vector<mini::MapGeometryBox> before;
        expandTypeForEdit(tb.def, before);
        const float spanBefore = before.back().x - before.front().x;

        centerAssemblyOrigin(tb);
        check(assemblyOriginOffset(tb) < 0.01f, "origine: dopo il centraggio cade sul centro");

        std::vector<mini::MapGeometryBox> after;
        expandTypeForEdit(tb.def, after);
        check(after.size() == before.size(), "origine: il centraggio non perde parti");
        check(std::fabs((after.back().x - after.front().x) - spanBefore) < 0.001f,
              "origine: la FORMA non cambia, cambia solo dove sta il perno");
        check(std::fabs(after.front().x + after.back().x) < 0.01f,
              "origine: le parti risultano simmetriche attorno allo zero");
    }

    // ── ID DA NOME: la regola che decide su quale FILE si scrive ─────────
    // Se due nomi diversi producessero lo stesso id, "Salva come copia"
    // sovrascriverebbe l'originale invece di affiancarlo — cioè farebbe
    // esattamente il danno da cui deve proteggere.
    {
        check(idFromLabel("Tower") == "tower", "id: minuscolo");
        check(idFromLabel("Tower variante") == "tower_variante", "id: spazi -> underscore");
        check(idFromLabel("Torre A/B") == "torre_a_b", "id: simboli -> underscore");
        check(idFromLabel("Tower   ") == "tower", "id: niente underscore in coda");
        check(idFromLabel("") == "struttura", "id: un nome vuoto non produce un file senza nome");
        check(idFromLabel("Tower") != idFromLabel("Tower 2"),
              "id: due nomi diversi restano due file diversi");
    }

    // ── STRUMENTI DI COSTRUZIONE (doc 53 L1/L2) ──────────────────────────
    // Sono operazioni che si invocano col mouse, quindi senza questi controlli
    // nessuno le verifica finché non le rompe in mano all'utente — è già successo
    // due volte (Serie, cambio modulo). Qui si esercita la LOGICA, che è dove
    // stanno gli invarianti.
    reset();
    {
        // TIRA LA FACCIA. L'invariante che distingue questo gesto dalla scala:
        // la faccia OPPOSTA non si muove. Se si muovesse, allungare un muro
        // sposterebbe anche l'altro capo — e ci si accorgerebbe solo misurando.
        m_boxes.clear();
        BoxEntry b; b.x = 10.0f; b.y = 1.5f; b.z = 0.0f;
        b.sx = 4.0f; b.sy = 3.0f; b.sz = 1.0f;
        m_boxes.push_back(b);
        m_multiSel.clear(); m_selBox = 0; m_selStruct = -1;

        const float leftBefore = m_boxes[0].x - m_boxes[0].sx * 0.5f;
        applyFaceDelta(1, 2.0f);                       // tira +X di 2 m
        check(std::fabs(m_boxes[0].sx - 6.0f) < 0.001f,
              "tira faccia: la misura cresce esattamente del delta");
        check(std::fabs((m_boxes[0].x - m_boxes[0].sx * 0.5f) - leftBefore) < 0.001f,
              "tira faccia: la faccia OPPOSTA non si muove");

        const float rightBefore = m_boxes[0].x + m_boxes[0].sx * 0.5f;
        applyFaceDelta(0, 1.0f);                       // tira -X di 1 m (verso fuori)
        check(std::fabs(m_boxes[0].sx - 7.0f) < 0.001f,
              "tira faccia: funziona anche sulla faccia negativa");
        check(std::fabs((m_boxes[0].x + m_boxes[0].sx * 0.5f) - rightBefore) < 0.001f,
              "tira faccia: sulla faccia negativa resta ferma la positiva");

        applyFaceDelta(3, -100.0f);                    // schiaccia oltre lo zero
        check(m_boxes[0].sy >= 0.05f,
              "tira faccia: non si puo' schiacciare a spessore zero");

        // SPOSTA DI UNA MISURA ESATTA, su tutta la selezione.
        m_boxes.clear();
        for (int k = 0; k < 3; ++k)
        { BoxEntry e; e.x = (float)k * 5.0f; e.sx = 2.0f; m_boxes.push_back(e); }
        m_multiSel = {0, 1, 2};
        moveSelectionBy({3.0f, 0.0f, -1.0f});
        check(std::fabs(m_boxes[0].x - 3.0f) < 0.001f
              && std::fabs(m_boxes[2].x - 13.0f) < 0.001f
              && std::fabs(m_boxes[1].z + 1.0f) < 0.001f,
              "sposta di: l'offset si applica a TUTTA la selezione");

        // ALLINEA A FILO. Il caso che conta è con spessori DIVERSI: allineando i
        // centri due muri di spessore diverso restano sfalsati, ed è la fessura che
        // il navmesh non attraversa.
        m_boxes.clear();
        { BoxEntry e; e.x = 0.0f;  e.sx = 2.0f; m_boxes.push_back(e); }
        { BoxEntry e; e.x = 10.0f; e.sx = 6.0f; m_boxes.push_back(e); }
        m_multiSel = {0, 1};
        alignSelection(0, 2);                          // a filo del bordo MAX
        check(std::fabs((m_boxes[0].x + 1.0f) - (m_boxes[1].x + 3.0f)) < 0.001f,
              "allinea: i BORDI coincidono anche con spessori diversi");

        // DISTRIBUISCI: gli estremi restano fermi, il centro finisce a meta'.
        m_boxes.clear();
        { BoxEntry e; e.x = 0.0f;  m_boxes.push_back(e); }
        { BoxEntry e; e.x = 1.0f;  m_boxes.push_back(e); }
        { BoxEntry e; e.x = 12.0f; m_boxes.push_back(e); }
        m_multiSel = {0, 1, 2};
        distributeSelection(0);
        check(std::fabs(m_boxes[0].x - 0.0f) < 0.001f
              && std::fabs(m_boxes[2].x - 12.0f) < 0.001f,
              "distribuisci: gli estremi non si muovono");
        check(std::fabs(m_boxes[1].x - 6.0f) < 0.001f,
              "distribuisci: lo spazio fra gli elementi diventa uguale");

        // DISEGNA BOX: l'impronta viene dal gesto, la BASE sta sul piano di lavoro.
        // La Y di un box e' il CENTRO: sbagliarlo interra meta' muro.
        m_boxes.clear();
        m_drawHeight = 3.0f;
        m_drawPlaneY = 4.0f;
        createDrawnBox({2.0f, 4.0f, -3.0f}, {8.0f, 4.0f, 1.0f});
        check(m_boxes.size() == 1, "disegna box: ne crea esattamente uno");
        if (!m_boxes.empty())
        {
            const auto& d = m_boxes[0];
            check(std::fabs(d.sx - 6.0f) < 0.001f && std::fabs(d.sz - 4.0f) < 0.001f,
                  "disegna box: l'impronta e' quella tracciata");
            check(std::fabs(d.x - 5.0f) < 0.001f && std::fabs(d.z + 1.0f) < 0.001f,
                  "disegna box: sta al centro dell'impronta");
            check(std::fabs((d.y - d.sy * 0.5f) - 4.0f) < 0.001f,
                  "disegna box: la BASE appoggia sul piano di lavoro");
        }
        m_boxes.clear(); m_multiSel.clear(); m_selBox = -1;
        m_drawPlaneY = 0.0f;

        // APPOGGIA. Il caso che conta e' la caduta a terra: "sembra appoggiato" a
        // 3 cm da terra e' indistinguibile a vista, e quei 3 cm stanno sotto la
        // soglia di erosione del navmesh.
        m_boxes.clear();
        { BoxEntry e; e.x = 0; e.y = 7.0f; e.z = 0; e.sx = 2; e.sy = 2; e.sz = 2;
          m_boxes.push_back(e); }
        m_multiSel.clear(); m_selBox = 0;
        snapSelectionToSurface(1, -1);
        check(std::fabs((m_boxes[0].y - m_boxes[0].sy * 0.5f) - 0.0f) < 0.001f,
              "appoggia: cade a terra fino a toccare, senza fessura");

        // Appoggio su un'altra superficie, non sul suolo: deve fermarsi sul TOP
        // del box sottostante, non attraversarlo.
        m_boxes.clear();
        { BoxEntry g; g.x = 0; g.y = 1.0f; g.z = 0; g.sx = 10; g.sy = 2; g.sz = 10;
          m_boxes.push_back(g); }                                  // top a y=2
        { BoxEntry e; e.x = 0; e.y = 9.0f; e.z = 0; e.sx = 2; e.sy = 2; e.sz = 2;
          m_boxes.push_back(e); }
        m_multiSel.clear(); m_selBox = 1;
        snapSelectionToSurface(1, -1);
        check(std::fabs((m_boxes[1].y - m_boxes[1].sy * 0.5f) - 2.0f) < 0.001f,
              "appoggia: si ferma sul TOP di cio' che sta sotto");

        // Accostamento laterale, e la selezione si muove come UN CORPO.
        m_boxes.clear();
        { BoxEntry w; w.x = 10.0f; w.y = 1; w.z = 0; w.sx = 2; w.sy = 2; w.sz = 8;
          m_boxes.push_back(w); }                                  // muro: bordo -X a 9
        { BoxEntry a; a.x = 0.0f; a.y = 1; a.z = -1.0f; a.sx = 2; a.sy = 2; a.sz = 2;
          m_boxes.push_back(a); }
        { BoxEntry b2; b2.x = 0.0f; b2.y = 1; b2.z = 1.0f; b2.sx = 2; b2.sy = 2; b2.sz = 2;
          m_boxes.push_back(b2); }
        m_multiSel = {1, 2};
        const float gapBefore = m_boxes[2].z - m_boxes[1].z;
        snapSelectionToSurface(0, +1);
        check(std::fabs((m_boxes[1].x + 1.0f) - 9.0f) < 0.001f,
              "accosta: arriva a contatto col muro, senza compenetrare");
        check(std::fabs((m_boxes[2].z - m_boxes[1].z) - gapBefore) < 0.001f,
              "accosta: la selezione si muove come un corpo, la forma non cambia");

        // Niente davanti = niente si muove. Un comando che sposta "verso il nulla"
        // farebbe sparire la geometria dalla vista senza dire dove.
        m_boxes.clear();
        { BoxEntry e; e.x = 0; e.y = 1; e.z = 0; e.sx = 2; e.sy = 2; e.sz = 2;
          m_boxes.push_back(e); }
        m_multiSel.clear(); m_selBox = 0;
        check(snapSelectionToSurface(0, +1) == 0,
              "accosta: senza ostacoli davanti non muove niente");

        m_boxes.clear(); m_multiSel.clear(); m_selBox = -1;
    }

    // ── SCALABILITÀ: quanto costa una modifica al crescere della mappa ───
    // Non è un controllo pass/fail, è una MISURA — e serve prima di costruire una
    // mappa 300 × 200. `updateViewport()` ricalcola l'esposizione a OGNI modifica
    // (default `recomputeDerived = true`), anche a ogni frame mentre si trascina col
    // gizmo, e `buildTacticalLinks` è O(n²) sulle posizioni tattiche.
    // Training Ground ne ha 169; la mappa grande, a parità di densità, ne avrà ~1500.
    reset();
    {
        std::printf("  --- costo di UNA modifica al crescere delle posizioni ---\n");
        const int sizes[] = { 169, 500, 1000, 1500 };
        for (int n : sizes)
        {
            m_positions.clear();
            m_positions.reserve(n);
            // Sparse su una griglia realistica, non tutte nello stesso punto: la LOS
            // fra posizioni coincidenti costerebbe molto meno del caso vero.
            const int side = (int)std::ceil(std::sqrt((double)n));
            for (int i = 0; i < n; ++i)
            {
                PositionEntry p;
                p.x = (float)(i % side) * 4.0f;
                p.z = (float)(i / side) * 4.0f;
                p.y = 0.9f;
                p.canShoot = true;
                p.role = "cover";
                m_positions.push_back(p);
            }
            const auto t0 = std::chrono::steady_clock::now();
            recomputeExposure();
            const float ms = std::chrono::duration<float, std::milli>(
                std::chrono::steady_clock::now() - t0).count();
            std::printf("  %5d posizioni -> %8.1f ms per modifica%s\n", n, ms,
                        ms > 100.0f ? "   <-- inutilizzabile" :
                        (ms > 33.0f ? "   <-- si sente" : ""));
        }
        m_positions.clear();
    }

    std::printf("[selftest] %d controlli falliti\n", failed);
    return failed;
}

// ════════════════════════════════════════════════════════════════════════════
// EDITOR STRUTTURE (doc 48, ADR-055)
// ════════════════════════════════════════════════════════════════════════════

// Espansione di un tipo COM'È NELL'EDITOR: nell'origine, senza posa d'istanza.
// Passa dalla stessa `expandInstance` del registry — un secondo criterio per
// decidere "assemblaggio o primitiva" avrebbe fatto divergere anteprima e gioco,
// che è il difetto che ADR-018/032/053 esistono per impedire.
void MapEditor::expandTypeForEdit(const mini::StructureTypeDef& def,
                                  std::vector<mini::MapGeometryBox>& out) const
{
    mini::StructureDef inst = def.defaults;
    inst.x = 0.0f; inst.y = def.defaults.y; inst.z = 0.0f; inst.ry = 0.0f;
    if (mini::mapstructures::isAssembly(def)) inst.y = 0.0f;   // le parti portano la loro quota
    mini::mapstructures::expandInstance(inst, &def, out, m_typeResolver);
}

// La "scala" su una PRIMITIVA agisce sulle sue misure normative, mai su un fattore
// moltiplicativo: scalare una scala del 30% produrrebbe alzate fuori norma, cioè
// proprio l'errore che ADR-053 rende inesprimibile. Ogni misura resta clampata al
// suo pavimento fisico.
void MapEditor::scalePrimitivePart(mini::StructureDef& s, const glm::vec3& d)
{
    auto bump = [&](mini::StructureParam pm, float delta) {
        if (std::fabs(delta) < 1e-5f) return;
        // Si parte dal valore EFFETTIVO: molti campi valgono 0 per dire "normativo",
        // e rifiutarli rendeva certe misure immodificabili col gizmo — è il motivo
        // per cui la pedata di una scala non si allungava (segnalato dall'utente).
        float v = mini::mapstructures::effectiveParam(s, pm) + delta;
        const float lo = mini::mapstructures::physicalMin(s.kind, pm, s);
        const float hi = mini::mapstructures::physicalMax(s.kind, pm);
        if (lo > 0.0f && v < lo) v = lo;
        if (hi > 0.0f && v > hi) v = hi;
        if (v < 0.01f) v = 0.01f;
        mini::mapstructures::setParam(s, pm, v);
    };
    using P = mini::StructureParam;
    switch (s.kind)
    {
        case mini::StructureKind::Stair:
        case mini::StructureKind::Ramp:
            // Anche la PEDATA: allungarla è lecito (una scala più dolce va sempre
            // bene) e accorciarla è bloccato dal pavimento fisico. Ometterla
            // rendeva la scala inallungabile col gizmo — segnalato dall'utente.
            bump(P::Width, d.x); bump(P::Rise, d.y); bump(P::Tread, d.z); break;
        case mini::StructureKind::Wall:
        case mini::StructureKind::Doorway:
        case mini::StructureKind::Barricade:
            bump(P::Length, d.x); bump(P::Height, d.y); bump(P::Thickness, d.z); break;
        case mini::StructureKind::Platform:
        case mini::StructureKind::Room:
            bump(P::SizeX, d.x); bump(P::SizeZ, d.z); bump(P::Height, d.y); break;
        case mini::StructureKind::Catwalk:
            bump(P::Length, d.x); bump(P::Width, d.z); break;
        default: break;
    }
}

// Una parte nuova nasce DOVE STAI GUARDANDO, come ogni elemento del Map Editor.
//
// Prima la mettevo a destra dell'ingombro esistente, con un metro di stacco. Serviva
// a non farle nascere una dentro l'altra, e quel problema lo risolveva — ma ne
// creava uno peggiore: l'assemblaggio **cresceva sempre verso destra**, e il suo
// centro finiva lontano dall'origine. In mappa l'origine è il perno di rotazione e
// il punto del gizmo, quindi ci si ritrovava a ruotare una torre attorno a un punto
// tre metri fuori dalla torre (segnalato dall'utente, ed era questa la causa).
void MapEditor::placePartClear(const StructTab& t, mini::StructurePart& p)
{
    (void)t;
    const glm::vec3 fp = m_structVp.groundFocusPoint();
    const float x = snap(fp.x), z = snap(fp.z);
    if (p.isBox) { p.box.x  = x; p.box.z  = z; }
    else         { p.prim.x = x; p.prim.z = z; }
}

// Sposta TUTTE le parti così che il centro dell'ingombro in pianta finisca
// sull'origine. L'origine di un assemblaggio non è un dettaglio: in mappa è il perno
// di rotazione e il punto in cui compare il gizmo. Averla fuori dalla struttura
// significa ruotare attorno al vuoto.
// Esplicito e non automatico al salvataggio: spostare i dati dell'autore senza che
// l'abbia chiesto è il tipo di sorpresa che fa perdere fiducia nello strumento.
// (È l'"Origin to Geometry" di Blender, stesso gesto e stesso motivo.)
void MapEditor::centerAssemblyOrigin(StructTab& t)
{
    if (t.def.parts.empty()) return;
    std::vector<mini::MapGeometryBox> boxes;
    expandTypeForEdit(t.def, boxes);
    if (boxes.empty()) return;

    bool any = false;
    float mnx = 0, mxx = 0, mnz = 0, mxz = 0;
    for (const auto& b : boxes)
    {
        // Ingombro con la rotazione, come per le dimensioni della mappa: ignorarla
        // darebbe un centro sbagliato proprio sulle parti ruotate.
        const float a = b.ry * 3.14159265f / 180.0f;
        const float c = std::fabs(std::cos(a)), s = std::fabs(std::sin(a));
        const float hx = (b.sx * 0.5f) * c + (b.sz * 0.5f) * s;
        const float hz = (b.sx * 0.5f) * s + (b.sz * 0.5f) * c;
        if (!any) { mnx = b.x-hx; mxx = b.x+hx; mnz = b.z-hz; mxz = b.z+hz; any = true; continue; }
        mnx = std::min(mnx, b.x-hx); mxx = std::max(mxx, b.x+hx);
        mnz = std::min(mnz, b.z-hz); mxz = std::max(mxz, b.z+hz);
    }
    const float cx = (mnx + mxx) * 0.5f, cz = (mnz + mxz) * 0.5f;
    if (std::fabs(cx) < 0.001f && std::fabs(cz) < 0.001f) return;   // già centrato

    t.undo.push(t.snapshot(m_selPart), "centra origine", m_editorClock, -1.0f);
    for (auto& p : t.def.parts)
    {
        float* x = p.isBox ? &p.box.x : &p.prim.x;
        float* z = p.isBox ? &p.box.z : &p.prim.z;
        *x -= cx; *z -= cz;
    }
    t.dirty = true;
    rebuildStructTabPreview(t);
}

// Quanto è lontana l'origine dal centro dell'ingombro, in pianta. Serve a dirlo
// invece di lasciarlo scoprire piazzando la struttura in mappa.
float MapEditor::assemblyOriginOffset(const StructTab& t) const
{
    if (t.def.parts.empty()) return 0.0f;
    std::vector<mini::MapGeometryBox> boxes;
    expandTypeForEdit(t.def, boxes);
    if (boxes.empty()) return 0.0f;
    bool any = false;
    float mnx = 0, mxx = 0, mnz = 0, mxz = 0;
    for (const auto& b : boxes)
    {
        const float a = b.ry * 3.14159265f / 180.0f;
        const float c = std::fabs(std::cos(a)), s = std::fabs(std::sin(a));
        const float hx = (b.sx * 0.5f) * c + (b.sz * 0.5f) * s;
        const float hz = (b.sx * 0.5f) * s + (b.sz * 0.5f) * c;
        if (!any) { mnx = b.x-hx; mxx = b.x+hx; mnz = b.z-hz; mxz = b.z+hz; any = true; continue; }
        mnx = std::min(mnx, b.x-hx); mxx = std::max(mxx, b.x+hx);
        mnz = std::min(mnz, b.z-hz); mxz = std::max(mxz, b.z+hz);
    }
    const float cx = (mnx + mxx) * 0.5f, cz = (mnz + mxz) * 0.5f;
    return std::sqrt(cx * cx + cz * cz);
}

// Il viewport delle strutture è UNO solo, condiviso da tutti i tab: l'overlay va
// riportato a ogni cambio di tab, o si vede il navmesh della struttura precedente.
void MapEditor::applyStructNavOverlay(const StructTab& t)
{
    if (m_structShowNav && !t.navTris.empty()) m_structVp.setNavMesh(t.navTris);
    else                                       m_structVp.clearNavMesh();
}

void MapEditor::refreshStructTypeIds()
{
    m_structTypeIds.clear();
    const fs::path folder = fs::path(getDataDir()) / "structures";
    std::error_code ec;
    if (!fs::exists(folder, ec)) return;
    for (const auto& e : fs::directory_iterator(folder, ec))
        if (e.path().extension() == ".json")
            m_structTypeIds.push_back(e.path().stem().string());
    std::sort(m_structTypeIds.begin(), m_structTypeIds.end());
}

// L'impronta della RICETTA: se cambia, la verifica precedente non vale più. Stesso
// principio di `geometryFingerprint` — un flag da alzare a mano nei venti punti che
// modificano un parametro prima o poi resta basso.
std::size_t MapEditor::structFingerprint(const mini::StructureTypeDef& d)
{
    std::size_t h = (std::size_t)d.kind * 2654435761u;
    auto mix = [&h](float v) {
        h ^= std::hash<int>{}((int)std::lround(v * 1000.0f)) + 0x9e3779b9u + (h << 6) + (h >> 2);
    };
    for (const auto& info : mini::mapstructures::paramsOf(d.kind))
        mix(mini::mapstructures::getParam(d.defaults, info.p));
    mix(d.defaults.ceiling ? 1.0f : 0.0f);
    mix(d.defaults.railing ? 1.0f : 0.0f);
    for (bool a : d.defaults.access) mix(a ? 1.0f : 0.0f);
    return h;
}

void MapEditor::openStructTab(const std::string& id)
{
    // Già aperto → ci si porta sopra invece di aprirne un secondo (regola dei tab
    // di ogni browser: due schede sullo stesso documento sono un difetto).
    for (int i = 0; i < (int)m_structTabs.size(); ++i)
        if (m_structTabs[i].id == id && !id.empty())
        { m_activeTab = i + 1; m_focusLastTab = false; return; }

    StructTab t;
    t.id = id;
    if (id.empty())
    {
        t.def.label = "nuovo tipo";
        t.def.kind  = mini::StructureKind::Stair;
        t.dirty     = true;   // non esiste ancora su disco
    }
    else
    {
        // Si rilegge dal REGISTRY, cioè dallo stesso parser del gioco: un secondo
        // lettore qui divergerebbe al primo campo aggiunto.
        m_prefabReg.loadStructureTypes(getDataDir());
        if (const auto* src = m_prefabReg.getStructureType(id)) t.def = *src;
        else { t.def.id = id; t.def.label = id; }
    }
    t.def.id = id;
    t.def.defaults.kind = t.def.kind;
    m_structTabs.push_back(std::move(t));
    rebuildStructTabPreview(m_structTabs.back());
    m_focusLastTab = true;
}

// ── MODIFICARE UNA SOLA STRUTTURA IN MAPPA ───────────────────────────────────
// Richiesta testuale dell'utente (2026-08-10): *"uso la composita Tactic Bunker, ne
// piazzo 4 diverse, ma su una devo fare una modifica specifica, quindi la seleziono
// e apro l'editor per quella singola composita"*.
//
// Lo STESSO editor, con un bersaglio diverso: le modifiche finiscono nelle parti
// locali dell'istanza (file della mappa) invece che nel tipo (file di libreria). Un
// secondo editor "semplificato" avrebbe significato due strumenti che divergono, e
// il secondo sempre indietro di qualche funzione rispetto al primo.
void MapEditor::openInstanceTab(int idx)
{
    if (idx < 0 || idx >= (int)m_structures.size()) return;
    const auto& s = m_structures[idx];
    const auto* ty = s.type.empty() ? nullptr : m_prefabReg.getStructureType(s.type);
    if (!ty || !mini::mapstructures::isAssembly(*ty)) return;

    // Già aperto su questa stessa struttura → ci si porta sopra.
    for (int i = 0; i < (int)m_structTabs.size(); ++i)
        if (m_structTabs[i].target == StructTab::Target::Instance
            && m_structTabs[i].instIdx == idx)
        { m_activeTab = i + 1; m_focusLastTab = false; return; }

    StructTab t;
    t.target     = StructTab::Target::Instance;
    t.instIdx    = idx;
    t.originType = s.type;
    t.id.clear();                 // non è un file: "Salva" qui non ha senso
    t.def        = *ty;           // vincoli, categoria e primitiva di base del tipo
    t.def.id.clear();
    t.def.label  = s.label.empty() ? ty->label : s.label;
    // Si parte da dove si era rimasti: le modifiche già fatte, o il tipo la prima
    // volta. Ripartire sempre dal tipo butterebbe via il lavoro precedente.
    t.def.parts  = s.localParts.empty() ? ty->parts : s.localParts;
    t.def.verified = false;       // è geometria di mappa: la verifica è un'altra cosa
    m_structTabs.push_back(std::move(t));
    rebuildStructTabPreview(m_structTabs.back());
    m_focusLastTab = true;
    m_selPart = -1;
}

void MapEditor::applyInstanceTab(StructTab& t)
{
    if (t.target != StructTab::Target::Instance) return;
    // L'indice è un'identità POSIZIONALE (KI #100): fra l'apertura del tab e questo
    // momento la struttura può essere stata cancellata, o un'altra può aver preso il
    // suo posto. Scrivere alla cieca modificherebbe la struttura sbagliata — che è
    // peggio di non scrivere. Il tipo di origine è la controprova.
    if (t.instIdx < 0 || t.instIdx >= (int)m_structures.size()
        || m_structures[t.instIdx].type != t.originType)
    {
        t.saveError = "La struttura non c'e' piu' (o e' cambiata): riaprila dalla mappa.";
        return;
    }
    pushUndo("modifica di una struttura");
    m_structures[t.instIdx].localParts = t.def.parts;
    m_structures[t.instIdx].label      = t.def.label;
    t.dirty = false;
    t.saveError.clear();
    m_dirty = true;
    rebuildStructurePreview();
    updateViewport();
}

// Ricetta → box → viewport isolata. UNA sola espansione (`mapstructures::expand`),
// la stessa del registry e del gate: un'anteprima con codice proprio divergerebbe.
void MapEditor::rebuildStructTabPreview(StructTab& t)
{
    // La struttura si guarda da sola, all'origine: è il senso dell'isolamento.
    t.def.defaults.kind = t.def.kind;
    t.def.defaults.x = 0.0f; t.def.defaults.z = 0.0f; t.def.defaults.ry = 0.0f;

    std::vector<mini::MapGeometryBox> boxes;
    expandTypeForEdit(t.activeDef(), boxes);
    t.check.boxes = (int)boxes.size();

    // Le box si generano PARTE PER PARTE, così ognuna porta il `pickId` della sua
    // parte: cliccare nella viewport seleziona la parte, come in mappa. Il viewport
    // condiviso sapeva già fare ray-picking (`popClickedMapBox`) — mancava solo di
    // dirgli a chi appartiene ogni box.
    std::vector<FreeCameraViewport::MapBoxDraw> draws;
    draws.reserve(boxes.size() + 3);
    auto emit = [&](const std::vector<mini::MapGeometryBox>& src, int pickId, bool sel) {
        for (const auto& b : src)
        {
            FreeCameraViewport::MapBoxDraw d;
            d.x = b.x; d.y = b.y; d.z = b.z; d.ry = b.ry;
            d.sx = b.sx; d.sy = b.sy; d.sz = b.sz;
            d.r = b.r; d.g = b.g; d.b = b.b;
            d.selected = sel;
            d.pickId = pickId;
            draws.push_back(d);
        }
    };
    if (t.parts().empty())
        emit(boxes, FreeCameraViewport::MapBoxDraw::kNoPick, false);
    else
    {
        std::vector<mini::MapGeometryBox> one;
        mini::StructureDef origin;   // assemblaggio all'origine, non ruotato
        for (int i = 0; i < (int)t.parts().size(); ++i)
        {
            one.clear();
            const auto& p = t.parts()[i];
            if (p.isRef())
            {
                // Un riferimento si disegna espandendo il sottotipo nella posa della
                // parte: è la STESSA strada dell'espansione vera (`expandAssembly`),
                // altrimenti l'anteprima mostrerebbe una struttura e il navmesh ne
                // riceverebbe un'altra.
                if (const auto* sub = m_prefabReg.getStructureType(p.refType))
                {
                    mini::StructureDef sub_inst;
                    sub_inst.x = p.prim.x; sub_inst.y = p.prim.y;
                    sub_inst.z = p.prim.z; sub_inst.ry = p.prim.ry;
                    mini::mapstructures::expandAssembly(*sub, sub_inst, one, m_typeResolver);
                }
            }
            else if (p.isBox) one.push_back(p.box);
            else              mini::mapstructures::expand(p.prim, one);
            for (auto& b : one)
                b = mini::mapstructures::transformPartBox(b, origin.x, 0.0f, origin.z, 0.0f);
            emit(one, i, i == m_selPart);
        }
    }

    // Le due FIGURE DI SCALA sono state tolte da qui (2026-08-08, richiesta utente:
    // *"sono solo un ingombro"*). Servivano a dare una misura di riferimento quando
    // non c'era altro, ma da allora sono arrivate la barra di scala, le coordinate ai
    // bordi e il righello — che dicono le misure in NUMERI invece che per confronto
    // visivo. Due sagome fisse accanto alla struttura erano rimaste solo un ostacolo
    // fra sé e la telecamera. Restano nel Map Editor, dove si attivano su richiesta.

    // ── L'ORIGINE, visibile ──────────────────────────────────────────────
    // Un perno invisibile è un concetto astratto finché non si sbaglia; disegnato,
    // si vede subito se cade fuori dalla struttura. Croce bassa e sottile
    // sull'origine dell'assemblaggio, in ciano.
    {
        auto mark = [&](float sx, float sz) {
            FreeCameraViewport::MapBoxDraw m;
            m.x = 0.0f; m.y = 0.05f; m.z = 0.0f; m.ry = 0.0f;
            m.sx = sx; m.sy = 0.10f; m.sz = sz;
            m.r = 0.35f; m.g = 0.90f; m.b = 1.00f;
            m.selected = false;
            m.pickId = FreeCameraViewport::MapBoxDraw::kNoPick;
            draws.push_back(m);
        };
        mark(3.0f, 0.12f);
        mark(0.12f, 3.0f);
    }

    m_structVp.setMapBoxes(draws);
    t.fingerprint = structFingerprint(t.def);
    t.check.stale = true;
}

// La verifica: il navmesh VERO sulla struttura ISOLATA, posata su un piano neutro.
// È la domanda che in mappa non si riesce a fare — lì la risposta è mescolata ad
// altre 167 box (KI #97). Qui è una struttura sola, quindi la risposta è netta.
void MapEditor::checkStructType(StructTab& t)
{
    const auto t0 = std::chrono::steady_clock::now();
    t.check = StructTab::Check{};
    t.check.run = true;

    std::vector<mini::MapGeometryBox> boxes;
    expandTypeForEdit(t.activeDef(), boxes);
    t.check.boxes = (int)boxes.size();

    // Ingombro, per dimensionare il piano d'appoggio e piazzare lo spawn.
    float minX = 0, maxX = 0, minZ = 0, maxZ = 0, minY = 0;
    for (const auto& b : boxes)
    {
        minX = std::min(minX, b.x - b.sx * 0.5f); maxX = std::max(maxX, b.x + b.sx * 0.5f);
        minZ = std::min(minZ, b.z - b.sz * 0.5f); maxZ = std::max(maxZ, b.z + b.sz * 0.5f);
        minY = std::min(minY, b.y - b.sy * 0.5f);
    }
    const float pad = 6.0f;

    mini::MapDef tmp;
    tmp.geometry.reserve(boxes.size() + 1);
    // Il piano: senza un suolo la struttura è sospesa e ogni gradino è un'isola —
    // si misurerebbe un difetto inventato dal banco di prova, non dalla struttura.
    // La quota del piano è la BASE della struttura, non il suo punto più basso: per
    // una piattaforma sospesa a 3 m, "sotto il punto più basso" incollerebbe il
    // pavimento sotto il ripiano e la renderebbe raggiungibile per finta. È
    // esattamente il modo in cui un banco di prova inventa il risultato che misura.
    const float groundTop = std::min(0.0f, minY);
    mini::MapGeometryBox ground;
    ground.x = (minX + maxX) * 0.5f;  ground.z = (minZ + maxZ) * 0.5f;
    ground.sx = (maxX - minX) + pad * 2.0f;
    ground.sz = (maxZ - minZ) + pad * 2.0f;
    ground.sy = 0.5f;  ground.y = groundTop - 0.25f;
    ground.type = mini::BoxType::Floor;
    ground.collider = true;
    tmp.geometry.push_back(ground);
    for (const auto& b : boxes) tmp.geometry.push_back(b);

    const glm::vec3 spawn = { minX - pad * 0.5f, groundTop + 0.4f, (minZ + maxZ) * 0.5f };
    tmp.spawnTeam1 = { spawn.x, spawn.y, spawn.z };
    tmp.spawnTeam2 = tmp.spawnTeam1;

    const mini::NavBuildStats st = m_structNav.build(tmp);
    if (!st.ok)
    {
        t.check.seconds =
            std::chrono::duration<float>(std::chrono::steady_clock::now() - t0).count();
        t.check.stale = false;
        m_structVp.clearNavMesh();
        return;
    }

    std::vector<mini::NavManager::DebugTri> tris;
    int nComp = 0;
    m_structNav.debugTriangles(tris, &nComp);
    t.check.tris       = (int)tris.size();
    t.check.components = nComp;
    const int mainComp = m_structNav.componentAt(spawn);

    std::vector<FreeCameraViewport::NavTriDraw> draw;
    draw.reserve(tris.size());
    for (const auto& tr : tris)
    {
        const bool ok = (tr.component == mainComp);
        FreeCameraViewport::NavTriDraw d;
        d.ax = tr.a.x; d.ay = tr.a.y; d.az = tr.a.z;
        d.bx = tr.b.x; d.by = tr.b.y; d.bz = tr.b.z;
        d.cx = tr.c.x; d.cy = tr.c.y; d.cz = tr.c.z;
        if (ok) { d.r = 0.25f; d.g = 0.85f; d.b = 0.40f; }
        else    { d.r = 0.95f; d.g = 0.30f; d.b = 0.25f; }
        draw.push_back(d);
    }
    // I triangoli si CONSERVANO nel tab, così l'interruttore può riaccenderli senza
    // ricostruire il navmesh (che costa ~0,1 s). Prima venivano dati al viewport e
    // basta: il viewport è uno solo, quindi restavano addosso anche cambiando tab.
    t.navTris = std::move(draw);
    m_structShowNav = true;      // appena verificato, si guarda
    applyStructNavOverlay(t);

    // ── Il SINTOMO: quanta superficie dichiarata calpestabile sopravvive ──
    // Non "quanti triangoli": quanti METRI QUADRI di ciò che l'autore ha dichiarato
    // calpestabile si possono davvero raggiungere. È la cosa che si rompe.
    // E la SINGOLA entità: quale box resta muto (doc 48 §Osservabilità).
    for (std::size_t i = 0; i < boxes.size(); ++i)
    {
        const auto& b = boxes[i];
        if (!mini::boxShouldBeReachable(b.type)) continue;
        const float area = b.sx * b.sz;
        t.check.declaredArea += area;
        const glm::vec3 top = { b.x, b.y + b.sy * 0.5f + 0.1f, b.z };
        if (m_structNav.isReachable(spawn, top)) t.check.navArea += area;
        else
        {
            char m[96];
            std::snprintf(m, sizeof(m), "box #%d  %.1f x %.1f a quota %.2f",
                          (int)i, b.sx, b.sz, b.y + b.sy * 0.5f);
            t.check.mute.emplace_back(m);
        }
    }

    t.check.seconds =
        std::chrono::duration<float>(std::chrono::steady_clock::now() - t0).count();
    t.check.stale = false;
    t.fingerprint = structFingerprint(t.def);

    // ── Cosa significa "verificata" dipende da COSA È la struttura ────────
    // Un muro, una porta e una barricata non dichiarano superficie calpestabile:
    // pretendere che ne abbiano li renderebbe **per sempre** non verificabili, e un
    // marchio che non si può mai ottenere non è un controllo, è rumore.
    // Per loro la domanda giusta è l'opposta: non ostruiscono tutto? Il piano di
    // prova deve restare percorribile attorno.
    // E il numero di componenti non è un difetto per loro: un muro che divide in due
    // il piano sta facendo esattamente il suo mestiere.
    t.check.hasWalkable = (t.check.declaredArea > 0.001f);
    const bool wasVerified = t.def.verified;
    t.def.verified = t.check.hasWalkable ? t.check.mute.empty()
                                         : (t.check.tris > 0);

    // L'esito si SALVA da solo, e il registry si ricarica.
    // Prima l'esito restava in memoria: nella Libreria il tipo continuava a comparire
    // in giallo "non verificata" anche dopo una verifica riuscita, perché il menu
    // legge dal REGISTRY e il registry legge dal FILE. L'utente ha verificato e
    // "non è cambiato nulla" — giustamente.
    // `verified` è un RISULTATO, non una scelta d'autore: chiedere un salvataggio
    // per conservarlo sarebbe un passo che nessuno indovina.
    if (wasVerified != t.def.verified)
    {
        if (!t.id.empty()) saveStructType(t);   // salva + ricarica la libreria
        else               t.dirty = true;      // tipo mai salvato: lo decide l'utente
    }

    // L'esito anche su stdout: la verifica è un'azione su richiesta, quindi una
    // riga non costa nulla — e rende l'esito leggibile senza guardare lo schermo,
    // cioè verificabile da riga di comando (§5-bis).
    std::printf("[Struttura] %s (%s): %d box, %d tri, %d componenti, "
                "%.1f/%.1f m2 raggiungibili, %d muti -> %s\n",
                t.def.label.empty() ? "(senza nome)" : t.def.label.c_str(),
                mini::mapstructures::kindName(t.def.kind),
                t.check.boxes, t.check.tris, t.check.components,
                t.check.navArea, t.check.declaredArea, (int)t.check.mute.size(),
                t.def.verified ? "VERIFICATA" : "NON verificata");
}

// Un id da un nome leggibile: minuscole, e tutto ciò che non è alfanumerico
// diventa `_`. È la stessa regola del primo salvataggio (id = filename stem,
// ADR-001), estratta perché ora la usano in due.
std::string MapEditor::idFromLabel(const std::string& label)
{
    std::string s;
    for (char c : label)
        s += std::isalnum((unsigned char)c) ? (char)std::tolower((unsigned char)c) : '_';
    while (!s.empty() && s.back() == '_') s.pop_back();
    return s.empty() ? std::string("struttura") : s;
}

// ── SALVA COME COPIA ────────────────────────────────────────────────────────
// Semantica di un "Salva con nome" fatto bene, che è meno ovvia di quanto sembri:
//   · l'ORIGINALE su disco non viene toccato — è tutto il punto;
//   · il TAB passa a lavorare sulla copia, altrimenti il salvataggio successivo
//     tornerebbe a sovrascrivere l'originale, che è la trappola classica;
//   · la copia nasce NON VERIFICATA, perché la sua geometria può già essere diversa
//     da quella che era stata verificata. Marcarla verificata sarebbe una
//     dichiarazione che nessuno ha controllato.
void MapEditor::drawSaveAsCopyPopup(StructTab& t)
{
    // Apertura e disegno nella stessa funzione (doc 52 F4): l'ID non può divergere.
    if (m_copyOpen && !ImGui::IsPopupOpen("Salva come copia"))
        ImGui::OpenPopup("Salva come copia");
    if (!m_copyOpen) return;

    if (ImGui::BeginPopupModal("Salva come copia", nullptr,
                               ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::TextUnformatted("Nome della variante:");
        ImGui::SetNextItemWidth(320.0f);
        ImGui::InputText("##copyname", m_copyName, sizeof(m_copyName));
        const std::string newId = idFromLabel(m_copyName);
        ImGui::TextDisabled("file: %s.json", newId.c_str());
        if (!t.id.empty())
            ImGui::TextDisabled("l'originale \"%s\" resta invariato", t.id.c_str());
        if (!m_copyError.empty())
            ImGui::TextColored(ImVec4(0.95f, 0.35f, 0.30f, 1.0f), "%s", m_copyError.c_str());

        ImGui::Separator();
        if (ImGui::Button("Crea la copia", {150, 0}))
        {
            std::error_code ec;
            const std::string path = getDataDir() + "/structures/" + newId + ".json";
            if (newId == t.id)
                m_copyError = "E' lo stesso nome dell'originale: cambialo.";
            else if (fs::exists(path, ec))
                m_copyError = "Esiste gia' un tipo con questo nome.";
            else
            {
                t.id            = newId;
                t.def.id        = newId;
                t.def.label     = m_copyName;
                t.def.verified  = false;   // la geometria puo' essere gia' cambiata
                t.undo.clear();            // la cronologia era dell'originale
                // Promozione da modifica d'istanza a TIPO vero: da qui in poi il tab
                // lavora sulla libreria. Lasciarlo legato all'istanza significherebbe
                // due bersagli per lo stesso "Salva" — e uno dei due sbagliato.
                t.target  = StructTab::Target::Library;
                t.instIdx = -1;
                t.isolated = -1;
                saveStructType(t);
                m_copyOpen = false;
                ImGui::CloseCurrentPopup();
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Annulla", {110, 0}))
        { m_copyOpen = false; ImGui::CloseCurrentPopup(); }
        ImGui::EndPopup();
    }
    else m_copyOpen = false;   // rete di sicurezza: nessuna intenzione appesa
}

// Salvataggio READ-MODIFY-WRITE (CLAUDE.md §2): si legge il file, si toccano solo i
// propri campi, si riscrive. Costruire un json nuovo da zero ha già causato una
// perdita dati reale su questo progetto.
void MapEditor::saveStructType(StructTab& t)
{
    // Nome file dal label, normalizzato: id = filename stem (ADR-001). Una sola
    // regola, condivisa con "Salva come copia": due normalizzazioni diverse
    // porterebbero lo stesso nome a due file diversi.
    if (t.id.empty()) t.id = idFromLabel(t.def.label);
    // Lo slash NON è opzionale: `getDataDir()` non ce l'ha, e senza il percorso
    // diventava `.../datastructures/...`. Il salvataggio falliva e lo diceva solo su
    // stderr — l'utente vedeva la verifica riuscire e il tipo restare "non
    // verificata", senza nessun indizio del perché.
    const std::string path = getDataDir() + "/structures/" + t.id + ".json";
    std::error_code mkec;
    fs::create_directories(fs::path(getDataDir()) / "structures", mkec);

    const bool ok = editor::jsonsave::saveJsonRMW(path, [&](nlohmann::json& j) {
        j["label"]    = t.def.label;
        j["kind"]     = mini::mapstructures::kindName(t.def.kind);
        j["note"]     = t.def.note;
        j["category"] = t.def.category;
        j["verified"] = t.def.verified;
        // NESSUN "id" dentro il file: id = filename stem (ADR-001).
        j.erase("id");

        nlohmann::json& d = j["defaults"];
        for (const auto& info : mini::mapstructures::paramsOf(t.def.kind))
            d[info.key] = mini::mapstructures::getParam(t.def.defaults, info.p);
        d["ceiling"] = t.def.defaults.ceiling;
        d["railing"] = t.def.defaults.railing;
        d["access"]  = { t.def.defaults.access[0], t.def.defaults.access[1],
                         t.def.defaults.access[2], t.def.defaults.access[3] };

        // ── Parti dell'assemblaggio (ADR-056) ────────────────────────────
        // Si riscrive l'intero array: le parti sono un elenco ordinato, e una fusione
        // "campo per campo" con quello su disco produrrebbe parti fantasma quando se
        // ne cancella una. Il RMW protegge il RESTO del file, che è il suo scopo.
        if (t.def.parts.empty()) j.erase("parts");
        else
        {
            nlohmann::json arr = nlohmann::json::array();
            // Scrittore UNICO (`mini::structjson`): finché stava qui a mano, il
            // lettore del registry e questo divergevano al primo campo nuovo. È
            // successo con `type`, e sarebbe successo con `ref`.
            for (const auto& p : t.def.parts)
                arr.push_back(mini::structjson::partToJson(p));
            j["parts"] = std::move(arr);
        }

        nlohmann::json& r = j["rules"];
        for (const auto& info : mini::mapstructures::paramsOf(t.def.kind))
        {
            const auto& rule = t.def.rules[(std::size_t)info.p];
            r[info.key]["editable"] = rule.editable;
            r[info.key]["min"]      = rule.min;
            r[info.key]["max"]      = rule.max;
        }
        return true;
    });

    // Un salvataggio fallito NON deve passare in silenzio: era il caso di
    // `datastructures/`, dove l'errore finiva solo su stderr e dall'editor sembrava
    // che la verifica non servisse a niente. Lo stato "modificato" resta acceso, così
    // la stella nel titolo del tab continua a dire che c'è del lavoro non salvato.
    if (!ok)
    {
        t.saveError = "Salvataggio FALLITO: " + path;
        std::fprintf(stderr, "[MapEditor] %s\n", t.saveError.c_str());
        return;
    }
    t.saveError.clear();

    t.dirty = false;
    refreshStructTypeIds();
    // Il registry va RILETTO: la Libreria del menu `+ Struttura` mostra quello, non
    // lo stato del tab. Senza, un tipo appena salvato continuava a comparire con i
    // valori vecchi (compreso il giallo "non verificata") fino al riavvio.
    m_prefabReg.loadStructureTypes(getDataDir());
}

// Il tab di un tipo: a sinistra i parametri e i loro vincoli, al centro la struttura
// isolata, a destra la verifica. Nessuna mappa attorno — è l'isolamento di Prefab Mode.
void MapEditor::drawStructTab(StructTab& t, float totalW, float totalH)
{
    const bool instMode = (t.target == StructTab::Target::Instance);

    // ── DI CHE COSA SI STA DECIDENDO LA SORTE ─────────────────────────────
    // Il tab in modo ISTANZA e quello di libreria si assomigliano troppo perché la
    // differenza possa restare implicita: uno cambia una struttura, l'altro le
    // cambia tutte. La riga colorata in cima è l'unica difesa contro il gesto giusto
    // fatto nel posto sbagliato.
    if (instMode)
    {
        ImGui::TextColored({1.00f, 0.80f, 0.35f, 1.0f},
                           "MODIFICA DI UNA SOLA STRUTTURA IN MAPPA");
        ImGui::SameLine();
        ImGui::TextDisabled("(deriva da \"%s\" — il tipo NON viene toccato)",
                            t.originType.c_str());
    }

    // ── Barra del tipo ────────────────────────────────────────────────────
    char lbl[128];
    std::snprintf(lbl, sizeof(lbl), "%s", t.def.label.c_str());
    ImGui::SetNextItemWidth(220.0f);
    if (ImGui::InputText(instMode ? "Nome di questa struttura" : "Nome del tipo",
                         lbl, sizeof(lbl)))
    { t.def.label = lbl; t.dirty = true; }
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip(instMode
            ? "Solo l'etichetta di questa struttura sulla mappa."
            : "Come si chiamera' nella Libreria del menu \"+ Struttura\".\n"
              "Al primo salvataggio diventa anche il nome del file.");
    if (!instMode)
    {
    ImGui::SameLine();
    char cat[64];
    std::snprintf(cat, sizeof(cat), "%s", t.def.category.c_str());
    ImGui::SetNextItemWidth(150.0f);
    if (ImGui::InputTextWithHint("Categoria", "es. Torri", cat, sizeof(cat)))
    { t.def.category = cat; t.dirty = true; }
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Raggruppa la Libreria. Scrivi quello che vuoi: una categoria\n"
                          "nuova compare da sola, senza dover cambiare il codice.");
    }   // categoria: solo in libreria — un'istanza non sta in nessuna categoria

    // Il combo della primitiva di base NON si mostra su un assemblaggio: lì ogni
    // parte ha la sua, e quella "del tipo" non governa nulla. Lasciarlo visibile
    // sembrava che aggiungere una parte SOSTITUISSE la primitiva scelta — confusione
    // segnalata dall'utente, e giustamente: un comando che non fa nulla ma cambia
    // aspetto è peggio di un comando assente.
    ImGui::SameLine();
    if (!t.parts().empty())
    {
        ImGui::TextColored(ImVec4(0.55f, 0.85f, 1.0f, 1.0f), "ASSEMBLAGGIO");
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Ogni parte ha la sua primitiva: si scelgono qui sotto,\n"
                              "nell'elenco delle parti.");
    }
    else {
    ImGui::SetNextItemWidth(180.0f);
    const char* kn = mini::mapstructures::kindName(t.def.kind);
    if (ImGui::BeginCombo("Primitiva", kn))
    {
        for (int k = 0; k <= (int)mini::StructureKind::Barricade; ++k)
        {
            const auto kk = (mini::StructureKind)k;
            // Switchback resta fuori dal menu finché il vano scala non è risolto
            // (doc 47: 3 torri su 6 non percorribili). Un tipo su una primitiva
            // rotta sarebbe una libreria che promette ciò che non mantiene.
            if (kk == mini::StructureKind::Switchback) continue;
            const bool sel = (t.def.kind == kk);
            if (ImGui::Selectable(mini::mapstructures::kindName(kk), sel))
            {
                t.def.kind = kk;
                t.def.defaults.kind = kk;
                t.dirty = true;
                rebuildStructTabPreview(t);
            }
            if (sel) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }
    }   // fine del ramo "tipo a primitiva singola"

    // ── I COMANDI su una riga LORO ────────────────────────────────────────
    // Prima nome, categoria, primitiva, Salva, Salva come copia, Verifica e la
    // spunta del navmesh stavano tutti in fila: otto controlli che il pannello non
    // può contenere, quindi gli ultimi finivano tagliati fuori dal bordo. È la
    // regola d'uso già confermata dall'utente — mai far tagliare i comandi.
    ImGui::Separator();

    // Il tipo che si sta SOVRASCRIVENDO, sempre in chiaro accanto al pulsante: è
    // l'unica difesa contro il "volevo farne una variante e ho salvato sull'originale".
    if (instMode)
    {
        // "Applica", non "Salva": non si scrive nessun file di libreria. Il nome del
        // comando deve dire dove finisce la roba, o l'utente presume il posto
        // sbagliato — ed è il presupposto sbagliato più costoso possibile qui.
        if (ImGui::Button("Applica alla struttura")) applyInstanceTab(t);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Scrive queste parti dentro QUELLA struttura in mappa.\n"
                              "Nessun file di libreria viene toccato: la modifica sta\n"
                              "nella mappa, e va salvata con la mappa.");
        ImGui::SameLine();
        if (ImGui::Button("Promuovi a tipo di libreria..."))
        {
            std::snprintf(m_copyName, sizeof(m_copyName), "%s variante",
                          t.originType.empty() ? "struttura" : t.originType.c_str());
            m_copyError.clear();
            m_copyOpen = true;
        }
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Se la modifica serve anche altrove: ne fa un tipo vero,\n"
                              "riusabile. Da quel momento il tab lavora sul tipo.");
        drawSaveAsCopyPopup(t);
        if (!t.saveError.empty())
        {
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(0.95f, 0.35f, 0.30f, 1.0f), "%s", t.saveError.c_str());
        }
        ImGui::SameLine();
    }
    else {
    if (ImGui::Button("Salva")) saveStructType(t);
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip(t.id.empty() ? "Primo salvataggio: crea il file dal nome."
                                       : "Sovrascrive '%s'.", t.id.c_str());
    ImGui::SameLine();
    // ── SALVA COME COPIA (richiesta utente 2026-08-08) ───────────────────
    // *"devo poter prendere quella struttura e poterla modificare ma in una copia,
    // per poter creare facilmente variazioni"*.
    // UNA sola strada e non due: l'utente ha già chiesto in passato di non avere due
    // modi per la stessa cosa ("almeno non si fa confusione"). Questa le copre
    // entrambe — si può decidere di fare una variante PRIMA di aprire (apri, salva
    // subito come copia) o DOPO averla modificata, che è il caso in cui serve
    // davvero, perché è quello in cui altrimenti si sovrascriverebbe l'originale.
    if (ImGui::Button("Salva come copia..."))
    {
        std::snprintf(m_copyName, sizeof(m_copyName), "%s variante",
                      t.def.label.empty() ? "tipo" : t.def.label.c_str());
        m_copyError.clear();
        m_copyOpen = true;
    }
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Crea una VARIANTE: l'originale resta com'e' su disco e il\n"
                          "tab passa a lavorare sulla copia. E' il modo per fare piu'\n"
                          "versioni di una struttura complessa senza rifarle da zero.");
    drawSaveAsCopyPopup(t);
    if (!t.saveError.empty())
    {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.95f, 0.35f, 0.30f, 1.0f), "%s", t.saveError.c_str());
    }
    ImGui::SameLine();
    }   // fine del ramo LIBRERIA

    if (ImGui::Button("Verifica")) checkStructType(t);
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Costruisce il navmesh VERO sulla struttura da sola,\n"
                          "posata su un piano neutro. In mappa la stessa domanda\n"
                          "ha la risposta mescolata ad altre 167 box.");
    // Il verde del navmesh si SPEGNE. Prima restava acceso per sempre — anche
    // cambiando tab e aprendo un'altra struttura, che mostrava il navmesh di quella
    // precedente. Un risultato di verifica che non si può togliere smette di essere
    // un'informazione e diventa un ostacolo alla costruzione.
    ImGui::SameLine();
    if (ImGui::Checkbox("Mostra navmesh", &m_structShowNav))
        applyStructNavOverlay(t);
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Spegne il verde senza perdere l'esito della verifica,\n"
                          "che resta scritto nel pannello di destra.");

    // Quante istanze in mappa userebbero questo tipo — la lezione di AutoCAD
    // REFEDIT: ridefinire tocca ogni inserzione, e va detto PRIMA.
    if (!t.id.empty())
    {
        int uses = 0;
        for (const auto& s : m_structures) if (s.type == t.id) ++uses;
        if (uses > 0)
        {
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(0.95f, 0.75f, 0.30f, 1.0f),
                               "usato da %d strutture in questa mappa", uses);
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Stringere un limite riporta nei nuovi limiti anche\n"
                                  "le strutture gia' piazzate, al prossimo caricamento.");
        }
    }

    const float remaining = totalH - ImGui::GetItemRectSize().y
                          - ImGui::GetStyle().ItemSpacing.y * 2 - 8.0f;
    // Larghezze RIDIMENSIONABILI, come nel tab Mappa. Erano fisse (320 e 300) e
    // quindi il pannello dei parametri tagliava i testi lunghi senza che si potesse
    // fare nulla — segnalato dall'utente. `panelSplitter` esiste dal changelog 169
    // e non l'avevo usato proprio qui.
    static float s_paramW = 320.0f;
    static float s_checkW = 300.0f;
    const float paramW = s_paramW;

    ImGui::BeginChild("##struct_panels", ImVec2(totalW, remaining));

    // ── Parametri e vincoli ───────────────────────────────────────────────
    ImGui::BeginChild("##struct_params", ImVec2(paramW, 0), ImGuiChildFlags_Borders);

    // ── PARTI DELL'ASSEMBLAGGIO (ADR-056) ─────────────────────────────────
    // Un tipo senza parti resta quello di ADR-055 — una primitiva sola — e sotto si
    // editano i suoi parametri. Appena si aggiunge una parte diventa un
    // ASSEMBLAGGIO, e da lì si edita la parte selezionata.
    {
        const bool assembly = !t.parts().empty();

        // ── SI STA LAVORANDO DENTRO UNA COPIA ────────────────────────────
        // Una modalità di cui non si vede il confine è una trappola: si crede di
        // modificare la struttura e si sta modificando una sua parte, o viceversa.
        // La fascia e il tasto di uscita stanno SOPRA i comandi delle parti, cioè
        // prima di qualunque cosa si possa premere per sbaglio.
        if (t.isolated >= 0 && t.isolated < (int)t.def.parts.size())
        {
            const auto& host = t.def.parts[t.isolated];
            ImGui::TextColored({1.00f, 0.80f, 0.35f, 1.0f}, "DENTRO \"%s\"",
                               host.label.empty() ? host.refType.c_str()
                                                  : host.label.c_str());
            ImGui::TextDisabled("Modifichi solo QUESTA copia. La struttura \"%s\" in\n"
                                "libreria non cambia, e nemmeno le sue altre copie.",
                                host.refType.c_str());
            if (ImGui::Button("Fine — richiudi in un oggetto solo"))
            { t.isolated = -1; m_selPart = -1; rebuildStructTabPreview(t); }
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Torna a vedere la struttura intera. Le modifiche\n"
                                  "restano dentro questa copia, che d'ora in poi porta\n"
                                  "l'asterisco nell'elenco.");
            ImGui::SameLine();
            if (ImGui::Button("Annulla le modifiche"))
            {
                t.undo.push(t.snapshot(-1), "annulla isolamento", m_editorClock, -1.0f);
                t.def.parts[t.isolated].localParts.clear();
                t.isolated = -1; m_selPart = -1;
                t.dirty = true; rebuildStructTabPreview(t);
            }
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Butta via le modifiche di questa copia: torna a\n"
                                  "essere un riferimento puro, uguale all'originale.");
            ImGui::Separator();
        }

        // SEMPRE APERTA. Era un'intestazione chiusa che diceva "Parti (0)": l'utente
        // ha cercato gli assemblaggi e ha concluso *"nell'editor strutture non è
        // cambiato nulla"*. Una capacità dietro un cassetto che nessuno apre non
        // esiste — è la stessa lezione di ADR-023 sui dropdown incompleti.
        ImGui::TextColored(ImVec4(0.55f, 0.85f, 1.0f, 1.0f),
                           assembly ? "ASSEMBLAGGIO — %d parti"
                                    : "Parti dell'assemblaggio", (int)t.parts().size());
        if (!assembly)
            ImGui::TextWrapped(
                "Questo tipo e' una PRIMITIVA SOLA. Aggiungi una parte per farne una "
                "struttura composta (torre, bunker, edificio): le parti si posizionano "
                "una rispetto all'altra e la verifica navmesh gira sull'INSIEME.");
        else
        {
            // L'origine è il perno: se è fuori dalla struttura, in mappa si ruota
            // attorno al vuoto e il gizmo compare in un punto scomodo. Lo si dice
            // QUI, dove si può ancora rimediare con un clic.
            const float off = assemblyOriginOffset(t);
            if (off > 0.5f)
            {
                // TESTO A CAPO e pulsante su una RIGA SUA. Prima erano sulla stessa
                // riga: il testo riempiva già la larghezza del pannello, quindi si
                // tagliava e spingeva il pulsante fuori dal bordo — il comando
                // esisteva e non si vedeva (segnalato dall'utente).
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.95f, 0.75f, 0.30f, 1.0f));
                ImGui::TextWrapped("origine a %.1f m dal centro", off);
                ImGui::PopStyleColor();
                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip(
                        "In mappa l'origine e' il PERNO di rotazione e il punto dove\n"
                        "compare il gizmo. Lontana dalla struttura significa ruotarla\n"
                        "attorno a un punto vuoto e avere le frecce fuori posto.");
            }
            if (ImGui::Button("Centra origine")) centerAssemblyOrigin(t);
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Sposta tutte le parti insieme, cosi' l'origine cade\n"
                                  "al centro della struttura. Non cambia la forma:\n"
                                  "cambia dove sta il perno.");
        }
        {
            // ── UN SOLO TASTO PER AGGIUNGERE ─────────────────────────────
            // Erano tre ("+ Primitiva", "+ Box", "+ Composita") in fila, e la fila
            // cresceva a ogni tipo nuovo di parte: il modo garantito per far tagliare
            // i comandi quando il pannello si stringe. Uno solo con la tendina: la
            // riga non cresce più, e l'elenco di cosa si può aggiungere sta in un
            // posto dichiarato invece che nella larghezza disponibile.
            if (ImGui::Button("+ Aggiungi   v"))
            { refreshStructTypeIds(); ImGui::OpenPopup("##addany"); }
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Primitiva, box libero, o un'altra struttura composita.");

            if (ImGui::BeginPopup("##addany"))
            {
                if (ImGui::BeginMenu("Primitiva"))
                {
                    for (int k = 0; k <= (int)mini::StructureKind::Barricade; ++k)
                    {
                        const auto kk = (mini::StructureKind)k;
                        if (kk == mini::StructureKind::Switchback) continue;  // non consegnata
                        if (ImGui::MenuItem(mini::mapstructures::kindLabel(kk)))
                        {
                            t.undo.push(t.snapshot(m_selPart), "+primitiva", m_editorClock);
                            mini::StructurePart p;
                            p.isBox = false;
                            p.prim.kind = kk;
                            p.label = mini::mapstructures::kindName(kk);
                            placePartClear(t, p);
                            t.parts().push_back(std::move(p));
                            m_selPart = (int)t.parts().size() - 1;
                            t.dirty = true; rebuildStructTabPreview(t);
                        }
                    }
                    ImGui::EndMenu();
                }
                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip("Scala, muro, piattaforma...\n"
                                      "Le misure restano garantite dalla primitiva.");

                if (ImGui::MenuItem("Box libero"))
                {
                    t.undo.push(t.snapshot(m_selPart), "+box", m_editorClock);
                    mini::StructurePart p;
                    p.isBox = true;
                    p.label = "box";
                    p.box.sx = 2.0f; p.box.sy = 2.0f; p.box.sz = 2.0f;
                    p.box.y  = 1.0f;
                    p.box.type = mini::BoxType::Wall;
                    placePartClear(t, p);
                    t.parts().push_back(std::move(p));
                    m_selPart = (int)t.parts().size() - 1;
                    t.dirty = true; rebuildStructTabPreview(t);
                }
                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip("Per cio' che nessuna primitiva esprime —\n"
                                      "contrafforti, parapetti storti, feritoie.");

                // ── Composita: un RIFERIMENTO, non una copia ─────────────
                // ADR-056 vietava di annidare; l'utente ha chiesto il contrario
                // ("preferirei le lasciassi normali") e ha ragione sull'uso: una
                // torre corretta una volta va corretta ovunque, altrimenti la
                // libreria diventa un archivio di copie divergenti.
                // Il motivo del divieto resta vero però (l'annidamento moltiplica i
                // modi in cui il navmesh si rompe), e resta pagato in tre modi:
                // catena anti-ciclo, tetto di profondità, e **solo composite
                // VERIFICATE** — cioè solo pezzi di cui si sa già che il navmesh li
                // attraversa da soli.
                if (ImGui::BeginMenu("Composita"))
                {
                    int shown = 0, hidden = 0;
                    for (const auto& id : m_structTypeIds)
                    {
                        if (id == t.id) continue;                  // non se stessa
                        const auto* src = m_prefabReg.getStructureType(id);
                        if (!src || !mini::mapstructures::isAssembly(*src)) continue;

                        // Perché NON si può, detto prima del clic invece che con una
                        // parte che sparisce dopo.
                        const char* why = nullptr;
                        if (!src->verified)
                            why = "non verificata: verificala nel suo tab";
                        else if (mini::mapstructures::assemblyUses(*src, t.id, m_typeResolver))
                            why = "contiene questa struttura: si annidderebbe in se stessa";
                        else if (1 + mini::mapstructures::assemblyDepth(*src, m_typeResolver)
                                 > mini::mapstructures::kMaxAssemblyDepth)
                            why = "troppi livelli di annidamento";

                        char lbl[192];
                        std::snprintf(lbl, sizeof(lbl), "%s  (%d parti)",
                                      src->label.empty() ? id.c_str() : src->label.c_str(),
                                      (int)src->parts.size());
                        if (why)
                        {
                            ++hidden;
                            ImGui::TextDisabled("%s", lbl);
                            if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", why);
                            continue;
                        }
                        ++shown;
                        if (ImGui::MenuItem(lbl))
                        {
                            t.undo.push(t.snapshot(m_selPart), "+composita",
                                        m_editorClock, -1.0f);
                            mini::StructurePart p;
                            p.isBox   = false;
                            p.refType = id;
                            p.prim.kind = src->kind;   // coerenza, non usata dal ref
                            p.label = src->label.empty() ? id : src->label;
                            placePartClear(t, p);
                            t.parts().push_back(std::move(p));
                            m_selPart = (int)t.parts().size() - 1;
                            t.dirty = true; rebuildStructTabPreview(t);
                        }
                    }
                    if (shown == 0 && hidden == 0)
                        ImGui::TextDisabled("Nessun'altra struttura composita in libreria.");
                    ImGui::EndMenu();
                }
                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip("Un'altra struttura intera, per RIFERIMENTO:\n"
                                      "correggendo l'originale, cambia anche qui.\n"
                                      "Solo composite gia' VERIFICATE.");
                ImGui::EndPopup();
            }

            ImGui::SameLine();
            if (ImGui::Button("Duplica") && m_selPart >= 0
                && m_selPart < (int)t.parts().size())
            {
                t.undo.push(t.snapshot(m_selPart), "duplica parte", m_editorClock, -1.0f);
                mini::StructurePart cp = t.parts()[m_selPart];
                // La copia nasce SPOSTATA di un metro: sovrapposta all'originale non
                // si vedrebbe, e si crederebbe che il comando non abbia funzionato.
                if (cp.isBox) cp.box.x  += 1.0f;
                else          cp.prim.x += 1.0f;
                t.parts().push_back(std::move(cp));
                m_selPart = (int)t.parts().size() - 1;
                t.dirty = true;
                rebuildStructTabPreview(t);
            }
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Copia la parte selezionata, spostata di un metro.");

            ImGui::SameLine();
            if (ImGui::Button("- Parte") && m_selPart >= 0
                && m_selPart < (int)t.parts().size())
            {
                t.undo.push(t.snapshot(m_selPart), "-parte", m_editorClock);
                t.parts().erase(t.parts().begin() + m_selPart);
                m_selPart = -1;
                t.dirty = true; rebuildStructTabPreview(t);
            }

            // ── ESPLODI: da oggetto unico a insieme di parti ─────────────
            // Il riferimento è la forma giusta finché la struttura va bene com'è.
            // Quando serve cambiarne un pezzo *qui e solo qui*, l'alternativa era
            // duplicare l'intero tipo in libreria per una modifica di mezzo metro.
            // Esplodere scioglie il legame e lascia le parti vere, modificabili una
            // per una: da lì in poi sono roba di questa struttura, e l'originale non
            // le tocca più. Il contrario si fa con Ctrl+Z (o rimettendo il ref).
            const bool refSel = m_selPart >= 0 && m_selPart < (int)t.parts().size()
                             && t.parts()[m_selPart].isRef();

            // ── ISOLA E MODIFICA ─────────────────────────────────────────
            // Richiesta testuale: *"isolare solo una determinata composita e
            // modificarla, per esempio rimuovendo, aggiungendo o modificando box o
            // primitive, poi una volta finito si da l'ok e quella composita torna ad
            // essere un singolo oggetto"*.
            //
            // Differenza da "Esplodi", che è la domanda giusta da farsi: esplodere
            // SCIOGLIE il riferimento e sparpaglia le parti dentro questa struttura
            // (niente più confine, niente più nome); isolare tiene il confine e il
            // nome, e cambia solo cosa c'è dentro **questa copia**. Il primo è una
            // demolizione, il secondo una variante. Servono tutti e due, e il modo
            // di dirlo è tenerli accanto con due verbi diversi.
            if (refSel && t.isolated < 0)
            {
                ImGui::SameLine();
                if (ImGui::Button("Isola e modifica"))
                {
                    t.undo.push(t.snapshot(m_selPart), "isola", m_editorClock, -1.0f);
                    auto& rp = t.parts()[m_selPart];
                    // Prima volta: si parte dalle parti del tipo riferito. Partire
                    // dal vuoto avrebbe cancellato la struttura invece di aprirla.
                    if (rp.localParts.empty())
                        if (const auto* sub = m_prefabReg.getStructureType(rp.refType))
                            rp.localParts = sub->parts;
                    if (!rp.localParts.empty())
                    {
                        t.isolated = m_selPart;
                        m_selPart  = -1;
                        rebuildStructTabPreview(t);
                    }
                }
                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip("Entra DENTRO questa copia: si vede da sola e si\n"
                                      "modifica pezzo per pezzo. Alla fine torna a\n"
                                      "essere un oggetto solo, e il tipo originale\n"
                                      "resta com'e'.");
            }

            if (refSel && t.isolated < 0)
            {
                ImGui::SameLine();
                if (ImGui::Button("Esplodi"))
                {
                    t.undo.push(t.snapshot(m_selPart), "esplodi", m_editorClock, -1.0f);
                    const mini::StructurePart ref = t.parts()[m_selPart];
                    const auto* sub = m_prefabReg.getStructureType(ref.refType);
                    if (sub)
                    {
                        std::vector<mini::StructurePart> expanded;
                        for (const auto& sp : sub->parts)
                            expanded.push_back(mini::mapstructures::transformPart(
                                sp, ref.prim.x, ref.prim.y, ref.prim.z, ref.prim.ry));
                        t.parts().erase(t.parts().begin() + m_selPart);
                        t.parts().insert(t.parts().begin() + m_selPart,
                                           expanded.begin(), expanded.end());
                        m_selPart = expanded.empty() ? -1 : m_selPart;
                        t.dirty = true; rebuildStructTabPreview(t);
                    }
                }
                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip("Scioglie il riferimento nelle sue parti vere,\n"
                                      "modificabili una per una. Da qui in poi\n"
                                      "l'originale non le cambia piu'.");
            }

            for (int i = 0; i < (int)t.parts().size(); ++i)
            {
                const auto& p = t.parts()[i];
                char lbl[192];
                if (p.isRef())
                {
                    // Un riferimento va riconosciuto a colpo d'occhio: è l'unica parte
                    // che può cambiare da sola, senza che nessuno tocchi questo tipo.
                    // L'asterisco dice l'opposto — questa copia è stata modificata e
                    // NON segue più l'originale. Sono due comportamenti opposti sotto
                    // lo stesso nome: senza il segno si confondono.
                    const auto* sub = m_prefabReg.getStructureType(p.refType);
                    std::snprintf(lbl, sizeof(lbl), "[rif]%s %s%s##pt%d",
                                  p.isModifiedRef() ? "*" : "",
                                  p.label.empty() ? p.refType.c_str() : p.label.c_str(),
                                  (sub || p.isModifiedRef()) ? "" : "  — MANCANTE", i);
                }
                else
                    std::snprintf(lbl, sizeof(lbl), "%s %s##pt%d",
                                  p.isBox ? "[box]" : "[prim]",
                                  p.label.empty() ? "(senza nome)" : p.label.c_str(), i);
                if (ImGui::Selectable(lbl, m_selPart == i)) m_selPart = i;
            }
        }
        ImGui::Separator();

        // Parte selezionata: posa locale + misure proprie.
        if (assembly && m_selPart >= 0 && m_selPart < (int)t.parts().size())
        {
            // Fotografia PRIMA che un widget cominci a modificare, con l'etichetta
            // che fa coalescere l'intero trascinamento in una voce sola — la stessa
            // regola del Map Editor, ora nel componente condiviso.
            if (ImGui::IsAnyItemActive())
                t.undo.push(t.snapshot(m_selPart), "parte", m_editorClock);
            auto& p = t.parts()[m_selPart];
            bool ch = false;
            ImGui::TextDisabled("Parte selezionata (posizione LOCALE)");

            char nb[64];
            std::snprintf(nb, sizeof(nb), "%s", p.label.c_str());
            ImGui::SetNextItemWidth(-1.0f);
            if (ImGui::InputText("##ptname", nb, sizeof(nb))) { p.label = nb; t.dirty = true; }

            float* px = p.isBox ? &p.box.x : &p.prim.x;
            float* py = p.isBox ? &p.box.y : &p.prim.y;
            float* pz = p.isBox ? &p.box.z : &p.prim.z;
            float* pr = p.isBox ? &p.box.ry : &p.prim.ry;
            ch |= editor::ui::dragRow("X##pt", *px, 0.1f, -200.f, 200.f);
            // La Y NON significa la stessa cosa per tutti: un BOX ha la Y al centro,
            // una primitiva alla BASE, una piattaforma al ripiano calpestabile.
            // Non dirlo produceva "piccole differenze" inspiegabili accostando un box
            // a un muro — segnalato dall'utente, ed era questo.
            const char* yLabel =
                p.isBox ? "Y (centro)##pt"
                : (p.prim.kind == mini::StructureKind::Platform
                   || p.prim.kind == mini::StructureKind::Catwalk)
                  ? "Y (ripiano)##pt" : "Y (base)##pt";
            ch |= editor::ui::dragRow(p.isRef() ? "Y (origine)##pt" : yLabel,
                                      *py, 0.1f, -50.f, 200.f);
            ch |= editor::ui::dragRow("Z##pt", *pz, 0.1f, -200.f, 200.f);
            ch |= editor::ui::dragRow("Rotazione##pt", *pr, 1.0f, -180.f, 180.f);
            if (p.isRef())
                ImGui::TextDisabled("un riferimento porta l'ORIGINE dell'altra struttura");
            else if (p.isBox)
                ImGui::TextDisabled("un box ha la Y al CENTRO: a y=%.2f va da %.2f a %.2f",
                                    p.box.y, p.box.y - p.box.sy * 0.5f,
                                    p.box.y + p.box.sy * 0.5f);
            else
                ImGui::TextDisabled("una primitiva ha la Y alla BASE: appoggia a %.2f", *py);

            if (p.isRef())
            {
                // Un riferimento non ha misure proprie: sono quelle dell'altra
                // struttura. Mostrare qui i parametri della primitiva sarebbe la
                // bugia peggiore possibile — leve che si muovono senza effetto.
                const auto* sub = m_prefabReg.getStructureType(p.refType);
                ImGui::Separator();
                if (!sub)
                {
                    ImGui::TextColored({1.0f, 0.45f, 0.40f, 1.0f},
                        "Struttura \"%s\" NON TROVATA in libreria.", p.refType.c_str());
                    ImGui::TextDisabled("Non viene disegnata ne' espansa. Rinominala\n"
                                        "indietro, oppure togli questa parte.");
                }
                else
                {
                    ImGui::TextDisabled("Riferimento a un'altra struttura");
                    ImGui::Text("%s", sub->label.empty() ? p.refType.c_str()
                                                         : sub->label.c_str());
                    ImGui::TextDisabled("%s — %d parti%s", p.refType.c_str(),
                                        (int)sub->parts.size(),
                                        sub->verified ? "" : " — NON verificata");
                    ImGui::TextDisabled("Le misure sono le sue: si cambiano nel suo tab,\n"
                                        "e cambiano ovunque sia usata.");
                    // NON si apre il tab da qui: `openStructTab` fa push_back su
                    // `m_structTabs`, e questo codice gira dentro un ciclo che tiene
                    // un RIFERIMENTO all'elemento corrente di quel vettore. Aprirlo
                    // subito = riferimento penzolante. Si registra l'intenzione e la
                    // esegue chi comanda il ciclo.
                    if (ImGui::Button("Apri la struttura originale"))
                        m_pendingOpenType = p.refType;
                }
            }
            else if (p.isBox)
            {
                ImGui::TextDisabled("Dimensioni");
                ch |= editor::ui::dragRow("Larghezza##pt", p.box.sx, 0.1f, 0.05f, 200.f);
                ch |= editor::ui::dragRow("Altezza##pt",   p.box.sy, 0.1f, 0.05f, 100.f);
                ch |= editor::ui::dragRow("Profondita'##pt", p.box.sz, 0.1f, 0.05f, 200.f);
                // La semantica conta: il navmesh e la derivazione dei metadata la
                // leggono, e un pavimento dichiarato "muro" non genera superficie.
                const char* types[] = { "floor", "wall", "platform", "cover", "decoration" };
                int cur = (int)p.box.type;
                ImGui::SetNextItemWidth(-1.0f);
                if (ImGui::Combo("##pttype", &cur, types, 5))
                { p.box.type = (mini::BoxType)cur; ch = true; }
                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip("Semantica del box: la leggono il navmesh e la\n"
                                      "derivazione dei metadata. Un pavimento dichiarato\n"
                                      "\"muro\" non genera superficie calpestabile.");
            }
            else
            {
                ImGui::TextDisabled("Misure della primitiva");
                for (const auto& info : mini::mapstructures::paramsOf(p.prim.kind))
                {
                    float v = mini::mapstructures::getParam(p.prim, info.p);
                    const float phys = mini::mapstructures::physicalMin(p.prim.kind, info.p, p.prim);
                    const float pmax = mini::mapstructures::physicalMax(p.prim.kind, info.p);
                    ImGui::PushID(info.key);
                    // Il formato resta un NUMERO puro.
                    // Ieri ci scrivevo dentro "normativo: 2.80" per dire quanto vale
                    // uno 0 — ed era un difetto: ImGui usa lo STESSO formato per il
                    // campo di modifica del doppio clic, e "normativo: 2.80" non è un
                    // numero. Il valore letto tornava sbagliato o zero, ed è il
                    // sintomo riferito dall'utente. Il valore effettivo si dice sotto,
                    // in una riga a parte, dove non può interferire con la lettura.
                    const float eff = mini::mapstructures::effectiveParam(p.prim, info.p);
                    if (editor::ui::dragRow(info.label, v, 0.05f, 0.0f, 500.0f))
                    {
                        if (v > 0.001f)
                        {
                            if (phys > 0.0f && v < phys)  v = phys;
                            if (pmax > 0.0f && v > pmax)  v = pmax;
                        }
                        if (v < 0.0f) v = 0.0f;
                        mini::mapstructures::setParam(p.prim, info.p, v);
                        ch = true;
                    }
                    // Il PAVIMENTO FISICO va detto anche qui. Nel tipo semplice
                    // c'era; nelle parti no, e passando agli assemblaggi "sono
                    // scomparse le indicazioni sulle metriche" — giustamente:
                    // senza, un valore che non scende sembra un comando rotto.
                    ImGui::Indent(12.0f);
                    // Quanto vale davvero uno 0: detto QUI, non dentro il campo.
                    if (v < 0.001f && eff > 0.001f)
                        ImGui::TextDisabled("0 = normativo, cioe' %.2f m", eff);
                    if (phys > 0.0f || pmax > 0.0f)
                    {
                        if (pmax > 0.0f) ImGui::TextDisabled("fisico: %.2f - %.2f m", phys, pmax);
                        else             ImGui::TextDisabled("fisico: min %.2f m", phys);
                    }
                    ImGui::Unindent(12.0f);
                    if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", info.help);
                    ImGui::PopID();
                }
                if (p.prim.kind == mini::StructureKind::Room)
                    if (ImGui::Checkbox("Soffitto##pt", &p.prim.ceiling)) ch = true;
                if (p.prim.kind == mini::StructureKind::Catwalk
                 || p.prim.kind == mini::StructureKind::Platform)
                    if (ImGui::Checkbox("Parapetto##pt", &p.prim.railing)) ch = true;
                if (p.prim.kind == mini::StructureKind::Platform
                 || p.prim.kind == mini::StructureKind::Room)
                {
                    ImGui::TextDisabled("Accessi / aperture");
                    const char* side[4] = { "-Z", "+Z", "-X", "+X" };
                    for (int i = 0; i < 4; ++i)
                    {
                        ImGui::PushID(100 + i);
                        if (ImGui::Checkbox(side[i], &p.prim.access[i])) ch = true;
                        ImGui::PopID();
                        if (i < 3) ImGui::SameLine();
                    }
                }
            }
            if (ch) { t.dirty = true; rebuildStructTabPreview(t); }
            ImGui::Separator();
        }

    }

    // Con le parti, i parametri della primitiva singola non si applicano più: le
    // misure si autorano parte per parte. Mostrarli comunque darebbe comandi inerti.
    const bool isAssembly = !t.parts().empty();
    if (isAssembly)
        ImGui::TextDisabled("Il tipo e' un ASSEMBLAGGIO: le misure si autorano\n"
                            "parte per parte, qui sopra.");

    bool changed = false;
    if (!isAssembly) {
    ImGui::TextDisabled("Misure e vincoli");
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("La spunta decide se la misura si potra' toccare quando la\n"
                          "struttura e' in mappa. Min/max la restringono ulteriormente.\n"
                          "Il PAVIMENTO FISICO non si puo' allentare: sotto quello la\n"
                          "struttura non genera navmesh (ADR-055).");
    ImGui::Separator();

    for (const auto& info : mini::mapstructures::paramsOf(t.def.kind))
    {
        auto& rule = t.def.rules[(std::size_t)info.p];
        const float phys = mini::mapstructures::physicalMin(t.def.kind, info.p,
                                                            t.def.defaults);
        const float physMax = mini::mapstructures::physicalMax(t.def.kind, info.p);

        ImGui::PushID(info.key);
        if (ImGui::Checkbox("##edit", &rule.editable)) { t.dirty = true; }
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Modificabile quando la struttura e' in mappa.");
        ImGui::SameLine();

        float v = mini::mapstructures::getParam(t.def.defaults, info.p);
        ImGui::SetNextItemWidth(110.0f);
        // Formato NUMERICO puro: vedi la nota nelle parti — un formato non
        // parsabile rompe il campo di modifica del doppio clic.
        {
            const float eff = mini::mapstructures::effectiveParam(t.def.defaults, info.p);
            if (ImGui::DragFloat(info.label, &v, 0.05f, 0.0f, 0.0f, "%.2f"))
        {
            // Lo 0 resta lecito dove significa "normativo": è l'assenza di scelta.
            const bool zeroMeansDefault =
                (info.p == mini::StructureParam::Riser || info.p == mini::StructureParam::Tread ||
                 info.p == mini::StructureParam::Height || info.p == mini::StructureParam::Thickness ||
                 info.p == mini::StructureParam::OpenW || info.p == mini::StructureParam::OpenH ||
                 info.p == mini::StructureParam::FlightRise || info.p == mini::StructureParam::Spacing);
            if (!(zeroMeansDefault && v < 0.001f))
            {
                if (phys > 0.0f && v < phys)       v = phys;
                if (physMax > 0.0f && v > physMax) v = physMax;
            }
            if (v < 0.0f) v = 0.0f;
            mini::mapstructures::setParam(t.def.defaults, info.p, v);
            t.dirty = true; changed = true;
        }
        // Quanto vale davvero uno 0: su una riga a parte, non dentro il campo.
        if (v < 0.001f && eff > 0.001f)
        {
            ImGui::Indent(24.0f);
            ImGui::TextDisabled("0 = normativo, cioe' %.2f m", eff);
            ImGui::Unindent(24.0f);
        }
        }   // fine del blocco del valore effettivo
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", info.help);

        // Il pavimento fisico, sempre visibile: è la ragione per cui un valore non
        // scende, e senza dirla l'editor sembrerebbe rotto.
        if (phys > 0.0f || physMax > 0.0f)
        {
            ImGui::Indent(24.0f);
            if (physMax > 0.0f)
                ImGui::TextDisabled("fisico: %.2f - %.2f m", phys, physMax);
            else
                ImGui::TextDisabled("fisico: min %.2f m", phys);
            ImGui::Unindent(24.0f);
        }

        if (rule.editable)
        {
            ImGui::Indent(24.0f);
            ImGui::SetNextItemWidth(70.0f);
            if (ImGui::DragFloat("min", &rule.min, 0.05f, 0.0f, 0.0f, "%.2f"))
            { if (rule.min < 0.0f) rule.min = 0.0f; t.dirty = true; }
            ImGui::SameLine();
            ImGui::SetNextItemWidth(70.0f);
            if (ImGui::DragFloat("max", &rule.max, 0.05f, 0.0f, 0.0f, "%.2f"))
            { if (rule.max < 0.0f) rule.max = 0.0f; t.dirty = true; }
            // Un minimo autorato SOTTO il pavimento non si applica: dirlo, invece di
            // accettarlo in silenzio e produrre una struttura che il navmesh scarta.
            if (rule.min > 0.0f && rule.min < phys)
                ImGui::TextColored(ImVec4(0.95f, 0.75f, 0.30f, 1.0f),
                                   "min sotto il fisico: vale %.2f", phys);
            if (rule.max > 0.0f && rule.min > 0.0f && rule.max < rule.min)
                ImGui::TextColored(ImVec4(0.95f, 0.35f, 0.30f, 1.0f), "max < min");
            ImGui::Unindent(24.0f);
        }
        ImGui::PopID();
        ImGui::Separator();
    }

    // Interruttori non dimensionali: non hanno min/max, solo un sì/no.
    if (t.def.kind == mini::StructureKind::Room)
        if (ImGui::Checkbox("Soffitto", &t.def.defaults.ceiling)) { t.dirty = true; changed = true; }
    if (t.def.kind == mini::StructureKind::Catwalk || t.def.kind == mini::StructureKind::Platform)
        if (ImGui::Checkbox("Parapetto", &t.def.defaults.railing)) { t.dirty = true; changed = true; }
    if (t.def.kind == mini::StructureKind::Platform || t.def.kind == mini::StructureKind::Room)
    {
        ImGui::TextDisabled("Accessi / aperture per lato");
        const char* side[4] = { "-Z", "+Z", "-X", "+X" };
        for (int i = 0; i < 4; ++i)
        {
            ImGui::PushID(i);
            if (ImGui::Checkbox(side[i], &t.def.defaults.access[i])) { t.dirty = true; changed = true; }
            ImGui::PopID();
            if (i < 3) ImGui::SameLine();
        }
    }
    }   // fine del ramo "tipo a primitiva singola"
    ImGui::EndChild();

    if (changed) rebuildStructTabPreview(t);

    ImGui::SameLine();
    // Maniglia fra parametri e viewport: si trascina per allargare i parametri.
    editor::ui::panelSplitter("##stparamsplit", s_paramW, remaining, 200.0f,
                              (totalW > 500.0f) ? totalW * 0.45f : 240.0f);
    ImGui::SameLine();

    // L'overlay appartiene a QUESTO tab: si riporta a ogni disegno, così passando
    // da una struttura all'altra non si eredita il navmesh di quella prima.
    applyStructNavOverlay(t);

    // ── Viewport isolata ──────────────────────────────────────────────────
    const float vpW = totalW - paramW - s_checkW
                    - ImGui::GetStyle().ItemSpacing.x * 4 - 12.0f;
    ImGui::BeginChild("##struct_vp", ImVec2(vpW < 160.0f ? 160.0f : vpW, 0),
                      ImGuiChildFlags_None,
                      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    m_structVp.draw(false);
    ImGui::EndChild();

    ImGui::SameLine();
    // Maniglia a SINISTRA del pannello di destra: `ChildFlags_ResizeX` metterebbe il
    // grip sul bordo finestra, e una volta stretto non si riallargherebbe più.
    editor::ui::panelSplitter("##stchecksplit", s_checkW, remaining, 200.0f,
                              (totalW > 500.0f) ? totalW * 0.45f : 240.0f);
    ImGui::SameLine();

    // ── Verifica ──────────────────────────────────────────────────────────
    ImGui::BeginChild("##struct_check", ImVec2(s_checkW, 0), ImGuiChildFlags_Borders);
    // Il risultato invecchia da solo: la ricetta è cambiata → la verifica non vale.
    if (t.check.run && structFingerprint(t.def) != t.fingerprint) t.check.stale = true;

    ImGui::TextDisabled("Verifica");
    ImGui::Separator();
    if (!t.check.run)
    {
        ImGui::TextWrapped("Non ancora verificata. Il navmesh e' l'unico controllo "
                           "che non puo' mentire: fra i box e le superfici "
                           "percorribili ci sono erosione, sfoltimento dei cigli, "
                           "altezza libera e area minima di regione.");
    }
    else
    {
        if (t.check.stale)
            ImGui::TextColored(ImVec4(0.95f, 0.75f, 0.30f, 1.0f),
                               "La ricetta e' cambiata: rifare la verifica.");

        // Il SINTOMO per primo, non l'esito lontano — ma il sintomo GIUSTO: per un
        // ostacolo puro (muro, porta, barricata) "superficie persa" non vuol dire
        // nulla, e mostrare 0% suonerebbe come una promessa non verificata.
        if (t.check.hasWalkable)
        {
            const float lost = 100.0f * (1.0f - t.check.navArea / t.check.declaredArea);
            const ImVec4 col = (lost < 0.5f) ? ImVec4(0.35f, 0.90f, 0.45f, 1.0f)
                                             : ImVec4(0.95f, 0.35f, 0.30f, 1.0f);
            ImGui::TextColored(col, "superficie persa: %.0f%%", lost);
            ImGui::TextDisabled("%.1f di %.1f m2 raggiungibili",
                                t.check.navArea, t.check.declaredArea);
        }
        else
        {
            ImGui::TextDisabled("Ostacolo puro: nessuna superficie calpestabile.");
            if (t.check.tris > 0)
                ImGui::TextColored(ImVec4(0.35f, 0.90f, 0.45f, 1.0f),
                                   "il terreno resta percorribile attorno");
            else
                ImGui::TextColored(ImVec4(0.95f, 0.35f, 0.30f, 1.0f),
                                   "ostruisce TUTTO: nessun navmesh attorno");
        }
        ImGui::Separator();

        // Il FUNNEL, coi suoi denominatori.
        ImGui::Text("box espansi:   %d", t.check.boxes);
        ImGui::Text("triangoli:     %d", t.check.tris);
        ImGui::Text("componenti:    %d", t.check.components);
        // Per un ostacolo, dividere il piano è il MESTIERE: segnalarlo come difetto
        // sarebbe un falso allarme, e i falsi allarmi insegnano a ignorare il pannello.
        if (t.check.components > 1)
        {
            if (t.check.hasWalkable)
                ImGui::TextColored(ImVec4(0.95f, 0.35f, 0.30f, 1.0f),
                                   "la struttura si SPEZZA");
            else
                ImGui::TextDisabled("divide il terreno: atteso per un ostacolo");
        }
        ImGui::TextDisabled("costruito in %.2f s", t.check.seconds);
        ImGui::Separator();

        // La SINGOLA entità: quale box resta muto.
        if (!t.check.hasWalkable)
        {
            ImGui::TextDisabled("Niente da raggiungere: si verifica solo che\n"
                                "non ostruisca il passaggio.");
        }
        else if (t.check.mute.empty())
        {
            ImGui::TextColored(ImVec4(0.35f, 0.90f, 0.45f, 1.0f),
                               "Ogni superficie dichiarata e' raggiungibile.");
        }
        else
        {
            ImGui::TextColored(ImVec4(0.95f, 0.35f, 0.30f, 1.0f),
                               "%d superfici irraggiungibili:", (int)t.check.mute.size());
            for (const auto& m : t.check.mute) ImGui::BulletText("%s", m.c_str());
            ImGui::Separator();
            ImGui::TextWrapped("Cause possibili, in ordine di frequenza: larghezza "
                               "sotto il minimo (erosione + area minima di regione), "
                               "gradini che si sfiorano invece di sovrapporsi, "
                               "altezza libera insufficiente sopra la superficie.");
        }
    }
    ImGui::EndChild();

    ImGui::EndChild();
}

// ── Ricarica degli asset prefab ───────────────────────────────────────────────
// Un punto solo. Funziona perché dal 2026-08-05 `loadPrefabs` AZZERA il proprio
// contenitore: prima sommava al vecchio stato, quindi un prefab cancellato dal
// disco restava nel menu e si poteva ancora piazzare fino al riavvio dell'editor.
// Il comando di eliminazione funzionava: non era autoritativo il ricaricamento.
void MapEditor::reloadPrefabAssets()
{
    m_prefabReg.loadPrefabs(getDataDir());
    m_prefabIds.clear();
    for (const auto& [pid, def] : m_prefabReg.prefabs()) m_prefabIds.push_back(pid);
    std::sort(m_prefabIds.begin(), m_prefabIds.end());
    if (m_prefabIds.empty()) m_prefabIds.push_back("");   // il combo vuole una voce
    if (m_prefabPick >= (int)m_prefabIds.size()) m_prefabPick = 0;
    updateViewport();   // le istanze di un prefab sparito diventano subito "rotte"
}

// ── DA OGGETTO UNICO A INSIEME DI PARTI, E RITORNO (richiesta utente) ────────
// Una composita in mappa è UN elemento: si sposta, si ruota e si aggiorna insieme
// alla sua definizione. È giusto finché va bene com'è. Quando serve cambiarne un
// pezzo *in quel punto e solo lì* — la barricata storta perché c'è una roccia —
// l'alternativa era duplicare l'intero tipo in libreria per una modifica di mezzo
// metro, e ritrovarsi la libreria piena di varianti quasi identiche.
//
// Esplodere scioglie l'istanza nelle sue parti VERE, ognuna un elemento della mappa:
// da lì in poi sono geometria di questa mappa, e l'originale non le tocca più.
// Perde il legame col tipo, ed è esattamente ciò che si vuole in quel momento.
void MapEditor::explodeStructure(int idx)
{
    if (idx < 0 || idx >= (int)m_structures.size()) return;
    const mini::StructureDef inst = m_structures[idx];
    const auto* ty = inst.type.empty() ? nullptr
                                       : m_prefabReg.getStructureType(inst.type);
    // Le PARTI LOCALI vincono, come nell'espansione: esplodere un'istanza modificata
    // deve dare le parti che si VEDONO, non quelle del tipo che non la governa più.
    // Leggere il tipo qui avrebbe silenziosamente annullato la modifica.
    // Puntatore e non ternario: un ternario con un vettore temporaneo nel ramo else
    // compila e funziona (la copia viene estesa in vita), ma è a un passo dal
    // riferimento penzolante se qualcuno domani lo "semplifica".
    const std::vector<mini::StructurePart>* srcp = nullptr;
    if (!inst.localParts.empty()) srcp = &inst.localParts;
    else if (ty)                  srcp = &ty->parts;
    if (!srcp || srcp->empty()) return;
    const std::vector<mini::StructurePart>& src = *srcp;
    const std::string originLabel = inst.type.empty() ? inst.label : inst.type;

    pushUndo("esplodi struttura");
    m_structures.erase(m_structures.begin() + idx);
    m_selStruct = -1; m_selBox = -1; m_multiSel.clear();

    for (const auto& p : src)
    {
        const mini::StructurePart w =
            mini::mapstructures::transformPart(p, inst.x, inst.y, inst.z, inst.ry);
        if (w.isBox)
        {
            BoxEntry b;
            b.x = w.box.x; b.y = w.box.y; b.z = w.box.z; b.ry = w.box.ry;
            b.sx = w.box.sx; b.sy = w.box.sy; b.sz = w.box.sz;
            b.r = w.box.r; b.g = w.box.g; b.b = w.box.b;
            b.isCollider = w.box.collider;
            std::snprintf(b.type, sizeof(b.type), "%s", mini::boxTypeName(w.box.type));
            std::snprintf(b.label, sizeof(b.label), "%s",
                          w.label.empty() ? originLabel.c_str() : w.label.c_str());
            m_boxes.push_back(b);
        }
        else
        {
            // Una parte primitiva torna a essere una struttura parametrica della
            // mappa: conserva la sua ricetta e le sue garanzie (alzate a norma,
            // larghezze minime). Appiattirla a box qui butterebbe via ADR-053 —
            // esplodere serve a poter modificare, non a perdere i vincoli.
            mini::StructureDef s = w.prim;
            s.label = w.label.empty() ? originLabel : w.label;
            // Una parte-RIFERIMENTO resta una composita, un livello più in basso:
            // esplodere è un passo, non una demolizione fino ai box.
            s.type  = w.refType;
            if (!w.refType.empty())
            {
                if (const auto* sub = m_prefabReg.getStructureType(w.refType))
                    s.kind = sub->kind;
                // Una parte-riferimento già ISOLATA E MODIFICATA porta con sé le sue
                // parti: diventa un'istanza modificata in mappa, non un riferimento
                // puro che tornerebbe all'originale perdendo il lavoro.
                s.localParts = w.localParts;
            }
            m_structures.push_back(std::move(s));
        }
    }
    m_dirty = true;
    rebuildStructurePreview();
    updateViewport();
}

// Il gesto inverso: prendere quello che si è costruito a mano in mappa e farne una
// composita riusabile. Senza, esplodere sarebbe una porta a senso unico — e la
// modifica ad hoc di oggi resterebbe per sempre geometria sciolta.
// Il tipo si scrive con lo STESSO salvataggio dei tab (`saveStructType`): un secondo
// scrittore qui e i due divergerebbero al primo campo nuovo.
void MapEditor::groupSelectionIntoType(const std::string& label)
{
    const std::vector<int> sel = m_multiSel;
    if (sel.size() < 2) return;

    // Baricentro in pianta: l'origine della composita cade al centro di ciò che si
    // è raggruppato, non sul primo elemento capitato. È il perno di rotazione, e
    // averla fuori dalla struttura significa ruotare attorno al vuoto.
    float sx = 0.0f, sz = 0.0f; int n = 0;
    for (int c : sel)
    {
        if (c >= 0 && c < (int)m_boxes.size()) { sx += m_boxes[c].x; sz += m_boxes[c].z; ++n; }
        else if (c <= -6000)
        {
            const int si = -6000 - c;
            if (si < (int)m_structures.size())
            { sx += m_structures[si].x; sz += m_structures[si].z; ++n; }
        }
    }
    if (n == 0) return;
    const float cx = sx / (float)n, cz = sz / (float)n;

    StructTab tab;
    tab.def.label = label;
    tab.def.kind  = mini::StructureKind::Platform;   // irrilevante per un assemblaggio
    for (int c : sel)
    {
        mini::StructurePart p;
        if (c >= 0 && c < (int)m_boxes.size())
        {
            const auto& b = m_boxes[c];
            p.isBox = true;
            p.box.x = b.x - cx; p.box.y = b.y; p.box.z = b.z - cz; p.box.ry = b.ry;
            p.box.sx = b.sx; p.box.sy = b.sy; p.box.sz = b.sz;
            p.box.r = b.r; p.box.g = b.g; p.box.b = b.b;
            p.box.collider = b.isCollider;
            p.box.type = mini::parseBoxType(b.type);
            p.label = b.label;
        }
        else if (c <= -6000)
        {
            const int si = -6000 - c;
            if (si >= (int)m_structures.size()) continue;
            const auto& s = m_structures[si];
            p.isBox = false;
            p.prim = s;
            p.prim.x -= cx; p.prim.z -= cz;
            // Una composita dentro il gruppo resta un RIFERIMENTO: raggruppare non
            // deve appiattire il lavoro già fatto in libreria.
            p.refType = s.type;
            p.label = s.label;
        }
        else continue;   // post, posizioni tattiche... non sono geometria
        tab.def.parts.push_back(std::move(p));
    }
    if (tab.def.parts.empty()) return;

    // Un nome già preso NON si sovrascrive: `saveStructType` fa read-modify-write,
    // quindi scriverebbe le parti nuove dentro il tipo di qualcun altro — e la
    // struttura originale sparirebbe senza che nessuno l'abbia chiesto.
    {
        std::error_code ec;
        const std::string id = idFromLabel(label);
        if (fs::exists(getDataDir() + "/structures/" + id + ".json", ec))
        { m_groupError = "Esiste gia' un tipo con questo nome."; return; }
    }
    m_groupError.clear();
    saveStructType(tab);
    if (!tab.saveError.empty()) { m_groupError = tab.saveError; return; }

    // Da qui in poi la mappa cambia: prima si toglie ciò che si è raggruppato (dagli
    // indici PIÙ ALTI, o gli indici rimanenti scalerebbero sotto i piedi — è il
    // difetto di identità posizionale di KI #100), poi si mette l'istanza.
    pushUndo("raggruppa in composita");
    std::vector<int> boxIdx, structIdx;
    for (int c : sel)
    {
        if (c >= 0) boxIdx.push_back(c);
        else if (c <= -6000) structIdx.push_back(-6000 - c);
    }
    std::sort(boxIdx.begin(), boxIdx.end(), std::greater<int>());
    std::sort(structIdx.begin(), structIdx.end(), std::greater<int>());
    for (int i : boxIdx)    if (i < (int)m_boxes.size())      m_boxes.erase(m_boxes.begin() + i);
    for (int i : structIdx) if (i < (int)m_structures.size()) m_structures.erase(m_structures.begin() + i);

    m_prefabReg.loadStructureTypes(getDataDir());   // il tipo appena scritto
    refreshStructTypeIds();
    mini::StructureDef inst;
    inst.type  = tab.id;
    inst.label = label;
    inst.kind  = tab.def.kind;
    inst.x = cx; inst.y = 0.0f; inst.z = cz; inst.ry = 0.0f;
    m_structures.push_back(std::move(inst));

    m_multiSel.clear();
    m_selBox = -1;
    m_selStruct = (int)m_structures.size() - 1;
    m_dirty = true;
    rebuildStructurePreview();
    updateViewport();
}

// ── UNDO / REDO a snapshot (doc 47 E1) ────────────────────────────────────────
MapEditor::Snapshot MapEditor::captureState() const
{
    Snapshot s;
    s.boxes = m_boxes;           s.posts       = m_posts;
    s.positions = m_positions;   s.sectors     = m_sectors;
    s.dangers = m_dangers;       s.routes      = m_routes;
    s.vehSpawns = m_vehSpawns;   s.targets     = m_targets;
    s.prefabInsts = m_prefabInsts;
    s.structures = m_structures; s.commander   = m_commander;
    s.spawnTeam1 = m_spawnTeam1; s.spawnTeam2  = m_spawnTeam2;
    s.spawnPoints1 = m_spawnPoints1; s.spawnPoints2 = m_spawnPoints2;
    return s;
}

void MapEditor::applyState(const Snapshot& s)
{
    m_boxes = s.boxes;           m_posts       = s.posts;
    m_positions = s.positions;   m_sectors     = s.sectors;
    m_dangers = s.dangers;       m_routes      = s.routes;
    m_vehSpawns = s.vehSpawns;   m_targets     = s.targets;
    m_prefabInsts = s.prefabInsts;
    m_structures = s.structures; m_commander   = s.commander;
    m_spawnTeam1 = s.spawnTeam1; m_spawnTeam2  = s.spawnTeam2;
    m_spawnPoints1 = s.spawnPoints1; m_spawnPoints2 = s.spawnPoints2;
    // La selezione può puntare a un elemento che non esiste più: azzerarla è più
    // sicuro che provare a rimapparla (un indice sbagliato fa modificare il box
    // sbagliato, che è peggio di perdere la selezione).
    m_selBox    = -1;
    m_selStruct = -1;
    m_dirty     = true;
    rebuildStructurePreview();
    updateViewport();
}

// Le tre operazioni sono ora tre righe: la logica (coalescenza per gesto, taglio
// del ramo di ripristino, profondità massima) vive in `UndoStack`, estratto proprio
// da qui. Nulla cambia per chi usa l'editor — cambia chi lo implementa, e adesso
// sono in tre moduli a beneficiarne invece di uno.
void MapEditor::pushUndo(const char* tag)
{
    m_hist.push(captureState(), tag, m_editorClock);
}

void MapEditor::doUndo()
{
    Snapshot s = captureState();
    if (m_hist.undo(s)) applyState(s);
}

void MapEditor::doRedo()
{
    Snapshot s = captureState();
    if (m_hist.redo(s)) applyState(s);
}

// ── Primitive parametriche (ADR-053) ──────────────────────────────────────────
// L'anteprima usa `mapstructures::expand`, LA STESSA funzione che il motore chiama
// al load. Non è pigrizia: due espansioni separate divergerebbero al primo campo
// aggiunto, e l'editor mostrerebbe una scala diversa da quella che si gioca.
// UN SOLO punto in cui l'editor espande una struttura della mappa.
// `expandInstance` e non `expand`: la seconda ignora il TIPO e mostra solo la
// primitiva di base — un assemblaggio appariva come la sola scala da cui era
// partito (segnalato dall'utente). C'erano QUATTRO chiamate sparse e tre di loro
// sbagliavano: anteprima, disegno nel viewport e conteggio in lista.
// Una funzione sola perché la quinta chiamata, quando arriverà, non possa
// dimenticarsene — è la stessa ragione per cui `expandInstance` esiste nel motore.
void MapEditor::expandStructureAt(int idx, std::vector<mini::MapGeometryBox>& out) const
{
    if (idx < 0 || idx >= (int)m_structures.size()) return;
    const auto& s = m_structures[idx];
    const mini::StructureTypeDef* ty = s.type.empty() ? nullptr
                                     : m_prefabReg.getStructureType(s.type);
    mini::mapstructures::expandInstance(s, ty, out, m_typeResolver);
}

void MapEditor::rebuildStructurePreview()
{
    m_structPreview.clear();
    for (int i = 0; i < (int)m_structures.size(); ++i)
        expandStructureAt(i, m_structPreview);
}

void MapEditor::addStructure(mini::StructureKind kind)
{
    pushUndo("addStructure");
    mini::StructureDef d;
    d.kind = kind;
    // Nasce davanti alla camera come gli altri elementi, e con i valori NORMATIVI
    // (doc 47 §4.3): il default è già a norma, non un punto di partenza da correggere.
    const glm::vec3 fp = m_viewport.groundFocusPoint();
    d.x = snap(fp.x);  d.z = snap(fp.z);  d.y = 0.0f;
    switch (kind)
    {
        case mini::StructureKind::Stair:
            d.label = "Scala";  d.rise = 2.0f;
            d.width = mini::mapmetrics::STAIR_MIN_WIDTH;  break;
        case mini::StructureKind::Ramp:
            d.label = "Rampa";  d.rise = 2.0f;
            d.width = mini::mapmetrics::CORRIDOR_MIN;     break;
        case mini::StructureKind::Wall:
            d.label = "Muro";   d.length = 6.0f;
            d.height = mini::mapmetrics::WALL_HEIGHT;
            d.thickness = 0.0f;                          break;
        case mini::StructureKind::Platform:
            d.label = "Piattaforma";  d.sizeX = 8.0f; d.sizeZ = 8.0f;
            d.y = 3.0f; d.baseY = 0.0f;
            d.access[0] = true;                          break;
        case mini::StructureKind::Switchback:
            d.label = "Scala doppia";  d.rise = 6.0f;
            d.width = mini::mapmetrics::STAIR_MIN_WIDTH;
            d.flightRise = 0.0f;                         break;
        case mini::StructureKind::Doorway:
            d.label = "Porta";  d.length = 6.0f;
            d.height = mini::mapmetrics::WALL_HEIGHT;
            d.openW = mini::mapmetrics::DOOR_WIDTH;
            d.openH = mini::mapmetrics::DOOR_HEIGHT;     break;
        case mini::StructureKind::Room:
            d.label = "Stanza";  d.sizeX = 10.0f; d.sizeZ = 8.0f;
            d.height = mini::mapmetrics::WALL_HEIGHT;
            d.openW = mini::mapmetrics::DOOR_WIDTH;
            d.openH = mini::mapmetrics::DOOR_HEIGHT;
            d.access[0] = true;                          break;
        case mini::StructureKind::Catwalk:
            d.label = "Passerella";  d.length = 12.0f;
            d.width = mini::mapmetrics::CORRIDOR_MIN;
            d.y = 4.0f;  d.railing = true;               break;
        case mini::StructureKind::Barricade:
            d.label = "Barricata";  d.length = 12.0f;
            d.width = 2.0f;  d.spacing = 1.5f;
            d.height = mini::mapmetrics::COVER_LOW;      break;
    }
    m_structures.push_back(d);
    // Passa da `setSelection`, che azzera anche l'INSIEME multiplo. Scrivere
    // `m_selStruct` a mano lasciava vivo `m_multiSel` con la selezione precedente:
    // il gizmo si disegnava sulla struttura nuova ma agiva su quella vecchia
    // (segnalato dall'utente). Il primario e l'insieme vanno cambiati insieme,
    // sempre, ed è il motivo per cui `setSelection` esiste.
    setSelection(-6000 - ((int)m_structures.size() - 1), /*additive=*/false);
    m_dirty = true;
    rebuildStructurePreview();
    updateViewport();
}

// ── updateViewport ────────────────────────────────────────────────────────────
void MapEditor::updateViewport(bool recomputeDerived)
{
    if (recomputeDerived) recomputeExposure();   // ADR-033: tenuta in pari con le posizioni
    std::vector<FreeCameraViewport::MapBoxDraw> draws;
    draws.reserve(m_boxes.size() + 2);

    // Selezione in corso per la creazione di un prefab: si raccoglie PRIMA del ciclo
    // dei box, perché ora gli elementi inclusi si mostrano COLORANDO il box stesso
    // invece di appoggiarci sopra un rombo. Il rombo funzionava ma è un oggetto in
    // più da leggere: colorare la cosa selezionata è come si comportano tutti gli
    // editor 3D, e non aggiunge geometria alla scena.
    std::vector<int> zoneBoxes, zonePositions;
    if (m_prefabZoneMode) prefabZoneCollect(zoneBoxes, zonePositions);
    auto inZone = [&](int idx) {
        return std::find(zoneBoxes.begin(), zoneBoxes.end(), idx) != zoneBoxes.end();
    };

    // ── Difetti per box (G7) ──────────────────────────────────────────────
    // −1 = nessun difetto, 0 = avviso, 1 = problema. Costruito da `m_issues`, che
    // viene da `analyzeTacticalHealth`: la stessa funzione del gate `--validate`.
    // Una seconda analisi "solo per l'editor" prima o poi darebbe un verdetto
    // diverso da quello del gioco, ed è il difetto che ci è costato di più
    // (changelog 77: due verità sullo stesso mondo).
    std::vector<int> boxDefect(m_boxes.size(), -1);
    if (m_showDefects)
        for (const auto& is : m_issues)
            if (is.sel >= 0 && is.sel < (int)boxDefect.size())
                boxDefect[is.sel] = std::max(boxDefect[is.sel], is.sev);

    // Seziona un box alla quota di taglio. `false` = sta tutto sopra, non si disegna.
    auto cutBox = [&](FreeCameraViewport::MapBoxDraw& d) -> bool {
        if (m_hideAboveY > 999.0f) return true;              // nessun taglio
        const float base = d.y - d.sy * 0.5f;
        const float top  = d.y + d.sy * 0.5f;
        if (base >= m_hideAboveY) return false;              // interamente sopra
        if (top  <= m_hideAboveY) return true;               // interamente sotto
        d.sy = m_hideAboveY - base;                          // a cavallo: si tronca
        d.y  = base + d.sy * 0.5f;
        return true;
    };

    // Tipi "floor" visualizzati diversamente se showNavmesh
    for (int i = 0; i < (int)m_boxes.size(); ++i)
    {
        const auto& b = m_boxes[i];
        // FILTRI DI VISIBILITÀ (E5): solo visivi, non toccano i dati.
        {
            const int ti = (std::strcmp(b.type,"floor")==0)    ? 0
                         : (std::strcmp(b.type,"wall")==0)     ? 1
                         : (std::strcmp(b.type,"platform")==0) ? 2
                         : (std::strcmp(b.type,"cover")==0)    ? 3 : 4;
            if (!m_showType[ti]) continue;
        }
        FreeCameraViewport::MapBoxDraw d;
        d.x = b.x; d.y = b.y; d.z = b.z; d.ry = b.ry;
        d.sx = b.sx; d.sy = b.sy; d.sz = b.sz;
        // ── SEZIONE alla quota di taglio ──────────────────────────────────
        // Non "nascondi ciò che ha la base sopra": quella regola lasciava in piedi
        // ogni muro che parte da terra, quindi muovendo lo slider non si vedeva
        // tagliare nulla. Qui il box viene **sezionato**: sopra la quota sparisce,
        // a cavallo viene disegnato solo fino al piano. È una vista in sezione vera,
        // e costa solo due sottrazioni — i DATI non si toccano.
        if (!cutBox(d)) continue;
        // Selezione multipla (G3): tutti gli elementi dell'insieme si evidenziano,
        // non solo il primario — altrimenti non si vede cosa si sta per spostare.
        d.selected = (i == m_selBox)
            || std::find(m_multiSel.begin(), m_multiSel.end(), i) != m_multiSel.end();
        d.pickId = i;

        // L'overlay navmesh sostituisce la vecchia tinta verde "Area navigabile",
        // che coloriva i box di tipo `floor` — cioè un'INTENZIONE dell'autore, non
        // ciò che l'AI può davvero calpestare. Con il navmesh vero disegnato sopra,
        // tingere anche i box confonderebbe le due cose.
        if (m_prefabZoneMode && inZone(i)) {
            // Incluso nel prefab: ciano pieno. Si legge a colpo d'occhio COSA entra,
            // senza rombi sospesi sopra gli oggetti.
            d.r = 0.30f; d.g = 0.90f; d.b = 1.00f;
        } else if (!m_prefabZoneMode && boxDefect[i] >= 0) {
            // DIFETTO (G7): rosso = problema (il navmesh non ci sale, nessuno ci
            // arriva), ambra = avviso. Si vede mentre costruisci, non dopo aver
            // salvato: è la differenza fra correggere un box e rifare una zona.
            if (boxDefect[i] >= 1) { d.r = 0.95f; d.g = 0.22f; d.b = 0.18f; }
            else                   { d.r = 0.95f; d.g = 0.72f; d.b = 0.20f; }
        } else if (m_prefabZoneMode) {
            // Escluso: smorzato, così il contrasto con gli inclusi è netto.
            d.r = b.r * 0.45f; d.g = b.g * 0.45f; d.b = b.b * 0.45f;
        } else {
            d.r = b.r; d.g = b.g; d.b = b.b;
        }

        draws.push_back(d);
    }

    // ── Box DERIVATI dalle primitive (ADR-053) ────────────────────────────
    // Non sono in `m_boxes` e non si salvano: si vedono, non si toccano. Il pickId
    // negativo li mappa sulla RICETTA che li ha generati, così cliccare un gradino
    // seleziona la scala — che è l'unica cosa modificabile.
    // Si rigenera per struttura, così si sa QUALE ricetta ha prodotto ogni box e la
    // selezionata si può evidenziare tutta insieme.
    for (int si = 0; si < (int)m_structures.size() && m_showStructures; ++si)
    {
        std::vector<mini::MapGeometryBox> own;
        // Anche qui il TIPO va risolto: è il disegno vero e proprio nel viewport, e
        // senza mostrava solo la primitiva di base dell'assemblaggio.
        expandStructureAt(si, own);
        const bool selected = (si == m_selStruct)
            || std::find(m_multiSel.begin(), m_multiSel.end(), -6000 - si) != m_multiSel.end();
        for (const auto& b : own)
        {
            FreeCameraViewport::MapBoxDraw d;
            d.x = b.x; d.y = b.y; d.z = b.z; d.ry = b.ry;
            d.sx = b.sx; d.sy = b.sy; d.sz = b.sz;
            if (!cutBox(d)) continue;   // i gradini di una scala si sezionano come tutto
            d.selected = selected;
            // Cliccare un GRADINO seleziona la SCALA: i box derivati non esistono
            // come entità modificabili, quindi il pickId rimanda alla ricetta.
            // Codice -6000-i, coerente con gli altri intervalli negativi.
            d.pickId = -6000 - si;
            if (selected)
            {   // Tutta la struttura si accende: una scala si seleziona intera.
                d.r = 1.00f; d.g = 0.78f; d.b = 0.35f;
            }
            else
            {   // Tinta più fredda: si distingue a colpo d'occhio ciò che è generato
                // da ciò che è stato messo a mano.
                d.r = b.r * 0.85f; d.g = b.g * 0.95f; d.b = b.b * 1.15f;
                if (d.b > 1.0f) d.b = 1.0f;
            }
            draws.push_back(d);
        }
    }

    // NOTA: nessuna lastra a segnare il piano di taglio. Un primo tentativo la
    // disegnava (2026-08-05) ed era controproducente — diventava essa stessa un
    // tetto che copriva la vista, cioè l'opposto di ciò per cui si taglia. Il piano
    // si vede da sé: è la quota a cui la geometria risulta sezionata.

    // ── FIGURA DI SCALA (E6) ──────────────────────────────────────────────
    // L'errore più comune del blockout sono gli sbagli di SCALA, e il rimedio
    // raccomandato è banale: avere sotto gli occhi una figura di riferimento.
    // Si disegnano DUE sagome affiancate — l'unità di oggi e il gigante da 2,40 ×
    // 1,20 su cui sono dimensionate le metriche — così si vede subito se una porta
    // o un corridoio reggono anche il caso peggiore.
    if (m_showScaleFigure)
    {
        const glm::vec3 fp = { m_scaleFigX, m_scaleFigY, m_scaleFigZ };
        auto figure = [&](float dx, float w, float h, float r, float g, float b) {
            FreeCameraViewport::MapBoxDraw f;
            f.x = fp.x + dx; f.z = fp.z; f.ry = 0.0f;
            f.sy = h;  f.y = fp.y + h * 0.5f;
            f.sx = w;  f.sz = w * 0.6f;
            f.r = r; f.g = g; f.b = b;
            f.selected = false;
            f.pickId = FreeCameraViewport::MapBoxDraw::kNoPick;
            draws.push_back(f);
        };
        figure(-0.9f, 0.80f, 2.00f, 0.30f, 0.85f, 0.45f);   // clone/B1: ~2,0 m
        figure( 0.9f, mini::mapmetrics::REF_UNIT_WIDTH,
                      mini::mapmetrics::REF_UNIT_HEIGHT, 0.95f, 0.65f, 0.25f);  // gigante
    }

    // Spawn team1 (blu) come croce
    if (m_showGamePoints)
    {
        FreeCameraViewport::MapBoxDraw s;
        s.x = m_spawnTeam1[0]; s.y = m_spawnTeam1[1]; s.z = m_spawnTeam1[2];
        s.ry = 0; s.sx = 0.6f; s.sy = 1.2f; s.sz = 0.6f;
        s.r = 0.20f; s.g = 0.50f; s.b = 1.00f;
        s.selected = (m_selBox == -2);
        s.pickId = -2;
        draws.push_back(s);
    }
    // Spawn team2 (rosso)
    if (m_showGamePoints)
    {
        FreeCameraViewport::MapBoxDraw s;
        s.x = m_spawnTeam2[0]; s.y = m_spawnTeam2[1]; s.z = m_spawnTeam2[2];
        s.ry = 0; s.sx = 0.6f; s.sy = 1.2f; s.sz = 0.6f;
        s.r = 1.00f; s.g = 0.20f; s.b = 0.20f;
        s.selected = (m_selBox == -3);
        s.pickId = -3;
        draws.push_back(s);
    }
    // Punti multi-spawn: croci più piccole, azzurro (team1) / arancio (team2).
    for (int i = 0; i < (int)m_spawnPoints1.size() && m_showGamePoints; ++i)
    {
        FreeCameraViewport::MapBoxDraw s;
        s.x = m_spawnPoints1[i][0]; s.y = m_spawnPoints1[i][1]; s.z = m_spawnPoints1[i][2];
        s.ry = 0; s.sx = 0.5f; s.sy = 1.0f; s.sz = 0.5f;
        s.r = 0.40f; s.g = 0.70f; s.b = 1.00f;
        s.selected = (m_selBox == -3000 - i);
        s.pickId = -3000 - i;
        draws.push_back(s);
    }
    for (int i = 0; i < (int)m_spawnPoints2.size() && m_showGamePoints; ++i)
    {
        FreeCameraViewport::MapBoxDraw s;
        s.x = m_spawnPoints2[i][0]; s.y = m_spawnPoints2[i][1]; s.z = m_spawnPoints2[i][2];
        s.ry = 0; s.sx = 0.5f; s.sy = 1.0f; s.sz = 0.5f;
        s.r = 1.00f; s.g = 0.55f; s.b = 0.30f;
        s.selected = (m_selBox == -3100 - i);
        s.pickId = -3100 - i;
        draws.push_back(s);
    }

    // Command post: palo alto + area di cattura, colorati per team
    for (int i = 0; i < (int)m_posts.size() && m_showGamePoints; ++i)
    {
        const auto& p = m_posts[i];
        float r = 0.75f, g = 0.75f, b = 0.75f;
        if (p.team == 1) { r = 0.25f; g = 0.50f; b = 1.00f; }
        if (p.team == 2) { r = 1.00f; g = 0.25f; b = 0.25f; }
        const bool sel = (m_selBox == -10 - i);

        FreeCameraViewport::MapBoxDraw pole;
        pole.x = p.x; pole.y = p.y + 1.5f; pole.z = p.z; pole.ry = 0;
        pole.sx = 0.3f; pole.sy = 3.0f; pole.sz = 0.3f;
        pole.r = r; pole.g = g; pole.b = b;
        pole.selected = sel;
        pole.pickId = -10 - i;
        draws.push_back(pole);

        FreeCameraViewport::MapBoxDraw area;
        area.x = p.x; area.y = p.y + 0.05f; area.z = p.z; area.ry = 0;
        area.sx = p.radius * 2.0f; area.sy = 0.05f; area.sz = p.radius * 2.0f;
        area.r = r * 0.6f; area.g = g * 0.6f; area.b = b * 0.6f;
        area.selected = sel;
        area.pickId = -10 - i;
        draws.push_back(area);
    }

    // Comandante strategico (ADR-041): palo viola alto + disco del raggio di
    // leash (l'area da cui non esce). Se leash 0 il disco non si disegna (fermo).
    if (m_commander.exists && m_showGamePoints)
    {
        const bool sel = (m_selBox == kSelCommander);
        FreeCameraViewport::MapBoxDraw pole;
        pole.x = m_commander.x; pole.y = 1.6f; pole.z = m_commander.z; pole.ry = 0;
        pole.sx = 0.5f; pole.sy = 3.2f; pole.sz = 0.5f;
        pole.r = 0.65f; pole.g = 0.25f; pole.b = 0.85f;   // viola = comando
        pole.selected = sel;
        pole.pickId = kSelCommander;
        draws.push_back(pole);

        if (m_commander.leashRadius > 0.01f)
        {
            FreeCameraViewport::MapBoxDraw area;
            area.x = m_commander.x; area.y = 0.06f; area.z = m_commander.z; area.ry = 0;
            area.sx = m_commander.leashRadius * 2.0f; area.sy = 0.05f;
            area.sz = m_commander.leashRadius * 2.0f;
            area.r = 0.40f; area.g = 0.16f; area.b = 0.55f;
            area.selected = sel;
            area.pickId = kSelCommander;
            draws.push_back(area);
        }
    }

    // Bersagli strategici: box arancione, con la STESSA scala e rotazione che
    // avranno in gioco. Prima erano disegnati con `ry = 0` e lato fisso 2.5:
    // ruotare o scalare cambiava il dato ma non si vedeva nulla, quindi
    // sembravano "non funzionare" (segnalato dall'utente).
    for (int i = 0; i < (int)m_targets.size() && m_showGamePoints; ++i)
    {
        const auto& t = m_targets[i];
        const float sc = 2.5f * ((t.scale > 0.0001f) ? t.scale : 1.0f);
        FreeCameraViewport::MapBoxDraw s;
        // Base a `t.y` sopra il suolo (0 = a terra); +mezza altezza per centrare il box.
        s.x = t.x; s.y = t.y + sc * 0.5f; s.z = t.z; s.ry = t.ry;
        s.sx = sc; s.sy = sc; s.sz = sc;
        s.r = 0.85f; s.g = 0.55f; s.b = 0.15f;
        s.selected = (m_selBox == -500 - i);
        s.pickId = -500 - i;
        draws.push_back(s);
    }

    // ── Creazione prefab: RAGGIO e SELEZIONE visibili (2026-08-02) ───────
    // Prima il raggio era invisibile e si doveva indovinare cosa sarebbe finito nel
    // prefab. Ora si vede il disco della zona e gli elementi presi sono evidenziati:
    // la stessa selezione che verrà salvata (`prefabZoneCollect`, una sola verità).
    if (m_prefabZoneMode)
    {
        FreeCameraViewport::MapBoxDraw disc;
        disc.x = m_prefabZoneX; disc.y = 0.04f; disc.z = m_prefabZoneZ; disc.ry = 0.0f;
        disc.sx = m_newPrefabRadius * 2.0f; disc.sy = 0.03f;
        disc.sz = m_newPrefabRadius * 2.0f;
        disc.r = 0.35f; disc.g = 0.85f; disc.b = 0.95f;   // ciano = zona di raccolta
        disc.selected = false; disc.pickId = FreeCameraViewport::MapBoxDraw::kNoPick;
        draws.push_back(disc);

        // I BOX inclusi non hanno più un rombo sopra: sono colorati di ciano nel
        // ciclo principale (vedi sopra). Restano i marker per le POSIZIONI TATTICHE,
        // che non hanno un volume da colorare — sono punti.
        const std::vector<int>& selPos = zonePositions;
        for (int i : selPos)
        {
            if (i < 0 || i >= (int)m_positions.size()) continue;
            const auto& p = m_positions[i];
            FreeCameraViewport::MapBoxDraw m;
            m.x = p.x; m.y = p.y + 1.9f; m.z = p.z; m.ry = 45.0f;
            m.sx = 0.35f; m.sy = 0.35f; m.sz = 0.35f;
            m.r = 0.35f; m.g = 0.95f; m.b = 1.0f;
            m.selected = false; m.pickId = -1000 - i;
            draws.push_back(m);
        }
    }

    // ── Istanze di PREFAB (ADR-048): anteprima ───────────────────────────
    // Si disegnano i box del prefab con la STESSA trasformazione dell'espansione del
    // motore (rotazione attorno a Y + traslazione), così ciò che si vede nell'editor è
    // ciò che esisterà in partita. Tinta distinta: sono contenuto DERIVATO, non box
    // della mappa — non si editano qui, si edita l'istanza.
    for (int i = 0; i < (int)m_prefabInsts.size(); ++i)
    {
        const auto& inst = m_prefabInsts[i];
        const mini::PrefabDef* pf = m_prefabReg.getPrefab(inst.id);
        const bool sel = (m_selBox == -4000 - i);
        if (!pf)
        {   // Riferimento rotto: marker rosso, così si vede invece di sparire in silenzio.
            FreeCameraViewport::MapBoxDraw bad;
            bad.x = inst.x; bad.y = inst.y + 1.0f; bad.z = inst.z;
            bad.sx = 1.0f; bad.sy = 2.0f; bad.sz = 1.0f;
            bad.r = 0.95f; bad.g = 0.15f; bad.b = 0.15f;
            bad.selected = sel; bad.pickId = -4000 - i;
            draws.push_back(bad);
            continue;
        }
        const float rad = inst.ry * 3.14159265f / 180.0f;
        const float cs = std::cos(rad), sn = std::sin(rad);
        for (const auto& b : pf->collision)
        {
            FreeCameraViewport::MapBoxDraw d;
            d.x  = inst.x + b.x * cs + b.z * sn;
            d.z  = inst.z - b.x * sn + b.z * cs;
            d.y  = inst.y + b.y;
            d.ry = b.ry + inst.ry;
            d.sx = b.sx; d.sy = b.sy; d.sz = b.sz;
            d.r = 0.55f; d.g = 0.45f; d.b = 0.75f;   // viola: contenuto da prefab
            d.selected = sel; d.pickId = -4000 - i;
            draws.push_back(d);
        }
    }

    // ── Posizioni tattiche (ADR-030) ─────────────────────────────────────
    // Un solo marker per tutte: colore dal RUOLO, altezza dalla copertura.
    // Chi ripara (protection > 0) è una lastra alta `height` (si vede cosa
    // copre); chi non ripara è un pilastro sottile. Naso = fronte; disco = raggio
    // d'influenza (solo per i ruoli d'area). pickId = -1000 - i.
    for (int i = 0; i < (int)m_positions.size() && m_showPositions; ++i)
    {
        const auto& p = m_positions[i];
        const bool sel = (m_selBox == -1000 - i);
        float r = 0.15f, g = 0.85f, b = 0.70f;                       // cover (verde-acqua)
        if      (p.role == "vantage")     { r = 0.2f;  g = 0.8f;  b = 0.9f;  }
        else if (p.role == "defensive")   { r = 0.9f;  g = 0.4f;  b = 0.2f;  }
        else if (p.role == "chokepoint")  { r = 0.7f;  g = 0.3f;  b = 0.9f;  }
        else if (p.role == "observation") { r = 0.9f;  g = 0.85f; b = 0.2f;  }

        const bool shields = (p.protection > 0.0f);
        const float h = shields ? p.height : 1.2f;

        FreeCameraViewport::MapBoxDraw body;
        body.x = p.x; body.y = p.y + h * 0.5f; body.z = p.z; body.ry = p.facing;
        body.sx = shields ? 0.9f : 0.4f; body.sy = h; body.sz = shields ? 0.25f : 0.4f;
        body.r = r; body.g = g; body.b = b;
        body.selected = sel; body.pickId = -1000 - i;
        draws.push_back(body);

        const float fr = glm::radians(p.facing);
        FreeCameraViewport::MapBoxDraw nose;
        nose.x = p.x + std::sin(fr) * 0.5f; nose.y = p.y + h * 0.5f;
        nose.z = p.z + std::cos(fr) * 0.5f;
        nose.ry = p.facing; nose.sx = 0.2f; nose.sy = 0.2f; nose.sz = 0.5f;
        nose.r = r * 0.6f; nose.g = g * 0.6f; nose.b = b * 0.6f;
        nose.selected = sel; nose.pickId = -1000 - i;
        draws.push_back(nose);

        // Segnale di posizione CIECA sopra/sotto (KI #83): tacca rossa sospesa. Si
        // disegna SOLO dove il difetto esiste davvero — ci sono altre quote in giro
        // (vertPairs > 0) ma da qui non se ne batte nessuna. Serve a trovarle a colpo
        // d'occhio, senza selezionarle una per una: il colore del corpo resta quello
        // del RUOLO, che non va perso.
        if (i < (int)m_vertSight.size() && i < (int)m_vertPairs.size()
            && m_vertPairs[i] > 0 && m_vertSight[i] == 0)
        {
            FreeCameraViewport::MapBoxDraw blind;
            blind.x = p.x; blind.y = p.y + h + 0.55f; blind.z = p.z;
            blind.ry = 45.0f;   // rombo: si distingue dai marker quadrati
            blind.sx = 0.34f; blind.sy = 0.34f; blind.sz = 0.34f;
            blind.r = 0.95f; blind.g = 0.25f; blind.b = 0.20f;
            blind.selected = sel; blind.pickId = -1000 - i;
            draws.push_back(blind);
        }

        if (p.role == "defensive" || p.role == "chokepoint")
        {
            FreeCameraViewport::MapBoxDraw disc;
            disc.x = p.x; disc.y = p.y - 0.4f; disc.z = p.z; disc.ry = 0;
            disc.sx = p.radius * 2.0f; disc.sy = 0.03f; disc.sz = p.radius * 2.0f;
            disc.r = r; disc.g = g; disc.b = b;
            disc.selected = sel; disc.pickId = -1000 - i;
            draws.push_back(disc);
        }

        // Settore di tiro (ADR-031): i due bordi dell'arco, come raggi lunghi
        // quanto la gittata. SOLO sulla posizione selezionata — con 60 posizioni
        // disegnarli tutti renderebbe il viewport illeggibile. Senza vederlo il
        // settore non è autorabile con cura, ed è il dato più delicato.
        if (sel && p.canShoot)
        {
            const float halfArc = p.fireArc * 0.5f;
            const float len = p.fireRange;
            for (int s = 0; s < 2; ++s)
            {
                const float a = glm::radians(p.facing + (s == 0 ? -halfArc : halfArc));
                FreeCameraViewport::MapBoxDraw ray;
                ray.x = p.x + std::sin(a) * len * 0.5f;   // centro del raggio
                ray.y = p.y + 0.15f;
                ray.z = p.z + std::cos(a) * len * 0.5f;
                ray.ry = p.facing + (s == 0 ? -halfArc : halfArc);
                ray.sx = 0.10f; ray.sy = 0.06f; ray.sz = len;
                ray.r = 1.0f; ray.g = 0.85f; ray.b = 0.25f;   // giallo: linea di tiro
                ray.selected = false;                          // non ri-evidenziare
                ray.pickId = -1000 - i;
                draws.push_back(ray);
            }
        }
    }

    // Settore (ADR-034): disco ampio e tenue, colore per importanza. È l'area su
    // cui ragiona il comandante. pickId = -2000 - i.
    for (int i = 0; i < (int)m_sectors.size() && m_showAreas; ++i)
    {
        const auto& s = m_sectors[i];
        const bool sel = (m_selBox == -2000 - i);
        FreeCameraViewport::MapBoxDraw area;
        area.x = s.x; area.y = 0.02f; area.z = s.z; area.ry = 0;
        area.sx = s.radius * 2.0f; area.sy = 0.02f; area.sz = s.radius * 2.0f;
        area.r = 0.35f + s.importance * 0.5f;   // più importante = più acceso
        area.g = 0.30f; area.b = 0.75f;
        area.selected = sel; area.pickId = -2000 - i;
        draws.push_back(area);
    }

    // Danger zone: disco arancione (più rosso quanto più pericoloso)
    for (int i = 0; i < (int)m_dangers.size() && m_showAreas; ++i)
    {
        const auto& d = m_dangers[i];
        const bool sel = (m_selBox == -200 - i);
        FreeCameraViewport::MapBoxDraw area;
        area.x = d.x; area.y = d.y + 0.03f; area.z = d.z; area.ry = 0;
        area.sx = d.radius * 2.0f; area.sy = 0.04f; area.sz = d.radius * 2.0f;
        area.r = 0.9f; area.g = 0.55f - d.level * 0.45f; area.b = 0.10f;
        area.selected = sel;
        area.pickId = -200 - i;
        draws.push_back(area);
    }

    // Spawn veicoli: box arancio a misura di speeder + freccia direzione
    for (int i = 0; i < (int)m_vehSpawns.size() && m_showGamePoints; ++i)
    {
        const auto& v = m_vehSpawns[i];
        const bool sel = (m_selBox == -400 - i);

        FreeCameraViewport::MapBoxDraw body;
        body.x = v.x; body.y = 0.6f; body.z = v.z; body.ry = v.ry;
        body.sx = 1.0f; body.sy = 1.0f; body.sz = 2.6f;
        body.r = 0.95f; body.g = 0.60f; body.b = 0.15f;
        body.selected = sel;
        body.pickId = -400 - i;
        draws.push_back(body);

        const float vr = glm::radians(v.ry);
        FreeCameraViewport::MapBoxDraw nose;
        nose.x = v.x + std::sin(vr) * 1.6f;
        nose.y = 0.6f;
        nose.z = v.z + std::cos(vr) * 1.6f;
        nose.ry = v.ry;
        nose.sx = 0.3f; nose.sy = 0.3f; nose.sz = 0.6f;
        nose.r = 0.8f; nose.g = 0.45f; nose.b = 0.1f;
        nose.selected = sel;
        nose.pickId = -400 - i;
        draws.push_back(nose);
    }

    // Patrol route: pilastrino viola per punto (quello attivo più alto)
    for (int ri = 0; ri < (int)m_routes.size() && m_showRoutes; ++ri)
    {
        const auto& r = m_routes[ri];
        const bool routeSel = (m_selBox == -300 - ri);
        for (int pi = 0; pi < (int)r.points.size(); ++pi)
        {
            const bool activePt = routeSel && (pi == m_selRoutePt);
            FreeCameraViewport::MapBoxDraw wp;
            wp.x = r.points[pi][0];
            wp.y = r.points[pi][1] + (activePt ? 0.9f : 0.5f);
            wp.z = r.points[pi][2];
            wp.ry = 0;
            wp.sx = 0.3f; wp.sy = activePt ? 1.8f : 1.0f; wp.sz = 0.3f;
            wp.r = 0.65f; wp.g = 0.35f; wp.b = 0.95f;
            wp.selected = activePt;
            wp.pickId = -300 - ri;
            draws.push_back(wp);
        }
    }

    // Overlay navmesh: dopo i box, cosi il viewport lo disegna sopra.
    if (m_showNav && m_navBuilt) m_viewport.setNavMesh(m_navTris);
    else                         m_viewport.clearNavMesh();
    m_viewport.setMapBoxes(draws);

    // Gizmo sull'elemento selezionato (box o spawn point).
    // I box mappa ruotano solo attorno a Y; gli spawn: solo Sposta.
    // SELEZIONE MULTIPLA (G3): il gizmo si mette al BARICENTRO del gruppo, che è
    // il punto attorno a cui la rotazione fa orbitare gli elementi. Scala
    // disattivata: su un gruppo eterogeneo non ha un significato unico.
    if (m_multiSel.size() > 1)
    {
        glm::vec3 c(0.0f); int n = 0;
        for (int code : m_multiSel)
        { glm::vec3 p; if (codePosition(code, p)) { c += p; ++n; } }
        if (n > 0)
        {
            m_viewport.setGizmoTarget(c / (float)n, true);
            m_viewport.setGizmoRotAxes(false, true, false);
            m_viewport.setGizmoCanRotateScale(true, false);
        }
        else m_viewport.setGizmoTarget({0,0,0}, false);
    }
    // Struttura selezionata: il gizmo agisce sulla RICETTA (ADR-053). Sposta,
    // ruota e scala sono tutti e tre attivi — la scala però tocca i PARAMETRI del
    // tipo, non un box, così i gradini non si possono rompere.
    else if (m_selStruct >= 0 && m_selStruct < (int)m_structures.size())
    {
        const auto& s = m_structures[m_selStruct];
        m_viewport.setGizmoTarget({s.x, s.y, s.z}, true);
        m_viewport.setGizmoRotAxes(false, true, false);
        m_viewport.setGizmoCanRotateScale(true, true);
    }
    else if (m_selBox >= 0 && m_selBox < (int)m_boxes.size())
    {
        const auto& b = m_boxes[m_selBox];
        m_viewport.setGizmoTarget({b.x, b.y, b.z}, true);
        m_viewport.setGizmoRotAxes(false, true, false);
        m_viewport.setGizmoCanRotateScale(true, true);
    }
    else if (m_selBox == -2)
    {
        m_viewport.setGizmoTarget({m_spawnTeam1[0], m_spawnTeam1[1], m_spawnTeam1[2]}, true);
        m_viewport.setGizmoCanRotateScale(false, false);
    }
    else if (m_selBox == -3)
    {
        m_viewport.setGizmoTarget({m_spawnTeam2[0], m_spawnTeam2[1], m_spawnTeam2[2]}, true);
        m_viewport.setGizmoCanRotateScale(false, false);
    }
    else if (m_selBox <= -3000 && m_selBox > -3100
             && (-3000 - m_selBox) < (int)m_spawnPoints1.size())   // multi-spawn team1
    {
        const auto& p = m_spawnPoints1[-3000 - m_selBox];
        m_viewport.setGizmoTarget({p[0], p[1], p[2]}, true);
        m_viewport.setGizmoCanRotateScale(false, false);
    }
    else if (m_selBox <= -3100 && m_selBox > -3200
             && (-3100 - m_selBox) < (int)m_spawnPoints2.size())   // multi-spawn team2
    {
        const auto& p = m_spawnPoints2[-3100 - m_selBox];
        m_viewport.setGizmoTarget({p[0], p[1], p[2]}, true);
        m_viewport.setGizmoCanRotateScale(false, false);
    }
    else if (m_selBox == kSelCommander && m_commander.exists)   // ADR-041
    {
        m_viewport.setGizmoTarget({m_commander.x, 1.6f, m_commander.z}, true);
        m_viewport.setGizmoCanRotateScale(false, true);   // scala → raggio di leash
    }
    else if (m_selBox <= -10 && m_selBox > -100
             && (-10 - m_selBox) < (int)m_posts.size())
    {
        const auto& p = m_posts[-10 - m_selBox];
        m_viewport.setGizmoTarget({p.x, p.y, p.z}, true);
        m_viewport.setGizmoCanRotateScale(false, true);   // scala → raggio (ADR-025)
    }
    else if (m_selBox <= -200 && m_selBox > -300
             && (-200 - m_selBox) < (int)m_dangers.size())
    {
        const auto& d = m_dangers[-200 - m_selBox];
        m_viewport.setGizmoTarget({d.x, d.y, d.z}, true);
        m_viewport.setGizmoCanRotateScale(false, true);   // scala → raggio (ADR-025)
    }
    else if (m_selBox <= -300 && m_selBox > -400
             && (-300 - m_selBox) < (int)m_routes.size())
    {
        const auto& r = m_routes[-300 - m_selBox];
        if (m_selRoutePt >= 0 && m_selRoutePt < (int)r.points.size())
        {
            const auto& pt = r.points[m_selRoutePt];
            m_viewport.setGizmoTarget({pt[0], pt[1], pt[2]}, true);
            m_viewport.setGizmoCanRotateScale(false, false);
        }
        else
            m_viewport.setGizmoTarget({0,0,0}, false);
    }
    else if (m_selBox <= -400 && m_selBox > -500
             && (-400 - m_selBox) < (int)m_vehSpawns.size())
    {
        const auto& v = m_vehSpawns[-400 - m_selBox];
        m_viewport.setGizmoTarget({v.x, 0.6f, v.z}, true);
        m_viewport.setGizmoRotAxes(false, true, false);   // orientamento veicolo (ry)
        m_viewport.setGizmoCanRotateScale(true, false);   // ruota → ry (ADR-025)
    }
    else if (m_selBox <= -500 && m_selBox > -1000
             && (-500 - m_selBox) < (int)m_targets.size())
    {
        const auto& t = m_targets[-500 - m_selBox];
        m_viewport.setGizmoTarget({t.x, t.y + 1.25f, t.z}, true);
        m_viewport.setGizmoRotAxes(false, true, false);   // orientamento struttura
        m_viewport.setGizmoCanRotateScale(true, true);    // ruota → ry, scala → scale
    }
    else if (m_selBox <= -1000 && m_selBox > -2000
             && (-1000 - m_selBox) < (int)m_positions.size())   // ADR-030
    {
        const auto& p = m_positions[-1000 - m_selBox];
        m_viewport.setGizmoTarget({p.x, p.y, p.z}, true);
        m_viewport.setGizmoRotAxes(false, true, false);   // fronte della posizione
        m_viewport.setGizmoCanRotateScale(true, false);
    }
    // Istanza di PREFAB (ADR-048): PRIMA del ramo settori, che usa `<= -2000` e
    // catturerebbe anche -4000. Sposta e RUOTA (la scala no: scalare un prefab
    // deformerebbe le posizioni tattiche che porta con sé).
    else if (m_selBox <= -4000 && (-4000 - m_selBox) < (int)m_prefabInsts.size())
    {
        const auto& p = m_prefabInsts[-4000 - m_selBox];
        m_viewport.setGizmoTarget({p.x, p.y + 1.0f, p.z}, true);
        m_viewport.setGizmoRotAxes(false, true, false);   // solo yaw
        m_viewport.setGizmoCanRotateScale(true, false);
    }
    else if (m_selBox <= -2000 && (-2000 - m_selBox) < (int)m_sectors.size())   // ADR-034
    {
        const auto& s = m_sectors[-2000 - m_selBox];
        m_viewport.setGizmoTarget({s.x, 0.5f, s.z}, true);
        m_viewport.setGizmoCanRotateScale(false, true);   // scala → raggio del settore
    }
    else
        m_viewport.setGizmoTarget({0,0,0}, false);
}

// ── draw ─────────────────────────────────────────────────────────────────────
void MapEditor::draw()
{
    // ── Aggancio unico dell'undo per TUTTE le modifiche da widget ─────────
    // Invece di infilare un `pushUndo` accanto a ogni DragFloat (decine di punti,
    // e il primo che si dimentica è un'operazione non annullabile), si osserva
    // quando un widget diventa attivo: si fotografa lo stato PRIMA che l'utente
    // cominci a trascinare, e lo si consegna alla pila solo se qualcosa è davvero
    // cambiato. Un intero trascinamento diventa una sola voce di undo.
    {
        static bool s_wasActive = false;
        static Snapshot s_before;
        static bool s_dirtyBefore = false;
        const bool active = ImGui::IsAnyItemActive();
        if (active && !s_wasActive)
        {
            s_before      = captureState();
            s_dirtyBefore = m_dirty;
        }
        else if (!active && s_wasActive && m_dirty && !s_dirtyBefore)
        {
            // La fotografia è quella presa PRIMA che il widget diventasse attivo, non
            // quella attuale: si consegna alla pila con un'etichetta univoca perché
            // non venga fusa con nient'altro (è già un gesto concluso).
            m_hist.push(s_before, "widget", m_editorClock, /*window=*/-1.0f);
        }
        s_wasActive = active;
    }

    // Scorciatoie: Ctrl+Z / Ctrl+Y (e Ctrl+Shift+Z, che molti si aspettano).
    // Solo sul tab Mappa: da un tab struttura, Ctrl+A "seleziona tutti i box"
    // agirebbe su una mappa che non si sta guardando — la peggior specie di effetto,
    // quella che non si vede accadere. `m_activeTab` è del frame precedente, che per
    // l'input è il valore giusto.
    // ── Annulla/Ripeti nel TAB STRUTTURE (doc 52 F2) ─────────────────────
    // Stessa pila, stessi tasti: prima Ctrl+Z semplicemente non faceva nulla qui, e
    // "funziona solo in un posto" è la cosa che rende una scorciatoia inaffidabile.
    if (m_activeTab > 0 && m_activeTab <= (int)m_structTabs.size()
        && ImGui::GetIO().KeyCtrl && !ImGui::GetIO().WantTextInput)
    {
        auto& tb = m_structTabs[m_activeTab - 1];
        const bool wantRedo = ImGui::IsKeyPressed(ImGuiKey_Y, false)
            || (ImGui::IsKeyPressed(ImGuiKey_Z, false) && ImGui::GetIO().KeyShift);
        const bool wantUndo = ImGui::IsKeyPressed(ImGuiKey_Z, false)
            && !ImGui::GetIO().KeyShift;
        if (wantUndo || wantRedo)
        {
            StructTab::UndoState st = tb.snapshot(m_selPart);
            const bool did = wantRedo ? tb.undo.redo(st) : tb.undo.undo(st);
            if (did)
            {
                tb.def    = st.def;
                m_selPart = st.selPart;
                tb.dirty  = true;
                rebuildStructTabPreview(tb);
            }
        }
    }

    // ── Ctrl+S: salva, da qualunque tab ───────────────────────────────────
    // Vale anche sui tab struttura, e fa la cosa giusta per ognuno: la mappa se sei
    // sulla mappa, il TIPO se stai modificando un tipo, l'ISTANZA se stai modificando
    // una sola struttura in mappa. Una scorciatoia che salva "qualcosa" e non "quello
    // che stai guardando" è peggio di non averla.
    if (ImGui::GetIO().KeyCtrl && !ImGui::GetIO().WantTextInput
        && ImGui::IsKeyPressed(ImGuiKey_S, false))
    {
        if (m_activeTab == 0) { if (saveMap()) m_savedFlash = 1.5f; }
        else if (m_activeTab <= (int)m_structTabs.size())
        {
            auto& tb = m_structTabs[m_activeTab - 1];
            if (tb.target == StructTab::Target::Instance) applyInstanceTab(tb);
            else                                          saveStructType(tb);
            m_savedFlash = 1.5f;
        }
    }

    if (m_activeTab == 0 && ImGui::GetIO().KeyCtrl && !ImGui::GetIO().WantTextInput)
    {
        if (ImGui::IsKeyPressed(ImGuiKey_Z, false))
        {
            if (ImGui::GetIO().KeyShift) doRedo(); else doUndo();
        }
        else if (ImGui::IsKeyPressed(ImGuiKey_Y, false)) doRedo();
        // Ctrl+A: INTERRUTTORE seleziona-tutto / deseleziona-tutto.
        // Un tasto solo per i due versi dello stesso gesto — Esc da solo non è
        // affidabile qui (Windows se lo prende in certe combinazioni), e un pulsante
        // "Deseleziona" separato aggiungeva un secondo modo di fare la stessa cosa.
        else if (ImGui::IsKeyPressed(ImGuiKey_A, false))
        {
            const size_t total = m_boxes.size() + m_structures.size();
            if (m_multiSel.size() >= total && total > 0)
            {
                setSelection(-1, false);   // già tutto selezionato → si azzera
            }
            else
            {
                m_multiSel.clear();
                for (int i = 0; i < (int)m_boxes.size(); ++i) m_multiSel.push_back(i);
                for (int i = 0; i < (int)m_structures.size(); ++i) m_multiSel.push_back(-6000 - i);
                if (!m_multiSel.empty())
                {
                    const int last = m_multiSel.back();
                    m_selBox    = last >= 0 ? last : -1;
                    m_selStruct = last <= -6000 ? -6000 - last : -1;
                }
                updateViewport();
            }
        }
    }

    // Il risultato della verifica navmesh invecchia da solo appena la mappa cambia.
    if (m_navBuilt) m_navStale = (geometryFingerprint() != m_navFingerprint);

    const float totalW = ImGui::GetContentRegionAvail().x;
    const float totalH = ImGui::GetContentRegionAvail().y;

    // ── Barra dei tab (doc 48 S1) ─────────────────────────────────────────
    // "Mappa" è sempre il primo e non si chiude; i tipi aperti gli si affiancano
    // come tab chiudibili, in stile browser (richiesta esplicita dell'utente).
    if (!ImGui::BeginTabBar("##mapeditor_tabs",
                            ImGuiTabBarFlags_Reorderable | ImGuiTabBarFlags_FittingPolicyScroll))
        return;

    const float tabsH = ImGui::GetFrameHeight() + ImGui::GetStyle().ItemSpacing.y;

    if (ImGui::BeginTabItem("Mappa"))
    {
        m_activeTab = 0;
        drawMapTab(totalW, totalH - tabsH);
        ImGui::EndTabItem();
    }

    for (int i = 0; i < (int)m_structTabs.size(); )
    {
        auto& t = m_structTabs[i];
        // Il pallino delle modifiche non salvate è nel titolo: è dove uno lo cerca,
        // ed è l'unico posto che resta visibile anche quando il tab non è attivo.
        char title[192];
        std::snprintf(title, sizeof(title), "%s%s%s###stab%d",
                      t.def.label.empty() ? "(nuovo tipo)" : t.def.label.c_str(),
                      // Il bersaglio nel TITOLO: è l'unica parte del tab che resta
                      // visibile quando non è quello attivo, e confondere "questa
                      // struttura" con "il tipo" costa tre bunker.
                      t.target == StructTab::Target::Instance ? " (solo questa)" : "",
                      t.dirty ? " *" : "", i);
        bool open = true;
        ImGuiTabItemFlags fl = ImGuiTabItemFlags_None;
        if (m_focusLastTab && i == (int)m_structTabs.size() - 1)
        { fl |= ImGuiTabItemFlags_SetSelected; m_focusLastTab = false; }
        if (ImGui::BeginTabItem(title, &open, fl))
        {
            m_activeTab = i + 1;
            drawStructTab(t, totalW, totalH - tabsH);
            ImGui::EndTabItem();
        }
        if (!open)
        {
            // Chiudere un tab con modifiche non salvate le perderebbe in silenzio:
            // è il contratto esplicito di Unity Prefab Mode all'uscita.
            // Qui si REGISTRA solo l'intenzione: `OpenPopup` va chiamata FUORI dalla
            // barra dei tab, perché `BeginTabBar` spinge un proprio livello di ID
            // (`PushOverrideID`, imgui_widgets.cpp) — aprire qui e disegnare là dà
            // due identificatori diversi, e la finestra resta "aperta" senza essere
            // mai disegnata: un modale invisibile che blocca i clic altrove.
            if (t.dirty) m_pendingCloseTab = i;
            else { m_structTabs.erase(m_structTabs.begin() + i); continue; }
        }
        ++i;
    }
    ImGui::EndTabBar();

    // Apertura RINVIATA di un tab (dal tasto "Apri la struttura originale" su una
    // parte-riferimento): qui il ciclo è finito e nessuno tiene più un riferimento
    // dentro `m_structTabs`, quindi il push_back è sicuro.
    if (!m_pendingOpenType.empty())
    {
        const std::string id = m_pendingOpenType;
        m_pendingOpenType.clear();
        refreshStructTypeIds();
        openStructTab(id);
    }
    if (m_pendingEditInstance >= 0)
    {
        const int idx = m_pendingEditInstance;
        m_pendingEditInstance = -1;
        openInstanceTab(idx);
    }

    // Finestra CONDIVISA (doc 52 F4): apertura e disegno nella stessa funzione,
    // quindi nello stesso livello di ID per costruzione. Qui prima c'era un modale
    // scritto a mano — ed era proprio da questa coppia sparsa che era nato il
    // modale invisibile che bloccava i clic (changelog 164).
    {
        const int idx = m_pendingCloseTab;
        const bool valid = (idx >= 0 && idx < (int)m_structTabs.size());
        bool wanted = (m_pendingCloseTab >= 0);
        const auto c = editor::dialogs::saveDiscardCancel(
            "Tipo non salvato", wanted,
            valid ? ("Il tipo \"" + (m_structTabs[idx].def.label.empty()
                                     ? std::string("(senza nome)")
                                     : m_structTabs[idx].def.label) + "\" ha modifiche non salvate.")
                  : std::string("Tab non piu' disponibile."),
            {}, valid && !m_structTabs[idx].id.empty());
        if (c != editor::dialogs::Choice::None)
        {
            if (valid && c == editor::dialogs::Choice::Yes) saveStructType(m_structTabs[idx]);
            if (valid && c != editor::dialogs::Choice::Cancel)
            { m_structTabs.erase(m_structTabs.begin() + idx); m_activeTab = 0; }
            m_pendingCloseTab = -1;
        }
        else if (!wanted) m_pendingCloseTab = -1;   // la finestra si è chiusa da sé
    }

}

// ── FINESTRE A SÉ ────────────────────────────────────────────────────────────
// Le chiama `EditorApp` **dopo** aver chiuso la finestra del modulo. Non stanno in
// `draw()` perché lì sarebbero ancora dentro `Begin("Map Editor")`: una finestra
// top-level annidata in un'altra è lecita ma fragile, e a schermo intero le due si
// contendono il layout — è il tremolio nero, tre episodi in due giorni.
void MapEditor::drawFloatingWindows()
{
    drawProblemsWindow();
}

// Il tab Mappa: esattamente ciò che il Map Editor era prima dei tab.
void MapEditor::drawMapTab(float totalW, float totalH)
{
    drawToolbar();
    // ── L'ALTEZZA DELLA BARRA SI LEGGE SUBITO, PRIMA DI QUALUNQUE ALTRA COSA ──
    // `GetItemRectSize()` ritorna l'ULTIMO elemento disegnato, chiunque l'abbia
    // disegnato. Avevo infilato `drawProblemsWindow()` qui in mezzo: l'ultimo
    // elemento diventava un widget di QUELLA finestra, quindi `toolbarH` prendeva la
    // sua altezza. Con la finestra Problemi a schermo intero il valore diventava
    // enorme, `remaining` negativo, e il pannello sotto oscillava di frame in frame
    // — il tremolio nero che ha bloccato l'editor (2026-08-11, secondo episodio).
    //
    // La misura di un layout va presa **immediatamente** dopo ciò che si misura.
    const float toolbarH = ImGui::GetItemRectSize().y + ImGui::GetStyle().ItemSpacing.y;
    const float remaining = totalH - toolbarH - 4.0f;

    // Pannelli ridimensionabili: lista e proprietà con grip sul bordo destro;
    // il viewport prende lo spazio residuo.
    static float s_propW = 260.0f;

    ImGui::BeginChild("##map_panels", ImVec2(totalW, remaining), ImGuiChildFlags_None);

    // ── Lista box ────────────────────────────────────────────────────────
    ImGui::BeginChild("##box_list", ImVec2(200, 0),
                      ImGuiChildFlags_Borders | ImGuiChildFlags_ResizeX);
    drawBoxList(ImGui::GetContentRegionAvail().x, remaining);
    ImGui::EndChild();
    const float listW = ImGui::GetItemRectSize().x;

    ImGui::SameLine();

    float vpW = totalW - listW - s_propW - ImGui::GetStyle().ItemSpacing.x * 2;
    if (vpW < 120.0f) vpW = 120.0f;

    // ── Viewport 3D ──────────────────────────────────────────────────────
    ImGui::BeginChild("##map_vp", ImVec2(vpW, 0), ImGuiChildFlags_None,
                      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    drawViewport(vpW, remaining);
    ImGui::EndChild();

    ImGui::SameLine();

    // ── Maniglia di ridimensionamento del pannello destro ────────────────
    // Splitter ESPLICITO invece di `ChildFlags_ResizeX`: quel flag mette il grip sul
    // bordo DESTRO del child, che qui coincide col bordo della finestra — una volta
    // stretto il pannello non c'era più nulla da afferrare e non si riallargava
    // (segnalato dall'utente). Con una maniglia a SINISTRA il gesto è simmetrico.
    ImGui::InvisibleButton("##propsplit", ImVec2(6.0f, remaining));
    if (ImGui::IsItemHovered() || ImGui::IsItemActive())
        ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
    if (ImGui::IsItemActive())
        s_propW -= ImGui::GetIO().MouseDelta.x;   // trascinando a sinistra si ALLARGA
    // Clamp: mai sotto il minimo leggibile, mai oltre metà finestra → non può
    // "incastrarsi" in uno stato da cui non si torna indietro.
    const float propMax = (totalW > 400.0f) ? totalW * 0.5f : 200.0f;
    if (s_propW < 180.0f)    s_propW = 180.0f;
    if (s_propW > propMax)   s_propW = propMax;
    ImGui::SameLine();

    // ── Proprietà ────────────────────────────────────────────────────────
    ImGui::BeginChild("##box_props", ImVec2(s_propW, 0), ImGuiChildFlags_Borders);
    drawProperties(ImGui::GetContentRegionAvail().x, remaining);
    ImGui::EndChild();

    ImGui::EndChild();
}

// ── drawToolbar ───────────────────────────────────────────────────────────────
// ── PEZZI DELLA BARRA, ESTRATTI PERCHÉ VANNO MISURATI ────────────────────────
// Una voce della barra deve dichiarare la propria larghezza prima di essere
// disegnata (è così che si decide chi ci sta). Un blocco scritto in linea non può
// farlo: queste sono le due voci "larghe", con la loro misura accanto al disegno,
// così non possono divergere.
void MapEditor::drawSnapCombo()
{
    ImGui::TextUnformatted("Snap:");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(60.0f);
    // Gli stessi passi che scorre Ctrl+rotella: due elenchi diversi darebbero un
    // combo che mostra valori che la rotella non raggiunge, e viceversa.
    const float snapValues[] = {0.0f, 0.10f, 0.25f, 0.5f, 1.0f, 2.0f, 4.0f, 8.0f};
    const char* snapLabels[] = {"Off","0.10","0.25","0.5","1.0","2.0","4.0","8.0"};
    constexpr int kSnapN = 8;
    int snapIdx = 3;
    for (int i = 0; i < kSnapN; ++i)
        if (std::fabs(m_gridSnap - snapValues[i]) < 0.001f) { snapIdx = i; break; }
    if (ImGui::BeginCombo("##snap", snapLabels[snapIdx], ImGuiComboFlags_NoArrowButton))
    {
        for (int i = 0; i < kSnapN; ++i) {
            bool s = (i == snapIdx);
            if (ImGui::Selectable(snapLabels[i], s))
            { m_gridSnap = snapValues[i]; m_viewport.setGridSnap(m_gridSnap); }
            if (s) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }
}

float MapEditor::navmeshWidgetWidth() const
{
    const char* lbl = m_navStale ? "Verifica navmesh" : "Ri-verifica navmesh";
    float w = editor::toolbar::buttonWidth(lbl);
    if (m_navBuilt)
        w += ImGui::GetStyle().ItemSpacing.x * 2.0f
           + ImGui::CalcTextSize("Mostra").x + ImGui::GetFrameHeight()
           + ImGui::CalcTextSize("(mappa cambiata)").x;
    return w;
}

// Validazione navmesh: sostituisce la vecchia spunta "Area navigabile", che
// evidenziava i box di tipo `floor` — cioè l'INTENZIONE dell'autore, non ciò su cui
// l'AI può davvero camminare. Le due cose divergono, ed è tutto KI #97.
void MapEditor::drawNavmeshWidget()
{
    const bool bad = m_navBuilt && (m_navReport.islandTris > 0
                   || !m_navReport.badPositions.empty() || !m_navReport.badPosts.empty());
    if (m_navBuilt) ImGui::PushStyleColor(ImGuiCol_Text,
        bad ? ImVec4(0.95f, 0.45f, 0.35f, 1.0f) : ImVec4(0.45f, 0.85f, 0.50f, 1.0f));
    if (ImGui::Button(m_navStale ? "Verifica navmesh" : "Ri-verifica navmesh"))
        validateNavmesh();
    if (m_navBuilt) ImGui::PopStyleColor();
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Costruisce il navmesh VERO (stesso codice del gioco) sullo\n"
                          "stato che stai editando, primitive comprese, e mostra dove\n"
                          "si cammina davvero.\n"
                          "VERDE = si arriva dallo spawn · ROSSO = isola.\n"
                          "Il gate sui dati NON puo' vederlo: dipende dalla\n"
                          "voxelizzazione, non dalla geometria dichiarata.");
    if (!m_navBuilt) return;
    ImGui::SameLine();
    if (ImGui::Checkbox("Mostra", &m_showNav))
        updateViewport(/*recomputeDerived=*/false);
    ImGui::SameLine();
    if (m_navStale)
    { ImGui::TextColored({0.90f, 0.75f, 0.35f, 1.0f}, "(mappa cambiata)"); return; }
    if (!bad)
    { ImGui::TextColored({0.45f, 0.85f, 0.50f, 1.0f}, "tutto connesso"); return; }
    // L'ESITO NEGATIVO non si scrive più qui. Restava rosso in permanenza anche per
    // 24 triangoli su 484 — e un avviso sempre acceso smette di essere un avviso e
    // diventa arredamento (segnalato dall'utente). Il dettaglio, raggruppato e
    // navigabile, sta nella finestra Problemi, che è l'unico posto in cui si
    // guardano i difetti di questa mappa.
    ImGui::TextDisabled("vedi Problemi");
}

// "Prova da qui" (doc 53 L4). Estratta perché ora la invocano due strade — il
// pulsante in barra e la voce del menu «...» — e l'azione deve essere una sola:
// due copie divergono al primo cambio, e una delle due smette di salvare.
void MapEditor::requestPlaytest()
{
    if (m_mapId.empty())
    { m_playtestNote = "La mappa non ha ancora un nome: salvala prima."; return; }
    if (m_dirty && !saveMap())
    { m_playtestNote = "Salvataggio fallito: non lancio il gioco."; return; }
    // DOVE SEI, non dove guardi. Prima usavo `groundFocusPoint()`, cioè il punto in
    // cui lo sguardo incontra il piano y=0: su una mappa piatta coincide con
    // l'intuizione, su una passerella rialzata no — l'utente si è messo sopra una
    // passerella di Warfare Ground ed è nato sotto (2026-08-11). La posizione della
    // telecamera è l'unica lettura che non ha casi speciali: mi porto dove voglio
    // comparire, e ci compaio.
    //
    // La QUOTA è il pezzo che mancava: senza, il motore prende la superficie più alta
    // a quelle coordinate, che su più livelli non è quella su cui sei. Con la quota
    // prende la più alta **sotto** di te.
    const glm::vec3 cam = m_viewport.camera().getPosition();
    char buf[320];
    // `--walk` e NON `--direct-prematch`: il pre-partita è un menu, e un menu fra te
    // e la mappa toglie a questo comando l'unica cosa che deve avere — l'immediatezza
    // (segnalato dall'utente: *"devo semplicemente poter camminare per la mappa"*).
    // Nessun manichino: si prova la percorribilità, non il combattimento.
    std::snprintf(buf, sizeof(buf), "--walk --map \"%s\" --at %.2f,%.2f,%.2f",
                  m_mapId.c_str(), cam.x, cam.y, cam.z);
    m_playtestArgs = buf;
    std::snprintf(buf, sizeof(buf), "Avvio a %.1f, %.1f (quota %.1f)...", cam.x, cam.z, cam.y);
    m_playtestNote = buf;
}

void MapEditor::drawToolbar()
{
    // ── LA BARRA È DICHIARATA, NON DISEGNATA A MANO ───────────────────────
    // Prima era una fila di diciotto controlli scritti uno dopo l'altro con
    // `SameLine()`. Aggiungerne uno la faceva superare la larghezza del pannello e
    // l'ULTIMO smetteva di esistere per chi lo usa — è successo tre volte, l'ultima
    // con "Prova da qui", consegnato e mai trovato.
    //
    // La causa non è la distrazione: **io non vedo lo schermo**, quindi una regola
    // che chiede di guardare il risultato non posso rispettarla. Qui l'elenco è un
    // dato: `editor::toolbar` lo misura e manda l'eccedenza in un menu «...» invece
    // che oltre il bordo. Il taglio diventa inesprimibile, e `m_tbReport` dice
    // quante voci sono rientrate — misura senza occhi (CLAUDE.md §5-bis).
    //
    // L'ORDINE DELL'ELENCO È LA PRIORITÀ: la prima voce è l'ultima a finire nel menu.
    using editor::toolbar::Item;
    std::vector<Item> tb;
    auto add = [&](Item it) { tb.push_back(std::move(it)); };
    // Apertura RINVIATA dei popup: una voce scelta dentro il menu «...» chiamerebbe
    // `OpenPopup` da un livello di ID diverso, e il popup resterebbe aperto senza
    // essere mai disegnato — il modale invisibile del changelog 164.
    auto openLater = [this](const char* id) { return [this, id]() { m_pendingPopup = id; }; };

    // [1] Mappa corrente — contesto, non comando: sempre in barra.
    add({ "##mapsel", "Mappa corrente (in coda: crea una mappa nuova)", {}, true, 140.0f,
          [this]() {
              ImGui::SetNextItemWidth(140.0f);
              if (ImGui::BeginCombo("##mapsel",
                                    m_mapId.empty() ? "-- nessuna --" : m_mapId.c_str()))
              {
                  for (auto& me : m_mapList)
                  {
                      bool sel = (me.id == m_mapId);
                      if (ImGui::Selectable(me.id.c_str(), sel)) loadMap(me.id);
                      if (sel) ImGui::SetItemDefaultFocus();
                  }
                  ImGui::Separator();
                  if (ImGui::Selectable("+ Nuova mappa...")) m_pendingPopup = "Nuova mappa";
                  ImGui::EndCombo();
              }
          } });

    // [2] Il pallino delle modifiche non salvate. Con "Salva" finito dentro un menu,
    // questo è ciò che dice se serve premerlo: deve restare visibile sempre.
    add({ "##dirty", "", {}, true, 14.0f, [this]() {
              if (m_dirty) ImGui::TextColored({1.0f,0.7f,0.2f,1.0f}, "*");
              else         ImGui::TextDisabled(" ");
          } });

    // [3] Passo di aggancio — si cambia di continuo mentre si costruisce.
    add({ "##snapg", "Passo di aggancio. Ctrl+rotella nel viewport lo cambia senza\n"
                     "venire fin qui: grande per le stanze, piccolo per la rifinitura.",
          {}, true, 108.0f, [this]() { drawSnapCombo(); } });

    add({ "Mappa", "Salva (Ctrl+S), rinomina, crea.", openLater("##mapmenu") });
    add({ "Crea",  "Box, strutture parametriche, disegno libero.", openLater("##createmenu") });
    add({ "Modifica", "Duplica, serie, precisione, elimina.", openLater("##editmenu") });

    add({ "Annulla", "Ctrl+Z — annulla l'ultima modifica", [this]() { doUndo(); } });
    add({ "Ripristina", "Ctrl+Y o Ctrl+Shift+Z", [this]() { doRedo(); } });

    add({ "Vista", "Nasconde per tipo o per quota. Solo visivo: non tocca i dati.",
          openLater("##vista") });

    // ── PROBLEMI: un solo posto, e un solo indicatore ─────────────────────
    // Sostituisce tre presentazioni diverse (salute tattica nel pannello, esito
    // navmesh scritto in barra, gate dei dati sotto l'elenco). Il colore dice il
    // grado, il numero la quantità, e il clic apre il dettaglio — invece di una
    // scritta rossa permanente che dopo due giorni non si legge più.
    {
        int np = 0, nw = 0;
        for (const auto& is : m_issues) { if (is.sev == 1) ++np; else ++nw; }
        for (const auto& l : m_gateLines) { if (l.first == 1) ++np; else ++nw; }
        if (m_navBuilt && !m_navStale)
        {
            for (const auto& i : m_navReport.islands) { if (i.area >= 6.0f) ++np; else ++nw; }
            np += (int)m_navReport.badPositions.size() + (int)m_navReport.badPosts.size();
        }
        char lbl[64];
        std::snprintf(lbl, sizeof(lbl), np + nw == 0 ? "Problemi" : "Problemi (%d)", np + nw);
        add({ lbl, "Tutti i difetti di questa mappa in un posto solo, raggruppati per\n"
                   "tipo. Clicca una voce e ti porta li'.",
              [this]() { m_showProblems = !m_showProblems; }, false, 0.0f,
              [this, np, nw, lbl]() {
                  const bool any = (np + nw) > 0;
                  if (any) ImGui::PushStyleColor(ImGuiCol_Text,
                      np > 0 ? ImVec4(0.95f, 0.55f, 0.40f, 1.0f)
                             : ImVec4(0.90f, 0.85f, 0.40f, 1.0f));
                  if (ImGui::Button(lbl)) m_showProblems = !m_showProblems;
                  if (any) ImGui::PopStyleColor();
              } });
    }

    add({ "Prova da qui",
          "Salva la mappa e ti mette a camminare DOVE SEI con la telecamera —\n"
          "compresa la QUOTA: sopra una passerella nasci sulla passerella.\n"
          "Da solo, senza menu. Camminare e' l'unico modo di accorgersi che una\n"
          "stanza e' troppo grande o un corridoio troppo stretto.",
          [this]() { requestPlaytest(); } });

    // [ultima] Navmesh: larga, con lo stato accanto. È anche quella che si può
    // rimandare al menu senza danno — il suo esito resta scritto nel pannello.
    add({ "Navmesh", "Costruisce il navmesh VERO (stesso codice del gioco) sullo stato\n"
                     "che stai editando. VERDE = si arriva dallo spawn, ROSSO = isola.",
          [this]() { validateNavmesh(); }, false, navmeshWidgetWidth(),
          [this]() { drawNavmeshWidget(); } });

    m_tbReport = editor::toolbar::draw("maptb", tb);

    // Il popup richiesto si apre QUI, allo stesso livello di ID in cui verrà
    // disegnato: è la regola di doc 52 F4, e il motivo per cui esiste `m_pendingPopup`.
    if (!m_pendingPopup.empty())
    { ImGui::OpenPopup(m_pendingPopup.c_str()); m_pendingPopup.clear(); }

    // ── I MENU RAGGRUPPATI ────────────────────────────────────────────────
    if (ImGui::BeginPopup("##mapmenu"))
    {
        if (ImGui::MenuItem("Salva", "Ctrl+S", false, m_dirty || true))
        { if (saveMap()) m_savedFlash = 1.5f; }
        if (ImGui::MenuItem("Rinomina...")) m_pendingPopup = "##renamemap";
        if (ImGui::MenuItem("Nuova mappa...")) m_pendingPopup = "Nuova mappa";
        ImGui::EndPopup();
    }
    if (ImGui::BeginPopup("##createmenu"))
    {
        if (ImGui::MenuItem("Box")) addBox();
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Un box a misura fissa, dove stai guardando.");
        if (ImGui::MenuItem("Struttura parametrica...")) m_pendingPopup = "##addstruct";
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Forme che si autorano come RICETTA, non come box:\n"
                              "un'alzata sbagliata diventa impossibile da disegnare.");
        const bool on = m_viewport.drawBoxActive();
        if (ImGui::MenuItem("Disegna sulla griglia", nullptr, on))
            m_viewport.setDrawBoxActive(!on);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Traccia l'impronta di un box trascinando sul piano di\n"
                              "lavoro. Le misure si leggono MENTRE trascini.");
        if (on)
        {
            ImGui::Separator();
            ImGui::SetNextItemWidth(90.0f);
            if (ImGui::DragFloat("altezza", &m_drawHeight, 0.1f, 0.1f, 40.0f, "%.2f m")
                && m_drawHeight < 0.1f) m_drawHeight = 0.1f;
            ImGui::SetNextItemWidth(90.0f);
            if (ImGui::DragFloat("quota", &m_drawPlaneY, 0.1f, -20.0f, 100.0f, "%.2f m"))
                m_viewport.setDrawPlaneY(m_drawPlaneY);
            ImGui::TextDisabled("la quota e' la BASE dei box nuovi");
        }
        ImGui::EndPopup();
    }
    if (ImGui::BeginPopup("##editmenu"))
    {
        const bool hasSel = !selectionCodes().empty();
        if (ImGui::MenuItem("Duplica", nullptr, false, hasSel)) duplicateSelected();
        if (ImGui::MenuItem("Serie...", nullptr, false, hasSel)) m_pendingPopup = "##array";
        if (ImGui::MenuItem("Precisione...", nullptr, false, hasSel))
            m_pendingPopup = "##precis";
        ImGui::Separator();
        if (ImGui::MenuItem("Elimina", nullptr, false, hasSel))
            m_pendingPopup = "##del_confirm";
        ImGui::EndPopup();
    }

    if (ImGui::BeginPopup("##addstruct"))
    {
        // Raggruppate per COSA SERVONO, non per ordine di implementazione: si cerca
        // "come salgo" o "come chiudo uno spazio", non "la quarta primitiva".
        ImGui::TextDisabled("Salire");
        if (ImGui::MenuItem("Scala"))  addStructure(mini::StructureKind::Stair);
        if (ImGui::MenuItem("Rampa"))  addStructure(mini::StructureKind::Ramp);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Alzata fine: dolce, adatta anche ai veicoli.");

        ImGui::Separator();
        ImGui::TextDisabled("Chiudere e aprire");
        if (ImGui::MenuItem("Muro"))   addStructure(mini::StructureKind::Wall);
        if (ImGui::MenuItem("Muro con apertura"))
            addStructure(mini::StructureKind::Doorway);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Porta o finestra. Stipiti e architrave li mette lui,\n"
                              "con le misure normative. Con il parapetto (sill) diventa\n"
                              "una finestra, e il parapetto e' COPERTURA vera.");
        if (ImGui::MenuItem("Stanza (guscio)"))
            addStructure(mini::StructureKind::Room);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Pavimento + 4 muri + soffitto opzionale, con una porta\n"
                              "per ogni lato dichiarato: un interno non nasce senza\n"
                              "vie d'ingresso.");

        ImGui::Separator();
        ImGui::TextDisabled("Terreno tattico");
        if (ImGui::MenuItem("Piattaforma con accessi"))
            addStructure(mini::StructureKind::Platform);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Dichiara da quali lati ci si sale: le scale nascono\n"
                              "con lei. Non puo' venire irraggiungibile.");
        if (ImGui::MenuItem("Passerella"))
            addStructure(mini::StructureKind::Catwalk);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Un corridoio IN QUOTA: domina il piano di sotto.\n"
                              "Parapetti opzionali — riparano, ma tolgono la visuale\n"
                              "verso il basso (KI #83).");
        if (ImGui::MenuItem("Linea di coperture"))
            addStructure(mini::StructureKind::Barricade);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Barricata a intervalli. Emette box di tipo `cover`,\n"
                              "che e' cio' che la derivazione dei metadata cerca.");

        // ── La LIBRERIA: tipi nominati e gia' verificati (ADR-055) ────────
        // Sopra ci sono le primitive NUDE, da riparametrizzare ogni volta; qui le
        // forme che qualcuno ha gia' tarato e collaudato. E' la differenza fra
        // avere nove ricette e avere una libreria.
        if (!m_structTypeIds.empty())
        {
            ImGui::Separator();
            ImGui::TextDisabled("Libreria");

            // Raggruppata per CATEGORIA, in sottomenu: con decine di tipi un elenco
            // piatto è lo stesso problema che avevano le liste del Map Editor.
            // Le categorie vengono dai DATI, così una nuova compare da sola.
            std::vector<std::string> cats;
            for (const auto& id : m_structTypeIds)
            {
                const auto* ty = m_prefabReg.getStructureType(id);
                const std::string c = (ty && !ty->category.empty()) ? ty->category
                                                                    : "Senza categoria";
                if (std::find(cats.begin(), cats.end(), c) == cats.end()) cats.push_back(c);
            }
            std::sort(cats.begin(), cats.end());

            for (const auto& cat : cats)
            {
            const bool single = (cats.size() == 1);
            // Con una sola categoria il sottomenu sarebbe un clic in più a vuoto.
            if (!single && !ImGui::BeginMenu(cat.c_str())) continue;
            for (const auto& id : m_structTypeIds)
            {
                const auto* ty = m_prefabReg.getStructureType(id);
                const std::string c = (ty && !ty->category.empty()) ? ty->category
                                                                    : "Senza categoria";
                if (c != cat) continue;
                const bool composite = (ty && mini::mapstructures::isAssembly(*ty));
                char item[192];
                // Il segno distingue una COMPOSITA da un preset di primitiva: sono
                // cose diverse da piazzare, e il nome da solo non lo dice.
                std::snprintf(item, sizeof(item), "%s %s%s",
                              composite ? "[+]" : "   ",
                              (ty && !ty->label.empty()) ? ty->label.c_str() : id.c_str(),
                              (ty && !ty->verified) ? "  (non verificata)" : "");
                const bool unverified = (ty && !ty->verified);
                if (unverified)
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.95f, 0.75f, 0.30f, 1.0f));
                if (ImGui::MenuItem(item) && ty)
                {
                    addStructure(ty->kind);
                    // L'istanza nasce dai predefiniti del tipo e ne porta l'id: e'
                    // il legame che rende il tipo una definizione e non un modello
                    // copiato una volta e poi dimenticato.
                    if (!m_structures.empty())
                    {
                        auto& s = m_structures.back();
                        const float px = s.x, py = s.y, pz = s.z, pry = s.ry;
                        s = ty->defaults;
                        s.x = px; s.y = py; s.z = pz; s.ry = pry;
                        s.type  = ty->id;
                        s.label = ty->label;
                        rebuildStructurePreview();
                        updateViewport();
                    }
                }
                if (unverified) ImGui::PopStyleColor();
                if (unverified && ImGui::IsItemHovered())
                    ImGui::SetTooltip("Questo tipo non ha superato la verifica navmesh:\n"
                                      "puo' generare superfici su cui l'AI non cammina.");
                else if (composite && ImGui::IsItemHovered())
                    ImGui::SetTooltip("[+] = struttura COMPOSITA: piu' parti in un pezzo solo.");
            }
            if (!single) ImGui::EndMenu();
            }   // fine categorie
        }

        // ── L'ingresso all'editor, in fondo (richiesta esplicita) ─────────
        // Si entra nella definizione DA DOVE la si usa: e' il gesto di Unity
        // Prefab Mode, e il motivo per cui il tasto sta qui e non in un menu a parte.
        ImGui::Separator();
        if (ImGui::MenuItem("Editor strutture..."))
        {
            refreshStructTypeIds();
            openStructTab("");
        }
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Apre un TAB accanto a \"Mappa\". Da li' si crea:\n"
                              " · un tipo SEMPLICE (una primitiva con i suoi limiti), oppure\n"
                              " · un ASSEMBLAGGIO: piu' parti (primitive e box) che formano\n"
                              "   una torre, un bunker, un edificio.\n"
                              "In entrambi i casi si verifica il navmesh sulla struttura\n"
                              "isolata prima di metterla in mappa.");
        if (!m_structTypeIds.empty() && ImGui::BeginMenu("Modifica un tipo"))
        {
            for (const auto& id : m_structTypeIds)
                if (ImGui::MenuItem(id.c_str())) { refreshStructTypeIds(); openStructTab(id); }
            ImGui::EndMenu();
        }
        ImGui::EndPopup();
    }
    if (ImGui::BeginPopup("##precis"))
    {
        const int nsel = (int)selectionCodes().size();
        ImGui::TextDisabled("%d element%s selezionat%s", nsel,
                            nsel == 1 ? "o" : "i", nsel == 1 ? "o" : "i");
        ImGui::SeparatorText("Sposta di una misura esatta");
        ImGui::SetNextItemWidth(210.0f);
        ImGui::DragFloat3("##off", m_offsetVal, 0.1f, -500.0f, 500.0f, "%.2f");
        if (ImGui::Button("Sposta", {100, 0}) && nsel > 0)
            moveSelectionBy({ m_offsetVal[0], m_offsetVal[1], m_offsetVal[2] });
        ImGui::SameLine();
        if (ImGui::Button("Un passo##offstep", {110, 0}))
        { m_offsetVal[0] = m_gridSnap; m_offsetVal[1] = 0.0f; m_offsetVal[2] = 0.0f; }
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Riempie X con il passo di griglia corrente (%.2f m).", m_gridSnap);

        ImGui::SeparatorText("Allinea (a filo, non al centro)");
        const char* axn[3] = { "X", "Y", "Z" };
        const char* modn[3] = { "min", "centro", "max" };
        for (int a = 0; a < 3; ++a)
        {
            ImGui::Text("%s", axn[a]); ImGui::SameLine();
            for (int m = 0; m < 3; ++m)
            {
                ImGui::PushID(a * 3 + m);
                if (ImGui::SmallButton(modn[m]) && nsel >= 2) alignSelection(a, m);
                ImGui::PopID();
                if (m < 2) ImGui::SameLine();
            }
            if (a < 2) ImGui::SameLine(0, 14);
        }
        ImGui::TextDisabled("a filo: due muri di spessore diverso restano complanari.\n"
                            "Un disallineamento di 3 cm fa una fessura che il navmesh\n"
                            "non attraversa, e non si vede.");

        ImGui::SeparatorText("Distribuisci (spazio uguale, estremi fermi)");
        for (int a = 0; a < 3; ++a)
        {
            ImGui::PushID(100 + a);
            if (ImGui::SmallButton(axn[a]) && nsel >= 3) distributeSelection(a);
            ImGui::PopID();
            if (a < 2) ImGui::SameLine();
        }
        ImGui::SameLine();
        ImGui::TextDisabled("(da 3 elementi in su)");

        // ── APPOGGIA / ACCOSTA ────────────────────────────────────────────
        ImGui::SeparatorText("Appoggia — fino a toccare, senza fessura");
        struct SnapBtn { const char* label; int axis; int dir; const char* tip; };
        static const SnapBtn sb[6] = {
            { "Giu'",  1, -1, "Posa la selezione sulla superficie sottostante (o a terra)." },
            { "Su",    1, +1, "La alza fino a toccare cio' che ha sopra." },
            { "-X",    0, -1, "La accosta all'ostacolo verso -X." },
            { "+X",    0, +1, "La accosta all'ostacolo verso +X." },
            { "-Z",    2, -1, "La accosta all'ostacolo verso -Z." },
            { "+Z",    2, +1, "La accosta all'ostacolo verso +Z." },
        };
        for (int i = 0; i < 6; ++i)
        {
            ImGui::PushID(200 + i);
            if (ImGui::SmallButton(sb[i].label) && nsel > 0)
                snapSelectionToSurface(sb[i].axis, sb[i].dir);
            ImGui::PopID();
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", sb[i].tip);
            if (i != 1 && i != 3 && i != 5) ImGui::SameLine();
            else if (i < 5) ImGui::SameLine(0, 14);
        }
        ImGui::TextDisabled("La selezione si muove come UN CORPO: la forma non cambia.\n"
                            "\"Sembra appoggiato\" a 3 cm da terra e' indistinguibile a\n"
                            "vista, ma quei 3 cm il navmesh non li perdona.");
        ImGui::EndPopup();
    }
    // ── SERIE (E4) ────────────────────────────────────────────────────────
    // Il pulsante sta nel menu "Modifica": la barra non deve crescere a ogni
    // comando nuovo. Il corpo del popup resta qui, disegnato sempre.
    if (ImGui::BeginPopup("##array"))
    {
        const int n = (int)selectionCodes().size();
        if (n == 0) ImGui::TextDisabled("Seleziona qualcosa prima.");
        else
        {
            ImGui::TextDisabled("%d element%s selezionat%s", n, n == 1 ? "o" : "i",
                                n == 1 ? "o" : "i");
            ImGui::SetNextItemWidth(160.0f);
            ImGui::DragInt("Copie", &m_arrayCount, 0.2f, 1, 200);
            ImGui::SetNextItemWidth(160.0f);
            ImGui::DragFloat3("Offset", m_arrayOff, 0.1f);
            ImGui::SetNextItemWidth(160.0f);
            ImGui::DragFloat("Rotazione", &m_arrayYawStep, 1.0f, -180.0f, 180.0f, "%.0f°/copia");
            ImGui::TextDisabled("Totale: %d nuovi elementi", m_arrayCount * n);
            ImGui::Separator();
            if (ImGui::Button("Crea", {110, 0})) { makeArray(); ImGui::CloseCurrentPopup(); }
            ImGui::SameLine();
            if (ImGui::Button("Annulla", {110, 0})) ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
    // ── Stato di salute SEMPRE VISIBILE (G7) ──────────────────────────────
    // Il conteggio stava solo dentro un pannello che bisognava aprire. Qui è
    // sott'occhio mentre si costruisce: se un'operazione introduce un problema, il
    // numero sale nello stesso istante.
    {
        int nProb = 0, nWarn = 0;
        for (const auto& is : m_issues) { if (is.sev >= 1) ++nProb; else ++nWarn; }
        ImGui::SameLine();
        if (nProb > 0)
            ImGui::TextColored({0.95f, 0.35f, 0.30f, 1.0f}, "%d problemi", nProb);
        else if (nWarn > 0)
            ImGui::TextColored({0.90f, 0.75f, 0.35f, 1.0f}, "%d avvisi", nWarn);
        else
            ImGui::TextColored({0.45f, 0.85f, 0.50f, 1.0f}, "nessun difetto");
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Salute tattica della mappa, ricalcolata a ogni modifica.\n"
                              "%d problemi, %d avvisi. I box colpevoli sono colorati nel\n"
                              "viewport; l'elenco completo è nel pannello a sinistra.",
                              nProb, nWarn);
    }

    // ── Tetti dei codici di selezione (doc 49 R1, KI #100) ────────────────
    // Compare SOLO quando serve: un avviso sempre acceso diventa arredamento.
    // A 80% avverte, al tetto è un errore — oltre, il codice di un elemento
    // cambia significato e si sposta un elemento di un altro tipo, in silenzio.
    {
        const auto caps = capacityReport();
        int worstPct = 0; const CapacityInfo* worst = nullptr;
        for (const auto& c : caps)
        {
            const int pct = (c.limit > 0) ? (c.used * 100 / c.limit) : 0;
            if (pct > worstPct) { worstPct = pct; worst = &c; }
        }
        if (worst && worstPct >= 80)
        {
            ImGui::SameLine();
            const bool over = (worst->used >= worst->limit);
            ImGui::TextColored(over ? ImVec4{0.95f, 0.35f, 0.30f, 1.0f}
                                    : ImVec4{0.90f, 0.75f, 0.35f, 1.0f},
                               over ? "TETTO RAGGIUNTO: %s %d/%d"
                                    : "vicino al tetto: %s %d/%d",
                               worst->name, worst->used, worst->limit);
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip(
                    "Ogni tipo di elemento ha un intervallo riservato di codici di\n"
                    "selezione, e il codice E' l'indice nell'array. Oltre il tetto il\n"
                    "codice ricade nell'intervallo di un ALTRO tipo: selezionare o\n"
                    "spostare colpisce l'elemento sbagliato, senza alcun errore.\n"
                    "Vedi doc 49 e KI #100.");
        }
    }

    if (ImGui::BeginPopup("##vista"))
    {
        // Ogni cambio ridisegna SUBITO. Prima nulla richiamava il ridisegno, quindi
        // le spunte cambiavano lo stato e non si vedeva nulla finché non si toccava
        // altro: il filtro sembrava non funzionare. `false` = non ricalcolare i dati
        // derivati, è solo vista.
        bool viewChanged = false;
        const char* tn[5] = { "Pavimenti", "Muri", "Piattaforme", "Coperture", "Decorazioni" };
        for (int i = 0; i < 5; ++i)
            if (ImGui::Checkbox(tn[i], &m_showType[i])) viewChanged = true;
        if (ImGui::Checkbox("Strutture (scale, rampe...)", &m_showStructures))
            viewChanged = true;

        // ── Marcatori, separati dalla geometria ──────────────────────────
        // Guardare il navmesh con 169 posizioni, i settori e i percorsi davanti
        // è illeggibile: la verifica serve a vedere le SUPERFICI. "Solo
        // geometria" è la scorciatoia che serve davvero — spegnerli a uno a uno
        // ogni volta è la ragione per cui un filtro non si usa.
        ImGui::SeparatorText("Marcatori");
        if (ImGui::Checkbox("Posizioni tattiche", &m_showPositions)) viewChanged = true;
        if (ImGui::Checkbox("Settori e zone di pericolo", &m_showAreas)) viewChanged = true;
        if (ImGui::Checkbox("Percorsi di pattuglia", &m_showRoutes)) viewChanged = true;
        if (ImGui::Checkbox("Spawn, post, bersagli, veicoli", &m_showGamePoints)) viewChanged = true;
        if (ImGui::Button("Solo geometria", {150, 0}))
        {
            m_showPositions = m_showAreas = m_showRoutes = m_showGamePoints = false;
            viewChanged = true;
        }
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Nasconde tutti i marcatori e lascia la sola geometria.\n"
                              "È la vista giusta per leggere l'overlay del navmesh.");
        ImGui::Separator();

        // Estensione VERA della mappa in quota: uno slider da -5 a 1000 sarebbe
        // inutilizzabile, e senza estremi sensati non si trova la quota giusta.
        float mapLo = 0.0f, mapHi = 8.0f;
        for (const auto& b : m_boxes)
        {
            mapLo = std::min(mapLo, b.y - b.sy * 0.5f);
            mapHi = std::max(mapHi, b.y + b.sy * 0.5f);
        }
        mapHi += 1.0f;
        const bool cutting = (m_hideAboveY < mapHi);
        float cut = cutting ? m_hideAboveY : mapHi;

        ImGui::SetNextItemWidth(200.0f);
        // Slider per trovare la quota trascinando (il taglio si vede mentre si
        // muove), Ctrl+click per digitarla precisa: ImGui dà entrambe le cose con
        // un controllo solo.
        if (ImGui::SliderFloat("Taglia sopra", &cut, mapLo, mapHi, "%.2f m"))
        { m_hideAboveY = (cut >= mapHi - 0.001f) ? 1000.0f : cut; viewChanged = true; }
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("VISTA IN SEZIONE: sopra questa quota la geometria sparisce,\n"
                              "e ciò che sta a cavallo viene tagliato al piano. È il modo\n"
                              "di guardare dentro un edificio dall'alto.\n"
                              "Ctrl+click per digitare un valore esatto.");
        ImGui::SameLine();
        if (ImGui::SmallButton("Tutto##nocut")) { m_hideAboveY = 1000.0f; viewChanged = true; }
        if (cutting)
            ImGui::TextDisabled("taglio a %.2f m (mappa %.1f → %.1f)", m_hideAboveY, mapLo, mapHi - 1.0f);
        else
            ImGui::TextDisabled("nessun taglio");
        ImGui::Separator();
        if (ImGui::Checkbox("Evidenzia i difetti", &m_showDefects)) viewChanged = true;
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Colora nel viewport i box con difetti: ROSSO = problema\n"
                              "(il navmesh non ci sale, nessuno ci arriva), AMBRA = avviso.\n"
                              "Stessa analisi del gate --validate, mostrata mentre costruisci.");
        if (ImGui::Checkbox("Figura di scala", &m_showScaleFigure))
        {
            if (m_showScaleFigure)
            {
                const glm::vec3 fp = m_viewport.groundFocusPoint();
                m_scaleFigX = fp.x; m_scaleFigY = fp.y; m_scaleFigZ = fp.z;
            }
            viewChanged = true;
        }
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Due sagome dove stai guardando ORA: l'unità di oggi\n"
                              "(2,0 m) e il gigante di riferimento (2,40 × 1,20) su cui\n"
                              "sono dimensionate le metriche. Restano dove le piazzi.");
        if (m_showScaleFigure && ImGui::SmallButton("Riposiziona qui"))
        {
            const glm::vec3 fp = m_viewport.groundFocusPoint();
            m_scaleFigX = fp.x; m_scaleFigY = fp.y; m_scaleFigZ = fp.z;
            viewChanged = true;
        }
        ImGui::Separator();
        if (ImGui::Button("Mostra tutto", {150, 0}))
        {
            for (bool& b : m_showType) b = true;
            m_showStructures = true;
            m_hideAboveY = 1000.0f;
            m_showPositions = m_showAreas = m_showRoutes = m_showGamePoints = true;
            viewChanged = true;
        }
        if (viewChanged) updateViewport(/*recomputeDerived=*/false);
        ImGui::EndPopup();
    }
    // Prima c'erano DUE caselle di testo affiancate con semantiche diverse: una
    // cambiava il NOME VISUALIZZATO (campo `name`), l'altra faceva il RENAME vero
    // (file + cross-reference). Due modi di "cambiare nome" con effetti diversi sono
    // una trappola, e occupavano permanentemente la toolbar (segnalato dall'utente
    // 2026-08-02). Ora: **un pulsante, un popup**, come Nuova mappa/Elimina — e il
    // rename allinea filename, id e nome visualizzato, così **il nome è uno solo,
    // uguale da qualunque parte lo si guardi**.
    if (!m_mapId.empty())
    {
        static char renameBuf[64] = "";
        static std::string renameErr;
        // Il pulsante è finito nel menu "Mappa" (richiesta dell'utente: raggruppare
        // salva e rinomina). Il campo si riempie all'apertura del popup, non al clic
        // del pulsante — che qui non esiste più.
        if (ImGui::IsPopupOpen("##renamemap") && renameBuf[0] == '\0')
        {
            std::snprintf(renameBuf, sizeof(renameBuf), "%s", m_mapId.c_str());
            renameErr.clear();
        }
        if (ImGui::BeginPopup("##renamemap"))
        {
            ImGui::TextDisabled("Il nome cambia ovunque: file, elenco e partita.");
            ImGui::SetNextItemWidth(220.0f);
            ImGui::InputText("##mrename", renameBuf, sizeof(renameBuf));
            if (!renameErr.empty())
                ImGui::TextColored({1.f,0.4f,0.4f,1.f}, "%s", renameErr.c_str());
            if (ImGui::Button("Rinomina", {110,0}) && renameBuf[0] != '\0'
                && m_mapId != renameBuf)
            {
                int refs = 0;
                renameErr = editor::rename::renameDefinition(
                    getDataDir() + "/", editor::rename::Category::Map,
                    m_mapId, renameBuf, &refs);
                if (renameErr.empty())
                {
                    const std::string newId = renameBuf;
                    // Il nome VISUALIZZATO segue l'id: è ciò che rende il nome unico.
                    editor::jsonsave::saveJsonRMW(
                        getDataDir() + "/maps/" + newId + ".json",
                        [&](nlohmann::json& j) { j["name"] = newId; return true; });
                    loadMaps();
                    loadMap(newId);
                    ImGui::CloseCurrentPopup();
                }
            }
            ImGui::SameLine();
            if (ImGui::Button("Annulla", {110,0})) ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
        }
    }


    // NB: i pulsanti modalità gizmo (Sposta/Ruota/Scala) NON stanno più qui: sono
    // l'overlay in alto a sinistra della viewport (FreeCameraViewport::drawGizmoOverlay),
    // che appare quando selezioni un oggetto. Erano un duplicato che saturava la
    // toolbar ([[ui-no-clipping-use-dropdowns]]). Le capacità ruota/scala per tipo di
    // selezione le imposta updateViewport() via setGizmoCanRotateScale, ogni frame.

    // ── CONFERMA DEL SALVATAGGIO, senza rubare un clic ────────────────────
    // Era un popup: con Ctrl+S sarebbe un popup che compare a ogni salvataggio e va
    // chiuso, cioè un premio per aver usato la scorciatoia. Ora è una scritta che
    // sfuma da sola accanto al nome della mappa.
    if (m_savedFlash > 0.0f)
    {
        ImGui::SameLine();
        ImGui::TextColored({0.40f, 0.95f, 0.45f, 1.0f}, "Salvato");
    }
    if (!m_playtestNote.empty())
    {
        ImGui::SameLine();
        ImGui::TextDisabled("%s", m_playtestNote.c_str());
    }
    // Quante voci non sono entrate: è la MIA misura, e si vede anche a lui. Senza,
    // "il comando non c'è" e "il comando è nel menu" sono indistinguibili.
    if (m_tbReport.inOverflow > 0 && ImGui::IsItemHovered()) { /* nel tooltip di «...» */ }
    if (ImGui::BeginPopupModal("##del_confirm", nullptr,
                               ImGuiWindowFlags_AlwaysAutoResize)) {
        const int n = (int)selectionCodes().size();
        if (n > 1) ImGui::Text("Eliminare i %d elementi selezionati?", n);
        else       ImGui::Text("Eliminare l'elemento selezionato?");
        if (ImGui::Button("Sì")) { deleteSelection(); ImGui::CloseCurrentPopup(); }
        ImGui::SameLine();
        if (ImGui::Button("No")) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }

    // ── Nuova mappa (id = nome file, ADR-001) ─────────────────────────────
    // Aperto dalla voce "＋ Nuova mappa…" in coda alla combo. Nominare un file
    // NUOVO è l'eccezione legittima alla regola "dropdown-only" (Todo, ADR-010).
    if (ImGui::BeginPopupModal("Nuova mappa", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        static char newMapId[64] = "";
        static std::string newMapErr;
        ImGui::TextUnformatted("Nome della nuova mappa (= nome file):");
        ImGui::SetNextItemWidth(260.0f);
        if (ImGui::IsWindowAppearing()) ImGui::SetKeyboardFocusHere();
        const bool entered = ImGui::InputText("##newmapid", newMapId, sizeof(newMapId),
                                              ImGuiInputTextFlags_EnterReturnsTrue);
        if (!newMapErr.empty())
            ImGui::TextColored({1.f,0.4f,0.4f,1.f}, "%s", newMapErr.c_str());

        auto tryCreate = [&]() -> bool {
            if (newMapId[0] == '\0') { newMapErr = "nome vuoto"; return false; }
            const std::string id = newMapId;
            // id = filename stem: gli id mappa ammettono spazi ("Training Ground"),
            // ma non i separatori di percorso/caratteri illegali (no traversal).
            const bool badChar = id.find_first_of("/\\:*?\"<>|") != std::string::npos
                                 || id.front() == '.' || id.back() == ' ';
            const std::string path = getDataDir() + "/maps/" + id + ".json";
            if (badChar)            { newMapErr = "nome non valido"; return false; }
            if (fs::exists(path))   { newMapErr = "esiste gia'";     return false; }
            // JSON minimo VALIDO e GIOCABILE: un pavimento (senza, niente navmesh →
            // unità nel vuoto, ContentValidation lo rifiuta) e i due spawn. Muri,
            // metadata, roster e comandante si autorano dopo.
            editor::jsonsave::saveJsonRMW(path, [&](json& j) {
                j["name"]        = id;
                j["spawn_team1"] = {0.0f, 0.86f,  8.0f};
                j["spawn_team2"] = {0.0f, 0.86f, -8.0f};
                json floor;
                floor["type"]  = "floor";  floor["label"] = "Pavimento";
                floor["x"]  = 0.0f;  floor["y"]  = -0.1f;  floor["z"]  = 0.0f;
                floor["ry"] = 0.0f;
                floor["sx"] = 50.0f; floor["sy"] = 0.4f;   floor["sz"] = 40.0f;
                floor["r"]  = 0.40f; floor["g"]  = 0.36f;  floor["b"]  = 0.30f;
                floor["collider"] = true;
                j["geometry"] = json::array({floor});
                return true;
            });
            loadMaps();
            loadMap(id);            // passa subito alla mappa nuova
            return true;
        };

        if ((ImGui::Button("Conferma", {120,0}) || entered) && tryCreate())
        { newMapId[0] = '\0'; newMapErr.clear(); ImGui::CloseCurrentPopup(); }
        ImGui::SameLine();
        if (ImGui::Button("Annulla", {120,0}))
        { newMapId[0] = '\0'; newMapErr.clear(); ImGui::CloseCurrentPopup(); }
        ImGui::EndPopup();
    }
}

// ── drawBoxList ───────────────────────────────────────────────────────────────
void MapEditor::drawBoxList(float /*panelW*/, float /*panelH*/)
{
    // ── DIMENSIONI DELLA MAPPA, SEMPRE IN VISTA (doc 50 M1) ──────────────
    // Richiesta dell'utente: *"mi serve un modo per sapere le dimensioni tipo della
    // mappa in maniera facile e chiara"*. Le figure di scala sono un surrogato:
    // dicono "circa due metri", non "questo corridoio è 3,40".
    // Prova a carico del fatto che serviva: io stesso ho sbagliato DUE VOLTE la
    // dimensione di Training Ground (50 × 40, poi 154,9 × 91,9) prima di arrivare a
    // 71,3 × 92,4 leggendo i limiti del navmesh. Era un dato che nessuno mostrava.
    {
        // Sui box a mano E su quelli generati dalle primitive: l'ingombro vero è
        // quello che il gioco vedrà, non quello di ciò che si è disegnato a mano.
        bool any = false;
        float minX = 0, maxX = 0, minY = 0, maxY = 0, minZ = 0, maxZ = 0;
        // La ROTAZIONE conta. Un box ruotato di 90° scambia larghezza e profondità,
        // e ignorarlo dà un ingombro grottescamente sbagliato: su Training Ground —
        // che ha due passerelle da 90 m ruotate — ignorare `ry` dà 154,9 × 91,9
        // invece di 71,3 × 92,4. È l'errore che ho fatto io la prima volta, quindi
        // qui la formula è quella completa dell'AABB di un box ruotato attorno a Y.
        auto acc = [&](float x, float y, float z, float ry,
                       float sx, float sy, float sz) {
            const float a = ry * 3.14159265f / 180.0f;
            const float c = std::fabs(std::cos(a)), s = std::fabs(std::sin(a));
            const float hx = (sx * 0.5f) * c + (sz * 0.5f) * s;
            const float hz = (sx * 0.5f) * s + (sz * 0.5f) * c;
            const float hy = sy * 0.5f;
            if (!any) { minX = x-hx; maxX = x+hx; minY = y-hy; maxY = y+hy;
                        minZ = z-hz; maxZ = z+hz; any = true; return; }
            minX = std::min(minX, x-hx); maxX = std::max(maxX, x+hx);
            minY = std::min(minY, y-hy); maxY = std::max(maxY, y+hy);
            minZ = std::min(minZ, z-hz); maxZ = std::max(maxZ, z+hz);
        };
        for (const auto& b : m_boxes)         acc(b.x, b.y, b.z, b.ry, b.sx, b.sy, b.sz);
        for (const auto& g : m_structPreview) acc(g.x, g.y, g.z, g.ry, g.sx, g.sy, g.sz);

        if (any)
        {
            ImGui::TextColored(ImVec4(0.55f, 0.85f, 1.0f, 1.0f), "%.1f x %.1f m",
                               maxX - minX, maxZ - minZ);
            ImGui::SameLine();
            ImGui::TextDisabled("quota %.1f -> %.1f", minY, maxY);
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip(
                    "Ingombro REALE della mappa: box a mano piu' quelli generati\n"
                    "dalle primitive. Riferimenti: Training Ground e' 71 x 92 m,\n"
                    "la mappa grande pianificata 300 x 200.\n"
                    "X da %.1f a %.1f, Z da %.1f a %.1f.",
                    minX, maxX, minZ, maxZ);
        }
        else ImGui::TextDisabled("mappa vuota");
    }
    ImGui::Separator();

    // La SALUTE TATTICA non sta più qui: era una delle tre presentazioni diverse
    // dello stesso "cosa non va", e rubava spazio all'elenco degli elementi anche
    // quando la mappa era a posto. Ora c'è una finestra sola — "Problemi" nella
    // barra — raggruppata per tipo, con navmesh e gate dei dati insieme.

    ImGui::TextDisabled("Box (%d)", (int)m_boxes.size());
    ImGui::Separator();

    const char* typeIcons[] = {"[F]","[W]","[P]","[C]","[D]"};
    auto typeIcon = [&](const char* t) -> const char* {
        if (std::strcmp(t,"floor")  == 0) return typeIcons[0];
        if (std::strcmp(t,"wall")   == 0) return typeIcons[1];
        if (std::strcmp(t,"platform")==0) return typeIcons[2];
        if (std::strcmp(t,"cover")  == 0) return typeIcons[3];
        return typeIcons[4];
    };

    // ── Esito della VERIFICA NAVMESH ──────────────────────────────────────
    // Sopra a tutto: se il navmesh non collega qualcosa, è la prima cosa da
    // sapere — e ogni voce è cliccabile, così si passa da "c'è un problema" a
    // "guarda questo", che è la differenza fra un avviso e uno strumento.
    if (m_navBuilt)
    {
        const int isole = m_navReport.components > 0 ? m_navReport.components - 1 : 0;
        const bool bad = isole > 0 || !m_navReport.badPositions.empty()
                       || !m_navReport.badPosts.empty();
        ImGui::PushStyleColor(ImGuiCol_Text,
            bad ? ImVec4(0.95f, 0.50f, 0.40f, 1.0f) : ImVec4(0.50f, 0.85f, 0.55f, 1.0f));
        char nh[96];
        std::snprintf(nh, sizeof(nh), "Navmesh: %s###navhdr",
                      m_navStale ? "da ri-verificare"
                                 : (bad ? "ci sono zone scollegate" : "tutto connesso"));
        if (bad) ImGui::SetNextItemOpen(true, ImGuiCond_Once);
        const bool open = ImGui::CollapsingHeader(nh);
        ImGui::PopStyleColor();
        if (open)
        {
            ImGui::TextDisabled("%d poligoni · %d isole · costruito in %.2f s",
                                m_navReport.polys, isole, m_navReport.buildSeconds);
            if (m_navReport.islandTris > 0)
                ImGui::TextDisabled("%d triangoli fuori dalla zona dello spawn (in rosso)",
                                    m_navReport.islandTris);
            for (int i : m_navReport.badPosts)
            {
                if (i < 0 || i >= (int)m_posts.size()) continue;
                char b[128];
                std::snprintf(b, sizeof(b), "! post '%s' non raggiungibile##npp%d",
                              m_posts[i].label, i);
                if (ImGui::Selectable(b)) setSelection(-10 - i, false);
            }
            for (int i : m_navReport.badPositions)
            {
                if (i < 0 || i >= (int)m_positions.size()) continue;
                char b[128];
                std::snprintf(b, sizeof(b), "! posizione %d [%s] non raggiungibile##npz%d",
                              i + 1, m_positions[i].role.c_str(), i);
                if (ImGui::Selectable(b)) setSelection(-1000 - i, false);
            }
            if (!bad) ImGui::TextDisabled("nessun elemento isolato");
        }
    }

    // ── Le STRUTTURE per prime (ADR-053) ──────────────────────────────────
    // Stanno sopra i box perché sono l'unità di lavoro: una scala da 15 gradini è
    // UNA riga qui, non quindici nella lista dei box.
    if (!m_structures.empty())
    {
        ImGui::SetNextItemOpen(true, ImGuiCond_Once);
        char shdr[64];
        std::snprintf(shdr, sizeof(shdr), "Strutture (%d)###structs", (int)m_structures.size());
        if (ImGui::CollapsingHeader(shdr))
        {
            for (int i = 0; i < (int)m_structures.size(); ++i)
            {
                const auto& s = m_structures[i];
                char sb[160];
                const char* nm = s.label.empty() ? "(senza nome)" : s.label.c_str();
                // Il conto dei box generati è l'informazione che serve davvero:
                // dice quanto costa quella riga.
                std::vector<mini::MapGeometryBox> tmpb;
                expandStructureAt(i, tmpb);
                // Il segno `[+]` distingue una COMPOSITA anche in lista: due righe
                // con lo stesso nome e conti diversi, senza, sono un enigma.
                const bool composite = !s.type.empty()
                    && m_prefabReg.getStructureType(s.type)
                    && mini::mapstructures::isAssembly(*m_prefabReg.getStructureType(s.type));
                // L'asterisco: questa copia è stata modificata solo qui e non segue
                // più il tipo. È l'unico modo per distinguere, in una fila di quattro
                // "Tactic Bunker", quello su cui si è messo mano.
                std::snprintf(sb, sizeof(sb), "%s%s%s  [%s, %d box]##st%d",
                              composite ? "[+] " : "", nm,
                              s.isModifiedInstance() ? " *" : "",
                              mini::mapstructures::kindName(s.kind), (int)tmpb.size(), i);
                const int code = -6000 - i;
                const bool ssel = (m_selStruct == i)
                    || std::find(m_multiSel.begin(), m_multiSel.end(), code) != m_multiSel.end();
                if (ImGui::Selectable(sb, ssel))
                    setSelection(code, ImGui::GetIO().KeyCtrl);
            }
        }
    }

    // ── BOX RAGGRUPPATE PER TIPO (doc 49 / richiesta utente) ─────────────
    // Su Training Ground sono 167 in un elenco piatto, e su una mappa 300 × 200
    // saranno molte di più: scorrerlo per trovare un muro non è lavoro, è attrito.
    // Le categorie sono le stesse dei filtri di Vista e dei tipi di `BoxType` —
    // un secondo vocabolario per dire le stesse cose farebbe solo confusione.
    {
        // Il filtro per nome prima di tutto: quando sai come si chiama, la strada
        // più corta non è la categoria giusta, è scriverlo.
        ImGui::SetNextItemWidth(-1.0f);
        ImGui::InputTextWithHint("##boxfilter", "filtra per nome...",
                                 m_listFilter, sizeof(m_listFilter));
        const bool filtering = (m_listFilter[0] != '\0');
        auto matches = [&](const char* name) {
            if (!filtering) return true;
            std::string a = name, b = m_listFilter;
            std::transform(a.begin(), a.end(), a.begin(), ::tolower);
            std::transform(b.begin(), b.end(), b.begin(), ::tolower);
            return a.find(b) != std::string::npos;
        };

        struct Cat { const char* label; mini::BoxType type; };
        static const Cat cats[] = {
            { "Pavimenti",   mini::BoxType::Floor      },
            { "Muri",        mini::BoxType::Wall       },
            { "Piattaforme", mini::BoxType::Platform   },
            { "Coperture",   mini::BoxType::Cover      },
            { "Decorazioni", mini::BoxType::Decoration },
        };

        for (const auto& cat : cats)
        {
            // Gli indici PRIMA dell'intestazione: il conteggio dev'essere quello
            // filtrato, o l'intestazione promette righe che non ci sono.
            std::vector<int> members;
            for (int i = 0; i < (int)m_boxes.size(); ++i)
            {
                if (mini::parseBoxType(m_boxes[i].type) != cat.type) continue;
                const char* nm = (m_boxes[i].label[0] != '\0') ? m_boxes[i].label : "";
                if (!matches(nm)) continue;
                members.push_back(i);
            }
            if (members.empty()) continue;

            // Col filtro attivo le categorie si aprono da sole: cercare e poi dover
            // anche aprire il cassetto giusto vanifica la ricerca.
            if (filtering) ImGui::SetNextItemOpen(true, ImGuiCond_Always);
            char hdr[64];
            std::snprintf(hdr, sizeof(hdr), "%s (%d)###cat%d",
                          cat.label, (int)members.size(), (int)cat.type);
            if (!ImGui::CollapsingHeader(hdr)) continue;

            for (int i : members)
            {
                const auto& b = m_boxes[i];
                char buf[128];
                const char* name = (b.label[0] != '\0') ? b.label : "(nessun nome)";
                std::snprintf(buf, sizeof(buf), "  %s %s##box%d", typeIcon(b.type), name, i);
                const bool sel = (i == m_selBox)
                    || std::find(m_multiSel.begin(), m_multiSel.end(), i) != m_multiSel.end();
                if (ImGui::Selectable(buf, sel))
                    setSelection(i, ImGui::GetIO().KeyCtrl);
                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("tipo: %s\npos: (%.1f, %.1f, %.1f)\ndim: %.1fx%.1fx%.1f",
                                      b.type, b.x, b.y, b.z, b.sx, b.sy, b.sz);
                }
            }
        }
    }

    ImGui::Separator();
    // Spawn points
    ImGui::TextDisabled("Spawn");
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4f, 0.7f, 1.0f, 1.0f));
    bool spawnT1 = (m_selBox == -2);
    if (ImGui::Selectable("[T1] Spawn Alleati", spawnT1)) { m_selBox = -2; updateViewport(); }
    ImGui::PopStyleColor();

    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.4f, 0.4f, 1.0f));
    bool spawnT2 = (m_selBox == -3);
    if (ImGui::Selectable("[T2] Spawn Nemici", spawnT2)) { m_selBox = -3; updateViewport(); }
    ImGui::PopStyleColor();

    // Punti multi-spawn AGGIUNTIVI: le AI si distribuiscono su questi + lo spawn
    // principale della fazione. Selezionabili dal viewport, spostabili col gizmo.
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.45f, 0.75f, 1.0f, 1.0f));
    for (int i = 0; i < (int)m_spawnPoints1.size(); ++i)
    {
        char lbl[48]; std::snprintf(lbl, sizeof(lbl), "  [T1] punto #%d##sp1_%d", i, i);
        if (ImGui::Selectable(lbl, m_selBox == -3000 - i)) { m_selBox = -3000 - i; updateViewport(); }
    }
    ImGui::PopStyleColor();
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.55f, 0.30f, 1.0f));
    for (int i = 0; i < (int)m_spawnPoints2.size(); ++i)
    {
        char lbl[48]; std::snprintf(lbl, sizeof(lbl), "  [T2] punto #%d##sp2_%d", i, i);
        if (ImGui::Selectable(lbl, m_selBox == -3100 - i)) { m_selBox = -3100 - i; updateViewport(); }
    }
    ImGui::PopStyleColor();
    if (ImGui::SmallButton("+ punto T1"))
    { const glm::vec3 fp = m_viewport.groundFocusPoint();
      m_spawnPoints1.push_back({fp.x, 0.86f, fp.z});
      m_selBox = -3000 - ((int)m_spawnPoints1.size()-1); m_dirty = true; updateViewport(); }
    ImGui::SameLine();
    if (ImGui::SmallButton("+ punto T2"))
    { const glm::vec3 fp = m_viewport.groundFocusPoint();
      m_spawnPoints2.push_back({fp.x, 0.86f, fp.z});
      m_selBox = -3100 - ((int)m_spawnPoints2.size()-1); m_dirty = true; updateViewport(); }
    ImGui::SameLine();
    if (ImGui::SmallButton("-##sp"))
    {
        if (m_selBox <= -3000 && m_selBox > -3100)
        { int i = -3000 - m_selBox; if (i >= 0 && i < (int)m_spawnPoints1.size())
          { m_spawnPoints1.erase(m_spawnPoints1.begin()+i); m_selBox = -1; m_dirty = true; updateViewport(); } }
        else if (m_selBox <= -3100 && m_selBox > -3200)
        { int i = -3100 - m_selBox; if (i >= 0 && i < (int)m_spawnPoints2.size())
          { m_spawnPoints2.erase(m_spawnPoints2.begin()+i); m_selBox = -1; m_dirty = true; updateViewport(); } }
    }
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Rimuove il punto multi-spawn selezionato.\n"
                          "Le AI si distribuiscono sui punti + lo spawn principale.");

    // ── Comandante strategico (ADR-041): uno per mappa ───────────────────
    ImGui::Separator();
    ImGui::TextDisabled("Comando");
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.75f, 0.45f, 0.95f, 1.0f));
    if (m_commander.exists)
    {
        bool selC = (m_selBox == kSelCommander);
        char lbl[96];
        std::snprintf(lbl, sizeof(lbl), "[CMD] %s##cmd",
                      m_commander.unit.empty() ? "(nessuna classe)"
                                               : m_commander.unit.c_str());
        if (ImGui::Selectable(lbl, selC)) { m_selBox = kSelCommander; updateViewport(); }
    }
    else if (ImGui::SmallButton("+ Comandante (Droide Tattico)"))
    {
        m_commander.exists = true;
        if (!m_commanderIds.empty()) m_commander.unit = m_commanderIds.front();
        const glm::vec3 fp = m_viewport.groundFocusPoint();
        m_commander.x = fp.x; m_commander.z = fp.z;
        m_commander.leashRadius = 6.0f;
        m_selBox = kSelCommander;
        m_dirty = true; updateViewport();
    }
    ImGui::PopStyleColor();

    // ── Command post ─────────────────────────────────────────────────────
    ImGui::Separator();
    ImGui::TextDisabled("Command Post (%d)", (int)m_posts.size());
    for (int i = 0; i < (int)m_posts.size(); ++i)
    {
        const auto& p = m_posts[i];
        ImVec4 col = (p.team == 1) ? ImVec4(0.4f,0.7f,1.0f,1.0f)
                   : (p.team == 2) ? ImVec4(1.0f,0.4f,0.4f,1.0f)
                                   : ImVec4(0.8f,0.8f,0.8f,1.0f);
        ImGui::PushStyleColor(ImGuiCol_Text, col);
        char lbl[96];
        std::snprintf(lbl, sizeof(lbl), "[CP] %s##cp%d", p.label, i);
        bool sel = (m_selBox == -10 - i);
        if (ImGui::Selectable(lbl, sel)) { m_selBox = -10 - i; updateViewport(); }
        ImGui::PopStyleColor();
    }
    if (ImGui::SmallButton("+ Post"))
    {
        PostEntry p;
        std::snprintf(p.label, sizeof(p.label), "Post %d", (int)m_posts.size() + 1);
        const glm::vec3 fp = m_viewport.groundFocusPoint();
        p.x = fp.x; p.z = fp.z;
        m_posts.push_back(p);
        m_selBox = -10 - ((int)m_posts.size() - 1);
        m_dirty = true;
        updateViewport();
    }
    ImGui::SameLine();
    if (ImGui::SmallButton("- Rimuovi##cp") && m_selBox <= -10 && m_selBox > -100)
    {
        int i = -10 - m_selBox;
        if (i >= 0 && i < (int)m_posts.size())
        {
            m_posts.erase(m_posts.begin() + i);
            m_selBox = -1;
            m_dirty = true;
            updateViewport();
        }
    }

    // ── Bersagli strategici (doc 25, DestroyTarget) ──────────────────────
    ImGui::Separator();
    ImGui::TextDisabled("Bersagli strategici (%d)", (int)m_targets.size());
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.9f, 0.6f, 0.2f, 1.0f));
    for (int i = 0; i < (int)m_targets.size(); ++i)
    {
        char lbl[96];
        // [COM] rende leggibile a colpo d'occhio quali strutture alimentano la
        // rete di comunicazione (doc 34), e di quale fazione.
        std::snprintf(lbl, sizeof(lbl), "%s %s (T%d)##tg%d",
                      m_targets[i].role == 1 ? "[COM]"
                    : m_targets[i].role == 2 ? "[CTRL]" : "[BG]",
                      m_targets[i].label, m_targets[i].team, i);
        bool sel = (m_selBox == -500 - i);
        if (ImGui::Selectable(lbl, sel)) { m_selBox = -500 - i; updateViewport(); }
    }
    ImGui::PopStyleColor();
    if (ImGui::SmallButton("+ Bersaglio"))
    {
        TargetEntry t;
        std::snprintf(t.label, sizeof(t.label), "Bersaglio %d", (int)m_targets.size() + 1);
        const glm::vec3 fp = m_viewport.groundFocusPoint();
        t.x = fp.x; t.z = fp.z;
        m_targets.push_back(t);
        m_selBox = -500 - ((int)m_targets.size() - 1);
        m_dirty = true;
        updateViewport();
    }
    ImGui::SameLine();
    if (ImGui::SmallButton("- Rimuovi##tg") && m_selBox <= -500)
    {
        int i = -500 - m_selBox;
        if (i >= 0 && i < (int)m_targets.size())
        {
            m_targets.erase(m_targets.begin() + i);
            m_selBox = -1;
            m_dirty = true;
            updateViewport();
        }
    }

    // ── Map Metadata (15_MapMetadata) ────────────────────────────────────
    ImGui::Separator();
    ImGui::TextDisabled("Metadata AI");

    // Riepilogo VISUALE VERTICALE (KI #83): quante posizioni, pur avendo altre quote
    // in giro, non ne battono NESSUNA. È lo stato di salute verticale della mappa a
    // colpo d'occhio — prima lo si scopriva solo giocando. Le cieche hanno un rombo
    // rosso sopra nel viewport; qui si vede subito se sono un caso isolato o la norma.
    {
        int blind = 0, withPairs = 0;
        for (size_t i = 0; i < m_vertPairs.size(); ++i)
            if (m_vertPairs[i] > 0)
            { ++withPairs; if (i < m_vertSight.size() && m_vertSight[i] == 0) ++blind; }
        if (withPairs > 0)
        {
            if (blind > 0)
                ImGui::TextColored(ImVec4(0.95f, 0.55f, 0.35f, 1.0f),
                                   "Verticale: %d/%d cieche", blind, withPairs);
            else
                ImGui::TextColored(ImVec4(0.45f, 0.85f, 0.50f, 1.0f),
                                   "Verticale: tutte battono altre quote");
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Posizioni che NON vedono nessuna quota diversa.\n"
                                  "Rombo rosso nel viewport. Rimedi: avvicinare al bordo,\n"
                                  "abbassare i parapetti, o accettare che il piano alto\n"
                                  "domini solo le lunghe distanze.");
        }
    }

    // ── PREFAB (ADR-048) ─────────────────────────────────────────────────
    // Piazzare un prefab porta con sé collisione E posizioni tattiche già pensate:
    // è il modo di autorare mappe profonde senza piazzare mille posizioni a mano.
    ImGui::Separator();
    ImGui::TextDisabled("Prefab (%d)", (int)m_prefabInsts.size());

    // CREAZIONE da zona: si costruisce il pezzo nella mappa e lo si promuove ad asset.
    // Senza questo si potevano solo piazzare prefab scritti a mano nel JSON.
    {
        static std::string prefabErr;
        if (ImGui::SmallButton("Crea prefab da zona..."))
        {
            prefabErr.clear();
            // Centro CONGELATO all'apertura: se seguisse la telecamera, la selezione
            // cambierebbe sotto gli occhi mentre si scrive il nome.
            const glm::vec3 fp = m_viewport.groundFocusPoint();
            m_prefabZoneX = fp.x; m_prefabZoneZ = fp.z;
            m_prefabZoneMode = true; m_prefabPickManual = false;
            m_prefabPickBoxes.clear(); m_prefabPickPositions.clear();
            updateViewport();
        }
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Prende box e posizioni tattiche entro un raggio dal punto\n"
                              "inquadrato e li salva come asset riusabile.");
        // Pannello INLINE, non un popup: un popup ImGui si chiude al primo click fuori,
        // quindi cliccare nel viewport per rifinire la selezione lo faceva sparire e il
        // Shift+click arrivava a modalità già spenta (segnalato dall'utente 2026-08-02).
        // Un'azione che richiede di cliccare nella SCENA non può vivere in un popup.
        if (m_prefabZoneMode)
        {
            ImGui::Separator();
            ImGui::TextDisabled("Zona in CIANO nel viewport; gli elementi presi hanno un rombo.");
            ImGui::TextDisabled("Shift/Ctrl+click su un elemento per aggiungerlo o toglierlo.");
            std::vector<int> selB, selP;
            prefabZoneCollect(selB, selP);
            ImGui::TextColored({0.35f,0.95f,1.0f,1.0f}, "Selezionati: %d box, %d posizioni%s",
                               (int)selB.size(), (int)selP.size(),
                               m_prefabPickManual ? "  (ritoccato a mano)" : "");
            ImGui::SetNextItemWidth(180.0f);
            ImGui::InputText("Nome", m_newPrefabId, sizeof(m_newPrefabId));
            // Il raggio agisce solo finché non si ritocca a mano: dopo comanda la lista.
            ImGui::BeginDisabled(m_prefabPickManual);
            if (editor::ui::sliderRow("Raggio", m_newPrefabRadius, 1.f, 40.f, 0.5f, "%.1f m"))
                updateViewport();
            ImGui::EndDisabled();
            if (m_prefabPickManual && ImGui::SmallButton("Torna al raggio"))
            { m_prefabPickManual = false; updateViewport(); }
            ImGui::Checkbox("Sostituisci in mappa con un'istanza", &m_newPrefabConsume);
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Consigliato: evita di avere due copie (una fissa nella\n"
                                  "mappa e una dal prefab) che poi divergono.");
            if (!prefabErr.empty())
                ImGui::TextColored({0.95f,0.35f,0.30f,1.0f}, "%s", prefabErr.c_str());
            if (ImGui::Button("Crea", {110,0}))
                savePrefabFromZone(prefabErr);   // esce da sé dalla modalità se riesce
            ImGui::SameLine();
            if (ImGui::Button("Annulla", {110,0}))
            {
                prefabErr.clear();
                m_prefabZoneMode = false; m_prefabPickManual = false;
                m_prefabPickBoxes.clear(); m_prefabPickPositions.clear();
                updateViewport();
            }
            ImGui::Separator();
        }
    }

    if (m_prefabIds.empty())
        ImGui::TextDisabled("nessun prefab in data/prefabs/");
    else
    {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.72f, 0.62f, 0.95f, 1.0f));
        for (int i = 0; i < (int)m_prefabInsts.size(); ++i)
        {
            const auto& p = m_prefabInsts[i];
            const bool broken = (m_prefabReg.getPrefab(p.id) == nullptr);
            char lbl[96];
            std::snprintf(lbl, sizeof(lbl), "%s%s##pfi%d",
                          broken ? "! " : "", p.id.c_str(), i);
            if (ImGui::Selectable(lbl, m_selBox == -4000 - i))
            { m_selBox = -4000 - i; updateViewport(); }
            if (broken && ImGui::IsItemHovered())
                ImGui::SetTooltip("Prefab '%s' non trovato in data/prefabs/", p.id.c_str());
        }
        ImGui::PopStyleColor();

        if (m_prefabPick >= (int)m_prefabIds.size()) m_prefabPick = 0;
        ImGui::SetNextItemWidth(150.0f);
        if (ImGui::BeginCombo("##pfsel", m_prefabIds[m_prefabPick].c_str()))
        {
            for (int k = 0; k < (int)m_prefabIds.size(); ++k)
                if (ImGui::Selectable(m_prefabIds[k].c_str(), m_prefabPick == k))
                    m_prefabPick = k;
            ImGui::EndCombo();
        }
        ImGui::SameLine();
        if (ImGui::SmallButton("+ Piazza"))
        {
            const glm::vec3 fp = m_viewport.groundFocusPoint();
            PrefabInstEntry e;
            e.id = m_prefabIds[m_prefabPick];
            e.x = fp.x; e.y = 0.0f; e.z = fp.z;
            m_prefabInsts.push_back(e);
            m_selBox = -4000 - ((int)m_prefabInsts.size() - 1);
            m_dirty = true; updateViewport();
        }
        ImGui::SameLine();
        if (ImGui::SmallButton("-##pfi") && m_selBox <= -4000)
        {
            const int i = -4000 - m_selBox;
            if (i >= 0 && i < (int)m_prefabInsts.size())
            { m_prefabInsts.erase(m_prefabInsts.begin() + i); m_selBox = -1;
              m_dirty = true; updateViewport(); }
        }

        // ── ELIMINA L'ASSET (non l'istanza) ───────────────────────────────
        // Su una riga PROPRIA sotto il menu a tendina, non stretto fra "+" e "-":
        // quelli aggiungono e tolgono un'ISTANZA in questa mappa, questo cancella
        // l'asset dal disco. Metterli in fila accostava due gesti con conseguenze
        // molto diverse. Agisce sul prefab scelto nel menu qui sopra.
        if (!m_prefabIds.empty() && !m_prefabIds[m_prefabPick].empty())
        {
            if (ImGui::SmallButton("Elimina asset...")) ImGui::OpenPopup("##delprefab");
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Cancella data/prefabs/%s.json dal disco.",
                                  m_prefabIds[m_prefabPick].c_str());
            if (ImGui::BeginPopup("##delprefab"))
            {
                const std::string pid = m_prefabIds[m_prefabPick];
                int used = 0;
                for (const auto& pi : m_prefabInsts) if (pi.id == pid) ++used;
                ImGui::Text("Eliminare l'asset '%s'?", pid.c_str());
                if (used > 0)
                    ImGui::TextColored({1.0f, 0.55f, 0.45f, 1.0f},
                                       "%d istanze in questa mappa resterebbero rotte.", used);
                else
                    ImGui::TextDisabled("Non è usato in questa mappa.");
                ImGui::Separator();
                if (ImGui::Button("Elimina", {110, 0}))
                {
                    std::error_code ec;
                    fs::remove(fs::path(getDataDir()) / "prefabs" / (pid + ".json"), ec);
                    // Ricarica AUTOMATICA: dal 2026-08-05 `loadPrefabs` azzera il
                    // proprio contenitore, quindi rileggere il disco è autoritativo
                    // e il prefab sparisce subito dal menu — niente riavvio, e
                    // nessun pulsante "ricarica" da ricordarsi di premere.
                    reloadPrefabAssets();
                    ImGui::CloseCurrentPopup();
                }
                ImGui::SameLine();
                if (ImGui::Button("Annulla", {110, 0})) ImGui::CloseCurrentPopup();
                ImGui::EndPopup();
            }
        }
    }

    ImGui::Separator();
    // ── POSIZIONI TATTICHE RAGGRUPPATE PER RUOLO (ADR-030) ───────────────
    // Erano 169 in un elenco piatto, distinguibili solo dal ruolo nell'etichetta:
    // per trovare "le vantage" bisognava leggerle tutte. I ruoli si ricavano dai
    // DATI, non da un elenco fisso, così un ruolo nuovo compare da solo invece di
    // finire in un "altro" che nessuno guarda.
    {
        std::vector<std::string> roles;
        for (const auto& p : m_positions)
            if (std::find(roles.begin(), roles.end(), p.role) == roles.end())
                roles.push_back(p.role);
        std::sort(roles.begin(), roles.end());

        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.3f, 0.9f, 0.75f, 1.0f));
        for (const auto& role : roles)
        {
            std::vector<int> members;
            int blind = 0;
            for (int i = 0; i < (int)m_positions.size(); ++i)
            {
                if (m_positions[i].role != role) continue;
                members.push_back(i);
                if (i < (int)m_vertPairs.size() && m_vertPairs[i] > 0
                    && i < (int)m_vertSight.size() && m_vertSight[i] == 0) ++blind;
            }
            if (members.empty()) continue;

            // Il numero di posizioni CIECHE nell'intestazione: si vede da fuori
            // quale gruppo ha un problema, senza aprirli tutti per scoprirlo.
            char hdr[96];
            if (blind > 0)
                std::snprintf(hdr, sizeof(hdr), "%s (%d)  ! %d cieche###rl_%s",
                              role.c_str(), (int)members.size(), blind, role.c_str());
            else
                std::snprintf(hdr, sizeof(hdr), "%s (%d)###rl_%s",
                              role.c_str(), (int)members.size(), role.c_str());
            if (!ImGui::CollapsingHeader(hdr)) continue;

            for (int i : members)
            {
                const bool blindPos = i < (int)m_vertPairs.size() && m_vertPairs[i] > 0
                                   && i < (int)m_vertSight.size() && m_vertSight[i] == 0;
                char lbl[80];
                std::snprintf(lbl, sizeof(lbl), "  %s#%d##tpos%d",
                              blindPos ? "! " : "", i + 1, i);
                if (ImGui::Selectable(lbl, m_selBox == -1000 - i))
                { m_selBox = -1000 - i; updateViewport(); }
            }
        }
        ImGui::PopStyleColor();
    }
    if (ImGui::SmallButton("+ Posizione"))
    {
        const glm::vec3 fp = m_viewport.groundFocusPoint();
        PositionEntry e; e.x = fp.x; e.z = fp.z;
        m_positions.push_back(e);
        m_selBox = -1000 - ((int)m_positions.size() - 1);
        m_dirty = true; updateViewport();
    }
    ImGui::SameLine();
    if (ImGui::SmallButton("-##tpos") && m_selBox <= -1000)
    {
        int i = -1000 - m_selBox;
        if (i >= 0 && i < (int)m_positions.size())
        { m_positions.erase(m_positions.begin() + i); m_selBox = -1; m_dirty = true; updateViewport(); }
    }

    // Settori / Combat Areas (ADR-034): il livello su cui ragiona il comandante.
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.7f, 0.55f, 0.95f, 1.0f));
    for (int i = 0; i < (int)m_sectors.size(); ++i)
    {
        char lbl[72];
        std::snprintf(lbl, sizeof(lbl), "[SET] %s##sec%d", m_sectors[i].label.c_str(), i);
        if (ImGui::Selectable(lbl, m_selBox == -2000 - i))
        { m_selBox = -2000 - i; updateViewport(); }
    }
    ImGui::PopStyleColor();
    if (ImGui::SmallButton("+ Settore"))
    {
        const glm::vec3 fp = m_viewport.groundFocusPoint();
        SectorEntry s; s.x = fp.x; s.z = fp.z;
        m_sectors.push_back(s);
        m_selBox = -2000 - ((int)m_sectors.size() - 1);
        m_dirty = true; updateViewport();
    }
    ImGui::SameLine();
    if (ImGui::SmallButton("-##sec") && m_selBox <= -2000)
    {
        int i = -2000 - m_selBox;
        if (i >= 0 && i < (int)m_sectors.size())
        { m_sectors.erase(m_sectors.begin() + i); m_selBox = -1; m_dirty = true; updateViewport(); }
    }

    // Danger zone
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.95f, 0.55f, 0.2f, 1.0f));
    for (int i = 0; i < (int)m_dangers.size(); ++i)
    {
        char lbl[48];
        std::snprintf(lbl, sizeof(lbl), "[DZ] Pericolo %d##dz%d", i + 1, i);
        if (ImGui::Selectable(lbl, m_selBox == -200 - i))
        { m_selBox = -200 - i; updateViewport(); }
    }
    ImGui::PopStyleColor();
    if (ImGui::SmallButton("+ Pericolo"))
    {
        const glm::vec3 fp = m_viewport.groundFocusPoint();
        DangerEntry d; d.x = fp.x; d.z = fp.z;
        m_dangers.push_back(d);
        m_selBox = -200 - ((int)m_dangers.size() - 1);
        m_dirty = true; updateViewport();
    }
    ImGui::SameLine();
    if (ImGui::SmallButton("-##dz") && m_selBox <= -200 && m_selBox > -300)
    {
        int i = -200 - m_selBox;
        if (i >= 0 && i < (int)m_dangers.size())
        { m_dangers.erase(m_dangers.begin() + i); m_selBox = -1; m_dirty = true; updateViewport(); }
    }

    // Patrol route
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.75f, 0.5f, 1.0f, 1.0f));
    for (int i = 0; i < (int)m_routes.size(); ++i)
    {
        char lbl[64];
        std::snprintf(lbl, sizeof(lbl), "[PR] %s (%d pt)##pr%d",
                      m_routes[i].id, (int)m_routes[i].points.size(), i);
        if (ImGui::Selectable(lbl, m_selBox == -300 - i))
        { m_selBox = -300 - i; m_selRoutePt = 0; updateViewport(); }
    }
    ImGui::PopStyleColor();
    if (ImGui::SmallButton("+ Percorso"))
    {
        RouteEntry r;
        std::snprintf(r.id, sizeof(r.id), "route_%d", (int)m_routes.size() + 1);
        const glm::vec3 fp = m_viewport.groundFocusPoint();
        r.points.push_back({fp.x,        0.5f, fp.z});
        r.points.push_back({fp.x + 4.0f, 0.5f, fp.z});
        m_routes.push_back(std::move(r));
        m_selBox = -300 - ((int)m_routes.size() - 1);
        m_selRoutePt = 0;
        m_dirty = true; updateViewport();
    }
    ImGui::SameLine();
    if (ImGui::SmallButton("-##pr") && m_selBox <= -300 && m_selBox > -400)
    {
        int i = -300 - m_selBox;
        if (i >= 0 && i < (int)m_routes.size())
        { m_routes.erase(m_routes.begin() + i); m_selBox = -1; m_dirty = true; updateViewport(); }
    }

    // Spawn veicoli (19_Vehicles Fase B)
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.95f, 0.65f, 0.25f, 1.0f));
    for (int i = 0; i < (int)m_vehSpawns.size(); ++i)
    {
        char lbl[96];
        std::snprintf(lbl, sizeof(lbl), "[VS] %s##vs%d",
                      m_vehSpawns[i].vehicleId.empty()
                          ? "(scegli veicolo)" : m_vehSpawns[i].vehicleId.c_str(), i);
        if (ImGui::Selectable(lbl, m_selBox == -400 - i))
        { m_selBox = -400 - i; updateViewport(); }
    }
    ImGui::PopStyleColor();
    if (ImGui::SmallButton("+ Veicolo"))
    {
        VehicleSpawnEntry v;
        if (!m_vehicleIds.empty()) v.vehicleId = m_vehicleIds[0];
        const glm::vec3 fp = m_viewport.groundFocusPoint();
        v.x = fp.x; v.z = fp.z;
        m_vehSpawns.push_back(std::move(v));
        m_selBox = -400 - ((int)m_vehSpawns.size() - 1);
        m_dirty = true; updateViewport();
    }
    ImGui::SameLine();
    if (ImGui::SmallButton("-##vs") && m_selBox <= -400 && m_selBox > -500)
    {
        int i = -400 - m_selBox;
        if (i >= 0 && i < (int)m_vehSpawns.size())
        { m_vehSpawns.erase(m_vehSpawns.begin() + i); m_selBox = -1; m_dirty = true; updateViewport(); }
    }
}

// ── drawProperties ────────────────────────────────────────────────────────────
void MapEditor::drawProperties(float panelW, float /*panelH*/)
{
    float sliderW = panelW - 16.0f;

    // ── I LIMITI DEI CAMPI NON POSSONO ESSERE PIÙ PICCOLI DELLA MAPPA ─────
    // Erano **±60 m**, scelti quando le mappe erano piccole: su una 300 × 200 metà
    // degli elementi non si sarebbe potuta né digitare né trascinare, perché il
    // campo li avrebbe riportati dentro i 60 m. Un limite che taglia i dati invece
    // di proteggerli è peggio di nessun limite, e si sarebbe scoperto costruendo.
    // Restano dei limiti (un errore di digitazione non deve mandare un post a
    // 10 000 m), ma tarati sulla mappa più grande che ha senso costruire.
    constexpr float kPosLimit = 1000.0f;   // XZ: mappe fino a 2000 × 2000 m
    constexpr float kElevLo   = -50.0f;    // sotto il livello del mare
    constexpr float kElevHi   = 200.0f;    // torri e passerelle alte

    // ── SELEZIONE MULTIPLA (G3) ───────────────────────────────────────────
    // Non si mostrano i campi di un elemento solo: sarebbe ambiguo su quale dei
    // dieci si sta agendo. Si mostra COSA c'è dentro e cosa si può farci.
    if (m_multiSel.size() > 1)
    {
        int nBox = 0, nStruct = 0, nAltro = 0;
        for (int c : m_multiSel)
        {
            if      (c >= 0)      ++nBox;
            else if (c <= -6000)  ++nStruct;
            else                  ++nAltro;
        }
        ImGui::TextColored({1.0f, 0.78f, 0.35f, 1.0f},
                           "%d elementi selezionati", (int)m_multiSel.size());
        ImGui::Separator();
        if (nBox)    ImGui::BulletText("%d box", nBox);
        if (nStruct) ImGui::BulletText("%d strutture", nStruct);
        if (nAltro)  ImGui::BulletText("%d altri elementi", nAltro);
        ImGui::Separator();
        ImGui::TextDisabled("Sposta e ruota agiscono su TUTTI (la rotazione fa\n"
                            "orbitare attorno al baricentro). Elimina e Duplica\n"
                            "pure. La SCALA resta al singolo: su un gruppo misto\n"
                            "un raggio e un'altezza non si scalano allo stesso modo.");

        // ── DA INSIEME DI PARTI A OGGETTO UNICO ───────────────────────────
        // Il gesto inverso di "Esplodi": ciò che si è costruito qui a mano diventa
        // una composita di libreria, riusabile su tutta la mappa e correggibile in
        // un posto solo. Sta qui perché è qui che l'insieme esiste — la selezione
        // multipla È il gruppo, e non serve un'altra modalità per dirlo.
        if (nBox + nStruct >= 2)
        {
            if (ImGui::Button("Raggruppa in una composita..."))
            { m_groupOpen = true; m_groupError.clear(); }
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Crea un TIPO di struttura con questi elementi e li\n"
                                  "sostituisce con una sola istanza. Da li' in poi si\n"
                                  "correggono nell'editor strutture, una volta per tutte.");
            if (nAltro)
                ImGui::TextDisabled("i %d elementi non geometrici restano dove sono", nAltro);

            // Apertura e disegno nella STESSA funzione (doc 52 F4): la coppia
            // sparsa è ciò che aveva prodotto il modale invisibile del changelog 164.
            if (m_groupOpen && !ImGui::IsPopupOpen("Raggruppa in composita"))
                ImGui::OpenPopup("Raggruppa in composita");
            if (ImGui::BeginPopupModal("Raggruppa in composita", nullptr,
                                       ImGuiWindowFlags_AlwaysAutoResize))
            {
                ImGui::TextUnformatted("Nome della struttura:");
                ImGui::SetNextItemWidth(320.0f);
                ImGui::InputText("##grpname", m_groupName, sizeof(m_groupName));
                ImGui::TextDisabled("file: data/structures/%s.json",
                                    idFromLabel(m_groupName).c_str());
                ImGui::TextDisabled("%d elementi diventano le sue parti; in mappa\n"
                                    "restano come UNA struttura, al loro baricentro.",
                                    nBox + nStruct);
                ImGui::TextDisabled("Nasce NON verificata: aprila e verificala prima\n"
                                    "di riusarla altrove.");
                if (!m_groupError.empty())
                    ImGui::TextColored({0.95f, 0.35f, 0.30f, 1.0f}, "%s", m_groupError.c_str());
                ImGui::Separator();
                if (ImGui::Button("Raggruppa", {150, 0}))
                {
                    groupSelectionIntoType(m_groupName);
                    if (m_groupError.empty())
                    { m_groupOpen = false; ImGui::CloseCurrentPopup(); }
                }
                ImGui::SameLine();
                if (ImGui::Button("Annulla", {110, 0}))
                { m_groupOpen = false; ImGui::CloseCurrentPopup(); }
                ImGui::EndPopup();
            }
            else m_groupOpen = false;
        }

        // ── INGOMBRO DELLA SELEZIONE (doc 50 M2) ──────────────────────────
        // Larghezza x profondita' x altezza di TUTTO il gruppo: è il numero che si
        // confronta con le metriche normative (corridoio 2,4 · porta 1,8 × 2,8 ·
        // gigante 1,20 × 2,40) per sapere se ci si passa. Le sole misure del singolo
        // box non lo dicono quando gli elementi sono più d'uno.
        {
            bool any = false;
            float mnx=0, mxx=0, mny=0, mxy=0, mnz=0, mxz=0;
            for (int code : m_multiSel)
            {
                // Solo le box hanno un ingombro dichiarato; per gli altri elementi
                // vale il punto. Mescolarli darebbe una misura che non significa nulla.
                if (code >= 0 && code < (int)m_boxes.size())
                {
                    const auto& b = m_boxes[code];
                    const float hx=b.sx*0.5f, hy=b.sy*0.5f, hz=b.sz*0.5f;
                    if (!any) { mnx=b.x-hx; mxx=b.x+hx; mny=b.y-hy; mxy=b.y+hy;
                                mnz=b.z-hz; mxz=b.z+hz; any=true; continue; }
                    mnx=std::min(mnx,b.x-hx); mxx=std::max(mxx,b.x+hx);
                    mny=std::min(mny,b.y-hy); mxy=std::max(mxy,b.y+hy);
                    mnz=std::min(mnz,b.z-hz); mxz=std::max(mxz,b.z+hz);
                }
            }
            if (any)
            {
                ImGui::Separator();
                ImGui::TextColored({0.55f, 0.85f, 1.0f, 1.0f}, "Ingombro selezione");
                ImGui::Text("%.2f x %.2f x %.2f m", mxx-mnx, mxz-mnz, mxy-mny);
                // Il confronto con la metrica che conta, detto invece che lasciato
                // da calcolare a mente.
                const float minSpan = std::min(mxx-mnx, mxz-mnz);
                if (minSpan < mini::mapmetrics::CORRIDOR_MIN)
                    ImGui::TextColored({0.95f, 0.75f, 0.35f, 1.0f},
                        "lato minore %.2f m: sotto il corridoio (%.2f)",
                        minSpan, mini::mapmetrics::CORRIDOR_MIN);
            }
        }

        // ── RIGHELLO (doc 47 E6) ──────────────────────────────────────────
        // Con ESATTAMENTE due elementi selezionati si misura la distanza fra loro.
        // Non serve uno strumento a parte: la selezione multipla è già il gesto
        // giusto, e il confronto con le metriche normative dice subito se un
        // corridoio o una porta stanno nei limiti.
        if (m_multiSel.size() == 2)
        {
            glm::vec3 a, b;
            if (codePosition(m_multiSel[0], a) && codePosition(m_multiSel[1], b))
            {
                const glm::vec3 d = b - a;
                const float horiz = std::sqrt(d.x * d.x + d.z * d.z);
                ImGui::Separator();
                ImGui::TextColored({0.55f, 0.85f, 1.0f, 1.0f}, "Distanza");
                ImGui::Text("%.2f m in pianta, %.2f m totali", horiz, glm::length(d));
                ImGui::TextDisabled("ΔX %.2f   ΔY %.2f   ΔZ %.2f", d.x, d.y, d.z);
                if (std::fabs(d.y) > 0.01f)
                {
                    const float rise = std::fabs(d.y);
                    if (rise > mini::config::STEP_HEIGHT)
                        ImGui::TextColored({1.0f, 0.75f, 0.35f, 1.0f},
                            "Dislivello %.2f m: oltre lo scalino (%.2f). Servono %d gradini.",
                            rise, mini::config::STEP_HEIGHT,
                            mini::mapmetrics::stepsFor(rise));
                }
            }
        }
        return;
    }

    // ── PRIMITIVA PARAMETRICA selezionata (ADR-053) ───────────────────────
    if (m_selStruct >= 0 && m_selStruct < (int)m_structures.size())
    {
        auto& s = m_structures[m_selStruct];
        bool changed = false;
        using SK = mini::StructureKind;
        const bool isFlight = (s.kind == SK::Stair || s.kind == SK::Ramp
                            || s.kind == SK::Switchback);
        const bool isWallish = (s.kind == SK::Wall || s.kind == SK::Doorway);
        const bool hasOpening = (s.kind == SK::Doorway || s.kind == SK::Room);

        // ── UN'ISTANZA COMPOSITA NON È UNA PRIMITIVA ─────────────────────
        // Finora mostrava lo stesso pannello: alzate, pedate, larghezze di una
        // scala — su una torre di dodici parti. Leve che si muovono e geometria che
        // non cambia, cioè il modo più diretto per far perdere fiducia nei numeri.
        // Una composita ha ESATTAMENTE tre cose autorabili qui: nome, posa, e la
        // decisione se restare un oggetto solo o diventare parti.
        const auto* asmType = s.type.empty() ? nullptr
                                             : m_prefabReg.getStructureType(s.type);
        if (asmType && mini::mapstructures::isAssembly(*asmType))
        {
            const bool modified = s.isModifiedInstance();
            if (modified)
            {
                ImGui::TextColored({1.00f, 0.80f, 0.35f, 1.0f},
                                   "Struttura composita *  (modificata solo qui)");
                ImGui::TextDisabled("%s — %d parti proprie, diverse dal tipo",
                                    s.type.c_str(), (int)s.localParts.size());
                ImGui::TextDisabled("Il tipo non la governa piu': modificarlo non\n"
                                    "cambia questa. Le altre copie non sono toccate.");
            }
            else
            {
                ImGui::TextColored({0.70f, 0.85f, 0.45f, 1.0f}, "Struttura composita");
                ImGui::TextDisabled("%s — %d parti%s", s.type.c_str(),
                                    (int)asmType->parts.size(),
                                    asmType->verified ? "" : "  (NON verificata)");
                ImGui::TextDisabled("Le misure sono del TIPO: si cambiano nell'editor\n"
                                    "strutture, e cambiano in ogni copia sulla mappa.");
            }
            ImGui::Separator();

            char cnb[64];
            std::snprintf(cnb, sizeof(cnb), "%s", s.label.c_str());
            ImGui::SetNextItemWidth(sliderW);
            if (editor::ui::textRow("Nome", cnb, sizeof(cnb)))
            { s.label = cnb; changed = true; }

            ImGui::TextDisabled("Posizione");
            ImGui::SetNextItemWidth(sliderW);
            if (ImGui::DragFloat("X##ca", &s.x, 0.1f)) changed = true;
            ImGui::SetNextItemWidth(sliderW);
            if (ImGui::DragFloat("Y##ca", &s.y, 0.1f)) changed = true;
            ImGui::SetNextItemWidth(sliderW);
            if (ImGui::DragFloat("Z##ca", &s.z, 0.1f)) changed = true;
            ImGui::SetNextItemWidth(sliderW);
            if (ImGui::DragFloat("Rotazione##ca", &s.ry, 1.0f, -360.0f, 360.0f, "%.0f°"))
                changed = true;

            ImGui::Separator();

            // ── LE DUE STRADE, DICHIARATE UNA ACCANTO ALL'ALTRA ──────────
            // Sono la stessa azione ("modifica questa struttura") con due portate
            // opposte, e sbagliare porta significa o rovinare tre bunker per
            // sistemarne uno, o rifare quattro volte la stessa correzione. Stanno
            // vicine apposta, con la portata scritta nel testo del pulsante.
            if (ImGui::Button("Modifica solo QUESTA..."))
                m_pendingEditInstance = m_selStruct;
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Apre l'editor strutture su questa sola struttura.\n"
                                  "Le modifiche restano qui: il tipo e le altre copie\n"
                                  "in mappa non cambiano.");

            if (ImGui::Button("Modifica il TIPO (tutte le copie)"))
                m_pendingOpenType = s.type;
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Modifica la struttura di libreria: cambia ovunque\n"
                                  "sia usata, in questa mappa e nelle altre.");

            if (modified)
            {
                if (ImGui::Button("Ripristina dall'originale"))
                {
                    pushUndo("ripristina struttura");
                    s.localParts.clear();
                    changed = true;
                }
                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip("Butta via le modifiche fatte solo qui e torna\n"
                                      "alla struttura del tipo. Ctrl+Z per rimediare.");
            }

            ImGui::Separator();
            if (ImGui::Button("Esplodi in parti"))
            {
                explodeStructure(m_selStruct);
                return;   // `s` non esiste piu': l'istanza e' stata sciolta
            }
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("La scioglie nelle sue parti, ognuna un elemento\n"
                                  "della mappa modificabile da solo. Serve per le\n"
                                  "modifiche ad hoc di UN punto: da qui in poi il\n"
                                  "tipo non le cambia piu'. Ctrl+Z per tornare indietro.");

            ImGui::Separator();
            if (ImGui::Button("Elimina struttura"))
            {
                m_structures.erase(m_structures.begin() + m_selStruct);
                m_selStruct = -1;
                changed = true;
            }
            if (changed)
            {
                m_dirty = true;
                rebuildStructurePreview();
                updateViewport();
            }
            return;
        }

        ImGui::TextColored({0.55f, 0.80f, 1.00f, 1.0f}, "Struttura parametrica");
        ImGui::TextDisabled("Si autora la RICETTA. I box li genera il motore, e non\n"
                            "si salvano: cambiano da soli se cambi i parametri.");
        ImGui::Separator();

        char nameBuf[64];
        std::snprintf(nameBuf, sizeof(nameBuf), "%s", s.label.c_str());
        ImGui::SetNextItemWidth(sliderW);
        if (editor::ui::textRow("Nome", nameBuf, sizeof(nameBuf)))
        { s.label = nameBuf; changed = true; }

        ImGui::SetNextItemWidth(sliderW);
        if (ImGui::BeginCombo("Tipo", mini::mapstructures::kindLabel(s.kind)))
        {
            // Switchback assente di proposito: vedi la nota in MapStructures.hpp —
            // non produce ancora torri percorribili in modo affidabile.
            const mini::StructureKind kinds[8] = {
                mini::StructureKind::Stair,      mini::StructureKind::Ramp,
                mini::StructureKind::Wall,       mini::StructureKind::Doorway,
                mini::StructureKind::Room,       mini::StructureKind::Platform,
                mini::StructureKind::Catwalk,    mini::StructureKind::Barricade };
            for (auto k : kinds)
                if (ImGui::Selectable(mini::mapstructures::kindLabel(k), s.kind == k))
                { s.kind = k; changed = true; }
            ImGui::EndCombo();
        }

        ImGui::Separator();
        ImGui::TextDisabled("Posizione");
        ImGui::SetNextItemWidth(sliderW);
        if (ImGui::DragFloat("X##st", &s.x, 0.1f)) changed = true;
        ImGui::SetNextItemWidth(sliderW);
        if (ImGui::DragFloat("Y##st", &s.y, 0.1f)) changed = true;
        ImGui::SetNextItemWidth(sliderW);
        if (ImGui::DragFloat("Z##st", &s.z, 0.1f)) changed = true;
        ImGui::SetNextItemWidth(sliderW);
        if (ImGui::DragFloat("Rotazione##st", &s.ry, 1.0f, -360.0f, 360.0f, "%.0f°"))
            changed = true;

        ImGui::Separator();
        if (isFlight)
        {
            ImGui::TextDisabled("Salita");
            ImGui::SetNextItemWidth(sliderW);
            if (ImGui::DragFloat("Dislivello##st", &s.rise, 0.1f, 0.1f, 40.0f, "%.2f m"))
                changed = true;
            ImGui::SetNextItemWidth(sliderW);
            if (ImGui::DragFloat("Larghezza##st", &s.width, 0.1f, 0.5f, 20.0f, "%.2f m"))
                changed = true;
            ImGui::SetNextItemWidth(sliderW);
            if (ImGui::DragFloat("Alzata##st", &s.riser, 0.01f, 0.0f,
                                 mini::config::STEP_HEIGHT, "%.2f m (0 = normativa)"))
                changed = true;
            // La PEDATA allunga o accorcia la scala senza toccare i gradini: il
            // numero di gradini dipende solo dal dislivello, quindi questa leva
            // cambia quanto spazio occupa e quanto è dolce, e non può romperla.
            ImGui::SetNextItemWidth(sliderW);
            if (ImGui::DragFloat("Pedata##st", &s.tread, 0.01f, 0.0f, 3.0f,
                                 "%.2f m (0 = normativa)"))
                changed = true;
            // I numeri che contano per progettare.
            const float run  = mini::mapstructures::flightRun(s);
            const float used = mini::mapstructures::effectiveRiser(s);
            const float trd  = mini::mapstructures::effectiveTread(s);
            const float slope = (trd > 0.001f)
                ? std::atan(used / trd) * 57.2957795f : 0.0f;
            ImGui::TextDisabled("%d gradini da %.2f × %.2f m — pendenza %.0f°",
                                mini::mapmetrics::stepsFor(s.rise, used), used, trd, slope);
            if (s.kind != SK::Switchback)
                ImGui::TextDisabled("occupa %.2f m in pianta", run);
            if (slope > 35.5f)
                ImGui::TextColored({1.0f, 0.80f, 0.40f, 1.0f},
                                   "Piu' ripida di una scala vera (30-35°): allunga la pedata.");
            if (s.kind == SK::Switchback)
            {
                ImGui::SetNextItemWidth(sliderW);
                if (ImGui::DragFloat("Dislivello per rampa##st", &s.flightRise, 0.1f, 0.0f, 10.0f,
                                     "%.2f m (0 = 3,00, un piano)")) changed = true;
                // I numeri che servono a progettare la torre: quante rampe e, soprattutto,
                // che l'INGOMBRO NON CAMBIA con l'altezza — è il punto di questa primitiva.
                const float maxF = (s.flightRise > 0.1f) ? s.flightRise : 3.0f;
                int nf = (int)std::ceil(s.rise / maxF); if (nf < 2) nf = 2;
                const float rF = s.rise / (float)nf;
                const float R  = (float)mini::mapmetrics::stepsFor(rF, used) * trd;
                const float wid = (s.width > 0.1f) ? s.width : mini::mapmetrics::STAIR_MIN_WIDTH;
                ImGui::TextDisabled("%d rampe da %.2f m — pianta %.1f × %.1f m",
                                    nf, rF, wid * 2.0f, R + wid);
                const float straight = (float)mini::mapmetrics::stepsFor(s.rise, used) * trd;
                ImGui::TextDisabled("diritta occuperebbe %.1f m di sviluppo", straight);
            }
            if (s.riser > mini::config::STEP_HEIGHT - 0.001f && s.riser > 0.0f)
                ImGui::TextColored({1.0f, 0.75f, 0.35f, 1.0f},
                                   "Alzata limitata a %.2f: oltre non ci si sale.",
                                   mini::config::STEP_HEIGHT);
        }
        else if (isWallish || s.kind == SK::Catwalk || s.kind == SK::Barricade)
        {
            ImGui::TextDisabled("%s", mini::mapstructures::kindLabel(s.kind));
            ImGui::SetNextItemWidth(sliderW);
            if (ImGui::DragFloat("Lunghezza##st", &s.length, 0.1f, 0.5f, 200.0f, "%.2f m"))
                changed = true;
            ImGui::SetNextItemWidth(sliderW);
            if (ImGui::DragFloat("Altezza##st", &s.height, 0.1f, 0.0f, 20.0f,
                                 "%.2f m (0 = normativa)")) changed = true;
            if (s.kind == SK::Catwalk)
            {
                ImGui::SetNextItemWidth(sliderW);
                if (ImGui::DragFloat("Larghezza##st", &s.width, 0.1f,
                                     mini::mapmetrics::CORRIDOR_MIN, 40.0f, "%.2f m"))
                { if (s.width < mini::mapmetrics::CORRIDOR_MIN) s.width = mini::mapmetrics::CORRIDOR_MIN;
                  changed = true; }
                ImGui::SetNextItemWidth(sliderW);
                if (ImGui::DragFloat("Quota impalcato##st", &s.y, 0.1f, 0.0f, 40.0f, "%.2f m"))
                    changed = true;
                if (ImGui::Checkbox("Parapetti", &s.railing)) changed = true;
                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip("Riparano chi ci sta sopra, MA tolgono la visuale\n"
                                      "verso il basso: e' il difetto KI #83 (posizione cieca\n"
                                      "verso le altre quote). Da mettere con criterio.");
                ImGui::TextDisabled("larghezza minima %.2f (e' un corridoio in quota);\n"
                                    "la lunghezza e' libera", mini::mapmetrics::CORRIDOR_MIN);
            }
            else if (s.kind == SK::Barricade)
            {
                ImGui::SetNextItemWidth(sliderW);
                if (ImGui::DragFloat("Lungh. elemento##st", &s.width, 0.1f, 0.3f, 20.0f, "%.2f m"))
                    changed = true;
                ImGui::SetNextItemWidth(sliderW);
                if (ImGui::DragFloat("Varco##st", &s.spacing, 0.1f, 0.0f, 20.0f,
                                     "%.2f m (0 = continua)")) changed = true;
                ImGui::SetNextItemWidth(sliderW);
                if (ImGui::DragFloat("Spessore##st", &s.thickness, 0.05f, 0.0f, 5.0f,
                                     "%.2f m (0 = 0,60)")) changed = true;
                ImGui::TextDisabled("Bassa %.2f = ci si spara sopra · alta %.2f = ripara in piedi",
                                    mini::mapmetrics::COVER_LOW, mini::mapmetrics::COVER_HIGH);
            }
            else
            {
                ImGui::SetNextItemWidth(sliderW);
                if (ImGui::DragFloat("Spessore##st", &s.thickness, 0.05f, 0.0f, 5.0f,
                                     "%.2f m (0 = normativo)")) changed = true;
            }
        }
        else if (s.kind == SK::Room)
        {
            ImGui::TextDisabled("Stanza");
            ImGui::SetNextItemWidth(sliderW);
            if (ImGui::DragFloat("Lato X##st", &s.sizeX, 0.1f, 2.0f, 80.0f, "%.2f m")) changed = true;
            ImGui::SetNextItemWidth(sliderW);
            if (ImGui::DragFloat("Lato Z##st", &s.sizeZ, 0.1f, 2.0f, 80.0f, "%.2f m")) changed = true;
            ImGui::SetNextItemWidth(sliderW);
            if (ImGui::DragFloat("Altezza##st", &s.height, 0.1f, 0.0f, 20.0f,
                                 "%.2f m (0 = normativa)")) changed = true;
            ImGui::SetNextItemWidth(sliderW);
            if (ImGui::DragFloat("Spessore muri##st", &s.thickness, 0.05f, 0.0f, 5.0f,
                                 "%.2f m (0 = normativo)")) changed = true;
            if (ImGui::Checkbox("Soffitto", &s.ceiling)) changed = true;
            ImGui::Separator();
            ImGui::TextDisabled("Aperture — su quali lati");
            const char* sideName[4] = { "Lato -Z", "Lato +Z", "Lato -X", "Lato +X" };
            for (int i = 0; i < 4; ++i)
                if (ImGui::Checkbox(sideName[i], &s.access[i])) changed = true;
            if (!s.access[0] && !s.access[1] && !s.access[2] && !s.access[3])
                ImGui::TextColored({1.0f, 0.55f, 0.45f, 1.0f},
                                   "Nessuna apertura: stanza sigillata.");
            const float lh = (s.height > 0.01f) ? s.height : mini::mapmetrics::WALL_HEIGHT;
            if (lh < mini::mapmetrics::CEILING_MIN - 0.01f)
                ImGui::TextColored({1.0f, 0.75f, 0.35f, 1.0f},
                    "Altezza sotto il minimo al coperto (%.2f m): il gigante non ci sta.",
                    mini::mapmetrics::CEILING_MIN);
        }
        else
        {
            ImGui::TextDisabled("Piattaforma");
            ImGui::SetNextItemWidth(sliderW);
            if (ImGui::DragFloat("Quota ripiano##st", &s.y, 0.1f, 0.0f, 40.0f, "%.2f m"))
                changed = true;
            ImGui::SetNextItemWidth(sliderW);
            const float pmin = mini::mapmetrics::ELEVATED_MIN_SPAN;
            if (ImGui::DragFloat("Lato X##st", &s.sizeX, 0.1f, pmin, 80.0f, "%.2f m"))
            { if (s.sizeX < pmin) s.sizeX = pmin; changed = true; }
            ImGui::SetNextItemWidth(sliderW);
            if (ImGui::DragFloat("Lato Z##st", &s.sizeZ, 0.1f, pmin, 80.0f, "%.2f m"))
            { if (s.sizeZ < pmin) s.sizeZ = pmin; changed = true; }
            ImGui::TextDisabled("lato minimo %.2f: sotto, il ripiano sparisce dal navmesh", pmin);
            ImGui::SetNextItemWidth(sliderW);
            if (ImGui::DragFloat("Quota di partenza##st", &s.baseY, 0.1f, -10.0f, 40.0f, "%.2f m"))
                changed = true;

            ImGui::Separator();
            ImGui::TextDisabled("Accessi — da quali lati ci si sale");
            const char* sideName[4] = { "Lato -Z", "Lato +Z", "Lato -X", "Lato +X" };
            for (int i = 0; i < 4; ++i)
                if (ImGui::Checkbox(sideName[i], &s.access[i])) changed = true;
            if (!s.access[0] && !s.access[1] && !s.access[2] && !s.access[3]
                && (s.y - s.baseY) > mini::config::STEP_HEIGHT)
                ImGui::TextColored({1.0f, 0.55f, 0.45f, 1.0f},
                                   "Nessun accesso: ci si sale solo volando.");
        }

        // ── APERTURA: comune a "muro con apertura" e alle porte della stanza ──
        if (hasOpening)
        {
            ImGui::Separator();
            const bool isWindow = (s.openSill > 0.01f);
            // PORTA o FINESTRA come SCELTA, non come "metti un numero nel parapetto".
            // Sono due cose diverse — una si attraversa, l'altra è un riparo da cui
            // sporgersi — e ognuna porta con sé le proprie misure sensate.
            ImGui::TextDisabled("Apertura");
            if (ImGui::RadioButton("Porta", !isWindow) && isWindow)
            {
                s.openSill = 0.0f;
                s.openW = mini::mapmetrics::DOOR_WIDTH;
                s.openH = mini::mapmetrics::DOOR_HEIGHT;
                changed = true;
            }
            ImGui::SameLine();
            if (ImGui::RadioButton("Finestra", isWindow) && !isWindow)
            {
                s.openSill = mini::mapmetrics::COVER_LOW;   // parapetto = copertura bassa
                s.openW = 2.0f;
                s.openH = 1.40f;
                changed = true;
            }
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Il parapetto sotto la finestra e' COPERTURA vera:\n"
                                  "ci si ripara dietro e si spara sopra.");

            ImGui::SetNextItemWidth(sliderW);
            const float minW = mini::mapstructures::minOpenWidth(s);
            const float minH = mini::mapstructures::minOpenHeight(s);
            if (ImGui::DragFloat("Larghezza##op", &s.openW, 0.05f, minW, 20.0f, "%.2f m"))
            { if (s.openW < minW) s.openW = minW; changed = true; }
            ImGui::SetNextItemWidth(sliderW);
            if (ImGui::DragFloat("Altezza##op", &s.openH, 0.05f, minH, 20.0f, "%.2f m"))
            { if (s.openH < minH) s.openH = minH; changed = true; }
            if (isWindow)
            {
                ImGui::SetNextItemWidth(sliderW);
                if (ImGui::DragFloat("Parapetto##op", &s.openSill, 0.05f, 0.20f, 5.0f, "%.2f m"))
                { if (s.openSill < 0.20f) s.openSill = 0.20f; changed = true; }
                ImGui::TextDisabled("bassa %.2f (ci si spara sopra) · alta %.2f (ripara in piedi)",
                                    mini::mapmetrics::COVER_LOW, mini::mapmetrics::COVER_HIGH);
            }
            if (s.kind == SK::Doorway)
            {
                ImGui::SetNextItemWidth(sliderW);
                if (ImGui::DragFloat("Scostamento##op", &s.openOff, 0.05f, -50.0f, 50.0f, "%.2f m"))
                    changed = true;
            }
            if (!isWindow)
                ImGui::TextDisabled("minimo %.2f × %.2f: ci passa il gigante", minW, minH);
        }

        ImGui::Separator();
        if (ImGui::Button("Elimina struttura"))
        {
            m_structures.erase(m_structures.begin() + m_selStruct);
            m_selStruct = -1;
            changed = true;
        }

        if (changed)
        {
            m_dirty = true;
            rebuildStructurePreview();
            updateViewport();
        }
        return;
    }

    // ── Comandante strategico (ADR-024/041) ──────────────────────────────
    if (m_selBox == kSelCommander)
    {
        if (!m_commander.exists) { ImGui::TextDisabled("Seleziona un elemento."); return; }
        ImGui::TextColored({0.75f,0.45f,0.95f,1.0f}, "Comandante strategico");
        ImGui::TextDisabled("Uno per mappa. NON e' nel roster: e' un obiettivo vivente\n"
                            "(come le torri). Non combatte, si difende soltanto.");
        ImGui::Separator();
        bool changed = false;

        // CommanderDef dal registry (dropdown, mai testo libero — ADR-044).
        ImGui::SetNextItemWidth(sliderW);
        if (ImGui::BeginCombo("Tipo##cmd",
                              m_commander.unit.empty() ? "-- scegli --"
                                                       : m_commander.unit.c_str()))
        {
            for (const auto& cid : m_commanderIds)
                if (ImGui::Selectable(cid.c_str(), m_commander.unit == cid))
                { m_commander.unit = cid; changed = true; }
            ImGui::EndCombo();
        }
        ImGui::TextDisabled("Definizione da data/commanders/ (non e' piu' una classe,\n"
                            "ADR-044). Non combatte: si difende soltanto.");

        ImGui::TextDisabled("Posizione (retrovie, al sicuro)");
        changed |= editor::ui::sliderRow("X##cmd", m_commander.x, -kPosLimit, kPosLimit, 0.1f, "%.2f", 18.0f);
        changed |= editor::ui::sliderRow("Z##cmd", m_commander.z, -kPosLimit, kPosLimit, 0.1f, "%.2f", 18.0f);

        ImGui::TextDisabled("Raggio di movimento (leash)");
        changed |= editor::ui::sliderRow("Raggio (m)##cmd", m_commander.leashRadius,
                                         0.f, 20.f, 0.5f, "%.1f m");
        if (m_commander.leashRadius <= 0.01f)
            ImGui::TextDisabled("0 = FERMO sul posto (si gira e spara soltanto).");
        else
            ImGui::TextDisabled("Area circolare da cui NON esce: si muove al suo\n"
                                "interno per coprirsi, mai fuori. Gizmo Scala = raggio.");

        if (ImGui::SmallButton("Rimuovi comandante"))
        { m_commander = CommanderEntry{}; m_selBox = -1; m_dirty = true; updateViewport(); return; }

        if (changed) { m_dirty = true; updateViewport(); }
        return;
    }

    if (m_selBox == -2 || m_selBox == -3)
    {
        // Spawn point
        auto& sp = (m_selBox == -2) ? m_spawnTeam1 : m_spawnTeam2;
        const char* teamName = (m_selBox == -2) ? "Spawn Alleati (T1)" : "Spawn Nemici (T2)";
        ImGui::TextColored({0.8f,0.8f,0.2f,1.0f}, "%s", teamName);
        ImGui::Separator();
        bool changed = false;
        changed |= editor::ui::sliderRow("X", sp[0], -kPosLimit, kPosLimit, 0.1f, "%.2f", 18.0f);
        changed |= editor::ui::sliderRow("Y", sp[1],   0.f,  5.f, 0.05f, "%.2f", 18.0f);
        changed |= editor::ui::sliderRow("Z", sp[2], -kPosLimit, kPosLimit, 0.1f, "%.2f", 18.0f);
        if (changed) { m_dirty = true; updateViewport(); }
        return;
    }

    // ── Istanza di PREFAB selezionata (ADR-048) ──────────────────────────
    // PRIMA del ramo settori: quello usa `<= -2000` come catch-all e catturerebbe
    // anche i codici dei prefab (-4000).
    if (m_selBox <= -4000 && (-4000 - m_selBox) < (int)m_prefabInsts.size())
    {
        auto& inst = m_prefabInsts[-4000 - m_selBox];
        const mini::PrefabDef* pf = m_prefabReg.getPrefab(inst.id);
        ImGui::TextColored({0.72f,0.62f,0.95f,1.0f}, "Prefab: %s", inst.id.c_str());
        ImGui::Separator();
        if (!pf)
            ImGui::TextColored({0.95f,0.35f,0.30f,1.0f},
                               "NON TROVATO in data/prefabs/ — l'istanza non produrra' nulla");
        else
            ImGui::TextDisabled("%d box di collisione, %d posizioni tattiche",
                                (int)pf->collision.size(), (int)pf->tactical.size());
        bool changed = false;
        changed |= editor::ui::dragRow("X", inst.x, 0.1f, -500.f, 500.f, "%.2f");
        changed |= editor::ui::dragRow("Y", inst.y, 0.1f, -100.f, 100.f, "%.2f");
        changed |= editor::ui::dragRow("Z", inst.z, 0.1f, -500.f, 500.f, "%.2f");
        changed |= editor::ui::sliderRow("Rotazione", inst.ry, 0.f, 360.f, 5.f, "%.0f");
        ImGui::TextDisabled("Contenuto e posizioni tattiche sono DERIVATI dal prefab:");
        ImGui::TextDisabled("si modificano nel file del prefab, valgono per ogni istanza.");
        if (changed) { m_dirty = true; updateViewport(); }
        return;
    }

    // ── Settore selezionato (ADR-034) ────────────────────────────────────
    if (m_selBox <= -2000)
    {
        int si = -2000 - m_selBox;
        if (si < 0 || si >= (int)m_sectors.size())
        { ImGui::TextDisabled("Seleziona un elemento."); return; }

        auto& s = m_sectors[si];
        ImGui::TextColored({0.7f,0.55f,0.95f,1.0f}, "Settore %d", si + 1);
        ImGui::Separator();
        bool changed = false;

        char buf[64];
        std::snprintf(buf, sizeof(buf), "%s", s.label.c_str());
        if (editor::ui::textRow("Nome", buf, sizeof(buf))) { s.label = buf; changed = true; }

        ImGui::TextDisabled("Area");
        changed |= editor::ui::sliderRow("X", s.x, -kPosLimit, kPosLimit, 0.1f, "%.2f", 18.0f);
        changed |= editor::ui::sliderRow("Z", s.z, -kPosLimit, kPosLimit, 0.1f, "%.2f", 18.0f);
        changed |= editor::ui::sliderRow("Raggio", s.radius, 2.f, 40.f, 0.5f, "%.1f");

        ImGui::TextDisabled("Valore strategico");
        changed |= editor::ui::sliderRow("Importanza", s.importance, 0.f, 1.f, 0.05f, "%.2f");
        ImGui::TextDisabled("Il Droide Tattico sceglie l'obiettivo fra i settori,\n"
                            "pesando importanza e quanto la zona e' contesa.");

        if (changed) { m_dirty = true; updateViewport(); }
        return;
    }

    // ── Posizione tattica selezionata (ADR-030) ──────────────────────────
    // Un solo pannello: il RUOLO descrive, i campi dicono cosa la posizione SA
    // fare. Le sezioni si adattano al ruolo per non mostrare campi inutili.
    if (m_selBox <= -1000 && m_selBox > -2000)
    {
        int pi = -1000 - m_selBox;
        if (pi < 0 || pi >= (int)m_positions.size())
        { ImGui::TextDisabled("Seleziona un elemento."); return; }

        auto& p = m_positions[pi];
        ImGui::TextColored({0.3f,0.9f,0.75f,1.0f}, "Posizione tattica %d", pi + 1);
        ImGui::Separator();
        bool changed = false;

        static const char* kRoles[] = {"cover", "vantage", "defensive",
                                       "chokepoint", "observation"};
        int roleIdx = 0;
        for (int k = 0; k < 5; ++k) if (p.role == kRoles[k]) { roleIdx = k; break; }
        if (ImGui::Combo("Ruolo", &roleIdx, kRoles, 5)) { p.role = kRoles[roleIdx]; changed = true; }
        ImGui::TextDisabled(roleIdx == 0 ? "Riparo da cui combattere"
                          : roleIdx == 1 ? "Sopraelevato / dominante"
                          : roleIdx == 2 ? "Posizione da tenere"
                          : roleIdx == 3 ? "Strettoia / ingresso da presidiare"
                                         : "Punto d'osservazione");

        ImGui::TextDisabled("Posizione");
        changed |= editor::ui::sliderRow("X", p.x, -kPosLimit, kPosLimit, 0.1f, "%.2f", 18.0f);
        changed |= editor::ui::sliderRow("Y", p.y, -kElevLo, kElevHi, 0.05f, "%.2f", 18.0f);
        changed |= editor::ui::sliderRow("Z", p.z, -kPosLimit, kPosLimit, 0.1f, "%.2f", 18.0f);
        changed |= editor::ui::sliderRow("Fronte", p.facing, -180.f, 180.f, 1.f, "%.0f");

        ImGui::TextDisabled("Riparo");
        changed |= editor::ui::sliderRow("Protezione", p.protection, 0.f, 1.f, 0.05f, "%.2f");
        ImGui::TextDisabled(p.protection <= 0.0f  ? "Non ripara: non e' una copertura"
                          : p.protection >= 0.75f ? "Ottima: l'AI la preferisce"
                          : p.protection <= 0.35f ? "Scarsa: ripiego"
                                                  : "Media");
        if (p.protection > 0.0f)
        {
            changed |= editor::ui::sliderRow("Altezza", p.height, 0.4f, 3.0f, 0.05f, "%.2f");
            ImGui::TextDisabled(p.height < 1.2f ? "Bassa: l'AI fara' peek-over"
                                                : "Alta: l'AI fara' peek-around");
        }
        changed |= ImGui::Checkbox("Si puo' fare fuoco da qui", &p.canShoot);

        // Settore di tiro (ADR-031): cosa questa posizione BATTE. È ciò che
        // permette all'AI di venirci per ATTACCARE, non solo per nascondersi.
        if (p.canShoot)
        {
            ImGui::TextDisabled("Settore di tiro (giallo nel viewport)");
            changed |= editor::ui::sliderRow("Ampiezza", p.fireArc, 10.f, 360.f, 5.f, "%.0f");
            changed |= editor::ui::sliderRow("Gittata", p.fireRange, 3.f, 60.f, 1.f, "%.0f");
            ImGui::TextDisabled(p.fireArc <= 60.f  ? "Stretto: posizione specializzata"
                              : p.fireArc >= 240.f ? "Molto ampio: copre quasi ovunque"
                                                   : "Medio");
        }

        // Esposizione (ADR-033): DERIVATA, sola lettura. Non si autora, ma vederla
        // guida l'authoring — un punto molto esposto è una cattiva posizione di tiro.
        if (pi < (int)m_exposure.size())
        {
            const float ex = m_exposure[pi];
            ImGui::TextDisabled("Esposizione (calcolata): %.0f%%", ex * 100.0f);
            ImGui::TextDisabled(ex >= 0.5f ? "Molto allo scoperto: battuta da meta' mappa"
                              : ex <= 0.15f ? "Riparata: pochi angoli di tiro la battono"
                                            : "Media");
        }

        // Copertura dall'alto (doc 41 B3): DERIVATA, sola lettura. Utile a capire se una
        // posizione è al riparo da tiro/lanci dall'alto e se sta sotto una struttura.
        if (pi < (int)m_overhead.size())
            ImGui::TextDisabled(m_overhead[pi] ? "Coperta dall'alto: si' (sotto una struttura)"
                                               : "Coperta dall'alto: no (cielo aperto)");

        // Visuale VERTICALE (KI #83): DERIVATA, sola lettura. Misurato in partita che
        // l'AI ingaggia tutto cio' che vede a quota diversa e spara: se qui c'e' 0, il
        // problema non e' l'AI ma la geometria — da qui non si battera' MAI nessuno
        // sopra/sotto, per quanto la posizione sia elevata.
        if (pi < (int)m_vertSight.size() && pi < (int)m_vertPairs.size())
        {
            const int vs = m_vertSight[pi], vp = m_vertPairs[pi];
            if (vp == 0)
                ImGui::TextDisabled("Visuale verticale: n/d (nessuna posizione ad altra quota)");
            else if (vs == 0)
            {
                ImGui::TextColored(ImVec4(0.95f, 0.45f, 0.35f, 1.0f),
                                   "Visuale verticale: 0 / %d — CIECA sopra/sotto", vp);
                ImGui::TextDisabled("Da qui non si battera' MAI un'altra quota:");
                ImGui::TextDisabled("avvicinala al BORDO, o abbassa il parapetto.");
            }
            else
            {
                ImGui::TextColored(ImVec4(0.45f, 0.85f, 0.50f, 1.0f),
                                   "Visuale verticale: %d / %d (%.0f%%)",
                                   vs, vp, 100.0f * (float)vs / (float)vp);
                ImGui::TextDisabled("Utile al combattimento verticale (ponti/rialzati).");
            }
        }

        ImGui::TextDisabled("Tattica");
        changed |= editor::ui::sliderRow("Importanza", p.importance, 0.f, 1.f, 0.05f, "%.2f");
        if (p.role == "defensive" || p.role == "chokepoint")
            changed |= editor::ui::sliderRow("Raggio", p.radius, 0.5f, 20.f, 0.1f, "%.2f");

        if (changed) { m_dirty = true; updateViewport(); }
        return;
    }

    // ── Danger zone selezionata (15_MapMetadata) ─────────────────────────
    if (m_selBox <= -200 && m_selBox > -300)
    {
        int di = -200 - m_selBox;
        if (di < 0 || di >= (int)m_dangers.size())
        { ImGui::TextDisabled("Seleziona un elemento."); return; }

        auto& d = m_dangers[di];
        ImGui::TextColored({0.95f,0.55f,0.2f,1.0f}, "Danger Zone %d", di + 1);
        ImGui::Separator();
        bool changed = false;
        ImGui::TextDisabled("Posizione");
        changed |= editor::ui::sliderRow("X", d.x, -kPosLimit, kPosLimit, 0.1f, "%.2f", 18.0f);
        changed |= editor::ui::sliderRow("Y", d.y, -kElevLo, kElevHi, 0.05f, "%.2f", 18.0f);
        changed |= editor::ui::sliderRow("Z", d.z, -kPosLimit, kPosLimit, 0.1f, "%.2f", 18.0f);
        ImGui::TextDisabled("Area");
        changed |= editor::ui::sliderRow("Raggio", d.radius, 0.5f, 30.f, 0.1f, "%.1f");
        changed |= editor::ui::sliderRow("Pericolo", d.level, 0.f, 1.f, 0.01f, "%.2f");
        if (changed) { m_dirty = true; updateViewport(); }
        return;
    }

    // ── Spawn veicolo selezionato (19_Vehicles Fase B) ───────────────────
    // ── Bersaglio strategico (DestroyTarget) — PRIMA dei veicoli: -500 <= -400 ──
    if (m_selBox <= -500)
    {
        int ti = -500 - m_selBox;
        if (ti < 0 || ti >= (int)m_targets.size())
        { ImGui::TextDisabled("Seleziona un elemento."); return; }

        auto& t = m_targets[ti];
        ImGui::TextColored({0.9f,0.6f,0.2f,1.0f}, "Bersaglio strategico");
        ImGui::Separator();
        bool changed = false;

        ImGui::SetNextItemWidth(sliderW);
        if (editor::ui::textRow("Label##tg", t.label, sizeof(t.label))) changed = true;
        ImGui::TextDisabled("La label e' il nome referenziato dall'obiettivo destroy_target.");
        ImGui::TextDisabled("Posizione");
        changed |= editor::ui::sliderRow("X##tg", t.x, -kPosLimit, kPosLimit, 0.1f, "%.2f", 18.0f);
        changed |= editor::ui::sliderRow("Z##tg", t.z, -kPosLimit, kPosLimit, 0.1f, "%.2f", 18.0f);
        changed |= editor::ui::sliderRow("Y (altezza)##tg", t.y, 0.f, 30.f, 0.1f, "%.2f m", 18.0f);
        ImGui::TextDisabled("Altezza sopra il suolo: 0 = a terra, >0 = alzata (es. su\n"
                            "una piattaforma). La struttura e' statica: resta dove la metti.");
        changed |= editor::ui::sliderRow("Rotazione##tg", t.ry, -180.f, 180.f, 1.f, "%.0f");
        changed |= editor::ui::sliderRow("Scala##tg", t.scale, 0.2f, 8.f, 0.05f, "%.2f");

        ImGui::TextDisabled("Fazione");
        int teamIdx = (t.team == 1) ? 0 : 1;
        static const char* kTeams[] = {"Repubblica (cloni)", "Separatisti (droidi)"};
        if (ImGui::Combo("Team##tg", &teamIdx, kTeams, 2))
        { t.team = (teamIdx == 0) ? 1 : 2; changed = true; }
        ImGui::TextDisabled("Serve per dare a ogni fazione le PROPRIE strutture\n"
                            "(torre comunicazioni/controllo).");

        ImGui::TextDisabled("Ruolo (doc 34/36)");
        static const char* kRoles[] = {"Generico (solo bersaglio)",
                                       "Torre di comunicazione",
                                       "Torre di controllo"};
        if (ImGui::Combo("Ruolo##tg", &t.role, kRoles, 3)) changed = true;
        if (t.role == 1)
            ImGui::TextDisabled("Finche' e' viva la sua fazione comunica bene. Se cade,\n"
                                "informazioni/ordini/rinforzi RALLENTANO - mai bloccati.");
        else if (t.role == 2)
            ImGui::TextDisabled("Da' visione d'insieme alla SUA fazione: SEGNALA i posti\n"
                                "che contano (settori contesi, strutture nemiche).\n"
                                "NON da' ordini e non manda nessuno in un punto preciso:\n"
                                "e' ogni soldato a scegliere quale segnale seguire.");

        ImGui::TextDisabled("Valore tattico per l'AI (doc 35)");
        changed |= editor::ui::sliderRow("Priorita'##tg", t.priority, 0.f, 1.f, 0.05f, "%.2f");
        ImGui::TextDisabled("Quanto la fazione AVVERSARIA vuole distruggerla.\n"
                            "E' il peso con cui il comando la confronta coi settori.");
        changed |= editor::ui::sliderRow("Raggio ingaggio (m)##tg", t.engageRadius,
                                         0.f, 60.f, 1.f, "%.0f m");
        if (t.engageRadius <= 0.0f)
            ImGui::TextDisabled("0 = MAI ingaggiata di iniziativa: resta affare del\n"
                                "giocatore e del comando. E' il default.");
        else if (t.engageRadius < 3.0f)
            ImGui::TextColored({1.0f, 0.6f, 0.2f, 1.0f},
                               "Troppo piccolo: nessuna AI si trovera' mai cosi'\n"
                               "vicino. Indicativamente servono 15-30 m.");
        else
            ImGui::TextDisabled("Un'unita' avversaria entro questo raggio la attacca\n"
                                "di iniziativa, ma SOLO se non ha bersagli-unita':\n"
                                "una struttura non spara, viene sempre dopo.\n"
                                "Riferimento: la mappa firebase e' 50x40 m.");

        ImGui::TextDisabled("Resistenza");
        changed |= editor::ui::sliderRow("HP##tg", t.hp, 10.f, 2000.f, 10.f, "%.0f");

        ImGui::TextDisabled("Collisione (0 = ricavata dalla scala)");
        changed |= editor::ui::sliderRow("Semiasse X##tg", t.halfX, 0.f, 10.f, 0.1f, "%.2f");
        changed |= editor::ui::sliderRow("Semiasse Y##tg", t.halfY, 0.f, 10.f, 0.1f, "%.2f");
        changed |= editor::ui::sliderRow("Semiasse Z##tg", t.halfZ, 0.f, 10.f, 0.1f, "%.2f");
        ImGui::TextDisabled("Struttura statica colpibile e SOLIDA: prima AI e giocatore\n"
                            "ci passavano attraverso (mancava il collider).");

        if (changed) { m_dirty = true; updateViewport(); }
        return;
    }

    if (m_selBox <= -400)
    {
        int vi = -400 - m_selBox;
        if (vi < 0 || vi >= (int)m_vehSpawns.size())
        { ImGui::TextDisabled("Seleziona un elemento."); return; }

        auto& v = m_vehSpawns[vi];
        ImGui::TextColored({0.95f,0.65f,0.25f,1.0f}, "Spawn Veicolo %d", vi + 1);
        ImGui::Separator();
        bool changed = false;

        // Veicolo dal registry (dropdown, mai testo libero)
        ImGui::SetNextItemWidth(sliderW);
        if (ImGui::BeginCombo("Veicolo##vsid",
                              v.vehicleId.empty() ? "-- scegli --"
                                                  : v.vehicleId.c_str()))
        {
            for (const auto& vid : m_vehicleIds)
                if (ImGui::Selectable(vid.c_str(), v.vehicleId == vid))
                { v.vehicleId = vid; changed = true; }
            ImGui::EndCombo();
        }

        ImGui::TextDisabled("Posizione");
        changed |= editor::ui::sliderRow("X", v.x, -kPosLimit, kPosLimit, 0.1f, "%.2f", 18.0f);
        changed |= editor::ui::sliderRow("Z", v.z, -kPosLimit, kPosLimit, 0.1f, "%.2f", 18.0f);
        ImGui::TextDisabled("Orientamento");
        changed |= editor::ui::sliderRow("Yaw°", v.ry, -180.f, 180.f, 1.f, "%.0f");
        ImGui::TextDisabled("Il runtime decollide lo spawn e appoggia al suolo.");

        if (changed) { m_dirty = true; updateViewport(); }
        return;
    }

    // ── Patrol route selezionata (15_MapMetadata) ────────────────────────
    if (m_selBox <= -300)
    {
        int ri = -300 - m_selBox;
        if (ri < 0 || ri >= (int)m_routes.size())
        { ImGui::TextDisabled("Seleziona un elemento."); return; }

        auto& r = m_routes[ri];
        ImGui::TextColored({0.75f,0.5f,1.0f,1.0f}, "Percorso pattuglia");
        ImGui::Separator();
        bool changed = false;

        ImGui::SetNextItemWidth(sliderW);
        if (editor::ui::textRow("Nome##prid", r.id, sizeof(r.id))) changed = true;

        ImGui::TextDisabled("Punti (%d)", (int)r.points.size());
        for (int pi = 0; pi < (int)r.points.size(); ++pi)
        {
            char lbl[32];
            std::snprintf(lbl, sizeof(lbl), "Punto %d##rp%d", pi + 1, pi);
            if (ImGui::Selectable(lbl, pi == m_selRoutePt))
            { m_selRoutePt = pi; updateViewport(); }
        }
        if (ImGui::SmallButton("+ Punto"))
        {
            // Nuovo punto accanto all'ultimo (o all'origine)
            std::array<float,3> np = r.points.empty()
                ? std::array<float,3>{0.f, 0.5f, 0.f}
                : std::array<float,3>{r.points.back()[0] + 2.f,
                                      r.points.back()[1],
                                      r.points.back()[2]};
            r.points.push_back(np);
            m_selRoutePt = (int)r.points.size() - 1;
            changed = true;
        }
        ImGui::SameLine();
        if (ImGui::SmallButton("- Punto") && !r.points.empty()
            && m_selRoutePt >= 0 && m_selRoutePt < (int)r.points.size())
        {
            r.points.erase(r.points.begin() + m_selRoutePt);
            m_selRoutePt = std::max(0, m_selRoutePt - 1);
            changed = true;
        }

        if (m_selRoutePt >= 0 && m_selRoutePt < (int)r.points.size())
        {
            auto& pt = r.points[m_selRoutePt];
            ImGui::TextDisabled("Posizione punto %d", m_selRoutePt + 1);
            changed |= editor::ui::sliderRow("X", pt[0], -kPosLimit, kPosLimit, 0.1f, "%.2f", 18.0f);
            changed |= editor::ui::sliderRow("Y", pt[1], -kElevLo, kElevHi, 0.05f, "%.2f", 18.0f);
            changed |= editor::ui::sliderRow("Z", pt[2], -kPosLimit, kPosLimit, 0.1f, "%.2f", 18.0f);
        }

        if (changed) { m_dirty = true; updateViewport(); }
        return;
    }

    // ── Command post selezionato ─────────────────────────────────────────
    if (m_selBox <= -10)
    {
        int pi = -10 - m_selBox;
        if (pi < 0 || pi >= (int)m_posts.size())
        { ImGui::TextDisabled("Seleziona un elemento."); return; }

        auto& p = m_posts[pi];
        ImGui::TextColored({0.8f,0.8f,0.2f,1.0f}, "Command Post");
        ImGui::Separator();
        bool changed = false;

        ImGui::SetNextItemWidth(sliderW);
        if (editor::ui::textRow("Nome##cpl", p.label, sizeof(p.label))) changed = true;

        const char* teams[] = {"Neutrale", "Alleati (T1)", "Nemici (T2)"};
        ImGui::SetNextItemWidth(sliderW);
        if (editor::ui::comboRow("Team iniziale##cpt", p.team, teams, 3)) changed = true;

        ImGui::TextDisabled("Posizione");
        changed |= editor::ui::sliderRow("X", p.x, -kPosLimit, kPosLimit, 0.1f, "%.2f", 18.0f);
        changed |= editor::ui::sliderRow("Y", p.y, -kElevLo, kElevHi, 0.05f, "%.2f", 18.0f);
        changed |= editor::ui::sliderRow("Z", p.z, -kPosLimit, kPosLimit, 0.1f, "%.2f", 18.0f);

        ImGui::TextDisabled("Cattura");
        changed |= editor::ui::sliderRow("Raggio", p.radius, 1.f, 20.f, 0.1f, "%.1f");
        changed |= editor::ui::sliderRow("Tempo (s)", p.captureTime, 1.f, 30.f, 0.5f, "%.1f");

        if (changed) { m_dirty = true; updateViewport(); }
        return;
    }

    if (m_selBox < 0 || m_selBox >= (int)m_boxes.size())
    {
        ImGui::TextDisabled("Seleziona un box.");
        return;
    }

    auto& b = m_boxes[m_selBox];
    ImGui::TextColored({0.8f,0.8f,0.2f,1.0f}, "Box %d", m_selBox);
    if (b.label[0]) ImGui::SameLine(), ImGui::TextDisabled(" - %s", b.label);
    ImGui::Separator();

    bool changed = false;

    // Etichetta
    ImGui::SetNextItemWidth(sliderW);
    if (editor::ui::textRow("Etichetta", b.label, sizeof(b.label)))
        changed = true;

    // Tipo
    ImGui::SetNextItemWidth(sliderW);
    const char* types[] = {"floor","wall","platform","cover","decoration"};
    int typeIdx = 1;
    for (int i = 0; i < 5; ++i) if (std::strcmp(b.type, types[i]) == 0) { typeIdx = i; break; }
    if (ImGui::BeginCombo("Tipo", types[typeIdx]))
    {
        for (int i = 0; i < 5; ++i) {
            bool s = (i == typeIdx);
            if (ImGui::Selectable(types[i], s)) {
                std::strncpy(b.type, types[i], sizeof(b.type) - 1);
                changed = true;
            }
            if (s) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }

    if (ImGui::Checkbox("Collider", &b.isCollider)) changed = true;

    ImGui::Separator();
    // ── I NUMERI DIGITATI NON SI AGGANCIANO ALLA GRIGLIA ──────────────────
    // Qui c'era `b.x = snap(b.x)` dopo ogni modifica: con passo 0,5 un 2,40 scritto
    // a mano diventava 2,50, e ogni valore fuori griglia era **inesprimibile**
    // (segnalato dall'utente). L'aggancio governa i GESTI nel viewport — disegnare,
    // trascinare il gizmo, tirare una faccia — dove serve a far combaciare le cose.
    // Nel pannello si scrive un numero: quel numero è la richiesta, e va rispettata.
    ImGui::TextDisabled("Posizione");
    const float posSpeed = m_gridSnap > 0 ? m_gridSnap : 0.1f;
    if (editor::ui::sliderRow("X", b.x, -1000.f, 1000.f, posSpeed, "%.2f", 18.0f))
        changed = true;
    if (editor::ui::sliderRow("Y", b.y, -50.f, 200.f, posSpeed * 0.5f, "%.2f", 18.0f))
        changed = true;
    if (editor::ui::sliderRow("Z", b.z, -1000.f, 1000.f, posSpeed, "%.2f", 18.0f))
        changed = true;

    ImGui::TextDisabled("Rotazione");
    if (editor::ui::sliderRow("Y°", b.ry, -180.f, 180.f, 1.0f, "%.1f", 18.0f))
        changed = true;

    ImGui::Separator();
    ImGui::TextDisabled("Dimensioni");
    // Niente `snap()` nemmeno qui: una misura si scrive come la si vuole. Resta il
    // pavimento a 5 cm, che non è una preferenza — sotto, il box sparisce dal
    // navmesh senza dire niente.
    if (editor::ui::sliderRow("W", b.sx, 0.05f, 500.f, posSpeed, "%.2f", 18.0f))
        { if (b.sx < 0.05f) b.sx = 0.05f; changed = true; }
    if (editor::ui::sliderRow("H", b.sy, 0.05f, 200.f, posSpeed * 0.5f, "%.2f", 18.0f))
        { if (b.sy < 0.05f) b.sy = 0.05f; changed = true; }
    if (editor::ui::sliderRow("D", b.sz, 0.05f, 500.f, posSpeed, "%.2f", 18.0f))
        { if (b.sz < 0.05f) b.sz = 0.05f; changed = true; }

    ImGui::Separator();
    ImGui::TextDisabled("Colore");
    float col[3] = {b.r, b.g, b.b};
    ImGui::SetNextItemWidth(sliderW);
    if (ImGui::ColorEdit3("##boxcol", col,
                          ImGuiColorEditFlags_NoAlpha | ImGuiColorEditFlags_Float))
    {
        b.r = col[0]; b.g = col[1]; b.b = col[2];
        changed = true;
    }

    if (changed) { m_dirty = true; updateViewport(); }
}

// ── drawViewport ─────────────────────────────────────────────────────────────
void MapEditor::drawViewport(float vpW, float vpH)
{
    // Il righello si aggancia alla STESSA griglia con cui si costruisce: misurare
    // su un passo diverso da quello con cui si posiziona darebbe numeri che non
    // corrispondono a nulla di piazzabile.
    m_viewport.setGridSnap(m_gridSnap);

    // L'ingombro del contenuto, così cambiare vista inquadra la MAPPA invece di
    // puntare a caso. Ricalcolato qui perché è l'unico posto che conosce sia i box
    // a mano sia quelli generati dalle primitive.
    {
        bool any = false;
        glm::vec3 mn{0.0f}, mx{0.0f};
        auto acc = [&](float x, float y, float z, float ry, float sx, float sy, float sz) {
            const float a = ry * 3.14159265f / 180.0f;
            const float c = std::fabs(std::cos(a)), s = std::fabs(std::sin(a));
            const float hx = (sx * 0.5f) * c + (sz * 0.5f) * s;
            const float hz = (sx * 0.5f) * s + (sz * 0.5f) * c;
            const float hy = sy * 0.5f;
            const glm::vec3 lo{x - hx, y - hy, z - hz}, hi{x + hx, y + hy, z + hz};
            if (!any) { mn = lo; mx = hi; any = true; return; }
            mn = glm::min(mn, lo); mx = glm::max(mx, hi);
        };
        for (const auto& b : m_boxes)         acc(b.x, b.y, b.z, b.ry, b.sx, b.sy, b.sz);
        for (const auto& g : m_structPreview) acc(g.x, g.y, g.z, g.ry, g.sx, g.sy, g.sz);
        if (any) m_viewport.setContentBounds(mn, mx);
    }

    m_viewport.draw(false);

    // Selezione dal viewport (ray-picking): un click su un oggetto lo seleziona
    // esattamente come cliccarlo nella lista. Il pickId assegnato in
    // updateViewport È già il codice di m_selBox, quindi basta assegnarlo.
    int picked = 0;
    if (m_viewport.popClickedMapBox(picked))
    {
        // In modalità creazione prefab, Shift/Ctrl+click NON cambia la selezione: la
        // RIFINISCE (aggiunge/toglie l'elemento), come nei file manager e nei software
        // 3D. Il raggio fa la presa grossolana, il click il lavoro di precisione.
        const bool refine = m_prefabZoneMode
                          && (ImGui::GetIO().KeyShift || ImGui::GetIO().KeyCtrl);
        if (refine)
        {
            // Al primo ritocco si "congela" la selezione corrente del raggio, così si
            // parte da ciò che si sta già vedendo invece che da una lista vuota.
            if (!m_prefabPickManual)
            {
                prefabZoneCollect(m_prefabPickBoxes, m_prefabPickPositions);
                m_prefabPickManual = true;
            }
            auto toggle = [](std::vector<int>& v, int idx) {
                auto it = std::find(v.begin(), v.end(), idx);
                if (it != v.end()) v.erase(it); else v.push_back(idx);
            };
            if (picked >= 0)                                   // box
                toggle(m_prefabPickBoxes, picked);
            else if (picked <= -1000 && picked > -2000)         // posizione tattica
                toggle(m_prefabPickPositions, -1000 - picked);
            updateViewport();
        }
        else
        {
            // Ctrl+click AGGIUNGE/TOGLIE dalla selezione (G3), click normale
            // sostituisce. Fuori dalla modalità prefab, dove Ctrl ha già un altro
            // significato — per questo il ramo `refine` viene prima.
            setSelection(picked, ImGui::GetIO().KeyCtrl);
        }
    }
    (void)vpW; (void)vpH;
}

} // namespace editor
