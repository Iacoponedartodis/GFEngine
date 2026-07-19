#pragma once

#include <glm/glm.hpp>

namespace mini::config
{

// ── Fisica ────────────────────────────────────────────────────────────────
constexpr float GRAVITY       = -14.0f;   // m/s²
constexpr float STEP_HEIGHT   =   0.55f;  // altezza max scalino percorribile
constexpr float JUMP_IMPULSE  =   6.0f;   // velocità iniziale salto (m/s)

// ── Player ────────────────────────────────────────────────────────────────
constexpr float PLAYER_SPEED  =   5.0f;   // m/s (camminata)
constexpr float PLAYER_HALF_X =   0.35f;
constexpr float PLAYER_HALF_Y =   0.85f;  // altezza occhi da y=0
constexpr float PLAYER_HALF_Z =   0.35f;

// Veicoli (19_Vehicles, Fase A)
constexpr float VEHICLE_MOUNT_RANGE = 3.5f;   // distanza max per salire (m)
constexpr float VEHICLE_EYE_HEIGHT  = 1.4f;   // altezza camera alla guida

// Gittata (R1): portata massima = grace * effective_range dell'arma
constexpr float WEAPON_RANGE_GRACE  = 2.0f;

constexpr float PLAYER_BULLET_SPEED  = 18.0f;
constexpr float PLAYER_BULLET_DAMAGE = 25.0f;
constexpr float PLAYER_BULLET_LIFE   =  3.0f;

// ── AI ────────────────────────────────────────────────────────────────────
constexpr float AI_HALF_X     = 0.40f;
constexpr float AI_HALF_Y     = 0.50f;
constexpr float AI_HALF_Z     = 0.40f;
constexpr float AI_GRAVITY    = -14.0f;
constexpr float AI_STUCK_TIME =   1.2f;   // secondi prima di anti-stuck
constexpr float AI_JUMP_IMPULSE = 5.5f;   // salto anti-ostacolo (se jump_enabled)
constexpr float AI_SPREAD_MAX   = 0.12f;  // dispersione colpi AI a accuracy 0 (rad)

// ── Combat ────────────────────────────────────────────────────────────────
constexpr float HIT_RADIUS    = 0.7f;     // raggio collisione proiettile

// ── Squadra: stato "a terra" + rianimazione (Phase C, doc 26) ───────────────
// Valori segnaposto da rifinire provando (il todo: "impostare valori temporanei
// da rifinire più avanti"). Globali, non per-definizione → stanno qui (CLAUDE.md).
constexpr float SQUAD_BLEEDOUT_TIME = 20.0f;  // s a terra prima della morte definitiva
constexpr float SQUAD_REVIVE_RADIUS =  2.5f;  // distanza per rianimare un compagno (m)
constexpr float SQUAD_REVIVE_TIME   =  3.0f;  // s di vicinanza per completare la rianimazione
constexpr float SQUAD_REVIVE_HP     =  0.5f;  // frazione di HP max al risveglio

// Command post: ogni post posseduto da una squadra RALLENTA il respawn della
// squadra AVVERSARIA di questa frazione (additivo per post). Sostituisce il
// vecchio "ticket bleed a tempo": conquistare posti non consuma le riserve
// nemiche, le fa RIENTRARE più lentamente (doc 25/GDD 5.4, direttiva utente
// 2026-07-18). Es. 0.15 + 3 post nemici → respawn +45%.
constexpr float POST_RESPAWN_SLOW = 0.15f;

// Ruota di comando: quanto rallenta il tempo di GIOCO mentre è aperta (stile
// Bannerlord). NON pausa: TUTTO il gioco avanza a questa frazione della velocità
// reale — simulazione (AI, proiettili) E giocatore (movimento, cadenza dell'arma,
// guida). Solo UI, camera e selezione della ruota restano a velocità reale, così
// si sceglie l'ordine senza perdere tempo prezioso ma con un minimo di immersione.
constexpr float WHEEL_TIME_SCALE    = 0.15f;  // ~1/7 → tempo molto rallentato (non pausa)

// ── Camera ────────────────────────────────────────────────────────────────
constexpr float CAMERA_FOV    = 60.0f;    // gradi
constexpr float CAMERA_NEAR   =  0.1f;
constexpr float CAMERA_FAR    = 100.0f;

// ── Frame pacing (Fase 2 ottimizzazione) ───────────────────────────────────
// Cap FPS di SICUREZZA usato SOLO quando la VSync è spenta: senza pacing GPU
// il main loop girerebbe a migliaia di FPS bruciando CPU/GPU. Con VSync ON
// (default) è inerte. Alto di proposito: non limita i monitor ad alto refresh.
constexpr int   MAX_UNCAPPED_FPS = 300;

// ── Conteggio AI (stress test / profiling Fase 3-4) ─────────────────────────
// Cap massimo di unità AI per team (sim e partita). Alzato a 50 per poter
// profilare a scala con Tracy la ricerca target (O(N²)) e la collisione, e
// decidere sui dati se la griglia spaziale / time-slicing della Fase 4 serve.
constexpr int   MAX_AI_PER_TEAM = 50;

// ── Time-slicing AI (Fase 4) ────────────────────────────────────────────────
// Ogni AI esegue la sensing pesante (ricerca target O(N²) + LOS) 1 tick su
// AI_SENSE_INTERVAL, scaglionata per entità → costo sensing ~/INTERVAL. Il
// movimento e lo sparo restano ogni tick (sul bersaglio cachato). 6 ≈ 10 Hz a
// 60 Hz: reattività ancora alta. Più alto = più economico ma più latente.
constexpr int   AI_SENSE_INTERVAL = 6;

// ── Cap LOS per sensing (Fase 4b) ───────────────────────────────────────────
// Su questa mappa aggroRange (~20 m) ≈ dimensione mappa (50×40), quindi una
// griglia spaziale non poterebbe nulla (un query 3×3 copre tutto). Il costo è
// LOS-bound nella mischia. Si limita quindi la LOS ai K bersagli PIÙ VICINI:
// l'AI ingaggia un nemico vicino visibile invece dello stretto più vicino
// globale → costo LOS O(N·K) invece di O(N²), comportamento ~identico.
constexpr int   AI_MAX_LOS_CHECKS = 8;

// Helper per avere i glm::vec3 a runtime (non constexpr in GLM pre-1.0)
inline glm::vec3 playerHalf() { return {PLAYER_HALF_X, PLAYER_HALF_Y, PLAYER_HALF_Z}; }
inline glm::vec3 aiHalf()     { return {AI_HALF_X, AI_HALF_Y, AI_HALF_Z}; }

} // namespace mini::config