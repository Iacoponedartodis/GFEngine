#pragma once

namespace mini
{

// AABB semiasse: il box va da (pos - half) a (pos + half) per ogni asse.
// Usato solo per oggetti statici dell'ambiente (non per nemici/proiettili).
struct ColliderComponent
{
    float hx = 0.5f;   // half-extent X
    float hy = 0.5f;   // half-extent Y
    float hz = 0.5f;   // half-extent Z
};

} // namespace mini