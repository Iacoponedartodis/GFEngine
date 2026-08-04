#pragma once
// ── Bilanciamento globale di gameplay, DATA-DRIVEN (ADR-043) ─────────────────
// Alcune costanti "di sensazione" (rianimazione, degrado delle comunicazioni)
// erano `constexpr` in GameConfig.hpp: per tararle bisognava RICOMPILARE. Queste
// vivono ora in `data/config/gameplay.json`, caricato all'avvio, editabile nel
// BalanceEditor. I valori di default qui SONO i vecchi valori delle costanti:
// se il file manca o una chiave è assente, il comportamento non cambia.
//
// Contratto solo-file (ADR-002): la stessa struttura la leggono runtime ed
// editor, ognuno dal proprio `data/`. Header-only con `inline` + static locale
// per non toccare CMake su entrambi i target.
#include <nlohmann/json.hpp>
#include <fstream>
#include <iterator>
#include <string>

namespace mini
{

struct GameplayBalance
{
    // ── Squadra / rianimazione (doc 26) ─────────────────────────────────
    float squadBleedoutTime      = 15.0f;   // s a terra prima della morte
    float squadReviveRadius      =  2.5f;   // m per rianimare un compagno
    float squadReviveTime        = 10.0f;   // s di canalizzazione
    float squadReviveHp          =  0.15f;  // frazione di HP max al risveglio
    float squadDownLethalHitFrac =  0.20f;  // colpo ≥ questa frazione = morte sul colpo
    int   squadMaxRevives        =  1;      // rianimazioni per vita, oltre = letale
    // ── Quando NON si va a soccorrere (utente 2026-08-02) ───────────────
    // "vanno a rianimare un alleato a terra buttandosi in mezzo alla mischia, e
    // stanno lì a rianimare in mezzo a svariati nemici". Nessun esercito manda un
    // uomo a recuperarne un altro sotto tiro incrociato: prima si bonifica, o si
    // aspetta. Il soccorso viene DIFFERITO, non annullato — il bleed-out continua
    // a scorrere, quindi qualche volta l'uomo si perde davvero. È il costo della
    // scelta, ed è coerente con "degradare, non bloccare".
    float squadRescueThreatRadius = 12.0f;  // m attorno al caduto in cui si contano i nemici
    int   squadRescueMaxThreats   =  1;     // oltre questi nemici vivi, non si parte

    // ── Rete di comunicazione: quanto degrada senza torre (doc 34) ──────
    float commsLostRangeMult     =  0.5f;   // raggio di condivisione contatti
    float commsLostShareDelay    =  2.5f;   // s di ritardo dell'avvistamento
    float commsLostOrderMult     =  2.5f;   // il comando ri-decide più di rado
    float commsLostReinforceMult =  1.6f;   // i rimpiazzi tardano
};

// Istanza globale unica (inline → una sola definizione fra le TU).
inline GameplayBalance& mutableGameplayBalance()
{
    static GameplayBalance g;
    return g;
}
inline const GameplayBalance& gameplay() { return mutableGameplayBalance(); }

// Carica da JSON, sovrascrivendo SOLO le chiavi presenti (le altre restano al
// default). File assente o JSON invalido → nessun cambiamento (default). True se
// il file è stato letto.
inline bool loadGameplayBalance(const std::string& path)
{
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;
    std::string txt((std::istreambuf_iterator<char>(f)),
                     std::istreambuf_iterator<char>());
    // Salta un eventuale BOM UTF-8 (alcuni editor Windows lo aggiungono).
    // Robustezza in più: nlohmann lo accetta comunque, ma qui il parse fallito
    // degrada in silenzio ai default — meglio non dipendere dal parser.
    if (txt.size() >= 3 && (unsigned char)txt[0] == 0xEF
        && (unsigned char)txt[1] == 0xBB && (unsigned char)txt[2] == 0xBF)
        txt.erase(0, 3);
    nlohmann::json j;
    try { j = nlohmann::json::parse(txt); } catch (...) { return false; }
    if (!j.is_object()) return false;

    GameplayBalance& b = mutableGameplayBalance();
    b.squadBleedoutTime      = j.value("squad_bleedout_time",        b.squadBleedoutTime);
    b.squadReviveRadius      = j.value("squad_revive_radius",        b.squadReviveRadius);
    b.squadReviveTime        = j.value("squad_revive_time",          b.squadReviveTime);
    b.squadReviveHp          = j.value("squad_revive_hp",            b.squadReviveHp);
    b.squadDownLethalHitFrac = j.value("squad_down_lethal_hit_frac", b.squadDownLethalHitFrac);
    b.squadMaxRevives        = j.value("squad_max_revives",          b.squadMaxRevives);
    b.squadRescueThreatRadius = j.value("squad_rescue_threat_radius", b.squadRescueThreatRadius);
    b.squadRescueMaxThreats   = j.value("squad_rescue_max_threats",   b.squadRescueMaxThreats);
    b.commsLostRangeMult     = j.value("comms_lost_range_mult",      b.commsLostRangeMult);
    b.commsLostShareDelay    = j.value("comms_lost_share_delay",     b.commsLostShareDelay);
    b.commsLostOrderMult     = j.value("comms_lost_order_mult",      b.commsLostOrderMult);
    b.commsLostReinforceMult = j.value("comms_lost_reinforce_mult",  b.commsLostReinforceMult);
    return true;
}

// Serializza i valori correnti (per il salvataggio dall'editor).
inline nlohmann::json gameplayBalanceToJson(const GameplayBalance& b)
{
    return {
        {"squad_bleedout_time",        b.squadBleedoutTime},
        {"squad_revive_radius",        b.squadReviveRadius},
        {"squad_revive_time",          b.squadReviveTime},
        {"squad_revive_hp",            b.squadReviveHp},
        {"squad_down_lethal_hit_frac", b.squadDownLethalHitFrac},
        {"squad_max_revives",          b.squadMaxRevives},
        {"squad_rescue_threat_radius", b.squadRescueThreatRadius},
        {"squad_rescue_max_threats",   b.squadRescueMaxThreats},
        {"comms_lost_range_mult",      b.commsLostRangeMult},
        {"comms_lost_share_delay",     b.commsLostShareDelay},
        {"comms_lost_order_mult",      b.commsLostOrderMult},
        {"comms_lost_reinforce_mult",  b.commsLostReinforceMult},
    };
}

} // namespace mini
