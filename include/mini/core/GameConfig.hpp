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

// Raggio entro cui un contatto avvistato viene condiviso coi compagni (m).
// Prima l'avvistamento era propagato a TUTTO l'esercito: bastava che uno vedesse
// un nemico perché ogni unità del team convergesse sullo stesso punto → un unico
// blocco, sempre lo stesso fronte, ogni partita identica (feedback utente
// 2026-07-20). Con un raggio locale nascono fronti INDIPENDENTI: uno scontro a
// ovest non risucchia chi presidia a est, che continua il suo compito.
// Rivisto 2026-07-20 (misurato): a 20 m su una mappa 50×40 il raggio copriva
// quasi tutto il campo, quindi un contatto qualsiasi richiamava comunque l'intera
// forza → telemetria: alert 6-7 su 9, hunt e search SEMPRE 0, cioè tutti in
// contatto permanente nello stesso punto. A 10 m gli scontri restano locali e
// chi è altrove continua il proprio compito.
constexpr float AI_CONTACT_SHARE_RADIUS = 10.0f;
constexpr float AI_JUMP_IMPULSE = 5.5f;   // salto anti-ostacolo (se jump_enabled)
// Quanti cloni un singolo segnale della torre di controllo può attirare prima di
// considerarsi "coperto" (KI #73). Oltre, i cloni in più vanno su altri segnali o
// tornano alla pattuglia invece di ammassarsi tutti sullo stesso posto. Basso =
// più dispersione. Taratura di sensazione, verificabile solo in partita.
constexpr int   ALLY_SIGNAL_CAPACITY = 3;
// Quanto un'unità insegue un contatto perduto prima di degradare a Search
// (KI #68). Ora è **per-profilo** (`AiProfileDef::huntTimeout`, doc 16): è una
// scelta di carattere, non una costante globale. Questo valore resta come DEFAULT
// documentato per i profili che non lo specificano. 20 s = doppio della sosta in
// Search (15 s): inseguire deve durare più del cercare, ma non per sempre.
constexpr float AI_HUNT_TIMEOUT_DEFAULT = 20.0f;  // s

// ── Rete di comunicazione (doc 34, ADR-038) ───────────────────────────────
// Quanto peggiora la comunicazione di una fazione che ha PERSO la sua torre.
// MIGRATI a `data/config/gameplay.json` (ADR-043): autorabili dal BalanceEditor.
// Vedi `mini::gameplay()`. Default lì: comms_lost_range_mult 0.5,
// comms_lost_share_delay 2.5, comms_lost_order_mult 2.5, comms_lost_reinforce_mult 1.6.
// (Nessuno azzera nulla: la direttiva è "rallentare", mai bloccare — doc 34.)
// Ogni quanto il comando rivaluta la direttiva strategica (doc 32). Prima era
// OGNI TICK: istantaneo e quindi impossibile da rallentare. Un periodo esplicito
// serve sia al realismo (un ordine non cambia 60 volte al secondo) sia come leva
// su cui la rete di comunicazione può agire.
constexpr float COMMAND_DECISION_PERIOD   = 3.0f;  // s, con comunicazioni intatte
// Per quanto un contatto resta utilizzabile DOPO essere arrivato. Con la torre
// viva (ritardo 0) la finestra utile è 0..FRESH secondi, cioè in pratica gli
// avvistamenti correnti: il comportamento nominale resta quello di prima, quando
// i contatti venivano ricostruiti da zero ogni tick. Senza torre la finestra si
// sposta in avanti (DELAY..DELAY+FRESH) senza allargarsi: non si sa di PIÙ, si sa
// più TARDI.
constexpr float COMMS_CONTACT_FRESH       = 1.0f;  // s
// Cap duro di vita di un contatto: deve superare DELAY + FRESH (2.5 + 1.0).
constexpr float COMMS_CONTACT_TTL         = 4.0f;  // s
// Deduplica dei campioni: senza, OGNI unità che vede OGNI tick di sensing
// aggiunge un contatto → misurati 1066 contatti vivi in 10v10, cioè migliaia di
// confronti per tick per un'informazione che si ripete. Un nuovo campione si
// registra solo se in quell'area non ce n'è già uno recente: il flusso resta
// continuo (serve a nutrire il canale ritardato) ma smette di duplicarsi.
constexpr float COMMS_CONTACT_MERGE_DIST  = 3.0f;  // m
constexpr float COMMS_CONTACT_MERGE_AGE   = 0.3f;  // s
constexpr float AI_SPREAD_MAX   = 0.12f;  // dispersione colpi AI a accuracy 0 (rad)

// ── Combat ────────────────────────────────────────────────────────────────
constexpr float HIT_RADIUS    = 0.7f;     // raggio collisione proiettile

// ── Squadra: stato "a terra" + rianimazione (Phase C, doc 26) ───────────────
// MIGRATI a `data/config/gameplay.json` (ADR-043): ora sono AUTORABILI dal
// BalanceEditor, non più `constexpr`. Vedi `mini::gameplay()` in
// `mini/game/data/GameplayBalance.hpp`. I default lì SONO questi valori:
//   squad_bleedout_time 15, squad_revive_radius 2.5, squad_revive_time 10,
//   squad_revive_hp 0.15, squad_down_lethal_hit_frac 0.20, squad_max_revives 1.

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