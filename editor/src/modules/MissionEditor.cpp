// MissionEditor — authoring missioni/obiettivi/conseguenze (ADR-019, doc 25).
#include "util/DataPath.hpp"
#include "modules/MissionEditor.hpp"
#include "util/JsonSave.hpp"
#include "util/DefinitionRename.hpp"
#include "util/UiWidgets.hpp"   // righe etichetta-a-sinistra: label mai tagliata

#include <imgui.h>
#include <SDL2/SDL.h>

#include <algorithm>
#include <filesystem>

namespace editor
{
namespace fs = std::filesystem;
namespace
{

// Radice dati sorgente. DEBITO NOTO (06_Todo R8): duplicata in ogni modulo.
// R8 chiuso: unica risoluzione in util/DataPath.
std::string dataDir() { return editor::datapath::dir(); }

// ── Tabelle enum→stringa: UNA sola volta, e nell'ordine dei dropdown ─────
// Le stringhe DEVONO combaciare con quelle che il loader parsa
// (DefinitionRegistry): sono il contratto sui dati, non un'etichetta UI.
const char* const kObjectiveTypes[] = {
    "reach_area", "eliminate_target", "hold_area_for_duration",
    "capture_zone", "defend_zone",
    "destroy_target", "escort_entity", "survive_wave", "interact_hack"
};
constexpr int kObjectiveTypeCount = 9;

// Quelli oltre il quinto sono dichiarati dal doc 25 ma NON eseguiti dal runtime:
// l'editor lo dice invece di lasciar autorare un obiettivo che fallirà sempre.
bool typeIsImplemented(int idx) { return idx <= 4; }

const char* const kTiers[]       = { "primary", "strategic", "tactical" };
const char* const kActivations[] = { "immediate", "after_objective", "after_time" };
const char* const kMissionRules[] = { "all_primary_complete", "any_primary_complete",
                                      "any_primary_failed", "time_limit" };
const char* const kConsequences[] = { "block_enemy_reinforcements", "enemy_accuracy",
                                      "ally_reinforcements", "unlock_spawn" };
constexpr int kConsequenceCount = 4;

int indexOf(const char* const* arr, int n, const std::string& v, int fallback = 0)
{
    for (int i = 0; i < n; ++i) if (v == arr[i]) return i;
    return fallback;
}

const char* objectiveTypeToString(mini::ObjectiveType t)
{
    switch (t) {
        case mini::ObjectiveType::ReachArea:           return "reach_area";
        case mini::ObjectiveType::EliminateTarget:     return "eliminate_target";
        case mini::ObjectiveType::HoldAreaForDuration: return "hold_area_for_duration";
        case mini::ObjectiveType::CaptureZone:         return "capture_zone";
        case mini::ObjectiveType::DefendZone:          return "defend_zone";
        case mini::ObjectiveType::DestroyTarget:       return "destroy_target";
        case mini::ObjectiveType::EscortEntity:        return "escort_entity";
        case mini::ObjectiveType::SurviveWave:         return "survive_wave";
        case mini::ObjectiveType::InteractHack:        return "interact_hack";
    }
    return "reach_area";
}
const char* tierToString(mini::ObjectiveTier t)
{
    switch (t) {
        case mini::ObjectiveTier::Primary:   return "primary";
        case mini::ObjectiveTier::Strategic: return "strategic";
        default:                             return "tactical";
    }
}
const char* activationToString(mini::ActivationType t)
{
    switch (t) {
        case mini::ActivationType::AfterObjective: return "after_objective";
        case mini::ActivationType::AfterTime:      return "after_time";
        default:                                   return "immediate";
    }
}
const char* missionRuleToString(mini::MissionRule r)
{
    switch (r) {
        case mini::MissionRule::AnyPrimaryComplete: return "any_primary_complete";
        case mini::MissionRule::AnyPrimaryFailed:   return "any_primary_failed";
        case mini::MissionRule::TimeLimit:          return "time_limit";
        default:                                    return "all_primary_complete";
    }
}
const char* consequenceToString(mini::ConsequenceType t)
{
    switch (t) {
        case mini::ConsequenceType::BlockEnemyReinforcements: return "block_enemy_reinforcements";
        case mini::ConsequenceType::EnemyAccuracy:            return "enemy_accuracy";
        case mini::ConsequenceType::AllyReinforcements:       return "ally_reinforcements";
        case mini::ConsequenceType::UnlockSpawn:              return "unlock_spawn";
        default:                                              return "block_enemy_reinforcements";
    }
}

} // namespace

MissionEditor::MissionEditor() { reload(); }

void MissionEditor::reload()
{
    m_registry.loadAll(dataDir());
    m_missions.clear();
    m_objectives.clear();

    for (const auto& [id, d] : m_registry.objectives())
        m_objectives.push_back({id, dataDir() + "objectives/" + id + ".json", d});
    for (const auto& [id, d] : m_registry.missions())
        m_missions.push_back({id, dataDir() + "missions/" + id + ".json", d});

    std::sort(m_objectives.begin(), m_objectives.end(),
              [](const auto& a, const auto& b) { return a.id < b.id; });
    std::sort(m_missions.begin(), m_missions.end(),
              [](const auto& a, const auto& b) { return a.id < b.id; });

    // Riselezione dopo rinomina/creazione: l'id è cambiato, l'indice no.
    if (!m_pendingSelectId.empty())
    {
        for (int i = 0; i < (int)m_objectives.size(); ++i)
            if (m_objectives[i].id == m_pendingSelectId) m_selObjective = i;
        for (int i = 0; i < (int)m_missions.size(); ++i)
            if (m_missions[i].id == m_pendingSelectId) m_selMission = i;
        m_pendingSelectId.clear();
    }
    m_selMission   = std::min(m_selMission,   (int)m_missions.size()   - 1);
    m_selObjective = std::min(m_selObjective, (int)m_objectives.size() - 1);
}

std::vector<std::string> MissionEditor::postsOfMap(const std::string& mapId) const
{
    std::vector<std::string> out;
    if (const mini::MapDef* m = m_registry.getMap(mapId))
        for (const auto& cp : m->commandPosts) out.push_back(cp.label);
    return out;
}

// ── Salvataggio: SEMPRE RMW (ADR-010) ────────────────────────────────────
// Si scrivono solo i campi che questo modulo possiede. `id` NON si scrive mai:
// è il filename (ADR-001), e un id in-file stantio ha già rotto le cross-ref
// in silenzio una volta (KI #21).
void MissionEditor::saveMission(const MissionEntry& e)
{
    const bool ok = jsonsave::saveJsonRMW(e.jsonPath, [&](nlohmann::json& j)
    {
        j["name"]     = e.def.name;
        j["briefing"] = e.def.briefing;
        j["map"]      = e.def.mapId;
        j["mode"]     = e.def.modeId;
        j["primary_objectives"]  = e.def.primaryObjectives;
        j["optional_objectives"] = e.def.optionalObjectives;
        j["success_rules"] = { {"rule", missionRuleToString(e.def.successRule)} };
        nlohmann::json fr = { {"rule", missionRuleToString(e.def.failureRule)} };
        if (e.def.failureRule == mini::MissionRule::TimeLimit)
            fr["time_limit"] = e.def.failureTimeLimit;
        j["failure_rules"] = fr;
        return true;
    });
    m_status = ok ? ("Salvato: " + e.id) : ("ERRORE salvataggio: " + e.id);
    if (ok) m_dirty = false;
}

void MissionEditor::saveObjective(const ObjectiveEntry& e)
{
    auto consequencesToJson = [](const std::vector<mini::ConsequenceDef>& list)
    {
        nlohmann::json arr = nlohmann::json::array();
        for (const auto& c : list)
        {
            nlohmann::json o = { {"type", consequenceToString(c.type)} };
            // Si scrivono solo i campi che il tipo USA: un `target` su
            // enemy_accuracy sarebbe rumore che il gate segnalerebbe.
            if (c.type == mini::ConsequenceType::EnemyAccuracy
                || c.type == mini::ConsequenceType::AllyReinforcements)
                o["value"] = c.value;
            if (c.type == mini::ConsequenceType::UnlockSpawn)
                o["target"] = c.target;
            arr.push_back(o);
        }
        return arr;
    };

    const bool ok = jsonsave::saveJsonRMW(e.jsonPath, [&](nlohmann::json& j)
    {
        const auto& d = e.def;
        j["name"] = d.name;
        j["type"] = objectiveTypeToString(d.type);
        j["tier"] = tierToString(d.tier);

        nlohmann::json t = j.contains("target") && j["target"].is_object()
                         ? j["target"] : nlohmann::json::object();
        // Si scrivono SOLO i campi che il tipo usa davvero. `actor_team` non vale
        // per EliminateTarget (conta qualsiasi kill del team bersaglio):
        // scriverlo suggerirebbe una capacità che non esiste — è il pattern di
        // KI #25, e succedeva davvero (thin_the_garrison si è portato dietro un
        // actor_team inerte al primo salvataggio dall'editor).
        switch (d.type)
        {
        case mini::ObjectiveType::ReachArea:
        case mini::ObjectiveType::HoldAreaForDuration:
            t["actor_team"] = d.actorTeam;
            t["x"] = d.x; t["z"] = d.z; t["radius"] = d.radius;
            if (d.type == mini::ObjectiveType::HoldAreaForDuration)
                t["hold_seconds"] = d.holdSeconds;
            break;
        case mini::ObjectiveType::EliminateTarget:
            t.erase("actor_team");
            t["target_team"] = d.targetTeam; t["count"] = d.count;
            break;
        case mini::ObjectiveType::CaptureZone:
            t["actor_team"] = d.actorTeam;
            t["post"] = d.targetPost;
            break;
        case mini::ObjectiveType::DefendZone:
            t["actor_team"] = d.actorTeam;
            t["post"] = d.targetPost; t["hold_seconds"] = d.holdSeconds;
            break;
        default: break;   // tipi non ancora eseguiti dal runtime
        }
        j["target"] = t;

        nlohmann::json a = { {"type", activationToString(d.activation)} };
        if (d.activation == mini::ActivationType::AfterObjective)
            a["objective"] = d.activationObjective;
        if (d.activation == mini::ActivationType::AfterTime)
            a["time"] = d.activationTime;
        j["activation"] = a;

        if (d.timeLimit > 0.0f) j["time_limit"] = d.timeLimit; else j.erase("time_limit");
        j["reward"] = d.reward;
        if (!d.onSuccess.empty()) j["on_success"] = consequencesToJson(d.onSuccess);
        else                      j.erase("on_success");
        if (!d.onFailure.empty()) j["on_failure"] = consequencesToJson(d.onFailure);
        else                      j.erase("on_failure");
        return true;
    });
    m_status = ok ? ("Salvato: " + e.id) : ("ERRORE salvataggio: " + e.id);
    if (ok) m_dirty = false;
}

// ── Conseguenze: la parte che rende un obiettivo una MOSSA ───────────────
void MissionEditor::drawConsequences(std::vector<mini::ConsequenceDef>& list,
                                     const char* label,
                                     const std::string& mapIdForPosts)
{
    ImGui::PushID(label);
    ImGui::SeparatorText(label);

    int toRemove = -1;
    for (int i = 0; i < (int)list.size(); ++i)
    {
        ImGui::PushID(i);
        auto& c = list[i];
        int ti = indexOf(kConsequences, kConsequenceCount, consequenceToString(c.type));
        ImGui::SetNextItemWidth(220.0f);
        if (editor::ui::comboRow("Tipo", ti, kConsequences, kConsequenceCount))
        {
            switch (ti) {
            case 0: c.type = mini::ConsequenceType::BlockEnemyReinforcements; break;
            case 1: c.type = mini::ConsequenceType::EnemyAccuracy;
                    if (c.value <= 0.0f || c.value > 1.0f) c.value = 0.6f; break;
            case 2: c.type = mini::ConsequenceType::AllyReinforcements;
                    if (c.value <= 0.0f) c.value = 2.0f; break;
            case 3: c.type = mini::ConsequenceType::UnlockSpawn; break;
            }
        }
        // Ogni tipo mostra SOLO i campi che usa: un `value` su
        // block_enemy_reinforcements confonderebbe (non lo legge nessuno).
        if (c.type == mini::ConsequenceType::EnemyAccuracy)
        {
            ImGui::SameLine(); ImGui::SetNextItemWidth(110.0f);
            editor::ui::sliderRowLR("moltiplicatore", c.value, 0.05f, 1.0f, "%.2f");
            ImGui::TextDisabled("  precisione nemica x%.2f (1 = nessun effetto)", c.value);
        }
        else if (c.type == mini::ConsequenceType::AllyReinforcements)
        {
            ImGui::SameLine(); ImGui::SetNextItemWidth(110.0f);
            int v = (int)c.value;
            if (editor::ui::inputIntRow("riserve", v)) c.value = (float)std::max(0, v);
        }
        else if (c.type == mini::ConsequenceType::UnlockSpawn)
        {
            // Post SOLO dalla mappa della missione: un id a testo libero qui
            // sarebbe un riferimento rotto (CLAUDE.md: dropdown, mai testo).
            const auto posts = postsOfMap(mapIdForPosts);
            std::vector<const char*> items;
            for (const auto& p : posts) items.push_back(p.c_str());
            int pi = 0;
            for (int k = 0; k < (int)posts.size(); ++k) if (posts[k] == c.target) pi = k;
            ImGui::SameLine(); ImGui::SetNextItemWidth(150.0f);
            if (items.empty())
                ImGui::TextColored({1.f,0.6f,0.3f,1.f},
                                   "(la mappa non ha command post)");
            else if (editor::ui::comboRow("post", pi, items.data(), (int)items.size()))
                c.target = posts[pi];
            if (!items.empty() && c.target.empty()) c.target = posts[pi];
        }
        ImGui::SameLine();
        if (ImGui::SmallButton("X")) toRemove = i;
        ImGui::PopID();
    }
    if (toRemove >= 0) list.erase(list.begin() + toRemove);
    if (ImGui::SmallButton("+ conseguenza"))
        list.push_back({mini::ConsequenceType::BlockEnemyReinforcements, 0.0f, ""});
    ImGui::PopID();
}

void MissionEditor::savePending()
{
    // Salva la voce selezionata del tab corrente: il modulo ne autora due tipi
    // indipendenti (missioni e obiettivi) e non ha un "salva tutto".
    if (!m_dirty) return;
    if (m_tab == Tab::Missions)
    { if (m_selMission >= 0 && m_selMission < (int)m_missions.size()) saveMission(m_missions[m_selMission]); }
    else
    { if (m_selObjective >= 0 && m_selObjective < (int)m_objectives.size()) saveObjective(m_objectives[m_selObjective]); }
}

void MissionEditor::draw()
{
    // Rilevamento del lavoro non salvato (doc 52 F3), prudente come nel ClassEditor.
    const bool haveSel = (m_tab == Tab::Missions) ? (m_selMission >= 0)
                                                  : (m_selObjective >= 0);
    if (haveSel && ImGui::IsAnyItemActive()) m_dirty = true;

    const ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(ImVec2(vp->WorkPos.x + 10, vp->WorkPos.y + 25),
                            ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(vp->WorkSize.x - 20, vp->WorkSize.y - 40),
                             ImGuiCond_FirstUseEver);
    ImGui::Begin("Missioni e obiettivi");

    if (ImGui::Button("Ricarica dati")) reload();
    ImGui::SameLine();
    ImGui::TextDisabled("Verifica i riferimenti con Moduli -> Validazione contenuti");
    if (!m_status.empty()) { ImGui::SameLine(); ImGui::TextUnformatted(m_status.c_str()); }

    if (ImGui::BeginTabBar("##mtabs"))
    {
        if (ImGui::BeginTabItem("Missioni"))   { m_tab = Tab::Missions;   ImGui::EndTabItem(); }
        if (ImGui::BeginTabItem("Obiettivi"))  { m_tab = Tab::Objectives; ImGui::EndTabItem(); }
        ImGui::EndTabBar();
    }

    ImGui::BeginChild("##list", ImVec2(m_listW, 0), true);
    if (m_tab == Tab::Missions) drawMissionList(); else drawObjectiveList();
    ImGui::EndChild();
    ImGui::SameLine();
    ImGui::BeginChild("##props", ImVec2(0, 0), true);
    if (m_tab == Tab::Missions) drawMissionProps(); else drawObjectiveProps();
    ImGui::EndChild();

    ImGui::End();
}

void MissionEditor::drawObjectiveList()
{
    ImGui::TextDisabled("OBIETTIVI (%d)", (int)m_objectives.size());
    ImGui::Separator();
    for (int i = 0; i < (int)m_objectives.size(); ++i)
    {
        const auto& e = m_objectives[i];
        const std::string lbl = e.def.name.empty() ? e.id : e.def.name;
        if (ImGui::Selectable((lbl + "##o" + std::to_string(i)).c_str(), m_selObjective == i))
            m_selObjective = i;
    }
    ImGui::Separator();
    static char newId[64] = "";
    ImGui::SetNextItemWidth(m_listW - 60.0f);
    ImGui::InputText("##newo", newId, sizeof(newId));
    ImGui::SameLine();
    if (ImGui::Button("Nuovo") && newId[0] != '\0')
    {
        // Creazione = un file nuovo con id = filename (ADR-001). Il contenuto è
        // il minimo VALIDO per il gate: un obiettivo che nasce già rifiutato
        // sarebbe authoring ostile.
        const std::string path = dataDir() + "objectives/" + newId + ".json";
        std::error_code ec;
        fs::create_directories(fs::path(dataDir()) / "objectives", ec);
        if (fs::exists(path, ec)) m_status = "Esiste gia': " + std::string(newId);
        else
        {
            jsonsave::saveJsonRMW(path, [&](nlohmann::json& j) {
                j["name"] = newId;
                j["type"] = "reach_area";
                j["tier"] = "tactical";
                j["target"] = { {"x", 0.0f}, {"z", 0.0f}, {"radius", 5.0f},
                                {"actor_team", 1} };
                j["activation"] = { {"type", "immediate"} };
                return true;
            }, /*makeBackup=*/false);
            m_pendingSelectId = newId;
            newId[0] = '\0';
            reload();
        }
    }
}

void MissionEditor::drawObjectiveProps()
{
    if (m_selObjective < 0 || m_selObjective >= (int)m_objectives.size())
    { ImGui::TextDisabled("Seleziona un obiettivo."); return; }

    auto& e = m_objectives[m_selObjective];
    auto& d = e.def;

    ImGui::Text("id: %s", e.id.c_str());
    ImGui::SameLine();
    ImGui::TextDisabled("(= nome file, ADR-001: si cambia con Rinomina)");

    char nameBuf[128];
    std::snprintf(nameBuf, sizeof(nameBuf), "%s", d.name.c_str());
    ImGui::SetNextItemWidth(300.0f);
    if (editor::ui::textRow("Nome", nameBuf, sizeof(nameBuf))) d.name = nameBuf;

    int ti = indexOf(kObjectiveTypes, kObjectiveTypeCount, objectiveTypeToString(d.type));
    ImGui::SetNextItemWidth(220.0f);
    if (editor::ui::comboRow("Tipo", ti, kObjectiveTypes, kObjectiveTypeCount))
    {
        static const mini::ObjectiveType map[] = {
            mini::ObjectiveType::ReachArea, mini::ObjectiveType::EliminateTarget,
            mini::ObjectiveType::HoldAreaForDuration, mini::ObjectiveType::CaptureZone,
            mini::ObjectiveType::DefendZone, mini::ObjectiveType::DestroyTarget,
            mini::ObjectiveType::EscortEntity, mini::ObjectiveType::SurviveWave,
            mini::ObjectiveType::InteractHack };
        d.type = map[ti];
    }
    if (!typeIsImplemented(ti))
        ImGui::TextColored({1.0f, 0.6f, 0.3f, 1.0f},
            "Tipo dichiarato ma NON ancora eseguito dal runtime: fallira' con causa.");

    int tier = indexOf(kTiers, 3, tierToString(d.tier));
    ImGui::SetNextItemWidth(220.0f);
    if (editor::ui::comboRow("Tier", tier, kTiers, 3))
        d.tier = (tier == 0) ? mini::ObjectiveTier::Primary
               : (tier == 1) ? mini::ObjectiveTier::Strategic
                             : mini::ObjectiveTier::Tactical;
    ImGui::TextDisabled("primary = decide l'esito della missione");

    // ── Bersaglio: solo i campi che il TIPO usa ──────────────────────
    ImGui::SeparatorText("Bersaglio");
    // La mappa dei post è quella della PRIMA missione che usa l'obiettivo: un
    // obiettivo esiste di per sé, ma i suoi post vivono in una mappa concreta.
    std::string mapForPosts;
    for (const auto& m : m_missions)
    {
        auto in = [&](const std::vector<std::string>& v)
        { return std::find(v.begin(), v.end(), e.id) != v.end(); };
        if (in(m.def.primaryObjectives) || in(m.def.optionalObjectives))
        { mapForPosts = m.def.mapId; break; }
    }

    switch (d.type)
    {
    case mini::ObjectiveType::ReachArea:
    case mini::ObjectiveType::HoldAreaForDuration:
        editor::ui::inputFloatRow("x", d.x);
        editor::ui::inputFloatRow("z", d.z);
        editor::ui::inputFloatRow("raggio (m)", d.radius);
        if (d.type == mini::ObjectiveType::HoldAreaForDuration)
        { editor::ui::inputFloatRow("tenuta (s)", d.holdSeconds); }
        break;
    case mini::ObjectiveType::EliminateTarget:
        editor::ui::inputIntRow("team bersaglio", d.targetTeam);
        editor::ui::inputIntRow("quantita'", d.count);
        break;
    case mini::ObjectiveType::CaptureZone:
    case mini::ObjectiveType::DefendZone:
    {
        const auto posts = postsOfMap(mapForPosts);
        std::vector<const char*> items;
        for (const auto& p : posts) items.push_back(p.c_str());
        int pi = 0;
        for (int k = 0; k < (int)posts.size(); ++k) if (posts[k] == d.targetPost) pi = k;
        ImGui::SetNextItemWidth(200.0f);
        if (items.empty())
            ImGui::TextColored({1.f,0.6f,0.3f,1.f},
                "Nessun post: assegna l'obiettivo a una missione con una mappa che ne ha.");
        else if (editor::ui::comboRow("Command post", pi, items.data(), (int)items.size()))
            d.targetPost = posts[pi];
        if (!items.empty() && d.targetPost.empty()) d.targetPost = posts[pi];
        if (!mapForPosts.empty())
            ImGui::TextDisabled("post della mappa '%s'", mapForPosts.c_str());
        if (d.type == mini::ObjectiveType::DefendZone)
        { editor::ui::inputFloatRow("tenuta (s)", d.holdSeconds); }
        break;
    }
    default:
        ImGui::TextDisabled("(nessun parametro: tipo non ancora eseguito)");
        break;
    }
    editor::ui::inputIntRow("team esecutore", d.actorTeam);

    // ── Attivazione ──────────────────────────────────────────────────
    ImGui::SeparatorText("Attivazione");
    int ai = indexOf(kActivations, 3, activationToString(d.activation));
    ImGui::SetNextItemWidth(200.0f);
    if (editor::ui::comboRow("Quando", ai, kActivations, 3))
        d.activation = (ai == 1) ? mini::ActivationType::AfterObjective
                     : (ai == 2) ? mini::ActivationType::AfterTime
                                 : mini::ActivationType::Immediate;
    if (d.activation == mini::ActivationType::AfterObjective)
    {
        // Prerequisito da DROPDOWN degli obiettivi esistenti, e mai se stesso
        // (si auto-bloccherebbe per sempre: il gate lo respinge).
        std::vector<const char*> items; std::vector<std::string> ids;
        for (const auto& o : m_objectives)
            if (o.id != e.id) { ids.push_back(o.id); }
        for (const auto& s : ids) items.push_back(s.c_str());
        int oi = 0;
        for (int k = 0; k < (int)ids.size(); ++k)
            if (ids[k] == d.activationObjective) oi = k;
        ImGui::SetNextItemWidth(220.0f);
        if (!items.empty() && editor::ui::comboRow("Dopo l'obiettivo", oi, items.data(),
                                           (int)items.size()))
            d.activationObjective = ids[oi];
        if (!items.empty() && d.activationObjective.empty())
            d.activationObjective = ids[oi];
    }
    if (d.activation == mini::ActivationType::AfterTime)
    { editor::ui::inputFloatRow("dopo (s)", d.activationTime); }

    ImGui::SetNextItemWidth(120.0f);
    editor::ui::inputFloatRow("Limite di tempo (s, 0 = nessuno)", d.timeLimit);
    ImGui::SetNextItemWidth(120.0f);
    editor::ui::inputIntRow("Ricompensa (punti comando)", d.reward);
    ImGui::TextDisabled("(economia tattica non ancora attiva)");

    // ── Conseguenze ──────────────────────────────────────────────────
    ImGui::SeparatorText("Conseguenze — cosa cambia nella battaglia");
    ImGui::TextDisabled("Un obiettivo senza conseguenze e' solo una casella da spuntare.");
    drawConsequences(d.onSuccess, "Se riesce", mapForPosts);
    drawConsequences(d.onFailure, "Se fallisce", mapForPosts);

    ImGui::Separator();
    if (ImGui::Button("Salva")) saveObjective(e);
    ImGui::SameLine();

    static char renameBuf[64] = "";
    static std::string renameErr;
    ImGui::SetNextItemWidth(160.0f);
    ImGui::InputText("##oren", renameBuf, sizeof(renameBuf));
    ImGui::SameLine();
    if (ImGui::Button("Rinomina") && renameBuf[0] != '\0')
    {
        int refs = 0;
        renameErr = rename::renameDefinition(dataDir(), rename::Category::Objective,
                                             e.id, renameBuf, &refs);
        if (renameErr.empty())
        {
            m_pendingSelectId = renameBuf;
            m_status = "Rinominato (" + std::to_string(refs) + " riferimenti aggiornati)";
            renameBuf[0] = '\0';
            reload();
        }
    }
    if (!renameErr.empty())
        ImGui::TextColored({1.f,0.4f,0.4f,1.f}, "%s", renameErr.c_str());
}

void MissionEditor::drawMissionList()
{
    ImGui::TextDisabled("MISSIONI (%d)", (int)m_missions.size());
    ImGui::Separator();
    for (int i = 0; i < (int)m_missions.size(); ++i)
    {
        const auto& e = m_missions[i];
        const std::string lbl = e.def.name.empty() ? e.id : e.def.name;
        if (ImGui::Selectable((lbl + "##m" + std::to_string(i)).c_str(), m_selMission == i))
            m_selMission = i;
    }
    ImGui::Separator();
    static char newId[64] = "";
    ImGui::SetNextItemWidth(m_listW - 60.0f);
    ImGui::InputText("##newm", newId, sizeof(newId));
    ImGui::SameLine();
    if (ImGui::Button("Nuova") && newId[0] != '\0')
    {
        const std::string path = dataDir() + "missions/" + newId + ".json";
        std::error_code ec;
        fs::create_directories(fs::path(dataDir()) / "missions", ec);
        if (fs::exists(path, ec)) m_status = "Esiste gia': " + std::string(newId);
        else
        {
            // Minimo VALIDO per il gate: regole di successo E fallimento sono
            // obbligatorie (doc 25), e la mappa dev'essere una reale.
            std::string firstMap = "firebase";
            if (!m_registry.maps().empty()) firstMap = m_registry.maps().begin()->first;
            jsonsave::saveJsonRMW(path, [&](nlohmann::json& j) {
                j["name"] = newId;
                j["map"]  = firstMap;
                j["mode"] = "conquest";
                j["primary_objectives"] = nlohmann::json::array();
                j["success_rules"] = { {"rule", "all_primary_complete"} };
                j["failure_rules"] = { {"rule", "any_primary_failed"} };
                return true;
            }, /*makeBackup=*/false);
            m_pendingSelectId = newId;
            newId[0] = '\0';
            reload();
        }
    }
}

void MissionEditor::drawMissionProps()
{
    if (m_selMission < 0 || m_selMission >= (int)m_missions.size())
    { ImGui::TextDisabled("Seleziona una missione."); return; }

    auto& e = m_missions[m_selMission];
    auto& d = e.def;

    ImGui::Text("id: %s", e.id.c_str());
    ImGui::SameLine();
    ImGui::TextDisabled("(= nome file, ADR-001)");

    char nameBuf[128], briefBuf[512];
    std::snprintf(nameBuf,  sizeof(nameBuf),  "%s", d.name.c_str());
    std::snprintf(briefBuf, sizeof(briefBuf), "%s", d.briefing.c_str());
    if (editor::ui::textRow("Nome", nameBuf, sizeof(nameBuf))) d.name = nameBuf;
    // Briefing: etichetta SOPRA (il multiline è alto; l'etichetta a destra si
    // taglierebbe). Box largo quanto il pannello, non più fisso a 500 px.
    ImGui::TextUnformatted("Briefing");
    if (ImGui::InputTextMultiline("##brief", briefBuf, sizeof(briefBuf),
                                  ImVec2(ImGui::GetContentRegionAvail().x, 50)))
        d.briefing = briefBuf;

    // Mappa da DROPDOWN del registry: gli obiettivi sono coordinate/post di QUELLA
    // mappa, e la missione la impone al gioco.
    {
        std::vector<std::string> ids;
        for (const auto& [id, m] : m_registry.maps()) ids.push_back(id);
        std::sort(ids.begin(), ids.end());
        std::vector<const char*> items;
        for (const auto& s : ids) items.push_back(s.c_str());
        int mi = 0;
        for (int k = 0; k < (int)ids.size(); ++k) if (ids[k] == d.mapId) mi = k;
        ImGui::SetNextItemWidth(220.0f);
        if (!items.empty() && editor::ui::comboRow("Mappa", mi, items.data(), (int)items.size()))
            d.mapId = ids[mi];
        ImGui::TextDisabled("la missione IMPONE la sua mappa al gioco");
    }
    {
        static const char* const modes[] = { "conquest", "assault", "defense" };
        int mi = indexOf(modes, 3, d.modeId, 0);
        ImGui::SetNextItemWidth(220.0f);
        if (editor::ui::comboRow("Modalita'", mi, modes, 3)) d.modeId = modes[mi];
    }

    // ── Obiettivi: si compongono, non si digitano ────────────────────
    // `selRef` è per-lista: uno `static` dentro questa lambda sarebbe CONDIVISO fra
    // le due chiamate (primari/opzionali), e selezionare in un dropdown farebbe
    // saltare l'altro — era il bug segnalato il 2026-07-16.
    auto objectiveListUi = [&](std::vector<std::string>& list, const char* title,
                               int& selRef)
    {
        ImGui::PushID(title);
        ImGui::SeparatorText(title);
        int toRemove = -1;
        for (int i = 0; i < (int)list.size(); ++i)
        {
            const mini::ObjectiveDef* od = m_registry.getObjective(list[i]);
            ImGui::PushID(i);
            if (od) ImGui::Text("%s  (%s)", od->name.c_str(), list[i].c_str());
            else    ImGui::TextColored({1.f,0.4f,0.4f,1.f},
                        "%s  <-- non esiste piu'", list[i].c_str());
            ImGui::SameLine();
            if (ImGui::SmallButton("X")) toRemove = i;
            ImGui::PopID();
        }
        if (toRemove >= 0) list.erase(list.begin() + toRemove);

        // Aggiunta SOLO dal registry (mai testo libero). Gli obiettivi GIÀ in questa
        // missione non ricompaiono: aggiungerli due volte, o metterne uno fra i
        // primari e gli opzionali insieme, sarebbe un dato incoerente.
        // Il conteggio sotto lo DICE: prima sembrava che il dropdown perdesse
        // obiettivi per conto suo (segnalato dall'utente il 2026-07-16).
        std::vector<std::string> avail;
        int alreadyUsed = 0;
        for (const auto& o : m_objectives)
        {
            auto in = [&](const std::vector<std::string>& v)
            { return std::find(v.begin(), v.end(), o.id) != v.end(); };
            if (!in(d.primaryObjectives) && !in(d.optionalObjectives))
                avail.push_back(o.id);
            else
                ++alreadyUsed;
        }
        if (!avail.empty())
        {
            // Nome + id: la lista sopra mostra il nome, il dropdown mostrava l'id
            // grezzo — due modi di chiamare la stessa cosa nella stessa finestra.
            std::vector<std::string> labels;
            for (const auto& id : avail)
            {
                const mini::ObjectiveDef* od = m_registry.getObjective(id);
                labels.push_back(od && !od->name.empty()
                                 ? od->name + "  (" + id + ")" : id);
            }
            std::vector<const char*> items;
            for (const auto& s : labels) items.push_back(s.c_str());
            selRef = std::clamp(selRef, 0, (int)avail.size() - 1);
            ImGui::SetNextItemWidth(300.0f);
            ImGui::Combo("##add", &selRef, items.data(), (int)items.size());
            ImGui::SameLine();
            if (ImGui::SmallButton("+ aggiungi")) list.push_back(avail[selRef]);
        }
        else ImGui::TextDisabled("(nessun altro obiettivo da aggiungere)");

        if (alreadyUsed > 0)
            ImGui::TextDisabled("%d obiettiv%s gia' in questa missione (non rielencat%s): "
                                "toglil%s con X per riassegnarl%s",
                                alreadyUsed, alreadyUsed == 1 ? "o" : "i",
                                alreadyUsed == 1 ? "o" : "i",
                                alreadyUsed == 1 ? "o" : "i",
                                alreadyUsed == 1 ? "o" : "i");
        ImGui::PopID();
    };
    objectiveListUi(d.primaryObjectives,  "Obiettivi primari (decidono l'esito)",
                    m_addSelPrimary);
    objectiveListUi(d.optionalObjectives, "Obiettivi opzionali", m_addSelOptional);

    // ── Regole: obbligatorie entrambe (doc 25 + gate ADR-018) ────────
    ImGui::SeparatorText("Regole di missione");
    int sr = indexOf(kMissionRules, 4, missionRuleToString(d.successRule));
    ImGui::SetNextItemWidth(220.0f);
    if (editor::ui::comboRow("Successo", sr, kMissionRules, 2))   // solo le 2 di successo
        d.successRule = (sr == 1) ? mini::MissionRule::AnyPrimaryComplete
                                  : mini::MissionRule::AllPrimaryComplete;
    int fr = indexOf(kMissionRules, 4, missionRuleToString(d.failureRule), 2) - 2;
    fr = std::clamp(fr, 0, 1);
    ImGui::SetNextItemWidth(220.0f);
    if (editor::ui::comboRow("Fallimento", fr, &kMissionRules[2], 2))   // solo le 2 di fallimento
        d.failureRule = (fr == 1) ? mini::MissionRule::TimeLimit
                                  : mini::MissionRule::AnyPrimaryFailed;
    if (d.failureRule == mini::MissionRule::TimeLimit)
    {
        ImGui::SetNextItemWidth(120.0f);
        editor::ui::inputFloatRow("Tempo limite (s)", d.failureTimeLimit);
    }

    ImGui::Separator();
    if (ImGui::Button("Salva")) saveMission(e);
    ImGui::SameLine();
    static char renameBuf[64] = "";
    static std::string renameErr;
    ImGui::SetNextItemWidth(160.0f);
    ImGui::InputText("##mren", renameBuf, sizeof(renameBuf));
    ImGui::SameLine();
    if (ImGui::Button("Rinomina") && renameBuf[0] != '\0')
    {
        int refs = 0;
        renameErr = rename::renameDefinition(dataDir(), rename::Category::Mission,
                                             e.id, renameBuf, &refs);
        if (renameErr.empty())
        {
            m_pendingSelectId = renameBuf;
            m_status = "Rinominata";
            renameBuf[0] = '\0';
            reload();
        }
    }
    if (!renameErr.empty())
        ImGui::TextColored({1.f,0.4f,0.4f,1.f}, "%s", renameErr.c_str());
}

} // namespace editor
