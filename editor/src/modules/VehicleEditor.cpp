// VehicleEditor.cpp — modulo veicoli (19_Vehicles).
// Layout: [lista | viewport 3D | proprietà]. Salvataggio RMW (ADR-010),
// id = nome file (ADR-001), rinomina con sweep vehicle_spawns nelle mappe.

#include "util/DataPath.hpp"
#include "modules/VehicleEditor.hpp"
#include "mini/game/data/DefinitionRegistry.hpp"
#include "util/FileDialog.hpp"
#include "util/JsonSave.hpp"
#include "util/DefinitionRename.hpp"
#include "util/UiWidgets.hpp"

#include <imgui.h>
#include <SDL2/SDL.h>
#include <nlohmann/json.hpp>
#include <filesystem>
#include <fstream>
#include <cstdio>
#include <cstring>
#include <algorithm>

namespace fs = std::filesystem;
using json   = nlohmann::json;

namespace editor
{

// R8 chiuso: unica risoluzione in util/DataPath (era una delle copie col
// controllo debole: accettava qualunque cartella di nome data).
static std::string getDataDir() { return editor::datapath::dir(); }

VehicleEditor::VehicleEditor()
{
    // Ciclo di vita degli asset delegato al componente condiviso (ADR-049 R1): questo
    // modulo aveva "Nuovo veicolo" ma NON Duplica, Rinomina o Elimina. Ora li ha tutti,
    // e con le regole del progetto applicate per costruzione (id = filename, rename via
    // comando ADR-010, RMW, conferma sull'eliminazione).
    editor::AssetBrowser::Config cfg;
    cfg.folder   = "vehicles";
    cfg.title    = "Veicoli";
    cfg.category = editor::rename::Category::Vehicle;
    cfg.makeDefault = [](const std::string& id) {
        mini::VehicleDef d; d.name = id;
        nlohmann::json j;
        j["name"]          = d.name;
        j["hp"]            = d.hp;
        j["max_speed"]     = d.maxSpeed;
        j["accel"]         = d.accel;
        j["turn_rate_deg"] = d.turnRateDeg;
        j["half_x"] = d.halfX; j["half_y"] = d.halfY; j["half_z"] = d.halfZ;
        j["color"]  = {d.color[0], d.color[1], d.color[2]};
        return j;
    };
    m_browser.configure(cfg);

    loadEntries();
    if (!m_entries.empty()) selectEntry(0);
    syncFromBrowser();
}

// Allinea la selezione del modulo a quella del browser (che ragiona per ID, non per
// indice: gli indici cambiano a ogni crea/elimina, l'id no).
void VehicleEditor::syncFromBrowser()
{
    const std::string& id = m_browser.selectedId();
    for (int i = 0; i < (int)m_entries.size(); ++i)
        if (m_entries[i].id == id) { selectEntry(i); return; }
    m_sel = -1;
}

void VehicleEditor::tick(float dt)
{
    m_viewport.tick(dt);
}

void VehicleEditor::loadEntries()
{
    // R4: parse dei VehicleDef UNIFICATO sul DefinitionRegistry (prima
    // questo modulo duplicava il parse riga per riga → rischio di deriva
    // dello schema tra editor e runtime). L'editor legge ESATTAMENTE ciò
    // che leggerà il gioco.
    m_entries.clear();
    m_sel = -1;

    mini::DefinitionRegistry reg;
    reg.loadAll(getDataDir());

    for (const auto& [id, def] : reg.vehicles())
    {
        VehicleEntry e;
        e.id       = id;
        e.jsonPath = getDataDir() + "vehicles/" + id + ".json";
        e.def      = def;
        m_entries.push_back(std::move(e));
    }
    std::sort(m_entries.begin(), m_entries.end(),
              [](const auto& a, const auto& b){ return a.id < b.id; });
}

void VehicleEditor::selectEntry(int idx)
{
    if (idx < 0 || idx >= (int)m_entries.size()) return;
    m_sel   = idx;
    m_dirty = false;
    updateViewport();
}

void VehicleEditor::updateViewport()
{
    if (m_sel < 0 || m_sel >= (int)m_entries.size())
    {
        m_viewport.clearModel();
        m_viewport.clearMapBoxes();
        return;
    }
    const auto& d = m_entries[m_sel].def;

    if (!d.meshPath.empty())
        m_viewport.loadModel(d.meshPath, d.meshRotX, d.meshScale, d.meshRotY);
    else
        m_viewport.clearModel();

    // Due wireframe: COLLISIONE (grigio, raggiunge il suolo) e DANNO
    // (giallo, il volume che i colpi devono colpire — esclude lo spazio
    // vuoto sotto un mezzo che fluttua). Y del box = altezza dal suolo,
    // come li spawna il runtime (centro fisico a halfY).
    std::vector<FreeCameraViewport::MapBoxDraw> boxes;

    // Collisione: CIANO brillante. Danno: GIALLO brillante. Colori base
    // distinti (niente 'selected', che li sbiadirebbe entrambi uguali).
    FreeCameraViewport::MapBoxDraw col;
    col.x = 0; col.y = d.halfY; col.z = 0; col.ry = 0;
    col.sx = d.halfX * 2.0f; col.sy = d.halfY * 2.0f; col.sz = d.halfZ * 2.0f;
    col.r = 0.2f; col.g = 0.9f; col.b = 1.0f;   // ciano
    col.selected = false;
    boxes.push_back(col);

    const float hx = (d.hitHalfX > 0.0f) ? d.hitHalfX : d.halfX;
    const float hy = (d.hitHalfY > 0.0f) ? d.hitHalfY : d.halfY;
    const float hz = (d.hitHalfZ > 0.0f) ? d.hitHalfZ : d.halfZ;
    FreeCameraViewport::MapBoxDraw hit;
    hit.x = 0; hit.y = d.halfY + d.hitOffsetY; hit.z = 0; hit.ry = 0;
    hit.sx = hx * 2.0f; hit.sy = hy * 2.0f; hit.sz = hz * 2.0f;
    hit.r = 1.0f; hit.g = 0.85f; hit.b = 0.1f;   // giallo
    hit.selected = false;
    boxes.push_back(hit);

    m_viewport.setMapBoxes(boxes);
}

void VehicleEditor::saveSelected()
{
    if (m_sel < 0 || m_sel >= (int)m_entries.size()) return;
    auto& e = m_entries[m_sel];
    const auto& d = e.def;
    editor::jsonsave::saveJsonRMW(e.jsonPath, [&](json& j) {
        j.erase("id"); // deprecato: id = nome file (ADR-001)
        j["name"]          = d.name;
        j["hp"]            = d.hp;
        j["max_speed"]     = d.maxSpeed;
        j["accel"]         = d.accel;
        j["turn_rate_deg"] = d.turnRateDeg;
        j["mesh"]          = d.meshPath;
        j["mesh_scale"]    = d.meshScale;
        j["mesh_rot_x"]    = d.meshRotX;
        j["mesh_rot_y"]    = d.meshRotY;
        j["mesh_offset_y"] = d.meshOffsetY;
        j["half_x"]        = d.halfX;
        j["half_y"]        = d.halfY;
        j["half_z"]        = d.halfZ;
        j["hit_offset_y"]  = d.hitOffsetY;
        j["hit_half_x"]    = d.hitHalfX;
        j["hit_half_y"]    = d.hitHalfY;
        j["hit_half_z"]    = d.hitHalfZ;
        j["color"]         = {d.color[0], d.color[1], d.color[2]};
        return true;
    });
    m_dirty = false;
}

void VehicleEditor::draw()
{
    // Riselezione post rinomina/creazione (deferred, come EntityEditor)
    if (!m_pendingSelectId.empty())
    {
        const std::string id = m_pendingSelectId;
        m_pendingSelectId.clear();
        loadEntries();
        for (int i = 0; i < (int)m_entries.size(); ++i)
            if (m_entries[i].id == id) { selectEntry(i); break; }
    }

    // ── Layout via ModuleShell (ADR-049) ─────────────────────────────
    // Prima questo modulo si costruiva il layout da sé e replicava il bug del pannello
    // destro (`ChildFlags_ResizeX`: grip sul bordo finestra → una volta stretto non si
    // riallarga). Ora la struttura sta in un posto solo: correggerla lì corregge ogni
    // modulo che la adotta.
    m_shell.begin();

    // ── Lista (sinistra) ─────────────────────────────────────────────
    if (m_shell.beginList())
    {
    // Lista + ciclo di vita completo dal componente condiviso (ADR-049 R1).
    // Quando qualcosa cambia (selezione o contenuto della cartella) si ricarica: gli
    // indici locali non sopravvivono a una creazione o a un'eliminazione, gli id sì.
    if (m_browser.draw())
    {
        loadEntries();
        syncFromBrowser();
    }

    if (ImGui::Button("Ricarica lista", {ImGui::GetContentRegionAvail().x, 0}))
        loadEntries();
    m_shell.endList();
    }

    // ── Viewport 3D (centro) ─────────────────────────────────────────
    if (m_shell.beginContent(/*noScroll=*/true))
    {
        m_viewport.draw(false);
        m_shell.endContent();
    }

    // ── Proprietà (destra) ───────────────────────────────────────────
    if (m_shell.beginProperties())
    {
        drawProperties();
        m_shell.endProperties();
    }
    m_shell.end();
}

void VehicleEditor::drawProperties()
{
    if (m_sel < 0 || m_sel >= (int)m_entries.size())
    { ImGui::TextDisabled("Seleziona un veicolo."); return; }

    auto& e = m_entries[m_sel];
    auto& d = e.def;
    bool changed = false;
    const float w = ImGui::GetContentRegionAvail().x - 16.0f;

    ImGui::TextColored({0.8f,0.8f,0.2f,1.0f}, "%s", d.name.c_str());
    ImGui::TextDisabled("[%s]", e.id.c_str());

    // Rinomina (ADR-010: aggiorna vehicle_spawns nelle mappe)
    {
        static char renameBuf[64] = "";
        static std::string renameErr;
        ImGui::SetNextItemWidth(w - 80.0f);
        ImGui::InputText("##vren", renameBuf, sizeof(renameBuf));
        ImGui::SameLine();
        if (ImGui::Button("Rinomina") && renameBuf[0] != '\0')
        {
            int refs = 0;
            renameErr = editor::rename::renameDefinition(
                getDataDir(), editor::rename::Category::Vehicle,
                e.id, renameBuf, &refs);
            if (renameErr.empty())
            {
                m_pendingSelectId = renameBuf;
                renameBuf[0] = '\0';
            }
        }
        if (!renameErr.empty())
            ImGui::TextColored({1.f,0.4f,0.4f,1.f}, "%s", renameErr.c_str());
    }
    ImGui::Separator();

    // Nome etichetta
    {
        char nameBuf[64];
        std::snprintf(nameBuf, sizeof(nameBuf), "%s", d.name.c_str());
        ImGui::SetNextItemWidth(w);
        if (editor::ui::textRow("Nome", nameBuf, sizeof(nameBuf)))
        { d.name = nameBuf; changed = true; }
    }

    ImGui::Separator();
    ImGui::TextDisabled("Statistiche");
    changed |= editor::ui::dragRow("HP", d.hp,          1.0f, 10.0f, 5000.0f, "%.0f");
    changed |= editor::ui::dragRow("Vel. max m/s", d.maxSpeed,    0.2f,  2.0f,   60.0f, "%.1f");
    changed |= editor::ui::dragRow("Accelerazione", d.accel,       0.2f,  1.0f,   60.0f, "%.1f");
    changed |= editor::ui::dragRow("Sterzata deg/s", d.turnRateDeg, 1.0f, 10.0f,  360.0f, "%.0f");

    ImGui::Separator();
    ImGui::TextDisabled("Modello 3D");
    {
        char meshBuf[256];
        std::snprintf(meshBuf, sizeof(meshBuf), "%s", d.meshPath.c_str());
        ImGui::SetNextItemWidth(w - 34.0f);
        if (ImGui::InputText("##vmesh", meshBuf, sizeof(meshBuf)))
        { d.meshPath = meshBuf; changed = true; }
        ImGui::SameLine();
        if (ImGui::Button("..."))
        {
            std::string picked = editor::openFileDialog(
                "Modelli 3D\0*.glb;*.gltf;*.obj\0Tutti i file\0*.*\0\0",
                getDataDir());
            if (!picked.empty()) { d.meshPath = picked; changed = true; }
        }
        ImGui::TextDisabled("Vuoto = box colorato nel runtime");
    }
    using editor::ui::dragRow;
    changed |= dragRow("Scala",              d.meshScale,   0.005f, 0.001f, 10.0f, "%.3f");
    changed |= dragRow("Rotazione X (deg)",  d.meshRotX,    1.0f, -180.0f, 180.0f, "%.0f");
    changed |= dragRow("Rotazione Y (deg)",  d.meshRotY,    1.0f, -180.0f, 180.0f, "%.0f");
    changed |= dragRow("Altezza mesh (su/giu)", d.meshOffsetY, 0.02f, -3.0f, 3.0f, "%.2f");
    ImGui::TextDisabled("Speeder troppo alto? Abbassa 'Altezza mesh' (valori negativi).");

    ImGui::Separator();
    ImGui::TextColored({0.3f,0.9f,1.0f,1.0f}, "Collisione (box CIANO)");
    ImGui::TextDisabled("Raggiunge il suolo per guidare. NON e' il volume di danno.");
    changed |= dragRow("Half X", d.halfX, 0.02f, 0.1f, 6.0f, "%.2f");
    changed |= dragRow("Half Y", d.halfY, 0.02f, 0.1f, 6.0f, "%.2f");
    changed |= dragRow("Half Z", d.halfZ, 0.02f, 0.1f, 6.0f, "%.2f");

    ImGui::Separator();
    ImGui::TextColored({1.0f,0.85f,0.2f,1.0f}, "Volume di DANNO (box GIALLO)");
    ImGui::TextDisabled("Cio' che i colpi colpiscono. Alza l'Offset per escludere lo\n"
                        "spazio vuoto sotto un mezzo che fluttua. Half=0 usa la collisione.");
    changed |= dragRow("Hit Offset Y", d.hitOffsetY, 0.02f, -3.0f, 3.0f, "%.2f");
    changed |= dragRow("Hit Half X",   d.hitHalfX, 0.02f, 0.0f, 6.0f, "%.2f");
    changed |= dragRow("Hit Half Y",   d.hitHalfY, 0.02f, 0.0f, 6.0f, "%.2f");
    changed |= dragRow("Hit Half Z",   d.hitHalfZ, 0.02f, 0.0f, 6.0f, "%.2f");

    ImGui::Separator();
    changed |= editor::ui::colorRow("Colore", d.color.data());

    ImGui::Separator();
    ImGui::TextDisabled("Fase B (19_Vehicles): hitbox a zone multiple, attach\n"
                        "point, armi di bordo, AI alla guida.");

    ImGui::Separator();
    if (m_dirty) ImGui::TextColored({1.f,0.7f,0.2f,1.f}, "* Modifiche non salvate");
    else         ImGui::TextDisabled("Salvato");
    const float bw = (ImGui::GetContentRegionAvail().x - 4.0f) * 0.5f;
    if (ImGui::Button("Salva JSON", {bw, 0})) saveSelected();
    ImGui::SameLine();
    if (ImGui::Button("Ripristina", {bw, 0}))
    {
        const std::string id = e.id;
        m_pendingSelectId = id;
    }

    if (changed) { m_dirty = true; updateViewport(); }
}

} // namespace editor
