#include "modules/BalanceEditor.hpp"
#include "util/JsonSave.hpp"
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
    m_selAI.clear();
    m_dirty = false;
    std::cout << "[Balance] Dati caricati da: " << getSourceDataDir() << "\n";
}

// ── Salvataggio ──────────────────────────────────────────────────────────

void BalanceEditor::saveWeapon(const mini::WeaponDef& w)
{
    std::string path = getSourceDataDir() + "weapons/" + w.id + ".json";
    // saveJsonRMW (ADR-010): preserva i campi degli altri moduli
    // (attach_points, mesh_scale, mesh_rot_x...).
    editor::jsonsave::saveJsonRMW(path, [&](json& j) {
    j.erase("id"); // deprecato: id = nome file (ADR-001)
    j["name"]             = w.name;
    j["faction"]          = mini::factionToString(w.faction);
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
    j["spread_base"]      = w.baseSpread;
    j["spread_ads"]       = w.adsSpread;
    j["spread_move"]      = w.moveSpread;
    j["spread_sprint"]    = w.sprintSpread;
    j["spread_jump"]      = w.jumpSpread;
    j["mesh"]             = w.meshPath;
    j["projectile_mesh"]  = w.projectileMeshPath;
    return true;
    });
    std::cout << "[Balance] Salvato: " << path << "\n";
    m_dirty = false;
    m_registry.reload(getSourceDataDir());
}

// Nemici/alleati si editano SOLO nell'Entity Editor (ADR-012):
// i vecchi tab redirect e i relativi save sono stati rimossi.

void BalanceEditor::saveAI(const mini::AiProfileDef& a)
{
    std::string path = getSourceDataDir() + "ai/" + a.id + ".json";
    editor::jsonsave::saveJsonRMW(path, [&](json& j) {
    j.erase("profile_id"); // deprecato: id = nome file (ADR-001)
    j["role"]                  = a.role;
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
    return true;
    });
    std::cout << "[Balance] Salvato: " << path << "\n";
    m_dirty = false;
    m_registry.reload(getSourceDataDir());
}

void BalanceEditor::saveAbility(const mini::AbilityDef& a)
{
    std::string path = getSourceDataDir() + "abilities/" + a.id + ".json";
    editor::jsonsave::saveJsonRMW(path, [&](json& j) {
    j.erase("id"); // deprecato: id = nome file (ADR-001)
    j["name"]     = a.name;
    j["type"]     = a.type;
    j["param1"]   = a.param1;
    j["param2"]   = a.param2;
    j["param3"]   = a.param3;
    j["cooldown"] = a.cooldown;
    j["passive"]  = a.passive;
    return true;
    });
    std::cout << "[Balance] Salvato: " << path << "\n";
    m_dirty = false;
    m_registry.reload(getSourceDataDir());
}

// ── Abilities tab ────────────────────────────────────────────────────────

