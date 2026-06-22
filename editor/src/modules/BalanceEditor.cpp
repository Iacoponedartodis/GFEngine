#include "modules/BalanceEditor.hpp"
#include <imgui.h>
#include <nlohmann/json.hpp>
#include <fstream>
#include <iostream>
#include <cstdio>
#include <cstring>

using json = nlohmann::json;

namespace editor
{

BalanceEditor::BalanceEditor() { reload(); }

void BalanceEditor::reload()
{
    m_registry.loadAll("data");
    m_selWeapon.clear();
    m_selEnemy.clear();
    m_selAI.clear();
    m_dirty = false;
}

// ── Salvataggio ──────────────────────────────────────────────────────────

void BalanceEditor::saveWeapon(const mini::WeaponDef& w)
{
    std::string path = "data/weapons/" + w.id + ".json";
    json j;
    j["id"]               = w.id;
    j["name"]             = w.name;
    j["damage"]           = w.damage;
    j["fire_rate"]        = w.fireRate;
    j["bullet_speed"]     = w.bulletSpeed;
    j["bullet_lifetime"]  = w.bulletLifetime;
    j["bullet_scale"]     = w.bulletScale;
    j["bullet_color"]     = {w.bulletColor[0], w.bulletColor[1], w.bulletColor[2]};
    j["heat_per_shot"]    = w.heatPerShot;
    j["cooldown_rate"]    = w.cooldownRate;
    j["overheat_penalty"] = w.overheatPenalty;
    j["mesh"]             = w.meshPath;
    j["projectile_mesh"]  = w.projectileMeshPath;
    std::ofstream f(path);
    if (!f.is_open()) { std::cerr << "[Balance] Cannot write: " << path << "\n"; return; }
    f << j.dump(4) << "\n";
    std::cout << "[Balance] Saved: " << path << "\n";
    m_dirty = false;
}

void BalanceEditor::saveEnemy(const mini::EnemyDef& e)
{
    std::string path = "data/enemies/" + e.id + ".json";
    json j;
    j["id"]             = e.id;
    j["name"]           = e.name;
    j["mesh"]           = e.meshPath;
    j["texture"]        = e.texturePath;
    j["hitbox_profile"] = e.hitboxProfileId;
    j["ai_profile"]     = e.aiProfileId;
    j["weapon"]         = e.weaponId;
    j["team"]           = e.team;
    j["color"]          = {e.color[0], e.color[1], e.color[2]};
    j["bullet_color"]   = {e.bulletColor[0], e.bulletColor[1], e.bulletColor[2]};
    j["stats"]["hp"]         = e.hp;
    j["stats"]["move_speed"] = e.moveSpeed;
    std::ofstream f(path);
    if (!f.is_open()) { std::cerr << "[Balance] Cannot write: " << path << "\n"; return; }
    f << j.dump(4) << "\n";
    std::cout << "[Balance] Saved: " << path << "\n";
    m_dirty = false;
}

void BalanceEditor::saveAI(const mini::AiProfileDef& a)
{
    std::string path = "data/ai/" + a.id + ".json";
    json j;
    j["profile_id"]            = a.id;
    j["sight_range"]           = a.sightRange;
    j["fov_deg"]               = a.fovDeg;
    j["hearing_range"]         = a.hearingRange;
    j["reaction_time"]         = a.reactionTime;
    j["aggression"]            = a.aggression;
    j["accuracy"]              = a.accuracy;
    j["cover_preference"]      = a.coverPreference;
    j["retreat_hp_threshold"]  = a.retreatHpThresh;
    j["peek_duration_min"]     = a.peekDurationMin;
    j["peek_duration_max"]     = a.peekDurationMax;
    j["hide_duration_min"]     = a.hideDurationMin;
    j["hide_duration_max"]     = a.hideDurationMax;
    j["reposition_chance"]     = a.repositionChance;
    j["flank_chance"]          = a.flankChance;
    j["shoot_interval"]        = a.shootInterval;
    j["patrol_speed"]          = a.patrolSpeed;
    j["seek_speed"]            = a.seekSpeed;
    j["jump_enabled"]          = a.jumpEnabled;
    std::ofstream f(path);
    if (!f.is_open()) { std::cerr << "[Balance] Cannot write: " << path << "\n"; return; }
    f << j.dump(4) << "\n";
    std::cout << "[Balance] Saved: " << path << "\n";
    m_dirty = false;
}

// ── Weapons tab ──────────────────────────────────────────────────────────

void BalanceEditor::drawWeaponsTab()
{
    const auto& weapons = m_registry.weapons();
    if (weapons.empty()) { ImGui::TextDisabled("Nessun file in data/weapons/"); return; }

    ImGui::BeginChild("##wlist", ImVec2(180, 0), true);
    for (auto& [id, w] : weapons)
    {
        bool sel = (id == m_selWeapon);
        if (ImGui::Selectable(w.name.c_str(), sel))
            m_selWeapon = id;
    }
    ImGui::EndChild();

    ImGui::SameLine();

    auto it = weapons.find(m_selWeapon);
    if (it == weapons.end()) { ImGui::TextDisabled("Seleziona un'arma."); return; }

    // Copia modificabile (il registry è const)
    static mini::WeaponDef edit;
    static std::string editId;
    if (editId != m_selWeapon) { edit = it->second; editId = m_selWeapon; }

    ImGui::BeginChild("##wedit", ImVec2(0, 0), false);

    ImGui::Text("Arma: %s  [%s]", edit.name.c_str(), edit.id.c_str());
    ImGui::Separator();

    ImGui::DragFloat("Danno",           &edit.damage,          0.5f, 1.0f, 200.0f, "%.1f");
    ImGui::DragFloat("Cadenza (rnd/s)", &edit.fireRate,         0.1f, 0.1f,  30.0f, "%.2f");
    ImGui::DragFloat("Vel. proiettile", &edit.bulletSpeed,      0.5f, 1.0f, 100.0f, "%.1f");
    ImGui::DragFloat("Vita proiettile", &edit.bulletLifetime,   0.1f, 0.1f,  10.0f, "%.2f");
    ImGui::DragFloat("Scala proiettile",&edit.bulletScale,      0.005f,0.01f, 1.0f, "%.3f");
    ImGui::ColorEdit3("Colore proiettile", edit.bulletColor.data());
    ImGui::Separator();
    ImGui::Text("Calore");
    ImGui::DragFloat("Calore/colpo",    &edit.heatPerShot,     0.005f,0.01f, 1.0f, "%.3f");
    ImGui::DragFloat("Raffreddamento",  &edit.cooldownRate,    0.005f,0.01f, 2.0f, "%.3f");
    ImGui::DragFloat("Penalità overheat",&edit.overheatPenalty,0.1f, 0.0f,  10.0f, "%.2f");

    ImGui::Separator();
    if (ImGui::Button("Salva", {120,0}))
        saveWeapon(edit);
    ImGui::SameLine();
    if (ImGui::Button("Ripristina", {120,0}))
        edit = it->second;
    ImGui::SameLine();
    if (ImGui::Button("Ricarica tutto", {120,0}))
        reload();

    ImGui::EndChild();
}

// ── Enemies tab ──────────────────────────────────────────────────────────

void BalanceEditor::drawEnemiesTab()
{
    const auto& enemies = m_registry.enemies();
    if (enemies.empty()) { ImGui::TextDisabled("Nessun file in data/enemies/"); return; }

    ImGui::BeginChild("##elist", ImVec2(180, 0), true);
    for (auto& [id, e] : enemies)
    {
        bool sel = (id == m_selEnemy);
        if (ImGui::Selectable(e.name.c_str(), sel)) m_selEnemy = id;
    }
    ImGui::EndChild();
    ImGui::SameLine();

    auto it = enemies.find(m_selEnemy);
    if (it == enemies.end()) { ImGui::TextDisabled("Seleziona un nemico."); return; }

    static mini::EnemyDef edit;
    static std::string editId;
    if (editId != m_selEnemy) { edit = it->second; editId = m_selEnemy; }

    ImGui::BeginChild("##eedit", ImVec2(0, 0), false);
    ImGui::Text("Nemico: %s  [%s]", edit.name.c_str(), edit.id.c_str());
    ImGui::Separator();

    ImGui::DragFloat("HP",             &edit.hp,        1.0f, 1.0f, 1000.0f, "%.0f");
    ImGui::DragFloat("Velocità",       &edit.moveSpeed, 0.1f, 0.5f,   20.0f, "%.2f");
    ImGui::ColorEdit3("Colore entità", edit.color.data());
    ImGui::ColorEdit3("Colore proiett.", edit.bulletColor.data());
    ImGui::Separator();

    char wBuf[64]; std::strncpy(wBuf, edit.weaponId.c_str(), 63);
    if (ImGui::InputText("Arma ID",       wBuf, 64)) edit.weaponId = wBuf;
    char aBuf[64]; std::strncpy(aBuf, edit.aiProfileId.c_str(), 63);
    if (ImGui::InputText("AI Profile ID", aBuf, 64)) edit.aiProfileId = aBuf;
    char hBuf[64]; std::strncpy(hBuf, edit.hitboxProfileId.c_str(), 63);
    if (ImGui::InputText("Hitbox ID",     hBuf, 64)) edit.hitboxProfileId = hBuf;

    ImGui::Separator();
    if (ImGui::Button("Salva", {120,0}))    saveEnemy(edit);
    ImGui::SameLine();
    if (ImGui::Button("Ripristina",{120,0})) edit = it->second;
    ImGui::SameLine();
    if (ImGui::Button("Ricarica tutto",{120,0})) reload();
    ImGui::EndChild();
}

// ── AI tab ───────────────────────────────────────────────────────────────

void BalanceEditor::drawAITab()
{
    const auto& profiles = m_registry.aiProfiles();
    if (profiles.empty()) { ImGui::TextDisabled("Nessun file in data/ai/"); return; }

    ImGui::BeginChild("##ailist", ImVec2(180, 0), true);
    for (auto& [id, a] : profiles)
    {
        bool sel = (id == m_selAI);
        if (ImGui::Selectable(id.c_str(), sel)) m_selAI = id;
    }
    ImGui::EndChild();
    ImGui::SameLine();

    auto it = profiles.find(m_selAI);
    if (it == profiles.end()) { ImGui::TextDisabled("Seleziona un profilo AI."); return; }

    static mini::AiProfileDef edit;
    static std::string editId;
    if (editId != m_selAI) { edit = it->second; editId = m_selAI; }

    ImGui::BeginChild("##aiedit", ImVec2(0, 0), false);
    ImGui::Text("AI Profile: %s", edit.id.c_str());
    ImGui::Separator();

    if (ImGui::CollapsingHeader("Percezione", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::DragFloat("Vista (range)",   &edit.sightRange,   0.5f,  1.0f, 100.0f, "%.1f");
        ImGui::DragFloat("Campo visivo°",   &edit.fovDeg,       1.0f, 10.0f, 360.0f, "%.0f");
        ImGui::DragFloat("Udito (range)",   &edit.hearingRange, 0.5f,  0.0f,  50.0f, "%.1f");
        ImGui::DragFloat("Reazione (sec)",  &edit.reactionTime, 0.01f, 0.0f,   3.0f, "%.2f");
    }
    if (ImGui::CollapsingHeader("Comportamento", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::SliderFloat("Aggressività",  &edit.aggression,   0.0f, 1.0f);
        ImGui::SliderFloat("Precisione",    &edit.accuracy,     0.0f, 1.0f);
        ImGui::SliderFloat("Pref. copertura",&edit.coverPreference,0.0f, 1.0f);
        ImGui::SliderFloat("HP ritiro",     &edit.retreatHpThresh,0.0f, 1.0f);
        ImGui::SliderFloat("Fiancheggia",   &edit.flankChance,  0.0f, 1.0f);
        ImGui::SliderFloat("Riposiziona",   &edit.repositionChance,0.0f, 1.0f);
    }
    if (ImGui::CollapsingHeader("Copertura e fuoco")) {
        ImGui::DragFloat("Peek min (s)",    &edit.peekDurationMin, 0.05f,0.0f, 5.0f, "%.2f");
        ImGui::DragFloat("Peek max (s)",    &edit.peekDurationMax, 0.05f,0.0f, 5.0f, "%.2f");
        ImGui::DragFloat("Nascondi min (s)",&edit.hideDurationMin, 0.05f,0.0f,10.0f, "%.2f");
        ImGui::DragFloat("Nascondi max (s)",&edit.hideDurationMax, 0.05f,0.0f,10.0f, "%.2f");
        ImGui::DragFloat("Intervallo sparo",&edit.shootInterval,   0.05f,0.1f,10.0f, "%.2f");
    }
    if (ImGui::CollapsingHeader("Movimento")) {
        ImGui::DragFloat("Vel. pattuglia",  &edit.patrolSpeed, 0.1f, 0.5f, 15.0f, "%.2f");
        ImGui::DragFloat("Vel. inseguimento",&edit.seekSpeed,  0.1f, 0.5f, 20.0f, "%.2f");
        ImGui::Checkbox("Può saltare",      &edit.jumpEnabled);
    }

    ImGui::Separator();
    if (ImGui::Button("Salva", {120,0}))     saveAI(edit);
    ImGui::SameLine();
    if (ImGui::Button("Ripristina",{120,0})) edit = it->second;
    ImGui::SameLine();
    if (ImGui::Button("Ricarica tutto",{120,0})) reload();
    ImGui::EndChild();
}

// ── Draw principale ──────────────────────────────────────────────────────

void BalanceEditor::draw()
{
    if (ImGui::BeginTabBar("##btabs"))
    {
        if (ImGui::BeginTabItem("Armi"))      { drawWeaponsTab(); ImGui::EndTabItem(); }
        if (ImGui::BeginTabItem("Nemici"))    { drawEnemiesTab(); ImGui::EndTabItem(); }
        if (ImGui::BeginTabItem("AI"))        { drawAITab();      ImGui::EndTabItem(); }
        ImGui::EndTabBar();
    }
}

} // namespace editor