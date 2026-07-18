#include "mini/game/CommandPosts.hpp"
#include "mini/ecs/World.hpp"
#include "mini/ecs/Components.hpp"
#include "mini/core/Telemetry.hpp"

#include <nlohmann/json.hpp>   // event() data (ADR-016)
#include <cmath>
#include <iostream>

namespace mini
{

// Nome leggibile del team per la telemetria (ADR-016 / ADR-009).
static const char* teamName(int team)
{ return team == 1 ? "Ally" : team == 2 ? "Enemy" : "Neutral"; }

// Colori per proprietario: neutrale grigio, team1 blu, team2 rosso
static void ownerColor(int owner, float& r, float& g, float& b)
{
    switch (owner)
    {
    case 1:  r = 0.25f; g = 0.50f; b = 1.00f; break;
    case 2:  r = 1.00f; g = 0.25f; b = 0.25f; break;
    default: r = 0.70f; g = 0.70f; b = 0.70f; break;
    }
}

void CommandPosts::init(World& world, const std::vector<CommandPostDef>& defs,
                        Mesh* mesh, Texture* tex)
{
    m_posts.clear();
    for (const auto& d : defs)
    {
        Post p;
        p.def   = d;
        p.owner = d.initialTeam;

        float r, g, b;
        ownerColor(p.owner, r, g, b);

        // Palo bandiera (visibile da lontano, nessun collider)
        p.flagEntity = world.createEntity();
        world.addTransform(p.flagEntity, {d.x, d.y + 1.75f, d.z, 0, 0, 0,
                                          0.18f, 3.5f, 0.18f});
        world.addMeshRenderer(p.flagEntity, {mesh, tex, r, g, b});

        // Piastra a terra: mostra l'area di cattura
        p.baseEntity = world.createEntity();
        world.addTransform(p.baseEntity, {d.x, d.y + 0.03f, d.z, 0, 0, 0,
                                          d.radius * 2.0f, 0.06f, d.radius * 2.0f});
        world.addMeshRenderer(p.baseEntity, {mesh, tex, r * 0.55f, g * 0.55f, b * 0.55f});

        m_posts.push_back(p);
    }
    if (!m_posts.empty())
        std::cout << "[CommandPosts] " << m_posts.size() << " post inizializzati.\n";
}

void CommandPosts::applyOwnerColor(World& world, Post& p)
{
    float r, g, b;
    ownerColor(p.owner, r, g, b);
    if (auto* mr = world.getMeshRenderer(p.flagEntity))
    { mr->r = r; mr->g = g; mr->b = b; }
    if (auto* mr = world.getMeshRenderer(p.baseEntity))
    { mr->r = r * 0.55f; mr->g = g * 0.55f; mr->b = b * 0.55f; }
}

void CommandPosts::update(World& world, float dt)
{
    if (m_posts.empty()) return;

    for (auto& p : m_posts)
    {
        // Conta unità vive per team dentro il raggio (distanza XZ).
        int n1 = 0, n2 = 0;
        const float r2 = p.def.radius * p.def.radius;
        for (EntityId id : world.getEntities())
        {
            const auto* tm = world.getTeam(id);
            const auto* hp = world.getHealth(id);
            const auto* tr = world.getTransform(id);
            if (!tm || !hp || !tr || hp->current <= 0.0f) continue;
            if (world.getBullet(id)) continue;
            float dx = tr->x - p.def.x, dz = tr->z - p.def.z;
            if (dx*dx + dz*dz > r2) continue;
            if      (tm->teamId == 1) ++n1;
            else if (tm->teamId == 2) ++n2;
        }

        // Presenza esclusiva → progresso di cattura; conteso/vuoto → decadi.
        int presentTeam = 0;
        if (n1 > 0 && n2 == 0) presentTeam = 1;
        if (n2 > 0 && n1 == 0) presentTeam = 2;

        if (presentTeam != 0 && presentTeam != p.owner)
        {
            if (p.capturingTeam != presentTeam)
            {
                p.capturingTeam = presentTeam; p.progress = 0.0f;
                // Evento discreto (non per-frame): inizio cattura (ADR-016)
                telemetry::event(telemetry::Level::Info, "CommandPost", "Capture started",
                    {{"cp_id", p.def.label}, {"progress", 0.0f},
                     {"owner", teamName(p.owner)}, {"capturing", teamName(presentTeam)}});
            }

            p.progress += dt;
            if (p.progress >= p.def.captureTime)
            {
                p.owner         = presentTeam;
                p.capturingTeam = 0;
                p.progress      = 0.0f;
                applyOwnerColor(world, p);
                // Evento discreto: cambio proprietario (ADR-016)
                telemetry::event(telemetry::Level::Info, "CommandPost", "Capture update",
                    {{"cp_id", p.def.label}, {"progress", 1.0f},
                     {"owner", teamName(p.owner)}});
                std::cout << "[CommandPosts] '" << p.def.label
                          << "' catturato dal team " << p.owner << "!\n";
            }
        }
        else
        {
            // Nessuna cattura in corso valida: il progresso decade
            p.progress -= dt * 0.5f;
            if (p.progress <= 0.0f) { p.progress = 0.0f; p.capturingTeam = 0; }
        }
    }
}

int CommandPosts::countOwnedBy(int team) const
{
    int n = 0;
    for (const auto& p : m_posts)
        if (p.owner == team) ++n;
    return n;
}

std::vector<CommandPosts::OwnedPost> CommandPosts::ownedByTeam(int team) const
{
    std::vector<OwnedPost> out;
    for (const auto& p : m_posts)
        if (p.owner == team)
            out.push_back({p.def.label, p.def.x, p.def.z});
    return out;
}

std::vector<CommandPosts::Status> CommandPosts::status() const
{
    std::vector<Status> out;
    out.reserve(m_posts.size());
    for (const auto& p : m_posts)
    {
        Status s;
        s.label         = p.def.label;
        s.owner         = p.owner;
        s.capturingTeam = p.capturingTeam;
        s.progress01    = (p.def.captureTime > 0.01f)
                          ? p.progress / p.def.captureTime : 0.0f;
        out.push_back(std::move(s));
    }
    return out;
}

void CommandPosts::forceAllOwners(World& world, int team)
{
    for (auto& p : m_posts)
    {
        p.owner         = team;
        p.capturingTeam = 0;
        p.progress      = 0.0f;
        applyOwnerColor(world, p);
    }
}

} // namespace mini
