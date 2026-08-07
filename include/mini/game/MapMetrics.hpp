#pragma once

#include "mini/core/GameConfig.hpp"

// ── Metriche di costruzione delle mappe (doc 47 §4, ADR-053) ────────────────
// LE MISURE NORMATIVE con cui si costruisce una mappa di Galactic Front.
// Sorgente UNICA di verità per tre consumatori: il gate `--validate`, il Map Editor
// (primitive parametriche e verifica dal vivo) e chi genera geometria.
//
// PERCHÉ ESISTE QUESTO FILE. Fino al 2026-08-05 non c'era nessun numero da rispettare,
// e questa è la ragione STRUTTURALE per cui le scale di Training Ground sono venute
// con alzate da 0,68-1,21 m contro un massimo di 0,55: l'autore non aveva sbagliato
// un numero, non ne aveva nessuno.
//
// ── DOVE STA IL MARGINE, E PERCHÉ STA LÌ ───────────────────────────────────
// `kAgentRadius`/`kAgentHeight` del navmesh sono costanti GLOBALI e il navmesh si
// costruisce UNA volta, per UNA taglia: non esiste un navmesh per classe. Quindi
// un'unità più larga di quella con cui il navmesh è stato costruito **non entra**
// nei passaggi, e non c'è nulla che l'AI possa fare a runtime.
//   → Il margine va messo nella GEOMETRIA, non in queste costanti.
//     Cambiare un raggio è una riga; allargare i corridoi di una mappa da 60.000 m²
//     già costruita è rifarla.
// Perciò le misure qui sotto sono dimensionate su un GIGANTE DI RIFERIMENTO più
// grande di qualunque unità esistente oggi, mentre il motore resta tarato sull'unità
// attuale. Quando servirà davvero: si alza la costante (una riga, e la mappa lo
// regge già) oppure si costruisce un secondo navmesh per taglia.
//
// Taglie reali MISURATE il 2026-08-05 (hitbox × mesh_scale, non dedotte):
//   Clone Trooper  1,98 m di altezza, busto 0,33 m
//   B1 Battle Droid 2,03 m di altezza, busto 0,30 m

