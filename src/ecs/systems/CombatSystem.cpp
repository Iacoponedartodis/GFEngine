#include "mini/ecs/systems/CombatSystem.hpp"
#include "mini/ecs/World.hpp"
#include "mini/ecs/components/HitboxComponent.hpp"
#include "mini/ecs/components/SquadComponent.hpp"   // Phase C: stato "a terra"
#include "mini/core/GameConfig.hpp"                 // costanti bleed-out/rianimazione
#include "mini/game/data/GameplayBalance.hpp"       // ADR-043: rianimazione data-driven
#include "mini/physics/HitTest.hpp"
#include "mini/physics/Collision.hpp"
#include "mini/core/Telemetry.hpp"

#include <tracy/Tracy.hpp>   // ADR-015: no-op se USE_TRACY_PROFILER=OFF
#include <glm/glm.hpp>
#include <nlohmann/json.hpp>   // data dell'evento "member downed" (doc 21)
#include <iostream>
#include <vector>
#include <cmath>
#include <algorithm>
#include <unordered_set>

namespace mini
{

// ── Test a SEGMENTO (anti-tunneling) ─────────────────────────────────────
// I proiettili si muovono a step discreti (a 55 m/s sono ~0.9 m per tick):
// un test puntuale ATTRAVERSAVA le zone piccole. Il test segmento-vs-zona
// (ora OBB, KI #13) è CONDIVISO col mirino in physics/HitTest.hpp: per
// costruzione, ciò che il mirino riconosce è ciò che il proiettile colpisce.
using hittest::segPointDistSq;
using hittest::segmentInZone;

// ── Funnel di COMBATTIMENTO (ADR-050, doc 42 buco O4) ────────────────────────
// Fino a ieri la mia metrica di esito principale — quella su cui ho deciso metà
// dell'indagine di KI #86 — era una **regex su stdout** (`[Combat] Colpito!`).
// È l'anello più debole di ogni misura fatta finora: si rompe se qualcuno cambia
// una stringa, non dice chi ha colpito chi, e soprattutto **non ha
// denominatore** — "40 colpi a segno" non significa niente se non si sa se
// erano 50 colpi sparati o 500.
//
// Qui il funnel è completo: sparati → impattati (a segno / sul terreno) → danno
// → a terra → uccisi, per team. Il tasso di mancati è anche un segnale di
// BILANCIAMENTO, non solo di correttezza: un'accuratezza che crolla dice che le
// AI sparano da troppo lontano o attraverso il fumo delle proprie manovre.
namespace
{
struct CombatStats
{
    int hits[3]   = {0,0,0};     // impatti su un'entità, per team che spara
    int zoneHits[3] = {0,0,0};   // ...di cui su una zona hitbox (testa/arti)
    int shieldAbsorb[3] = {0,0,0};
    int kills[3]  = {0,0,0};
    int downs[3]  = {0,0,0};     // messi a terra (non uccisi): squadra alleata
    int wallHits  = 0;           // proiettili morti sulla geometria
    int expired   = 0;           // proiettili spenti per lifetime: MANCATI puri
    float damage[3] = {0,0,0};   // danno agli HP inflitto
};
CombatStats g_cs;
} // namespace

struct HitResult { bool hit = false; float mult = 1.0f; std::string zone; };

static HitResult testHit(const glm::vec3& bulletPrev,
                          const glm::vec3& bulletPos,
                          const glm::vec3& entityPos,
                          float scale, float yawDeg, float meshOffY,
                          const HitboxComponent* hb)
{
    // ── 1. Broad test: distanza segmento-centro (O(1) early-out) ────────
    const float distSq = segPointDistSq(bulletPrev, bulletPos, entityPos);
    const float broadR = 2.5f;
    if (distSq >= broadR * broadR) return {false, 1.0f, ""};

    // ── 2. Zone test se disponibile ────────────────────────────────────
    if (hb && hb->profile && !hb->profile->zones.empty())
    {
        for (const auto& zone : hb->profile->zones)
        {
            if (segmentInZone(bulletPrev, bulletPos, entityPos,
                              scale, yawDeg, meshOffY, zone))
                return {true, zone.damageMultiplier, zone.name};
        }
        // Nessuna zona colpita → fallback di striscio sul corpo
        const float fallbackR = 0.7f;
        if (distSq < fallbackR * fallbackR)
            return {true, 0.5f, "glance"};
        return {false, 1.0f, ""};
    }

    // ── 3. Fallback sferico puro (nessun profilo hitbox) ───────────────
    if (distSq < CombatSystem::k_hitRadius * CombatSystem::k_hitRadius)
        return {true, 1.0f, ""};

    return {false, 1.0f, ""};
}

// Test segmento vs sagoma OBB di un veicolo (19_Vehicles Fase B): il danno
// al mezzo vale su tutto il box (prima solo la sfera k_hitRadius al centro,
// coi colpi ai bordi che si fermavano sul collider senza danneggiare).
static bool segmentHitsVehicle(const glm::vec3& a, const glm::vec3& b,
                               const glm::vec3& pos, float yawDeg,
                               const VehicleComponent& vc)
{
    // Volume di DANNO (19 Fase B): offset verticale + half extents dedicati,
    // così lo spazio vuoto sotto uno speeder che fluttua non conta.
    HitZone box;
    box.offset      = {0.0f, vc.hitOffsetY, 0.0f};
    box.halfExtents = {vc.hitHalfX, vc.hitHalfY, vc.hitHalfZ};
    return segmentInZone(a, b, pos, /*scale=*/1.0f, yawDeg,
                         /*meshOffY=*/0.0f, box);
}

void CombatSystem::update(World& world, float dt)
{
    ZoneScoped;   // ADR-015: combat/collision update loop
    std::vector<EntityId> toDestroy;
    const std::vector<EntityId> entities = world.getEntities();

    // ── Piloti a bordo (R5): finché guidano, il MEZZO assorbe i colpi al
    //    posto loro (l'entità pilota segue la camera dentro il veicolo).
    std::unordered_set<EntityId> drivers;
    for (EntityId eid : entities)
        if (const auto* vc = world.getVehicle(eid); vc && vc->driver != 0)
            drivers.insert(vc->driver);

    // ── Rigenerazione scudi (ability "shield", 16_AiBehavior) ────────
    for (EntityId eid : entities)
    {
        auto* sh = world.getShield(eid);
        if (!sh || sh->max <= 0.0f) continue;
        if (sh->timer > 0.0f) { sh->timer -= dt; continue; }
        if (sh->current < sh->max && sh->regenRate > 0.0f)
            sh->current = std::min(sh->max, sh->current + sh->regenRate * dt);
    }

    for (EntityId bid : entities)
    {
        auto* bullet = world.getBullet(bid);
        if (!bullet) continue;

        bullet->lifetime -= dt;
        // Spento per tempo = MANCATO puro: non ha toccato né bersaglio né muro.
        // È la voce che dice "spara nel vuoto", distinta da "spara contro un muro".
        if (bullet->lifetime <= 0.0f)
        { ++g_cs.expired; toDestroy.push_back(bid); continue; }

        auto* bt = world.getTransform(bid);
        if (!bt) continue;
        const glm::vec3 bPos = {bt->x, bt->y, bt->z};

        // Posizione a inizio tick (per il test a segmento anti-tunneling)
        glm::vec3 bPrev = bPos;
        if (const auto* bv = world.getVelocity(bid))
            bPrev -= glm::vec3(bv->vx, bv->vy, bv->vz) * dt;

        bool hitSomething = false;
        for (EntityId eid : entities)
        {
            if (eid == bid || world.getBullet(eid)) continue;
            auto* team = world.getTeam(eid);
            if (!team || team->teamId == bullet->ownerTeam) continue;
            auto* et = world.getTransform(eid);
            auto* eh = world.getHealth(eid);
            if (!et || !eh) continue;
            // Un CADUTO ha `current == 0` ma **non** è morto: questo filtro lo
            // rendeva intoccabile, e il commento poco sotto ("un colpo su un
            // già-a-terra lo finisce") era falso da sempre. Ora il fuoco incrociato
            // può davvero finirlo — nessuna AI lo sceglie come bersaglio (fix
            // gemello in AiSystem), ma un colpo di passaggio conta.
            {
                const auto* sqd = world.getSquad(eid);
                const bool downed = sqd && sqd->downed;
                if (eh->current <= 0.0f && !downed) continue;
            }

            // Pilota a bordo: intoccabile direttamente, spara al mezzo (R5)
            if (drivers.count(eid)) continue;

            const glm::vec3 ePos = {et->x, et->y, et->z};

            HitResult result;
            if (const auto* vcomp = world.getVehicle(eid))
            {
                // Veicolo: sagoma OBB completa del box (Fase B)
                if (segmentHitsVehicle(bPrev, bPos, ePos, et->ry, *vcomp))
                    result = {true, 1.0f, "veicolo"};
            }
            else
            {
                const auto* hb = world.getHitbox(eid);
                const auto* mr = world.getMeshRenderer(eid);
                const float scale   = (et->sx > 0.0001f) ? et->sx : 1.0f;
                const float meshOff = mr ? mr->meshOffsetY : 0.0f;
                result = testHit(bPrev, bPos, ePos, scale, et->ry, meshOff, hb);
            }
            if (!result.hit)
            {
                // ── SOPPRESSIONE (doc 40 A3) ─────────────────────────────
                // Il colpo ha MANCATO, ma se è passato vicino l'unità se ne accorge e
                // reagisce: peggiora la mira e cerca riparo. È ciò che rende utile una
                // raffica che non uccide — la differenza fra fuoco di soppressione e
                // duello a chi mira meglio. Si riusa il segmento già calcolato per
                // l'anti-tunneling: costo praticamente nullo.
                if (auto* eai = world.getAi(eid))
                {
                    const glm::vec3 seg = bPos - bPrev;
                    const float len2 = glm::dot(seg, seg);
                    float t = (len2 > 1e-6f) ? glm::dot(ePos - bPrev, seg) / len2 : 0.0f;
                    t = (t < 0.0f) ? 0.0f : (t > 1.0f ? 1.0f : t);
                    const glm::vec3 closest = bPrev + seg * t;
                    const glm::vec3 d = ePos - closest;
                    if (glm::dot(d, d) < config::SUPPRESSION_NEAR_MISS
                                       * config::SUPPRESSION_NEAR_MISS)
                    {
                        eai->suppression += config::SUPPRESSION_PER_SHOT;
                        if (eai->suppression > 1.0f) eai->suppression = 1.0f;
                    }
                }
                continue;
            }

            const float dmg = bullet->damage * result.mult;

            // Lo scudo assorbe il danno prima degli HP; l'eccedenza passa
            // agli HP. Ogni colpo azzera l'attesa della rigenerazione.
            float toHp = dmg;
            if (auto* sh = world.getShield(eid); sh && sh->current > 0.0f)
            {
                const float absorbed = std::min(sh->current, toHp);
                sh->current -= absorbed;
                toHp        -= absorbed;
                sh->timer    = sh->regenDelay;
                if (bullet->ownerTeam >= 1 && bullet->ownerTeam <= 2)
                    g_cs.shieldAbsorb[bullet->ownerTeam] += (int)absorbed;   // O5
                telemetry::logTrace("shield: entita' "
                    + std::to_string(eid) + " assorbe "
                    + std::to_string((int)absorbed) + " (resta "
                    + std::to_string((int)sh->current) + ")");
                world.pushEvent("SCUDO #" + std::to_string(eid)
                    + " assorbe " + std::to_string((int)absorbed)
                    + "  [" + std::to_string((int)sh->current)
                    + "/" + std::to_string((int)sh->max) + "]");
            }
            // L'armatura divide il danno che arriva agli HP (dopo lo scudo, che
            // e' una barriera a se'). armor = 1 → nessuna riduzione, cioe' il
            // comportamento storico: il campo e' neutro finche' i dati non lo
            // cambiano (KI #35). Guardia su <= 0: un dato assurdo non deve
            // trasformarsi in danno infinito o negativo.
            // Danno EFFETTIVO agli HP di questo colpo: serve anche a decidere se
            // un alleato va a terra o muore sul posto (config::SQUAD_DOWN_LETHAL_HIT_FRAC).
            const float hpDamage = (eh->armor > 0.01f) ? (toHp / eh->armor) : toHp;
            eh->current -= hpDamage;

            // Funnel O4: impatto attribuito al team che ha sparato.
            if (bullet->ownerTeam >= 1 && bullet->ownerTeam <= 2)
            {
                const int ot = bullet->ownerTeam;
                ++g_cs.hits[ot];
                g_cs.damage[ot] += hpDamage;
                if (!result.zone.empty() && result.zone != "glance") ++g_cs.zoneHits[ot];
            }

            // Feedback HUD (hitmarker) SOLO per i colpi del giocatore —
            // non per quelli degli alleati AI (stesso team).
            if (bullet->fromPlayer)
            {
                world.combatFeedback.team1Hit = true;
                if (eh->current <= 0.0f) world.combatFeedback.team1Kill = true;
            }

            // Danno SUBITO dal GIOCATORE: registra la direzione della sorgente
            // (opposta al moto del proiettile) per l'indicatore direzionale.
            if (eid == world.playerEntity)
            {
                const float dx = -(bPos.x - bPrev.x);   // sorgente = da dove arriva
                const float dz = -(bPos.z - bPrev.z);
                const float len = std::sqrt(dx * dx + dz * dz);
                if (len > 1e-4f)
                {
                    world.combatFeedback.playerDamaged = true;
                    world.combatFeedback.hitDirX = dx / len;
                    world.combatFeedback.hitDirZ = dz / len;
                }
            }

            // Telemetria: ogni hit nel log (con zona, danno, hp, team) —
            // "i nemici non muoiono" diventa diagnosticabile dal file.
            telemetry::logTrace("hit: team" + std::to_string(bullet->ownerTeam)
                + " -> entita' " + std::to_string(eid)
                + " zona=" + (result.zone.empty() ? "corpo" : result.zone)
                + " x" + std::to_string(result.mult)
                + " danno=" + std::to_string((int)dmg)
                + " hp=" + std::to_string((int)eh->current)
                + "/" + std::to_string((int)eh->max));

            if (!result.zone.empty() && result.zone != "glance")
                std::cout << "[Combat] " << result.zone << " x" << result.mult
                          << " — danno: " << (int)dmg
                          << " HP: " << (int)eh->current << "/" << (int)eh->max << "\n";
            else
                std::cout << "[Combat] Colpito! HP: "
                          << (int)eh->current << "/" << (int)eh->max << "\n";

            // Log chat: colpo (con zona) e — se letale — eliminazione
            world.pushEvent("HIT team" + std::to_string(bullet->ownerTeam)
                + " -> #" + std::to_string(eid)
                + " " + (result.zone.empty() ? "corpo" : result.zone)
                + " -" + std::to_string((int)dmg)
                + "  hp " + std::to_string((int)std::max(0.0f, eh->current))
                + "/" + std::to_string((int)eh->max));

            hitSomething = true;
            toDestroy.push_back(bid);
            if (eh->current <= 0.0f)
            {
                // ── Phase C (doc 26): un membro della squadra alleata non muore
                //    subito, va "A TERRA" con una finestra di rianimazione. Solo
                //    la PRIMA volta: un colpo su un già-a-terra lo finisce (cade
                //    nel ramo morte sotto, perché sq->downed è true). Il GIOCATORE
                //    è escluso (ha il suo respawn, KI #39); i nemici muoiono come
                //    prima. Intercettazione ADDITIVA: cambia solo questo caso.
                auto* sq = world.getSquad(eid);
                const auto* tmv = world.getTeam(eid);
                const bool allySquad = sq && tmv && tmv->teamId == 1
                                       && eid != world.playerEntity;
                // Colpo pesante = morte sul posto, niente finestra di rianimazione
                // (bilanciamento 2026-07-20): finire gli HP non significa sempre
                // "a terra". Armi pesanti e colpi alla testa (moltiplicatore hitbox)
                // superano la soglia e uccidono; il fuoco leggero mette a terra.
                const bool lethalBlow =
                    eh->max > 0.0f
                    && hpDamage >= gameplay().squadDownLethalHitFrac * eh->max;
                // Cap di rianimazioni per vita: esaurito, la caduta è LETALE — un
                // uomo non si rialza all'infinito. Chiude il "non muore mai
                // nessuno" che tempi/HP da soli non risolvevano.
                const bool revivesLeft =
                    sq && sq->revivesUsed < gameplay().squadMaxRevives;
                if (allySquad && !sq->downed && !lethalBlow && revivesLeft)
                {
                    sq->downed            = true;
                    sq->bleedoutRemaining = gameplay().squadBleedoutTime;
                    sq->reviveProgress    = 0.0f;
                    sq->order             = OrderType::None;   // smette di eseguire
                    sq->state             = OrderState::None;
                    eh->current           = 0.0f;              // inerme, ma NON distrutto
                    telemetry::event(telemetry::Level::Info, "Squad", "member downed",
                                     {{"entity", (int)eid},
                                      {"bleedout", gameplay().squadBleedoutTime}});
                    if (bullet->ownerTeam >= 1 && bullet->ownerTeam <= 2)
                        ++g_cs.downs[bullet->ownerTeam];   // funnel O4
                    world.pushEvent("A TERRA #" + std::to_string(eid)
                        + " — rianimabile per " + std::to_string((int)gameplay().squadBleedoutTime) + "s");
                    break;   // niente morte/distruzione questo tick
                }

                telemetry::logInfo("kill: entita' " + std::to_string(eid)
                    + " eliminata dal team " + std::to_string(bullet->ownerTeam));
                if (bullet->ownerTeam >= 1 && bullet->ownerTeam <= 2)
                    ++g_cs.kills[bullet->ownerTeam];   // funnel O4
                std::cout << "[Combat] Eliminato!\n";
                world.pushEvent("KILL #" + std::to_string(eid)
                    + " eliminata dal team " + std::to_string(bullet->ownerTeam));
                // L'entità sparisce sotto (destroyEntity): chi gira dopo la
                // vedrebbe solo "non esiste più". Segnalare l'ELIMINAZIONE è
                // ciò che permette a SquadSystem di completare un FocusFire.
                {
                    const auto* vt = world.getTeam(eid);   // ancora vivo QUI
                    const int victimTeam = vt ? vt->teamId : 0;
                    world.killedThisTick.push_back({eid, victimTeam});
                    // Statistiche di missione (doc 25): si contano ORA, mentre il
                    // fatto esiste — l'entità sta per essere distrutta e chi
                    // l'ha uccisa è noto solo qui.
                    if (victimTeam == 2)
                    {
                        ++world.missionStats.teamKills;
                        if (bullet->fromPlayer) ++world.missionStats.playerKills;
                    }
                    // Il GIOCATORE è team 1: senza escluderlo finirebbe fra gli
                    // "alleati persi", che sono un costo diverso dalle sue morti
                    // (le conta Application, che conosce anche le morti non da
                    // proiettile).
                    else if (victimTeam == 1 && eid != world.playerEntity)
                        ++world.missionStats.alliesLost;
                }
                toDestroy.push_back(eid);
            }
            break;
        }

        // ── Proiettili vs geometria: un colpo che nel tick ha attraversato
        //    un collider (muri/casse/veicoli) muore lì — prima i proiettili
        //    ATTRAVERSAVANO i muri (il test copriva solo le entità con HP).
        //    Limite: se nello stesso tick c'è anche un bersaglio, vince il
        //    bersaglio (segmento ~0.9m: caso raro, accettato). ─────────────
        if (!hitSomething && !physics::hasLineOfSight(bPrev, bPos, world))
        { ++g_cs.wallHits; toDestroy.push_back(bid); }
    }
    for (EntityId id : toDestroy) world.destroyEntity(id);

    // ── Report del funnel (ADR-050 O4) ───────────────────────────────────
    // Stessa cadenza dell'AI e della navigazione (600 tick): le tre letture si
    // incrociano nel tempo, che è metà del loro valore — "in questa finestra le
    // AI hanno acquisito poco E l'accuratezza è crollata" è una frase che si può
    // scrivere solo se i tre battiti coincidono.
    if (world.getTickCount() % 600 == 1)
    {
        nlohmann::json d;
        for (int t = 1; t <= 2; ++t)
        {
            const std::string k = "team" + std::to_string(t);
            nlohmann::json e;
            e["sparati"]   = world.shotsFired[t];
            e["a_segno"]   = g_cs.hits[t];
            e["su_zona"]   = g_cs.zoneHits[t];   // testa/arti: qualità del colpo
            e["danno"]     = g_cs.damage[t];
            e["a_terra"]   = g_cs.downs[t];
            e["uccisi"]    = g_cs.kills[t];
            // L'accuratezza è il dato che mancava del tutto: senza denominatore
            // "40 colpi a segno" non distingue un tiratore da uno che spreca.
            e["accuratezza"] = world.shotsFired[t] > 0
                             ? (double)g_cs.hits[t] / world.shotsFired[t] : 0.0;
            d[k] = e;
        }
        d["su_geometria"] = g_cs.wallHits;   // fermati da un muro
        d["spenti_nel_vuoto"] = g_cs.expired;  // mancati puri
        d["scudo_assorbito"] = g_cs.shieldAbsorb[1] + g_cs.shieldAbsorb[2];
        telemetry::event(telemetry::Level::Info, "Combat", "funnel di fuoco", d);

        // ── ABILITY e VEICOLI (ADR-050, doc 42 buco O5) ──────────────────
        // Due sistemi interi mai osservati: non sapevo nemmeno se le ability
        // venissero usate. La lettura del codice dice che di ATTIVABILE a
        // runtime c'è solo `roll` (un unico punto in AiSystem) — `shield` e
        // `command` diventano componenti allo spawn e poi la loro `AbilityState`
        // non la guarda più nessuno. Questi numeri servono a confermarlo e a
        // rispondere alla domanda dell'utente: *cosa non funziona e aggiunge
        // solo peso*. Uno stato che esiste, si porta dietro un cooldown e non
        // viene mai attivato è esattamente quel peso.
        {
            int entConAbility = 0, statiTot = 0, statiAttivabili = 0, statiInCd = 0;
            for (EntityId eid : world.getEntities())
            {
                const auto* ab = world.getAbilities(eid);
                if (!ab || ab->states.empty()) continue;
                ++entConAbility;
                for (const auto& s : ab->states)
                {
                    ++statiTot;
                    if (s.type == "roll") ++statiAttivabili;   // l'unico con un trigger
                    if (s.cooldown > 0.0f) ++statiInCd;
                }
            }
            int scudi = 0, veicoli = 0, veicoliGuidati = 0;
            float scudoCarica = 0.0f;
            for (EntityId eid : world.getEntities())
            {
                if (const auto* sh = world.getShield(eid); sh && sh->max > 0.0f)
                { ++scudi; scudoCarica += sh->current; }
                if (const auto* vc = world.getVehicle(eid))
                { ++veicoli; if (vc->driver != 0) ++veicoliGuidati; }
            }
            nlohmann::json a;
            a["entita_con_ability"] = entConAbility;
            a["stati_totali"]       = statiTot;
            a["stati_attivabili"]   = statiAttivabili;   // se 0, il sistema è inerte
            a["stati_in_cooldown"]  = statiInCd;         // se 0 con attivabili>0: mai usate
            a["scudi"]              = scudi;
            a["scudo_carica"]       = scudoCarica;
            a["veicoli"]            = veicoli;
            a["veicoli_guidati"]    = veicoliGuidati;
            telemetry::event(telemetry::Level::Info, "Combat", "ability e veicoli", a);
        }
        g_cs = CombatStats{};
        world.shotsFired[1] = world.shotsFired[2] = 0;
    }
}

} // namespace mini