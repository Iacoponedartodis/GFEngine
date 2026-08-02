#include "mini/core/StateDump.hpp"

#include "mini/ecs/World.hpp"
#include "mini/ecs/components/AiComponent.hpp"
#include "mini/render/Camera.hpp"
#include "mini/game/PlayerController.hpp"
#include "mini/game/game_modes/IGameMode.hpp"

#include <glm/glm.hpp>

namespace mini::statedump
{

nlohmann::json build(const char* reason, int gameState, bool worldReady,
                     const Camera& cam, const PlayerController& player,
                     const IGameMode* mode, const World& world)
{
    const glm::vec3 cp = cam.getPosition();
    const glm::vec3 cf = cam.getForward();
    nlohmann::json js;
    js["app"]         = "GFEngine";
    js["dump_reason"] = reason;
    js["game_state"]  = gameState;
    js["world_ready"] = worldReady;
    js["camera"]["pos"]     = {cp.x, cp.y, cp.z};
    js["camera"]["forward"] = {cf.x, cf.y, cf.z};
    js["player"]["hp"]     = player.prevHp;
    js["player"]["dead"]   = player.isDead;
    js["player"]["weapon"] = player.weapon().name;
    js["player"]["heat"]   = player.weapon().heat;
    js["tickets"]["team1"] = mode ? mode->getTeam1Tickets() : 0;
    js["tickets"]["team2"] = mode ? mode->getTeam2Tickets() : 0;
    auto& ents = js["entities"] = nlohmann::json::array();
    if (worldReady)
        for (EntityId id : world.getEntities())
        {
            const auto* tr = world.getTransform(id);
            if (!tr) continue;
            nlohmann::json ent;
            ent["id"]  = id;
            ent["pos"] = {tr->x, tr->y, tr->z};
            if (const auto* tm = world.getTeam(id))   ent["team"] = tm->teamId;
            if (const auto* hp = world.getHealth(id)) { ent["hp"] = hp->current; ent["hp_max"] = hp->max; }
            if (const auto* ai = world.getAi(id))
            {
                switch (ai->state) {
                    case AiState::Patrol: ent["ai_state"] = "Patrol"; break;
                    case AiState::Alert:  ent["ai_state"] = "Alert";  break;
                    case AiState::Hunt:   ent["ai_state"] = "Hunt";   break;
                    case AiState::Search: ent["ai_state"] = "Search"; break;
                }
                if (ai->hasLastKnown) ent["goal"] = {ai->lastKnownX, ai->lastKnownZ};
                // ── Ispettore del RAGIONAMENTO (doc 40 A5) ────────────────
                // Perché quest'unità sta (o non sta) combattendo. Senza questi campi,
                // "l'AI non spara" è un'osservazione che non si può indagare: si vede
                // il risultato e non la causa. Servono direttamente a KI #86 — dove
                // GUARDA (facing + ampiezza del cono) rispetto a dove sono i nemici, e
                // quale dei gate la sta bloccando.
                ent["facing_deg"]  = tr->ry;          // dove guarda ORA
                ent["fov_deg"]     = ai->fovDeg;      // quanto ampio è il suo cono
                ent["target"]      = ai->targetEntity;  // 0 = nessun bersaglio acquisito
                ent["suppression"] = ai->suppression;   // >0 = sotto tiro
                ent["role"]        = ai->combatRole;    // 1 sopprime, 2 aggira, 3 avanza
                ent["evading"]     = ai->evading;       // in fase di copertura (non spara)
                ent["reposition"]  = ai->repositionActive;
            }
            if (world.getBullet(id))  ent["kind"] = "bullet";
            if (world.getVehicle(id)) ent["kind"] = "vehicle";
            ents.push_back(std::move(ent));
        }
    js["entity_count"] = (int)ents.size();
    return js;
}

} // namespace mini::statedump
