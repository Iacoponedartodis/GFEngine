#pragma once

#include "mini/ecs/Components.hpp"
#include "mini/ecs/Entity.hpp"

#include <cstdint>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace mini { class ISystem; struct MapDef; class NavManager; }

namespace mini
{

class World
{
public:
    World()  = default;
    ~World() = default;
    World(const World&)            = delete;
    World& operator=(const World&) = delete;

    void initialize();
    void tick(float dt);
    void registerSystem(std::unique_ptr<ISystem> system);

    EntityId createEntity();
    bool     destroyEntity(EntityId entity);
    [[nodiscard]] bool isValidEntity(EntityId entity) const;

    // Transform
    void addTransform(EntityId e, const TransformComponent& c);
    [[nodiscard]] bool hasTransform(EntityId e) const;
    TransformComponent*       getTransform(EntityId e);
    const TransformComponent* getTransform(EntityId e) const;

    // Team
    void addTeam(EntityId e, const TeamComponent& c);
    [[nodiscard]] bool hasTeam(EntityId e) const;
    TeamComponent*       getTeam(EntityId e);
    const TeamComponent* getTeam(EntityId e) const;

    // Velocity
    void addVelocity(EntityId e, const VelocityComponent& c);
    [[nodiscard]] bool hasVelocity(EntityId e) const;
    VelocityComponent*       getVelocity(EntityId e);
    const VelocityComponent* getVelocity(EntityId e) const;

    // Health
    void addHealth(EntityId e, const HealthComponent& c);
    [[nodiscard]] bool hasHealth(EntityId e) const;
    HealthComponent*       getHealth(EntityId e);
    const HealthComponent* getHealth(EntityId e) const;

    // MeshRenderer
    void addMeshRenderer(EntityId e, const MeshRendererComponent& c);
    [[nodiscard]] bool hasMeshRenderer(EntityId e) const;
    MeshRendererComponent*       getMeshRenderer(EntityId e);
    const MeshRendererComponent* getMeshRenderer(EntityId e) const;

    // Bullet
    void addBullet(EntityId e, const BulletComponent& c);
    [[nodiscard]] bool hasBullet(EntityId e) const;
    BulletComponent*       getBullet(EntityId e);
    const BulletComponent* getBullet(EntityId e) const;

    // Ai
    void addAi(EntityId e, const AiComponent& c);
    [[nodiscard]] bool hasAi(EntityId e) const;
    AiComponent*       getAi(EntityId e);
    const AiComponent* getAi(EntityId e) const;

    // Collider (AABB statico — solo oggetti ambiente)
    void addCollider(EntityId e, const ColliderComponent& c);
    [[nodiscard]] bool hasCollider(EntityId e) const;
    ColliderComponent*       getCollider(EntityId e);
    const ColliderComponent* getCollider(EntityId e) const;

    // Hitbox
    void addHitbox(EntityId e, const HitboxComponent& c);
    [[nodiscard]] bool hasHitbox(EntityId e) const;
    HitboxComponent*       getHitbox(EntityId e);
    const HitboxComponent* getHitbox(EntityId e) const;

    // Shield (ability "shield", 16_AiBehavior)
    void addShield(EntityId e, const ShieldComponent& c);
    [[nodiscard]] bool hasShield(EntityId e) const;
    ShieldComponent*       getShield(EntityId e);
    const ShieldComponent* getShield(EntityId e) const;

    // Vehicle (19_Vehicles, Fase A)
    void addVehicle(EntityId e, const VehicleComponent& c);
    [[nodiscard]] bool hasVehicle(EntityId e) const;
    VehicleComponent*       getVehicle(EntityId e);
    const VehicleComponent* getVehicle(EntityId e) const;

    // Abilities attive (16_AiBehavior est.: roll, ...)
    void addAbilities(EntityId e, const AbilityComponent& c);
    [[nodiscard]] bool hasAbilities(EntityId e) const;
    AbilityComponent*       getAbilities(EntityId e);
    const AbilityComponent* getAbilities(EntityId e) const;

    void setDebugLogging(bool enabled);
    [[nodiscard]] bool isDebugLoggingEnabled() const;
    [[nodiscard]] std::uint64_t                getTickCount() const;
    [[nodiscard]] const std::vector<EntityId>& getEntities()  const;

    // ── Eventi combat per il feedback HUD (hitmarker) ─────────────────
    // Scritti dal CombatSystem nel tick, consumati (e azzerati) da
    // Application dopo world.tick(). Mailbox minimale, niente event bus.
    struct CombatFeedback
    {
        bool team1Hit  = false;   // un proiettile del team 1 ha colpito
        bool team1Kill = false;   // ...e ha eliminato il bersaglio
        void reset() { team1Hit = false; team1Kill = false; }
    };
    CombatFeedback combatFeedback;

    // ── Log chat in-game (17_SandboxTools): messaggi leggibili degli
    //    eventi di gioco. Mailbox: i sistemi accodano, Application drena
    //    ogni frame verso la HUD. Non sostituisce la telemetria su file.
    std::vector<std::string> eventFeed;
    void pushEvent(std::string msg) { eventFeed.push_back(std::move(msg)); }

    // ── Mappa attiva (18_AiMapConsumption): puntatore opaco settato dal
    //    game mode in start(); l'AiSystem lo usa per cover/danger zone.
    //    L'ECS non include header di gioco: solo forward declaration.
    const MapDef* activeMap = nullptr;

    // ── Navigazione (ADR-017 Phase B): puntatore opaco al NavManager, settato
    //    da Application dopo la build del navmesh. AiSystem lo usa per il
    //    pathfinding, CrowdSystem per tick+write-back. nullptr = fallback aiMove.
    NavManager* nav = nullptr;

private:
    std::uint64_t m_tickCount    = 0;
    EntityId      m_nextEntityId = 1;

    std::vector<std::unique_ptr<ISystem>>               m_systems;
    std::vector<EntityId>                               m_entities;
    std::unordered_set<EntityId>                        m_aliveEntities;

    std::unordered_map<EntityId, TransformComponent>    m_transforms;
    std::unordered_map<EntityId, TeamComponent>         m_teams;
    std::unordered_map<EntityId, VelocityComponent>     m_velocities;
    std::unordered_map<EntityId, HealthComponent>       m_healths;
    std::unordered_map<EntityId, MeshRendererComponent> m_meshRenderers;
    std::unordered_map<EntityId, BulletComponent>       m_bullets;
    std::unordered_map<EntityId, AiComponent>           m_ais;
    std::unordered_map<EntityId, ColliderComponent>     m_colliders;
    std::unordered_map<EntityId, HitboxComponent>      m_hitboxes;
    std::unordered_map<EntityId, ShieldComponent>       m_shields;
    std::unordered_map<EntityId, VehicleComponent>      m_vehicles;
    std::unordered_map<EntityId, AbilityComponent>      m_abilities;

    bool m_debugLogging = false;
};

} // namespace mini