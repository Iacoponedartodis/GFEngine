# 37 — Audit dei sistemi nuovi (ADR-024 → ADR-040)

> **Analisi richiesta dall'utente il 2026-07-21**, prima di procedere a rendere tutto autorabile:
> *"ci vuole una bella analisi profonda del progetto, focalizzandoci sulle cose nuove, per verificare
> che non ci siano errori, che sia tutto ben collegato e integrato, che la documentazione sia a
> posto, e solo dopo aver fatto questo procediamo a rendere tutto il più autorabile possibile"*.
>
> Perimetro: i sistemi aggiunti fra il 2026-07-19 e il 2026-07-21 — comando nemico (ADR-024), catena
> metadata (ADR-025→035), strutture (036/039), stato privo di ordini (037), rete di comunicazione
> (038), torre di controllo (040).

## Sintesi

Trovati **3 bug di correttezza** (2 corretti subito, 1 aperto), **1 sistema costruito e mai
consumato**, **3 buchi di validazione** e **1 correzione di documentazione**. Il quadro generale è
sano: i sistemi sono collegati da mailbox coerenti e nessuno viola i vincoli architetturali (ADR-002
rispettato, nessun id hardcoded introdotto, nessun salvataggio non-RMW).

Il problema strutturale non è la qualità dei singoli sistemi ma il **rapporto fra codice e
authoring**: sono state introdotte **17 costanti di gameplay** in `GameConfig.hpp` e **nessuna** è
raggiungibile dall'editor. È esattamente la diagnosi dell'utente sulla rianimazione.

---

## A. Bug di correttezza

### A1. Lo stato per-missione sopravviveva alla partita — CORRETTO
`World::initialize()` azzerava `battleState` e `missionStats` ma **non** `comms`, `enemyCommand`,
`allyIntel`, `sectorStates`, `strategicTargets`. Poiché **i sistemi sopravvivono a `initialize()`**
(registrati una volta in `Application`, commento a `Application.cpp:309`) mentre lo stato del World
no, una fazione che aveva **perso la torre** in una partita iniziava **già degradata** quella dopo:
raggio di condivisione dimezzato, ordini lenti, rinforzi lenti — senza alcuna causa visibile.
→ **Corretto**: `initialize()` azzera anche questi.

### A2. I contatti condivisi attraversavano le partite — CORRETTO
Stessa radice: `AiSystem::m_contacts` è stato del **sistema**, quindi sopravvive. A inizio partita le
unità nascevano "informate" di contatti della battaglia precedente (fino a `COMMS_CONTACT_TTL`).
→ **Corretto**: azzerati al tick 0.

### A3. Con pochi segnali la torre di controllo li ammassa — APERTO (KI #73)
Osservato dall'utente: *"dopo un po' hanno iniziato ad aggregarsi tutti lì vicino"*. La scelta
decorrelata dal `bias` (ADR-040) disperde i cloni **solo se ci sono abbastanza segnali**. A fine
partita i settori tenuti vengono filtrati e resta 1-2 segnali: **tutti i cloni convergono lì**, cioè
proprio il comportamento che ADR-040 esisteva per evitare. E arrivati sul posto, se il segnale è una
struttura che non possono ingaggiare, non hanno nulla da fare e restano a girare intorno.
→ Non corretto qui: la soluzione giusta è di design (un segnale "saturo" smette di attirare, oppure
i cloni tornano a presidiare) e va decisa, non improvvisata.

---

## B. Costruito e mai consumato

### B1. Il grafo "chi copre chi" (ADR-032) è calcolato al load e non lo usa nessuno
`MapDef.positionCovers` è costruito da `buildTacticalLinks` a ogni caricamento mappa, ed è letto solo
da `worldintel::bestOverwatchFor` — che **non è chiamata da nessuna parte**. `AiSystem` usa
`bestFlankingPosition`, `bestFiringPosition`, `nearestPositionByRole`, `bestCoverToward`: mai
l'overwatch.
È il caso che il progetto stesso chiama *"metadato decorativo"* (doc 33 §6, ADR-027). Due uscite
oneste: consumarlo (coppie di overwatch esplicite, già elencate fra i candidati in 06_Todo) oppure
rimuoverlo. **Non lasciarlo com'è**: oggi costa tempo di caricamento e fa credere che l'AI usi un
grafo che non guarda.

