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

namespace mini
{
namespace aiutility
{

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
