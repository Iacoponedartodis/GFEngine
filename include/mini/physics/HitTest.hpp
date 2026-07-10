#pragma once
// HitTest.hpp — test di collisione segmento vs zona hitbox, CONDIVISO tra
// CombatSystem (proiettili) e Application (mirino): per costruzione, ciò che
// il mirino riconosce è ciò che il proiettile colpisce.
//
// KI #13 (risolto 2026-07-10): il test ora è OBB — la rotazione locale della
// zona (`eulerDeg`, ordine Y*X*Z come il wireframe dell'editor) e lo yaw
// dell'entità inclinano il volume di collisione, non solo il rendering.

#include "mini/ecs/components/HitboxComponent.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <cmath>
#include <algorithm>

namespace mini::hittest
{

// Distanza² tra un punto e un segmento [a,b] (broad phase).
inline float segPointDistSq(const glm::vec3& a, const glm::vec3& b,
                            const glm::vec3& p)
{
    const glm::vec3 ab = b - a;
    const float len2 = glm::dot(ab, ab);
    float t = (len2 > 1e-8f) ? glm::dot(p - a, ab) / len2 : 0.0f;
    t = std::clamp(t, 0.0f, 1.0f);
    const glm::vec3 c = a + ab * t;
    return glm::dot(p - c, p - c);
}

// Segmento [a,b] vs AABB [mn,mx] (slab test, t in [0,1]).
inline bool segAABB(const glm::vec3& a, const glm::vec3& b,
                    const glm::vec3& mn, const glm::vec3& mx)
{
    const glm::vec3 d = b - a;
    float tmin = 0.0f, tmax = 1.0f;
    for (int i = 0; i < 3; ++i)
    {
        if (std::abs(d[i]) < 1e-8f)
        {
            if (a[i] < mn[i] || a[i] > mx[i]) return false;
        }
        else
        {
            const float inv = 1.0f / d[i];
            float t0 = (mn[i] - a[i]) * inv;
            float t1 = (mx[i] - a[i]) * inv;
            if (t0 > t1) std::swap(t0, t1);
            tmin = std::max(tmin, t0);
            tmax = std::min(tmax, t1);
            if (tmin > tmax) return false;
        }
    }
    return true;
}

// Segmento [a,b] (world) vs zona hitbox dell'entità in (entityPos, scale,
// yawDeg, meshOffY). OBB: il segmento viene portato nello spazio LOCALE
// della zona (inversa di yaw entità * euler zona) e testato contro
// l'AABB ±halfExtents.
inline bool segmentInZone(const glm::vec3& a, const glm::vec3& b,
                          const glm::vec3& entityPos,
                          float scale, float yawDeg, float meshOffY,
                          const HitZone& zone)
{
    // Centro world della zona: offset scalato, ruotato per lo yaw entità,
    // + offset verticale mesh (identico al rendering).
    glm::vec3 off = zone.offset * scale;
    glm::mat3 Ryaw(1.0f);
    if (yawDeg != 0.0f)
        Ryaw = glm::mat3(glm::rotate(glm::mat4(1.0f),
                                     glm::radians(yawDeg), {0, 1, 0}));
    const glm::vec3 center = entityPos + glm::vec3(0, meshOffY, 0) + Ryaw * off;

    // Rotazione totale della zona: yaw entità * euler locale (Y*X*Z,
    // stesso ordine del wireframe editor).
    glm::mat3 R = Ryaw;
    if (zone.eulerDeg.x != 0.0f || zone.eulerDeg.y != 0.0f || zone.eulerDeg.z != 0.0f)
    {
        const glm::mat4 rz =
              glm::rotate(glm::mat4(1.0f), glm::radians(zone.eulerDeg.y), {0,1,0})
            * glm::rotate(glm::mat4(1.0f), glm::radians(zone.eulerDeg.x), {1,0,0})
            * glm::rotate(glm::mat4(1.0f), glm::radians(zone.eulerDeg.z), {0,0,1});
        R = Ryaw * glm::mat3(rz);
    }

    // Segmento nello spazio locale della zona (R è ortonormale: inv = T).
    const glm::mat3 Rt = glm::transpose(R);
    const glm::vec3 la = Rt * (a - center);
    const glm::vec3 lb = Rt * (b - center);
    const glm::vec3 he = zone.halfExtents * scale;
    return segAABB(la, lb, -he, he);
}

} // namespace mini::hittest