`positionExposure` invece **è** consumato (via `bestFlankingPosition`) ed è mostrato in editor: sano.

---

## C. Buchi di validazione (ADR-018) — TUTTI CHIUSI 2026-07-21

> **Chiusi.** Il loader non normalizza più il `role` in silenzio (conserva il grezzo, il gate segnala
> i refusi come **errore**), e sono stati aggiunti i controlli su `hp`, `engage_radius` inerte, torre
> di controllo di team 2, torri duplicate e asimmetria fra fazioni. Alla prima esecuzione il gate ha
> segnalato le 3 strutture di firebase con `engage_radius: 1` — l'errore reale dell'utente.

Il gate copriva bene commander e label dei bersagli. **Non** copriva nulla di ciò che è stato aggiunto
dopo:
1. **`role` non validato**: un refuso (`"comm"`, `"Control"`) viene silenziosamente normalizzato a
   `"generic"` dal loader. L'autore crede di aver messo una torre e non ha messo niente. È
   esattamente il tipo di errore silenzioso che ADR-018 esiste per intercettare.
2. **Nessun controllo di coerenza sulle torri**: due torri di controllo per la stessa fazione (la
   seconda non fa nulla), una torre di controllo di **team 2** (non fa nulla — `allyIntel` è solo
   team 1), una mappa con `role: "comms"` per una sola fazione (asimmetria involontaria).
3. **`engage_radius` / `priority` non commentati**: un raggio di 1 m è formalmente valido e
   praticamente inerte (vedi D2). Un warning "raggio sotto i 3 m: la struttura non verrà mai
   ingaggiata" avrebbe risparmiato all'utente un test a vuoto.

---

## D. Documentazione da correggere

> **Stato 2026-07-21**: D1 corretto **e** il difetto residuo (`Hunt` senza timeout) **risolto**;
> D2 chiuso lato editor (unità di misura, avviso sotto i 3 m, riferimento alla scala della mappa).

### D1. KI #68 era sbagliato — CORRETTO
Avevo scritto che *"la partita non finisce quando una fazione è spazzata via"*. **La partita finisce**;
quello che non si ferma è la **simulazione in sandbox** — e non deve fermarsi, serve all'utente per
osservare. Restava valido solo il secondo difetto: **`Hunt` non scade mai**, quindi un'unità insegue
un contatto inesistente all'infinito invece di degradare a Search e poi a Patrol.

### D2. `engage_radius` è in METRI e l'editor non lo dice
L'utente ha impostato `1` aspettandosi un effetto e ha ottenuto una struttura ingaggiabile solo da
**1 metro**, cioè mai. Lo slider non ha unità di misura né un ordine di grandezza suggerito. Difetto
di authoring, non di codice — da sistemare nella fase successiva.

---

## E. Il problema vero: 17 costanti, zero autorabili

> **Stato 2026-07-21 (ADR-043)**: i due gruppi principali sono CHIUSI — squadra/rianimazione (6) e
> rete di comunicazione (4) vivono in `data/config/gameplay.json`, editabili dal tab **Gameplay** del
> BalanceEditor. `hunt_timeout` è nei profili AI. Le soglie tecniche dei contatti restano non esposte
> di proposito. Restano compile-time: `AI_CONTACT_SHARE_RADIUS`, `COMMAND_DECISION_PERIOD`,
> `ALLY_SIGNAL_CAPACITY`, `AI_STUCK_TIME` — candidabili allo stesso file se servirà tararli.

Tutte introdotte fra ADR-035 e ADR-040, tutte in `GameConfig.hpp`, **nessuna raggiungibile
dall'editor**. CLAUDE.md permette `GameConfig.hpp` per ciò che è "veramente globale", e formalmente
lo sono — ma il risultato pratico è che per bilanciare la rianimazione **bisogna ricompilare**.

