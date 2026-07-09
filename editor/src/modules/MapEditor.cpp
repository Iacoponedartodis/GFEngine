// MapEditor.cpp
// Modulo GFEditor per la modifica visuale della geometria delle mappe.
// Layout: [lista box | viewport 3D | pannello proprietà]
// Salva/carica da data/maps/<id>.json, campo "geometry".

#include "modules/MapEditor.hpp"
#include "util/FileDialog.hpp"
#include "util/UiWidgets.hpp"

#include <imgui.h>
#include <SDL2/SDL.h>

#include <nlohmann/json.hpp>
#include <fstream>
#include <filesystem>
#include <algorithm>
#include <cstring>
#include <cstdio>
#include <cmath>

namespace fs = std::filesystem;
using json   = nlohmann::json;

namespace editor
{

// ── Helpers ───────────────────────────────────────────────────────────────────
static std::string getDataDir()
{
    char* base = SDL_GetBasePath();
    std::string exeDir = base ? base : "./";
    SDL_free(base);
    // Vai 3 livelli su dall'exe (build/config/Debug → project root) per la cartella sorgente
    std::error_code ec;
    std::filesystem::path sourceData = std::filesystem::canonical(
        std::filesystem::path(exeDir) / "../../../data", ec);
    if (!ec && std::filesystem::exists(sourceData, ec))
        return sourceData.string();
    // Fallback: output dir
    return exeDir + "data";
}

// ── Ctor ─────────────────────────────────────────────────────────────────────
MapEditor::MapEditor()
{
    loadMaps();
    if (!m_mapList.empty())
        loadMap(m_mapList[0].id);
}

// ── tick ─────────────────────────────────────────────────────────────────────
void MapEditor::tick(float dt)
{
    m_viewport.tick(dt);

    // Gizmo Sposta: applica lo spostamento all'elemento selezionato.
    glm::vec3 delta;
    if (m_viewport.popGizmoDelta(delta))
    {
        if (m_selBox >= 0 && m_selBox < (int)m_boxes.size())
        {
            auto& b = m_boxes[m_selBox];
            b.x += delta.x; b.y += delta.y; b.z += delta.z;
        }
        else if (m_selBox == -2)
        { m_spawnTeam1[0]+=delta.x; m_spawnTeam1[1]+=delta.y; m_spawnTeam1[2]+=delta.z; }
        else if (m_selBox == -3)
        { m_spawnTeam2[0]+=delta.x; m_spawnTeam2[1]+=delta.y; m_spawnTeam2[2]+=delta.z; }
        else if (m_selBox <= -10 && (-10 - m_selBox) < (int)m_posts.size())
        {
            auto& p = m_posts[-10 - m_selBox];
            p.x += delta.x; p.y += delta.y; p.z += delta.z;
        }
        m_dirty = true;
        updateViewport();
    }

    // Gizmo Ruota: i box mappa ruotano solo attorno a Y.
    glm::vec3 rotDelta;
    if (m_viewport.popGizmoRotDelta(rotDelta))
    {
        if (m_selBox >= 0 && m_selBox < (int)m_boxes.size())
        {
            auto& b = m_boxes[m_selBox];
            b.ry += rotDelta.y;
            while (b.ry >  180.0f) b.ry -= 360.0f;
            while (b.ry < -180.0f) b.ry += 360.0f;
            m_dirty = true;
            updateViewport();
        }
    }

    // Gizmo Scala: dimensioni del box (full size), con minimo.
    glm::vec3 scaleDelta;
    if (m_viewport.popGizmoScaleDelta(scaleDelta))
    {
        if (m_selBox >= 0 && m_selBox < (int)m_boxes.size())
        {
            auto& b = m_boxes[m_selBox];
            b.sx += scaleDelta.x; if (b.sx < 0.1f) b.sx = 0.1f;
            b.sy += scaleDelta.y; if (b.sy < 0.1f) b.sy = 0.1f;
            b.sz += scaleDelta.z; if (b.sz < 0.1f) b.sz = 0.1f;
            m_dirty = true;
            updateViewport();
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
        me.id   = j.value("id",   entry.path().stem().string());
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
    m_boxes.clear();
    m_posts.clear();
    m_selBox = -1;

    if (j.contains("spawn_team1") && j["spawn_team1"].size() >= 3)
        m_spawnTeam1 = {j["spawn_team1"][0], j["spawn_team1"][1], j["spawn_team1"][2]};
    if (j.contains("spawn_team2") && j["spawn_team2"].size() >= 3)
        m_spawnTeam2 = {j["spawn_team2"][0], j["spawn_team2"][1], j["spawn_team2"][2]};

    if (j.contains("geometry") && j["geometry"].is_array())
    {
        for (auto& gb : j["geometry"])
        {
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

    m_dirty = false;
    updateViewport();
}

// ── saveMap ───────────────────────────────────────────────────────────────────
bool MapEditor::saveMap()
{
    if (m_mapJsonPath.empty()) return false;

    // Leggi JSON esistente (per preservare i campi non toccati)
    json j;
    {
        std::ifstream f(m_mapJsonPath);
        if (f) { try { f >> j; } catch (...) {} }
    }

    j["spawn_team1"] = {m_spawnTeam1[0], m_spawnTeam1[1], m_spawnTeam1[2]};
    j["spawn_team2"] = {m_spawnTeam2[0], m_spawnTeam2[1], m_spawnTeam2[2]};

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

    std::ofstream out(m_mapJsonPath);
    if (!out) return false;
    out << j.dump(4);
    m_dirty = false;
    return true;
}

// ── addBox ────────────────────────────────────────────────────────────────────
void MapEditor::addBox()
{
    BoxEntry b;
    // Posiziona al centro della scena
    b.x = 0; b.y = 1.0f; b.z = 0;
    std::strncpy(b.type, "wall", sizeof(b.type) - 1);
    std::strncpy(b.label, "Nuovo Box", sizeof(b.label) - 1);
    m_boxes.push_back(b);
    m_selBox = (int)m_boxes.size() - 1;
    m_dirty  = true;
    updateViewport();
}

// ── duplicateBox ─────────────────────────────────────────────────────────────
void MapEditor::duplicateBox(int idx)
{
    if (idx < 0 || idx >= (int)m_boxes.size()) return;
    BoxEntry b = m_boxes[idx];
    b.x += 1.0f;
    m_boxes.insert(m_boxes.begin() + idx + 1, b);
    m_selBox = idx + 1;
    m_dirty  = true;
    updateViewport();
}

// ── deleteBox ─────────────────────────────────────────────────────────────────
void MapEditor::deleteBox(int idx)
{
    if (idx < 0 || idx >= (int)m_boxes.size()) return;
    m_boxes.erase(m_boxes.begin() + idx);
    m_selBox = std::min(m_selBox, (int)m_boxes.size() - 1);
    m_dirty  = true;
    updateViewport();
}

// ── updateViewport ────────────────────────────────────────────────────────────
void MapEditor::updateViewport()
{
    std::vector<FreeCameraViewport::MapBoxDraw> draws;
    draws.reserve(m_boxes.size() + 2);

    // Tipi "floor" visualizzati diversamente se showNavmesh
    for (int i = 0; i < (int)m_boxes.size(); ++i)
    {
        const auto& b = m_boxes[i];
        FreeCameraViewport::MapBoxDraw d;
        d.x = b.x; d.y = b.y; d.z = b.z; d.ry = b.ry;
        d.sx = b.sx; d.sy = b.sy; d.sz = b.sz;
        d.selected = (i == m_selBox);

        bool isFloor = (std::string(b.type) == "floor");

        if (m_showNavmesh && isFloor) {
            // Overlay verde per area navigabile
            d.r = 0.10f; d.g = 0.90f; d.b = 0.30f;
        } else {
            d.r = b.r; d.g = b.g; d.b = b.b;
        }

        draws.push_back(d);
    }

    // Spawn team1 (blu) come croce
    {
        FreeCameraViewport::MapBoxDraw s;
        s.x = m_spawnTeam1[0]; s.y = m_spawnTeam1[1]; s.z = m_spawnTeam1[2];
        s.ry = 0; s.sx = 0.6f; s.sy = 1.2f; s.sz = 0.6f;
        s.r = 0.20f; s.g = 0.50f; s.b = 1.00f;
        s.selected = false;
        draws.push_back(s);
    }
    // Spawn team2 (rosso)
    {
        FreeCameraViewport::MapBoxDraw s;
        s.x = m_spawnTeam2[0]; s.y = m_spawnTeam2[1]; s.z = m_spawnTeam2[2];
        s.ry = 0; s.sx = 0.6f; s.sy = 1.2f; s.sz = 0.6f;
        s.r = 1.00f; s.g = 0.20f; s.b = 0.20f;
        s.selected = false;
        draws.push_back(s);
    }

    // Command post: palo alto + area di cattura, colorati per team
    for (int i = 0; i < (int)m_posts.size(); ++i)
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
        draws.push_back(pole);

        FreeCameraViewport::MapBoxDraw area;
        area.x = p.x; area.y = p.y + 0.05f; area.z = p.z; area.ry = 0;
        area.sx = p.radius * 2.0f; area.sy = 0.05f; area.sz = p.radius * 2.0f;
        area.r = r * 0.6f; area.g = g * 0.6f; area.b = b * 0.6f;
        area.selected = sel;
        draws.push_back(area);
    }

    m_viewport.setMapBoxes(draws);

    // Gizmo sull'elemento selezionato (box o spawn point).
    // I box mappa ruotano solo attorno a Y; gli spawn: solo Sposta.
    if (m_selBox >= 0 && m_selBox < (int)m_boxes.size())
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
    else if (m_selBox <= -10 && (-10 - m_selBox) < (int)m_posts.size())
    {
        const auto& p = m_posts[-10 - m_selBox];
        m_viewport.setGizmoTarget({p.x, p.y, p.z}, true);
        m_viewport.setGizmoCanRotateScale(false, false);
    }
    else
        m_viewport.setGizmoTarget({0,0,0}, false);
}

// ── draw ─────────────────────────────────────────────────────────────────────
void MapEditor::draw()
{
    float totalW = ImGui::GetContentRegionAvail().x;
    float totalH = ImGui::GetContentRegionAvail().y;

    drawToolbar();

    float toolbarH = ImGui::GetItemRectSize().y + ImGui::GetStyle().ItemSpacing.y;
    float remaining = totalH - toolbarH - 4.0f;

    float listW   = 200.0f;
    float propW   = 220.0f;
    float vpW     = totalW - listW - propW - ImGui::GetStyle().ItemSpacing.x * 2;

    ImGui::BeginChild("##map_panels", ImVec2(totalW, remaining), false);

    // ── Lista box ────────────────────────────────────────────────────────
    ImGui::BeginChild("##box_list", ImVec2(listW, 0), true);
    drawBoxList(listW, remaining);
    ImGui::EndChild();

    ImGui::SameLine();

    // ── Viewport 3D ──────────────────────────────────────────────────────
    ImGui::BeginChild("##map_vp", ImVec2(vpW, 0), false,
                      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    drawViewport(vpW, remaining);
    ImGui::EndChild();

    ImGui::SameLine();

    // ── Proprietà ────────────────────────────────────────────────────────
    ImGui::BeginChild("##box_props", ImVec2(propW, 0), true);
    drawProperties(propW, remaining);
    ImGui::EndChild();

    ImGui::EndChild();
}

// ── drawToolbar ───────────────────────────────────────────────────────────────
void MapEditor::drawToolbar()
{
    // Selettore mappa
    ImGui::SetNextItemWidth(140.0f);
    if (ImGui::BeginCombo("##mapsel", m_mapId.empty() ? "-- nessuna --" : m_mapId.c_str()))
    {
        for (auto& me : m_mapList)
        {
            bool sel = (me.id == m_mapId);
            if (ImGui::Selectable(me.id.c_str(), sel))
                loadMap(me.id);
            if (sel) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Mappa corrente");

    ImGui::SameLine();
    if (m_dirty) ImGui::TextColored({1.0f,0.7f,0.2f,1.0f}, "*");
    else         ImGui::TextDisabled(" ");
    ImGui::SameLine();

    if (ImGui::Button("Salva")) {
        if (saveMap()) ImGui::OpenPopup("##saved_ok");
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Salva geometry in JSON");

    ImGui::SameLine();
    if (ImGui::Button("+ Box"))         addBox();
    ImGui::SameLine();
    if (ImGui::Button("Duplica") && m_selBox >= 0) duplicateBox(m_selBox);
    ImGui::SameLine();
    if (ImGui::Button("Elimina") && m_selBox >= 0) {
        ImGui::OpenPopup("##del_confirm");
    }

    ImGui::SameLine(0, 16);
    ImGui::TextDisabled("|");
    ImGui::SameLine(0, 16);

    ImGui::TextUnformatted("Snap:");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(60.0f);
    const float snapValues[] = {0.0f, 0.25f, 0.5f, 1.0f, 2.0f};
    const char* snapLabels[] = {"Off","0.25","0.5","1.0","2.0"};
    int snapIdx = 2;
    for (int i = 0; i < 5; ++i) if (m_gridSnap == snapValues[i]) { snapIdx = i; break; }
    if (ImGui::BeginCombo("##snap", snapLabels[snapIdx], ImGuiComboFlags_NoArrowButton))
    {
        for (int i = 0; i < 5; ++i) {
            bool s = (i == snapIdx);
            if (ImGui::Selectable(snapLabels[i], s)) m_gridSnap = snapValues[i];
            if (s) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }

    ImGui::SameLine(0, 16);
    ImGui::Checkbox("Area navigabile", &m_showNavmesh);
    if (m_showNavmesh) { updateViewport(); } // aggiorna colori floor

    ImGui::SameLine(0, 16);
    ImGui::TextDisabled("|");
    ImGui::SameLine(0, 16);
    // Modalità gizmo: gli spawn point supportano solo Sposta.
    const bool boxSel = (m_selBox >= 0 && m_selBox < (int)m_boxes.size());
    editor::ui::gizmoModeBar(m_viewport, boxSel, boxSel);

    // Popups
    if (ImGui::BeginPopup("##saved_ok")) {
        ImGui::TextColored({0.4f,1.0f,0.4f,1.0f}, "Salvato!");
        ImGui::EndPopup();
    }
    if (ImGui::BeginPopupModal("##del_confirm", nullptr,
                               ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Eliminare il box selezionato?");
        if (ImGui::Button("Sì")) { deleteBox(m_selBox); ImGui::CloseCurrentPopup(); }
        ImGui::SameLine();
        if (ImGui::Button("No")) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }
}

// ── drawBoxList ───────────────────────────────────────────────────────────────
void MapEditor::drawBoxList(float /*panelW*/, float /*panelH*/)
{
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

    for (int i = 0; i < (int)m_boxes.size(); ++i)
    {
        const auto& b = m_boxes[i];
        char buf[128];
        const char* name = (b.label[0] != '\0') ? b.label : "(nessun nome)";
        std::snprintf(buf, sizeof(buf), "%s %s##box%d", typeIcon(b.type), name, i);

        bool sel = (i == m_selBox);
        if (ImGui::Selectable(buf, sel))
        {
            m_selBox = i;
            updateViewport();
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("tipo: %s\npos: (%.1f, %.1f, %.1f)\ndim: %.1fx%.1fx%.1f",
                              b.type, b.x, b.y, b.z, b.sx, b.sy, b.sz);
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
        m_posts.push_back(p);
        m_selBox = -10 - ((int)m_posts.size() - 1);
        m_dirty = true;
        updateViewport();
    }
    ImGui::SameLine();
    if (ImGui::SmallButton("- Rimuovi##cp") && m_selBox <= -10)
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
}

// ── drawProperties ────────────────────────────────────────────────────────────
void MapEditor::drawProperties(float panelW, float /*panelH*/)
{
    float sliderW = panelW - 16.0f;

    if (m_selBox == -2 || m_selBox == -3)
    {
        // Spawn point
        auto& sp = (m_selBox == -2) ? m_spawnTeam1 : m_spawnTeam2;
        const char* teamName = (m_selBox == -2) ? "Spawn Alleati (T1)" : "Spawn Nemici (T2)";
        ImGui::TextColored({0.8f,0.8f,0.2f,1.0f}, "%s", teamName);
        ImGui::Separator();
        bool changed = false;
        changed |= editor::ui::sliderRow("X", sp[0], -60.f, 60.f, 0.1f, "%.2f", 18.0f);
        changed |= editor::ui::sliderRow("Y", sp[1],   0.f,  5.f, 0.05f, "%.2f", 18.0f);
        changed |= editor::ui::sliderRow("Z", sp[2], -60.f, 60.f, 0.1f, "%.2f", 18.0f);
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
        if (ImGui::InputText("Nome##cpl", p.label, sizeof(p.label))) changed = true;

        const char* teams[] = {"Neutrale", "Alleati (T1)", "Nemici (T2)"};
        ImGui::SetNextItemWidth(sliderW);
        if (ImGui::Combo("Team iniziale##cpt", &p.team, teams, 3)) changed = true;

        ImGui::TextDisabled("Posizione");
        changed |= editor::ui::sliderRow("X", p.x, -60.f, 60.f, 0.1f, "%.2f", 18.0f);
        changed |= editor::ui::sliderRow("Y", p.y, -2.f, 10.f, 0.05f, "%.2f", 18.0f);
        changed |= editor::ui::sliderRow("Z", p.z, -60.f, 60.f, 0.1f, "%.2f", 18.0f);

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
    if (ImGui::InputText("Etichetta", b.label, sizeof(b.label)))
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
    ImGui::TextDisabled("Posizione");
    const float posSpeed = m_gridSnap > 0 ? m_gridSnap : 0.1f;
    if (editor::ui::sliderRow("X", b.x, -60.f, 60.f, posSpeed, "%.2f", 18.0f))
        { b.x = snap(b.x); changed = true; }
    if (editor::ui::sliderRow("Y", b.y, -2.f, 10.f, posSpeed * 0.5f, "%.2f", 18.0f))
        { b.y = snap(b.y); changed = true; }
    if (editor::ui::sliderRow("Z", b.z, -60.f, 60.f, posSpeed, "%.2f", 18.0f))
        { b.z = snap(b.z); changed = true; }

    ImGui::TextDisabled("Rotazione");
    if (editor::ui::sliderRow("Y°", b.ry, -180.f, 180.f, 1.0f, "%.1f", 18.0f))
        changed = true;

    ImGui::Separator();
    ImGui::TextDisabled("Dimensioni");
    if (editor::ui::sliderRow("W", b.sx, 0.1f, 120.f, posSpeed, "%.2f", 18.0f))
        { b.sx = snap(b.sx); if (b.sx < 0.1f) b.sx = 0.1f; changed = true; }
    if (editor::ui::sliderRow("H", b.sy, 0.1f, 20.f, posSpeed * 0.5f, "%.2f", 18.0f))
        { b.sy = snap(b.sy); if (b.sy < 0.1f) b.sy = 0.1f; changed = true; }
    if (editor::ui::sliderRow("D", b.sz, 0.1f, 120.f, posSpeed, "%.2f", 18.0f))
        { b.sz = snap(b.sz); if (b.sz < 0.1f) b.sz = 0.1f; changed = true; }

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
    m_viewport.draw(false);
    (void)vpW; (void)vpH;
}

} // namespace editor
