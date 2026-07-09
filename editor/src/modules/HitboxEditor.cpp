#include "modules/HitboxEditor.hpp"
#include "util/FileDialog.hpp"
#include "util/UiWidgets.hpp"
#include <imgui.h>
#include <nlohmann/json.hpp>
#include <SDL2/SDL.h>
#include <fstream>
#include <iostream>
#include <cstring>
#include <cctype>
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

static std::string hbGetExeDir()
{
    char* base = SDL_GetBasePath();
    std::string d = base ? base : "./";
    SDL_free(base);
    return d;
}

static std::string hbGetAssetsDir()
{
    std::error_code ec;
    fs::path assetsDir = fs::canonical(fs::path(hbGetExeDir()) / "../../../assets", ec);
    if (!ec && fs::exists(assetsDir)) return assetsDir.string();
    return hbGetExeDir();
}

// Risolve un campo mesh ("assets/...") in percorso assoluto.
static std::string hbResolveAssetPath(const std::string& meshField)
{
    if (meshField.empty()) return {};
    fs::path exeDir = hbGetExeDir();
    std::error_code ec;
    if (meshField.size() >= 7 && meshField.substr(0,7) == "assets/")
    {
        fs::path cand = fs::canonical(exeDir / "../../../" / meshField, ec);
        if (!ec && fs::exists(cand)) return cand.string();
    }
    fs::path cand = fs::canonical(exeDir / meshField, ec);
    if (!ec && fs::exists(cand)) return cand.string();
    return {};
}

// Apre un dialog per scegliere un modello, restituendo un percorso relativo
// "assets/..." quando possibile.
static bool hbBrowseModel(std::string& outRel)
{
    std::string abs = openFileDialog(
        "Modelli 3D\0*.glb;*.gltf;*.obj\0Tutti i file\0*.*\0\0",
        hbGetAssetsDir());
    if (abs.empty()) return false;
    auto toFwd = [](std::string s){ for (auto& c : s) if (c=='\\') c='/'; return s; };
    std::string absF = toFwd(abs);
    std::error_code ec;
    fs::path projRoot = fs::canonical(fs::path(hbGetExeDir()) / "../../..", ec);
    if (!ec) {
        std::string pr = toFwd(projRoot.string()) + "/";
        if (absF.rfind(pr, 0) == 0) { outRel = absF.substr(pr.size()); return true; }
    }
    outRel = absF;
    return true;
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
        jz["bone"]              = z.boneName;
        jz["rotation"]          = {z.eulerDeg.x, z.eulerDeg.y, z.eulerDeg.z};
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
    syncViewport();
}

void HitboxEditor::removeZone(int idx)
{
    if (idx < 0 || idx >= (int)m_edit.zones.size()) return;
    m_edit.zones.erase(m_edit.zones.begin() + idx);
    m_selZone = std::min(m_selZone, (int)m_edit.zones.size()-1);
    m_dirty = true;
    syncViewport();
}

void HitboxEditor::duplicateZone(int idx)
{
    if (idx < 0 || idx >= (int)m_edit.zones.size()) return;
    mini::HitZone copy = m_edit.zones[idx];
    copy.name += "_copy";
    m_edit.zones.insert(m_edit.zones.begin() + idx + 1, copy);
    m_selZone = idx + 1;
    m_dirty = true;
    syncViewport();
}

// ── Viewport 3D ────────────────────────────────────────────────────────────────
void HitboxEditor::tick(float dt)
{
    m_viewport.tick(dt);

    const bool zoneSel = (m_selZone >= 0 && m_selZone < (int)m_edit.zones.size());

    // Gizmo Sposta
    glm::vec3 delta;
    if (m_viewport.popGizmoDelta(delta) && zoneSel)
    {
        m_edit.zones[m_selZone].offset += delta;
        m_dirty = true;
        syncViewport();
    }

    // Gizmo Ruota (euler XYZ della zona)
    glm::vec3 rotDelta;
    if (m_viewport.popGizmoRotDelta(rotDelta) && zoneSel)
    {
        auto& z = m_edit.zones[m_selZone];
        z.eulerDeg += rotDelta;
        for (int i = 0; i < 3; ++i)
        {
            while (z.eulerDeg[i] >  180.0f) z.eulerDeg[i] -= 360.0f;
            while (z.eulerDeg[i] < -180.0f) z.eulerDeg[i] += 360.0f;
        }
        m_dirty = true;
        syncViewport();
    }

    // Gizmo Scala (half extents)
    glm::vec3 scaleDelta;
    if (m_viewport.popGizmoScaleDelta(scaleDelta) && zoneSel)
    {
        auto& z = m_edit.zones[m_selZone];
        z.halfExtents += scaleDelta * 0.5f; // delta full-size → half extents
        z.halfExtents = glm::max(z.halfExtents, glm::vec3(0.01f));
        m_dirty = true;
        syncViewport();
    }
}