| Gruppo | Costanti | Note |
|---|---|---|
| Squadra / rianimazione | `SQUAD_BLEEDOUT_TIME`, `SQUAD_REVIVE_RADIUS`, `SQUAD_REVIVE_TIME`, `SQUAD_REVIVE_HP`, `SQUAD_DOWN_LETHAL_HIT_FRAC`, `SQUAD_MAX_REVIVES` | **caso guida**. La causa "troppo efficace" era una **regola** (rianimazione infinita), non un numero: risolta col cap `SQUAD_MAX_REVIVES` (2026-07-21). Resta da esporre il resto. |
| Rete di comunicazione | `COMMS_LOST_RANGE_MULT`, `COMMS_LOST_SHARE_DELAY`, `COMMS_LOST_ORDER_MULT`, `COMMS_LOST_REINFORCE_MULT` | il *peso* della torre non è autorabile |
| Contatti | `AI_CONTACT_SHARE_RADIUS`, `COMMS_CONTACT_FRESH`, `COMMS_CONTACT_TTL`, `COMMS_CONTACT_MERGE_DIST`, `COMMS_CONTACT_MERGE_AGE` | governano quanto un esercito è "connesso" |
| Comando | `COMMAND_DECISION_PERIOD` | quanto il comandante è reattivo |
| Anti-stuck | `AI_STUCK_TIME` | |

**Non tutte vanno esposte allo stesso modo**, ed è la domanda da porsi nella fase successiva: alcune
sono bilanciamento globale (un pannello nel BalanceEditor), altre andrebbero **per-definizione**
(la rianimazione potrebbe dipendere dalla classe: un medico rianima più in fretta) e altre ancora
sono soglie tecniche che non hanno senso esposte (`COMMS_CONTACT_MERGE_AGE`).

**Altro buco di authoring**: il campo `MapDef.commander` (ADR-024) **non ha alcuna UI** nel MapEditor
— si autora solo a mano nel JSON, pur essendo "un obiettivo vivente per mappa".

---

## F. Cosa invece è sano

- **Mailbox coerenti**: `enemyCommand`, `allyIntel`, `comms`, `sectorStates`, `strategicTargets`,
  `battleState` seguono tutte lo stesso schema (scrittore unico, lettori disaccoppiati). `ecs/` non
  conosce i game mode. ADR-002 rispettato: nessuna dipendenza editor↔engine introdotta.
- **Sorgente unica** rispettata dove conta: `StrategicTargetDef::solidHalfExtents` alimenta collider
  **e** navmesh; `worldintel::buildTacticalLinks` è chiamata sia dal runtime sia dall'editor.
- **Nessun id o costante di gameplay hardcoded nei game mode** introdotto dai lavori nuovi.
- **Non-regressione per default** applicata con metodo: `engage_radius = 0`, `hadTower`,
  `patrolRoute = -1`, assenza di torre → sistemi inerti. Le mappe vecchie non cambiano comportamento.
- **Osservabilità**: ogni sistema nuovo ha contatori in `AI / tactical decisions`. È ciò che ha
  permesso di trovare i tre bug delle strutture (KI #70) invece di ipotizzarli.

---

## G. Stato dell'audit al 2026-07-21

**Chiuso in autonomia** (nessuna decisione di design richiesta):
- A1, A2 — stato per-missione che sopravviveva alla partita.
- C1, C2, C3 — gate di validazione esteso alle strutture.
- D1 (residuo) — `Hunt` ora scade → Search → Patrol.
- D2 — slider dell'editor con unità di misura e avviso.

**Aperto, in attesa di una decisione dell'utente** (elencata a voce, non duplicata qui perché si
risolve subito):
- **A3 / KI #73** — la torre di controllo ammassa i cloni quando i segnali sono pochi.
- **B1** — il grafo "chi copre chi": consumarlo (bounding overwatch) o rimuoverlo.
- **§E** — dove vive ognuna delle 17 costanti: globale, per-classe o per-mappa. Il caso guida è la
  rianimazione.
- **UI del `commander`** nel MapEditor: lavoro meccanico, va nella fase di authoring.
- **Stance del Droide Tattico**: oggi è un termostato sul rapporto di teste.
