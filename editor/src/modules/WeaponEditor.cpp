// WeaponEditor.cpp
#include "util/DataPath.hpp"
#include "modules/WeaponEditor.hpp"
#include "util/FileDialog.hpp"
#include "util/RigReader.hpp"
#include "util/UiWidgets.hpp"
#include "util/JsonSave.hpp"
#include "util/DefinitionRename.hpp"
#include "mini/game/WeaponHandPose.hpp"   // LA formula della posa: una sola, condivisa col runtime

#include <imgui.h>
#include <SDL2/SDL.h>
#include <nlohmann/json.hpp>
#include <glm/gtc/matrix_transform.hpp>

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
// R8 chiuso: unica risoluzione in util/DataPath (era una delle copie col
// controllo debole: accettava qualunque cartella di nome data).
static std::string getDataDir() { return editor::datapath::dir(); }

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

// ── Trasformazione arma (coerente con loadModel: rotY * rotX * scala) ────────

glm::mat4 WeaponEditor::weaponTransform() const
{
    return glm::rotate(glm::mat4(1.0f), glm::radians(m_rotY), {0,1,0})
         * glm::rotate(glm::mat4(1.0f), glm::radians(m_rotX), {1,0,0})
         * glm::scale(glm::mat4(1.0f), {m_scale, m_scale, m_scale});
}

glm::vec3 WeaponEditor::toWorld(const glm::vec3& modelPos) const
{
    return glm::vec3(weaponTransform() * glm::vec4(modelPos, 1.0f));
}

glm::vec3 WeaponEditor::deltaToLocal(const glm::vec3& worldDelta) const
{
    return glm::inverse(glm::mat3(weaponTransform())) * worldDelta;
}

// ── syncViewportMarkers ───────────────────────────────────────────────────────
// Rende gli attach point visibili nel viewport come oggetti (box + label),
// selezionabili con click e spostabili col gizmo.
void WeaponEditor::syncViewportMarkers()
{
    if (m_showProjectileMesh)
    {
        // Gli attach point appartengono alla mesh dell'arma, non al proiettile.
        m_viewport.clearMarkers();
        m_viewport.setGizmoTarget({0,0,0}, false);
        return;
    }

    std::vector<FreeCameraViewport::ViewportMarker> markers;
    for (auto& [name, ap] : m_attachPoints)
    {
        FreeCameraViewport::ViewportMarker mk;
        mk.name     = name;
        mk.pos      = toWorld({ap.x, ap.y, ap.z});
        mk.r        = 0.2f; mk.g = 1.0f; mk.b = 0.2f;
        mk.selected = (name == m_selAttachPoint);
        markers.push_back(mk);
    }
    m_viewport.setMarkers(markers);

    // Gizmo sul punto selezionato (gli attach point sono punti: solo Sposta)
    if (!m_selAttachPoint.empty() && m_attachPoints.count(m_selAttachPoint))
    {
        auto& ap = m_attachPoints.at(m_selAttachPoint);
        m_viewport.setGizmoTarget(toWorld({ap.x, ap.y, ap.z}), true);
    }
    else
        m_viewport.setGizmoTarget({0,0,0}, false);
    m_viewport.setGizmoCanRotateScale(false, false);
}

// ── tick ─────────────────────────────────────────────────────────────────────

