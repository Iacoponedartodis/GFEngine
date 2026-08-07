#include "util/DataPath.hpp"
#include "modules/EntityEditor.hpp"
#include "util/RigReader.hpp"
#include "util/FileDialog.hpp"
#include "util/UiWidgets.hpp"
#include "util/JsonSave.hpp"
#include "util/DefinitionRename.hpp"
#include "util/StartupOptions.hpp"
#include "mini/game/ClassResolve.hpp"   // ADR-022: la regola "la classe vince", non una copia
#include "mini/game/WeaponHandPose.hpp"  // LA formula della posa in mano: una sola, condivisa col runtime
#include <imgui.h>
#include <nlohmann/json.hpp>
#include <SDL2/SDL.h>
#include <fstream>
#include <iostream>
#include <cstring>
#include <filesystem>
#include <cmath>
#include <cctype>
#include <algorithm>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

using json = nlohmann::json;
namespace fs = std::filesystem;

namespace editor
{

// R8 chiuso: unica risoluzione in util/DataPath (era una delle copie col
// controllo debole: accettava qualunque cartella di nome data).
static std::string getDataDir() { return editor::datapath::dir(); }

static std::string getExeDir()
{
    char* base = SDL_GetBasePath();
    std::string dir = base ? base : "./";
    SDL_free(base);
    return dir;
}

static std::string getAssetsDir()
{
    std::error_code ec;
    fs::path assetsDir = fs::canonical(fs::path(getExeDir()) / "../../../assets", ec);
    if (!ec && fs::exists(assetsDir)) return assetsDir.string();
    return getExeDir();
}

static bool browseForEntityMesh(std::string& outPath)
{
    std::string abs = openFileDialog(
        "Modelli 3D\0*.glb;*.gltf;*.obj\0Tutti i file\0*.*\0\0",
        getAssetsDir());
    if (abs.empty()) return false;
    auto toFwd = [](std::string s) { for (auto& c : s) if (c=='\\') c='/'; return s; };
    std::string absF = toFwd(abs);
    std::error_code ec;
    fs::path projRoot = fs::canonical(fs::path(getExeDir()) / "../../..", ec);
    if (!ec) {
        std::string pr = toFwd(projRoot.string()) + "/";
        if (absF.find(pr) == 0) { outPath = absF.substr(pr.size()); return true; }
    }
    outPath = absF;
    return true;
}

static std::string resolveAssetPath(const std::string& meshField)
{
    if (meshField.empty()) return {};
    char* base = SDL_GetBasePath();
    fs::path exeDir = base ? base : ".";
    SDL_free(base);

    if (meshField.size() >= 7 && meshField.substr(0,7) == "assets/")
    {
        std::error_code ec;
        fs::path cand = fs::canonical(exeDir / "../../../" / meshField, ec);
        if (!ec && fs::exists(cand)) return cand.string();
    }
    std::error_code ec;
    fs::path cand = fs::canonical(exeDir / meshField, ec);
    if (!ec && fs::exists(cand)) return cand.string();
    return {};
}

static float jf(const json& j, const char* k, float def)
{
    if (j.contains(k) && j[k].is_number()) return j[k].get<float>();
    return def;
}

// ── Costanti nomi attach point standard ──────────────────────────────────────
static const char* k_standardPoints[] = { "foot", "eye", "right_hand", "left_hand", "muzzle" };
static const char* k_pointDesc[] = {
    "Piedi — usato per ground placement (spawn_y = ground - foot.y)",
    "Occhi — LoS AI, futura camera TPS/FPS",
    "Mano destra — attachment arma",
    "Mano sinistra — shield / secondary",
    "Bocca fucile — effetti sparo (tracers, muzzle flash)"
};

EntityEditor::EntityEditor()
{
    loadEntries();
    loadAvailableIds();

    // `--entity <id|indice>`: seleziona subito, così il percorso di CARICAMENTO
    // MODELLO (mesh, rig, ossa, arma in mano) viene esercitato senza mouse.
    // Senza questa opzione il modulo si apre a `m_sel = -1`, cioè con una viewport
    // vuota: tutte le riproduzioni automatiche di KI #98 hanno collaudato lo
    // scenario in cui non c'è nulla da disegnare, che non è quello dell'utente.
    if (editor::startup::g_entitySelectSet && !m_entries.empty())
    {
        int idx = -1;
        for (int i = 0; i < (int)m_entries.size(); ++i)
            if (m_entries[i].id == editor::startup::g_entitySelect) { idx = i; break; }
        if (idx < 0)
        {
            char* end = nullptr;
            const long n = std::strtol(editor::startup::g_entitySelect.c_str(), &end, 10);
            if (end && *end == '\0' && n >= 0 && n < (long)m_entries.size()) idx = (int)n;
        }
        if (idx >= 0)
        {
            std::printf("[EntityEditor] selezione all'avvio: %s\n",
                        m_entries[idx].id.c_str());
            selectEntry(idx);
        }
        else
            std::fprintf(stderr, "[EntityEditor] --entity: '%s' non trovata\n",
                         editor::startup::g_entitySelect.c_str());
    }
}

void EntityEditor::loadAvailableIds()
{
    auto scanIds = [&](const std::string& subdir) -> std::vector<std::string>
    {
        std::vector<std::string> ids;
        fs::path folder = getDataDir() + subdir;
        std::error_code ec;
        for (auto& e : fs::directory_iterator(folder, ec))
            if (e.path().extension() == ".json")
                ids.push_back(e.path().stem().string());
        std::sort(ids.begin(), ids.end());
        return ids;
    };
    m_availableClasses  = scanIds("classes");
    m_availableHitboxes = scanIds("hitboxes");

    // Registry: serve solo a risolvere l'arma della classe via `classres`.
    // Ricaricata insieme alle liste, così una classe appena creata/modificata in
    // Moduli → Classi si riflette subito nell'anteprima dell'arma in mano.
    m_registry.loadAll(getDataDir());
}

void EntityEditor::loadEntries()
{
    m_entries.clear();
    m_sel = -1;
    m_attachPoints.clear();
    m_selAttachPoint.clear();
    const std::string dataDir = getDataDir();

    auto scanDir = [&](const std::string& subdir, bool isAlly)
    {
        fs::path folder = dataDir + subdir;
        if (!fs::exists(folder)) return;
        std::error_code ec;
        for (auto& entry : fs::directory_iterator(folder, ec))
        {
            if (entry.path().extension() != ".json") continue;
            std::ifstream f(entry.path());
            if (!f.is_open()) continue;
            json j;
            try { f >> j; } catch (...) { continue; }

            EntityEntry e;
            e.isAlly   = isAlly;
            e.jsonPath = entry.path().string();
            e.id       = j.contains("id") ? j["id"].get<std::string>()
                                          : entry.path().stem().string();
            e.name     = j.contains("name") ? j["name"].get<std::string>() : e.id;
            e.faction  = j.value("faction", std::string("neutral"));
            e.meshPath  = j.contains("mesh") ? j["mesh"].get<std::string>() : "";
            e.meshRotX  = jf(j, "mesh_rot_x", 0.0f);
            e.meshRotY  = jf(j, "mesh_rot_y", 0.0f);
            e.meshScale = jf(j, "mesh_scale", 1.0f);

            // Solo `hp`: move_speed e damage_scale non si leggono nemmeno più —
            // il primo lo sovrascrive sempre il profilo AI, il secondo non ha
            // consumatori. Tenerli in memoria significava solo poterli riscrivere
            // per sbaglio da uno stato che l'utente non vede.
            if (j.contains("stats") && j["stats"].is_object())
                e.hp = jf(j["stats"], "hp", 80.0f);

            e.classId         = j.value("class",          std::string(""));
            e.hitboxProfileId = j.value("hitbox_profile", std::string(""));
            // `weapons` serve ancora, ma in SOLA LETTURA: è il fallback quando
            // l'unità non ha una classe (classres), e alimenta l'anteprima
            // dell'arma in mano. Non lo si edita più qui (ADR-022).
            if (j.contains("weapons") && j["weapons"].is_array())
                for (auto& wid : j["weapons"]) e.weaponIds.push_back(wid.get<std::string>());

            if (j.contains("attach_points") && j["attach_points"].is_object())
            {
                for (auto& [apName, apVal] : j["attach_points"].items())
                {
                    AttachPointEntry ap;
                    if (apVal.is_array() && apVal.size() >= 3)
                    {
                        ap.x = apVal[0].get<float>();
                        ap.y = apVal[1].get<float>();
                        ap.z = apVal[2].get<float>();
                    }
                    e.attachPoints[apName] = ap;
                }
            }

            // ── Hitbox: fonte autorevole = PROFILO (ADR-006) ──────────────
            // Il runtime legge solo data/hitboxes/<profileId>.json. L'editor
            // carica dal profilo se esiste; le zone inline "hitbox_zones" nel
            // JSON entità sono legacy e usate solo come fallback di migrazione.
            {
                std::string profileId = e.hitboxProfileId.empty() ? e.id
                                                                  : e.hitboxProfileId;
                fs::path profPath = fs::path(dataDir) / "hitboxes" / (profileId + ".json");
                bool loadedFromProfile = false;
                std::error_code pec;
                if (fs::exists(profPath, pec))
                {
                    std::ifstream pf(profPath);
                    json pj;
                    if (pf.is_open()) { try { pf >> pj; } catch (...) { pj = {}; } }
                    if (pj.contains("zones") && pj["zones"].is_array())
                    {
                        for (auto& zj : pj["zones"])
                        {
                            EntityEntry::InlineHitZone z;
                            z.name = zj.value("name", std::string("zona"));
                            if (zj.contains("offset") && zj["offset"].size() >= 3)
                                z.offset = {zj["offset"][0], zj["offset"][1], zj["offset"][2]};
                            if (zj.contains("half_extents") && zj["half_extents"].size() >= 3)
                                z.halfExt = {zj["half_extents"][0], zj["half_extents"][1], zj["half_extents"][2]};
                            z.damageMult   = zj.value("damage_multiplier", 1.0f);
                            z.boneName     = zj.value("bone", std::string(""));
                            z.debugVisible = zj.value("debug_visible", true);
                            if (zj.contains("rotation") && zj["rotation"].size() >= 3)
                                z.eulerDeg = {zj["rotation"][0], zj["rotation"][1], zj["rotation"][2]};
                            e.hitboxZones.push_back(z);
                        }
                        loadedFromProfile = true;
                    }
                }

                // Fallback legacy: zone inline nel JSON entità (pre-ADR-006)
                if (!loadedFromProfile
                    && j.contains("hitbox_zones") && j["hitbox_zones"].is_array())
                {
                    for (auto& zj : j["hitbox_zones"])
                    {
                        EntityEntry::InlineHitZone z;
                        z.name = zj.value("name", std::string("zona"));
                        if (zj.contains("offset") && zj["offset"].size() >= 3)
                            z.offset = {zj["offset"][0], zj["offset"][1], zj["offset"][2]};
                        if (zj.contains("half_extents") && zj["half_extents"].size() >= 3)
                            z.halfExt = {zj["half_extents"][0], zj["half_extents"][1], zj["half_extents"][2]};
                        z.damageMult = zj.value("damage_mult", 1.0f);
                        z.boneName   = zj.value("bone", std::string(""));
                        if (zj.contains("rotation") && zj["rotation"].size() >= 3)
                            z.eulerDeg = {zj["rotation"][0], zj["rotation"][1], zj["rotation"][2]};
                        e.hitboxZones.push_back(z);
                    }
                }
            }

            if (j.contains("weapon_display") && j["weapon_display"].is_object())
            {
                auto& wd = j["weapon_display"];
                e.dispWeaponId    = wd.value("id", std::string(""));
                e.dispWeaponScale = wd.value("scale", 1.0f);
                e.dispWeaponHand  = wd.value("hand", std::string("right_hand"));
                if (wd.contains("rot") && wd["rot"].size() >= 3)
                    e.dispWeaponRot = {wd["rot"][0], wd["rot"][1], wd["rot"][2]};
                if (wd.contains("offset") && wd["offset"].size() >= 3)
                    e.dispWeaponOffset = {wd["offset"][0], wd["offset"][1], wd["offset"][2]};
            }

            m_entries.push_back(std::move(e));
        }
    };

    scanDir("enemies", false);
    scanDir("allies",  true);
}

void EntityEditor::loadZonesFromProfile(const std::string& profileId)
{
    m_hitboxZones.clear();
    if (profileId.empty()) return;

    std::ifstream pf(getDataDir() + "hitboxes/" + profileId + ".json");
    if (!pf.is_open()) return; // profilo nuovo: si parte da zero zone
    json pj;
    try { pf >> pj; } catch (...) { return; }
    if (!pj.contains("zones") || !pj["zones"].is_array()) return;

    for (auto& zj : pj["zones"])
    {
        EntityEntry::InlineHitZone z;
        z.name = zj.value("name", std::string("zona"));
        if (zj.contains("offset") && zj["offset"].size() >= 3)
            z.offset = {zj["offset"][0], zj["offset"][1], zj["offset"][2]};
        if (zj.contains("half_extents") && zj["half_extents"].size() >= 3)
            z.halfExt = {zj["half_extents"][0], zj["half_extents"][1], zj["half_extents"][2]};
        z.damageMult   = zj.value("damage_multiplier", 1.0f);
        z.boneName     = zj.value("bone", std::string(""));
        z.debugVisible = zj.value("debug_visible", true);
        if (zj.contains("rotation") && zj["rotation"].size() >= 3)
            z.eulerDeg = {zj["rotation"][0], zj["rotation"][1], zj["rotation"][2]};
        m_hitboxZones.push_back(z);
    }
}

void EntityEditor::selectEntry(int idx)
{
    if (idx < 0 || idx >= (int)m_entries.size()) return;
    m_sel   = idx;
    m_dirty = false;
    // La cronologia appartiene all'ENTITÀ che si stava modificando: conservarla
    // attraverso un cambio di selezione farebbe applicare a questa entità lo stato
    // di un'altra — un annullamento che invece di riparare rompe.
    m_undo.clear();
    auto& e = m_entries[idx];
    m_rotX  = e.meshRotX;
    m_rotY  = e.meshRotY;
    m_scale = e.meshScale;

    m_attachPoints   = e.attachPoints;
    m_selAttachPoint = m_attachPoints.count("foot") ? "foot" : "";
    m_hitboxZones    = e.hitboxZones;
    m_selZone        = -1;

    // Arma in mano: STESSA precedenza del runtime (WeaponAttach), risolta con la
    // STESSA funzione — classe (ADR-022) → loadout dell'entità → display legacy.
    // Riscriverla qui è come l'ha riscritta il runtime: diverge (bug 2026-07-17).
    m_weaponId = mini::classres::primaryWeaponId(
        m_registry, e.classId, e.weaponIds.empty() ? std::string() : e.weaponIds[0]);
    if (m_weaponId.empty()) m_weaponId = e.dispWeaponId;
    m_weaponScale    = e.dispWeaponScale;
    m_weaponRot      = e.dispWeaponRot;
    m_weaponOffset   = e.dispWeaponOffset;
    m_weaponHandPoint= e.dispWeaponHand.empty() ? "right_hand" : e.dispWeaponHand;

    reloadPreview();
    updateMarker();
    loadRigJoints();
    loadBones();
    loadWeaponPreview();
    syncViewportMarkers();
}

void EntityEditor::reloadPreview()
{
    if (m_sel < 0 || m_sel >= (int)m_entries.size()) return;
    auto& e = m_entries[m_sel];
    if (e.meshPath.empty()) { m_viewport.clearModel(); return; }
    std::string absPath = resolveAssetPath(e.meshPath);
    if (absPath.empty())
    {
        SDL_Log("[EntityEditor] Mesh non trovata: %s", e.meshPath.c_str());
        m_viewport.clearModel();
        return;
    }
    m_viewport.loadModel(absPath, m_rotX, m_scale);
}

void EntityEditor::updateMarker()
{
    if (!m_selAttachPoint.empty() && m_attachPoints.count(m_selAttachPoint))
    {
        float y = m_attachPoints.at(m_selAttachPoint).y;
        m_viewport.setFootMarker(y, true);
    }
    else
    {
        m_viewport.setFootMarker(0.0f, false);
    }
}

void EntityEditor::loadRigJoints()
{
    m_rigJoints.clear();
    if (m_sel < 0 || m_sel >= (int)m_entries.size()) return;
    const std::string& meshPath = m_entries[m_sel].meshPath;
    if (meshPath.empty()) return;
    std::string absPath = resolveAssetPath(meshPath);
    if (absPath.empty()) return;
    std::string ext2 = absPath.size() >= 4 ? absPath.substr(absPath.size()-4) : "";
    for (auto& c : ext2) c = (char)std::tolower((unsigned char)c);
    if (ext2 != ".glb") return;
    m_rigJoints = readGlbJointNames(absPath);
}

void EntityEditor::loadBones()
{
    m_joints.clear();
    m_viewport.clearBones();
    if (m_sel < 0 || m_sel >= (int)m_entries.size()) return;
    const std::string& meshPath = m_entries[m_sel].meshPath;
    if (meshPath.empty()) return;
    std::string absPath = resolveAssetPath(meshPath);
    if (absPath.empty()) return;
    std::string ext = absPath.size() >= 4 ? absPath.substr(absPath.size()-4) : "";
    for (auto& c : ext) c = (char)std::tolower((unsigned char)c);
    if (ext != ".glb") return;
    m_joints = readGlbJointData(absPath);
    if (!m_joints.empty())
        m_viewport.setBoneData(m_joints, m_rotX, m_scale);
}

glm::mat4 EntityEditor::charTransform() const
{
    return glm::rotate(glm::mat4(1.0f), glm::radians(m_rotX), {1,0,0})
         * glm::scale(glm::mat4(1.0f), {m_scale, m_scale, m_scale});
}

glm::vec3 EntityEditor::toWorld(const glm::vec3& modelPos) const
{
    return glm::vec3(charTransform() * glm::vec4(modelPos, 1.0f));
}

glm::vec3 EntityEditor::deltaToLocal(const glm::vec3& worldDelta) const
{
    return glm::inverse(glm::mat3(charTransform())) * worldDelta;
}

void EntityEditor::loadWeaponPreview()
{
    m_weaponMeshPath.clear();
    m_weaponGrip = {0, 0, 0};
    m_viewport.clearAttachmentModel();
    if (m_weaponId.empty()) return;

    std::ifstream f(getDataDir() + "weapons/" + m_weaponId + ".json");
    if (!f.is_open()) return;
    json j; try { f >> j; } catch (...) { return; }

    m_weaponMeshPath = j.value("mesh", std::string(""));
    // Correzione canonica dell'arma (come il runtime weaponattach)
    m_weaponBaseRotX = j.value("mesh_rot_x", 0.0f);
    m_weaponBaseRotY = j.value("mesh_rot_y", 0.0f);
    if (j.contains("attach_points") && j["attach_points"].is_object())
    {
        auto& ap = j["attach_points"];
        for (const char* k : {"right_hand", "grip", "left_hand"})
            if (ap.contains(k) && ap[k].is_array() && ap[k].size() >= 3)
            { m_weaponGrip = {ap[k][0], ap[k][1], ap[k][2]}; break; }
    }

    // Posa in mano dall'ARMA (KI #49). Se autorata (hand_scale>0) VINCE: la
    // preview e i valori mostrati vengono dall'arma, identici al runtime, e la
    // posa non si edita più qui ma nel Weapon Editor. Senza (legacy) restano i
    // valori dell'entità già caricati in selectEntry (fallback di transizione).
    const float hs = j.value("hand_scale", 0.0f);
    m_weaponPoseFromWeapon = (hs > 0.0f);
    if (m_weaponPoseFromWeapon)
    {
        m_weaponScale = hs;
        if (j.contains("hand_rot") && j["hand_rot"].size() >= 3)
            m_weaponRot = {j["hand_rot"][0], j["hand_rot"][1], j["hand_rot"][2]};
        else m_weaponRot = {0,0,0};
        if (j.contains("hand_offset") && j["hand_offset"].size() >= 3)
            m_weaponOffset = {j["hand_offset"][0], j["hand_offset"][1], j["hand_offset"][2]};
        else m_weaponOffset = {0,0,0};
    }
    updateWeaponTransform();
}

void EntityEditor::updateWeaponTransform()
{
    if (m_weaponId.empty() || m_weaponMeshPath.empty())
    { m_viewport.clearAttachmentModel(); return; }
    std::string abs = resolveAssetPath(m_weaponMeshPath);
    if (abs.empty()) { m_viewport.clearAttachmentModel(); return; }

    // Mano del personaggio (model space) dall'attach point.
    glm::vec3 hand{0, 0, 0};
    if (m_attachPoints.count(m_weaponHandPoint))
    { auto& h = m_attachPoints.at(m_weaponHandPoint); hand = {h.x, h.y, h.z}; }

    // Trasformazione del personaggio (coerente con modello + marker).
    glm::mat4 M = glm::rotate(glm::mat4(1.0f), glm::radians(m_rotX), {1,0,0})
                * glm::scale(glm::mat4(1.0f), {m_scale, m_scale, m_scale});

    // La posa la calcola `weaponattach::handLocal`, LA STESSA funzione del
    // runtime: qui c'era una copia della formula, allineata solo da un commento
    // ("DEVE combaciare con..."). Un'anteprima che diverge dal gioco è peggio di
    // nessuna anteprima — mostra una cosa e ne salva un'altra.
    mini::weaponattach::HandPose p;
    p.hand      = hand;
    p.offset    = m_weaponOffset;
    p.rot       = m_weaponRot;
    p.grip      = m_weaponGrip;
    p.scale     = m_weaponScale;
    p.charScale = m_scale;
    p.baseRotX  = m_weaponBaseRotX;
    p.baseRotY  = m_weaponBaseRotY;

    m_viewport.setAttachmentModel(abs, M * mini::weaponattach::handLocal(p));
}

void EntityEditor::syncViewportMarkers()
{
    glm::mat4 M = glm::rotate(glm::mat4(1.0f), glm::radians(m_rotX), {1,0,0})
                * glm::scale(glm::mat4(1.0f), {m_scale, m_scale, m_scale});

    std::vector<FreeCameraViewport::ViewportMarker> markers;

    for (auto& [name, ap] : m_attachPoints)
    {
        glm::vec3 wpos = glm::vec3(M * glm::vec4(ap.x, ap.y, ap.z, 1.0f));
        FreeCameraViewport::ViewportMarker mk;
        mk.name     = name;
        mk.pos      = wpos;
        mk.r        = 0.2f; mk.g = 1.0f; mk.b = 0.2f;
        mk.selected = (name == m_selAttachPoint);
        markers.push_back(mk);
    }

    for (int i = 0; i < (int)m_hitboxZones.size(); ++i)
    {
        auto& z = m_hitboxZones[i];
        glm::vec3 wpos = glm::vec3(M * glm::vec4(z.offset, 1.0f));
        FreeCameraViewport::ViewportMarker mk;
        mk.name     = z.name;
        mk.pos      = wpos;
        mk.r        = 1.0f; mk.g = 0.6f; mk.b = 0.1f;
        mk.selected = (i == m_selZone);
        markers.push_back(mk);
    }

    m_viewport.setMarkers(markers);

    // Wireframe 3D delle zone hitbox (in world space, colorate per danno)
    std::vector<mini::HitZone> hzv;
    hzv.reserve(m_hitboxZones.size());
    for (auto& z : m_hitboxZones)
    {
        mini::HitZone hz;
        hz.name             = z.name;
        hz.offset           = glm::vec3(M * glm::vec4(z.offset, 1.0f));
        hz.halfExtents      = z.halfExt * m_scale;
        hz.damageMultiplier = z.damageMult;
        hz.eulerDeg         = z.eulerDeg;
        hz.boneName         = z.boneName;
        hzv.push_back(hz);
    }
    m_viewport.setHitboxes(hzv, m_selZone);

    // Le modalità Ruota/Scala hanno senso solo sulle zone hitbox;
    // gli attach point sono punti puri (solo Sposta).
    const bool zoneTarget = (m_gizmoTarget.rfind("hit:", 0) == 0);
    m_viewport.setGizmoCanRotateScale(zoneTarget, zoneTarget);
}

void EntityEditor::saveSelected()
{
    if (m_sel < 0 || m_sel >= (int)m_entries.size()) return;
    auto& e = m_entries[m_sel];

    // saveJsonRMW (ADR-010): unico canale di scrittura JSON dell'editor.
    if (e.hitboxProfileId.empty())
        e.hitboxProfileId = e.id;

    editor::jsonsave::saveJsonRMW(e.jsonPath, [&](json& j) {
    j.erase("id"); // deprecato: id = nome file (ADR-001)
    j["mesh"]       = e.meshPath;
    j["mesh_rot_x"] = m_rotX;
    j["mesh_rot_y"] = m_rotY;
    j["mesh_scale"] = m_scale;

    json apObj = json::object();
    for (auto& [name, ap] : m_attachPoints)
        apObj[name] = { ap.x, ap.y, ap.z };
    j["attach_points"] = apObj;

    j["name"]           = e.name;
    j["faction"]        = e.faction;
    j["stats"]["hp"]    = e.hp;
    // ADR-022: la classe fornisce loadout+comportamento all'unità che la
    // referenzia. Vuota = valgono i campi propri dell'unità (additivo).
    if (e.classId.empty()) j.erase("class");
    else                   j["class"] = e.classId;
    j["hitbox_profile"] = e.hitboxProfileId;

    // NON si scrivono più: `ai_profile`, `weapons`, `abilities` (li decide la
    // CLASSE) né `stats.move_speed` / `stats.damage_scale` (morti: il primo lo
    // sovrascrive il profilo AI, il secondo non ha consumatori).
    // Questo modulo non li edita più, quindi non deve nemmeno rivendicarli:
    // RMW (ADR-010) preserva i valori già nel file, e un'unità senza classe
    // continua a funzionare esattamente come prima. Riscriverli da uno stato
    // che l'utente non può più vedere sarebbe il modo esatto per perderli.
    j.erase("hitbox_zones"); // legacy inline: deprecato da ADR-006

    // Arma in mano: si salva SOLO la POSA.
    // `wd["id"]` NON si scrive più: `m_weaponId` ora è l'arma RISOLTA (la classe
    // vince), e riscriverla qui inietterebbe l'arma della CLASSE dentro l'entità —
    // cioè ricreerebbe nei dati proprio la confusione che ADR-022 elimina. L'`id`
    // già nel file resta dov'è (RMW, ADR-010) e continua a fare da ultimo fallback.
    {
        json wd = j.contains("weapon_display") && j["weapon_display"].is_object()
                  ? j["weapon_display"] : json::object();
        wd["hand"] = m_weaponHandPoint;   // la MANO è sempre del personaggio
        // scala/rot/offset: si scrivono SOLO in modalità legacy (arma senza posa
        // autorata). Se la posa viene dall'ARMA (KI #49), i valori mostrati sono
        // dell'arma: riscriverli qui li "fotograferebbe" sull'entità, ricreando il
        // dato duplicato che questa migrazione elimina. RMW li lascia com'erano.
        if (!m_weaponPoseFromWeapon)
        {
            wd["scale"]  = m_weaponScale;
            wd["rot"]    = {m_weaponRot.x, m_weaponRot.y, m_weaponRot.z};
            wd["offset"] = {m_weaponOffset.x, m_weaponOffset.y, m_weaponOffset.z};
        }
        j["weapon_display"] = wd;
    }
    return true;
    });

    // ── Hitbox → PROFILO condiviso (ADR-006), via saveJsonRMW ─────────
    // Zone scritte in data/hitboxes/<profileId>.json (schema runtime).
    {
        std::string profPath = getDataDir() + "hitboxes/" + e.hitboxProfileId + ".json";
        editor::jsonsave::saveJsonRMW(profPath, [&](json& pj) {
            pj.erase("profile_id"); // deprecato: id = nome file (ADR-001)
            json zonesArr = json::array();
            for (auto& z : m_hitboxZones)
            {
                json zj;
                zj["name"]              = z.name;
                zj["offset"]            = {z.offset.x, z.offset.y, z.offset.z};
                zj["half_extents"]      = {z.halfExt.x, z.halfExt.y, z.halfExt.z};
                zj["damage_multiplier"] = z.damageMult;
                zj["debug_visible"]     = z.debugVisible;
                zj["bone"]              = z.boneName;
                zj["rotation"]          = {z.eulerDeg.x, z.eulerDeg.y, z.eulerDeg.z};
                zonesArr.push_back(zj);
            }
            pj["zones"] = zonesArr;
            return true;
        });
        std::cout << "[EntityEditor] Profilo hitbox salvato: " << profPath << "\n";
    }

    e.meshRotX    = m_rotX;
    e.meshRotY    = m_rotY;
    e.meshScale   = m_scale;
    e.attachPoints = m_attachPoints;
    e.hitboxZones  = m_hitboxZones;
    // NON `e.dispWeaponId = m_weaponId`: m_weaponId è l'arma RISOLTA (può venire
    // dalla classe), dispWeaponId è il fallback legacy dell'entità. Confonderli
    // avrebbe copiato l'arma della classe dentro l'entità al primo salvataggio.
    e.dispWeaponScale  = m_weaponScale;
    e.dispWeaponRot    = m_weaponRot;
    e.dispWeaponOffset = m_weaponOffset;
    e.dispWeaponHand   = m_weaponHandPoint;
    m_dirty = false;
    std::cout << "[EntityEditor] Salvato: " << e.jsonPath << "\n";
}

// Ripristina uno stato fotografato. Un solo punto, così annullare e ripetere non
// possono divergere fra loro (era la trappola classica dei due percorsi separati).
void EntityEditor::applyUndoState(const UndoState& s)
{
    m_attachPoints    = s.attach;
    m_hitboxZones     = s.zones;
    m_selAttachPoint  = s.selAttach;
    m_selZone         = s.selZone;
    m_rotX            = s.rotX;
    m_scale           = s.scale;
    m_dirty           = true;
    updateMarker();
    syncViewportMarkers();
    updateWeaponTransform();
}

void EntityEditor::tick(float dt)
{
    m_viewport.tick(dt);
    m_clock += dt;

    // ── Annulla / Ripeti (doc 52 F2) ─────────────────────────────────────
    // Prima non esistevano in questo modulo: si spostava una zona hitbox col gizmo e
    // non c'era modo di tornare indietro.
    if (ImGui::GetIO().KeyCtrl && !ImGui::GetIO().WantTextInput)
    {
        const bool wantRedo = ImGui::IsKeyPressed(ImGuiKey_Y, false)
            || (ImGui::IsKeyPressed(ImGuiKey_Z, false) && ImGui::GetIO().KeyShift);
        const bool wantUndo = ImGui::IsKeyPressed(ImGuiKey_Z, false)
            && !ImGui::GetIO().KeyShift;
        if (wantUndo || wantRedo)
        {
            UndoState st = snapshot();
            if (wantRedo ? m_undo.redo(st) : m_undo.undo(st)) applyUndoState(st);
        }
    }

    // La fotografia si prende quando il gesto COMINCIA: il gizmo produce un delta
    // per frame, e fotografare a ogni delta riempirebbe la pila rendendo "annulla"
    // un ritorno indietro di un pixel per volta.
    if (m_viewport.gizmoDragging()) m_undo.push(snapshot(), "gizmo", m_clock);

    // Handle gizmo drag — il delta arriva in world space; i punti sono in
    // model space, quindi va riportato con l'inversa della trasformazione.
    glm::vec3 delta;
    if (m_viewport.popGizmoDelta(delta))
    {
        const glm::vec3 localDelta = deltaToLocal(delta);
        if (!m_gizmoTarget.empty())
        {
            if (m_gizmoTarget.substr(0, 4) == "hit:" && m_selZone >= 0
                && m_selZone < (int)m_hitboxZones.size())
            {
                auto& z = m_hitboxZones[m_selZone];
                z.offset += localDelta;
                m_viewport.setGizmoTarget(toWorld(z.offset), true);
                m_dirty = true;
                syncViewportMarkers();
            }
            else if (m_attachPoints.count(m_gizmoTarget))
            {
                auto& ap = m_attachPoints[m_gizmoTarget];
                ap.x += localDelta.x; ap.y += localDelta.y; ap.z += localDelta.z;
                m_viewport.setGizmoTarget(toWorld({ap.x, ap.y, ap.z}), true);
                m_dirty = true;
                updateMarker();
                syncViewportMarkers();
                updateWeaponTransform();
            }
        }
    }

    // Gizmo Ruota/Scala: solo sulle zone hitbox selezionate.
    const bool zoneTarget = (m_gizmoTarget.rfind("hit:", 0) == 0)
                          && m_selZone >= 0 && m_selZone < (int)m_hitboxZones.size();

    glm::vec3 rotDelta;
    if (m_viewport.popGizmoRotDelta(rotDelta) && zoneTarget)
    {
        auto& z = m_hitboxZones[m_selZone];
        z.eulerDeg += rotDelta;
        for (int i = 0; i < 3; ++i)
        {
            while (z.eulerDeg[i] >  180.0f) z.eulerDeg[i] -= 360.0f;
            while (z.eulerDeg[i] < -180.0f) z.eulerDeg[i] += 360.0f;
        }
        m_dirty = true;
        syncViewportMarkers();
    }

    glm::vec3 scaleDelta;
    if (m_viewport.popGizmoScaleDelta(scaleDelta) && zoneTarget)
    {
        auto& z = m_hitboxZones[m_selZone];
        // Delta in world units → half extents in model space (scala uniforme)
        const float s = (m_scale > 0.0001f) ? m_scale : 1.0f;
        z.halfExt += (scaleDelta * 0.5f) / s;
        z.halfExt = glm::max(z.halfExt, glm::vec3(0.01f));
        m_dirty = true;
        syncViewportMarkers();
    }

    // Handle item click from viewport
    std::string clicked = m_viewport.popClickedItem();
    if (!clicked.empty())
    {
        // Check if it's an attach point
        if (m_attachPoints.count(clicked))
        {
            m_selAttachPoint = clicked;
            m_gizmoTarget    = clicked;
            auto& ap = m_attachPoints[clicked];
            m_viewport.setGizmoTarget(toWorld({ap.x, ap.y, ap.z}), true);
            updateMarker();
            syncViewportMarkers();
        }
        else
        {
            // Check hitbox zones
            bool foundZone = false;
            for (int i = 0; i < (int)m_hitboxZones.size(); ++i)
            {
                if (m_hitboxZones[i].name == clicked)
                {
                    m_selZone     = i;
                    m_gizmoTarget = "hit:" + clicked;
                    m_viewport.setGizmoTarget(toWorld(m_hitboxZones[i].offset), true);
                    syncViewportMarkers();
                    foundZone = true;
                    break;
                }
            }
            // Altrimenti: è un bone? Memorizzalo come bone selezionato.
            if (!foundZone)
            {
                for (const auto& jd : m_joints)
                    if (jd.name == clicked)
                    {
                        m_selBoneName = clicked;
                        m_viewport.setSelectedBone(clicked);
                        break;
                    }
            }
        }
    }
}

void EntityEditor::draw()
{
    // Fotografia anche per le modifiche dai CAMPI, non solo dal gizmo: un valore
    // digitato è una modifica come le altre, e un annullamento che ne copre metà
    // sarebbe peggio di non averlo. La coalescenza per etichetta fa sì che un intero
    // trascinamento su uno slider resti una voce sola.
    if (ImGui::IsAnyItemActive() && !ImGui::GetIO().WantTextInput)
        m_undo.push(snapshot(), "campo", m_clock);

    // Rinomina completata nel frame precedente: ricarica e riseleziona.
    if (!m_pendingSelectId.empty())
    {
        const std::string newId = m_pendingSelectId;
        m_pendingSelectId.clear();
        loadEntries();
        for (int i = 0; i < (int)m_entries.size(); ++i)
            if (m_entries[i].id == newId) { selectEntry(i); break; }
    }

    // ── Colonna sinistra: lista entità (resizable) ────────────────────────
    ImGui::BeginGroup();
    ImGui::Text("Entita':");
    // ResizeX: trascina il bordo destro per allargare la lista
    ImGui::BeginChild("##elist", ImVec2(m_listW, -50),
                      ImGuiChildFlags_Borders | ImGuiChildFlags_ResizeX);
    for (int i = 0; i < (int)m_entries.size(); ++i)
    {
        auto& e = m_entries[i];
        char lbl[128];
        std::snprintf(lbl, sizeof(lbl), "%s %s",
            e.isAlly ? "[A]" : "[N]", e.id.c_str());
        if (ImGui::Selectable(lbl, m_sel == i))
            selectEntry(i);
    }
    ImGui::EndChild();

    // ── Nuova entità (id = nome file, ADR-001) ───────────────────────────
    {
        static char newEId[64] = "";
        static int  newEKind   = 0;   // 0 = Nemico, 1 = Alleato
        ImGui::SetNextItemWidth(m_listW - 70.0f);
        ImGui::InputText("##neweid", newEId, sizeof(newEId));
        ImGui::SameLine();
        ImGui::SetNextItemWidth(62.0f);
        const char* kinds[2] = {"Nemico", "Alleato"};
        ImGui::Combo("##newekind", &newEKind, kinds, 2);
        if (ImGui::Button("+ Nuova entita'", {m_listW, 0}) && newEId[0] != '\0')
        {
            const std::string sub  = (newEKind == 1) ? "allies/" : "enemies/";
            const std::string path = getDataDir() + sub + newEId + ".json";
            if (!fs::exists(path))
            {
                // JSON minimo valido: il resto si autora dall'editor.
                editor::jsonsave::saveJsonRMW(path, [&](json& j) {
                    j["name"]      = newEId;
                    j["faction"]   = (newEKind == 1) ? "republic" : "separatist";
                    j["stats"]["hp"]           = 100.0f;
                    j["stats"]["move_speed"]   = 4.0f;
                    j["stats"]["damage_scale"] = 1.0f;
                    j["weapons"]   = json::array();
                    j["abilities"] = json::array();
                    return true;
                });
                m_pendingSelectId = newEId;
                newEId[0] = '\0';
            }
        }
    }

    // Ricarica ANCHE le liste di riferimento (classi, hitbox) e la registry:
    // altrimenti una classe appena creata/modificata in Moduli → Classi non
    // comparirebbe nel dropdown e l'anteprima dell'arma userebbe la classe vecchia.
    // Prima ricaricava solo le entità → le classi nuove restavano invisibili.
    if (ImGui::Button("Ricarica lista", {m_listW, 0}))
    {
        // Conserva l'unità selezionata per id: loadEntries() azzera m_sel, ma
        // dopo un refresh l'utente si aspetta di restare sulla stessa unità (con
        // l'arma ri-risolta dai dati freschi della classe).
        const std::string keepId = (m_sel >= 0 && m_sel < (int)m_entries.size())
                                    ? m_entries[m_sel].id : std::string();
        loadAvailableIds();
        loadEntries();
        if (!keepId.empty())
            for (int i = 0; i < (int)m_entries.size(); ++i)
                if (m_entries[i].id == keepId) { selectEntry(i); break; }
    }
    ImGui::EndGroup();

    // Separator/drag handle between list and center
    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Button,        {0.2f,0.2f,0.2f,0.6f});
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, {0.3f,0.3f,0.3f,0.8f});
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,  {0.35f,0.35f,0.35f,1.0f});
    ImGui::Button("##vsep1", ImVec2(4, -1));
    if (ImGui::IsItemHovered()) ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
    if (ImGui::IsItemActive())
    {
        float newW = m_listW + ImGui::GetIO().MouseDelta.x;
        m_listW = newW < 100.0f ? 100.0f : (newW > 400.0f ? 400.0f : newW);
    }
    ImGui::PopStyleColor(3);
    ImGui::SameLine();

    if (m_sel < 0 || m_sel >= (int)m_entries.size())
    {
        ImGui::TextDisabled("Seleziona un'entita' dalla lista.");
        return;
    }

    auto& e = m_entries[m_sel];

    // ── Colonna centrale: tab Visuale / Statistiche / Hitbox ────────────
    // ResizeX: trascina il bordo destro per allargare il pannello
    ImGui::BeginChild("##ecenter", ImVec2(m_centerW, 0),
                      ImGuiChildFlags_Borders | ImGuiChildFlags_ResizeX);
    if (ImGui::BeginTabBar("##etabs"))
    {
        if (ImGui::BeginTabItem("Visuale"))
        {
            ImGui::Text("%s", e.id.c_str());
            ImGui::SameLine();
            ImGui::TextDisabled("%s", e.isAlly ? "(Alleato)" : "(Nemico)");

            // ── Rinomina (ADR-010): file fisico + sweep cross-reference ──
            {
                static char renameBuf[64] = "";
                static std::string renameErr;
                ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 76.f);
                ImGui::InputText("##erename", renameBuf, sizeof(renameBuf));
                ImGui::SameLine();
                if (ImGui::Button("Rinomina") && renameBuf[0] != '\0')
                {
                    int refs = 0;
                    renameErr = editor::rename::renameDefinition(
                        getDataDir(),
                        e.isAlly ? editor::rename::Category::Ally
                                 : editor::rename::Category::Enemy,
                        e.id, renameBuf, &refs);
                    if (renameErr.empty())
                    {
                        // Reload deferito al prossimo frame (stack ImGui intatto)
                        m_pendingSelectId = renameBuf;
                        renameBuf[0] = '\0';
                    }
                }
                // ── Elimina (doc 39 R1) ──────────────────────────────────
                // Un'entità-corpo è il riferimento più citato di tutti (roster
                // di mappa, `base_entity` delle classi): il popup lo dice, e
                // `--validate` trova cosa resta rotto. Comando condiviso.
                ImGui::SameLine();
                if (ImGui::Button("Elimina")) ImGui::OpenPopup("##edel");
                if (ImGui::BeginPopup("##edel"))
                {
                    ImGui::Text("Eliminare '%s'?", e.id.c_str());
                    ImGui::TextDisabled("Il file viene cancellato. Roster di mappa e classi che");
                    ImGui::TextDisabled("la usano come base_entity resteranno rotti (--validate).");
                    if (ImGui::Button("Elimina", {110, 0}))
                    {
                        renameErr = editor::rename::deleteDefinition(
                            getDataDir(),
                            e.isAlly ? editor::rename::Category::Ally
                                     : editor::rename::Category::Enemy,
                            e.id);
                        ImGui::CloseCurrentPopup();
                        // Reload deferito come per la rinomina: lo stack ImGui
                        // deve restare intatto fino a fine frame.
                        if (renameErr.empty()) m_pendingSelectId.clear();
                    }
                    ImGui::SameLine();
                    if (ImGui::Button("Annulla", {110, 0})) ImGui::CloseCurrentPopup();
                    ImGui::EndPopup();
                }
                if (!renameErr.empty())
                    ImGui::TextColored({1.f,0.4f,0.4f,1.f}, "%s", renameErr.c_str());
            }

            // Mesh path con browse
            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 70);
            ImGui::TextDisabled("%s", e.meshPath.empty() ? "(nessuna mesh)" : e.meshPath.c_str());
            ImGui::SameLine();
            if (ImGui::Button("Sfoglia##mesh"))
            {
                std::string newPath;
                if (browseForEntityMesh(newPath))
                {
                    e.meshPath = newPath;
                    m_dirty = true;
                    reloadPreview();
                    loadRigJoints();
                    loadBones();
                }
            }
            ImGui::Separator();

            bool changed = false;

            ImGui::TextDisabled("Trasformazione modello");

            auto floatRow = [&](const char* label, float& val,
                                float vmin, float vmax, float speed, const char* fmt) -> bool
            {
                bool c = false;
                ImGui::PushID(label);
                ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 68);
                if (ImGui::SliderFloat("##sl", &val, vmin, vmax, fmt)) c = true;
                ImGui::SameLine();
                ImGui::SetNextItemWidth(60);
                if (ImGui::DragFloat("##dg", &val, speed, vmin, vmax, fmt)) c = true;
                ImGui::SameLine(0,4); ImGui::TextDisabled("%s", label);
                ImGui::PopID();
                return c;
            };

            if (floatRow("RotX", m_rotX, -180.0f, 180.0f, 1.0f, "%.0f"))
            { changed = true; reloadPreview(); loadBones(); updateWeaponTransform(); }
            if (floatRow("RotY", m_rotY, -180.0f, 180.0f, 1.0f, "%.0f"))
            { changed = true; }
            if (floatRow("Scala", m_scale, 0.01f, 5.0f, 0.01f, "%.3f"))
            { changed = true; reloadPreview(); loadBones(); updateWeaponTransform(); }

            ImGui::Spacing();
            ImGui::Separator();

            // Posizione model-space di un osso dato il nome.
            auto boneModelPos = [&](const std::string& n, glm::vec3& out) -> bool {
                for (const auto& jd : m_joints)
                    if (jd.name == n) { out = jd.modelPos; return true; }
                return false;
            };

            // ── Ossa del rig ────────────────────────────────────────────
            if (!m_rigJoints.empty())
            {
                ImGui::TextDisabled("Ossa del rig (%d)", (int)m_rigJoints.size());
                ImGui::TextWrapped("Clicca un osso: crea un attach point NELLA posizione "
                                   "dell'osso. Usa la tendina sotto per agganciare un "
                                   "punto standard (es. right_hand) a un osso.");
                float childH = (int)m_rigJoints.size() > 8 ? 130.0f : 0.0f;
                if (ImGui::BeginChild("##rig_joints", ImVec2(0, childH), true))
                {
                    for (const auto& joint : m_rigJoints)
                    {
                        bool alreadyUsed = m_attachPoints.count(joint) > 0;
                        if (alreadyUsed)
                            ImGui::TextDisabled("  %s", joint.c_str());
                        else if (ImGui::SmallButton(("+ " + joint).c_str()))
                        {
                            glm::vec3 bp{0,0,0};
                            boneModelPos(joint, bp);   // posizione reale dell'osso
                            m_attachPoints[joint] = AttachPointEntry{bp.x, bp.y, bp.z};
                            m_selAttachPoint = joint;
                            m_gizmoTarget = joint;
                            m_viewport.setGizmoTarget(toWorld(bp), true);
                            updateMarker();
                            syncViewportMarkers();
                            m_dirty = true;
                        }
                    }
                }
                ImGui::EndChild();
                ImGui::Spacing();
            }

            // ── Attach Points ──────────────────────────────────────────
            ImGui::TextDisabled("Attach Points");
            ImGui::TextWrapped("Punti nominati nel model space.");
            ImGui::Spacing();

            auto apNames = std::vector<std::string>();
            for (auto& [n, _] : m_attachPoints) apNames.push_back(n);
            for (auto* s : k_standardPoints)
                if (!m_attachPoints.count(s)) apNames.push_back(s);
            std::sort(apNames.begin(), apNames.end());

            for (auto& apName : apNames)
            {
                bool exists = m_attachPoints.count(apName) > 0;
                AttachPointEntry& ap = m_attachPoints[apName];

                bool enabled = exists;
                ImGui::PushID(apName.c_str());
                if (ImGui::Checkbox("##en", &enabled))
                {
                    if (!enabled) m_attachPoints.erase(apName);
                    changed = true;
                    updateMarker();
                    syncViewportMarkers();
                }
                ImGui::SameLine();

                bool isSelected = (m_selAttachPoint == apName);
                if (ImGui::Selectable(apName.c_str(), isSelected,
                                      ImGuiSelectableFlags_None, {0, 0}))
                {
                    m_selAttachPoint = apName;
                    if (exists) {
                        m_gizmoTarget = apName;
                        m_viewport.setGizmoTarget(toWorld({ap.x, ap.y, ap.z}), true);
                    }
                    updateMarker();
                    syncViewportMarkers();
                }

                if (ImGui::IsItemHovered())
                {
                    for (int di = 0; di < (int)(sizeof(k_standardPoints)/sizeof(k_standardPoints[0])); ++di)
                        if (apName == k_standardPoints[di])
                        { ImGui::SetTooltip("%s", k_pointDesc[di]); break; }
                }

                if (enabled && isSelected)
                {
                    ImGui::Indent(12.0f);

                    // Aggancia il punto a un osso del modello.
                    if (!m_joints.empty())
                    {
                        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 4);
                        if (ImGui::BeginCombo("##apbone", "Aggancia a un osso..."))
                        {
                            for (auto& jd : m_joints)
                                if (ImGui::Selectable(jd.name.c_str()))
                                {
                                    ap.x = jd.modelPos.x;
                                    ap.y = jd.modelPos.y;
                                    ap.z = jd.modelPos.z;
                                    m_gizmoTarget = apName;
                                    m_viewport.setGizmoTarget(toWorld({ap.x,ap.y,ap.z}), true);
                                    changed = true; updateMarker();
                                    syncViewportMarkers(); updateWeaponTransform();
                                }
                            ImGui::EndCombo();
                        }
                    }

                    if (floatRow("X", ap.x, -2.0f, 2.0f, 0.01f, "%.3f")) { changed = true; updateMarker(); syncViewportMarkers(); updateWeaponTransform(); }
                    if (floatRow("Y", ap.y, -3.0f, 3.0f, 0.01f, "%.3f")) { changed = true; updateMarker(); syncViewportMarkers(); updateWeaponTransform(); }
                    if (floatRow("Z", ap.z, -2.0f, 2.0f, 0.01f, "%.3f")) { changed = true; updateMarker(); syncViewportMarkers(); updateWeaponTransform(); }
                    ImGui::Unindent(12.0f);
                }

                ImGui::PopID();
            }

            if (changed) m_dirty = true;

            // ── Arma in mano (posa) ──────────────────────────────────────
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::TextDisabled("Arma in mano (posa)");
            ImGui::TextWrapped("Aggancia un'arma all'attach point della mano. "
                               "Imposta prima il punto (es. 'right_hand') sul personaggio.");

            ImGui::SetNextItemWidth(140);
            if (ImGui::BeginCombo("Mano##wh", m_weaponHandPoint.c_str()))
            {
                for (const char* hp : {"right_hand", "left_hand"})
                    if (ImGui::Selectable(hp, m_weaponHandPoint == hp))
                    { m_weaponHandPoint = hp; updateWeaponTransform(); }
                ImGui::EndCombo();
            }

            // QUALE arma: sola lettura. Prima questo combo scriveva `weapons[0]`,
            // cioè era l'editor del LOADOUT travestito da posa — e con una classe
            // assegnata quel valore viene ignorato dal gioco (ADR-022).
            // Qui si autora la POSA (mano/rotazione/offset/scala): quella sì che è
            // dell'entità, perché dipende da come il modello impugna.
            ImGui::TextDisabled("Arma mostrata");
            ImGui::SameLine();
            if (m_weaponId.empty())
                ImGui::TextColored({1,0.6f,0.2f,1}, "(nessuna)");
            else
                ImGui::TextColored({0.6f,0.85f,1.0f,1}, "%s", m_weaponId.c_str());
            if (!e.classId.empty())
                ImGui::TextDisabled("Dalla CLASSE '%s' — si cambia in Moduli -> Classi.",
                                    e.classId.c_str());
            else if (!m_weaponId.empty())
                ImGui::TextDisabled("Dai campi legacy dell'unita' (nessuna classe assegnata).");
            else
                ImGui::TextDisabled("Assegna una classe (tab Statistiche) per avere un'arma.");

            if (!m_weaponId.empty())
            {
                if (!m_attachPoints.count(m_weaponHandPoint))
                    ImGui::TextColored({1,0.6f,0.2f,1},
                        "Attach point '%s' non impostato sul personaggio.",
                        m_weaponHandPoint.c_str());
                if (m_weaponMeshPath.empty())
                    ImGui::TextColored({1,0.6f,0.2f,1}, "L'arma non ha un mesh assegnato.");

                if (m_weaponPoseFromWeapon)
                {
                    // Posa dall'ARMA (KI #49): sola lettura. Editarla qui
                    // scriverebbe su un campo ignorato — la stessa trappola KI #25.
                    ImGui::TextColored({0.55f,0.8f,0.95f,1.0f},
                        "Posa in mano definita dall'ARMA '%s'.", m_weaponId.c_str());
                    ImGui::TextDisabled("Scala %.3f  Rot (%.0f, %.0f, %.0f)  Off (%.3f, %.3f, %.3f)",
                        m_weaponScale, m_weaponRot.x, m_weaponRot.y, m_weaponRot.z,
                        m_weaponOffset.x, m_weaponOffset.y, m_weaponOffset.z);
                    ImGui::TextDisabled("Si modifica in Moduli -> Weapon Editor (vale per ogni\n"
                                        "unita' che impugna quest'arma). Qui resta solo la MANO.");
                }
                else
                {
                    // Arma legacy senza posa autorata: si edita ancora qui, ma è
                    // la POSA GIUSTA solo per quest'arma. Il posto definitivo è
                    // il Weapon Editor (hand_scale/hand_rot/hand_offset).
                    ImGui::TextColored({1.0f,0.75f,0.35f,1.0f},
                        "Arma '%s' senza posa in mano: valori LEGACY dell'unita'.\n"
                        "Autora hand_scale nel Weapon Editor per renderla dell'arma.",
                        m_weaponId.c_str());
                    bool wc = false;
                    if (floatRow("Scala arma", m_weaponScale, 0.02f, 5.0f, 0.01f, "%.3f")) wc = true;
                    ImGui::TextDisabled("Rotazione arma (gradi)");
                    if (floatRow("RotX##w", m_weaponRot.x, -180.0f, 180.0f, 1.0f, "%.0f")) wc = true;
                    if (floatRow("RotY##w", m_weaponRot.y, -180.0f, 180.0f, 1.0f, "%.0f")) wc = true;
                    if (floatRow("RotZ##w", m_weaponRot.z, -180.0f, 180.0f, 1.0f, "%.0f")) wc = true;
                    ImGui::TextDisabled("Offset fine (rispetto alla mano)");
                    if (floatRow("OffX##w", m_weaponOffset.x, -1.0f, 1.0f, 0.005f, "%.3f")) wc = true;
                    if (floatRow("OffY##w", m_weaponOffset.y, -1.0f, 1.0f, 0.005f, "%.3f")) wc = true;
                    if (floatRow("OffZ##w", m_weaponOffset.z, -1.0f, 1.0f, 0.005f, "%.3f")) wc = true;
                    if (wc) { updateWeaponTransform(); m_dirty = true; }
                }
            }

            ImGui::Spacing();
            ImGui::Separator();

            if (m_dirty)
                ImGui::TextColored({1.0f, 0.7f, 0.2f, 1.0f}, "* Modifiche non salvate");
            else
                ImGui::TextDisabled("Salvato");

            const float bw = ImGui::GetContentRegionAvail().x / 2.0f - 2.0f;
            if (ImGui::Button("Salva JSON", {bw, 0}))    saveSelected();
            ImGui::SameLine();
            if (ImGui::Button("Ripristina", {bw, 0}))    selectEntry(m_sel);

            ImGui::Spacing();
            if (!m_selAttachPoint.empty() && m_attachPoints.count(m_selAttachPoint))
            {
                ImGui::TextColored({0.2f,1.0f,0.45f,1.0f}, "Ring verde = \"%s\"",
                    m_selAttachPoint.c_str());
            }

            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Statistiche"))
        {
            drawStatsPanel();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Hitbox"))
        {
            drawHitboxTab();
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
    ImGui::EndChild();

    // Separator/drag handle between center and viewport
    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Button,        {0.2f,0.2f,0.2f,0.6f});
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, {0.3f,0.3f,0.3f,0.8f});
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,  {0.35f,0.35f,0.35f,1.0f});
    ImGui::Button("##vsep2", ImVec2(4, -1));
    if (ImGui::IsItemHovered()) ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
    if (ImGui::IsItemActive())
    {
        float newW = m_centerW + ImGui::GetIO().MouseDelta.x;
        m_centerW = newW < 150.0f ? 150.0f : (newW > 600.0f ? 600.0f : newW);
    }
    ImGui::PopStyleColor(3);
    ImGui::SameLine();

    // ── Colonna destra: viewport 3D ─────────────────────────────────────
    ImGui::BeginChild("##eview", ImVec2(0, 0), false,
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    m_viewport.draw();
    ImGui::EndChild();
}

