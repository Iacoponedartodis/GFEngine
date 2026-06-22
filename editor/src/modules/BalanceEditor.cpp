#include "modules/BalanceEditor.hpp"
#include <imgui.h>
#include <nlohmann/json.hpp>
#include <SDL2/SDL.h>
#include <fstream>
#include <iostream>
#include <cstdio>
#include <cstring>
#include <string>

using json = nlohmann::json;

#include <filesystem>
namespace fs = std::filesystem;

namespace editor
{

// Restituisce la dir SOURCE del progetto (data/ nella root).
// L'exe è in build/windows-debug/Debug/ → sali 3 livelli per il source.
static std::string getSourceDataDir()
{
    char* base = SDL_GetBasePath();
    fs::path exeDir = base ? base : ".";
    SDL_free(base);

    // Prova 3 livelli su: build/config/Debug → project root
    std::error_code ec;
    fs::path sourceData = fs::canonical(exeDir / "../../../data", ec);
    if (!ec && fs::exists(sourceData / "weapons", ec))
        return sourceData.string() + "/";

    // Fallback: usa la copia locale nell'output dir
    return (exeDir / "data").string() + "/";
}


BalanceEditor::BalanceEditor() { reload(); }

void BalanceEditor::reload()
{
    m_registry.loadAll(getSourceDataDir());
    m_selWeapon.clear();
    m_selEnemy.clear();
    m_selAI.clear();
    m_dirty = false;
    std::cout << "[Balance] Dati caricati da: " << getSourceDataDir() << "\n";
}

// ── Salvataggio ──────────────────────────────────────────────────────────

void BalanceEditor::saveWeapon(const mini::WeaponDef& w)
{
    std::string path = getSourceDataDir() + "weapons/" + w.id + ".json";
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
    if (!f.is_open())
    {
        std::cerr << "[Balance] ERRORE scrittura: " << path << "\n";
        return;
    }
    f << j.dump(4) << "\n";
    f.close(); // flush e chiudi prima del reload
    std::cout << "[Balance] Salvato: " << path << "\n";
    m_dirty = false;
    m_registry.reload(getSourceDataDir());
}

void BalanceEditor::saveEnemy(const mini::EnemyDef& e)
{
    std::string path = getSourceDataDir() + "enemies/" + e.id + ".json";
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
    if (!f.is_open())
    {
        std::cerr << "[Balance] ERRORE scrittura: " << path << "\n";
        return;
    }
    f << j.dump(4) << "\n";
    f.close(); // flush e chiudi prima del reload
    std::cout << "[Balance] Salvato: " << path << "\n";
    m_dirty = false;
    m_registry.reload(getSourceDataDir());
}

void BalanceEditor::saveAI(const mini::AiProfileDef& a)
{
    std::string path = getSourceDataDir() + "ai/" + a.id + ".json";
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
    if (!f.is_open())
    {
        std::cerr << "[Balance] ERRORE scrittura: " << path << "\n";
        return;
    }
    f << j.dump(4) << "\n";
    f.close(); // flush e chiudi prima del reload
    std::cout << "[Balance] Salvato: " << path << "\n";
    m_dirty = false;
    m_registry.reload(getSourceDataDir());
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

    static mini::WeaponDef edit;
    static std::string editId;
    static char newWId[64] = "";

    ImGui::BeginChild("##wedit", ImVec2(0, 0), false);

    // Crea nuova arma
    ImGui::SetNextItemWidth(130);
    ImGui::InputText("Nuovo ID##w", newWId, 64);
    ImGui::SameLine();
    if (ImGui::Button("+ Crea arma") && newWId[0] != '\0')
    {
        std::string path = getSourceDataDir() + "weapons/" + newWId + ".json";
        if (!fs::exists(path))
        { mini::WeaponDef def; def.id=newWId; def.name=newWId; saveWeapon(def); m_selWeapon=newWId; }
        newWId[0] = '\0';
    }
    ImGui::Separator();

    auto it = weapons.find(m_selWeapon);
    if (it == weapons.end())
    { ImGui::TextDisabled("Seleziona un'arma."); ImGui::EndChild(); return; }
    if (editId != m_selWeapon) { edit = it->second; editId = m_selWeapon; }

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

    static mini::EnemyDef edit;
    static std::string editId;
    static char newEId[64] = "";

    ImGui::BeginChild("##eedit", ImVec2(0, 0), false);

    ImGui::SetNextItemWidth(130); ImGui::InputText("Nuovo ID##e", newEId, 64); ImGui::SameLine();
    if (ImGui::Button("+ Crea nemico") && newEId[0] != '\0')
    {
        std::string path = getSourceDataDir() + "enemies/" + newEId + ".json";
        if (!fs::exists(path))
        { mini::EnemyDef def; def.id=newEId; def.name=newEId; saveEnemy(def); m_selEnemy=newEId; }
        newEId[0] = '\0';
    }
    ImGui::Separator();

    auto it = enemies.find(m_selEnemy);
    if (it == enemies.end())
    { ImGui::TextDisabled("Seleziona un nemico."); ImGui::EndChild(); return; }

    if (editId != m_selEnemy) { edit = it->second; editId = m_selEnemy; }
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
    if (ImGui::Button("Salva", {120,0}))     saveEnemy(edit);
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

    static mini::AiProfileDef edit;
    static std::string editId;
    static char newAId[64] = "";

    ImGui::BeginChild("##aiedit", ImVec2(0, 0), false);

    ImGui::SetNextItemWidth(130); ImGui::InputText("Nuovo ID##a", newAId, 64); ImGui::SameLine();
    if (ImGui::Button("+ Crea profilo AI") && newAId[0] != '\0')
    {
        std::string path = getSourceDataDir() + "ai/" + newAId + ".json";
        if (!fs::exists(path))
        { mini::AiProfileDef def; def.id=newAId; saveAI(def); m_selAI=newAId; }
        newAId[0] = '\0';
    }
    ImGui::Separator();

    auto it = profiles.find(m_selAI);
    if (it == profiles.end())
    { ImGui::TextDisabled("Seleziona un profilo AI."); ImGui::EndChild(); return; }

    if (editId != m_selAI) { edit = it->second; editId = m_selAI; }
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