void WeaponEditor::tick(float dt)
{
    m_viewport.tick(dt);

    // Drag del gizmo → sposta l'attach point selezionato (world → model space)
    glm::vec3 delta;
    if (m_viewport.popGizmoDelta(delta))
    {
        if (!m_selAttachPoint.empty() && m_attachPoints.count(m_selAttachPoint))
        {
            glm::vec3 ld = deltaToLocal(delta);
            auto& ap = m_attachPoints[m_selAttachPoint];
            ap.x += ld.x; ap.y += ld.y; ap.z += ld.z;
            m_dirty = true;
            syncViewportMarkers();
        }
    }

    // Click su un marker nel viewport → seleziona l'attach point
    std::string clicked = m_viewport.popClickedItem();
    if (!clicked.empty() && m_attachPoints.count(clicked))
    {
        m_selAttachPoint = clicked;
        syncViewportMarkers();
    }
}

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
        w.meshRotY           = j.value("mesh_rot_y",        0.0f);
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
        w.adsFov             = j.value("ads_fov",         35.f);
        w.spreadBase         = j.value("spread_base",     0.02f);
        w.spreadAds          = j.value("spread_ads",      0.005f);
        w.spreadMove         = j.value("spread_move",     0.06f);
        w.spreadSprint       = j.value("spread_sprint",   0.14f);
        w.spreadJump         = j.value("spread_jump",     0.20f);
        w.faction            = j.value("faction",         std::string("neutral"));
        if (j.contains("bullet_color") && j["bullet_color"].size() >= 3)
            w.bulletColor = {j["bullet_color"][0], j["bullet_color"][1], j["bullet_color"][2]};

        // Posa in mano (KI #49)
        w.handScale = j.value("hand_scale", 0.0f);
        if (j.contains("hand_rot") && j["hand_rot"].size() >= 3)
            w.handRot = {j["hand_rot"][0], j["hand_rot"][1], j["hand_rot"][2]};
        if (j.contains("hand_offset") && j["hand_offset"].size() >= 3)
            w.handOffset = {j["hand_offset"][0], j["hand_offset"][1], j["hand_offset"][2]};

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
    m_rotY  = w.meshRotY;
    m_scale = w.meshScale;
    m_attachPoints  = w.attachPoints;
    m_selAttachPoint = m_attachPoints.count("muzzle") ? "muzzle" : "";

    reloadPreview();
    loadRigJoints();
    syncViewportMarkers();
}

// ── reloadPreview ─────────────────────────────────────────────────────────────

void WeaponEditor::reloadPreview()
{
    if (m_sel < 0 || m_sel >= (int)m_weapons.size()) return;
    const auto& w = m_weapons[m_sel];
    const std::string& field = m_showProjectileMesh ? w.projectileMeshPath : w.meshPath;
    std::string abs = resolveMesh(field);
    if (abs.empty()) { m_viewport.clearModel(); return; }
    m_viewport.loadModel(abs, m_rotX, m_scale, m_rotY);
}

// ── Anteprima IN MANO ─────────────────────────────────────────────────────────
// Il modello principale del viewport diventa il PERSONAGGIO e l'arma passa ad
// attachment, posizionata da `weaponattach::handLocal` — la stessa funzione che
// usa il runtime. Non è un'approssimazione "abbastanza simile": se divergesse,
// l'anteprima direbbe una bugia proprio sul valore che si sta tarando.

void WeaponEditor::refreshHandUnits()
{
    m_handUnits.clear();
    m_handRegistry.loadAll(editor::datapath::root());
    for (const auto& [id, def] : m_handRegistry.allies())
        if (!def.meshPath.empty()) m_handUnits.push_back(id);
    for (const auto& [id, def] : m_handRegistry.enemies())
        if (!def.meshPath.empty()) m_handUnits.push_back(id);
    if (m_handUnit >= (int)m_handUnits.size()) m_handUnit = 0;
}

