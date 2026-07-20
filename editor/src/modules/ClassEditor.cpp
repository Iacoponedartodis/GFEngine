// ClassEditor — authoring delle classi (14_ClassSystem).
#include "util/DataPath.hpp"
#include "modules/ClassEditor.hpp"
#include "util/JsonSave.hpp"
#include "util/DefinitionRename.hpp"
#include "util/UiWidgets.hpp"   // textRow: etichetta a sinistra, mai tagliata

#include <imgui.h>
#include <SDL2/SDL.h>

#include <algorithm>
#include <filesystem>

namespace editor
{
namespace fs = std::filesystem;
namespace
{
// R8 chiuso: unica risoluzione in util/DataPath.
std::string dataDir() { return editor::datapath::dir(); }

// Dropdown di id + "(nessuna)" in testa. Ritorna true se la scelta è cambiata.
// Gli id si SCELGONO sempre da una lista (CLAUDE.md: mai testo libero).
bool idCombo(const char* label, const std::vector<std::string>& ids,
             const std::vector<std::string>& labels, std::string& value,
             bool allowNone, float width = 240.0f)
{
    std::vector<const char*> items;
    if (allowNone) items.push_back("(nessuna)");
    for (const auto& s : labels) items.push_back(s.c_str());

    int cur = 0;
    for (int i = 0; i < (int)ids.size(); ++i)
        if (ids[i] == value) { cur = i + (allowNone ? 1 : 0); break; }

    ImGui::SetNextItemWidth(width);
    if (!ImGui::Combo(label, &cur, items.data(), (int)items.size())) return false;
    const int off = allowNone ? 1 : 0;
    value = (cur < off) ? std::string() : ids[cur - off];
    return true;
}
} // namespace

ClassEditor::ClassEditor() { reload(); }

void ClassEditor::reload()
{
    m_registry.loadAll(dataDir());
    m_entries.clear();
    for (const auto& [id, d] : m_registry.classes())
        m_entries.push_back({id, dataDir() + "classes/" + id + ".json", d});
    std::sort(m_entries.begin(), m_entries.end(),
              [](const auto& a, const auto& b) { return a.id < b.id; });

    if (!m_pendingSelectId.empty())
    {
        for (int i = 0; i < (int)m_entries.size(); ++i)
            if (m_entries[i].id == m_pendingSelectId) m_sel = i;
        m_pendingSelectId.clear();
    }
    m_sel = std::min(m_sel, (int)m_entries.size() - 1);
}

// SEMPRE RMW (ADR-010): si toccano solo i campi propri. `id` non si scrive mai
// nel JSON (ADR-001: è il filename; un id in-file stantio ha già rotto le
// cross-ref in silenzio, KI #21).
void ClassEditor::save(const Entry& e)
{
    const bool ok = jsonsave::saveJsonRMW(e.jsonPath, [&](nlohmann::json& j)
    {
        j["name"]           = e.def.name;
        j["role"]           = e.def.role;
        if (e.def.aiProfileId.empty()) j.erase("ai_profile");
        else j["ai_profile"] = e.def.aiProfileId;   // ADR-022
        j["primary_weapon"] = e.def.primaryWeaponId;
        if (e.def.secondaryWeaponId.empty()) j.erase("secondary_weapon");
        else j["secondary_weapon"] = e.def.secondaryWeaponId;
        if (e.def.abilityIds.empty()) j.erase("abilities");
        else j["abilities"] = e.def.abilityIds;
        // ADR-023: corpo + moltiplicatori. base_entity vuoto = classe non
        // istanziabile da sola (si toglie dal file per non lasciare rumore).
        if (e.def.baseEntityId.empty()) j.erase("base_entity");
        else j["base_entity"] = e.def.baseEntityId;
        j["hp_mult"]     = e.def.hpMult;
        j["speed_mult"]  = e.def.speedMult;
        j["damage_mult"] = e.def.damageMult;
        j["color_mult"]  = { e.def.colorMult[0], e.def.colorMult[1], e.def.colorMult[2] };
        return true;
    });
    m_status = ok ? ("Salvato: " + e.id) : ("ERRORE salvataggio: " + e.id);
}

void ClassEditor::draw()
{
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(ImVec2(vp->WorkPos.x + 10, vp->WorkPos.y + 25),
                            ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(vp->WorkSize.x - 20, vp->WorkSize.y - 40),
                             ImGuiCond_FirstUseEver);
    ImGui::Begin("Classi");

    if (ImGui::Button("Ricarica dati")) reload();
    ImGui::SameLine();
    ImGui::TextDisabled("Verifica i riferimenti con Moduli -> Validazione contenuti");
    if (!m_status.empty()) { ImGui::SameLine(); ImGui::TextUnformatted(m_status.c_str()); }

    ImGui::BeginChild("##clist", ImVec2(m_listW, 0), true);
    drawList();
    ImGui::EndChild();
    ImGui::SameLine();
    ImGui::BeginChild("##cprops", ImVec2(0, 0), true);
    drawProps();
    ImGui::EndChild();
    ImGui::End();
}

void ClassEditor::drawList()
{
    ImGui::TextDisabled("CLASSI (%d)", (int)m_entries.size());
    ImGui::Separator();
    for (int i = 0; i < (int)m_entries.size(); ++i)
    {
        const auto& e = m_entries[i];
        const std::string lbl = e.def.name.empty() ? e.id : e.def.name;
        if (ImGui::Selectable((lbl + "##c" + std::to_string(i)).c_str(), m_sel == i))
            m_sel = i;
    }
    ImGui::Separator();
    static char newId[64] = "";
    ImGui::SetNextItemWidth(m_listW - 60.0f);
    ImGui::InputText("##newc", newId, sizeof(newId));
    ImGui::SameLine();
    if (ImGui::Button("Nuova") && newId[0] != '\0')
    {
        const std::string path = dataDir() + "classes/" + newId + ".json";
        std::error_code ec;
        fs::create_directories(fs::path(dataDir()) / "classes", ec);
        if (fs::exists(path, ec)) m_status = "Esiste gia': " + std::string(newId);
        else
        {
            // Minimo VALIDO per il gate ADR-018: l'arma primaria è obbligatoria
            // (senza, la classe non equipaggia nulla). Nascere già rifiutata
            // sarebbe authoring ostile. Prima arma disponibile, di qualunque
            // fazione: la fazione della classe non è decisa qui, e l'autore la
            // cambia subito dal dropdown.
            std::string firstWeapon = m_registry.weapons().empty()
                ? std::string() : m_registry.weapons().begin()->first;
            jsonsave::saveJsonRMW(path, [&](nlohmann::json& j) {
                j["name"] = newId;
                j["role"] = "assault";
                j["primary_weapon"] = firstWeapon;
                return true;
            }, /*makeBackup=*/false);
            m_pendingSelectId = newId;
            newId[0] = '\0';
            reload();
        }
    }
}

void ClassEditor::drawProps()
{
    if (m_sel < 0 || m_sel >= (int)m_entries.size())
    { ImGui::TextDisabled("Seleziona una classe."); return; }

    auto& e = m_entries[m_sel];
    auto& d = e.def;

    ImGui::Text("id: %s", e.id.c_str());
    ImGui::SameLine();
    ImGui::TextDisabled("(= nome file, ADR-001: si cambia con Rinomina)");

    char nameBuf[128], roleBuf[64];
    std::snprintf(nameBuf, sizeof(nameBuf), "%s", d.name.c_str());
    std::snprintf(roleBuf, sizeof(roleBuf), "%s", d.role.c_str());
    ImGui::SetNextItemWidth(300.0f);
    if (editor::ui::textRow("Nome", nameBuf, sizeof(nameBuf))) d.name = nameBuf;
    ImGui::SetNextItemWidth(200.0f);
    if (editor::ui::textRow("Ruolo", roleBuf, sizeof(roleBuf))) d.role = roleBuf;
    ImGui::TextColored({1.0f, 0.75f, 0.35f, 1.0f},
        "Il ruolo e' ancora solo un'etichetta: nessun sistema lo consuma "
        "(diventera' un enum quando la squadra assegnera' i task per ruolo, ADR-022).");

    // ── Comportamento: è ciò che rende la classe una PROFESSIONE ──────
    ImGui::SeparatorText("Comportamento (ADR-022)");
    std::vector<std::string> apIds, apLabels;
    for (const auto& [id, a] : m_registry.aiProfiles())
    { apIds.push_back(id); apLabels.push_back(id + "  (" + a.role + ")"); }
    idCombo("Profilo AI", apIds, apLabels, d.aiProfileId, /*allowNone=*/true);
    ImGui::TextDisabled("Vale per i CLONI ALLEATI che referenziano questa classe: e' cio' che\n"
                        "rende una squadra Trooper+Heavy+Recon diversa da una monoclasse\n"
                        "(GDD 12.3). Vuoto = l'unita' tiene il proprio profilo.");

    // ── Corpo + moltiplicatori (ADR-023) ─────────────────────────────
    ImGui::SeparatorText("Corpo e stat (ADR-023)");
    std::vector<std::string> beIds, beLabels;
    for (const auto& [bid, a] : m_registry.allies())
    { beIds.push_back(bid); beLabels.push_back(bid + "  [alleato]"); (void)a; }
    for (const auto& [bid, a] : m_registry.enemies())
    { beIds.push_back(bid); beLabels.push_back(bid + "  [nemico]"); (void)a; }
    idCombo("Corpo (base_entity)", beIds, beLabels, d.baseEntityId, /*allowNone=*/true);
    ImGui::TextDisabled("Modello, hitbox e stat base vengono da qui. Impostato = la classe\n"
                        "e' un TIPO-UNITA' (i roster la referenziano). Vuoto = solo 'sopra'\n"
                        "un'entita' esistente (modello legacy).");
    editor::ui::sliderRow("HP x",        d.hpMult,     0.1f, 3.0f, 0.01f, "%.2f", 100.0f);
    editor::ui::sliderRow("Velocita' x", d.speedMult,  0.1f, 3.0f, 0.01f, "%.2f", 100.0f);
    editor::ui::sliderRow("Danno x",     d.damageMult, 0.1f, 3.0f, 0.01f, "%.2f", 100.0f);
    ImGui::TextDisabled("Moltiplicatori sulle stat BASE del corpo (1.0 = invariato).");
    ImGui::SetNextItemWidth(220.0f);
    ImGui::ColorEdit3("Tinta colore (x sul corpo)", d.colorMult.data());
    ImGui::TextDisabled("Distingue a colpo d'occhio le professioni che condividono il\n"
                        "corpo: MOLTIPLICA il colore base (bianco = invariato).");

    // ── Armi: dropdown dal registry (mai testo libero) ────────────────
    ImGui::SeparatorText("Loadout");
    // TUTTE le armi, di ogni fazione. Il filtro "solo non-separatiste" impediva
    // di dare le armi corrette ai nemici (una classe per il B1 Battle Droid deve
    // poter usare l'E-5): le classi valgono anche per gli NPC nemici (ADR-022),
    // non solo per la Repubblica. La fazione è in etichetta, non un filtro, così
    // resta visibile senza togliere scelte.
    std::vector<std::string> wIds, wLabels;
    for (const auto& [id, w] : m_registry.weapons())
    {
        wIds.push_back(id);
        std::string label = w.name.empty() ? id : w.name;
        label += "  [";
        label += mini::factionToString(w.faction);
        label += "]";
        wLabels.push_back(label);
    }
    // Ordinamento congiunto: le due liste sono parallele, ordinarne una sola le
    // disallineerebbe (l'arma mostrata non sarebbe quella scelta).
    {
        std::vector<int> order(wIds.size());
        for (int i = 0; i < (int)order.size(); ++i) order[i] = i;
        std::sort(order.begin(), order.end(),
                  [&](int a, int b) { return wLabels[a] < wLabels[b]; });
        std::vector<std::string> ni, nl;
        for (int i : order) { ni.push_back(wIds[i]); nl.push_back(wLabels[i]); }
        wIds = std::move(ni); wLabels = std::move(nl);
    }
    idCombo("Arma primaria", wIds, wLabels, d.primaryWeaponId, /*allowNone=*/false);
    if (d.primaryWeaponId.empty())
        ImGui::TextColored({1.f,0.4f,0.4f,1.f},
                           "Obbligatoria: senza, la classe non equipaggia nulla.");
    idCombo("Arma secondaria", wIds, wLabels, d.secondaryWeaponId, /*allowNone=*/true);
    if (!d.secondaryWeaponId.empty() && d.secondaryWeaponId == d.primaryWeaponId)
        ImGui::TextColored({1.f,0.75f,0.35f,1.f},
                           "Primaria e secondaria sono la stessa arma.");

    // ── Abilità ──────────────────────────────────────────────────────
    ImGui::SeparatorText("Abilita'");
    ImGui::TextColored({1.0f, 0.75f, 0.35f, 1.0f},
        "Trasportate dalla classe ma SENZA effetto: non esiste ancora un sistema "
        "abilita' lato giocatore (KI #32).");
    std::vector<std::string> aIds, aLabels;
    for (const auto& [id, a] : m_registry.abilities())
    { aIds.push_back(id); aLabels.push_back(a.name.empty() ? id : a.name); }

    int toRemove = -1;
    for (int i = 0; i < (int)d.abilityIds.size(); ++i)
    {
        ImGui::PushID(i);
        const mini::AbilityDef* ad = m_registry.getAbility(d.abilityIds[i]);
        if (ad) ImGui::Text("%s  (%s)", ad->name.c_str(), d.abilityIds[i].c_str());
        else    ImGui::TextColored({1.f,0.4f,0.4f,1.f},
                    "%s  <-- non esiste piu'", d.abilityIds[i].c_str());
        ImGui::SameLine();
        if (ImGui::SmallButton("X")) toRemove = i;
        ImGui::PopID();
    }
    if (toRemove >= 0) d.abilityIds.erase(d.abilityIds.begin() + toRemove);

    // Solo le abilità non già assegnate: aggiungerne due volte la stessa è un
    // dato incoerente. Il conteggio lo dice, invece di far sembrare che il
    // dropdown perda voci da solo (lezione dal bug del MissionEditor, 07-16).
    std::vector<std::string> availIds, availLabels;
    for (int i = 0; i < (int)aIds.size(); ++i)
        if (std::find(d.abilityIds.begin(), d.abilityIds.end(), aIds[i])
            == d.abilityIds.end())
        { availIds.push_back(aIds[i]); availLabels.push_back(aLabels[i]); }

    if (!availIds.empty())
    {
        std::vector<const char*> items;
        for (const auto& s : availLabels) items.push_back(s.c_str());
        m_abilitySel = std::clamp(m_abilitySel, 0, (int)availIds.size() - 1);
        ImGui::SetNextItemWidth(240.0f);
        ImGui::Combo("##addab", &m_abilitySel, items.data(), (int)items.size());
        ImGui::SameLine();
        if (ImGui::SmallButton("+ aggiungi")) d.abilityIds.push_back(availIds[m_abilitySel]);
    }
    else ImGui::TextDisabled("(nessun'altra abilita' da aggiungere)");
    if ((int)d.abilityIds.size() > 0)
        ImGui::TextDisabled("%d gia' assegnat%s (non rielencat%s): toglil%s con X",
                            (int)d.abilityIds.size(),
                            d.abilityIds.size() == 1 ? "a" : "e",
                            d.abilityIds.size() == 1 ? "a" : "e",
                            d.abilityIds.size() == 1 ? "a" : "e");

    ImGui::Separator();
    if (ImGui::Button("Salva")) save(e);
    ImGui::SameLine();
    static char renameBuf[64] = "";
    static std::string renameErr;
    ImGui::SetNextItemWidth(160.0f);
    ImGui::InputText("##cren", renameBuf, sizeof(renameBuf));
    ImGui::SameLine();
    if (ImGui::Button("Rinomina") && renameBuf[0] != '\0')
    {
        int refs = 0;
        renameErr = rename::renameDefinition(dataDir(), rename::Category::Class,
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
