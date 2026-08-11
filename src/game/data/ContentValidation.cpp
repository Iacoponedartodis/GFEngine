// ContentValidation.cpp — 24_ContentValidation / ADR-018.
// I gate qui dentro NON sono teorici: ognuno corrisponde a un problema che il
// progetto ha già pagato (KI #7 near-duplicate, KI #24/#26 id e fallback morti,
// incidente hitbox 2026-07-09, ADR-007 id di fallback hardcoded).
#include "mini/core/GameConfig.hpp"   // STEP_HEIGHT: lo scalino che il navmesh sa salire
#include "mini/game/MapMetrics.hpp"   // metriche normative (doc 47 §4): sorgente unica
#include "mini/game/MapStructures.hpp"  // riferimenti fra composite (ADR-056 rivisto)
#include "mini/game/data/ContentValidation.hpp"
#include "mini/game/data/DefinitionRegistry.hpp"
#include "mini/game/ClassResolve.hpp"   // ADR-022: arma effettiva = quella della classe
#include "mini/core/Telemetry.hpp"

#include <nlohmann/json.hpp>
#include <algorithm>
#include <cstdio>   // snprintf (salute tattica, doc 41 B4)
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

        // DestroyTarget: la 'structure' è una LABEL da risolvere nella mappa
        // della missione, stessa disciplina del post (deve esistere ed essere unica).
        if (o->type == ObjectiveType::DestroyTarget)
        {
            if (o->targetStructure.empty())
                add(d, L::Error, "Mission", "objectives/" + id + ".json",
                    "destroy_target senza 'structure'",
                    "Indica target.structure con la label di un bersaglio strategico della mappa.");
            else if (const MapDef* md = reg.getMap(m.mapId))
            {
                int found = 0;
                for (const auto& st : md->strategicTargets)
                    if (st.label == o->targetStructure) ++found;
                if (found == 0)
                    add(d, L::Error, "Mission", "objectives/" + id + ".json",
                        "bersaglio '" + o->targetStructure + "' non esiste nella mappa '"
                        + m.mapId + "'",
                        "Usa la label di un bersaglio strategico autorato in quella mappa, "
                        "oppure cambia la mappa della missione.");
                else if (found > 1)
                    add(d, L::Error, "Map", "maps/" + m.mapId + ".json",
                        "piu' bersagli strategici con la label '" + o->targetStructure
                        + "' → il riferimento e' ambiguo",
                        "Le label dei bersagli sono il loro unico nome: rendile univoche.");
            }
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
        checkRef(d, reg.classes(),       u.classId,         f, "classe");
        checkRef(d, reg.aiProfiles(),     u.aiProfileId,     f, "profilo AI");
        checkRef(d, reg.hitboxProfiles(), u.hitboxProfileId, f, "profilo hitbox");
        for (const auto& wid : u.weaponIds)
            checkRef(d, reg.weapons(), wid, f, "arma");
        for (const auto& aid : u.abilityIds)
            checkRef(d, reg.abilities(), aid, f, "abilita'");

        // Posa dell'arma in mano (KI #49). Il modello impugnato è l'arma
        // EFFETTIVA (classe → loadout, via WeaponAttach). La posa/scala DEVE
        // venire da quell'arma (`hand_scale`), non dal `weapon_display` legacy
        // dell'unità: quest'ultimo è tarato per un'arma fissa e, quando la classe
        // cambia arma, produce un modello a scala sbagliata (gigante/minuscolo).
        {
            const std::string eff = mini::classres::primaryWeaponId(reg, u);
            const WeaponDef* wpn = eff.empty() ? nullptr : reg.getWeapon(eff);
            if (wpn && wpn->handScale <= 0.0f)
                add(d, L::Warn, "Content", f,
                    "l'arma effettiva '" + eff + "' non ha una posa in mano "
                    "autorata (hand_scale) → si usa il weapon_display legacy "
                    "dell'unita', che e' tarato per un'arma fissa e sbaglia scala "
                    "se la classe cambia arma",
                    "Apri Weapon Editor → '" + eff + "' → tab Mesh → 'Posa in "
                    "mano' e attiva/tara la scala. Cosi' vale per ogni unita' che "
                    "impugna quest'arma (KI #49).");
        }
        // Unita' senza classe: NON e' rotta (i campi propri restano il fallback,
        // ADR-022 e' additivo) ma e' contenuto in stato legacy — e dal 2026-07-17
        // l'Entity Editor non edita piu' arma/profilo/abilita', perche' li decide
        // la classe. Quindi una classless ha un loadout che il gioco usa e che
        // l'editor non mostra: va detto, o diventa invisibile.
        if (u.classId.empty())
            add(d, L::Warn, "Content", f,
                "nessuna CLASSE assegnata → arma, profilo AI e abilita' vengono dai "
                "campi legacy dell'unita', non piu' editabili dall'editor",
                "Assegna una classe in Entity Editor → tab Statistiche (la classe e' "
                "la professione: loadout + comportamento + abilita').");
        if (u.weaponIds.empty() && u.classId.empty())
            add(d, L::Warn, "Content", f, "nessuna arma e nessuna classe",
                "L'unita' entrera' in partita disarmata: assegnale una classe "
                "(Entity Editor → Statistiche), o conferma che e' voluto.");
        if (u.hp <= 0.0f)
            add(d, L::Error, "Content", f, "hp <= 0 → l'unita' muore allo spawn",
                "Imposta 'hp' > 0.");
        checkAsset(d, dataRoot, u.meshPath, f, "mesh unita'");
    };
    for (const auto& [id, u] : reg.enemies()) checkUnit(id, u, "enemies");
    for (const auto& [id, u] : reg.allies())  checkUnit(id, u, "allies");
    checkNearDuplicates(d, reg.enemies(), "enemies");
    checkNearDuplicates(d, reg.allies(),  "allies");

    // ── Profili hitbox: il vuoto degrada in SILENZIO ─────────────────────
    // zones vuoto non e' fatale: testHit() cade sul fallback sferico
    // (CombatSystem, k_hitRadius, moltiplicatore 1.0). Ma vuol dire niente
    // zone, niente headshot e niente colpi di striscio — l'unita' si comporta
    // diversamente da quello che il profilo promette, senza un solo messaggio.
    // E' l'unico modo per accorgersene senza leggere il codice del combat.
    for (const auto& [id, hp] : reg.hitboxProfiles())
    {
        const std::string f = "hitboxes/" + id + ".json";
        if (hp.zones.empty())
        {
            add(d, L::Warn, "Content", f,
                "profilo senza zone → l'unita' viene colpita come una SFERA, "
                "niente headshot ne' moltiplicatori",
                "Disegna le zone in Entity Editor → tab Hitbox, oppure elimina "
                "il profilo se non serve piu'.");
            continue;
        }
        for (const auto& z : hp.zones)
        {
            const std::string zf = f + " (zona '" + z.name + "')";
            if (z.halfExtents.x <= 0.0f || z.halfExtents.y <= 0.0f ||
                z.halfExtents.z <= 0.0f)
                add(d, L::Error, "Content", zf,
                    "half_extents <= 0 → zona di volume nullo, mai colpibile",
                    "Imposta tutti e tre gli half_extents > 0.");
            if (z.damageMultiplier <= 0.0f)
                add(d, L::Warn, "Content", zf,
                    "damage_multiplier <= 0 → colpire questa zona non fa danno",
                    "Imposta 'damage_multiplier' > 0, o conferma che e' voluto "
                    "(es. una zona corazzata).");
        }
    }

    // Un archetipo di roster (ADR-023) è valido se risolve come ENTITÀ o come
    // CLASSE con `base_entity` esistente (classe istanziabile come tipo-unità).
    auto checkRosterUnit = [&](const std::string& tid, bool ally,
                               const std::string& f, const char* what)
    {
        if (tid.empty()) return;
        const auto& table = ally ? reg.allies() : reg.enemies();
        if (table.count(tid)) return;                       // entità
        auto ci = reg.classes().find(tid);
        if (ci != reg.classes().end())
        {
            const ClassDef& c = ci->second;
            if (c.baseEntityId.empty())
                add(d, L::Error, "Content", f,
                    std::string(what) + " '" + tid + "' e' una classe SENZA base_entity "
                    "→ non istanziabile come unita'",
                    "Aggiungi base_entity alla classe (ADR-023) o referenzia un'entita'.");
            else if (!table.count(c.baseEntityId))
                add(d, L::Error, "Content", f,
                    std::string(what) + " '" + tid + "': la classe punta al corpo '"
                    + c.baseEntityId + "' inesistente fra gli " + (ally ? "alleati" : "nemici"),
                    "Imposta base_entity a un'entita' esistente.");
            return;                                          // classe + corpo → ok
        }
        checkRef(d, table, tid, f, what);                   // né entità né classe
    };

    // Una classe è "comandante" se porta un'ability di tipo "command" (ADR-024).
    // Serve a due controlli: il comandante di mappa deve esserlo, e NON deve finire
    // nel roster (spawnerebbe in molti — il bug che il campo `commander` evita).
    auto classIsCommander = [&](const std::string& id) -> bool
    {
        auto ci = reg.classes().find(id);
        if (ci == reg.classes().end()) return false;
        for (const auto& abId : ci->second.abilityIds)
        {
            const AbilityDef* ab = reg.getAbility(abId);
            if (ab && ab->type == "command") return true;
        }
        return false;
    };

    // ── Strutture composite: i riferimenti fra tipi (ADR-056 rivisto) ─────
    // Da quando una composita può contenerne un'altra per RIFERIMENTO, esiste una
    // nuova classe di errori completamente muta: il riferimento non risolve e
    // `expandAssembly` semplicemente **salta quella parte**. Nessun crash, nessun
    // messaggio — una torre che in mappa arriva senza il suo secondo piano.
    // Saltare in silenzio è la scelta giusta a runtime (meglio una parte in meno che
    // un blocco); dirlo è il mestiere del gate.
    {
        const auto resolve = [&reg](const std::string& tid) { return reg.getStructureType(tid); };
        for (const auto& [tid, t] : reg.structureTypes())
        {
            const std::string f = "structures/" + tid + ".json";
            for (const auto& p : t.parts)
            {
                if (!p.isRef()) continue;
                // Isolata e modificata: porta le sue parti, il tipo non la governa
                // più. Non c'è niente da risolvere, quindi niente da segnalare.
                if (!p.localParts.empty()) continue;
                const StructureTypeDef* sub = reg.getStructureType(p.refType);
                if (!sub)
                {
                    add(d, L::Error, "Struct", f,
                        "la parte '" + (p.label.empty() ? p.refType : p.label)
                        + "' rimanda a '" + p.refType + "', che non esiste "
                        "→ sparisce dall'espansione, in silenzio",
                        "Ricrea data/structures/" + p.refType + ".json, oppure togli "
                        "la parte dall'editor strutture.");
                    continue;
                }
                if (sub->parts.empty())
                    add(d, L::Error, "Struct", f,
                        "la parte rimanda a '" + p.refType + "', che NON e' un "
                        "assemblaggio → non produce nulla",
                        "Un riferimento vale solo verso una struttura composita. Usa "
                        "una primitiva se ti serve un tipo semplice.");
                else if (!sub->verified)
                    add(d, L::Warn, "Struct", f,
                        "la parte rimanda a '" + p.refType + "', che non ha superato "
                        "la verifica navmesh",
                        "Apri '" + p.refType + "' nell'editor strutture e premi "
                        "\"Verifica\": una parte impercorribile lo resta anche qui.");
            }
            if (mapstructures::assemblyUses(t, tid, resolve))
                add(d, L::Error, "Struct", f,
                    "la struttura contiene se stessa (anche indirettamente) "
                    "→ l'espansione si ferma a meta', sempre",
                    "Togli il riferimento circolare: una composita non puo' essere "
                    "una propria parte.");
            else if (mapstructures::assemblyDepth(t, resolve)
                     >= mapstructures::kMaxAssemblyDepth)
                add(d, L::Warn, "Struct", f,
                    "annidamento a " + std::to_string(mapstructures::assemblyDepth(t, resolve))
                    + " livelli: oltre " + std::to_string(mapstructures::kMaxAssemblyDepth)
                    + " le parti piu' interne vengono saltate",
                    "Esplodi uno dei livelli intermedi nell'editor strutture.");
        }
    }

    // ── Mappe ─────────────────────────────────────────────────────────────
    for (const auto& [id, m] : reg.maps())
    {
        const std::string f = "maps/" + id + ".json";

        // Un'istanza che dichiara un `type` inesistente ricade sulla primitiva nuda:
        // in mappa compare una scala al posto di una torre. È il difetto che l'utente
        // ha già visto ("appare solo una scala"), stavolta detto invece che scoperto.
        for (const auto& st : m.structures)
        {
            // Un'istanza MODIFICATA porta la sua geometria: il tipo può anche essere
            // sparito, lei si disegna lo stesso. Segnalarla sarebbe un falso allarme
            // — e i falsi allarmi sono il modo in cui un gate smette di essere letto.
            if (st.type.empty() || !st.localParts.empty()) continue;
            if (reg.getStructureType(st.type) == nullptr)
                add(d, L::Error, "Map", f,
                    "la struttura '" + (st.label.empty() ? st.type : st.label)
                    + "' usa il tipo '" + st.type + "', che non esiste "
                    "→ viene disegnata come la primitiva nuda",
                    "Ricrea data/structures/" + st.type + ".json, oppure riassegna "
                    "la struttura dal Map Editor.");
        }
        if (m.geometry.empty())
            add(d, L::Error, "Map", f, "geometry vuota → nessun pavimento",
                "Aggiungi almeno un box 'floor'. Senza geometria non esiste navmesh "
                "(ADR-017) e le unita' cadono nel vuoto.");
        for (const auto& t : m.enemyTypes)
        {
            checkRosterUnit(t, false, f, "archetipo nemico");
            if (classIsCommander(t))
                add(d, L::Warn, "Content", f,
                    "'" + t + "' e' un comandante (ability 'command') nel roster enemy_types "
                    "→ spawnerebbe in molti come truppa",
                    "Spostalo nel campo 'commander' della mappa (uno per mappa, ADR-024/doc 32).");
        }
        for (const auto& t : m.allyTypes)
            checkRosterUnit(t, true, f, "archetipo alleato");
        // Comandante strategico (ADR-024/044, doc 32): opzionale. Ora è una
        // definizione PROPRIA (`data/commanders/`); si accetta ancora una classe
        // legacy come fallback durante la transizione.
        if (!m.commander.unit.empty())
        {
            const CommanderDef* cdef = reg.getCommander(m.commander.unit);
            if (cdef)
            {
                // Nuovo path: il corpo deve risolvere e serve un'ability 'command'.
                if (reg.getEnemy(cdef->baseEntity) == nullptr
                    && reg.getAlly(cdef->baseEntity) == nullptr)
                    add(d, L::Error, "Content", f,
                        "comandante '" + m.commander.unit + "': base_entity '"
                        + cdef->baseEntity + "' non esiste",
                        "Scegli un'entità-corpo valida (es. B1 Battle Droid).");
                bool hasCmd = false;
                for (const auto& abId : cdef->abilities)
                { const AbilityDef* ab = reg.getAbility(abId);
                  if (ab && ab->type == "command") { hasCmd = true; break; } }
                if (!hasCmd)
                    add(d, L::Warn, "Content", "commanders/" + m.commander.unit + ".json",
                        "il comandante '" + m.commander.unit + "' non ha un'ability 'command' "
                        "→ non dirigera' i droidi",
                        "Aggiungi 'Tactical Command' fra le abilities del CommanderDef.");
            }
            else   // legacy: una classe con ability 'command' (transizione ADR-044)
            {
                checkRosterUnit(m.commander.unit, false, f, "comandante");
                if (!classIsCommander(m.commander.unit))
                    add(d, L::Warn, "Content", f,
                        "il comandante '" + m.commander.unit + "' non è un CommanderDef "
                        "né una classe con ability 'command' → non dirigera' i droidi",
                        "Crea data/commanders/" + m.commander.unit + ".json (ADR-044).");
            }
        }
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

        // ── Strutture strategiche (doc 34/35/36) ─────────────────────────
        // Questi errori sono tutti SILENZIOSI a runtime: la struttura nasce e
        // si vede, ma non fa ciò che l'autore credeva. È il caso per cui esiste
        // il gate (ADR-018).
        int commsPerTeam[3] = {0,0,0}, controlPerTeam[3] = {0,0,0};
        for (const auto& st : m.strategicTargets)
        {
            if (st.role != "generic" && st.role != "comms" && st.role != "control")
                add(d, L::Error, "Map", f,
                    "struttura '" + st.label + "': role '" + st.role + "' sconosciuto "
                    "→ trattata come 'generic', non fara' nulla",
                    "Usa 'generic', 'comms' (torre comunicazioni) o 'control' "
                    "(torre di controllo).");
            if (st.role == "comms"   && (st.team == 1 || st.team == 2)) ++commsPerTeam[st.team];
            if (st.role == "control" && (st.team == 1 || st.team == 2)) ++controlPerTeam[st.team];

            if (st.hp <= 0.0f)
                add(d, L::Error, "Map", f,
                    "struttura '" + st.label + "' con hp <= 0",
                    "Imposta 'hp' > 0.");
            // Un raggio piccolissimo è formalmente valido e praticamente inerte:
            // nessuna unità si troverà mai così vicina. Segnalato dall'utente
            // dopo averlo impostato a 1 (metro) aspettandosi un effetto.
            if (st.engageRadius > 0.0f && st.engageRadius < 3.0f)
                add(d, L::Warn, "Map", f,
                    "struttura '" + st.label + "': engage_radius "
                    + std::to_string((int)st.engageRadius) + " m e' troppo piccolo "
                    "→ nessuna AI la ingaggera' mai",
                    "Il raggio e' in METRI: usa 0 per 'mai di iniziativa', "
                    "oppure indicativamente 15-30 m.");
        }
        // La torre di controllo serve solo alla Repubblica (allyIntel e' team 1):
        // una autorata per i Separatisti e' lavoro buttato, non un errore fatale.
        if (controlPerTeam[2] > 0)
            add(d, L::Warn, "Map", f,
                "torre di controllo di team 2 (Separatisti): non ha alcun effetto",
                "I droidi sono diretti dal comandante (campo 'commander'). "
                "La torre di controllo e' della Repubblica (team 1).");
        for (int team = 1; team <= 2; ++team)
        {
            if (commsPerTeam[team] > 1)
                add(d, L::Warn, "Map", f,
                    "team " + std::to_string(team) + ": " + std::to_string(commsPerTeam[team])
                    + " torri di comunicazione → basta distruggerne una per degradare tutto",
                    "Ne serve una per fazione: le altre non aggiungono resistenza.");
            if (controlPerTeam[team] > 1)
                add(d, L::Warn, "Map", f,
                    "team " + std::to_string(team) + ": piu' di una torre di controllo",
                    "Le altre non fanno nulla: i segnali sono gia' pubblicati dalla prima.");
        }
        // Asimmetria involontaria: una fazione ha la torre e l'altra no.
        if ((commsPerTeam[1] > 0) != (commsPerTeam[2] > 0))
            add(d, L::Warn, "Map", f,
                "solo una fazione ha la torre di comunicazione → vantaggio strutturale "
                "non dichiarato",
                "Autorane una per entrambe, o accetta l'asimmetria di proposito.");
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
        // POSA IN MANO dell'arma di CLASSE (KI #49 / KI #90, 2026-08-02).
        // Il controllo gemello esiste già sulle ENTITÀ, ma non copriva le classi —
        // ed è proprio lì che serve: il caso in cui la posa sbaglia è **la classe
        // che cambia l'arma**. Il Marksman equipaggia il DC-15X, che non aveva
        // `hand_scale`: ricadeva sul `weapon_display` del corpo, tarato per il
        // DC-15A → fucile minuscolo in mano, e nessun avviso da nessuna parte.
        for (const char* which : {"primaria", "secondaria"})
        {
            const std::string& wid = (which[0] == 'p') ? c.primaryWeaponId
                                                       : c.secondaryWeaponId;
            if (wid.empty()) continue;
            const WeaponDef* wpn = reg.getWeapon(wid);
            if (wpn && wpn->handScale <= 0.0f)
                add(d, L::Warn, "Content", f,
                    "l'arma " + std::string(which) + " '" + wid + "' non ha una posa "
                    "in mano autorata (hand_scale) → si usa il weapon_display del "
                    "CORPO, tarato per un'altra arma: in mano esce a scala sbagliata",
                    "Weapon Editor → l'arma → 'Posa in mano': attiva 'Anteprima in "
                    "mano', scegli il corpo e tara la scala guardandola.");
        }
        // Il profilo AI e' cio' che rende la classe una PROFESSIONE (ADR-022):
        // un riferimento rotto qui la degrada a pacchetto di armi in silenzio.
        checkRef(d, reg.aiProfiles(), c.aiProfileId, f, "profilo AI");
        for (const auto& aid : c.abilityIds)
            checkRef(d, reg.abilities(), aid, f, "abilita'");
        // ADR-023: se la classe porta un corpo (istanziabile come unità), il
        // corpo deve esistere; i moltiplicatori > 0 (0/negativo = unità
        // invisibile o inerte).
        if (!c.baseEntityId.empty()
            && !reg.allies().count(c.baseEntityId)
            && !reg.enemies().count(c.baseEntityId))
            add(d, L::Error, "Content", f,
                "base_entity '" + c.baseEntityId + "' non esiste (ne' alleato ne' nemico)",
                "Scegli un'entita'-corpo esistente dal dropdown, o lascia base_entity vuoto.");
        if (c.hpMult <= 0.0f || c.speedMult <= 0.0f || c.damageMult <= 0.0f)
            add(d, L::Error, "Content", f,
                "moltiplicatore <= 0 (hp/speed/damage) → unita' invisibile o inerte",
                "Usa valori > 0 (1.0 = corpo invariato).");
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

// ── Salute tattica di una mappa (doc 41 B4) ────────────────────────────────
// Regole in UN SOLO POSTO: le usano editor (pannello cliccabile) e `--validate`
// (gate headless). Non inventano dati: leggono i derivati gia calcolati.
const char* tacticalDefectKindName(TacticalDefect::Kind k)
{
    switch (k)
    {
        case TacticalDefect::Kind::NoCoverage:    return "Non coprono nessuna posizione";
        case TacticalDefect::Kind::BlindVertical: return "Cieche verso le altre quote";
        case TacticalDefect::Kind::HighExposure:  return "Molto esposte";
        case TacticalDefect::Kind::Redundant:     return "Ridondanti";
        case TacticalDefect::Kind::EmptySector:   return "Settori senza posizioni";
        case TacticalDefect::Kind::UnmarkedCover: return "Ostacoli che tagliano il tiro ma non sono coperture";
        case TacticalDefect::Kind::UnreachablePoint: return "Punti che il gioco chiede di raggiungere ma la navigazione no";
        case TacticalDefect::Kind::TooSmallElevated: return "Ripiani troppo piccoli: spariscono dal navmesh";
        case TacticalDefect::Kind::NarrowGap:        return "Fessure che il navmesh non attraversa";
        default:                                   return "Altro";
    }
}

std::vector<TacticalDefect> analyzeTacticalHealth(const MapDef& map)
{
    std::vector<TacticalDefect> out;
    const size_t np = map.tacticalPositions.size();
    char buf[192];
    for (size_t i = 0; i < np; ++i)
    {
        const auto& p = map.tacticalPositions[i];
        const int idx = (int)i;
        // 1) Non copre NESSUNO: da qui non si batte alcuna altra posizione — spesso
        //    guarda un muro, o ha arco/gittata troppo stretti. Tatticamente muta.
        if (p.canShoot && i < map.positionCovers.size() && map.positionCovers[i].empty())
        {
            // Distinzione necessaria (misurata 2026-08-02): "non copre altre posizioni"
            // NON basta a dire che una posizione è inutile — una di PRIMA LINEA copre il
            // terreno e l'avvicinamento, non altri nodi, ed è legittima. Il difetto vero
            // è l'ISOLAMENTO: non copre nessuno **e** nessuno la batte → è fuori dalla
            // rete tattica, nessuno la userà mai. Se invece è esposta, è nel gioco: avviso.
            const float exposure = (i < map.positionExposure.size())
                                 ? map.positionExposure[i] : 0.0f;
            const bool isolated = (exposure <= 0.001f);
            std::snprintf(buf, sizeof(buf),
                          isolated ? "[%s %d] ISOLATA: non copre nessuno e nessuno la batte"
                                   : "[%s %d] non copre altre posizioni (avanzata? esp. %.0f%%)",
                          p.role.c_str(), idx + 1, exposure * 100.0f);
            out.push_back({TacticalDefect::Target::Position, TacticalDefect::Kind::NoCoverage,
                           idx, isolated ? 1 : 0, buf});
        }
        // 2) Molto esposta: battuta da mezza mappa (ADR-033) → cattiva posizione di tiro.
        if (i < map.positionExposure.size() && map.positionExposure[i] >= 0.55f)
        {
            std::snprintf(buf, sizeof(buf), "[%s %d] molto esposta (%.0f%%)",
                          p.role.c_str(), idx + 1, map.positionExposure[i] * 100.0f);
            out.push_back({TacticalDefect::Target::Position, TacticalDefect::Kind::HighExposure, idx, 0, buf});
        }
        // 3) Ridondante: stesso ruolo, a meno di 2 m E che guarda DALLA STESSA PARTE.
        //    Il facing e' decisivo: due posizioni sovrapposte con versi OPPOSTI (o molto
        //    diversi) coprono archi diversi e sono due opzioni tattiche legittime — es.
        //    due vantage schiena a schiena su una torretta. Segnalarle era un falso
        //    positivo (segnalato dall'utente 2026-08-02). Ridondante e' solo chi
        //    duplica davvero: stessa zona E stessa direzione.
        for (size_t j = i + 1; j < np; ++j)
        {
            const auto& q = map.tacticalPositions[j];
            if (q.role != p.role) continue;
            const float dx = q.x - p.x, dy = q.y - p.y, dz = q.z - p.z;
            if (dx*dx + dy*dy + dz*dz > 4.0f) continue;
            // Differenza angolare minima fra i due fronti, normalizzata a [0,180].
            float dAng = std::fabs(q.facingDeg - p.facingDeg);
            while (dAng > 360.0f) dAng -= 360.0f;
            if (dAng > 180.0f) dAng = 360.0f - dAng;
            if (dAng > 45.0f) continue;   // guardano altrove: NON ridondanti
            std::snprintf(buf, sizeof(buf), "[%s %d] ridondante con #%d (< 2 m, stesso fronte)",
                          p.role.c_str(), idx + 1, (int)j + 1);
            out.push_back({TacticalDefect::Target::Position, TacticalDefect::Kind::Redundant, idx, 0, buf});
            break;
        }
    }
    // 4) Settore SENZA posizioni tattiche: il comando ci manda unita che non trovano
    //    dove stare → buco di copertura a livello di teatro.
    for (size_t s = 0; s < map.sectors.size(); ++s)
    {
        const auto& sec = map.sectors[s];
        bool any = false;
        for (const auto& p : map.tacticalPositions)
        {
            const float dx = p.x - sec.x, dz = p.z - sec.z;
            if (dx*dx + dz*dz <= sec.radius * sec.radius) { any = true; break; }
        }
        if (!any)
        {
            // Severita' proporzionale all'IMPORTANZA autorata: un settore che il comando
            // sceglie come fronte e senza posizioni e' un buco vero; una corsia di
            // TRANSITO a bassa importanza (o uno spawn) e' normale che non ne abbia.
            // Senza questa distinzione l'elenco si riempiva di falsi positivi (misurato
            // su Training Ground: 9 su 13 "problemi" erano corsie/spawn) — e un elenco
            // rumoroso si smette di leggere.
            const bool matters = sec.importance >= 1.0f;
            std::snprintf(buf, sizeof(buf),
                          matters ? "[settore %s] nessuna posizione tattica dentro (imp. %.1f)"
                                  : "[settore %s] nessuna posizione tattica (transito? imp. %.1f)",
                          sec.label.c_str(), sec.importance);
            out.push_back({TacticalDefect::Target::Sector, TacticalDefect::Kind::EmptySector, (int)s, matters ? 1 : 0, buf});
        }
    }
    // 5) OSTACOLO CHE TAGLIA IL TIRO MA NON È UNA COPERTURA (KI #86 causa 3).
    //    Un muro non autorato ha il peggio dei due mondi: toglie le linee di tiro come
    //    una copertura, ma nessuna AI sa usarlo — non ci si ripara dietro, non lo si
    //    aggira, non lo si sfrutta. Il combattimento si spegne senza che sia colpa di
    //    nessun sistema, e a occhio non si distingue da un bug dell'AI.
    //
    //    LA SOGLIA VA SCALATA CON L'OGGETTO, e questo è costato un errore: una prima
    //    misura a runtime usava un raggio FISSO di 3 m dal CENTRO del bloccante e
    //    concludeva "57% geometria muta". Falso: per un muro largo 7 m o un impalcato
    //    largo 31 m le coperture autorate stanno ai BORDI (misurate a 3,3-5,4 m dal
    //    centro) e venivano contate come assenti. Con la soglia proporzionata, Training
    //    Ground ha 4 ostacoli non marcati — non il 57%.
    //
    //    CALIBRAZIONE (un elenco rumoroso si smette di leggere — lezione dei settori
    //    di transito qui sopra): si considerano SOLO i box che bloccano davvero un tiro
    //    al busto, cioè che salgono oltre ~0.9 m dal proprio appoggio E hanno una
    //    pianta di almeno 1.5 m. Un paletto o un gradino non entrano. Gli impalcati
    //    orizzontali (`sy` piccolo) sono esclusi: sono pavimenti/tettoie, li giudica
    //    `BlindVertical`, non questo.
    {
        // Raggio entro cui una posizione autorata "spiega" l'ostacolo: la sua mezza
        // pianta più 2.5 m — la distanza a cui si sta effettivamente dietro un muro.
        int reported = 0;
        for (size_t g = 0; g < map.geometry.size(); ++g)
        {
            const auto& b = map.geometry[g];
            if (!b.collider) continue;
            const float hx = b.sx * 0.5f, hy = b.sy * 0.5f, hz = b.sz * 0.5f;
            if (hy < 0.45f) continue;                       // troppo basso per tagliare un tiro
            if (hx < 0.75f && hz < 0.75f) continue;         // paletto/dettaglio: non è copertura
            const float reach = std::max(hx, hz) + 2.5f;
            bool explained = false;
            for (const auto& p : map.tacticalPositions)
            {
                const float dx = p.x - b.x, dz = p.z - b.z;
                if (dx*dx + dz*dz <= reach * reach) { explained = true; break; }
            }
            if (explained) continue;
            // Severità: 1 solo per gli ostacoli GROSSI (≥ 4 m di lato), che tagliano
            // interi corridoi di tiro. Gli altri restano avvisi da valutare.
            const bool major = (hx >= 2.0f || hz >= 2.0f);
            std::snprintf(buf, sizeof(buf),
                          "[geometria %d] ostacolo %.1f×%.1f×%.1f a (%.0f, %.0f): taglia il tiro, "
                          "nessuna copertura autorata entro %.0f m",
                          (int)g + 1, b.sx, b.sy, b.sz, b.x, b.z, reach);
            out.push_back({TacticalDefect::Target::Geometry, TacticalDefect::Kind::UnmarkedCover,
                           (int)g, major ? 1 : 0, buf});
            if (++reported >= 40) break;   // oltre, l'elenco non si legge: sistemane un po'
        }
    }

    // 6) PUNTO CHIESTO MA IRRAGGIUNGIBILE (KI #90).
    //    Un command post su un gradino più alto di `STEP_HEIGHT` non è salibile:
    //    il navmesh lo tratta come ostacolo e Detour ci passa **intorno**. Il
    //    gioco continua a chiedere di catturarlo, le AI ci orbitano attorno, e
    //    nulla lo segnala — è successo davvero su `firebase`/Alpha, con una
    //    missione resa incompletabile e mezza giornata per capirlo.
    //
    //    Controllo puramente sui DATI, quindi vale nell'editor e nel gate senza
    //    costruire il navmesh: il post sta dentro la pianta di un box il cui
    //    ripiano supera lo scalino massimo? Allora ci si sale solo se qualcosa
    //    di più basso fa da gradino — e questo il controllo non lo sa, perciò
    //    resta un AVVISO da valutare, non un errore.
    // La domanda giusta NON è "il centro sta su un ripiano alto" — un primo
    // tentativo la poneva così e segnalava anche i post Bravo/Charlie, che nelle
    // simulazioni vengono catturati regolarmente: stanno su un rialzo, ma c'è
    // terreno normale abbastanza vicino da rientrare nel raggio di cattura.
    // Una guardia che grida al lupo si smette di leggere (lezione già pagata due
    // volte: settori di transito, funnel di missione).
    //
    // La domanda giusta è: **esiste, dentro il raggio di cattura, almeno un punto
    // in cui un'unità a terra può stare?** Si campiona il disco e si guarda il
    // ripiano sotto ogni campione: se ovunque è più alto dello scalino massimo,
    // per salirci non c'è modo e il post è irraggiungibile.
    //
    // ⚠ CORREZIONE 2026-08-05: "a terra" NON vuol dire "a quota zero". La versione
    // precedente dichiarava calpestabile solo `top <= STEP_HEIGHT`, cioè **segnalava
    // ogni post in quota anche quando le scale funzionavano** — puniva esattamente la
    // verticalità che stiamo cercando di abilitare, e su Training Ground gridava al
    // lupo su Alpha, che l'utente cattura regolarmente. Un ripiano è calpestabile se
    // è al suolo **oppure** se ha un gradino adiacente entro `STEP_HEIGHT` da cui
    // salirci. (La risposta completa — "è connesso allo spawn?" — è il `componentId`
    // del navmesh, doc 46 M1: qui si resta sui soli dati, come dichiarato sopra.)
    // La "raggiungibilità a gradini" è una proprietà della SUPERFICIE, non del punto.
    // Prima chiedevo "c'è un gradino accanto a questo campione?", e su una piattaforma
    // 8×8 con la scala su un lato tutti i campioni entro il raggio di cattura distavano
    // più di mezzo metro dalla scala → falso allarme, mentre il navmesh ci arrivava
    // benissimo (verificato: `found:true`, arrivo a 10 cm). La domanda giusta è: **il
    // box che regge questo punto ha un accesso a gradini?**
    // Usata sia dal controllo dei command post sia da quello dei ripiani: una logica
    // sola, altrimenti divergono al primo aggiustamento (è già successo).
    auto boxHasStepAccess = [&](const MapGeometryBox& b) -> bool
    {
        const float top = b.y + b.sy * 0.5f;
        if (top <= config::STEP_HEIGHT + 0.01f) return true;       // è già il suolo
        for (const auto& o : map.geometry)
        {
            if (&o == &b || !o.collider) continue;
            // Il gradino da cui si sale deve poter reggere un piede. La soglia è la
            // PEDATA NORMATIVA (doc 47 §4.3), non un numero scelto a mano: con 0,6 m
            // il gate scartava le scale generate dalle nostre stesse primitive, che
            // hanno pedate da 0,30 — cioè contraddiceva le metriche che deve far
            // rispettare.
            if (o.sx < mapmetrics::STAIR_TREAD || o.sz < mapmetrics::STAIR_TREAD) continue;
            const float otop = o.y + o.sy * 0.5f;
            if (otop >= top - 0.01f) continue;                     // non è sotto
            if (top - otop > config::STEP_HEIGHT) continue;        // troppo alto
            if (std::fabs(b.x - o.x) - (b.sx + o.sx) * 0.5f > 0.5f) continue;
            if (std::fabs(b.z - o.z) - (b.sz + o.sz) * 0.5f > 0.5f) continue;
            return true;
        }
        return false;
    };
    for (size_t c = 0; c < map.commandPosts.size(); ++c)
    {
        const auto& cp = map.commandPosts[c];
        const float R = (cp.radius > 0.5f) ? cp.radius : 3.0f;
        bool standable = false;
        constexpr int kSteps = 12;   // griglia 12×12 sul disco: ~110 campioni utili
        for (int iz = 0; iz <= kSteps && !standable; ++iz)
            for (int ix = 0; ix <= kSteps && !standable; ++ix)
            {
                const float px = cp.x - R + (2.0f * R) * ix / kSteps;
                const float pz = cp.z - R + (2.0f * R) * iz / kSteps;
                const float ddx = px - cp.x, ddz = pz - cp.z;
                if (ddx * ddx + ddz * ddz > R * R) continue;   // fuori dal disco
                // Il box che REGGE il campione: è la superficie di cui va chiesta
                // la raggiungibilità (non il punto — vedi il commento su
                // `boxHasStepAccess`).
                float top = 0.0f;
                const MapGeometryBox* support = nullptr;
                for (const auto& b : map.geometry)
                {
                    if (!b.collider) continue;
                    if (std::fabs(px - b.x) > b.sx * 0.5f) continue;
                    if (std::fabs(pz - b.z) > b.sz * 0.5f) continue;
                    const float t = b.y + b.sy * 0.5f;
                    if (t > top) { top = t; support = &b; }
                }
                if (top <= config::STEP_HEIGHT + 0.01f) standable = true;      // suolo
                else if (support && boxHasStepAccess(*support)) standable = true;
            }
        if (standable) continue;
        std::snprintf(buf, sizeof(buf),
                      "[post %s] nessun punto calpestabile entro il raggio di cattura "
                      "(%.1f m): tutto il disco sta su ripiani oltre lo scalino massimo "
                      "(%.2f m) → il navmesh ci passa INTORNO e nessuno lo cattura",
                      cp.label.c_str(), R, config::STEP_HEIGHT);
        out.push_back({TacticalDefect::Target::Sector,   // niente target dedicato: il post
                       TacticalDefect::Kind::UnreachablePoint, (int)c, 1, buf});
    }

    // 7) PIATTAFORME SENZA ACCESSO — isole del navmesh (2026-08-04).
    //    Trovato indagando perché le AI non salissero mai su `firebase`: la mappa
    //    ha piattaforme a 1,0 / 2,0 / 2,5 m e **nessun box che faccia da gradino**.
    //    Il salto dal pavimento è 0,90 m contro uno `STEP_HEIGHT` di 0,55: per il
    //    navmesh quelle superfici sono **isole irraggiungibili**, e nessuna unità
    //    potrà mai salirci. Le scale esistevano solo nel modello VISIVO — ed è
    //    esattamente il divario che ADR-047 mette in guardia: la verità tattica
    //    sono i box, il mesh è decorazione.
    //
    //    Il controllo è volutamente semplice: per ogni ripiano abbastanza largo da
    //    starci sopra, esiste un altro ripiano entro `STEP_HEIGHT` sotto di esso e
    //    adiacente in pianta? Se no, non c'è modo di salirci a piedi.
    {
        constexpr float kMinFootprint = 1.2f;   // come mapquery::groundHeightAt
        int reported = 0;
        for (size_t g = 0; g < map.geometry.size() && reported < 12; ++g)
        {
            const auto& b = map.geometry[g];
            if (!b.collider) continue;
            // ── È una superficie su cui si DEVE poter salire? ────────────────
            // Solo `floor` e `platform` (ADR-053). Un muro o un cubo-ostacolo
            // irraggiungibile non è un difetto: è un muro. Prima il gate non aveva
            // modo di distinguerli e segnalava 4 cubi 2×2×2 messi apposta come
            // ostacoli su Training Ground.
            if (!boxShouldBeReachable(b.type)) continue;
            if (b.sx < kMinFootprint || b.sz < kMinFootprint) continue;   // non è pavimento
            const float top = b.y + b.sy * 0.5f;
            if (top <= config::STEP_HEIGHT + 0.01f) continue;             // già al suolo
            // ── C'è ALTEZZA per starci in piedi? Se no, non è un ripiano. ─────
            // Una scalinata si costruisce impilando slab: le terrazze di Training
            // Ground hanno 2-4 slab sovrapposti per fascia, e solo il PIÙ ALTO è la
            // superficie calpestabile — gli altri sono riempimento, sepolti sotto.
            // Segnalarli produceva 10 falsi allarmi su 22, e un gate che grida al lupo
            // su metà dei casi è un gate che si smette di leggere.
            // Il criterio giusto NON è "è sepolto" (fra due slab autorati a mano restano
            // 3 cm d'aria, e 3 cm non sono un posto dove stare): è **quanto spazio
            // libero c'è sopra**. Sotto l'altezza dell'agente non ci si sta, punto —
            // ed è lo stesso `walkableHeight` che usa Recast per scartare quelle celle.
            // La domanda va posta per PUNTI, non per centro: dopo aver diviso una
            // pedata in due metà, il centro dello slab sottostante cade sul confine
            // fra le due e nessuna delle due "lo contiene" — il test sul solo centro
            // dava il risultato opposto a quello giusto. Si campiona la superficie e
            // si chiede: **esiste un punto dove ci si sta in piedi?** Se no, sepolto.
            constexpr float kAgentHeadroom = mapmetrics::AGENT_HEIGHT;
            constexpr int   kSamples       = 5;       // 5×5 sulla pianta
            bool standable = false;
            for (int si = 0; si < kSamples && !standable; ++si)
            for (int sj = 0; sj < kSamples && !standable; ++sj)
            {
                const float fx = (si + 0.5f) / kSamples - 0.5f;
                const float fz = (sj + 0.5f) / kSamples - 0.5f;
                const float px = b.x + fx * b.sx;
                const float pz = b.z + fz * b.sz;
                float clearance = 1e9f;
                for (const auto& o : map.geometry)
                {
                    if (&o == &b || !o.collider) continue;
                    // "Sta sopra" si giudica dal TOP, non dalla base: gli slab autorati
                    // a mano si COMPENETRANO (la terrazza sopra parte 3 cm sotto il top
                    // di quella sotto), e filtrando per base si scartava proprio il box
                    // che seppellisce. Con la base sotto il top la luce è negativa —
                    // che è esattamente la risposta giusta: lì non ci si sta.
                    if (o.y + o.sy * 0.5f <= top + 0.01f) continue;
                    if (std::fabs(px - o.x) > o.sx * 0.5f) continue;
                    if (std::fabs(pz - o.z) > o.sz * 0.5f) continue;
                    clearance = std::min(clearance, (o.y - o.sy * 0.5f) - top);
                }
                if (clearance >= kAgentHeadroom) standable = true;
            }
            if (!standable) continue;
            // Si cerca il ripiano adiacente PIÙ ALTO sotto questo: è il gradino da
            // cui si salirebbe. Distinguere "non c'è nessun gradino" da "il gradino
            // c'è ma è troppo alto" cambia completamente l'azione da fare — ed è il
            // caso reale di Training Ground, dove le scale ESISTONO (ripiani a 0,63
            // → 1,44 → 2,20 → 2,96) ma ogni alzata è 0,68-0,81 m contro un massimo
            // di 0,55. Un avviso che dicesse solo "manca un gradino" manderebbe a
            // costruire scale che ci sono già.
            bool hasAccess = false;
            float bestBelow = -1.0f;   // ripiano adiacente più alto sotto di questo
            for (const auto& o : map.geometry)
            {
                if (&o == &b || !o.collider) continue;
                // Il GRADINO da cui si sale non deve essere largo come un pavimento:
                // basta che ci si appoggi un piede, perché i gradini si concatenano.
                // Con la soglia da pavimento (1,2 m) le scale "CT stair" — pedate da
                // 0,8 m, del tutto normali — non venivano viste come gradini e il gate
                // dichiarava irraggiungibile un ripiano che ha la sua scala.
                // La soglia è la PEDATA NORMATIVA (0,30), non un numero a mano: con
                // 0,6 il gate scartava le scale prodotte dalle nostre stesse primitive.
                if (o.sx < mapmetrics::STAIR_TREAD || o.sz < mapmetrics::STAIR_TREAD) continue;
                const float otop = o.y + o.sy * 0.5f;
                if (otop >= top - 0.01f) continue;                 // non è sotto
                // Adiacente in pianta? (bordi che si toccano entro mezzo metro)
                const float gapX = std::fabs(b.x - o.x) - (b.sx + o.sx) * 0.5f;
                const float gapZ = std::fabs(b.z - o.z) - (b.sz + o.sz) * 0.5f;
                if (gapX > 0.5f || gapZ > 0.5f) continue;
                if (otop > bestBelow) bestBelow = otop;
                if (top - otop <= config::STEP_HEIGHT) { hasAccess = true; break; }
            }
            if (hasAccess) continue;
            if (bestBelow >= 0.0f)
            {
                const float rise = top - bestBelow;
                // L'AZIONE, non solo la diagnosi (doc 47 §8): quanti gradini servono
                // per coprire il dislivello all'alzata normativa.
                const int need = mapmetrics::stepsFor(rise);
                // La FASCIA PROIBITA merita una frase in più, perché è il caso
                // peggiore: non "nessuno ci sale" ma "il GIOCATORE ci sale e l'AI no".
                // Un difetto asimmetrico è più insidioso di uno simmetrico — sembra
                // funzionare finché non lo provi con le AI.
                const char* trap = mapmetrics::inLedgeTrap(rise)
                    ? " ⚠ ed e' nella FASCIA PROIBITA: il giocatore ci salta sopra, l'AI NO"
                      " (l'AI non salta mai col navmesh attivo) → asimmetria ingiusta"
                    : "";
                std::snprintf(buf, sizeof(buf),
                              "[geometria %d] ripiano a %.2f m: il gradino adiacente piu' alto e' a "
                              "%.2f m, ALZATA %.2f m contro un massimo di %.2f → troppo ripido, il "
                              "navmesh non ci sale. Servono %d gradini da %.2f m%s",
                              (int)g + 1, top, bestBelow, rise, config::STEP_HEIGHT,
                              need, mapmetrics::STAIR_RISER, trap);
            }
            else
                std::snprintf(buf, sizeof(buf),
                              "[geometria %d] ripiano calpestabile a %.2f m senza alcun gradino "
                              "adiacente: il navmesh non lo collega, nessuna unita' puo' salirci "
                              "(le scale del modello visivo non contano)",
                              (int)g + 1, top);
            out.push_back({TacticalDefect::Target::Geometry,
                           TacticalDefect::Kind::UnreachablePoint, (int)g, 1, buf});
            ++reported;
        }
    }

    // ── DUE MODI DI PERDERE UNA SUPERFICIE IN SILENZIO (doc 53 L5) ────────
    // Entrambi producono geometria perfetta nei dati e inesistente per l'AI, e
    // nessuno dei due si vede guardando la mappa. Le soglie vengono da `MapMetrics`,
    // che le ricava dai filtri di Recast: nessun numero scelto a occhio, perché una
    // soglia inventata qui produrrebbe un elenco che si smette di leggere.
    {
        // Superfici calpestabili SOPRAELEVATE (a terra i filtri non mordono allo
        // stesso modo: non c'è strapiombo attorno).
        struct Top { int idx; float x, z, sx, sz, top; };
        std::vector<Top> tops;
        for (std::size_t g = 0; g < map.geometry.size(); ++g)
        {
            const auto& b = map.geometry[g];
            if (!b.collider || !boxShouldBeReachable(b.type)) continue;
            const float top = b.y + b.sy * 0.5f;
            if (top <= config::STEP_HEIGHT + 0.01f) continue;   // è il suolo
            // È un RIPIANO o un GRADINO? Sotto la larghezza dell'unità di
            // riferimento (1,20 m) non è un posto dove stare: è una pedata, un
            // parapetto, un cordolo. La prima versione di questo controllo non lo
            // distingueva e produceva **412 segnalazioni**, tutte pedate di scala da
            // 2,01 × 0,30 — cioè esattamente l'elenco che si smette di leggere, il
            // difetto che questo controllo doveva evitare. Le scale hanno già il
            // loro controllo (`UnreachablePoint`, che ragiona sull'alzata).
            if (b.sx < mapmetrics::REF_UNIT_WIDTH || b.sz < mapmetrics::REF_UNIT_WIDTH)
                continue;
            tops.push_back({(int)g, b.x, b.z, b.sx, b.sz, top});
        }

        // 1) TROPPO PICCOLO. Sotto `ELEVATED_MIN_SPAN` su ENTRAMBI i lati non resta
        // nulla dopo erosione e area minima: è un problema. Sotto su UN lato solo è
        // una striscia — può essere una passerella voluta, quindi avviso.
        const float span = mapmetrics::ELEVATED_MIN_SPAN;
        for (const auto& t : tops)
        {
            const bool nx = t.sx < span, nz = t.sz < span;
            if (!nx && !nz) continue;
            const int sev = (nx && nz) ? 1 : 0;
            std::snprintf(buf, sizeof(buf),
                          "[geometria %d] ripiano a %.2f m, %.2f x %.2f: sotto il minimo di "
                          "%.2f m %s. Il navmesh toglie una cella di strapiombo per lato piu' "
                          "%.2f di erosione, poi scarta le regioni sotto 2,56 m2 → %s. "
                          "Allargalo ad almeno %.2f m.",
                          t.idx + 1, t.top, t.sx, t.sz, span,
                          (nx && nz) ? "su ENTRAMBI i lati" : "su un lato",
                          mapmetrics::AGENT_RADIUS,
                          (nx && nz) ? "non ci cammina nessuno"
                                     : "resta una striscia sottile o niente",
                          span);
            out.push_back({TacticalDefect::Target::Geometry,
                           TacticalDefect::Kind::TooSmallElevated, t.idx, sev, buf});
        }

        // 2) FESSURA. Due ripiani alla STESSA quota, affiancati, separati da meno del
        // diametro dell'agente: l'erosione mangia i due bordi e il passaggio non si
        // forma. Sotto 1 mm sono a contatto (nessuna fessura), quindi si ignora.
        const float minGap = 2.0f * mapmetrics::AGENT_RADIUS;
        for (std::size_t i = 0; i < tops.size(); ++i)
        for (std::size_t j = i + 1; j < tops.size(); ++j)
        {
            const Top& a = tops[i];
            const Top& b2 = tops[j];
            if (std::fabs(a.top - b2.top) > 0.02f) continue;    // quote diverse
            const float gapX = std::fabs(a.x - b2.x) - (a.sx + b2.sx) * 0.5f;
            const float gapZ = std::fabs(a.z - b2.z) - (a.sz + b2.sz) * 0.5f;
            // Affiancati su un asse e sovrapposti sull'altro: altrimenti sono
            // diagonali e la "fessura" non è un passaggio mancato.
            const bool alongX = (gapX > 0.001f && gapX < minGap && gapZ < -0.05f);
            const bool alongZ = (gapZ > 0.001f && gapZ < minGap && gapX < -0.05f);
            if (!alongX && !alongZ) continue;
            const float gap = alongX ? gapX : gapZ;
            std::snprintf(buf, sizeof(buf),
                          "[geometria %d] e [geometria %d]: due ripiani a %.2f m separati da "
                          "%.2f m. Sotto %.2f m (il diametro dell'agente) l'erosione chiude il "
                          "passaggio: si vede un varco e nessuno ci passa. Accostali "
                          "(Precisione > Appoggia) o allarga oltre %.2f m.",
                          a.idx + 1, b2.idx + 1, a.top, gap, minGap, minGap);
            out.push_back({TacticalDefect::Target::Geometry,
                           TacticalDefect::Kind::NarrowGap, a.idx, 1, buf});
        }
    }

    std::stable_sort(out.begin(), out.end(),
        [](const TacticalDefect& a, const TacticalDefect& b) { return a.severity > b.severity; });
    return out;
}

} // namespace mini
