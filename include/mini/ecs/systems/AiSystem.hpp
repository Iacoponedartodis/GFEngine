#pragma once
#include "mini/ecs/ISystem.hpp"
#include "mini/ecs/Entity.hpp"   // EntityId (firma di updateEnemyCommand)
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
    // Livello di COMANDO del lato separatista (doc 32 v2 / ADR-024): legge i settori,
    // aggiorna torre e quadro tattico, e — con la sua CADENZA — ricostruisce le
    // direttive del Droide Tattico. Definito in `AiCommandLayer.cpp` insieme al resto
    // del command layer (audit #7): `update()` era 1740 righe. Restituisce la deriva
    // del comandante dalla sua casa (leash ADR-041), che serve alla telemetria.
    float updateEnemyCommand(World& world, const std::vector<EntityId>& snap, float dt);

    // ── Contatti condivisi (doc 34) ──────────────────────────────────────
    // Prima erano ricostruiti da zero ogni tick: un avvistamento si propagava
    // ISTANTANEAMENTE e non c'era nulla su cui la rete di comunicazione potesse
    // agire. Ora ogni contatto PERSISTE con la sua età: con le comunicazioni
    // intatte è utilizzabile subito, senza torre solo dopo `shareDelay` — e la
    // posizione resta quella dell'avvistamento, quindi i compagni convergono su
    // dove il nemico ERA. L'informazione vecchia è imprecisa: è il punto.
    // `confidence`: quanto ci si può fidare di questo contatto ORA (doc 40 A2). Nasce
    // alta se l'ha prodotto la VISTA, bassa se l'UDITO (si sa da dove, non chi), e
    // decade col tempo. È ciò che permette di distinguere "so dov'è" da "lì è successo
    // qualcosa": senza, l'informazione vecchia e quella fresca valevano uguale.
    struct SharedContact { float x, z; int team; float age; float confidence; };
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