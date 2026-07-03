// WeaponEditor.cpp
#include "modules/WeaponEditor.hpp"
#include "util/FileDialog.hpp"
#include "util/RigReader.hpp"

#include <imgui.h>
#include <SDL2/SDL.h>
#include <nlohmann/json.hpp>

#include <fstream>
#include <filesystem>
#include <algorithm>
#include <cstring>
#include <cctype>
#include <cstdio>
#include <iostream>

namespace fs = std::filesystem;
using json   = nlohmann::json;

namespace editor
{

// ── helpers ──────────────────────────────────────────────────────────────────

static std::string getExeDir()
{
    char* base = SDL_GetBasePath();
    std::string d = base ? base : "./";
    SDL_free(base);
    return d;
}

// data/ source folder — 3 livelli sopra l'exe (build/config/Debug -> project root)
static std::string getDataDir()
{
    std::error_code ec;
    fs::path src = fs::canonical(fs::path(getExeDir()) / "../../../data", ec);
    if (!ec && fs::exists(src, ec)) return src.string() + "/";
    return getExeDir() + "data/"; // fallback: output dir
}

// assets/ at project root (3 levels above Debug/)
static std::string getAssetsDir()
{
    std::error_code ec;
    fs::path cand = fs::canonical(fs::path(getExeDir()) / "../../../assets", ec);
    if (!ec && fs::exists(cand)) return cand.string();
    // Fallback: data/assets/
    cand = fs::path(getDataDir()) / "assets";
    if (fs::exists(cand)) return cand.string();
    return getDataDir();
}

// Resolve a stored path (relative to data/ OR absolute) to absolute
static std::string resolveMesh(const std::string& field)
{
    if (field.empty()) return "";
    fs::path p(field);
    if (p.is_absolute() && fs::exists(p)) return p.string();

    // assets/ prefix — relative to project root
    if (field.size() >= 7 && field.substr(0,7) == "assets/")
    {
        std::error_code ec;
        fs::path cand = fs::canonical(fs::path(getExeDir()) / "../../../" / field, ec);
        if (!ec && fs::exists(cand)) return cand.string();
    }

    // Relative to data/
    std::string cand = getDataDir() + "/" + field;
    if (fs::exists(cand)) return cand;

    // Relative to exe
    cand = getExeDir() + field;
    if (fs::exists(cand)) return cand;

    return "";
}

// ── ctor ─────────────────────────────────────────────────────────────────────

WeaponEditor::WeaponEditor() { loadWeapons(); if (!m_weapons.empty()) selectWeapon(0); }

// ── tick ─────────────────────────────────────────────────────────────────────

void WeaponEditor::tick(float dt) { m_viewport.tick(dt); }

// ── loadWeapons ───────────────────────────────────────────────────────────────

void WeaponEditor::loadWeapons()
{
    m_weapons.clear();
    fs::path folder = getDataDir() + "/weapons";
    if (!fs::exists(folder)) return;

    for (auto& entry : fs::directory_iterator(folder))
    {
        if (entry.path().extension() != ".json") continue;
        std::ifstream f(entry.path());
        if (!f) continue;
        json j;
        try { f >> j; } catch (...) { continue; }

        WeaponEntry w;
        w.jsonPath           = entry.path().string();
        w.id                 = j.value("id",              entry.path().stem().string());
        w.name               = j.value("name",            w.id);
        w.meshPath           = j.value("mesh",            std::string(""));
        w.projectileMeshPath = j.value("projectile_mesh", std::string(""));
        w.meshRotX           = j.value("mesh_rot_x",      -90.0f);
        w.meshScale          = j.value("mesh_scale",        0.8f);

        if (j.contains("attach_points") && j["attach_points"].is_object())
        {
            for (auto& [ap, val] : j["attach_points"].items())
                if (val.is_array() && val.size() >= 3)
                    w.attachPoints[ap] = {val[0], val[1], val[2]};
        }

        w.damage             = j.value("damage",          25.f);
        w.fireRate           = j.value("fire_rate",       4.5f);
        w.bulletSpeed        = j.value("bullet_speed",    25.f);
        w.bulletLifetime     = j.value("bullet_lifetime", 3.0f);
        w.bulletScale        = j.value("bullet_scale",    0.12f);
        w.heatPerShot        = j.value("heat_per_shot",   0.12f);
        w.cooldownRate       = j.value("cooldown_rate",   0.30f);
        w.overheatPenalty    = j.value("overheat_penalty",2.0f);
        w.effectiveRange     = j.value("effective_range", 20.f);
        w.minRange           = j.value("min_range",       0.0f);
        w.spreadBase         = j.value("spread_base",     0.02f);
        w.spreadAds          = j.value("spread_ads",      0.005f);
        w.spreadMove         = j.value("spread_move",     0.06f);
        w.spreadSprint       = j.value("spread_sprint",   0.14f);
        w.spreadJump         = j.value("spread_jump",     0.20f);
        w.faction            = j.value("faction",         std::string("neutral"));
        if (j.contains("bullet_color") && j["bullet_color"].size() >= 3)
            w.bulletColor = {j["bullet_color"][0], j["bullet_color"][1], j["bullet_color"][2]};

        m_weapons.push_back(std::move(w));
    }
    std::sort(m_weapons.begin(), m_weapons.end(),
              [](const WeaponEntry& a, const WeaponEntry& b){ return a.id < b.id; });
}

// ── selectWeapon ─────────────────────────────────────────────────────────────

void WeaponEditor::selectWeapon(int idx)
{
    if (idx < 0 || idx >= (int)m_weapons.size()) return;
    m_sel   = idx;
    m_dirty = false;
    m_showProjectileMesh = false;

    const auto& w = m_weapons[idx];
    m_rotX  = w.meshRotX;
    m_scale = w.meshScale;
    m_attachPoints  = w.attachPoints;
    m_selAttachPoint = m_attachPoints.count("muzzle") ? "muzzle" : "";

    reloadPreview();
    loadRigJoints();
}

// ── reloadPreview ─────────────────────────────────────────────────────────────

void WeaponEditor::reloadPreview()
{
    if (m_sel < 0 || m_sel >= (int)m_weapons.size()) return;
    const auto& w = m_weapons[m_sel];
    const std::string& field = m_showProjectileMesh ? w.projectileMeshPath : w.meshPath;
    std::string abs = resolveMesh(field);
    if (abs.empty()) { m_viewport.clearModel(); return; }
    m_viewport.loadModel(abs, m_rotX, m_scale);
}

// ── loadRigJoints ─────────────────────────────────────────────────────────────

void WeaponEditor::loadRigJoints()
{
    m_rigJoints.clear();
    if (m_sel < 0 || m_sel >= (int)m_weapons.size()) return;
    const std::string& field = m_weapons[m_sel].meshPath;
    if (field.empty()) return;
    std::string abs = resolveMesh(field);
    std::string ext = abs.size() >= 4 ? abs.substr(abs.size()-4) : "";
    for (auto& c : ext) c = (char)std::tolower((unsigned char)c);
    if (ext != ".glb") return;
    m_rigJoints = readGlbJointNames(abs);
}

// ── saveSelected ─────────────────────────────────────────────────────────────

void WeaponEditor::saveSelected()
{
    if (m_sel < 0 || m_sel >= (int)m_weapons.size()) return;
    auto& w = m_weapons[m_sel];

    // Commit live slider values back to the entry
    w.meshRotX  = m_rotX;
    w.meshScale = m_scale;
    w.attachPoints = m_attachPoints;

    json j;
    {
        std::ifstream f(w.jsonPath);
        if (f) { try { f >> j; } catch (...) {} }
    }

    j["id"]               = w.id;
    j["name"]             = w.name;
    j["faction"]          = w.faction;
    j["mesh"]             = w.meshPath;
    j["projectile_mesh"]  = w.projectileMeshPath;
    j["mesh_rot_x"]       = w.meshRotX;
    j["mesh_scale"]       = w.meshScale;

    json apObj = json::object();
    for (auto& [name, ap] : w.attachPoints)
        apObj[name] = { ap.x, ap.y, ap.z };
    j["attach_points"] = apObj;

    j["damage"]           = w.damage;
    j["fire_rate"]        = w.fireRate;
    j["bullet_speed"]     = w.bulletSpeed;
    j["bullet_lifetime"]  = w.bulletLifetime;
    j["bullet_scale"]     = w.bulletScale;
    j["bullet_color"]     = {w.bulletColor[0], w.bulletColor[1], w.bulletColor[2]};
    j["heat_per_shot"]    = w.heatPerShot;
    j["cooldown_rate"]    = w.cooldownRate;
    j["overheat_penalty"] = w.overheatPenalty;
    j["effective_range"]  = w.effectiveRange;
    j["min_range"]        = w.minRange;
    j["spread_base"]      = w.spreadBase;
    j["spread_ads"]       = w.spreadAds;
    j["spread_move"]      = w.spreadMove;
    j["spread_sprint"]    = w.spreadSprint;
    j["spread_jump"]      = w.spreadJump;

    std::ofstream out(w.jsonPath);
    if (out) { out << j.dump(4); m_dirty = false; }
}

// ── browseForMesh ─────────────────────────────────────────────────────────────

bool WeaponEditor::browseForMesh(std::string& outPath)
{
    std::string startDir = getAssetsDir();

    std::string abs = openFileDialog(
        "Modelli 3D\0*.glb;*.gltf;*.obj\0Tutti i file\0*.*\0\0",
        startDir);
    if (abs.empty()) return false;

    auto toForward = [](std::string s) {
        for (auto& c : s) if (c == '\\') c = '/';
        return s;
    };

    // Try to make path relative to project root (assets/ prefix)
    std::string absF = toForward(abs);
    std::error_code ec;
    fs::path projRoot = fs::canonical(fs::path(getExeDir()) / "../../..", ec);
    if (!ec)
    {
        std::string projRootStr = toForward(projRoot.string()) + "/";
        if (absF.find(projRootStr) == 0)
        {
            outPath = absF.substr(projRootStr.size());
            return true;
        }
    }

    // Fall back to relative to data/
    std::string dataDirAbs = toForward(fs::absolute(getDataDir()).string()) + "/";
    if (absF.find(dataDirAbs) == 0)
        outPath = absF.substr(dataDirAbs.size());
    else
        outPath = absF;
    return true;
}

// ── draw ─────────────────────────────────────────────────────────────────────

void WeaponEditor::draw()
{
    float totalW = ImGui::GetContentRegionAvail().x;
    float totalH = ImGui::GetContentRegionAvail().y;

    float listW  = 180.0f;
    float panelW = 280.0f;
    float vpW    = totalW - listW - panelW - ImGui::GetStyle().ItemSpacing.x * 2;

    ImGui::BeginChild("##wp_panels", ImVec2(totalW, totalH), false);

    // Lista
    ImGui::BeginChild("##wlist", ImVec2(listW, 0), true);
    drawList(listW);
    ImGui::EndChild();

    ImGui::SameLine();

    // Viewport
    ImGui::BeginChild("##wvp", ImVec2(vpW, 0), false,
                      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    drawViewport(vpW);
    ImGui::EndChild();

    ImGui::SameLine();

    // Pannello destra con tab Mesh / Statistiche
    ImGui::BeginChild("##wpanel", ImVec2(panelW, 0), true);
    if (ImGui::BeginTabBar("##wtabs"))
    {
        if (ImGui::BeginTabItem("Mesh"))
        {
            drawMeshTab(panelW - 8.0f);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Statistiche"))
        {
            drawStatsTab(panelW - 8.0f);
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
    ImGui::EndChild();

    ImGui::EndChild();
}

// ── drawList ─────────────────────────────────────────────────────────────────

void WeaponEditor::drawList(float /*w*/)
{
    ImGui::TextDisabled("Armi (%d)", (int)m_weapons.size());
    ImGui::Separator();
    for (int i = 0; i < (int)m_weapons.size(); ++i)
    {
        const auto& w = m_weapons[i];
        bool hasMesh = !w.meshPath.empty();
        if (!hasMesh) ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f,0.5f,0.5f,1.0f));
        bool sel = (i == m_sel);
        if (ImGui::Selectable(w.name.c_str(), sel)) selectWeapon(i);
        if (!hasMesh) ImGui::PopStyleColor();
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip(hasMesh ? "Mesh: %s" : "Nessun mesh", w.meshPath.c_str());
    }
    ImGui::Separator();
    if (ImGui::Button("Ricarica lista")) loadWeapons();
}

// ── drawMeshTab ───────────────────────────────────────────────────────────────

void WeaponEditor::drawMeshTab(float panelW)
{
    if (m_sel < 0 || m_sel >= (int)m_weapons.size())
    { ImGui::TextDisabled("Seleziona un'arma."); return; }

    auto& w = m_weapons[m_sel];
    float slW = panelW - 8.0f;
    bool changed = false;

    ImGui::TextColored({0.9f,0.7f,0.2f,1.0f}, "%s", w.name.c_str());
    ImGui::Separator();

    // ── Mesh arma ───────────────────────────────────────────────────────
    ImGui::TextDisabled("Mesh arma");
    char mbuf[512]; std::strncpy(mbuf, w.meshPath.c_str(), sizeof(mbuf)-1);
    ImGui::SetNextItemWidth(slW - 36.f);
    if (ImGui::InputText("##meshp", mbuf, sizeof(mbuf)))
        { w.meshPath = mbuf; changed = true; }
    ImGui::SameLine();
    if (ImGui::SmallButton("...##bm"))
    {
        if (browseForMesh(w.meshPath)) { changed = true; reloadPreview(); loadRigJoints(); }
    }

    ImGui::TextDisabled("Mesh proiettile");
    char pbuf[512]; std::strncpy(pbuf, w.projectileMeshPath.c_str(), sizeof(pbuf)-1);
    ImGui::SetNextItemWidth(slW - 36.f);
    if (ImGui::InputText("##pmeshp", pbuf, sizeof(pbuf)))
        { w.projectileMeshPath = pbuf; changed = true; }
    ImGui::SameLine();
    if (ImGui::SmallButton("...##bp"))
    {
        std::string tmp = w.projectileMeshPath;
        if (browseForMesh(tmp)) { w.projectileMeshPath = tmp; changed = true; }
    }

    // Vista viewport
    {
        bool s = !m_showProjectileMesh;
        if (ImGui::RadioButton("Arma", s)) { m_showProjectileMesh = false; reloadPreview(); }
        ImGui::SameLine();
        if (ImGui::RadioButton("Proiettile", m_showProjectileMesh))
            { m_showProjectileMesh = true; reloadPreview(); }
    }

    ImGui::Separator();

    // ── Trasformazione modello ──────────────────────────────────────────
    ImGui::TextDisabled("Trasformazione modello");

    auto floatRow = [&](const char* label, float& val,
                        float vmin, float vmax, float speed, const char* fmt) -> bool
    {
        bool c = false;
        ImGui::PushID(label);
        ImGui::SetNextItemWidth(slW - 90.0f);
        if (ImGui::SliderFloat("##sl", &val, vmin, vmax, fmt)) c = true;
        ImGui::SameLine();
        ImGui::SetNextItemWidth(60);
        if (ImGui::DragFloat("##dg", &val, speed, vmin, vmax, fmt)) c = true;
        ImGui::SameLine(0,4); ImGui::TextDisabled("%s", label);
        ImGui::PopID();
        return c;
    };

    if (floatRow("RotX", m_rotX, -180.f, 180.f, 1.0f, "%.0f"))
    { changed = true; reloadPreview(); }
    if (floatRow("Scala", m_scale, 0.01f, 5.0f, 0.01f, "%.3f"))
    { changed = true; reloadPreview(); }

    ImGui::Separator();

    // ── Attach points ───────────────────────────────────────────────────
    ImGui::TextDisabled("Attach Points");

    // Lista degli attach points correnti
    for (auto& [apName, ap] : m_attachPoints)
    {
        bool apSel = (apName == m_selAttachPoint);
        if (ImGui::Selectable(apName.c_str(), apSel, 0, ImVec2(slW * 0.45f, 0)))
            m_selAttachPoint = apName;
        ImGui::SameLine();
        ImGui::TextDisabled("(%.2f, %.2f, %.2f)", ap.x, ap.y, ap.z);
    }

    // Pannello edit del punto selezionato
    if (!m_selAttachPoint.empty() && m_attachPoints.count(m_selAttachPoint))
    {
        auto& ap = m_attachPoints.at(m_selAttachPoint);
        ImGui::SetNextItemWidth(slW);
        if (ImGui::DragFloat3("##apxyz", &ap.x, 0.001f, -2.0f, 2.0f, "%.3f")) changed = true;

        if (ImGui::SmallButton("Rimuovi##rmap"))
        {
            m_attachPoints.erase(m_selAttachPoint);
            m_selAttachPoint.clear();
            changed = true;
        }
    }

    // Aggiungi attach point standard
    ImGui::Spacing();
    static const char* k_stdPoints[] = {"muzzle","right_hand","left_hand","stock","sight"};
    ImGui::TextDisabled("Aggiungi:");
    for (const char* pt : k_stdPoints)
    {
        if (m_attachPoints.count(pt)) continue;
        if (ImGui::SmallButton(pt)) { m_attachPoints[pt] = {}; m_selAttachPoint = pt; changed = true; }
        ImGui::SameLine();
    }
    ImGui::NewLine();

    // ── Ossa del rig ────────────────────────────────────────────────────
    if (!m_rigJoints.empty())
    {
        ImGui::Separator();
        ImGui::TextDisabled("Ossa rig (%d)", (int)m_rigJoints.size());
        float listH = (int)m_rigJoints.size() > 6 ? 110.0f : 0.0f;
        if (ImGui::BeginChild("##rig_wp", ImVec2(0, listH), true))
        {
            for (const auto& joint : m_rigJoints)
            {
                bool already = m_attachPoints.count(joint) > 0;
                if (already) ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4f,0.8f,0.4f,1.0f));
                if (ImGui::SmallButton(joint.c_str()))
                {
                    if (!already) { m_attachPoints[joint] = {}; m_selAttachPoint = joint; changed = true; }
                }
                if (already) ImGui::PopStyleColor();
            }
        }
        ImGui::EndChild();
    }

    ImGui::Separator();
    if (changed) m_dirty = true;
    if (m_dirty) ImGui::TextColored({1.f,0.7f,0.2f,1.f}, "* Modifiche non salvate");
    else         ImGui::TextDisabled("Salvato");

    float bw = (slW - 4.f) * 0.5f;
    if (ImGui::Button("Salva JSON", {bw, 0})) saveSelected();
    ImGui::SameLine();
    if (ImGui::Button("Ripristina", {bw, 0})) selectWeapon(m_sel);
}

// ── drawStatsTab ──────────────────────────────────────────────────────────────

void WeaponEditor::drawStatsTab(float panelW)
{
    if (m_sel < 0 || m_sel >= (int)m_weapons.size())
    { ImGui::TextDisabled("Seleziona un'arma."); return; }

    auto& w = m_weapons[m_sel];
    float slW = panelW - 8.0f;
    bool changed = false;

    ImGui::TextColored({0.9f,0.7f,0.2f,1.0f}, "%s", w.name.c_str());
    ImGui::Separator();

    // Fazione
    ImGui::TextDisabled("Fazione");
    const char* factions[] = {"neutral","republic","separatist"};
    int fi = 0;
    for (int i = 0; i < 3; ++i) if (w.faction == factions[i]) fi = i;
    ImGui::SetNextItemWidth(slW);
    if (ImGui::BeginCombo("##faction", factions[fi]))
    {
        for (int i = 0; i < 3; ++i) {
            bool s = (i == fi);
            if (ImGui::Selectable(factions[i], s)) { w.faction = factions[i]; changed = true; }
            if (s) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }

    ImGui::Separator();
    ImGui::TextDisabled("Combattimento");

    auto drag = [&](const char* label, float& v, float lo, float hi, const char* fmt = "%.2f") {
        ImGui::SetNextItemWidth(slW);
        if (ImGui::DragFloat(label, &v, (hi-lo)*0.005f, lo, hi, fmt)) changed = true;
    };

    drag("Danno##dmg",        w.damage,          1.f, 500.f);
    drag("Cadenza (colpi/s)", w.fireRate,         0.5f, 30.f);
    drag("Vel. proiettile",   w.bulletSpeed,      5.f, 200.f);
    drag("Vita proiettile",   w.bulletLifetime,   0.1f, 10.f);
    drag("Scala proiettile",  w.bulletScale,      0.01f, 1.0f);

    ImGui::Spacing();
    ImGui::TextDisabled("Colore proiettile");
    ImGui::SetNextItemWidth(slW);
    if (ImGui::ColorEdit3("##bcol", w.bulletColor.data(),
                          ImGuiColorEditFlags_Float | ImGuiColorEditFlags_NoAlpha))
        changed = true;

    ImGui::Separator();
    ImGui::TextDisabled("Surriscaldamento");
    drag("Calore/colpo",       w.heatPerShot,      0.f, 1.f);
    drag("Raffreddamento/s",   w.cooldownRate,     0.f, 2.f);
    drag("Penalita' overheat", w.overheatPenalty,  0.f, 10.f);

    ImGui::Separator();
    ImGui::TextDisabled("Precisione");
    drag("Spread base",      w.spreadBase,   0.f, 0.5f, "%.4f");
    drag("Spread ADS",       w.spreadAds,    0.f, 0.2f, "%.4f");
    drag("Spread movimento", w.spreadMove,   0.f, 0.5f, "%.4f");
    drag("Spread sprint",    w.spreadSprint, 0.f, 1.0f, "%.4f");
    drag("Spread salto",     w.spreadJump,   0.f, 1.0f, "%.4f");

    ImGui::Separator();
    ImGui::TextDisabled("Gittata");
    drag("Gittata effettiva", w.effectiveRange, 1.f, 100.f);
    drag("Gittata minima",    w.minRange,       0.f,  20.f);

    ImGui::Separator();
    if (changed) m_dirty = true;
    if (m_dirty) ImGui::TextColored({1.f,0.7f,0.2f,1.f}, "* Modifiche non salvate");
    else         ImGui::TextDisabled("Salvato");

    float bw = (slW - 4.f) * 0.5f;
    if (ImGui::Button("Salva JSON##s", {bw, 0})) saveSelected();
    ImGui::SameLine();
    if (ImGui::Button("Ripristina##r", {bw, 0})) selectWeapon(m_sel);
}

// ── drawViewport ──────────────────────────────────────────────────────────────

void WeaponEditor::drawViewport(float /*vpW*/)
{
    ImGui::TextDisabled("TAB = cattura mouse | WASD/QE = vola | Esc = ritorna");
    if (m_sel >= 0 && m_sel < (int)m_weapons.size())
    {
        const auto& w = m_weapons[m_sel];
        const std::string& mp = m_showProjectileMesh ? w.projectileMeshPath : w.meshPath;
        if (mp.empty())
            ImGui::TextColored({1.f,0.6f,0.2f,1.f},
                               "Nessun mesh. Usa ... nella tab Mesh per sfogliare.");
    }
    m_viewport.draw(false);
}

} // namespace editor