void BalanceEditor::drawAbilitiesTab()
{
    const auto& abilities = m_registry.abilities();

    ImGui::BeginChild("##ablist", ImVec2(180, 0), ImGuiChildFlags_Borders | ImGuiChildFlags_ResizeX);
    for (auto& [id, a] : abilities)
    {
        bool sel = (id == m_selAbility);
        if (ImGui::Selectable((a.name.empty() ? id : a.name).c_str(), sel))
            m_selAbility = id;
    }
    ImGui::EndChild();
    ImGui::SameLine();

    static mini::AbilityDef edit;
    static std::string editId;
    static char newAId[64] = "";

    ImGui::BeginChild("##abedit", ImVec2(0, 0), false);

    // Crea nuova abilità
    ImGui::TextDisabled("Nuova abilita':");
    ImGui::SetNextItemWidth(160);
    ImGui::InputText("##newaid", newAId, 64);
    ImGui::SameLine();
    if (ImGui::Button("+ Crea") && newAId[0] != '\0')
    {
        std::string path = getSourceDataDir() + "abilities/" + newAId + ".json";
        if (!fs::exists(path))
        {
            mini::AbilityDef def;
            def.id = newAId; def.name = newAId; def.type = "shield";
            def.param1 = 100.0f; def.param2 = 5.0f; def.param3 = 3.0f;
            def.cooldown = 10.0f; def.passive = true;
            saveAbility(def); m_selAbility = newAId;
        }
        newAId[0] = '\0';
    }
    ImGui::Separator();

    auto it = abilities.find(m_selAbility);
    if (it == abilities.end())
    { ImGui::TextDisabled("Seleziona un'abilita'."); ImGui::EndChild(); return; }
    if (editId != m_selAbility) { edit = it->second; editId = m_selAbility; }

    ImGui::Text("Abilita': %s  [%s]", edit.name.c_str(), edit.id.c_str());
    ImGui::Separator();

    // Nome (etichetta, non id)
    {
        char nameBuf[64];
        std::snprintf(nameBuf, sizeof(nameBuf), "%s", edit.name.c_str());
        if (ImGui::InputText("Nome", nameBuf, sizeof(nameBuf)))
            edit.name = nameBuf;
    }

    // Tipo dall'elenco supportato (niente testo libero)
    {
        static const char* kTypes[] =
            {"shield", "roll", "melee", "jetpack", "missile", "command_aura"};
        int ti = 0;
        for (int i = 0; i < 6; ++i) if (edit.type == kTypes[i]) { ti = i; break; }
        if (ImGui::Combo("Tipo", &ti, kTypes, 6)) edit.type = kTypes[ti];
    }
    ImGui::TextDisabled("Runtime attivo: solo 'shield' (16_AiBehavior). Gli altri tipi\n"
                        "sono autorabili ma non ancora consumati in partita.");
    ImGui::Separator();

    // Parametri con etichette contestuali per il tipo shield
    const bool isShield = (edit.type == "shield");
    ImGui::DragFloat(isShield ? "HP scudo (param1)"    : "param1", &edit.param1, 1.0f,  0.0f, 1000.0f, "%.1f");
    ImGui::DragFloat(isShield ? "Rigenerazione/s (param2)" : "param2", &edit.param2, 0.1f,  0.0f,  100.0f, "%.1f");
    ImGui::DragFloat(isShield ? "Ritardo regen s (param3)" : "param3", &edit.param3, 0.1f,  0.0f,   30.0f, "%.1f");
    ImGui::DragFloat("Cooldown (s)", &edit.cooldown, 0.1f, 0.0f, 60.0f, "%.1f");
    ImGui::Checkbox("Passiva", &edit.passive);

    ImGui::Separator();
    if (ImGui::Button("Salva", {120,0}))
        saveAbility(edit);
    ImGui::SameLine();
    if (ImGui::Button("Ripristina", {120,0}))
        edit = it->second;
    ImGui::SameLine();
    if (ImGui::Button("Ricarica tutto", {120,0}))
        reload();

    ImGui::EndChild();
}

// ── Weapons tab ──────────────────────────────────────────────────────────