void HitboxEditor::setModel(const std::string& meshField)
{
    m_modelPath = meshField;
    m_joints.clear();
    m_viewport.clearBones();
    m_viewport.clearModel();

    std::string abs = hbResolveAssetPath(meshField);
    if (abs.empty()) { syncViewport(); return; }

    m_viewport.loadModel(abs, 0.0f, 1.0f);

    std::string ext = abs.size() >= 4 ? abs.substr(abs.size()-4) : "";
    for (auto& c : ext) c = (char)std::tolower((unsigned char)c);
    if (ext == ".glb")
    {
        m_joints = readGlbJointData(abs);
        if (!m_joints.empty())
            m_viewport.setBoneData(m_joints, 0.0f, 1.0f);
    }

    // Le zone agganciate a un osso vengono riallineate alla posizione reale
    // del bone del modello caricato (l'osso è la fonte autorevole).
    for (auto& z : m_edit.zones)
        if (!z.boneName.empty())
            for (const auto& jd : m_joints)
                if (jd.name == z.boneName) { z.offset = jd.modelPos; break; }

    syncViewport();
}

// Cerca un'entità (nemico o alleato) che usi questo profilo hitbox e ne carica
// il modello, così le zone vengono mostrate sopra il personaggio reale.
void HitboxEditor::loadModelForProfile()
{
    std::string meshField;
    auto match = [&](const auto& map) {
        for (auto& [id, def] : map)
        {
            if (!meshField.empty()) break;
            bool byProfile = (def.hitboxProfileId == m_selProfile);
            bool byId      = (id == m_selProfile);
            if ((byProfile || byId) && !def.meshPath.empty())
                meshField = def.meshPath;
        }
    };
    match(m_registry.enemies());
    if (meshField.empty()) match(m_registry.allies());

    setModel(meshField);
}

void HitboxEditor::syncViewport()
{
    // Hitbox come wireframe
    m_viewport.setHitboxes(m_edit.zones, m_selZone);

    // Marker dei bone agganciati (evidenzia quelli usati dalle zone)
    m_viewport.setSelectedBone(
        (m_selZone >= 0 && m_selZone < (int)m_edit.zones.size())
            ? m_edit.zones[m_selZone].boneName : std::string());

    // Gizmo a 3 frecce sulla zona selezionata
    if (m_selZone >= 0 && m_selZone < (int)m_edit.zones.size())
        m_viewport.setGizmoTarget(m_edit.zones[m_selZone].offset, true);
    else
        m_viewport.setGizmoTarget({0,0,0}, false);
}

void HitboxEditor::drawViewport()
{
    // Selettore modello
    ImGui::SetNextItemWidth(220);
    char buf[512];
    std::snprintf(buf, sizeof(buf), "%s",
                  m_modelPath.empty() ? "(nessun modello)" : m_modelPath.c_str());
    ImGui::InputText("##modelpath", buf, sizeof(buf), ImGuiInputTextFlags_ReadOnly);
    ImGui::SameLine();
    if (ImGui::Button("Sfoglia modello"))
    {
        std::string rel;
        if (hbBrowseModel(rel)) setModel(rel);
    }
    ImGui::SameLine();
    if (ImGui::Button("Auto")) loadModelForProfile();
    ImGui::SameLine();
    ImGui::Checkbox("Viste 2D", &m_show2DViews);

    // Barra modalità gizmo (zone: tutte le modalità disponibili)
    editor::ui::gizmoModeBar(m_viewport, true, true);

    m_viewport.draw(false);
}

void HitboxEditor::drawProfileList()
{
    ImGui::Text("Profili hitbox");
    ImGui::Separator();
    ImGui::BeginChild("##hlist", ImVec2(0, -64), true);
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
            loadModelForProfile();
        }
    }
    ImGui::EndChild();

    // Crea nuovo profilo
    static char newHId[64] = "";
    ImGui::SetNextItemWidth(-1); ImGui::InputText("##newhid", newHId, 64);
    if (ImGui::Button("+ Nuovo profilo", {-1, 0}) && newHId[0] != '\0')
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
}

void HitboxEditor::drawZoneList()
{
    ImGui::Text("Zone (%d)", (int)m_edit.zones.size());
    ImGui::BeginChild("##zlist", ImVec2(0, 150), true);
    for (int i = 0; i < (int)m_edit.zones.size(); ++i)
    {
        auto& z = m_edit.zones[i];
        float mult = z.damageMultiplier;
        ImVec4 col = (mult >= 2.0f) ? ImVec4(1,0.3f,0.3f,1) :
                     (mult >= 1.0f) ? ImVec4(1,0.85f,0.3f,1) :
                                      ImVec4(0.6f,0.6f,1,1);
        ImGui::PushStyleColor(ImGuiCol_Text, col);
        bool sel = (i == m_selZone);
        char lbl[80];
        std::snprintf(lbl, 80, "%s%s  x%.1f",
                      z.boneName.empty() ? "" : "[B] ", z.name.c_str(), mult);
        if (ImGui::Selectable(lbl, sel)) { m_selZone = i; syncViewport(); }
        ImGui::PopStyleColor();
    }
    ImGui::EndChild();

    float bw = (ImGui::GetContentRegionAvail().x - 8) / 3.0f;
    if (ImGui::Button("+ Zona", {bw,0})) addZone();
    ImGui::SameLine();
    if (ImGui::Button("- Rim.", {bw,0}) && m_selZone >= 0) removeZone(m_selZone);
    ImGui::SameLine();
    if (ImGui::Button("Dup", {bw,0}) && m_selZone >= 0) duplicateZone(m_selZone);
}

