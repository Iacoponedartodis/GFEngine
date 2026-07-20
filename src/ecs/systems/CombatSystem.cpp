#include "mini/ecs/systems/CombatSystem.hpp"
#include "mini/ecs/World.hpp"
#include "mini/ecs/components/HitboxComponent.hpp"
#include "mini/ecs/components/SquadComponent.hpp"   // Phase C: stato "a terra"
#include "mini/core/GameConfig.hpp"                 // costanti bleed-out/rianimazione
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
        if (bullet->lifetime <= 0.0f) { toDestroy.push_back(bid); continue; }

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
            if (!et || !eh || eh->current <= 0.0f) continue;

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
            if (!result.hit) continue;

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
            eh->current -= (eh->armor > 0.01f) ? (toHp / eh->armor) : toHp;

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
                if (allySquad && !sq->downed)
                {
                    sq->downed            = true;
                    sq->bleedoutRemaining = config::SQUAD_BLEEDOUT_TIME;
                    sq->reviveProgress    = 0.0f;
                    sq->order             = OrderType::None;   // smette di eseguire
                    sq->state             = OrderState::None;
                    eh->current           = 0.0f;              // inerme, ma NON distrutto
                    telemetry::event(telemetry::Level::Info, "Squad", "member downed",
                                     {{"entity", (int)eid},
                                      {"bleedout", config::SQUAD_BLEEDOUT_TIME}});
                    world.pushEvent("A TERRA #" + std::to_string(eid)
                        + " — rianimabile per " + std::to_string((int)config::SQUAD_BLEEDOUT_TIME) + "s");
                    break;   // niente morte/distruzione questo tick
                }

                telemetry::logInfo("kill: entita' " + std::to_string(eid)
                    + " eliminata dal team " + std::to_string(bullet->ownerTeam));
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
            toDestroy.push_back(bid);
    }
    for (EntityId id : toDestroy) world.destroyEntity(id);
}

} // namespace mini