void BalanceEditor::drawWeaponsTab()
{
    const auto& weapons = m_registry.weapons();
    if (weapons.empty()) { ImGui::TextDisabled("Nessun file in data/weapons/"); return; }

    ImGui::BeginChild("##wlist", ImVec2(180, 0), ImGuiChildFlags_Borders | ImGuiChildFlags_ResizeX);
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
    ImGui::TextDisabled("Nome arma:");
    ImGui::SetNextItemWidth(160);
    ImGui::InputText("##newwid", newWId, 64);
    ImGui::SameLine();
    if (ImGui::Button("+ Crea") && newWId[0] != '\0')
    {
        std::string path = getSourceDataDir() + "weapons/" + newWId + ".json";
        if (!fs::exists(path))
        { mini::WeaponDef def;
            def.id=newWId; def.name=newWId; def.faction=mini::Faction::Neutral;
            def.damage=25.0f; def.fireRate=4.5f; def.bulletSpeed=24.0f;
            def.bulletLifetime=3.0f; def.bulletScale=0.10f;
            def.bulletColor={0.3f,0.65f,1.0f};
            def.heatPerShot=0.10f; def.cooldownRate=0.30f; def.overheatPenalty=2.0f;
            def.effectiveRange=18.0f; def.minRange=0.0f;
            saveWeapon(def); m_selWeapon=newWId; }
        newWId[0] = '\0';
    }
    ImGui::Separator();

    auto it = weapons.find(m_selWeapon);
    if (it == weapons.end())
    { ImGui::TextDisabled("Seleziona un'arma."); ImGui::EndChild(); return; }
    if (editId != m_selWeapon) { edit = it->second; editId = m_selWeapon; }

    ImGui::Text("Arma: %s  [%s]", edit.name.c_str(), edit.id.c_str());
    ImGui::Separator();

    // Fazione
    {
        int fi = mini::factionToIndex(edit.faction);
        const char* const* fnames = mini::factionNames();
        if (ImGui::Combo("Fazione##w", &fi, fnames, 3)) edit.faction = mini::factionFromIndex(fi);
    }
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
    ImGui::Text("Gittata");
    ImGui::DragFloat("Range effettivo",  &edit.effectiveRange, 0.5f, 1.0f, 200.0f, "%.1f");
    ImGui::DragFloat("Range minimo",     &edit.minRange,       0.1f, 0.0f,  20.0f, "%.1f");

    ImGui::Separator();
    if (ImGui::CollapsingHeader("Precisione (spread in gradi)"))
    {
        ImGui::TextDisabled("0 = perfetto, valori tipici: 0.01-0.20");
        ImGui::DragFloat("Base (fermo)",   &edit.baseSpread,   0.001f, 0.0f, 1.0f, "%.4f");
        ImGui::DragFloat("In mira (ADS)",  &edit.adsSpread,    0.001f, 0.0f, 1.0f, "%.4f");
        ImGui::DragFloat("In movimento",   &edit.moveSpread,   0.001f, 0.0f, 1.0f, "%.4f");
        ImGui::DragFloat("In corsa",       &edit.sprintSpread, 0.001f, 0.0f, 1.0f, "%.4f");
        ImGui::DragFloat("In aria",        &edit.jumpSpread,   0.001f, 0.0f, 1.0f, "%.4f");

        // Barra visuale di confronto spread
        ImGui::Spacing();
        ImGui::TextDisabled("Anteprima relativa:");
        float maxS = std::max({edit.baseSpread, edit.adsSpread, edit.moveSpread, edit.sprintSpread, edit.jumpSpread, 0.001f});
        const char* labels[] = {"Base","ADS","Mov","Corsa","Aria"};
        float vals[]  = {edit.baseSpread, edit.adsSpread, edit.moveSpread, edit.sprintSpread, edit.jumpSpread};
        for (int i = 0; i < 5; ++i)
        {
            ImGui::Text("%-6s", labels[i]);
            ImGui::SameLine();
            ImGui::ProgressBar(vals[i] / maxS, {-1, 10});
        }
    }

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

// ── AI tab ───────────────────────────────────────────────────────────────

void BalanceEditor::drawAITab()
{
    const auto& profiles = m_registry.aiProfiles();
    if (profiles.empty()) { ImGui::TextDisabled("Nessun file in data/ai/"); return; }

    ImGui::BeginChild("##ailist", ImVec2(180, 0), ImGuiChildFlags_Borders | ImGuiChildFlags_ResizeX);
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

    ImGui::TextDisabled("Nome profilo AI:");
    ImGui::SetNextItemWidth(160); ImGui::InputText("##newaid", newAId, 64); ImGui::SameLine();
    if (ImGui::Button("+ Crea") && newAId[0] != '\0')
    {
        std::string path = getSourceDataDir() + "ai/" + newAId + ".json";
        if (!fs::exists(path))
        { mini::AiProfileDef def;
            def.id=newAId; def.role="infantry";
            def.sightRange=18.0f; def.fovDeg=110.0f; def.hearingRange=12.0f;
            def.reactionTime=0.4f; def.aggression=0.65f; def.accuracy=0.55f;
            def.coverPreference=0.75f; def.retreatHpThresh=0.25f;
            def.shootInterval=2.5f; def.patrolSpeed=2.5f; def.seekSpeed=4.0f;
            def.jumpEnabled=true;
            saveAI(def); m_selAI=newAId; }
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

// ── Map tab ──────────────────────────────────────────────────────────────

void BalanceEditor::saveMap(const mini::MapDef& m)
{
    std::string path = getSourceDataDir() + "maps/" + m.id + ".json";
    // saveJsonRMW (ADR-010): il file mappa contiene campi di ALTRI moduli
    // (geometry, command_posts, ally_*) — l'incidente 2026-07-08 nasce qui.
    editor::jsonsave::saveJsonRMW(path, [&](json& j) {
    j.erase("id"); // deprecato: id = nome file (ADR-001)
    j["name"]         = m.name;
    j["mesh"]         = m.meshPath;
    j["metadata"]     = m.metadataPath;
    j["navmesh"]      = m.navmeshPath;
    j["spawn_team1"]  = { m.spawnTeam1[0], m.spawnTeam1[1], m.spawnTeam1[2] };
    j["spawn_team2"]  = { m.spawnTeam2[0], m.spawnTeam2[1], m.spawnTeam2[2] };
    j["max_tickets"]  = m.maxTickets;
    j["enemy_count"]  = m.enemyCount;
    j["enemy_types"]  = m.enemyTypes;
    return true;
    });
    std::cout << "[Balance] Salvato: " << path << "\n";
    m_dirty = false;
    m_registry.reload(getSourceDataDir());
}

void BalanceEditor::drawMapsTab()
{
    const auto& maps = m_registry.maps();
    if (maps.empty()) { ImGui::TextDisabled("Nessun file in data/maps/"); return; }

    // ── Lista mappe (sinistra) ───────────────────────────────────────
    ImGui::BeginChild("##mlist", ImVec2(180, 0), ImGuiChildFlags_Borders | ImGuiChildFlags_ResizeX);
    for (auto& [id, m] : maps)
    {
        bool sel = (id == m_selMap);
        if (ImGui::Selectable(m.name.c_str(), sel)) m_selMap = id;
    }
    ImGui::EndChild();
    ImGui::SameLine();

    // ── Pannello edit (destra) ───────────────────────────────────────
    static mini::MapDef edit;
    static std::string editId;

    ImGui::BeginChild("##medit", ImVec2(0, 0), false);

    auto it = maps.find(m_selMap);
    if (it == maps.end())
    { ImGui::TextDisabled("Seleziona una mappa."); ImGui::EndChild(); return; }

    if (editId != m_selMap) { edit = it->second; editId = m_selMap; }

    ImGui::Text("Mappa: %s  [%s]", edit.name.c_str(), edit.id.c_str());
    ImGui::Separator();

    // Percorsi asset (solo testo, non editabile interattivamente per ora)
    ImGui::TextDisabled("Mesh:     %s", edit.meshPath.empty() ? "(nessuna)" : edit.meshPath.c_str());
    ImGui::TextDisabled("Metadata: %s", edit.metadataPath.empty() ? "(nessuno)" : edit.metadataPath.c_str());
    ImGui::Separator();

    // ── Ticket e conteggio ───────────────────────────────────────────
    if (ImGui::CollapsingHeader("Partita", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::DragInt("Ticket massimi",   &edit.maxTickets, 1, 1, 200);
        ImGui::DragInt("Nemici in campo",  &edit.enemyCount, 1, 1,  20);
    }

    // ── Spawn points ─────────────────────────────────────────────────
    if (ImGui::CollapsingHeader("Spawn Points", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::DragFloat3("Spawn Team 1 (X/Y/Z)", edit.spawnTeam1.data(), 0.1f, -50.f, 50.f, "%.2f");
        ImGui::DragFloat3("Spawn Team 2 (X/Y/Z)", edit.spawnTeam2.data(), 0.1f, -50.f, 50.f, "%.2f");
    }

    // ── Enemy types con dropdown ──────────────────────────────────────
    if (ImGui::CollapsingHeader("Tipi Nemici (enemy_types)", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::TextDisabled("Ordine = sequenza di spawn ciclica in partita.");
        ImGui::Spacing();

        const auto& enemies = m_registry.enemies();

        // Costruisce lista nomi per i dropdown
        std::vector<std::string> enemyIds;
        for (auto& [id, _] : enemies) enemyIds.push_back(id);

        for (int i = 0; i < (int)edit.enemyTypes.size(); ++i)
        {
            ImGui::PushID(i);
            ImGui::Text("[%d]", i);
            ImGui::SameLine();
            ImGui::SetNextItemWidth(220);
            const std::string& cur = edit.enemyTypes[i];
            if (ImGui::BeginCombo(("##et" + std::to_string(i)).c_str(),
                                  cur.empty() ? "-- seleziona --" : cur.c_str()))
            {
                for (auto& eid : enemyIds)
                {
                    const auto* edef = m_registry.getEnemy(eid);
                    std::string label = edef ? (edef->name + "  [" + eid + "]") : eid;
                    bool sel = (cur == eid);
                    if (ImGui::Selectable(label.c_str(), sel))
                        edit.enemyTypes[i] = eid;
                    if (sel) ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }
            ImGui::SameLine();

            // Preview colore nemico
            if (!cur.empty())
            {
                const auto* edef = m_registry.getEnemy(cur);
                if (edef)
                {
                    ImVec4 col(edef->color[0], edef->color[1], edef->color[2], 1.0f);
                    ImGui::ColorButton(("##cb" + std::to_string(i)).c_str(), col,
                                       ImGuiColorEditFlags_NoTooltip, ImVec2(18, 18));
                    ImGui::SameLine();
                }
            }

            if (ImGui::SmallButton("X##rm"))
            { edit.enemyTypes.erase(edit.enemyTypes.begin() + i); ImGui::PopID(); break; }
            ImGui::PopID();
        }

        ImGui::Spacing();
        if (ImGui::SmallButton("+ Aggiungi slot"))
            edit.enemyTypes.push_back(enemyIds.empty() ? "" : enemyIds[0]);

        // Bottoni per duplicare i pattern comuni
        ImGui::Spacing();
        ImGui::TextDisabled("Pattern rapidi:");
        if (!enemyIds.empty())
        {
            if (ImGui::SmallButton("Tutti uguali (primo tipo)"))
            {
                for (auto& et : edit.enemyTypes) et = enemyIds[0];
            }
            if (enemyIds.size() >= 2)
            {
                ImGui::SameLine();
                if (ImGui::SmallButton("Alterna 2 tipi"))
                {
                    for (int i = 0; i < (int)edit.enemyTypes.size(); ++i)
                        edit.enemyTypes[i] = enemyIds[i % 2];
                }
            }
        }
    }

    // ── Salvataggio ──────────────────────────────────────────────────
    ImGui::Spacing();
    ImGui::Separator();
    if (ImGui::Button("Salva", {120, 0}))     saveMap(edit);
    ImGui::SameLine();
    if (ImGui::Button("Ripristina", {120, 0})) edit = it->second;
    ImGui::SameLine();
    if (ImGui::Button("Ricarica tutto", {120, 0})) reload();

    ImGui::EndChild();
}

// ── Tab Alleati ──────────────────────────────────────────────────────────

// ── Tab Personaggio (solo stat base) ─────────────────────────────────────

void BalanceEditor::savePlayerDef(const mini::PlayerDef& p)
{
    std::string path = getSourceDataDir() + "characters/" + p.id + ".json";
    editor::jsonsave::saveJsonRMW(path, [&](json& j) {
        j.erase("id"); // deprecato: id = nome file (ADR-001)
        j["name"]        = p.name;
        j["description"] = p.description;
        j["stats"]["hp"]           = p.hp;
        j["stats"]["move_speed"]   = p.moveSpeed;
        j["stats"]["jump_height"]  = p.jumpHeight;
        j["stats"]["sprint_mult"]  = p.sprintMult;
        j["stats"]["armor_rating"] = p.armorRating;
        return true;
    });
    std::cout << "[Balance] PlayerDef salvato: " << path << "\n";
    m_registry.loadAll(getSourceDataDir());
}

void BalanceEditor::drawPlayerDefTab()
{
    ImGui::BeginGroup();
    ImGui::Text("Preset personaggio:");
    ImGui::BeginChild("##pdlist", ImVec2(180, -50), ImGuiChildFlags_Borders | ImGuiChildFlags_ResizeX);
    for (auto& [id, p] : m_registry.playerDefs())
    {
        bool sel = (id == m_selPlayerDef);
        if (ImGui::Selectable(p.name.c_str(), sel)) m_selPlayerDef = id;
    }
    ImGui::EndChild();

    static char newPdId[64] = "";
    ImGui::SetNextItemWidth(136); ImGui::InputText("##newpdid", newPdId, 64); ImGui::SameLine();
    if (ImGui::Button("+##pd") && newPdId[0] != '\0')
    {
        mini::PlayerDef p; p.id = newPdId; p.name = newPdId;
        savePlayerDef(p);
        m_selPlayerDef = newPdId;
        newPdId[0] = '\0';
    }
    ImGui::EndGroup();

    ImGui::SameLine();

    auto it = m_registry.playerDefs().find(m_selPlayerDef);
    if (it == m_registry.playerDefs().end())
    { ImGui::TextDisabled("Seleziona un preset."); return; }

    static std::string lastSelPd;
    if (lastSelPd != m_selPlayerDef) { m_editPlayerDef = it->second; lastSelPd = m_selPlayerDef; }
    mini::PlayerDef& edit = m_editPlayerDef;

    ImGui::BeginChild("##pdedit", ImVec2(0, 0), true);
    ImGui::TextColored({0.7f,0.9f,1.0f,1.0f},
        "Nota: equipaggiamento, armatura e colori vengono scelti nel PreMatch.");
    ImGui::Spacing();

    char buf[256];
    std::strncpy(buf, edit.name.c_str(), 255);
    if (ImGui::InputText("Nome", buf, 255)) edit.name = buf;

    std::strncpy(buf, edit.description.c_str(), 255);
    if (ImGui::InputText("Descrizione", buf, 255)) edit.description = buf;

    ImGui::Separator(); ImGui::Text("Stat base");
    ImGui::DragFloat("HP base",        &edit.hp,          1.0f, 50.0f, 500.0f, "%.0f");
    ImGui::DragFloat("Velocità",       &edit.moveSpeed,   0.1f,  1.0f,  20.0f, "%.1f");
    ImGui::DragFloat("Salto (mult)",   &edit.jumpHeight,  0.05f, 0.5f,   3.0f, "x%.2f");
    ImGui::DragFloat("Scatto (mult)",  &edit.sprintMult,  0.05f, 1.0f,   3.0f, "x%.2f");
    ImGui::DragFloat("Armatura base",  &edit.armorRating, 0.05f, 0.0f,   3.0f, "%.2f");
    ImGui::TextDisabled("  0=nessuna  1=standard  2=pesante");

    ImGui::Spacing(); ImGui::Separator();
    if (ImGui::Button("Salva", {120,0})) { savePlayerDef(edit); lastSelPd.clear(); }
    ImGui::SameLine();
    if (ImGui::Button("Ripristina", {120,0})) edit = it->second;
    ImGui::SameLine();
    if (ImGui::Button("Ricarica tutto", {120,0})) { reload(); lastSelPd.clear(); }

    ImGui::EndChild();
}

// ── Draw principale ──────────────────────────────────────────────────────

void BalanceEditor::draw()
{
    if (ImGui::BeginTabBar("##btabs"))
    {
        if (ImGui::BeginTabItem("Armi"))        { drawWeaponsTab();    ImGui::EndTabItem(); }
        if (ImGui::BeginTabItem("AI"))          { drawAITab();         ImGui::EndTabItem(); }
        if (ImGui::BeginTabItem("Mappe"))       { drawMapsTab();       ImGui::EndTabItem(); }
        if (ImGui::BeginTabItem("Personaggio")) { drawPlayerDefTab();  ImGui::EndTabItem(); }
        if (ImGui::BeginTabItem("Abilita'"))    { drawAbilitiesTab();  ImGui::EndTabItem(); }
        ImGui::EndTabBar();
    }
}

} // namespace editor