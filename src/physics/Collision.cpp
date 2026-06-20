#include "mini/physics/Collision.hpp"
#include "mini/ecs/World.hpp"

#include <cmath>

namespace mini::physics
{

static constexpr float PI = 3.14159265f;

// ── AABB ─────────────────────────────────────────────────────────────────

AABB computeWorldAABB(const TransformComponent& t, const ColliderComponent& c)
{
    const float ry = t.ry * PI / 180.0f;
    const float ca = std::abs(std::cos(ry));
    const float sa = std::abs(std::sin(ry));
    const float wx = c.hx * ca + c.hz * sa;
    const float wz = c.hx * sa + c.hz * ca;
    return {
        {t.x - wx, t.y - c.hy, t.z - wz},
        {t.x + wx, t.y + c.hy, t.z + wz}
    };
}

bool hasCollision(const glm::vec3& pos, const glm::vec3& half, World& world)
{
    const glm::vec3 pn = pos - half;
    const glm::vec3 px = pos + half;

    for (EntityId id : world.getEntities())
    {
        const auto* col = world.getCollider(id);
        const auto* t   = world.getTransform(id);
        if (!col || !t) continue;

        const AABB b = computeWorldAABB(*t, *col);
        if (pn.x < b.max.x && px.x > b.min.x &&
            pn.y < b.max.y && px.y > b.min.y &&
            pn.z < b.max.z && px.z > b.min.z)
            return true;
    }
    return false;
}

// ── Slide semplice ───────────────────────────────────────────────────────

glm::vec3 slideMove(const glm::vec3& prev, const glm::vec3& next,
                    const glm::vec3& half, World& world)
{
    glm::vec3 r = prev;
    if (!hasCollision({next.x, r.y, r.z}, half, world)) r.x = next.x;
    if (!hasCollision({r.x, next.y, r.z}, half, world)) r.y = next.y;
    if (!hasCollision({r.x, r.y, next.z}, half, world)) r.z = next.z;
    return r;
}

// ── Slide con step-up ────────────────────────────────────────────────────

glm::vec3 slideMoveWithStepUp(const glm::vec3& prev, const glm::vec3& next,
                               const glm::vec3& half, World& world,
                               float stepHeight)
{
    glm::vec3 r = prev;

    // Asse X
    if (!hasCollision({next.x, r.y, r.z}, half, world))
    {
        r.x = next.x;
    }
    else
    {
        const glm::vec3 stepped = {next.x, r.y + stepHeight, r.z};
        if (!hasCollision(stepped, half, world) &&
            !hasCollision({r.x, r.y + stepHeight, r.z}, half, world))
        {
            r.x = next.x;
            r.y += stepHeight;
        }
    }

    // Asse Y (gravità — no step-up)
    if (!hasCollision({r.x, next.y, r.z}, half, world))
        r.y = next.y;

    // Asse Z
    if (!hasCollision({r.x, r.y, next.z}, half, world))
    {
        r.z = next.z;
    }
    else
    {
        const glm::vec3 stepped = {r.x, r.y + stepHeight, next.z};
        if (!hasCollision(stepped, half, world) &&
            !hasCollision({r.x, r.y + stepHeight, r.z}, half, world))
        {
            r.z = next.z;
            r.y += stepHeight;
        }
    }

    return r;
}

// ── Ray-AABB LOS ─────────────────────────────────────────────────────────

static bool rayBlockedByAABB(const glm::vec3& o, const glm::vec3& d,
                              const glm::vec3& bMin, const glm::vec3& bMax)
{
    float tmin = 0.0f, tmax = 1.0f;
    for (int i = 0; i < 3; ++i)
    {
        const float di = d[i];
        if (std::abs(di) < 1e-6f)
        {
            if (o[i] < bMin[i] || o[i] > bMax[i]) return false;
        }
        else
        {
            float t1 = (bMin[i] - o[i]) / di;
            float t2 = (bMax[i] - o[i]) / di;
            if (t1 > t2) { float tmp = t1; t1 = t2; t2 = tmp; }
            if (t1 > tmin) tmin = t1;
            if (t2 < tmax) tmax = t2;
            if (tmin > tmax) return false;
        }
    }
    return true;
}

bool hasLineOfSight(const glm::vec3& from, const glm::vec3& to, World& world)
{
    const glm::vec3 dir = to - from;

    for (EntityId id : world.getEntities())
    {
        const auto* col = world.getCollider(id);
        const auto* t   = world.getTransform(id);
        if (!col || !t) continue;

        const AABB b = computeWorldAABB(*t, *col);

        // Ignora se l'origine è dentro il box (AI a contatto col muro)
        if (from.x > b.min.x && from.x < b.max.x &&
            from.y > b.min.y && from.y < b.max.y &&
            from.z > b.min.z && from.z < b.max.z)
            continue;

        if (rayBlockedByAABB(from, dir, b.min, b.max))
            return false;
    }
    return true;
}

} // namespace mini::physics