// ── drawHitboxTab ─────────────────────────────────────────────────────────────
void EntityEditor::drawHitboxTab()
{
    ImGui::TextDisabled("Zone hitbox (salvate nel profilo condiviso — ADR-006)");
    editor::ui::gizmoModeBar(m_viewport,
        m_gizmoTarget.rfind("hit:", 0) == 0,
        m_gizmoTarget.rfind("hit:", 0) == 0);
    ImGui::Separator();

    // Helper: posizione model-space di un bone dato il nome (vec3 + trovato).
    auto boneModelPos = [&](const std::string& name, glm::vec3& out) -> bool {
        for (const auto& jd : m_joints)
            if (jd.name == name) { out = jd.modelPos; return true; }
        return false;
    };

    // ── Crea hitbox da bone selezionato nella viewport ───────────────────────
    if (!m_joints.empty())
    {
        ImGui::TextDisabled("Bone selezionato:");
        ImGui::SameLine();
        if (m_selBoneName.empty())
            ImGui::TextDisabled("(clicca un osso nella viewport)");
        else
            ImGui::TextColored({1.0f, 0.9f, 0.2f, 1.0f}, "%s", m_selBoneName.c_str());

        ImGui::BeginDisabled(m_selBoneName.empty());
        if (ImGui::Button("+ Crea hitbox sul bone", {ImGui::GetContentRegionAvail().x, 0}))
        {
            glm::vec3 bp{0,0,0};
            boneModelPos(m_selBoneName, bp);
            EntityEntry::InlineHitZone z;
            z.name     = m_selBoneName;
            z.boneName = m_selBoneName;
            z.offset   = bp;
            m_hitboxZones.push_back(z);
            m_selZone = (int)m_hitboxZones.size() - 1;
            m_gizmoTarget = "hit:" + z.name;
            m_viewport.setGizmoTarget(toWorld(z.offset), true);
            m_dirty = true;
            syncViewportMarkers();
        }
        ImGui::EndDisabled();
        ImGui::Separator();
    }

    // Layout VERTICALE: la colonna centrale è stretta — lista zone in alto,
    // proprietà a piena larghezza sotto. (Il vecchio layout affiancato
    // schiacciava le proprietà a ~0px: "Danno x" e il resto erano invisibili.)
    ImGui::TextDisabled("Zone (%d)", (int)m_hitboxZones.size());
    ImGui::BeginChild("##hzlist", ImVec2(0, 120), true);
    for (int i = 0; i < (int)m_hitboxZones.size(); ++i)
    {
        auto& z = m_hitboxZones[i];
        bool sel = (i == m_selZone);
        char lbl[80];
        std::snprintf(lbl, sizeof(lbl), "%s%s  x%.1f",
                      z.boneName.empty() ? "" : "[B] ", z.name.c_str(), z.damageMult);
        if (ImGui::Selectable(lbl, sel))
        {
            m_selZone = i;
            m_gizmoTarget = "hit:" + z.name;
            m_viewport.setGizmoTarget(toWorld(z.offset), true);
            syncViewportMarkers();
        }
    }
    ImGui::EndChild();

    {
        const float bw = (ImGui::GetContentRegionAvail().x - 4.0f) * 0.5f;
        if (ImGui::Button("+ Aggiungi", {bw, 0}))
        {
            EntityEntry::InlineHitZone z;
            z.name = "zona_" + std::to_string((int)m_hitboxZones.size());
            m_hitboxZones.push_back(z);
            m_selZone = (int)m_hitboxZones.size() - 1;
            m_dirty = true;
            syncViewportMarkers();
        }
        ImGui::SameLine();
        ImGui::BeginDisabled(m_selZone < 0 || m_selZone >= (int)m_hitboxZones.size());
        if (ImGui::Button("- Rimuovi", {bw, 0}))
        {
            m_hitboxZones.erase(m_hitboxZones.begin() + m_selZone);
            m_selZone = (m_selZone > 0) ? m_selZone - 1 : ((int)m_hitboxZones.size() > 0 ? 0 : -1);
            m_dirty = true;
            syncViewportMarkers();
        }
        ImGui::EndDisabled();
    }
    ImGui::Separator();

    // Proprietà zona: piena larghezza, sotto la lista.
    // -64 riserva lo spazio per la barra Salva/Ripristina in fondo.
    ImGui::BeginChild("##hzprops", ImVec2(0, -64), false);
    if (m_selZone >= 0 && m_selZone < (int)m_hitboxZones.size())
    {
        auto& z = m_hitboxZones[m_selZone];
        bool changed = false;

        // Name
        char nameBuf[64]; std::strncpy(nameBuf, z.name.c_str(), 63); nameBuf[63] = '\0';
        if (editor::ui::textRow("Nome##hzn", nameBuf, 64))
        { z.name = nameBuf; changed = true; }

        // Bone attachment
        ImGui::TextDisabled("Bone attachment");
        const char* bonePreview = z.boneName.empty() ? "-- nessuno --" : z.boneName.c_str();
        if (ImGui::BeginCombo("##hzbone", bonePreview))
        {
            if (ImGui::Selectable("-- nessuno --", z.boneName.empty()))
            { z.boneName.clear(); changed = true; }
            for (auto& jd : m_joints)
                if (ImGui::Selectable(jd.name.c_str(), z.boneName == jd.name))
                {
                    z.boneName = jd.name;
                    z.offset   = jd.modelPos; // aggancia la zona alla posizione del bone
                    changed = true;
                    m_viewport.setGizmoTarget(toWorld(z.offset), true);
                }
            ImGui::EndCombo();
        }
        if (!z.boneName.empty())
        {
            ImGui::SameLine();
            if (ImGui::SmallButton("Riallinea"))
            {
                glm::vec3 bp{0,0,0};
                if (boneModelPos(z.boneName, bp))
                { z.offset = bp; changed = true; m_viewport.setGizmoTarget(toWorld(z.offset), true); }
            }
        }

        // Offset
        ImGui::TextDisabled("Posizione (model space)");
        float off[3] = {z.offset.x, z.offset.y, z.offset.z};
        if (editor::ui::sliderRow3("hzo", off, -3.0f, 3.0f, 0.01f, "%.3f"))
        { z.offset = {off[0], off[1], off[2]}; changed = true;
          m_viewport.setGizmoTarget(toWorld(z.offset), true); }

        // Half extents
        ImGui::TextDisabled("Dimensioni (half extents)");
        float he[3] = {z.halfExt.x, z.halfExt.y, z.halfExt.z};
        if (editor::ui::sliderRow3("hzhe", he, 0.01f, 2.0f, 0.005f, "%.3f"))
        { z.halfExt = {he[0], he[1], he[2]}; changed = true; }

        // Rotation
        ImGui::TextDisabled("Rotazione (gradi)");
        float euler[3] = {z.eulerDeg.x, z.eulerDeg.y, z.eulerDeg.z};
        if (editor::ui::sliderRow3("hzrot", euler, -180.0f, 180.0f, 1.0f, "%.1f"))
        { z.eulerDeg = {euler[0], euler[1], euler[2]}; changed = true; }

        // Damage multiplier
        if (editor::ui::sliderRow("Danno x", z.damageMult, 0.1f, 5.0f, 0.05f, "%.2f"))
            changed = true;

        if (ImGui::Checkbox("Debug visibile##hzdbg", &z.debugVisible))
            changed = true;

        if (changed) { m_dirty = true; syncViewportMarkers(); }
    }
    else
    {
        ImGui::TextDisabled("Seleziona o crea una zona.");
    }
    ImGui::EndChild();

    ImGui::Separator();
    if (m_dirty)
        ImGui::TextColored({1.0f, 0.7f, 0.2f, 1.0f}, "* Modifiche non salvate");
    else
        ImGui::TextDisabled("Salvato");

    float bw = (ImGui::GetContentRegionAvail().x - 4.0f) * 0.5f;
    if (ImGui::Button("Salva JSON##hzsave", {bw, 0})) saveSelected();
    ImGui::SameLine();
    if (ImGui::Button("Ripristina##hzrest", {bw, 0})) selectEntry(m_sel);
}

