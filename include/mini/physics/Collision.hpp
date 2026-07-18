#pragma once

#include "mini/ecs/Entity.hpp"
#include <glm/glm.hpp>

namespace mini
{

// Forward
class World;
struct TransformComponent;
struct ColliderComponent;

namespace physics
{

// ── AABB axis-aligned bounding box (world-space) ─────────────────────────
struct AABB
{
    glm::vec3 min, max;
};

// Calcola l'AABB world-space di un collider tenendo conto della rotazione Y.
// ColliderComponent contiene half-extents RAW — questa funzione applica
// la rotazione UNA SOLA VOLTA. Non pre-ruotare i valori prima di passarli.
AABB computeWorldAABB(const TransformComponent& t, const ColliderComponent& c);

// True se un box centrato su 'pos' con half-extents 'half' si sovrappone
// a qualche entità con ColliderComponent nel world.
// excludeId: entità da ignorare (es. un veicolo che testa il PROPRIO
// movimento non deve collidere col proprio collider). 0 = nessuna.
bool hasCollision(const glm::vec3& pos, const glm::vec3& half, World& world,
                  EntityId excludeId = 0);

// Sliding per-asse: testa X, Y, Z separatamente e accetta solo i movimenti
// che non causano collisione. Zero jitter, sliding fluido lungo i muri.
glm::vec3 slideMove(const glm::vec3& prev, const glm::vec3& next,
                    const glm::vec3& half, World& world,
                    EntityId excludeId = 0);

// Sliding con step-up: come slideMove ma se bloccato orizzontalmente e lo
// spazio sopra l'ostacolo (≤ stepHeight) è libero, il soggetto viene
// spinto automaticamente in alto. Permette di salire scale e scalini.
glm::vec3 slideMoveWithStepUp(const glm::vec3& prev, const glm::vec3& next,
                               const glm::vec3& half, World& world,
                               float stepHeight = 0.55f,
                               EntityId excludeId = 0);

// Se pos compenetra un collider (es. spawn dentro un veicolo o sulla lastra di
// un command post), cerca la posizione libera più vicina e la ritorna. La
// ricerca è 3D a livelli di quota crescenti: a ogni livello prova il punto
// esatto e poi anelli orizzontali crescenti. Il livello 0 (quota di partenza)
// gestisce i muri (ci si sposta di lato verso il terreno libero); i livelli
// superiori gestiscono la geometria RIALZATA più larga del raggio orizzontale
// (lastre/piattaforme: si sale e ci si posiziona SOPRA). Prima l'orizzontale a
// terra, poi la risalita → non si "scala un muro" quando basta un passo di lato.
// Ritorna pos invariato se già libero o se nessun tentativo riesce.
glm::vec3 nudgeOutOfColliders(const glm::vec3& pos, const glm::vec3& half,
                              World& world, float step = 1.4f, int rings = 3);

// ── Ray-AABB line-of-sight ───────────────────────────────────────────────

// True se il segmento from→to NON è bloccato da nessun ColliderComponent.
// Usa il slab test per ray-AABB intersection.
bool hasLineOfSight(const glm::vec3& from, const glm::vec3& to, World& world);

} // namespace physics
} // namespace mini