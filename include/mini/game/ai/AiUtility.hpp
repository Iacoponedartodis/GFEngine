#pragma once
// ── AiUtility — i PESI delle decisioni tattiche, in un posto solo (doc 40 A5) ─
//
// PERCHÉ ESISTE. L'AI di questo progetto è, di fatto, una **Utility AI**: ogni scelta
// (quale copertura, quale posizione di tiro, quale fronte) nasce da un punteggio che
// pesa protezione, importanza autorata, distanza, pericolo ed esposizione. Ma quei pesi
// erano **numeri magici sparsi in nove formule su due file**: nessuno poteva vederli
// insieme, quindi nessuno poteva rispondere a domande come "perché l'AI preferisce
// coprirsi invece di avanzare?" o "quanto conta l'importanza rispetto al riparo?".
// Tararli significava cercarli a grep e cambiarli a mano, sperando di trovarli tutti.
//
// COSA NON FA: non cambia i valori. Sono ESATTAMENTE quelli già in uso — questo è un
// refactor a comportamento invariato (verificato con `--sim-ticks`). Cambiare le curve
// è un'altra cosa, si fa dopo e si MISURA: mescolare le due avrebbe reso impossibile
// dire se una differenza viene dal refactor o dalla nuova formula.
//
// COME LEGGERLO: ogni query tattica risponde a una domanda diversa, quindi ha un suo
// bilancio. Le differenze fra i blocchi qui sotto SONO il design: `bestHoldPosition`
// pesa protezione e importanza alla pari (presidiare), `bestAdvantageInArea` premia
// l'importanza (occupare il buon terreno), `bestFlankingPosition` premia l'angolo
// nuovo (aggirare). Vederle affiancate rende quelle scelte discutibili invece che
// implicite.

#include <cmath>