namespace mini::mapmetrics
{

// ── L'agente di OGGI (quello con cui si costruisce il navmesh) ──────────────
// Vive qui e non più dentro NavManager.cpp perché serve a due consumatori: la
// costruzione del navmesh e il gate, che deve sapere quanta altezza libera serve
// per starci in piedi. Due copie diverse = due verità diverse sullo stesso mondo.
// AGENT_HEIGHT era 1,80 — più BASSO dei modelli (1,98 e 2,03): il navmesh dichiarava
// percorribile un sottopasso in cui la testa passava dentro il soffitto.
inline constexpr float AGENT_HEIGHT = 2.10f;
inline constexpr float AGENT_RADIUS = config::AI_HALF_X;   // 0,40

// ── Il gigante di riferimento ───────────────────────────────────────────────
// +18% in altezza e +50% in larghezza sull'unità di oggi: copre Super Battle Droid,
// Magnaguard, Droideka in cammino, Wookiee.
inline constexpr float REF_UNIT_HEIGHT = 2.40f;   // m
inline constexpr float REF_UNIT_WIDTH  = 1.20f;   // m

// ── Costruzione ─────────────────────────────────────────────────────────────
inline constexpr float GRID_SNAP    = 0.50f;   // griglia di lavoro
inline constexpr float MODULE_STEP  = 2.00f;   // i moduli stanno su multipli di 2 m

// ── Scale e rampe ───────────────────────────────────────────────────────────
// L'alzata è un multiplo esatto di `kCellHeight` (0,10) del navmesh: così il gradino
// che vedi è il gradino che Recast costruisce, senza arrotondamenti.
// 0,20 / 0,30 = 33,7°, dentro la banda 30-35° usata da tutta la letteratura.
inline constexpr float STAIR_RISER      = 0.20f;   // alzata standard
inline constexpr float STAIR_TREAD      = 0.30f;   // pedata minima
inline constexpr float RAMP_RISER       = 0.10f;   // rampa dolce (veicoli): 14° con 0,40
inline constexpr float RAMP_TREAD       = 0.40f;
inline constexpr float STAIR_MIN_WIDTH  = 1.60f;   // ci passa il gigante + margine
// Una rampa di VANO SCALA non è una scala qualsiasi: è fiancheggiata dal vuoto (o
// dalla corsia accanto, più bassa) su entrambi i lati. Recast le toglie le celle di
// bordo come strapiombo (`rcFilterLedgeSpans`), poi erode il raggio agente per lato,
// poi scarta le regioni sotto `minRegionArea` — e a 1,60 m non resta abbastanza:
// **misurato il 2026-08-05, una torre a tre rampe da 1,60 si interrompe alla terza;
// la stessa a 2,40 è percorribile fino in cima.** Quindi una corsia di vano scala
// vuole la larghezza di un CORRIDOIO, non quella di una scala.
inline constexpr float STAIRWELL_MIN_WIDTH = 2.40f;   // = CORRIDOR_MIN

// ── Il minimo di una superficie SOPRAELEVATA ────────────────────────────────
// Ricavato dai filtri di Recast, non scelto a occhio. Una superficie in quota
// perde, prima di diventare navmesh:
//   · `rcFilterLedgeSpans` — una cella per lato, perché oltre il bordo c'è il vuoto;
//   · `rcErodeWalkableArea` — `AGENT_RADIUS` (0,40) per lato;
//   · `minRegionArea` — le regioni sotto ~2,56 m² vengono scartate del tutto.
// Per un ripiano quadrato di lato s resta (s − 1,20)², che deve superare 2,56 m²
// → **s ≥ 2,80**. Sotto quella misura il ripiano semplicemente non esiste per l'AI,
// pur essendo perfetto nei dati — ed è esattamente il modo in cui un difetto passa
// inosservato. Si arrotonda a 3,00 per stare larghi.
inline constexpr float ELEVATED_MIN_SPAN = 3.00f;

// ── La FASCIA PROIBITA ──────────────────────────────────────────────────────
// Sopra `STEP_HEIGHT` l'AI si ferma: NON salta (il ramo di salto è dietro
// `!useCrowd`, e col navmesh attivo — cioè sempre in partita — non viene mai
// eseguito). Il giocatore invece supera fino a PLAYER_JUMP_REACH.
// Un dislivello in mezzo è un posto dove il giocatore sale e l'AI resta a sbattere.
inline constexpr float LEDGE_BAND_MIN = config::STEP_HEIGHT;              // 0,55
// v²/(2g) con JUMP_IMPULSE 6,0 e GRAVITY 14,0 → 1,286 m
inline constexpr float PLAYER_JUMP_REACH =
    (config::JUMP_IMPULSE * config::JUMP_IMPULSE) / (2.0f * -config::GRAVITY);
inline constexpr float LEDGE_BAND_MAX = PLAYER_JUMP_REACH;

// ── Passaggi ────────────────────────────────────────────────────────────────
// Il corridoio deve ospitare il gigante DOPO l'erosione del navmesh, che toglie
// `kAgentRadius` per lato: con un raggio da gigante (0,60) un corridoio da 2,0 m
// lascerebbe 0,8 m di navmesh — ci si passa a stento e il crowd si incastra.
inline constexpr float CORRIDOR_MIN   = 2.40f;   // REF_UNIT_WIDTH + 2 × 0,60
inline constexpr float CORRIDOR_MAIN  = 3.60f;   // due unità di fronte
inline constexpr float DOOR_WIDTH     = 1.80f;   // gigante + mezzo metro anti-incastro
inline constexpr float DOOR_HEIGHT    = 2.80f;   // gigante + 0,40 di franco
inline constexpr float CEILING_MIN    = 2.80f;   // altezza libera al coperto

// ── Volumi ──────────────────────────────────────────────────────────────────
inline constexpr float WALL_HEIGHT    = 3.20f;   // copre il gigante, non scavalcabile
// Spessore standard di un muro. 0,40 risultava visibilmente grosso in gioco
// (segnalato dall'utente): 0,25 è più vicino a un muro vero e resta ben sopra la
// cella del navmesh (0,20), che è il limite sotto il quale un muro rischia di non
// essere voxelizzato come ostacolo continuo.
inline constexpr float WALL_THICKNESS = 0.25f;
inline constexpr float COVER_LOW      = 1.00f;   // ripara accovacciati, ci si sporge sopra
inline constexpr float COVER_HIGH     = 1.70f;   // ripara in piedi anche il gigante

// ── Ritmo ───────────────────────────────────────────────────────────────────
// Distanza spawn→obiettivo espressa in SECONDI di cammino: è la metrica che conta
// per il ritmo, e diventa verificabile col campo di distanza di percorso (doc 46).
inline constexpr float PACE_MIN_S = 8.0f;
inline constexpr float PACE_MAX_S = 12.0f;

// Un dislivello sta nella fascia in cui il giocatore sale e l'AI no?
inline constexpr bool inLedgeTrap(float rise)
{
    return rise > LEDGE_BAND_MIN && rise <= LEDGE_BAND_MAX;
}

// Quanti gradini servono per superare `rise` senza mai eccedere `riser`?
inline constexpr int stepsFor(float rise, float riser = STAIR_RISER)
{
    if (rise <= 0.0f || riser <= 0.0f) return 0;
    int n = (int)(rise / riser);
    if (rise - (float)n * riser > 1e-4f) ++n;
    return n < 1 ? 1 : n;
}

} // namespace mini::mapmetrics
