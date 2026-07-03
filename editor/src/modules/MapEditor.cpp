// MapEditor.cpp
// Modulo GFEditor per la modifica visuale della geometria delle mappe.
// Layout: [lista box | viewport 3D | pannello proprietà]
// Salva/carica da data/maps/<id>.json, campo "geometry".

#include "modules/MapEditor.hpp"
#include "util/FileDialog.hpp"

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

    // Gizmo a 3 frecce: applica lo spostamento all'elemento selezionato.
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
        m_dirty = true;
        updateViewport();
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

    m_viewport.setMapBoxes(draws);

    // Gizmo a 3 frecce sull'elemento selezionato (box o spawn point).
    if (m_selBox >= 0 && m_selBox < (int)m_boxes.size())
    {
        const auto& b = m_boxes[m_selBox];
        m_viewport.setGizmoTarget({b.x, b.y, b.z}, true);
    }
    else if (m_selBox == -2)
        m_viewport.setGizmoTarget({m_spawnTeam1[0], m_spawnTeam1[1], m_spawnTeam1[2]}, true);
    else if (m_selBox == -3)
        m_viewport.setGizmoTarget({m_spawnTeam2[0], m_spawnTeam2[1], m_spawnTeam2[2]}, true);
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
        ImGui::SetNextItemWidth(sliderW);
        bool changed = false;
        changed |= ImGui::DragFloat("X##spx", &sp[0], 0.1f, -60.f, 60.f, "%.2f");
        ImGui::SetNextItemWidth(sliderW);
        changed |= ImGui::DragFloat("Y##spy", &sp[1], 0.1f,   0.f,  5.f, "%.2f");
        ImGui::SetNextItemWidth(sliderW);
        changed |= ImGui::DragFloat("Z##spz", &sp[2], 0.1f, -25.f, 25.f, "%.2f");
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
    ImGui::SetNextItemWidth(sliderW);
    if (ImGui::DragFloat("X##bx", &b.x, m_gridSnap > 0 ? m_gridSnap : 0.1f, -60.f, 60.f, "%.2f"))
        { b.x = snap(b.x); changed = true; }
    ImGui::SetNextItemWidth(sliderW);
    if (ImGui::DragFloat("Y##by", &b.y, m_gridSnap > 0 ? m_gridSnap : 0.05f, -2.f, 10.f, "%.2f"))
        { b.y = snap(b.y); changed = true; }
    ImGui::SetNextItemWidth(sliderW);
    if (ImGui::DragFloat("Z##bz", &b.z, m_gridSnap > 0 ? m_gridSnap : 0.1f, -25.f, 25.f, "%.2f"))
        { b.z = snap(b.z); changed = true; }

    ImGui::SetNextItemWidth(sliderW);
    if (ImGui::DragFloat("Rot Y##bry", &b.ry, 1.0f, -180.f, 180.f, "%.1f°"))
        changed = true;

    ImGui::Separator();
    ImGui::TextDisabled("Dimensioni");
    ImGui::SetNextItemWidth(sliderW);
    if (ImGui::DragFloat("W (X)##bsx", &b.sx, m_gridSnap > 0 ? m_gridSnap : 0.1f, 0.1f, 120.f, "%.2f"))
        { b.sx = snap(b.sx); if (b.sx < 0.1f) b.sx = 0.1f; changed = true; }
    ImGui::SetNextItemWidth(sliderW);
    if (ImGui::DragFloat("H (Y)##bsy", &b.sy, m_gridSnap > 0 ? m_gridSnap : 0.05f, 0.1f, 20.f, "%.2f"))
        { b.sy = snap(b.sy); if (b.sy < 0.1f) b.sy = 0.1f; changed = true; }
    ImGui::SetNextItemWidth(sliderW);
    if (ImGui::DragFloat("D (Z)##bsz", &b.sz, m_gridSnap > 0 ? m_gridSnap : 0.1f, 0.1f, 120.f, "%.2f"))
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
    // Hint TAB
    ImGui::TextDisabled("TAB = cattura mouse | WASD/QE = vola | Esc = ritorna");
    float hintH = ImGui::GetItemRectSize().y + ImGui::GetStyle().ItemSpacing.y;

    m_viewport.draw(false);
    (void)vpW; (void)vpH; (void)hintH;
}

} // namespace editor