namespace mini
{
namespace aiutility
{

// ── CURVE DI RISPOSTA (doc 40 §6, A5 parte 3) ───────────────────────────────
// Fino a qui ogni termine entrava nel punteggio in modo LINEARE: un'importanza 4
// valeva esattamente quattro volte un'importanza 1, e un'esposizione dello 0,9
// costava il triplo di una da 0,3. Sono due cose che il buon senso tattico nega.
//
// La composizione resta ADDITIVA (`U = Σ wᵢ·fᵢ`) e non moltiplicativa: un fattore
// a zero azzererebbe tutto e produrrebbe unità che non fanno nulla — difetto già
// visto in questo progetto quando l'importanza veniva schiacciata (KI #81).
// Cambia solo la FORMA di ogni singolo termine.

// ⚠ Valore del terreno: curva concava **PROVATA E RIFIUTATA** (2026-08-04).
// Doc 40 §6 raccomandava `imp^0.7` (rendimenti decrescenti). L'ho implementata e
// misurata con A/B sullo stesso binario, e NON conviene:
//
//   | metrica              | lineare | ^0.85 | ^0.7 |
//   |----------------------|---------|-------|------|
//   | acquisizioni         |   2362  |  3272 | 3298 |
//   | colpi sparati        |    379  |   440 |  415 |
//   | **eventi combat**    | **265** |   222 |  211 |
//
// Più acquisizioni e più colpi, ma **meno colpi a segno**: comprimendo l'importanza
// le unità scelgono posizioni più vicine e meno buone, sparano di più e colpiscono
// di meno. E soprattutto va **contro il modello del progetto**: l'importanza è la
// dichiarazione tattica dell'AUTORE (ADR-030/033), il segnale con cui il mondo
// guida AI semplici — attenuarlo sposta la decisione dal designer alla formula.
//
// Lasciata qui come RECORD, non come trappola: chi la ritenta sappia che è già
// stata misurata. Se un giorno l'importanza venisse usata su scale molto più
// ampie, il problema tornerebbe e la risposta giusta sarebbe normalizzare i dati,
// non curvare il punteggio.
inline float terrainValue(float importance) { return importance; }   // lineare, per misura

// Rischio: CONVESSA, punisce l'esposizione ALTA. Lineare, una posizione un po'
// esposta veniva scartata quasi quanto una esposta a mezza mappa; così invece la
// penalità è quasi nulla fino a metà scala e poi morde. 0,3 → 0,16 · 0,9 → 0,85.
// ADOTTATA, ma va detto: misurata, **non cambia nulla oggi** (265 eventi identici
// al lineare, acquisizioni e colpi allo stesso numero). Tocca solo aggiramento e
// overwatch, che in 6000 tick capitano 5 volte. Tenuta perché è la forma giusta e
// costa zero — conterà quando l'aggiramento sarà frequente, non prima. Il valore
// di questa riga oggi è sapere che è inerte, invece di crederla efficace.
inline float riskPenalty(float exposure)
{
    return (exposure > 0.0f) ? std::pow(exposure, 1.5f) : 0.0f;
}

// Prossimità: IPERBOLICA, crolla con la distanza invece di calare piano. `d0` è
// la distanza a cui il valore si dimezza. Serve dove "vicino" è una preferenza
// forte ma "lontano" non deve diventare un veto: oltre d0 la differenza fra 30 m
// e 40 m smette di contare, che è come ragiona chi sceglie dove andare.
inline float proximity(float dist, float d0)
{
    const float r = (d0 > 0.001f) ? (dist / d0) : dist;
    return 1.0f / (1.0f + r * r);
}

// ── Copertura DIFENSIVA: "dove mi riparo" (bestCoverToward) ─────────────────
struct CoverW {
    float protection = 1.0f;    // il riparo è tutto
    float distance   = 0.5f;    // normalizzata su maxDist²
    float danger     = 1.0f;    // ADR-046/C3: una cover dentro una danger non ripara
};

// ── AGGIRAMENTO: "da dove lo colpisco da un'altra direzione" (bestFlankingPosition)
struct FlankW {
    float flankBonus = 1.5f;    // l'angolo NUOVO è lo scopo della manovra
    float protection = 0.6f;
    float arc        = 0.4f;    // orientamento: preferenza, non esclusione (ADR-031)
    float unexposed  = 0.6f;    // (1 - esposizione): preferisci il coperto
    float distance   = 0.4f;    // ma non attraversare la mappa
};

// ── OVERWATCH: "da dove copro il compagno che avanza" (bestOverwatchForPosition)
struct OverwatchW {
    float protection = 2.0f;    // chi copre deve sopravvivere: il riparo domina
    float importance = 1.0f;
    float distance   = 0.08f;   // su distanza lineare
    float exposure   = 1.0f;
};

// ── POSIZIONE DI TIRO: "da dove lo colpisco restando coperto" (bestFiringPosition)
struct FiringW {
    float protection = 1.0f;
    float arc        = 0.5f;
    float importance = 0.5f;
    float distance   = 0.5f;    // normalizzata su maxDist²
    float danger     = 1.0f;
};

// ── PRESIDIO: "quale posizione TENGO" (bestHoldPosition) ────────────────────
struct HoldW {
    float protection = 1.0f;    // alla pari con l'importanza: presidiare è restare vivi
    float importance = 1.0f;    //   su un punto che conta
    float distance   = 0.05f;
    float danger     = 1.0f;
};

// ── BUON TERRENO: "dove mi conviene stare in quest'area" (bestAdvantageInArea)
struct AdvantageW {
    float importance = 1.5f;    // premia il terreno che l'AUTORE ha marcato come buono
    float protection = 1.0f;
    float distance   = 0.05f;
    float danger     = 1.0f;
};

// ── QUADRO TATTICO della torre (updateAllyTactical / bestOrderPosition) ─────
struct TacticalPictureW {
    float importance = 1.5f;
    float protection = 1.0f;
    float danger     = 1.0f;
    float distance   = 0.05f;   // a parità di valore, la più vicina
    // `fireBonus` vive in GameConfig (TAC_FIRE_BONUS): è una leva di GAMEPLAY già
    // esposta, non un peso interno — chi tara vuole trovarla lì.
};

// ── SETTORE: "quanto conta questo fronte" (sectorTacticalWeight) ────────────
// Condiviso da torre di controllo e Droide Tattico (changelog 88): un solo bilancio,
// così i due lati non possono divergere.
struct SectorW {
    float importance = 1.0f;    // valore autorato: la base
    float pressure   = 2.0f;    // LA CONTESA COMANDA: dove si combatte le forze affluiscono
    float minority   = 0.8f;    // per unità di svantaggio numerico → rinforza
    float enemyHeld  = 0.6f;    // terreno in mano al nemico → riprenderlo
    float opportunity= 0.4f;    // valore alto e poco difeso → sfruttarlo
    float holdBonus  = 0.5f;    // difendere un obiettivo conteso (solo comandante)
};

inline constexpr CoverW           kCover{};
inline constexpr FlankW           kFlank{};
inline constexpr OverwatchW       kOverwatch{};
inline constexpr FiringW          kFiring{};
inline constexpr HoldW            kHold{};
inline constexpr AdvantageW       kAdvantage{};
inline constexpr TacticalPictureW kPicture{};
inline constexpr SectorW          kSector{};

} // namespace aiutility
} // namespace mini
