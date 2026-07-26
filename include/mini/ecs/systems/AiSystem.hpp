#pragma once
#include "mini/ecs/ISystem.hpp"
#include <vector>

namespace mini
{

// AiSystem: ogni entita' con AiComponent + TeamComponent spara
// verso le entita' del team avversario entro aggroRange.
// I proiettili AI volano a velocita' piu' bassa di quelli del giocatore.
class AiSystem : public ISystem
{
public:
    void update(World& world, float dt) override;

    static constexpr float k_bulletSpeed = 8.0f;
    static constexpr float k_bulletDmg   = 20.0f;
    static constexpr float k_bulletLife  = 5.0f;

private:
    // ── Contatti condivisi (doc 34) ──────────────────────────────────────
    // Prima erano ricostruiti da zero ogni tick: un avvistamento si propagava
    // ISTANTANEAMENTE e non c'era nulla su cui la rete di comunicazione potesse
    // agire. Ora ogni contatto PERSISTE con la sua età: con le comunicazioni
    // intatte è utilizzabile subito, senza torre solo dopo `shareDelay` — e la
    // posizione resta quella dell'avvistamento, quindi i compagni convergono su
    // dove il nemico ERA. L'informazione vecchia è imprecisa: è il punto.
    struct SharedContact { float x, z; int team; float age; };
    std::vector<SharedContact> m_contacts;

    // Rete di comunicazione: quando il comando ha rivalutato la direttiva, per
    // team. Senza torre il periodo si allunga → si esegue più a lungo un intento
    // ormai vecchio.
    float m_lastCommandDecision[3] = {0.0f, -1e9f, -1e9f};
    float m_time = 0.0f;   // tempo di gioco accumulato (cadenze e età dei contatti)

    // ── Bounding overwatch esplicito (ADR-032) ───────────────────────────
    // Le AVANZATE avviate questo tick verso una posizione tattica: un compagno
    // che resta fermo consulta l'elenco del tick PRECEDENTE (niente dipendenza
    // dall'ordine di iterazione) e va a coprire il punto d'avanzata usando il
    // grafo `positionCovers`. È il consumo esplicito del grafo, accanto
    // all'overwatch emergente (ADR-035) che resta.
    // `ttl`: l'avanzata resta leggibile per l'overwatch per la durata della
    // manovra (~5 s), non un solo tick — altrimenti il segnale (raro) e la
    // valutazione di chi copre (sparsa) non coincidono quasi mai (ADR-032).
    struct Advance { int coveredIdx; int team; float x, z; float ttl; };
    std::vector<Advance> m_advances, m_advancesPrev;

    // Cadenza del QUADRO TATTICO della torre (Fase torre-hub): la torre ricalcola la
    // LOS posizioni↔nemici a intervalli (non ogni tick — è pesante e non serve fresca
    // al frame). L'occupancy delle posizioni vive invece in `World::allyTac.claimed`
    // (centrale, azzerata ogni tick), condivisa da ordini e cloni autonomi.
    float m_allyTacTimer = 0.0f;
};

} // namespace mini