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
struct CoverPointDef;
struct TacticalPointDef;

namespace worldintel
{

// Miglior cover point (entro maxDist, XZ) il cui fronte guarda verso (towardX,
// towardZ). "Miglior" = punteggio che pesa `protection` meno una penalità di
// distanza (Cover Intelligence, ADR-026): l'AI sceglie la copertura più PROTETTIVA,
// non solo la più vicina. Con protezione uniforme degenera nella più vicina
// (retrocompatibile). nullptr se nessuna qualifica. Ritorna il def completo.
const CoverPointDef* bestCoverToward(const MapDef& map,
                                     float x, float z,
                                     float towardX, float towardZ,
                                     float maxDist);

// Livello di pericolo aggregato in (x,z): somma dei dangerLevel pesati sulla
// vicinanza al centro di ogni danger zone che copre il punto. 0 = sicuro.
float dangerAt(const MapDef& map, float x, float z);

// Tactical point più vicino (entro maxDist, XZ) del tipo dato (Fase 2, ADR-027).
// type vuoto = qualunque tipo. nullptr se nessuno. Seam per i consumatori futuri
// (Squad/settori): oggi nessun sistema AI lo chiama ancora.
const TacticalPointDef* nearestTacticalPoint(const MapDef& map, float x, float z,
                                             const char* type, float maxDist);

} // namespace worldintel
} // namespace mini
