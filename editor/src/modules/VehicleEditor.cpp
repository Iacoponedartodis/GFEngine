// VehicleEditor.cpp — modulo veicoli (19_Vehicles).
// Layout: [lista | viewport 3D | proprietà]. Salvataggio RMW (ADR-010),
// id = nome file (ADR-001), rinomina con sweep vehicle_spawns nelle mappe.

#include "modules/VehicleEditor.hpp"
#include "mini/game/data/DefinitionRegistry.hpp"
#include "util/FileDialog.hpp"
#include "util/JsonSave.hpp"
#include "util/DefinitionRename.hpp"

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

static std::string getDataDir()
{
    char* base = SDL_GetBasePath();
    std::string exeDir = base ? base : "./";
    SDL_free(base);
    std::error_code ec;
    fs::path sourceData = fs::canonical(fs::path(exeDir) / "../../../data", ec);
    if (!ec && fs::exists(sourceData, ec))
        return sourceData.string() + "/";
    return exeDir + "data/";
}

VehicleEditor::VehicleEditor()
{
    loadEntries();
    if (!m_entries.empty()) selectEntry(0);
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
        m_viewport.loadModel(d.meshPath, 0.0f, d.meshScale);
    else
        m_viewport.clearModel();

    // Box di collisione come wireframe (centrato sull'origine, poggiato
    // idealmente a terra: il runtime spawna con centro a halfY dal suolo).
    std::vector<FreeCameraViewport::MapBoxDraw> boxes;
    FreeCameraViewport::MapBoxDraw b;
    b.x = 0; b.y = d.halfY; b.z = 0; b.ry = d.meshRotY;
    b.sx = d.halfX * 2.0f; b.sy = d.halfY * 2.0f; b.sz = d.halfZ * 2.0f;
    b.r = d.color[0]; b.g = d.color[1]; b.b = d.color[2];
    b.selected = true;
    boxes.push_back(b);
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
        j["mesh_rot_y"]    = d.meshRotY;
        j["half_x"]        = d.halfX;
        j["half_y"]        = d.halfY;
        j["half_z"]        = d.halfZ;
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

    // ── Lista (sinistra) ─────────────────────────────────────────────
    ImGui::BeginGroup();
    ImGui::Text("Veicoli:");
    ImGui::BeginChild("##vlist", ImVec2(m_listW, -78),
                      ImGuiChildFlags_Borders | ImGuiChildFlags_ResizeX);
    for (int i = 0; i < (int)m_entries.size(); ++i)
    {
        const auto& e = m_entries[i];
        if (ImGui::Selectable((e.def.name + "##v" + std::to_string(i)).c_str(),
                              m_sel == i))
            selectEntry(i);
    }
    ImGui::EndChild();
    m_listW = ImGui::GetItemRectSize().x;

    // Nuovo veicolo
    {
        static char newVId[64] = "";
        ImGui::SetNextItemWidth(m_listW);
        ImGui::InputText("##newvid", newVId, sizeof(newVId));
        if (ImGui::Button("+ Nuovo veicolo", {m_listW, 0}) && newVId[0] != '\0')
        {
            const std::string path = getDataDir() + "vehicles/" + newVId + ".json";
            if (!fs::exists(path))
            {
                mini::VehicleDef d;
                d.name = newVId;
                editor::jsonsave::saveJsonRMW(path, [&](json& j) {
                    j["name"]          = d.name;
                    j["hp"]            = d.hp;
                    j["max_speed"]     = d.maxSpeed;
                    j["accel"]         = d.accel;
                    j["turn_rate_deg"] = d.turnRateDeg;
                    j["half_x"] = d.halfX; j["half_y"] = d.halfY; j["half_z"] = d.halfZ;
                    j["color"]  = {d.color[0], d.color[1], d.color[2]};
                    return true;
                });
                m_pendingSelectId = newVId;
                newVId[0] = '\0';
            }
        }
    }
    if (ImGui::Button("Ricarica lista", {m_listW, 0})) loadEntries();
    ImGui::EndGroup();

    ImGui::SameLine();

    // ── Viewport 3D (centro) ─────────────────────────────────────────
    static float s_propW = 300.0f;
    const float vpW = ImGui::GetContentRegionAvail().x - s_propW
                    - ImGui::GetStyle().ItemSpacing.x;
    ImGui::BeginChild("##vvp", ImVec2(vpW < 120.0f ? 120.0f : vpW, 0),
                      ImGuiChildFlags_None,
                      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    m_viewport.draw(false);
    ImGui::EndChild();

    ImGui::SameLine();

    // ── Proprietà (destra) ───────────────────────────────────────────
    ImGui::BeginChild("##vprops", ImVec2(s_propW, 0),
                      ImGuiChildFlags_Borders | ImGuiChildFlags_ResizeX);
    drawProperties();
    ImGui::EndChild();
    s_propW = ImGui::GetItemRectSize().x;
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
        if (ImGui::InputText("Nome", nameBuf, sizeof(nameBuf)))
        { d.name = nameBuf; changed = true; }
    }

    ImGui::Separator();
    ImGui::TextDisabled("Statistiche");
    changed |= ImGui::DragFloat("HP",             &d.hp,          1.0f, 10.0f, 5000.0f, "%.0f");
    changed |= ImGui::DragFloat("Vel. max m/s",   &d.maxSpeed,    0.2f,  2.0f,   60.0f, "%.1f");
    changed |= ImGui::DragFloat("Accelerazione",  &d.accel,       0.2f,  1.0f,   60.0f, "%.1f");
    changed |= ImGui::DragFloat("Sterzata deg/s", &d.turnRateDeg, 1.0f, 10.0f,  360.0f, "%.0f");

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
    changed |= ImGui::DragFloat("Scala",     &d.meshScale, 0.005f, 0.001f, 10.0f, "%.3f");
    changed |= ImGui::DragFloat("Rot Y deg", &d.meshRotY,  1.0f, -180.0f, 180.0f, "%.0f");

    ImGui::Separator();
    ImGui::TextDisabled("Collisione (box, wireframe nel viewport)");
    changed |= ImGui::DragFloat("Half X", &d.halfX, 0.02f, 0.1f, 6.0f, "%.2f");
    changed |= ImGui::DragFloat("Half Y", &d.halfY, 0.02f, 0.1f, 6.0f, "%.2f");
    changed |= ImGui::DragFloat("Half Z", &d.halfZ, 0.02f, 0.1f, 6.0f, "%.2f");

    ImGui::Separator();
    changed |= ImGui::ColorEdit3("Colore", d.color.data());

    ImGui::Separator();
    ImGui::TextDisabled("Fase B (19_Vehicles): hitbox a zone, attach point,\n"
                        "armi di bordo, authoring spawn nel Map Editor.");

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