void HitboxEditor::drawZoneProperties()
{
    if (m_selZone < 0 || m_selZone >= (int)m_edit.zones.size())
    { ImGui::TextDisabled("Seleziona una zona."); return; }

    auto& z = m_edit.zones[m_selZone];
    bool changed = false;

    ImGui::Text("Proprietà zona:");
    ImGui::Separator();

    char nameBuf[64]; std::strncpy(nameBuf, z.name.c_str(), 63);
    if (ImGui::InputText("Nome", nameBuf, 64))
    { z.name = nameBuf; changed = true; }

    // ── Bone attachment (dropdown dalle ossa del modello) ────────────────
    ImGui::TextDisabled("Osso (bone attachment)");
    auto boneModelPos = [&](const std::string& n, glm::vec3& out) -> bool {
        for (const auto& jd : m_joints)
            if (jd.name == n) { out = jd.modelPos; return true; }
        return false;
    };
    const char* bonePrev = z.boneName.empty() ? "-- nessuno --" : z.boneName.c_str();
    if (ImGui::BeginCombo("Bone##hbbone", bonePrev))
    {
        if (ImGui::Selectable("-- nessuno --", z.boneName.empty()))
        { z.boneName.clear(); changed = true; }
        for (auto& jd : m_joints)
            if (ImGui::Selectable(jd.name.c_str(), z.boneName == jd.name))
            {
                z.boneName = jd.name;
                z.offset   = jd.modelPos; // aggancia la zona al bone
                changed = true;
            }
        ImGui::EndCombo();
    }
    if (m_joints.empty())
        ImGui::TextDisabled("(nessun osso: carica un modello con rig)");
    else if (!z.boneName.empty())
    {
        ImGui::SameLine();
        if (ImGui::SmallButton("Riallinea"))
        {
            glm::vec3 bp{0,0,0};
            if (boneModelPos(z.boneName, bp)) { z.offset = bp; changed = true; }
        }
    }

    ImGui::Spacing();
    ImGui::TextDisabled("Posizione (offset)");
    float off[3] = {z.offset.x, z.offset.y, z.offset.z};
    if (editor::ui::sliderRow3("off", off, -3.0f, 3.0f, 0.01f, "%.3f"))
    { z.offset = {off[0],off[1],off[2]}; changed = true; }

    ImGui::Spacing();
    ImGui::TextDisabled("Dimensioni (half extents)");
    float he[3] = {z.halfExtents.x, z.halfExtents.y, z.halfExtents.z};
    if (editor::ui::sliderRow3("he", he, 0.01f, 2.0f, 0.005f, "%.3f"))
    { z.halfExtents = {he[0],he[1],he[2]}; changed = true; }

    ImGui::Spacing();
    ImGui::TextDisabled("Rotazione locale (gradi)");
    float euler[3] = {z.eulerDeg.x, z.eulerDeg.y, z.eulerDeg.z};
    if (editor::ui::sliderRow3("rot", euler, -180.0f, 180.0f, 1.0f, "%.1f"))
    { z.eulerDeg = {euler[0], euler[1], euler[2]}; changed = true; }

    ImGui::Spacing();
    if (editor::ui::sliderRow("Danno x", z.damageMultiplier, 0.1f, 5.0f, 0.05f, "%.2f"))
        changed = true;

    if (changed) { m_dirty = true; syncViewport(); }

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
    const float bottomH = 36.0f;
    const float colH = ImGui::GetContentRegionAvail().y - bottomH;

    // ── Colonna 1: profili ───────────────────────────────────────────────
    ImGui::BeginChild("##col_prof", ImVec2(190, colH), false);
    drawProfileList();
    ImGui::EndChild();

    ImGui::SameLine();

    // ── Colonna 2: zone + proprietà ──────────────────────────────────────
    ImGui::BeginChild("##col_zone", ImVec2(330, colH), false);
    if (m_selProfile.empty())
        ImGui::TextDisabled("Seleziona un profilo.");
    else
    {
        drawZoneList();
        ImGui::Separator();
        drawZoneProperties();
    }
    ImGui::EndChild();

    ImGui::SameLine();

    // ── Colonna 3: viewport 3D (modello + ossa + hitbox) ─────────────────
    ImGui::BeginChild("##col_view", ImVec2(0, colH), true,
                      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    if (m_selProfile.empty())
        ImGui::TextDisabled("Seleziona un profilo per vedere il modello.");
    else
    {
        drawViewport();
        if (m_show2DViews)
        {
            ImGui::Separator();
            ImGui::Text("Viste 2D");
            drawVisualPreview();
        }
    }
    ImGui::EndChild();

    // ── Barra inferiore ──────────────────────────────────────────────────
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
        { m_edit.zones = p->zones; m_dirty = false; loadModelForProfile(); }
    }
    ImGui::SameLine();
    if (ImGui::Button("Ricarica tutto", {120,0}))
        reload();
}

} // namespace editor