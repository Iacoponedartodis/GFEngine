#include "util/DataPath.hpp"
#include "modules/BalanceEditor.hpp"
#include "util/JsonSave.hpp"
#include "util/FileDialog.hpp"
#include "util/DefinitionRename.hpp"
#include "util/UiWidgets.hpp"   // righe etichetta-a-sinistra: label mai tagliata
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
    // R8 chiuso: unica risoluzione in util/DataPath (prima ogni modulo aveva
    // la sua, e le copie erano già divergenti).
    return editor::datapath::dir();
}


BalanceEditor::BalanceEditor() { reload(); }

void BalanceEditor::reload()
{
    m_registry.loadAll(getSourceDataDir());
    // Bilanciamento globale (ADR-043): stessa load del runtime → stessi valori.
    // File assente → default (= vecchie costanti compile-time).
    mini::mutableGameplayBalance() = mini::GameplayBalance{};
    mini::loadGameplayBalance(getSourceDataDir() + "config/gameplay.json");
    m_gameplay = mini::gameplay();
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
        if (editor::ui::textRow("Nome", nameBuf, sizeof(nameBuf)))
            edit.name = nameBuf;
    }

    // Tipo dall'elenco supportato (niente testo libero)
    {
        static const char* kTypes[] =
            {"shield", "roll", "melee", "jetpack", "missile", "command"};
        int ti = 0;
        for (int i = 0; i < 6; ++i) if (edit.type == kTypes[i]) { ti = i; break; }
        if (editor::ui::comboRow("Tipo", ti, kTypes, 6)) edit.type = kTypes[ti];
    }
    ImGui::TextDisabled("Runtime attivo: 'shield' (16), 'roll' (Combat Roll), 'command'\n"
                        "(Droide Tattico, ADR-024/doc 32). 'melee'/'jetpack'/'missile'\n"
                        "sono autorabili ma non ancora consumati in partita.");
    ImGui::Separator();

    // Parametri con etichette contestuali per il tipo shield
    const bool isShield = (edit.type == "shield");
    editor::ui::dragRow(isShield ? "HP scudo (param1)"    : "param1", edit.param1, 1.0f,  0.0f, 1000.0f, "%.1f");
    editor::ui::dragRow(isShield ? "Rigenerazione/s (param2)" : "param2", edit.param2, 0.1f,  0.0f,  100.0f, "%.1f");
    editor::ui::dragRow(isShield ? "Ritardo regen s (param3)" : "param3", edit.param3, 0.1f,  0.0f,   30.0f, "%.1f");
    editor::ui::dragRow("Cooldown (s)", edit.cooldown, 0.1f, 0.0f, 60.0f, "%.1f");
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
        if (editor::ui::comboRow("Fazione##w", fi, fnames, 3)) edit.faction = mini::factionFromIndex(fi);
    }
    ImGui::Separator();

    editor::ui::dragRow("Danno", edit.damage,          0.5f, 1.0f, 200.0f, "%.1f");
    editor::ui::dragRow("Cadenza (rnd/s)", edit.fireRate,         0.1f, 0.1f,  30.0f, "%.2f");
    editor::ui::dragRow("Vel. proiettile", edit.bulletSpeed,      0.5f, 1.0f, 100.0f, "%.1f");
    editor::ui::dragRow("Vita proiettile", edit.bulletLifetime,   0.1f, 0.1f,  10.0f, "%.2f");
    editor::ui::dragRow("Scala proiettile", edit.bulletScale,      0.005f,0.01f, 1.0f, "%.3f");
    editor::ui::colorRow("Colore proiettile", edit.bulletColor.data());
    ImGui::Separator();
    ImGui::Text("Calore");
    editor::ui::dragRow("Calore/colpo", edit.heatPerShot,     0.005f,0.01f, 1.0f, "%.3f");
    editor::ui::dragRow("Raffreddamento", edit.cooldownRate,    0.005f,0.01f, 2.0f, "%.3f");
    editor::ui::dragRow("Penalità overheat", edit.overheatPenalty,0.1f, 0.0f,  10.0f, "%.2f");

    ImGui::Separator();
    ImGui::Text("Gittata");
    editor::ui::dragRow("Range effettivo", edit.effectiveRange, 0.5f, 1.0f, 200.0f, "%.1f");
    editor::ui::dragRow("Range minimo (non attivo)", edit.minRange, 0.1f, 0.0f, 20.0f, "%.1f");
    ImGui::TextDisabled("(non attivo) = salvato ma non ancora consumato dal runtime — KI #25");

    ImGui::Separator();
    if (ImGui::CollapsingHeader("Precisione (spread in gradi)"))
    {
        ImGui::TextDisabled("0 = perfetto, valori tipici: 0.01-0.20");
        editor::ui::dragRow("Base (fermo)", edit.baseSpread,   0.001f, 0.0f, 1.0f, "%.4f");
        editor::ui::dragRow("In mira (ADS)", edit.adsSpread,    0.001f, 0.0f, 1.0f, "%.4f");
        editor::ui::dragRow("In movimento", edit.moveSpread,   0.001f, 0.0f, 1.0f, "%.4f");
        editor::ui::dragRow("In corsa", edit.sprintSpread, 0.001f, 0.0f, 1.0f, "%.4f");
        editor::ui::dragRow("In aria", edit.jumpSpread,   0.001f, 0.0f, 1.0f, "%.4f");

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
        editor::ui::dragRow("Vista (range)", edit.sightRange,   0.5f,  1.0f, 100.0f, "%.1f");
        editor::ui::dragRow("Campo visivo° (non attivo)", edit.fovDeg, 1.0f, 10.0f, 360.0f, "%.0f");
        editor::ui::dragRow("Udito (non attivo)", edit.hearingRange, 0.5f, 0.0f, 50.0f, "%.1f");
        editor::ui::dragRow("Reazione (sec)", edit.reactionTime, 0.01f, 0.0f,   3.0f, "%.2f");
        ImGui::TextDisabled("(non attivo) = salvato ma non ancora consumato dal runtime — KI #25");
    }
    if (ImGui::CollapsingHeader("Comportamento", ImGuiTreeNodeFlags_DefaultOpen)) {
        editor::ui::sliderRowLR("Aggressività", edit.aggression,   0.0f, 1.0f);
        editor::ui::sliderRowLR("Precisione", edit.accuracy,     0.0f, 1.0f);
        editor::ui::sliderRowLR("Pref. copertura", edit.coverPreference,0.0f, 1.0f);
        editor::ui::sliderRowLR("HP ritiro", edit.retreatHpThresh,0.0f, 1.0f);
        editor::ui::sliderRowLR("Fiancheggia", edit.flankChance,  0.0f, 1.0f);
        editor::ui::sliderRowLR("Riposiziona (non attivo)", edit.repositionChance, 0.0f, 1.0f);
    }
    if (ImGui::CollapsingHeader("Copertura e fuoco")) {
        editor::ui::dragRow("Peek min (s)", edit.peekDurationMin, 0.05f,0.0f, 5.0f, "%.2f");
        editor::ui::dragRow("Peek max (s)", edit.peekDurationMax, 0.05f,0.0f, 5.0f, "%.2f");
        editor::ui::dragRow("Nascondi min (s)", edit.hideDurationMin, 0.05f,0.0f,10.0f, "%.2f");
        editor::ui::dragRow("Nascondi max (s)", edit.hideDurationMax, 0.05f,0.0f,10.0f, "%.2f");
        editor::ui::dragRow("Intervallo sparo", edit.shootInterval,   0.05f,0.1f,10.0f, "%.2f");
    }
    if (ImGui::CollapsingHeader("Movimento")) {
        editor::ui::dragRow("Vel. pattuglia", edit.patrolSpeed, 0.1f, 0.5f, 15.0f, "%.2f");
        editor::ui::dragRow("Vel. inseguimento", edit.seekSpeed,  0.1f, 0.5f, 20.0f, "%.2f");
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
    j["spawn_team1"]  = { m.spawnTeam1[0], m.spawnTeam1[1], m.spawnTeam1[2] };
    j["spawn_team2"]  = { m.spawnTeam2[0], m.spawnTeam2[1], m.spawnTeam2[2] };
    j["max_tickets"]  = m.maxTickets;
    j["enemy_count"]  = m.enemyCount;
    j["ally_count"]   = m.allyCount;
    j["enemy_types"]  = m.enemyTypes;
    j["ally_types"]   = m.allyTypes;
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
        editor::ui::intRow("Ticket massimi", edit.maxTickets, 1, 1, 200);
        editor::ui::intRow("Nemici in campo", edit.enemyCount, 1, 1,  20);
        editor::ui::intRow("Alleati in campo", edit.allyCount,  1, 0,  20);
    }

    // ── Spawn points ─────────────────────────────────────────────────
    if (ImGui::CollapsingHeader("Spawn Points", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::DragFloat3("Spawn Team 1 (X/Y/Z)", edit.spawnTeam1.data(), 0.1f, -50.f, 50.f, "%.2f");
        ImGui::DragFloat3("Spawn Team 2 (X/Y/Z)", edit.spawnTeam2.data(), 0.1f, -50.f, 50.f, "%.2f");
    }

    // ── Roster per team: UI unica per enemy_types e ally_types ─────────
    // Vuoto = "auto": il runtime usa TUTTE le definizioni registrate
    // (fallback ADR-007) — le nuove entità entrano in partita da sole.
    auto drawRoster = [&](const char* header, const char* tag,
                          std::vector<std::string>& types, bool isAllyList)
    {
        if (!ImGui::CollapsingHeader(header, ImGuiTreeNodeFlags_DefaultOpen))
            return;

        ImGui::TextDisabled("Ordine = sequenza di spawn ciclica in partita.");
        ImGui::TextDisabled("Lista VUOTA = automatico: tutte le definizioni registrate.");
        ImGui::Spacing();

        std::vector<std::string> ids;
        if (isAllyList) { for (auto& [id, _] : m_registry.allies())  ids.push_back(id); }
        else            { for (auto& [id, _] : m_registry.enemies()) ids.push_back(id); }

        auto defOf = [&](const std::string& id) -> const mini::EnemyDef* {
            return isAllyList ? m_registry.getAlly(id) : m_registry.getEnemy(id);
        };

        for (int i = 0; i < (int)types.size(); ++i)
        {
            ImGui::PushID(tag); ImGui::PushID(i);
            ImGui::Text("[%d]", i);
            ImGui::SameLine();
            ImGui::SetNextItemWidth(220);
            const std::string& cur = types[i];
            if (ImGui::BeginCombo("##slot",
                                  cur.empty() ? "-- seleziona --" : cur.c_str()))
            {
                for (auto& eid : ids)
                {
                    const auto* edef = defOf(eid);
                    std::string label = edef ? (edef->name + "  [" + eid + "]") : eid;
                    bool sel = (cur == eid);
                    if (ImGui::Selectable(label.c_str(), sel)) types[i] = eid;
                    if (sel) ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }
            ImGui::SameLine();

            if (!cur.empty())
            {
                if (const auto* edef = defOf(cur))
                {
                    ImVec4 col(edef->color[0], edef->color[1], edef->color[2], 1.0f);
                    ImGui::ColorButton("##cb", col,
                                       ImGuiColorEditFlags_NoTooltip, ImVec2(18, 18));
                    ImGui::SameLine();
                }
            }

            if (ImGui::SmallButton("X##rm"))
            { types.erase(types.begin() + i); ImGui::PopID(); ImGui::PopID(); break; }
            ImGui::PopID(); ImGui::PopID();
        }

        ImGui::Spacing();
        ImGui::PushID(tag);
        if (ImGui::SmallButton("+ Aggiungi slot"))
            types.push_back(ids.empty() ? "" : ids[0]);

        ImGui::Spacing();
        ImGui::TextDisabled("Pattern rapidi:");
        if (!ids.empty())
        {
            if (ImGui::SmallButton("Tutti uguali (primo tipo)"))
                for (auto& et : types) et = ids[0];
            if (ids.size() >= 2)
            {
                ImGui::SameLine();
                if (ImGui::SmallButton("Alterna 2 tipi"))
                    for (int i = 0; i < (int)types.size(); ++i)
                        types[i] = ids[i % 2];
            }
            ImGui::SameLine();
            if (ImGui::SmallButton("Uno per ogni definizione"))
            {
                types = ids;   // round-robin naturale su tutto il registrato
            }
            ImGui::SameLine();
            if (ImGui::SmallButton("Svuota (auto)"))
                types.clear();
        }
        ImGui::PopID();
    };

    drawRoster("Tipi Nemici (enemy_types)", "en", edit.enemyTypes, false);
    drawRoster("Tipi Alleati (ally_types)", "al", edit.allyTypes, true);

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
    if (editor::ui::textRow("Nome", buf, 255)) edit.name = buf;

    std::strncpy(buf, edit.description.c_str(), 255);
    if (editor::ui::textRow("Descrizione", buf, 255)) edit.description = buf;

    ImGui::Separator(); ImGui::Text("Stat base");
    editor::ui::dragRow("HP base", edit.hp,          1.0f, 50.0f, 500.0f, "%.0f");
    editor::ui::dragRow("Velocità", edit.moveSpeed,   0.1f,  1.0f,  20.0f, "%.1f");
    editor::ui::dragRow("Salto (mult)", edit.jumpHeight,  0.05f, 0.5f,   3.0f, "x%.2f");
    editor::ui::dragRow("Scatto (mult)", edit.sprintMult,  0.05f, 1.0f,   3.0f, "x%.2f");
    editor::ui::dragRow("Armatura base", edit.armorRating, 0.05f, 0.0f,   3.0f, "%.2f");
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

// ── Gameplay globale (ADR-043) ───────────────────────────────────────────
// I parametri di sensazione che erano `constexpr` (ricompilare per tararli):
// ora vivono in data/config/gameplay.json, letti dal runtime all'avvio.
void BalanceEditor::saveGameplay()
{
    const std::string path = getSourceDataDir() + "config/gameplay.json";
    editor::jsonsave::saveJsonRMW(path, [&](json& j) {
        for (auto& [k, v] : mini::gameplayBalanceToJson(m_gameplay).items())
            j[k] = v;
        return true;
    });
    std::cout << "[Balance] Gameplay salvato: " << path << "\n";
}

void BalanceEditor::drawGameplayTab()
{
    auto& g = m_gameplay;
    bool ch = false;

    ImGui::TextDisabled("Parametri GLOBALI di gameplay (data/config/gameplay.json).");
    ImGui::TextDisabled("Il runtime li carica all'avvio: salva e riavvia la partita per provarli.");
    ImGui::Separator();

    ImGui::TextColored({0.55f,0.85f,0.55f,1.0f}, "Squadra: a terra e rianimazione (doc 26)");
    ch |= editor::ui::sliderRow("Bleed-out (s)",  g.squadBleedoutTime, 3.f, 60.f, 0.5f, "%.0f s", 150.0f);
    ImGui::TextDisabled("Quanto resta a terra prima della morte definitiva.");
    ch |= editor::ui::sliderRow("Raggio soccorso (m)", g.squadReviveRadius, 1.f, 8.f, 0.1f, "%.1f m", 150.0f);
    ch |= editor::ui::sliderRow("Canalizzazione (s)",  g.squadReviveTime, 1.f, 30.f, 0.5f, "%.1f s", 150.0f);
    ImGui::TextDisabled("Secondi di presenza continua per completare la rianimazione.");
    ch |= editor::ui::sliderRow("HP al risveglio",     g.squadReviveHp, 0.05f, 1.f, 0.01f, "%.2f", 150.0f);
    ch |= editor::ui::sliderRow("Colpo letale (fraz.)", g.squadDownLethalHitFrac, 0.05f, 1.f, 0.01f, "%.2f", 150.0f);
    ImGui::TextDisabled("Un colpo che toglie almeno questa frazione degli HP max\n"
                        "uccide sul posto invece di mettere a terra.");
    {
        int mr = g.squadMaxRevives;
        ImGui::SetNextItemWidth(120.0f);
        if (ImGui::InputInt("Rianimazioni per vita", &mr))
        { g.squadMaxRevives = (mr < 0) ? 0 : (mr > 9 ? 9 : mr); ch = true; }
        ImGui::TextDisabled("Oltre il cap la caduta e' LETALE. 0 = mai a terra,\n"
                            "chi cade muore. Si azzera col respawn.");
    }

    ImGui::Separator();
    ImGui::TextColored({0.55f,0.75f,0.95f,1.0f}, "Rete di comunicazione: degrado senza torre (doc 34)");
    ImGui::TextDisabled("Quanto peggiora una fazione che ha PERSO la sua torre.\n"
                        "Nessun valore blocca nulla: la regola e' RALLENTARE.");
    ch |= editor::ui::sliderRow("Raggio contatti (x)",  g.commsLostRangeMult, 0.1f, 1.f, 0.05f, "x%.2f", 150.0f);
    ch |= editor::ui::sliderRow("Ritardo avviso (s)",   g.commsLostShareDelay, 0.f, 10.f, 0.1f, "%.1f s", 150.0f);
    ImGui::TextDisabled("L'avvistamento arriva ai compagni in ritardo: si accorre\n"
                        "dove il nemico ERA.");
    ch |= editor::ui::sliderRow("Cadenza ordini (x)",   g.commsLostOrderMult, 1.f, 6.f, 0.1f, "x%.1f", 150.0f);
    ch |= editor::ui::sliderRow("Ritardo rinforzi (x)", g.commsLostReinforceMult, 1.f, 4.f, 0.05f, "x%.2f", 150.0f);

    if (ch) m_dirty = true;

    ImGui::Separator();
    if (ImGui::Button("Salva gameplay")) { saveGameplay(); m_dirty = false; }
    ImGui::SameLine();
    if (ImGui::Button("Ripristina default"))
    { m_gameplay = mini::GameplayBalance{}; m_dirty = true; }
    if (m_dirty) { ImGui::SameLine(); ImGui::TextColored({1.f,0.8f,0.3f,1.f}, "modifiche non salvate"); }
}

void BalanceEditor::draw()
{
    if (ImGui::BeginTabBar("##btabs"))
    {
        // Armi: modulo dedicato "Weapon Editor" (con viewport 3D). Rimossa
        // da qui per evitare due UI che editano gli stessi file senza
        // live-sync (confusione utente 2026-07-11).
        if (ImGui::BeginTabItem("AI"))          { drawAITab();         ImGui::EndTabItem(); }
        if (ImGui::BeginTabItem("Mappe"))       { drawMapsTab();       ImGui::EndTabItem(); }
        if (ImGui::BeginTabItem("Personaggio")) { drawPlayerDefTab();  ImGui::EndTabItem(); }
        if (ImGui::BeginTabItem("Abilita'"))    { drawAbilitiesTab();  ImGui::EndTabItem(); }
        if (ImGui::BeginTabItem("Gameplay"))    { drawGameplayTab();   ImGui::EndTabItem(); }
        // Veicoli: modulo dedicato "Vehicle Editor" (19_Vehicles)
        ImGui::EndTabBar();
    }
}

} // namespace editor