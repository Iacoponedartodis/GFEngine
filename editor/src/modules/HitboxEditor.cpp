#include "modules/HitboxEditor.hpp"
#include <imgui.h>
#include <nlohmann/json.hpp>
#include <SDL2/SDL.h>
#include <fstream>
#include <iostream>
#include <cstring>
#include <cmath>
#include <algorithm>
#include <string>

using json = nlohmann::json;

#include <filesystem>
namespace fs = std::filesystem;

namespace editor
{

static std::string getSourceDataDir()
{
    char* base = SDL_GetBasePath();
    fs::path exeDir = base ? base : ".";
    SDL_free(base);
    std::error_code ec;
    fs::path sourceData = fs::canonical(exeDir / "../../../data", ec);
    if (!ec && fs::exists(sourceData / "hitboxes", ec))
        return sourceData.string() + "/";
    return (exeDir / "data").string() + "/";
}

HitboxEditor::HitboxEditor() { reload(); }

void HitboxEditor::reload()
{
    m_registry.loadAll(getSourceDataDir());
    m_selProfile.clear();
    m_selZone = -1;
    m_edit = {};
    m_dirty = false;
    std::cout << "[Hitbox] Dati caricati da: " << getSourceDataDir() << "\n";
}

void HitboxEditor::saveProfile(const mini::HitboxProfile& p)
{
    std::string path = getSourceDataDir() + "hitboxes/" + p.profileId + ".json";
    json j;
    j["profile_id"] = p.profileId;
    json zones = json::array();
    for (auto& z : p.zones)
    {
        json jz;
        jz["name"]              = z.name;
        jz["offset"]            = {z.offset.x, z.offset.y, z.offset.z};
        jz["half_extents"]      = {z.halfExtents.x, z.halfExtents.y, z.halfExtents.z};
        jz["damage_multiplier"] = z.damageMultiplier;
        jz["debug_visible"]     = z.debugVisible;
        zones.push_back(jz);
    }
    j["zones"] = zones;
    std::ofstream f(path);
    if (!f.is_open())
    {
        std::cerr << "[Hitbox] ERRORE scrittura: " << path << "\n";
        return;
    }
    f << j.dump(4) << "\n";
    f.close(); // flush e chiudi prima del reload
    std::cout << "[Hitbox] Salvato: " << path << "\n";
    m_dirty = false;
    m_registry.reload(getSourceDataDir());
}

void HitboxEditor::addZone()
{
    mini::HitZone z;
    z.name             = "nuova_zona";
    z.offset           = {0, 1.0f, 0};
    z.halfExtents      = {0.15f, 0.2f, 0.15f};
    z.damageMultiplier = 1.0f;
    m_edit.zones.push_back(z);
    m_selZone = (int)m_edit.zones.size() - 1;
    m_dirty = true;
}

void HitboxEditor::removeZone(int idx)
{
    if (idx < 0 || idx >= (int)m_edit.zones.size()) return;
    m_edit.zones.erase(m_edit.zones.begin() + idx);
    m_selZone = std::min(m_selZone, (int)m_edit.zones.size()-1);
    m_dirty = true;
}

void HitboxEditor::duplicateZone(int idx)
{
    if (idx < 0 || idx >= (int)m_edit.zones.size()) return;
    mini::HitZone copy = m_edit.zones[idx];
    copy.name += "_copy";
    m_edit.zones.insert(m_edit.zones.begin() + idx + 1, copy);
    m_selZone = idx + 1;
    m_dirty = true;
}

void HitboxEditor::drawProfileList()
{
    // BeginGroup fa sì che SameLine() in draw() sia relativo all'intera colonna sinistra
    ImGui::BeginGroup();
    ImGui::Text("Profili hitbox:");
    // Altezza -50 lascia spazio al campo "nuovo profilo" sotto la lista
    ImGui::BeginChild("##hlist", ImVec2(160, -50), true);
    for (auto& [id, p] : m_registry.hitboxProfiles())
    {
        bool sel = (id == m_selProfile);
        if (ImGui::Selectable(id.c_str(), sel))
        {
            m_selProfile = id;
            m_selZone = -1;
            m_edit.profileId = p.profileId;
            m_edit.zones = p.zones;
            m_dirty = false;
        }
    }
    ImGui::EndChild();

    // Crea nuovo profilo (sotto la lista, dentro il group)
    static char newHId[64] = "";
    ImGui::SetNextItemWidth(120); ImGui::InputText("##newhid", newHId, 64); ImGui::SameLine();
    if (ImGui::Button("+ Nuovo") && newHId[0] != '\0')
    {
        std::string path = getSourceDataDir() + "hitboxes/" + newHId + ".json";
        if (!fs::exists(path))
        {
            mini::HitboxProfile p2;
            p2.profileId = newHId;
            saveProfile(p2);
            m_selProfile = newHId;
            m_edit = p2;
            m_selZone = -1;
        }
        newHId[0] = '\0';
    }
    ImGui::EndGroup(); // chiude la colonna sinistra
}

void HitboxEditor::drawZoneList()
{
    ImGui::Text("Zone (%d):", (int)m_edit.zones.size());
    ImGui::BeginChild("##zlist", ImVec2(160, 0), true);
    for (int i = 0; i < (int)m_edit.zones.size(); ++i)
    {
        auto& z = m_edit.zones[i];
        float mult = z.damageMultiplier;
        ImVec4 col = (mult >= 2.0f) ? ImVec4(1,0.3f,0.3f,1) :
                     (mult >= 1.0f) ? ImVec4(1,0.85f,0.3f,1) :
                                      ImVec4(0.6f,0.6f,1,1);
        ImGui::PushStyleColor(ImGuiCol_Text, col);
        bool sel = (i == m_selZone);
        char lbl[64]; std::snprintf(lbl, 64, "%s  x%.1f", z.name.c_str(), mult);
        if (ImGui::Selectable(lbl, sel)) m_selZone = i;
        ImGui::PopStyleColor();
    }
    ImGui::EndChild();

    if (ImGui::Button("+", {50,0})) addZone();
    ImGui::SameLine();
    if (ImGui::Button("-", {50,0}) && m_selZone >= 0) removeZone(m_selZone);
    ImGui::SameLine();
    if (ImGui::Button("Dup", {50,0}) && m_selZone >= 0) duplicateZone(m_selZone);
}

void HitboxEditor::drawZoneProperties()
{
    if (m_selZone < 0 || m_selZone >= (int)m_edit.zones.size())
    { ImGui::TextDisabled("Seleziona una zona."); return; }

    auto& z = m_edit.zones[m_selZone];

    ImGui::Text("Proprietà zona:");
    ImGui::Separator();

    char nameBuf[64]; std::strncpy(nameBuf, z.name.c_str(), 63);
    if (ImGui::InputText("Nome", nameBuf, 64))
    { z.name = nameBuf; m_dirty = true; }

    float off[3] = {z.offset.x, z.offset.y, z.offset.z};
    if (ImGui::DragFloat3("Offset", off, 0.01f, -5.0f, 5.0f, "%.3f"))
    { z.offset = {off[0],off[1],off[2]}; m_dirty = true; }

    float he[3] = {z.halfExtents.x, z.halfExtents.y, z.halfExtents.z};
    if (ImGui::DragFloat3("Half Extents", he, 0.005f, 0.01f, 2.0f, "%.3f"))
    { z.halfExtents = {he[0],he[1],he[2]}; m_dirty = true; }

    if (ImGui::DragFloat("Moltiplicatore danno", &z.damageMultiplier,
                          0.05f, 0.1f, 5.0f, "x%.2f"))
        m_dirty = true;

    ImGui::Separator();
    ImGui::TextColored({1,0.3f,0.3f,1}, "  >= 2.0 = critico (testa)");
    ImGui::TextColored({1,0.85f,0.3f,1},"  1.0    = normale (torso)");
    ImGui::TextColored({0.6f,0.6f,1,1}, "  < 1.0  = ridotto (braccia/gambe)");
}

// Disegna una singola vista 2D dell'hitbox.
// ax/ay = assi dello spazio 3D proiettati sullo schermo (0=X, 1=Y, 2=Z).
// ox/ay = asse verticale (Y sullo schermo = -ay nell'asse scelto).
static void drawHitboxView(ImDrawList* dl, ImVec2 origin,
                            float scale, float viewH,
                            int axH, int axV, // assi 3D → orizzontale/verticale schermo
                            const mini::HitboxProfile& prof,
                            int selZone,
                            int& outSel)
{
    // Silhouette corpo: rettangolo 0.5m largo, 2m alto
    const float bw = 0.25f * scale;
    const float bh = 2.0f  * scale;
    const float cx = origin.x;
    const float cy = origin.y;
    dl->AddRect({cx - bw, cy - bh}, {cx + bw, cy}, IM_COL32(80,80,80,100), 0, 0, 1.5f);

    // Asse verticale (griglia)
    dl->AddLine({cx, cy - viewH}, {cx, cy + 10}, IM_COL32(80,200,80,120), 1.0f);
    for (int m = 0; m <= 2; ++m)
    {
        float py = cy - m * scale;
        dl->AddLine({cx - 6, py}, {cx + 6, py}, IM_COL32(80,200,80,80));
        char buf[8]; std::snprintf(buf, 8, "%dm", m);
        dl->AddText({cx + 10, py - 7}, IM_COL32(80,200,80,160), buf);
    }

    // Zone
    const glm::vec3 axH_v = (axH == 0) ? glm::vec3{1,0,0} :
                             (axH == 2) ? glm::vec3{0,0,1} : glm::vec3{0,1,0};
    const glm::vec3 axV_v = (axV == 1) ? glm::vec3{0,1,0} :
                             (axV == 0) ? glm::vec3{1,0,0} : glm::vec3{0,0,1};
    (void)axH_v; (void)axV_v;

    for (int i = 0; i < (int)prof.zones.size(); ++i)
    {
        const auto& z = prof.zones[i];
        float offH = (axH == 0) ? z.offset.x : (axH == 2) ? z.offset.z : z.offset.y;
        float offV = (axV == 1) ? z.offset.y : (axV == 0) ? z.offset.x : z.offset.z;
        float heH  = (axH == 0) ? z.halfExtents.x : (axH == 2) ? z.halfExtents.z : z.halfExtents.y;
        float heV  = (axV == 1) ? z.halfExtents.y : (axV == 0) ? z.halfExtents.x : z.halfExtents.z;

        float sx = cx + offH * scale;
        float sy = cy - offV * scale;
        float hw = heH * scale;
        float hh = heV * scale;

        bool sel = (i == selZone);
        float mult = z.damageMultiplier;
        ImU32 fill = (mult >= 2.0f) ? IM_COL32(220,60,60,sel?180:90) :
                     (mult >= 1.0f) ? IM_COL32(220,180,60,sel?180:90) :
                                      IM_COL32(80,120,220,sel?180:90);
        ImU32 border = sel ? IM_COL32(255,255,255,255) : IM_COL32(180,180,180,160);

        dl->AddRectFilled({sx-hw, sy-hh}, {sx+hw, sy+hh}, fill);
        dl->AddRect      ({sx-hw, sy-hh}, {sx+hw, sy+hh}, border, 0, 0, sel?2.0f:1.0f);
        if (sel) dl->AddText({sx-hw+2, sy-8}, IM_COL32(255,255,255,220), z.name.c_str());

        if (ImGui::IsMouseClicked(0))
        {
            ImVec2 mp = ImGui::GetMousePos();
            if (mp.x >= sx-hw && mp.x <= sx+hw && mp.y >= sy-hh && mp.y <= sy+hh)
                outSel = i;
        }
    }
}

void HitboxEditor::drawVisualPreview()
{
    const float scale  = 90.0f;
    const float viewH  = 2.2f * scale;
    const float panelW = 160.0f;
    const float panelH = viewH + 20.0f;

    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 base = ImGui::GetCursorScreenPos();

    struct ViewDef { const char* label; int axH, axV; float offX; };
    const ViewDef views[3] = {
        { "Frontale (X/Y)",   0, 1,  0.0f  },
        { "Laterale (Z/Y)",   2, 1,  panelW + 8.0f },
        { "Superiore (X/Z)",  0, 2,  (panelW + 8.0f) * 2.0f }
    };

    for (auto& v : views)
    {
        float ox = base.x + v.offX + panelW * 0.5f;
        float oy = base.y + viewH;
        ImGui::GetWindowDrawList(); // assicura draw list valida

        // Label
        ImVec2 lblPos = {base.x + v.offX + 4, base.y + 2};
        dl->AddText(lblPos, IM_COL32(180,180,180,200), v.label);

        // Bordo pannello
        dl->AddRect({base.x + v.offX, base.y},
                    {base.x + v.offX + panelW, base.y + panelH},
                    IM_COL32(60,60,60,180), 3.0f);

        // Vista
        drawHitboxView(dl, {ox, oy}, scale, viewH,
                       v.axH, v.axV, m_edit, m_selZone, m_selZone);
    }

    ImGui::Dummy(ImVec2((panelW + 8.0f) * 3.0f, panelH + 8.0f));
}

void HitboxEditor::draw()
{
    drawProfileList();
    ImGui::SameLine();

    if (m_selProfile.empty())
    {
        ImGui::TextDisabled("Seleziona un profilo dalla lista.");
        return;
    }

    ImGui::BeginGroup();
    drawZoneList();
    ImGui::Separator();
    drawZoneProperties();
    ImGui::EndGroup();

    ImGui::SameLine();

    ImGui::BeginChild("##hpreview", ImVec2(520, 0), true);
    ImGui::Text("Anteprima 3 viste");
    ImGui::Separator();
    drawVisualPreview();
    ImGui::EndChild();

    ImGui::Separator();
    if (m_dirty)
        ImGui::TextColored({1,0.7f,0.2f,1}, "* Modifiche non salvate");
    else
        ImGui::TextDisabled("Nessuna modifica pendente");
    ImGui::SameLine(300);
    if (ImGui::Button("Salva JSON", {120,0}))
        saveProfile(m_edit);
    ImGui::SameLine();
    if (ImGui::Button("Ripristina", {120,0}))
    {
        if (auto* p = m_registry.getHitboxProfile(m_selProfile))
        { m_edit.zones = p->zones; m_dirty = false; }
    }
    ImGui::SameLine();
    if (ImGui::Button("Ricarica tutto", {120,0}))
        reload();
}

} // namespace editor