void WeaponEditor::updateHandPreview()
{
    if (!m_handPreview || m_sel < 0 || m_sel >= (int)m_weapons.size()
        || m_handUnits.empty())
        return;

    const auto& w = m_weapons[m_sel];
    const mini::EnemyDef* unit = m_handRegistry.getAlly(m_handUnits[m_handUnit]);
    if (!unit) unit = m_handRegistry.getEnemy(m_handUnits[m_handUnit]);
    if (!unit) return;

    // Personaggio come modello principale.
    const std::string unitAbs = resolveMesh(unit->meshPath);
    if (unitAbs.empty()) { m_viewport.clearModel(); return; }
    m_viewport.loadModel(unitAbs, unit->meshRotX, unit->meshScale, unit->meshRotY);

    const std::string wAbs = resolveMesh(w.meshPath);
    if (wAbs.empty()) { m_viewport.clearAttachmentModel(); return; }

    // Mano: l'attach point del PERSONAGGIO (l'unica parte della posa che dipende
    // da chi impugna). Si prova quello dichiarato dall'arma, poi "right_hand".
    glm::vec3 hand{0.0f};
    auto ap = unit->attachPoints.find(unit->weaponDisplay.hand);
    if (ap == unit->attachPoints.end()) ap = unit->attachPoints.find("right_hand");
    if (ap != unit->attachPoints.end())
        hand = {ap->second[0], ap->second[1], ap->second[2]};

    // Se la posa NON è autorata sull'arma si mostra il fallback che userebbe il
    // runtime (weapon_display dell'entità): l'anteprima deve far vedere anche il
    // problema, non solo la soluzione — è esattamente il caso del DC-15X.
    const bool authored = w.handScale > 0.0f;
    mini::weaponattach::HandPose p;
    p.hand      = hand;
    p.scale     = authored ? w.handScale : unit->weaponDisplay.scale;
    p.rot       = authored ? glm::vec3{w.handRot[0], w.handRot[1], w.handRot[2]}
                           : glm::vec3{unit->weaponDisplay.rot[0],
                                       unit->weaponDisplay.rot[1],
                                       unit->weaponDisplay.rot[2]};
    p.offset    = authored ? glm::vec3{w.handOffset[0], w.handOffset[1], w.handOffset[2]}
                           : glm::vec3{unit->weaponDisplay.offset[0],
                                       unit->weaponDisplay.offset[1],
                                       unit->weaponDisplay.offset[2]};
    // Grip: `right_hand` ha la precedenza su `grip`, come al caricamento (riga ~141).
    {
        auto g = w.attachPoints.find("right_hand");
        if (g == w.attachPoints.end()) g = w.attachPoints.find("grip");
        if (g != w.attachPoints.end())
            p.grip = {g->second.x, g->second.y, g->second.z};
    }
    p.charScale = unit->meshScale;
    p.baseRotX  = w.meshRotX;
    p.baseRotY  = w.meshRotY;

    const glm::mat4 M =
          glm::rotate(glm::mat4(1.0f), glm::radians(unit->meshRotX), {1,0,0})
        * glm::scale(glm::mat4(1.0f), glm::vec3(unit->meshScale));
    m_viewport.setAttachmentModel(wAbs, M * mini::weaponattach::handLocal(p));
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
    w.meshRotY  = m_rotY;
    w.meshScale = m_scale;
    w.attachPoints = m_attachPoints;

    // saveJsonRMW (ADR-010): unico canale di scrittura JSON dell'editor.
    editor::jsonsave::saveJsonRMW(w.jsonPath, [&](json& j) {
    j.erase("id"); // deprecato: id = nome file (ADR-001)
    j["name"]             = w.name;
    j["faction"]          = w.faction;
    j["mesh"]             = w.meshPath;
    j["projectile_mesh"]  = w.projectileMeshPath;
    j["mesh_rot_x"]       = w.meshRotX;
    j["mesh_rot_y"]       = w.meshRotY;
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
    j["ads_fov"]          = w.adsFov;
    j["spread_base"]      = w.spreadBase;
    j["spread_ads"]       = w.spreadAds;
    j["spread_move"]      = w.spreadMove;
    j["spread_sprint"]    = w.spreadSprint;
    j["spread_jump"]      = w.spreadJump;

    // Posa in mano (KI #49): si scrive solo se autorata (handScale>0). A 0 si
    // rimuove, così un'arma "senza posa" torna esplicitamente al fallback legacy.
    if (w.handScale > 0.0f)
    {
        j["hand_scale"]  = w.handScale;
        j["hand_rot"]    = {w.handRot[0], w.handRot[1], w.handRot[2]};
        j["hand_offset"] = {w.handOffset[0], w.handOffset[1], w.handOffset[2]};
    }
    else
    {
        j.erase("hand_scale"); j.erase("hand_rot"); j.erase("hand_offset");
    }
    return true;
    });
    m_dirty = false;
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

    // Larghezze ridimensionabili (persistite nell'ini di ImGui via ResizeX);
    // il viewport prende lo spazio che resta.
    static float s_panelW = 320.0f;

    ImGui::BeginChild("##wp_panels", ImVec2(totalW, totalH), ImGuiChildFlags_None);

    // Lista (trascina il bordo destro per allargare)
    ImGui::BeginChild("##wlist", ImVec2(180, 0),
                      ImGuiChildFlags_Borders | ImGuiChildFlags_ResizeX);
    drawList(ImGui::GetContentRegionAvail().x);
    ImGui::EndChild();
    const float listW = ImGui::GetItemRectSize().x;

    ImGui::SameLine();

    float vpW = totalW - listW - s_panelW - ImGui::GetStyle().ItemSpacing.x * 2;
    if (vpW < 120.0f) vpW = 120.0f;

    // Viewport
    ImGui::BeginChild("##wvp", ImVec2(vpW, 0), ImGuiChildFlags_None,
                      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    drawViewport(vpW);
    ImGui::EndChild();

    ImGui::SameLine();

    // Maniglia ESPLICITA a sinistra del pannello: `ImGuiChildFlags_ResizeX` mette il
    // grip sul bordo DESTRO, che qui coincide col bordo della finestra — una volta
    // stretto non c'era più nulla da afferrare e il pannello non si riallargava
    // (segnalato dall'utente). Stessa riparazione già fatta nel Map Editor, ora
    // condivisa in `editor::ui::panelSplitter` per non riscoprirla modulo per modulo.
    editor::ui::panelSplitter("##wpsplit", s_panelW, totalH, 180.0f,
                              (totalW > 400.0f) ? totalW * 0.5f : 200.0f);
    ImGui::SameLine();

    // Pannello destra con tab Mesh / Statistiche
    ImGui::BeginChild("##wpanel", ImVec2(s_panelW, 0), ImGuiChildFlags_Borders);
    {
        const float panelW = ImGui::GetContentRegionAvail().x;
        if (ImGui::BeginTabBar("##wtabs"))
        {
            if (ImGui::BeginTabItem("Mesh"))
            {
                drawMeshTab(panelW);
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Statistiche"))
            {
                drawStatsTab(panelW);
                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }
    }
    ImGui::EndChild();
    s_panelW = ImGui::GetItemRectSize().x;

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
    ImGui::TextDisabled("id: %s", w.id.c_str());

    // ── Rinomina (ADR-010): file fisico + sweep cross-reference ─────────
    {
        static char renameBuf[64] = "";
        static std::string renameErr;
        ImGui::SetNextItemWidth(slW - 76.f);
        ImGui::InputText("##wrename", renameBuf, sizeof(renameBuf));
        ImGui::SameLine();
        if (ImGui::Button("Rinomina") && renameBuf[0] != '\0')
        {
            int refs = 0;
            renameErr = editor::rename::renameDefinition(
                getDataDir(), editor::rename::Category::Weapon,
                w.id, renameBuf, &refs);
            if (renameErr.empty())
            {
                std::string newId = renameBuf;
                renameBuf[0] = '\0';
                loadWeapons();
                for (int i = 0; i < (int)m_weapons.size(); ++i)
                    if (m_weapons[i].id == newId) { selectWeapon(i); break; }
                return; // lista rigenerata: chiudi il frame corrente
            }
        }
        // ── Elimina (doc 39 R1) ──────────────────────────────────────────
        // Mancava: si poteva creare e rinominare un'arma ma non toglierla, e
        // l'unico modo era cancellare il file fuori dall'editor. Comando
        // condiviso (`editor::rename::deleteDefinition`), non una copia locale.
        ImGui::SameLine();
        if (ImGui::Button("Elimina")) ImGui::OpenPopup("##wdel");
        if (ImGui::BeginPopup("##wdel"))
        {
            ImGui::Text("Eliminare l'arma '%s'?", w.id.c_str());
            ImGui::TextDisabled("Il file viene cancellato. I riferimenti da entita'/classi");
            ImGui::TextDisabled("resteranno rotti: --validate li segnala.");
            if (ImGui::Button("Elimina", {110, 0}))
            {
                renameErr = editor::rename::deleteDefinition(
                    getDataDir(), editor::rename::Category::Weapon, w.id);
                ImGui::CloseCurrentPopup();
                if (renameErr.empty())
                {
                    ImGui::EndPopup();
                    loadWeapons();
                    if (!m_weapons.empty()) selectWeapon(0); else m_sel = -1;
                    return;   // lista rigenerata: chiudi il frame corrente
                }
            }
            ImGui::SameLine();
            if (ImGui::Button("Annulla", {110, 0})) ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
        }
        if (!renameErr.empty())
            ImGui::TextColored({1.f,0.4f,0.4f,1.f}, "%s", renameErr.c_str());
    }
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

    ImGui::TextDisabled("Mesh proiettile (non attiva: il runtime usa il cubo — KI #25)");
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
        if (ImGui::RadioButton("Arma", s))
            { m_showProjectileMesh = false; reloadPreview(); syncViewportMarkers(); }
        ImGui::SameLine();
        if (ImGui::RadioButton("Proiettile", m_showProjectileMesh))
            { m_showProjectileMesh = true; reloadPreview(); syncViewportMarkers(); }
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

    if (floatRow("RotY", m_rotY, -180.f, 180.f, 1.0f, "%.0f"))
    { changed = true; reloadPreview(); syncViewportMarkers(); }
    if (floatRow("RotX", m_rotX, -180.f, 180.f, 1.0f, "%.0f"))
    { changed = true; reloadPreview(); syncViewportMarkers(); }
    if (floatRow("Scala", m_scale, 0.01f, 5.0f, 0.01f, "%.3f"))
    { changed = true; reloadPreview(); syncViewportMarkers(); }

    ImGui::Separator();

    // ── Attach points ───────────────────────────────────────────────────
    ImGui::TextDisabled("Attach Points");

    ImGui::TextWrapped("I punti appaiono nel viewport: clicca per selezionare, "
                       "trascina le frecce per spostare.");

    // Lista degli attach points correnti
    for (auto& [apName, ap] : m_attachPoints)
    {
        bool apSel = (apName == m_selAttachPoint);
        if (ImGui::Selectable(apName.c_str(), apSel, 0, ImVec2(slW * 0.45f, 0)))
        { m_selAttachPoint = apName; syncViewportMarkers(); }
        ImGui::SameLine();
        ImGui::TextDisabled("(%.2f, %.2f, %.2f)", ap.x, ap.y, ap.z);
    }

    // Pannello edit del punto selezionato
    if (!m_selAttachPoint.empty() && m_attachPoints.count(m_selAttachPoint))
    {
        auto& ap = m_attachPoints.at(m_selAttachPoint);
        float xyz[3] = {ap.x, ap.y, ap.z};
        if (editor::ui::sliderRow3("apx", xyz, -2.0f, 2.0f, 0.001f, "%.3f"))
        {
            ap.x = xyz[0]; ap.y = xyz[1]; ap.z = xyz[2];
            changed = true;
        }

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

    // ── Posa in mano (KI #49) ─────────────────────────────────────────────
    // Come l'arma sta impugnata: vale per QUALUNQUE unità la usi (prima era
    // tarata per singola entità → cambiando arma via classe usciva sbagliata).
    ImGui::SeparatorText("Posa in mano (vale per ogni unita')");
    {
        auto& w2 = m_weapons[m_sel];

        // ── Anteprima IN MANO: si tara guardando, non a memoria ───────────
        // La scala compensa la dimensione NATIVA del mesh e varia di quattro
        // ordini di grandezza fra le armi (0.0015 → 80): indovinarla senza
        // vederla non è realistico, ed è il motivo per cui il DC-15X è rimasto
        // senza posa e in partita appariva minuscolo.
        if (ImGui::Checkbox("Anteprima in mano", &m_handPreview))
        {
            if (m_handPreview) { refreshHandUnits(); updateHandPreview(); }
            else               { m_viewport.clearAttachmentModel(); reloadPreview(); }
        }
        if (m_handPreview && !m_handUnits.empty())
        {
            ImGui::SameLine();
            ImGui::SetNextItemWidth(200.0f);
            if (ImGui::BeginCombo("##handunit", m_handUnits[m_handUnit].c_str()))
            {
                for (int u = 0; u < (int)m_handUnits.size(); ++u)
                {
                    const bool s = (u == m_handUnit);
                    if (ImGui::Selectable(m_handUnits[u].c_str(), s))
                    { m_handUnit = u; updateHandPreview(); }
                    if (s) ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }
            if (w2.handScale <= 0.0f)
                ImGui::TextColored({1.f,0.6f,0.3f,1.f},
                    "Posa NON autorata: stai vedendo il fallback dell'entita'.");
        }
        else if (m_handPreview)
            ImGui::TextDisabled("Nessuna entita' con mesh in data/allies o data/enemies.");

        bool authored = w2.handScale > 0.0f;
        if (ImGui::Checkbox("Posa definita da quest'arma", &authored))
        {
            // Attivando: parte da 1.0 (poi si tara guardando in gioco/anteprima
            // unità). Disattivando: torna al fallback legacy weapon_display.
            w2.handScale = authored ? (w2.handScale > 0.0f ? w2.handScale : 1.0f) : 0.0f;
            m_dirty = true;
            updateHandPreview();
        }
        if (w2.handScale > 0.0f)
        {
            bool poseChanged = false;
            // Scala LOGARITMICA: fra 0.0015 (E-5C) e 80 (Z-6) ci sono quattro
            // ordini di grandezza — con un drag lineare non si arriva mai al
            // valore giusto partendo da 1.0.
            poseChanged |= ImGui::DragFloat("Scala in mano", &w2.handScale, 0.01f,
                                            0.0005f, 200.0f, "%.4f",
                                            ImGuiSliderFlags_Logarithmic);
            poseChanged |= ImGui::DragFloat3("Rotazione in mano", w2.handRot.data(),
                                             1.0f, -180.0f, 180.0f, "%.0f");
            poseChanged |= ImGui::DragFloat3("Offset in mano", w2.handOffset.data(),
                                             0.005f, -1.0f, 1.0f, "%.3f");
            if (poseChanged) { m_dirty = true; updateHandPreview(); }
            ImGui::TextDisabled("La scala compensa la dimensione NATIVA del mesh:\n"
                                "es. Z-6 ~80, DC-15A ~0.4, E-5C ~0.0015.");
        }
        else
            ImGui::TextDisabled("Non autorata: le unita' usano il loro weapon_display\n"
                                "legacy (transizione KI #49). Attivala per centralizzarla.");
    }

    ImGui::Separator();
    if (changed) { m_dirty = true; syncViewportMarkers(); }
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

    // Etichetta a sinistra (non tagliata) + campo che riempie il resto.
    auto drag = [&](const char* label, float& v, float lo, float hi, const char* fmt = "%.2f") {
        if (editor::ui::dragRow(label, v, (hi-lo)*0.005f, lo, hi, fmt)) changed = true;
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
    ImGui::TextDisabled("Gittata e mira");
    drag("Gittata effettiva", w.effectiveRange, 1.f, 100.f);
    drag("Gittata minima (non attiva)", w.minRange, 0.f, 20.f);
    drag("Zoom in mira / FOV (basso = piu' zoom)", w.adsFov, 10.f, 60.f);
    ImGui::TextDisabled("(non attiva) = salvata ma non consumata dal runtime — KI #25");

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
