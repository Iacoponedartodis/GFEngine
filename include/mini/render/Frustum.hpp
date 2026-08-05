#pragma once
// ── Frustum — non disegnare ciò che la camera non inquadra (doc 43 R2) ───────
//
// PERCHÉ ORA. Misurato (KI #87): la scena 3D è il **95% del frame** e si spediscono
// **1,45-1,65 milioni di vertici per frame**. Il funnel di rendering diceva
// `esaminate 195 → disegnate 194`: **si disegnava tutto**, anche ciò che sta dietro
// la camera. Con il rendering client-side-array (ADR-003) ogni draw call rispedisce
// i vertici alla GPU, quindi un oggetto fuori campo costa quanto uno inquadrato.
//
// COME. Estrazione dei sei piani dalla matrice view-projection (metodo di
// Gribb/Hartmann): ogni piano è una combinazione di righe della matrice, quindi il
// frustum si ricava senza conoscere FOV, aspetto o near/far — e resta corretto
// qualunque proiezione la camera usi, comprese quelle dello split-screen.
//
// TEST A SFERA e non a box, deliberatamente: costa quattro moltiplicazioni per
// piano ed è **conservativo** (una sfera che racchiude l'oggetto non lo esclude
// mai per errore). Un test più stretto scarterebbe qualche oggetto in più al
// prezzo di poter far sparire qualcosa a bordo schermo — il difetto peggiore che
// un culling possa avere, perché si manifesta come "sfarfallio" ed è difficile da
// attribuire guardando lo schermo.

#include <glm/glm.hpp>
#include <cmath>

namespace mini::render
{

class Frustum
{
public:
    // Costruisce dai sei piani impliciti in `viewProj`. I piani sono NORMALIZZATI:
    // senza, `distance` non sarebbe una distanza metrica e il raggio della sfera
    // andrebbe scalato per ogni piano — errore classico e silenzioso.
    explicit Frustum(const glm::mat4& vp)
    {
        // riga i della matrice (glm è column-major: m[col][row])
        auto row = [&vp](int i) {
            return glm::vec4(vp[0][i], vp[1][i], vp[2][i], vp[3][i]);
        };
        const glm::vec4 r0 = row(0), r1 = row(1), r2 = row(2), r3 = row(3);
        m_planes[0] = r3 + r0;   // sinistro
        m_planes[1] = r3 - r0;   // destro
        m_planes[2] = r3 + r1;   // basso
        m_planes[3] = r3 - r1;   // alto
        m_planes[4] = r3 + r2;   // vicino
        m_planes[5] = r3 - r2;   // lontano
        for (auto& p : m_planes)
        {
            const float len = std::sqrt(p.x * p.x + p.y * p.y + p.z * p.z);
            if (len > 1e-6f) p /= len;
        }
    }

    // La sfera (centro, raggio) interseca il frustum? Conservativo: `true` in caso
    // di dubbio. Un falso positivo costa una draw call; un falso negativo fa
    // sparire un oggetto dallo schermo.
    [[nodiscard]] bool sphereVisible(const glm::vec3& c, float radius) const
    {
        for (const auto& p : m_planes)
            if (p.x * c.x + p.y * c.y + p.z * c.z + p.w < -radius)
                return false;   // completamente oltre un piano → fuori
        return true;
    }

private:
    glm::vec4 m_planes[6];
};

} // namespace mini::render
