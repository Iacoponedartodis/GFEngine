#pragma once
// SquadComponent — appartenenza a una squadra + ordine corrente (26_SquadAndCommand,
// ADR-020). Phase A: modello + ordini di movimento come VINCOLO sulla decisione dell'AI.
//
// Contratto di autonomia (doc 26): il giocatore/leader esprime l'INTENZIONE, il
// SquadSystem assegna il task, l'AI individuale (doc 16) sceglie autonomamente come
// eseguirlo (movimento/copertura/micro-combattimento), il CrowdSystem (doc 22) muove.
// L'ordine NON scrive mai il transform: è un vincolo, non un telecomando.

#include "mini/ecs/Entity.hpp"
#include <cstdint>
#include <vector>

namespace mini
{

// Ordini iniziali del doc 26 (pochi e utili, non un catalogo).
// Phase A implementa i tre di MOVIMENTO; gli altri sono dichiarati ma NON ancora
// eseguiti (Phase B/C) — un ordine non implementato fallisce ESPLICITAMENTE.
enum class OrderType : unsigned char
{
    None = 0,
    Follow,        // A: segui il leader
    HoldPosition,  // A: resta sul posto (ingaggia comunque)
    MoveTo,        // A: raggiungi un punto
    TakeCover,     // B: raggiungi un cover point reale del MapDef (doc 15/18)
    FocusFire,     // B: concentra il fuoco su un bersaglio designato
    Revive,        // C: va a rianimare un compagno a terra
    CoveringFire,  // C: un alleato DIRETTO tiene la posizione e fa fuoco di supporto
    Regroup,       // B2: ruota di comando (non ancora eseguito)
    Advance,       // B2: avanza di posizione di tiro in posizione di tiro verso il nemico
    Retreat        // B2: ripiega alla zona sicura più vicina (settore nostro / spawn)
};

// Un ordine non sparisce mai in silenzio: o si completa, o fallisce CON CAUSA (doc 26).
enum class OrderState : unsigned char { None = 0, Pending, Active, Done, Failed };

// Nome leggibile: unico per telemetria (SquadSystem) e HUD (Application) — due
// tabelle separate divergerebbero al primo ordine nuovo.
inline const char* orderName(OrderType t)
{
    switch (t) {
        case OrderType::Follow:       return "Follow";
        case OrderType::HoldPosition: return "HoldPosition";
        case OrderType::MoveTo:       return "MoveTo";
        case OrderType::TakeCover:    return "TakeCover";
        case OrderType::FocusFire:    return "FocusFire";
        case OrderType::Revive:       return "Revive";
        case OrderType::CoveringFire: return "CoveringFire";
        case OrderType::Regroup:      return "Regroup";
        case OrderType::Advance:      return "Advance";
        case OrderType::Retreat:      return "Retreat";
        default:                      return "None";
    }
}

struct SquadComponent
{
    int  squadId  = 0;       // 0 = nessuna squadra
    bool isLeader = false;

    // ── Ordine corrente ──────────────────────────────────────────────
    OrderType  order       = OrderType::None;
    OrderState state       = OrderState::None;
    EntityId   targetEntity = 0;      // Follow / FocusFire / Revive
    float      targetX = 0.0f, targetZ = 0.0f;   // MoveTo / HoldPosition / TakeCover
    std::uint64_t issuedTick = 0;
    // Causa esplicita del fallimento (stringa statica, mai nullptr se state==Failed).
    const char* failureReason = nullptr;

    // ── Stato "a terra" (Phase C, doc 26) ────────────────────────────────
    // Incapacitazione non letale: invece di morire, un membro va a terra con una
    // finestra di rianimazione. È ciò che dà peso alle perdite (la squadra è una
    // RISORSA, non comparse). Vive qui e non in un componente nuovo perché solo i
    // membri della squadra vanno a terra — e ce l'hanno già.
    bool  downed            = false;
    float bleedoutRemaining = 0.0f;   // secondi prima della morte definitiva
    float reviveProgress    = 0.0f;   // 0..SQUAD_REVIVE_TIME mentre un vivo è vicino
    // Quante volte questa VITA è già stata rianimata. Esaurito il cap, la prossima
    // caduta è letale: un uomo non si rialza all'infinito (bilanciamento — la
    // rianimazione restava troppo efficace anche coi tempi/HP corretti). Si azzera
    // col respawn, che crea una nuova entità → nuovo SquadComponent.
    int   revivesUsed       = 0;

    [[nodiscard]] bool hasActiveOrder() const
    { return order != OrderType::None &&
             (state == OrderState::Pending || state == OrderState::Active); }
};

// ── Mailbox dell'ordine del giocatore (Phase B) ──────────────────────────
// L'Application risolve il contesto (raycast mirino + cover point) e deposita
// QUI l'intenzione; il SquadSystem la consuma e la traduce in task per i membri.
// È il pattern mailbox del doc 10: `ecs/` non deve dipendere dal codice di gioco,
// quindi l'input NON entra nei sistemi — ci arriva attraverso il World.
struct SquadOrderRequest
{
    bool       pending = false;   // true = c'è un'intenzione da consumare
    OrderType  order   = OrderType::None;
    EntityId   targetEntity = 0;  // FocusFire (nemico) / Revive (compagno a terra)
    float      targetX = 0.0f, targetZ = 0.0f;   // MoveTo / TakeCover / CoveringFire
    // Ordine DIRETTO a membri SPECIFICI (vuoto = tutta la squadra). Era un singolo
    // `directedMember` (bastava a Revive/CoveringFire, che puntano un compagno); ora è
    // una LISTA perché il giocatore può SELEZIONARE più compagni e dare a ciascun
    // gruppo un ordine diverso — "più ordini diversi a più membri"
    // ([[orders-design-vision]]). Un solo destinatario = lista con un elemento.
    std::vector<EntityId> directedMembers;
};

} // namespace mini
