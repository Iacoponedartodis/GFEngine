#include "modules/HitboxEditor.hpp"
#include <imgui.h>
#include <nlohmann/json.hpp>
#include <fstream>
#include <iostream>
#include <cstring>
#include <cmath>
#include <algorithm>

using json = nlohmann::json;

namespace editor
{

HitboxEditor::HitboxEditor() { reload(); }

void HitboxEditor::reload()
{
    m_registry.loadAll("data");
    m_selProfile.clear();
    m_selZone = -1;
    m_edit = {};
    m_dirty = false;
}

void HitboxEditor::saveProfile(const mini::HitboxProfile& p)
{
    std::string path = "data/hitboxes/" + p.profileId + ".json";
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
    if (!f.is_open()) { std::cerr << "[Hitbox] Cannot write: " << path << "\n"; return; }
    f << j.dump(4) << "\n";
    std::cout << "[Hitbox] Saved: " << path << "\n";
    m_dirty = false;
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
    ImGui::Text("Profili hitbox:");
    ImGui::BeginChild("##hlist", ImVec2(160, 0), true);
    for (auto& [id, p] : m_registry.hitboxProfiles())
    {
        bool sel = (id == m_selProfile);
        if (ImGui::Selectable(id.c_str(), sel))
        {
            m_selProfile = id;
            m_selZone = -1;
            // Copia profilo per editare (dobbiamo creare copia non-const)
            m_edit.profileId = p.profileId;
            m_edit.zones = p.zones;
            m_dirty = false;
        }
    }
    ImGui::EndChild();
}

void HitboxEditor::drawZoneList()
{
    ImGui::Text("Zone (%d):", (int)m_edit.zones.size());
    ImGui::BeginChild("##zlist", ImVec2(160, 0), true);
    for (int i = 0; i < (int)m_edit.zones.size(); ++i)
    {
        auto& z = m_edit.zones[i];
        // Colore in base al moltiplicatore
        float mult = z.damageMultiplier;
        ImVec4 col = (mult >= 2.0f) ? ImVec4(1,0.3f,0.3f,1) :   // rosso = critico
                     (mult >= 1.0f) ? ImVec4(1,0.85f,0.3f,1) :   // giallo = normale
                                      ImVec4(0.6f,0.6f,1,1);      // blu = ridotto
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
    // Legenda moltiplicatori
    ImGui::TextColored({1,0.3f,0.3f,1}, "  >= 2.0 = critico (testa)");
    ImGui::TextColored({1,0.85f,0.3f,1},"  1.0    = normale (torso)");
    ImGui::TextColored({0.6f,0.6f,1,1}, "  < 1.0  = ridotto (braccia/gambe)");
}

void HitboxEditor::drawVisualPreview()
{
    // Vista frontale 2D delle zone come rettangoli colorati
    const float scale = 120.0f;  // pixel per metro
    const float pivotX = 100.0f;
    const float pivotY = 200.0f; // base del personaggio in pixel

    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 p = ImGui::GetCursorScreenPos();

    // Sagoma personaggio (rettangolo grigio)
    dl->AddRect(
        {p.x + pivotX - 0.25f*scale, p.y + pivotY - 2.0f*scale},
        {p.x + pivotX + 0.25f*scale, p.y + pivotY},
        IM_COL32(80,80,80,120), 0, 0, 1.5f);

    for (int i = 0; i < (int)m_edit.zones.size(); ++i)
    {
        auto& z = m_edit.zones[i];
        float cx = p.x + pivotX + z.offset.x * scale;
        float cy = p.y + pivotY - z.offset.y * scale; // Y invertita
        float hw = z.halfExtents.x * scale;
        float hh = z.halfExtents.y * scale;

        bool sel = (i == m_selZone);
        float mult = z.damageMultiplier;
        ImU32 fill = (mult >= 2.0f) ? IM_COL32(220,60,60,sel?180:90) :
                     (mult >= 1.0f) ? IM_COL32(220,180,60,sel?180:90) :
                                      IM_COL32(80,120,220,sel?180:90);
        ImU32 border = sel ? IM_COL32(255,255,255,255) : IM_COL32(180,180,180,160);

        dl->AddRectFilled({cx-hw, cy-hh}, {cx+hw, cy+hh}, fill);
        dl->AddRect      ({cx-hw, cy-hh}, {cx+hw, cy+hh}, border, 0, 0, sel?2.0f:1.0f);

        // Label zona
        dl->AddText({cx-hw+2, cy-8}, IM_COL32(255,255,255,200), z.name.c_str());

        // Click per selezionare zona dalla preview
        if (ImGui::IsMouseClicked(0))
        {
            ImVec2 mp = ImGui::GetMousePos();
            if (mp.x>=cx-hw && mp.x<=cx+hw && mp.y>=cy-hh && mp.y<=cy+hh)
                m_selZone = i;
        }
    }

    // Asse Y (linea verde)
    dl->AddLine({p.x+pivotX, p.y+pivotY-2.2f*scale},
                {p.x+pivotX, p.y+pivotY+0.1f*scale},
                IM_COL32(80,200,80,160), 1.0f);
    // Ticks altezza
    for (int y = 0; y <= 2; ++y) {
        float py = p.y + pivotY - y * scale;
        dl->AddLine({p.x+pivotX-8, py}, {p.x+pivotX+8, py}, IM_COL32(80,200,80,100));
        char buf[8]; std::snprintf(buf,8,"%dm",y);
        dl->AddText({p.x+pivotX+12, py-8}, IM_COL32(80,200,80,180), buf);
    }

    ImGui::Dummy(ImVec2(pivotX*2, pivotY+20));
}

void HitboxEditor::draw()
{
    // ── Colonna sinistra: lista profili ──────────────────────────────
    drawProfileList();
    ImGui::SameLine();

    if (m_selProfile.empty())
    {
        ImGui::TextDisabled("Seleziona un profilo dalla lista.");
        return;
    }

    // ── Colonna centrale: lista zone + proprietà ──────────────────────
    ImGui::BeginGroup();
    drawZoneList();
    ImGui::Separator();
    drawZoneProperties();
    ImGui::EndGroup();

    ImGui::SameLine();

    // ── Colonna destra: preview 2D ───────────────────────────────────
    ImGui::BeginChild("##hpreview", ImVec2(230, 0), true);
    ImGui::Text("Vista frontale");
    ImGui::Separator();
    drawVisualPreview();
    ImGui::EndChild();

    // ── Toolbar salvataggio ───────────────────────────────────────────
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