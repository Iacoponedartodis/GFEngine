// ContentValidation.cpp — 24_ContentValidation / ADR-018.
// I gate qui dentro NON sono teorici: ognuno corrisponde a un problema che il
// progetto ha già pagato (KI #7 near-duplicate, KI #24/#26 id e fallback morti,
// incidente hitbox 2026-07-09, ADR-007 id di fallback hardcoded).
#include "mini/game/data/ContentValidation.hpp"
#include "mini/game/data/DefinitionRegistry.hpp"
#include "mini/core/Telemetry.hpp"

#include <nlohmann/json.hpp>
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <iostream>

namespace mini
{
namespace fs = std::filesystem;
namespace
{

using L = telemetry::Level;

void add(Diagnostics& d, L sev, const char* cat, std::string file,
         std::string msg, std::string suggestion)
{
    d.push_back({sev, cat, std::move(file), std::move(msg), std::move(suggestion)});
}

// Riferimento a un'altra definizione: il caso più frequente di rottura silenziosa.
// Un id che non risolve non è un dettaglio cosmetico — è un'unità senza arma, un
// nemico senza hitbox, una mappa che spawna il nulla.
template <class MapT>
void checkRef(Diagnostics& d, const MapT& table, const std::string& id,
              const std::string& owner, const char* what, L sev = L::Error)
{
    if (id.empty() || table.count(id)) return;
    add(d, sev, "Content", owner,
        std::string(what) + " '" + id + "' non esiste nel registry",
        std::string("Correggi il riferimento in '") + owner
        + "' scegliendolo dal dropdown dell'editor (mai id a testo libero), "
          "oppure crea data/.../" + id + ".json");
}

// Normalizza un nome visualizzato per il confronto near-duplicate: minuscolo,
// solo alfanumerici. "DC-15A Blaster" e "dc15a  blaster" collassano sullo stesso.
std::string normName(const std::string& s)
{
    std::string o;
    for (unsigned char c : s)
        if (std::isalnum(c)) o.push_back((char)std::tolower(c));
    return o;
}

// KI #7 (P0): un rename senza tooling lascia il file vecchio su disco, e in
// partita compaiono due righe quasi identiche ("DC-15A Blaster" / "DC-15A Blaster
// Rifle"). Il sintomo osservato era sui NOMI VISUALIZZATI, non sugli id: gli id
// sono per forza diversi (= filename), è il nome che tradisce il duplicato.
template <class MapT>
void checkNearDuplicates(Diagnostics& d, const MapT& table, const char* kind)
{
    std::vector<std::pair<std::string, std::string>> items;   // (normName, id)
    items.reserve(table.size());
    for (const auto& [id, def] : table)
        items.emplace_back(normName(def.name), id);

    for (size_t i = 0; i < items.size(); ++i)
        for (size_t j = i + 1; j < items.size(); ++j)
        {
            const std::string& a = items[i].first;
            const std::string& b = items[j].first;
            if (a.empty() || b.empty()) continue;
            const bool dup = (a == b);
            // Prefisso: "dc15ablaster" ⊂ "dc15ablasterrifle" — il caso reale di KI #7.
            const bool pre = !dup && (a.size() > 4 && b.size() > 4)
                           && (b.rfind(a, 0) == 0 || a.rfind(b, 0) == 0);
            if (!dup && !pre) continue;
            add(d, L::Warn, "Content", std::string(kind) + ": "
                + items[i].second + " / " + items[j].second,
                std::string(dup ? "nome visualizzato IDENTICO"
                                : "nome visualizzato quasi identico")
                + " fra '" + items[i].second + "' e '" + items[j].second + "'",
                "Probabile near-duplicate da rename manuale (KI #7): verifica quale "
                "file e' canonico, usa il comando Rinomina dell'editor (ADR-010) e "
                "cancella l'abbandonato. Se sono davvero due contenuti diversi, "
                "dai loro nomi distinguibili.");
        }
}

// Asset mancante = "modello invisibile": il gioco parte, l'unità non si vede.
void checkAsset(Diagnostics& d, const std::string& root, const std::string& path,
                const std::string& owner, const char* what)
{
    if (root.empty() || path.empty()) return;
    if (fs::exists(fs::path(root) / path) || fs::exists(fs::path(path))) return;
    add(d, L::Error, "Asset", owner,
        std::string(what) + " '" + path + "' non trovato su disco",
        "Correggi il path o aggiungi il file. Un asset mancante non fa fallire il "
        "load: produce un'entita' invisibile in partita, e il sintomo appare "
        "lontano dalla causa.");
}

} // namespace

Diagnostics validateMission(const MissionDef& m, const DefinitionRegistry& reg)
{
    Diagnostics d;

    // Una missione che non sa dire quando e' vinta o persa non e' una missione.
    // Entrambe obbligatorie (doc 25). Nota: il flag e' false anche quando la
    // regola c'era ma con una stringa sconosciuta — un typo non deve diventare
    // un default silenzioso.
    if (!m.hasSuccessRule)
        add(d, L::Error, "Mission", "missions/" + m.id + ".json",
            "manca 'success_rules' (o la regola non e' riconosciuta)",
            "Aggiungi success_rules.rule = all_primary_complete | any_primary_complete");
    if (!m.hasFailureRule)
        add(d, L::Error, "Mission", "missions/" + m.id + ".json",
            "manca 'failure_rules' (o la regola non e' riconosciuta)",
            "Aggiungi failure_rules.rule = any_primary_failed | time_limit "
            "(con time_limit > 0)");
    if (m.hasFailureRule && m.failureRule == MissionRule::TimeLimit
        && m.failureTimeLimit <= 0.0f)
        add(d, L::Error, "Mission", "missions/" + m.id + ".json",
            "failure_rules e' 'time_limit' ma time_limit <= 0 → non scadra' mai",
            "Imposta failure_rules.time_limit a un valore > 0 (secondi).");

    if (m.primaryObjectives.empty())
        add(d, L::Error, "Mission", "missions/" + m.id + ".json",
            "nessun obiettivo primario",
            "Una missione senza primari non puo' essere vinta: aggiungi almeno un "
            "id in primary_objectives.");

    if (!m.mapId.empty() && !reg.getMap(m.mapId))
        add(d, L::Error, "Mission", "missions/" + m.id + ".json",
            "mappa '" + m.mapId + "' non esiste",
            "Scegli una mappa esistente da data/maps/ (dropdown, mai testo libero).");

    auto checkObj = [&](const std::string& id, bool mustBePrimary)
    {
        const ObjectiveDef* o = reg.getObjective(id);
        if (!o)
        {
            add(d, L::Error, "Mission", "missions/" + m.id + ".json",
                "obiettivo '" + id + "' non esiste",
                "Crea data/objectives/" + id + ".json oppure correggi il riferimento.");
            return;
        }
        // Il tier lo dichiara l'ObjectiveDef; la lista nel MissionDef e' solo un
        // raggruppamento. Se i due si contraddicono il dato e' ambiguo: rifiuta.
        if (mustBePrimary && o->tier != ObjectiveTier::Primary)
            add(d, L::Error, "Mission", "objectives/" + id + ".json",
                "elencato fra i primari di '" + m.id + "' ma ha tier != primary",
                "Allinea i due: metti tier='primary' nell'obiettivo, oppure spostalo "
                "in optional_objectives.");
        // Dipendenza non risolvibile = obiettivo che non si attivera' MAI.
        if (o->activation == ActivationType::AfterObjective)
        {
            if (o->activationObjective.empty() || !reg.getObjective(o->activationObjective))
                add(d, L::Error, "Mission", "objectives/" + id + ".json",
                    "activation 'after_objective' ma il prerequisito '"
                    + o->activationObjective + "' non esiste",
                    "Indica un objective id valido in activation.objective: cosi' "
                    "com'e', l'obiettivo non si attivera' mai.");
            else if (o->activationObjective == id)
                add(d, L::Error, "Mission", "objectives/" + id + ".json",
                    "dipende da se stesso",
                    "Rimuovi l'auto-dipendenza: l'obiettivo non si attiverebbe mai.");
        }
        if (o->type == ObjectiveType::EliminateTarget && o->count <= 0)
            add(d, L::Error, "Mission", "objectives/" + id + ".json",
                "eliminate_target con count <= 0 → completato subito",
                "Imposta target.count >= 1.");
        if (o->type == ObjectiveType::HoldAreaForDuration && o->holdSeconds <= 0.0f)
            add(d, L::Error, "Mission", "objectives/" + id + ".json",
                "hold_area_for_duration con hold_seconds <= 0 → completato subito",
                "Imposta target.hold_seconds > 0.");
        if ((o->type == ObjectiveType::ReachArea
             || o->type == ObjectiveType::HoldAreaForDuration) && o->radius <= 0.0f)
            add(d, L::Error, "Mission", "objectives/" + id + ".json",
                "zona con radius <= 0 → irraggiungibile",
                "Imposta target.radius > 0 (metri).");
        // Conseguenze (doc 25): un tipo non riconosciuto resta None e non farebbe
        // NULLA — l'obiettivo sembrerebbe avere un effetto e invece è una casella.
        auto checkConsequences = [&](const std::vector<ConsequenceDef>& list,
                                     const char* when)
        {
            for (const auto& c : list)
            {
                const std::string f = "objectives/" + id + ".json";
                if (c.type == ConsequenceType::None)
                {
                    add(d, L::Error, "Mission", f,
                        std::string("conseguenza '") + when + "' con type sconosciuto",
                        "Tipi validi: block_enemy_reinforcements, enemy_accuracy, "
                        "ally_reinforcements, unlock_spawn.");
                    continue;
                }
                if (c.type == ConsequenceType::EnemyAccuracy
                    && (c.value <= 0.0f || c.value > 1.0f))
                    add(d, L::Error, "Mission", f,
                        "enemy_accuracy con value fuori da (0,1] → non disorganizza",
                        "E' un MOLTIPLICATORE della precisione nemica: 0.5 = meta' "
                        "precisione. 1 = nessun effetto, >1 li renderebbe migliori.");
                if (c.type == ConsequenceType::AllyReinforcements && c.value == 0.0f)
                    add(d, L::Warn, "Mission", f,
                        "ally_reinforcements con value 0 → non aggiunge nulla",
                        "Imposta value al numero di riserve da aggiungere.");
                if (c.type == ConsequenceType::UnlockSpawn)
                {
                    if (c.target.empty())
                        add(d, L::Error, "Mission", f,
                            "unlock_spawn senza 'target'",
                            "Indica la label del command post dove far rinascere la squadra.");
                    else if (const MapDef* md = reg.getMap(m.mapId))
                    {
                        bool ok = false;
                        for (const auto& cp : md->commandPosts)
                            if (cp.label == c.target) { ok = true; break; }
                        if (!ok)
                            add(d, L::Error, "Mission", f,
                                "unlock_spawn punta al post '" + c.target
                                + "' che non esiste nella mappa '" + m.mapId + "'",
                                "Usa la label di un command post di quella mappa.");
                    }
                }
            }
        };
        checkConsequences(o->onSuccess, "on_success");
        checkConsequences(o->onFailure, "on_failure");

        // Command post: il riferimento è una LABEL e va risolto nella mappa DELLA
        // MISSIONE — è l'unico riferimento incrociato che dipende da un'altra
        // definizione scelta altrove, quindi si valida qui e non sull'obiettivo.
        if (o->type == ObjectiveType::CaptureZone || o->type == ObjectiveType::DefendZone)
        {
            if (o->targetPost.empty())
                add(d, L::Error, "Mission", "objectives/" + id + ".json",
                    "capture/defend_zone senza 'post'",
                    "Indica target.post con la label di un command post della mappa.");
            else if (const MapDef* md = reg.getMap(m.mapId))
            {
                int found = 0;
                for (const auto& cp : md->commandPosts)
                    if (cp.label == o->targetPost) ++found;
                if (found == 0)
                    add(d, L::Error, "Mission", "objectives/" + id + ".json",
                        "post '" + o->targetPost + "' non esiste nella mappa '"
                        + m.mapId + "'",
                        "Usa la label di un command post autorato in quella mappa "
                        "(Map Editor), oppure cambia la mappa della missione.");
                else if (found > 1)
                    add(d, L::Error, "Map", "maps/" + m.mapId + ".json",
                        "piu' command post con la label '" + o->targetPost
                        + "' → il riferimento e' ambiguo",
                        "Le label dei post sono il loro unico nome: rendile univoche.");
            }
            if (o->type == ObjectiveType::DefendZone && o->holdSeconds <= 0.0f)
                add(d, L::Error, "Mission", "objectives/" + id + ".json",
                    "defend_zone con hold_seconds <= 0 → completato subito",
                    "Imposta target.hold_seconds > 0 (secondi di tenuta).");
        }
    };
    for (const auto& id : m.primaryObjectives)  checkObj(id, true);
    for (const auto& id : m.optionalObjectives) checkObj(id, false);

    return d;
}

Diagnostics validateContent(const DefinitionRegistry& reg, const std::string& dataRoot)
{
    Diagnostics d;

    // ── Armi: i campi che il runtime CONSUMA devono essere sensati ────────
    for (const auto& [id, w] : reg.weapons())
    {
        const std::string f = "weapons/" + id + ".json";
        if (w.damage <= 0.0f)
            add(d, L::Error, "Content", f, "damage <= 0 → l'arma non fa nulla",
                "Imposta 'damage' > 0.");
        if (w.fireRate <= 0.0f)
            add(d, L::Error, "Content", f, "fire_rate <= 0 → l'arma non sparera' mai",
                "Imposta 'fire_rate' > 0 (colpi al secondo).");
        if (w.bulletSpeed <= 0.0f)
            add(d, L::Error, "Content", f, "bullet_speed <= 0 → il proiettile non parte",
                "Imposta 'bullet_speed' > 0.");
        if (w.effectiveRange <= 0.0f)
            add(d, L::Error, "Content", f, "effective_range <= 0 → l'AI non ingaggera' mai",
                "Imposta 'effective_range' > 0 (metri).");
        if (w.minRange > 0.0f && w.minRange >= w.effectiveRange)
            add(d, L::Warn, "Content", f,
                "min_range >= effective_range → finestra di tiro vuota",
                "Riduci 'min_range' sotto 'effective_range'.");
        checkAsset(d, dataRoot, w.meshPath, f, "mesh arma");
    }
    checkNearDuplicates(d, reg.weapons(), "weapons");

    // ── Unita' (nemici + alleati): i riferimenti incrociati ───────────────
    auto checkUnit = [&](const std::string& id, const EnemyDef& u, const char* kind)
    {
        const std::string f = std::string(kind) + "/" + id + ".json";
        checkRef(d, reg.aiProfiles(),     u.aiProfileId,     f, "profilo AI");
        checkRef(d, reg.hitboxProfiles(), u.hitboxProfileId, f, "profilo hitbox");
        for (const auto& wid : u.weaponIds)
            checkRef(d, reg.weapons(), wid, f, "arma");
        for (const auto& aid : u.abilityIds)
            checkRef(d, reg.abilities(), aid, f, "abilita'");
        if (u.weaponIds.empty())
            add(d, L::Warn, "Content", f, "nessuna arma assegnata",
                "L'unita' entrera' in partita disarmata: assegna almeno un'arma "
                "dal dropdown, o conferma che e' voluto.");
        if (u.hp <= 0.0f)
            add(d, L::Error, "Content", f, "hp <= 0 → l'unita' muore allo spawn",
                "Imposta 'hp' > 0.");
        checkAsset(d, dataRoot, u.meshPath, f, "mesh unita'");
    };
    for (const auto& [id, u] : reg.enemies()) checkUnit(id, u, "enemies");
    for (const auto& [id, u] : reg.allies())  checkUnit(id, u, "allies");
    checkNearDuplicates(d, reg.enemies(), "enemies");
    checkNearDuplicates(d, reg.allies(),  "allies");

    // ── Mappe ─────────────────────────────────────────────────────────────
    for (const auto& [id, m] : reg.maps())
    {
        const std::string f = "maps/" + id + ".json";
        if (m.geometry.empty())
            add(d, L::Error, "Map", f, "geometry vuota → nessun pavimento",
                "Aggiungi almeno un box 'floor'. Senza geometria non esiste navmesh "
                "(ADR-017) e le unita' cadono nel vuoto.");
        for (const auto& t : m.enemyTypes)
            checkRef(d, reg.enemies(), t, f, "archetipo nemico");
        for (const auto& t : m.allyTypes)
            checkRef(d, reg.allies(), t, f, "archetipo alleato");
        for (const auto& cp : m.commandPosts)
            if (cp.radius <= 0.0f)
                add(d, L::Error, "Map", f,
                    "command post '" + cp.label + "' con radius <= 0 → incatturabile",
                    "Imposta 'radius' > 0 (metri).");
            else if (cp.captureTime <= 0.0f)
                add(d, L::Warn, "Map", f,
                    "command post '" + cp.label + "' con capture_time <= 0 → cattura istantanea",
                    "Imposta 'capture_time' > 0 se vuoi che serva presenza.");
        for (const auto& vs : m.vehicleSpawns)
            checkRef(d, reg.vehicles(), vs.vehicleId, f, "veicolo");
    }

    // ── Missioni e obiettivi (ADR-019): STESSE regole del runtime ─────────
    for (const auto& [id, m] : reg.missions())
    {
        Diagnostics md = validateMission(m, reg);
        d.insert(d.end(), md.begin(), md.end());
    }
    // Obiettivo non referenziato da nessuna missione: non e' un errore (puo'
    // essere in lavorazione), ma e' esattamente il modo in cui nascono gli orfani.
    for (const auto& [oid, o] : reg.objectives())
    {
        bool used = false;
        for (const auto& [mid, m] : reg.missions())
        {
            auto in = [&](const std::vector<std::string>& v)
            { return std::find(v.begin(), v.end(), oid) != v.end(); };
            if (in(m.primaryObjectives) || in(m.optionalObjectives)) { used = true; break; }
        }
        if (!used)
            add(d, L::Warn, "Mission", "objectives/" + oid + ".json",
                "obiettivo non referenziato da nessuna missione",
                "Aggiungilo a una missione o cancellalo: i file orfani diventano "
                "near-duplicate al primo rename (KI #7).");
    }

    // ── Personaggi (PlayerDef) ───────────────────────────────────────────
    // Consumati dal runtime dal 2026-07-15 (KI #35): prima erano dati morti e
    // sbagliarli non aveva conseguenze. Ora ne hanno.
    for (const auto& [id, p] : reg.playerDefs())
    {
        const std::string f = "characters/" + id + ".json";
        if (p.hp <= 0.0f)
            add(d, L::Error, "Content", f, "hp <= 0 → il giocatore muore allo spawn",
                "Imposta stats.hp > 0.");
        if (p.moveSpeed <= 0.0f)
            add(d, L::Error, "Content", f, "move_speed <= 0 → il giocatore non si muove",
                "Imposta stats.move_speed > 0 (m/s).");
        if (p.armorRating <= 0.0f)
            add(d, L::Error, "Content", f,
                "armor_rating <= 0 → divisore del danno invalido",
                "Imposta stats.armor_rating > 0 (1 = nessuna riduzione, 2 = meta' danno).");
        if (p.sprintMult < 1.0f)
            add(d, L::Warn, "Content", f, "sprint_mult < 1 → correre RALLENTA",
                "Quasi certamente non voluto: sprint_mult e' un moltiplicatore (1.65 = storico).");
    }
    checkNearDuplicates(d, reg.playerDefs(), "characters");

    // ── Classi (14_ClassSystem) ──────────────────────────────────────────
    for (const auto& [id, c] : reg.classes())
    {
        const std::string f = "classes/" + id + ".json";
        if (c.primaryWeaponId.empty())
            add(d, L::Error, "Content", f,
                "nessuna arma primaria → la classe non equipaggia niente",
                "Assegna 'primary_weapon' scegliendola dal dropdown delle armi.");
        else
            checkRef(d, reg.weapons(), c.primaryWeaponId, f, "arma primaria");
        checkRef(d, reg.weapons(),   c.secondaryWeaponId, f, "arma secondaria");
        for (const auto& aid : c.abilityIds)
            checkRef(d, reg.abilities(), aid, f, "abilita'");
        if (c.primaryWeaponId == c.secondaryWeaponId && !c.primaryWeaponId.empty())
            add(d, L::Warn, "Content", f,
                "arma primaria e secondaria sono la stessa",
                "Probabile errore di authoring: scegli armi diverse o lascia "
                "'secondary_weapon' vuoto.");
    }
    checkNearDuplicates(d, reg.classes(), "classes");

    // ── Campi fantasma: chiavi che NESSUN loader legge ───────────────────
    // È così che un refuso degrada in silenzio: "fire_rat": 4.5 non fallisce —
    // l'arma prende il default e il sintomo appare lontano dalla causa.
    // Warning e non Error: una chiave in più non rompe la partita, e bloccare
    // l'avvio su questo renderebbe l'authoring ostile (policy doc 24).
    // ATTENZIONE al limite: qui si vedono le chiavi IGNORATE dal loader, non i
    // campi che il loader legge ma nessun sistema consuma (min_range, fov_deg,
    // hearing_range — KI #25). Quelli sono codice, non dati: nessun gate che
    // guardi il registry può vederli.
    for (const auto& [file, keys] : reg.unknownKeys())
        for (const auto& k : keys)
        {
            // 'id' merita un messaggio suo: non e' un refuso, e' il campo che
            // ADR-001 ignora di proposito — ed e' esattamente ciò che, quando
            // era autoritativo, registrava le definizioni sotto la chiave
            // sbagliata rompendo le cross-ref in silenzio (KI #21).
            if (k == "id" || k == "profile_id")
                add(d, L::Warn, "Content", file,
                    "contiene il campo '" + k + "', che viene IGNORATO (ADR-001: "
                    "l'id e' il nome del file)",
                    "Rimuovilo: se diverge dal filename fa credere a un id che non "
                    "esiste. Per rinominare, usa il comando Rinomina dell'editor.");
            else
                add(d, L::Warn, "Content", file,
                    "campo '" + k + "' non letto da nessun loader",
                    "Probabile refuso o campo obsoleto: un nome sbagliato non "
                    "fallisce, il valore viene semplicemente ignorato e il runtime "
                    "usa il default. Correggi il nome o rimuovi il campo.");
        }

    // ── Abilita': i parametri che il TIPO consuma ────────────────────────
    for (const auto& [id, a] : reg.abilities())
        if (a.type.empty())
            add(d, L::Error, "Content", "abilities/" + id + ".json",
                "campo 'type' assente → nessun lettore sapra' cosa farne",
                "Imposta 'type' a un tipo di abilita' gestito dal runtime.");

    return d;
}

bool reportDiagnostics(const Diagnostics& diags, bool printToStdout)
{
    for (const auto& x : diags)
    {
        nlohmann::json j;
        j["file"]       = x.file;
        j["suggestion"] = x.suggestion;
        j["diag_category"] = x.category;
        telemetry::event(x.severity, "Content", x.message, j);

        if (!printToStdout) continue;
        const bool err = (x.severity == telemetry::Level::Error);
        (err ? std::cerr : std::cout)
            << (err ? "[ERROR] " : "[WARN ] ") << x.category << "  " << x.file
            << "\n         " << x.message
            << "\n         → " << x.suggestion << "\n";
    }
    return hasErrors(diags);
}

} // namespace mini
