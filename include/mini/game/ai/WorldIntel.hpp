#pragma once

// ── World Intelligence Layer (doc 33, ADR-025) ──────────────────────────────
// Il seam UNICO con cui AI/squadre interrogano la conoscenza tattica della mappa.
// SOLO dati + query pure su MapDef: nessuna logica AI qui (decoupling, doc 15).
// Oggi copre cover e danger (Fase 0); crescerà con tactical points, rete tattica
// e settori (doc 33). Scansione lineare per ora — il seam permette di aggiungere
// un indice spaziale senza toccare i chiamanti.

namespace mini
{
struct MapDef;
struct TacticalPositionDef;

namespace worldintel
{

// Miglior posizione che RIPARA (protection > 0) entro maxDist il cui fronte
// guarda verso (towardX, towardZ). "Miglior" = punteggio che pesa `protection`
// meno una penalità di distanza (ADR-026): l'AI sceglie la copertura più
// PROTETTIVA, non solo la più vicina. Filtra per CAPACITÀ, non per ruolo: una
// `vantage` che ripara vale come copertura (ADR-030). nullptr se nessuna qualifica.
const TacticalPositionDef* bestCoverToward(const MapDef& map,
                                           float x, float z,
                                           float towardX, float towardZ,
                                           float maxDist);

// Livello di pericolo aggregato in (x,z): somma dei dangerLevel pesati sulla
// vicinanza al centro di ogni danger zone che copre il punto. 0 = sicuro.
float dangerAt(const MapDef& map, float x, float z);

// Linea di tiro libera fra due punti? (ADR-032) Segmento contro i box `collider`
// della mappa, slab test nel frame locale del box (gestisce la rotazione `ry`).
// Lavora su MapDef e NON sul World: serve al load (grafo dei link) e ovunque il
// World non esista. `physics::hasLineOfSight` resta quello runtime sui collider.
bool hasLineOfFire(const MapDef& map,
                   float ax, float ay, float az,
                   float bx, float by, float bz);

// Costruisce il grafo "chi copre chi" (ADR-032): per ogni posizione, l'elenco
// delle posizioni che copre (dentro settore + gittata + linea di tiro libera).
// DATO DERIVATO: si ricalcola a ogni load, non si autora e non si salva → non può
// diventare stale. Chiamata dal registry dopo il parse della mappa.
void buildTacticalLinks(MapDef& map);

// Posizione da cui AGGIRARE (ADR-033): fra quelle che possono battere il bersaglio,
// quella che lo attacca da una direzione DIVERSA rispetto a dove è già ingaggiato
// (threat = da dove arriva la pressione attuale, tipicamente chi cerca) e che è
// meno ESPOSTA. È la "corsia di aggiramento" espressa come destinazione invece che
// come tracciato da autorare. nullptr se nessuna qualifica.
const TacticalPositionDef* bestFlankingPosition(const MapDef& map,
                                                float fromX, float fromZ,
                                                float targetX, float targetZ,
                                                float threatX, float threatZ,
                                                float maxDist);

// Bounding overwatch che USA IL GRAFO (ADR-032): fra le posizioni che, per il grafo
// `positionCovers`, coprono la posizione tattica `coveredIdx` verso cui un compagno
// sta avanzando, la migliore raggiungibile entro maxDist (vicina, protetta, poco
// esposta). È il consumatore che mancava a `positionCovers` — restituisce anche
// l'INDICE via `outIdx` per non ricalcolarlo. nullptr se nessuna qualifica.
const TacticalPositionDef* bestOverwatchForPosition(const MapDef& map,
                                                    float fromX, float fromZ,
                                                    int coveredIdx,
                                                    float maxDist,
                                                    int* outIdx = nullptr);

// Miglior POSIZIONE DI TIRO (ADR-031): fra quelle raggiungibili entro maxDist da
// (x,z), la migliore che può davvero BATTERE (targetX,targetZ) — cioè `canShoot`,
// bersaglio entro `fireRange` e dentro il settore `fireArcDeg` centrato su
// `facingDeg`. Punteggio: premia la protezione, penalizza la distanza da chi cerca.
// È la domanda OFFENSIVA ("da dove lo colpisco restando coperto"), distinta da
// bestCoverToward che è quella DIFENSIVA ("dove mi riparo"). nullptr se nessuna.
// NB: filtro geometrico — non verifica ostacoli fra posizione e bersaglio (M4).
const TacticalPositionDef* bestFiringPosition(const MapDef& map,
                                              float x, float z,
                                              float targetX, float targetZ,
                                              float maxDist);

// (x,z) è dentro/vicino a un command post? `slack` allarga il raggio del post.
// Serve a distinguere un waypoint "su obiettivo" (dove ha senso sostare a lungo
// per catturare) da un waypoint qualunque della route (dove sostare a lungo
// bloccherebbe la pattuglia).
bool nearCommandPost(const MapDef& map, float x, float z, float slack);

// Posizione tattica più vicina (entro maxDist, XZ) col ruolo dato (ADR-030).
// role vuoto = qualunque ruolo. nullptr se nessuna.
// Miglior posizione da TENERE (ADR-046): fra le `defensive`/`chokepoint` dentro
// l'area (settore che il comandante vuole presidiare), la migliore per
// protezione + importanza, vicina e fuori dalle danger zone. Dà comportamento ai
// due ruoli che prima erano decorativi. nullptr se nessuna qualifica.
const TacticalPositionDef* bestHoldPosition(const MapDef& map, float x, float z,
                                            float areaX, float areaZ, float areaRadius);

// Miglior posizione VANTAGGIOSA in un'area (doc 36): fra cover/vantage/defensive/
// chokepoint dentro l'area, la migliore per IMPORTANZA autorata + protezione,
// vicina e fuori dal pericolo. Serve a far OCCUPARE proattivamente il buon terreno
// (anche elevato, marcato con importanza alta) quando un'unità avanza su un fronte,
// invece di stare al centro del settore. Distinta da bestFiringPosition (che serve
// un bersaglio) e da bestHoldPosition (solo difensive/chokepoint). nullptr se nessuna.
const TacticalPositionDef* bestAdvantageInArea(const MapDef& map, float x, float z,
                                               float areaX, float areaZ, float areaRadius);

const TacticalPositionDef* nearestPositionByRole(const MapDef& map, float x, float z,
                                                 const char* role, float maxDist);

} // namespace worldintel
} // namespace mini