// ── drawStatsPanel ────────────────────────────────────────────────────────────
void EntityEditor::drawStatsPanel()
{
    if (m_sel < 0 || m_sel >= (int)m_entries.size())
    { ImGui::TextDisabled("Seleziona un'entita'."); return; }

    auto& e = m_entries[m_sel];

    ImGui::TextColored({0.9f,0.7f,0.2f,1.0f}, "%s", e.id.c_str());
    ImGui::Separator();

    // ── La CLASSE per prima: decide se il resto del pannello conta ────
    // ADR-022: l'entità è il CORPO (mesh, hitbox), la classe è la PROFESSIONE
    // (loadout, comportamento, abilità). Con una classe assegnata, arma/profilo
    // AI/abilità qui sotto vengono IGNORATI dal runtime — e un campo che si può
    // editare senza effetto è la classe di problema di KI #25.
    // Sta in cima e non in mezzo perché è la scelta che condiziona le altre.
    ImGui::TextDisabled("CLASSE (professione: loadout + comportamento + abilita')");
    ImGui::SetNextItemWidth(280.0f);
    if (ImGui::BeginCombo("##cls", e.classId.empty() ? "-- nessuna --" : e.classId.c_str()))
    {
        if (ImGui::Selectable("-- nessuna --", e.classId.empty()))
        { e.classId.clear(); m_dirty = true; }
        for (const auto& id : m_availableClasses)
            if (ImGui::Selectable(id.c_str(), e.classId == id))
            { e.classId = id; m_dirty = true; }
        ImGui::EndCombo();
    }
    if (!e.classId.empty())
        ImGui::TextDisabled("Arma, profilo AI e abilita' si editano in Moduli -> Classi.");
    else
        ImGui::TextColored({1.0f, 0.55f, 0.35f, 1.0f},
            "NESSUNA CLASSE: quest'unita' usa i campi legacy del suo JSON\n"
            "(weapons / ai_profile / abilities), che NON sono piu' editabili qui.\n"
            "Assegna una classe per poterli cambiare.");
    ImGui::Separator();

    char buf[256];
    std::strncpy(buf, e.name.c_str(), 255);
    if (editor::ui::textRow("Nome", buf, 255)) { e.name = buf; m_dirty = true; }

    const char* factions[] = {"neutral","republic","separatist"};
    int fi = 0;
    for (int i = 0; i < 3; ++i) if (e.faction == factions[i]) fi = i;
    if (editor::ui::comboRow("Fazione", fi, factions, 3)) { e.faction = factions[fi]; m_dirty = true; }

    ImGui::Separator();
    ImGui::TextDisabled("Corpo");

    auto drag = [&](const char* label, float& v, float lo, float hi, const char* fmt = "%.2f") {
        if (ImGui::DragFloat(label, &v, (hi-lo)*0.005f, lo, hi, fmt)) m_dirty = true;
    };
    // Solo HP: è l'unica stat dell'unità che il runtime consuma davvero
    // (`out.hp = enemy->hp`). Qui c'erano anche:
    //  - "Velocita'"    → morta: `resolveUnitArchetype` la sovrascrive sempre con
    //                     `ai->patrolSpeed` del profilo. Si edita nel PROFILO AI.
    //  - "Danno Scale"  → morta: ZERO consumatori nel runtime (classe KI #25).
    // Un campo editabile che non fa nulla non è un dettaglio estetico: insegna a
    // fidarsi di numeri che non contano.
    drag("HP", e.hp, 1.f, 2000.f, "%.0f");

    ImGui::TextDisabled("Hitbox Profile   (il CORPO: resta sempre dell'entita')");
    if (ImGui::BeginCombo("##hbx", e.hitboxProfileId.empty() ? "-- nessuno --" : e.hitboxProfileId.c_str()))
    {
        if (ImGui::Selectable("-- nessuno --", e.hitboxProfileId.empty())) { e.hitboxProfileId.clear(); m_dirty = true; }
        for (auto& id : m_availableHitboxes)
            if (ImGui::Selectable(id.c_str(), e.hitboxProfileId == id))
            {
                e.hitboxProfileId = id;
                // CRITICO: ricarica le zone dal profilo appena selezionato.
                // Senza questo, il salvataggio scriveva le zone del profilo
                // PRECEDENTE (anche vuote) sul nuovo → clobber di profili
                // condivisi (incidente 2026-07-09: B1 svuotato via Heavy).
                loadZonesFromProfile(id);
                m_selZone = -1;
                syncViewportMarkers();
                m_dirty = true;
            }
        ImGui::EndCombo();
    }

    // Qui c'erano "Armi" e "Abilita'". Rimossi (ADR-022): li decide la CLASSE.
    // "Armi" era anche una lista bugiarda — il runtime legge solo `weapons[0]`
    // (`EnemyDef::primaryWeaponId()`), quindi il pulsante "+ Arma" prometteva un
    // arsenale che il gioco ignorava.
    // I valori esistenti NON vengono toccati: `saveSelected` non scrive più questi
    // campi e RMW (ADR-010) li preserva. Un'unità senza classe continua a usarli.

    ImGui::Separator();
    if (m_dirty) ImGui::TextColored({1.f,0.7f,0.2f,1.f}, "* Modifiche non salvate");
    else         ImGui::TextDisabled("Salvato");

    float bw = (ImGui::GetContentRegionAvail().x - 4.f) * 0.5f;
    if (ImGui::Button("Salva JSON##s", {bw, 0})) saveSelected();
    ImGui::SameLine();
    if (ImGui::Button("Ripristina##r", {bw, 0})) selectEntry(m_sel);
}

} // namespace editor
