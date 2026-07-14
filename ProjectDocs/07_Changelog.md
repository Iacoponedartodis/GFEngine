# 07 — Changelog

Dated engineering changes and their architectural effect.

## 2026-07-14 (8) — Fix piedi-sottoterra (regressione crowd) + loadout abilità onesto
- **Piedi sottoterra (regressione Phase B):** il write-back del `CrowdSystem` scriveva
  `tr->y = npos.y` (superficie navmesh ≈ piedi), ma il transform.y è il CENTRO fisico
  (spawn = suolo + `AI_HALF_Y`) → i modelli affondavano. Fix: `tr->y = agentPos.y + AI_HALF_Y`.
  Inoltre la voxelizzazione mette la superficie navmesh ~`cellHeight` SOPRA il pavimento reale:
  `agentPos` ora sottrae quella polarizzazione (e `cellHeight` 0.2→0.1 per Y più fine). Probe
  runtime: transform.y = 0.60 esatto (= suolo 0.1 + AI_HALF_Y 0.5). Piedi a terra, niente float.
- **Loadout abilità/gadget del giocatore:** verificato che `abilityIds`/`gadgetId` sono
  selezionabili nel PreMatch ma **mai applicati all'entità player** (usati solo dalle AI). Le
  righe sono marcate "(non attiva/o)" (convenzione KI #25) finché non esiste un sistema
  abilità/gadget lato giocatore (lavoro futuro). NB: la schivata/roll è già un comando base
  (tasto "Schivata"=Q, funzionante), NON un'abilità di loadout.
- Build-verified, warning-free; smoke `--stress`: AI combatte, 0 crash. **Smoke visivo utente:**
  confermare i piedi a terra in gioco.

## 2026-07-14 (7) — Navigazione Recast/Detour, Phase C: aree semantiche (ADR-017) — COMPLETO
- **Marking aree** in `NavManager::build` (dopo erosione, prima delle regioni): `rcMarkCylinderArea`
  tagga le `dangerZones` come area DANGER e i `coverPoints` come COVER (id 1/2; ground=0). I
  metadata vengono da `MapDef.dangerZones`/`coverPoints` (caricati a runtime; `MapGeometryBox`
  non ha `type`, è editor-only).
- **Costo filtro crowd** (`getEditableFilter(0)`): DANGER=10 → il pathfinding **aggira le danger
  zone**; GROUND/COVER neutri. Aree extra restano WALKABLE (attraversabili se non c'è alternativa).
  `findPath` usa lo stesso costo. Per-ruolo = estensione banale (filtri + `queryFilterType`).
- **Build-verified**, zero warning. Su firebase: **5 poligoni DANGER + 7 COVER** taggati
  (telemetria `navmesh built`); AI combatte (166 colpi), stuck ~33 (nessuna regressione), 0 crash.
- **ADR-017 COMPLETO (Phase A+B+C)** — navigazione Recast in force. Resta: costi per-ruolo (dati),
  veicoli come ostacoli dinamici, taratura congestione. **Smoke manuale utente:** partita reale +
  feel combattimento + outpost.

## 2026-07-14 (6) — Navigazione Recast/Detour, Phase B: DetourCrowd → movimento AI (ADR-017)
- **CrowdSystem** (`src/ecs/systems/`, registrato dopo AiSystem): registra le AI come agenti
  `dtCrowd`, reap dei morti (mappa idx→entità + generazione navmesh), tick del crowd 1×/step
  fisso, write-back `npos`→transform. `World::nav` (puntatore opaco), `AiComponent::crowdAgentIdx`.
- **AiSystem movimento** ora via crowd: traversata (Hunt/Search/Patrol) → `requestMoveTarget`
  (pathfinding, aggira gli ostacoli); Alert/roll → `requestMoveVelocity` (velocità tattica +
  avoidance). **Fallback `aiMove`** se il navmesh manca. Sensing/player/proiettili/veicoli
  invariati. Salto anti-ostacolo e stuck-WARN in Alert disattivati col crowd (falsi positivi).
- **Fix chiave:** `requestMoveTarget` non ripianifica se il target è ~invariato (chiamarlo ogni
  frame rendeva il moto lento/a scatti → prima sembrava che l'AI non si muovesse).
- **Build-verified**, zero warning. `--stress 20` (40 bot): 40/40 agenti on-mesh, traversano e
  combattono (**168 colpi**), reap ok, **0 crash**. **Stuck: da ~80 a 35** e spostato via dal
  cover z=-6 → obstacle-stuck RISOLTO dal pathfinding; il residuo è congestione crowd in mischia.
- **Da fare:** Phase C (aree semantiche). **Smoke manuale utente:** partita reale + feel
  combattimento + mappa outpost. Feel combat via velocity (avoidance in più) da validare.

## 2026-07-14 (5) — Navigazione Recast/Detour, Phase A: navmesh da MapDef (ADR-017)
- **CMake:** `recastnavigation` v1.6.0 via FetchContent (tag pinnato), demo/tests/examples OFF,
  linkato SOLO a GFEngine (`Recast Detour`). Fix: `CMAKE_POLICY_VERSION_MINIMUM=3.5` (v1.6.0
  ha un `cmake_minimum_required` che CMake 4.0 rifiuta).
- **`NavManager`** (`include/mini/game/nav/` + `src/game/nav/`): costruisce un `dtNavMesh`
  single-tile dai box collider di `MapDef.geometry` (box→12 triangoli con `ry`, pipeline
  Recast solo-mesh); `findPath` via Detour. `walkableClimb=STEP_HEIGHT` scavalca scalini
  bassi, muri/cover alti aggirati. Header leggero (tipi Detour forward-declared).
- **Hook:** build in `Application::initWorld` al load mappa; **zero cambi di comportamento**
  (AI ancora su `aiMove`). Validazione via telemetria JSONL (ADR-016): eventi `navmesh built`
  e `sample path`.
- **Build-verified**, zero warning (Recast esterno silenziato). Su firebase: **navmesh 74
  poligoni** da 264 triangoli, bounds mappa corretti; **`findPath` spawn1→spawn2: 8 waypoint,
  40.0m** (vs ~32m in retta) → il path **aggira** la geometria centrale = fix dell'AI-stuck
  alla radice. 0 crash.
- **Da fare:** Phase B (DetourCrowd → movimento AI, traversata-prima/combattimento-manuale),
  Phase C (aree semantiche). **Smoke manuale utente:** avvio su mappa diversa (outpost).

## 2026-07-14 (4) — Telemetria JSONL, Phase 4: dump stato completo (ADR-016) — piano COMPLETO
- **Non creato `dumpFullState(EntityManager&)`** (non esiste `EntityManager`): **esteso** il
  `dumpGameState`/F12 esistente (ADR-013). Nuovo builder condiviso `buildStateDump(reason)` in
  Application: oltre a camera/player/ticket, aggiunge un array **`entities`** con OGNI entità
  attiva — `{id, pos:[x,y,z], team, hp/hp_max, ai_state (Patrol/Alert/Hunt/Search), goal
  (lastKnown), kind (bullet/vehicle)}` — più `dump_reason`.
- **Trigger:** F12 (`reason=f12`), **fine partita** (rilevatore di transizione one-shot su
  Win/Lose → `reason=match_win/lose` + evento `match end` nel JSONL), **crash** (callback
  `setStateDumpCallback` registrata da Application; il crash net la invoca best-effort DOPO
  `crash_report.txt`, con guardia di ri-entranza `g_dumping` + try/catch).
- **Build-verified**, warning-free. Struttura del dump validata con parser JSON (46 entità:
  AI con `ai_state=Alert`/`goal`, veicolo con hp, ecc.). Fine-partita non testabile headless
  (in `--sim` l'outcome è forzato `Ongoing`, observer) e crash idem (serve un crash vero) —
  meccanismo verificato via trigger temporaneo; **smoke reali in carico all'utente** (F12 in
  partita, fine di una partita vera, crash indotto).
- **Piano telemetria LLM-observable COMPLETO** (Phase 1-4). ADR-016 Accepted.

## 2026-07-14 (3) — Telemetria JSONL, Phase 3: stato AI + stuck detection (ADR-016, 06_Todo #1)
- **State change (INFO):** in `AiSystem::update`, log SOLO sulle transizioni reali di stato
  (`oldState` catturato a inizio iterazione, confronto prima del blocco sparo che ha molti
  `continue`). Payload come da piano: `{"bot_id","state","pos":[x,y,z],"target_pos":[tx,ty,tz]}`
  (target = nearest se presente, altrimenti lastKnown).
- **Stuck detection (WARN):** aggancio all'anti-stuck esistente (`stuckTimer > AI_STUCK_TIME`).
  Nuovo flag `AiComponent::stuckReported` → **una WARN per episodio** (non per-frame), azzerato
  quando l'AI torna a muoversi. Payload: `{"bot_id","state","pos":[x,y,z],"stuck_time"}`.
- **Build-verified**, warning-free. Verificato `--stress 20`: 173 `state change`, 80 `stuck`
  (WARN), tutti JSON validi. **La telemetria rivela subito il problema di #1:** le coordinate
  stuck si addensano attorno a z≈-6 (= "Cover Centro N" del MapDef firebase) → i bot si
  incastrano sulle coperture. Dato azionabile per il fix AI-stuck (che resta da fare: questa
  fase lo rende OSSERVABILE, non lo risolve).
- Nota volume: 80 stuck/20s a 40 bot indica bot che si bloccano ripetutamente (reale). Se
  troppo rumoroso, alzare `AI_STUCK_TIME` o aggiungere un cooldown per-bot — non ora.

## 2026-07-14 (2) — Telemetria JSONL, Phase 2: hook GameMode + CommandPost (ADR-016)
- **GameMode (ADR-008):** `GameModeFactory::createGameMode` emette `mode created`
  (`{"mode_id":...}`) e `WARN` su modalità sconosciuta. `ConquestMode::updateObjectiveRules`
  emette **`Ticket bleed`** al drenaggio (`{"tickets_ally","tickets_enemy","posts_ally",
  "posts_enemy","drained"}`).
- **CommandPost (ADR-009):** `CommandPosts::update` emette eventi **discreti** (non per-frame,
  per non inondare il log): `Capture started` (nuovo team in cattura) e `Capture update`
  (cambio proprietario) con `{"cp_id"(=label),"progress","owner"}`.
- **Build-verified**, warning-free. Verificato con `--sim` (30s): prodotti e validati
  `mode created`, `Capture started`, `Capture update` (Alpha→Enemy), `Ticket bleed` (ally
  3→1) — tutti JSON validi coi campi base `frame/time/system/level/msg/data`.
- Hook solo in GFEngine (l'editor non compila questi TU). Phase 3 (AI state/stuck) e Phase 4
  (dump stato su crash/fine partita, estendendo `dumpGameState`) in attesa di verifica.

## 2026-07-14 — Telemetria JSONL LLM-observable, Phase 1 (ADR-016, estende ADR-013)
- **Audit prima di agire:** il "basic text logger" da rifattorizzare del piano è in realtà il
  sistema ADR-013 completo (`mini::telemetry`). Il piano assumeva `Application::tick()` ed
  `EntityManager` inesistenti, un frame counter da aggiungere (esiste: `frame()`), e Phase 4
  `dumpFullState` (esiste già: `dumpGameState`/F12). "No raw text logs" contraddice ADR-013.
- **Fatto (additivo, non distruttivo):** aggiunto sink **JSONL** dentro `mini::telemetry` —
  `session_latest.jsonl` (`editor_session.jsonl` per l'editor) accanto a `engine_run.log`,
  NON al suo posto. API: `Level` enum, `event(Level, system, msg, json data)` (+ overload),
  `flushEvents()`. Riusa nlohmann/json e il frame counter già presenti. Riga = un oggetto JSON
  `{"frame","time","system","level","msg","data"}`. Flush a fine frame + immediato su ERROR/FATAL.
- **NON fatto (contro ADR-013):** rinomina in `AITelemetry`, rimozione dei log testuali.
  Segnalato: i log testuali e i ~centinaia di `logInfo/logTrace` restano.
- **Build-verified**, warning-free; `session_latest.jsonl` prodotto e **validato con parser**
  (`ConvertFrom-Json` OK). Phase 2-4 (hook GameMode/CommandPost/AI + dump stato) in attesa di
  verifica dell'utente.

## 2026-07-13 (7) — Fase 4b ottimizzazione: cap LOS ai K vicini (invece della griglia spaziale)
- **Finding che cambia la tecnica:** la griglia spaziale del piano rende solo se il raggio di
  query << mondo. Qui `aggroRange` = `sight_range` ≈ **20 m** e la mappa è **50×40 m** → un
  query 3×3 copre l'intera mappa: **una griglia non poterebbe nulla**. Verificato sui valori
  reali. Il costo residuo è LOS-bound nella mischia densa (ogni AI fa `hasLineOfSight` per
  ~ogni nemico in range ≈ N).
- **Tecnica corretta (stesso obiettivo Fase 4b — eliminare l'O(N²)):** nella ricerca target
  si raccolgono i **K = `config::AI_MAX_LOS_CHECKS` (8) bersagli più vicini** (solo distanze,
  inserimento in array ordinato — flop economici) e si verifica il LOS solo su quei K, dal più
  vicino: il primo visibile è il nearest visibile. Bounda la LOS costosa a **O(N·K)** invece
  di O(N²). Stesso cap applicato alla passata shared-awareness. Comportamento: l'AI ingaggia
  un nemico vicino visibile invece dello stretto-più-vicino globale — differenza impercettibile
  (shared-awareness + ri-sensing compensano).
- **Misura (probe temporanea, DEBUG, `World::tick`):**
  | scala | baseline | +time-slice (4a) | +K-cap (4b) |
  |---|---|---|---|
  | 30 AI | ~23 ms* | — | **6.2 ms** |
  | 100 AI | 203 ms | 55 ms | **36 ms** |
  *stima da scaling quadratico. Totale a 100 AI: **203→36 ms ≈ 5.6×**; il residuo ora è
  dominato dall'azione per-frame (collisione movimento), non più dalla sensing. In Release
  (~5-20× più veloce) 30 AI ≈ 0.3-1.2 ms, 100 AI ≈ 2-7 ms.
- **Build-verified**, warning-free. Smoke `--stress 50`: 300 `[Combat] Colpito!`, 0 crash;
  sandbox normale 0 crash.
- **Nota:** ulteriori guadagni verrebbero da una struttura spaziale per i COLLIDER (per
  abbassare il costo di ogni singola `hasLineOfSight`/collisione movimento), non dalla griglia
  delle entità. Non necessario per la scala reale del gioco (30-50 AI) — da fare solo se serve.
## 2026-07-13 (6) — Fase 4a ottimizzazione: time-slicing della sensing AI
- **Motivo (dati Fase 3):** l'AI è il 95-99% del tick e scala O(N²) (ricerca target + LOS).
  Oltre ~20 AI il gioco lagga già sul PC dello sviluppatore.
- **Cosa:** la sensing pesante (le due passate O(N²) — shared-awareness e ricerca nearest,
  con `hasLineOfSight`) ora gira per ogni AI solo 1 tick su `config::AI_SENSE_INTERVAL`
  (=6, ~10 Hz), **scaglionata per entità** (`(tickCount + entityId) % INTERVAL`). Fra un
  sensing e l'altro l'AI riusa il bersaglio cachato (`AiComponent::targetEntity`) per
  mirare/muoversi; movimento e sparo restano ogni tick. La morte del target è rilevata ogni
  frame (getTransform); il **LOS è ri-verificato al momento dello sparo** (solo a cooldown
  scaduto → economico) così non spara attraverso i muri col target cachato → nessuna
  regressione di comportamento.
- **Misura (probe temporanea, DEBUG, 100 AI = 50v50):**
  | | prima | dopo | speedup |
  |---|---|---|---|
  | `AiSystem::update` | 202 ms | 54 ms | **3.7×** |
  | `World::tick` | 203 ms | 55 ms | **3.7×** |
  Non il 6× pieno perché il time-slicing taglia solo la sensing; il lavoro per-frame
  (movimento+collisione, sparo) resta e ora domina il residuo.
- **Build-verified**, warning-free. Smoke `--stress 50`: 287 `[Combat] Colpito!`, 0 crash;
  sandbox normale 0 crash. Comportamento AI preservato.
- **Prossimo (Fase 4b):** griglia spaziale per la sensing residua O(N²) → O(vicini): sui
  tick di sensing ogni AI interroga solo le celle adiacenti invece di tutti i bersagli.
  Attaccherebbe il residuo e permetterebbe di abbassare `AI_SENSE_INTERVAL` (più reattività).

## 2026-07-13 (5) — Stress test AI: cap alzati + spawn a griglia + flag `--stress`
- **Obiettivo:** poter profilare a scala con Tracy (Fase 3-4) — prima sim max 20 AI,
  partita max 30.
- **Cap unificato:** nuova `config::MAX_AI_PER_TEAM = 50`. Sostituisce i literal sparsi:
  clamp SandboxMenu (era 10/10), slider PreMatch (erano 10/20), `std::min` ConquestMode
  (erano 20 nemici / 10 alleati). Ora fino a 50 per team in sim e partita.
- **Spawn scalabile:** `SandboxMode` usava una FILA singola (passo 3m) → con 50 unità si
  estendeva ±73m fuori da una mappa larga 50m. Convertito a GRIGLIA (`perRow=10` +
  `findFreeSpot`), come già faceva `ConquestMode::genPositions`. Resta nei limiti mappa.
- **`--stress N` (CLI, main.cpp/Application::run):** forza sim + N (clampato a 50) AI per
  team, headless. `GFEngine.exe --stress 50` → riproducibile per il profiling.
- **Build-verified**, warning-free. Smoke `--stress 50`: **100 AI spawnati, 100 `[Combat]
  Colpito!`, 0 crash**; sandbox normale (griglia) 0 errori.
- **Misura iniziale (probe temporanea, build DEBUG non ottimizzata → valori pessimistici):**
  | scala | World::tick | AiSystem::update | AI % |
  |---|---|---|---|
  | 20 AI | 10.5 ms | 10.0 ms | 95% |
  | 100 AI | 203 ms | 202 ms | 99% |
  L'AI è il collo di bottiglia (95-99% del tick) e scala **~quadraticamente** (5× unità →
  ~19× costo → conferma O(N²) di ricerca target + LOS). La Release è ~5-20× più veloce
  (assoluti molto più bassi) ma lo scaling quadratico è architetturale e resta.
- **Conclusione dati:** la Fase 4 (time-slicing AI + griglia spaziale) è **giustificata** —
  attacca direttamente ricerca target e LOS O(N²). Numero preciso di Release da rilevare con
  la build RelWithDebInfo + Tracy (`--stress 50`) prima di tarare `UPDATE_RATE` / cella griglia.

## 2026-07-13 (4) — Fase 3 ottimizzazione: layout dati SoA per la ricerca target AI
- **Premessa del piano corretta (verificata sul codice live):** l'ECS NON usa
  `shared_ptr<Entity>` polimorfici (la premessa della Fase 3); usa
  `std::unordered_map<EntityId, Component>` per componente. La riscrittura totale a SoA
  toccherebbe World + ogni accessor + ogni sistema + ogni game mode → alto rischio, non
  giustificato senza misura. Ho quindi implementato l'approccio *additivo* che il piano
  stesso prescrive (array flat PARALLELO, non sostituzione dello storage).
- **Cosa:** `AiSystem::update` aveva due passate O(AI × bersagli) (shared-awareness + nearest)
  che facevano `getTransform(tgt)` — hash lookup + pointer-chase su heap sparso — nel ciclo
  interno per OGNI coppia. Ora id+posizione dei bersagli sono catturati UNA volta in array
  SoA contigui (`team{1,2}Tgts` / `team{1,2}Pos`); i loop leggono `pos[i]` contiguo. Il
  componente pesante viene recuperato SOLO per il nearest selezionato (`getTransform(nearest)`),
  come da piano.
- **Semantica:** la SELEZIONE del target ora usa posizioni a inizio-frame (snapshot coerente)
  invece che parzialmente aggiornate da AI mosse prima nello stesso tick. Differenza sub-frame
  (≤~5 cm a 60 Hz), impercettibile e più deterministica. Il puntamento/sparo effettivo usa
  comunque un `getTransform(nearest)` FRESCO → nessun impatto sulla mira.
- **Build-verified**, warning-free; smoke `--sim`: AI ingaggiano e si colpiscono
  (`[Combat] Colpito!`), comportamento preservato. Il guadagno è a scala (40-50 AI, cfr.
  Fase 4): elimina i lookup hash per-coppia; **misurabile con Tracy** (zona `AiSystem::update`).
- **Non toccato:** broad-phase di `hasCollision` (altro loop caldo con lookup per-entità) —
  sarà risolto in modo più fondamentale dalla griglia spaziale della Fase 4, che riuserà
  questi array flat.

## 2026-07-13 (3) — Fase 2 ottimizzazione: frame pacing (doppia precisione + cap di sicurezza)
- **Contesto (verificato sul codice live):** l'accumulator a timestep fisso, il decoupling
  sim/render e la VSync **esistono già** ed erano corretti. La Fase 2 quindi *migliora*
  l'esistente, non riscrive.
- **Timing in doppia precisione:** il main loop ora deriva il dt da
  `SDL_GetPerformanceCounter`/`Frequency` in `double` con accumulatore `double`
  (`SIMULATION_STEP = 1.0/60.0`), invece dell'accumulatore `float` su `std::chrono`. La
  simulazione riceve comunque `fixedDt` (float, 1/60) invariato → gameplay identico. Elimina
  il drift di accumulo su sessioni lunghe. Clamp anti spiral-of-death (0.25s) invariato.
- **Frame-cap di sicurezza (`config::MAX_UNCAPPED_FPS = 300`):** attivo SOLO quando la VSync
  è spenta (`SDL_GL_GetSwapInterval()==0`), con sleep IBRIDO (`SDL_Delay` grossolano +
  busy-wait finale sub-ms) come da piano — lo Sleep di Windows ha risoluzione ~15ms. Con
  VSync ON (default) è inerte (un check per frame). Evita il loop non limitato a migliaia di
  FPS (100% CPU) se qualcuno imposta `WindowConfig.vsync=false`.
- **Build-verified**, warning-free. Probe runtime: **59.98 tick/s su 3s = 60 Hz esatti**
  (velocità di gioco preservata).
- **NON fatto (proposto separatamente):** interpolazione di rendering (alpha =
  accumulator/step). È il vero guadagno visivo su monitor ad alto refresh (sim 60 Hz renderizzata
  a 144 Hz → micro-stutter delle entità AI/proiettili), ma è invasiva (richiede store
  prev/current transform + gestione "snap" su respawn/teletrasporto). Il piano la marca
  "optional"; da decidere se affrontarla.

## 2026-07-13 (2) — Fix spawn giocatore incastrato nel pavimento (respawn "sospeso sopra un muro")
- **Sintomo:** dopo una simulazione, al riavvio della sandbox il giocatore respawnava in
  aria/sopra un muro e restava sospeso anche muovendosi.
- **Causa radice (diagnosticata con probe collisione runtime):** lo spawn usava una Y fissa
  hardcoded `SPAWN_Y=0.86` → piedi a y=0, ma il collider "Pavimento" di firebase ha il top a
  y=0.1. Il giocatore nasceva **incastrato di 0.1 nel pavimento** → `hasCollision@spawn=true`
  → `slideMoveWithStepUp` lo sollevava di un intero `STEP_HEIGHT` (~0.55) lanciandolo a occhi
  1.42, poi la gravità lo riassestava. Su firebase il rimbalzo si auto-correggeva in ~0.4s;
  con spawn adiacenti a geometria più alta la sospensione restava persistente. Il bug era su
  **ogni** spawn/respawn, non solo dopo la sim (la sim lo rendeva evidente).
- **Fix:** nuovo helper data-driven `mapquery::groundedSpawn` (MapQuery.hpp): sposta lo spawn
  fuori dagli ostacoli (`findFreeSpot`) e mette la Y-occhi = `groundHeightAt + PLAYER_HALF_Y`,
  cioè sul suolo reale della mappa. Usato in `SandboxMode::start` e `ConquestMode::start` al
  posto di `SPAWN_Y` fisso (fallback al vecchio valore solo se manca la MapDef).
- **Build-verified**; probe runtime: camY ora **stabile a 0.95 dal frame 0** al primo spawn e
  dopo restart post-sim (prima: 0.86→1.42→assestamento). Warning-free.
- **Nota latente (non toccata):** `slideMoveWithStepUp` solleva di un intero `STEP_HEIGHT`
  anche per micro-compenetrazioni; con lo spawn ora corretto il trigger sparisce, ma la
  correzione "a scatto" resta un candidato per il futuro sistema di collisioni accurate.

## 2026-07-13 — Fase 1 ottimizzazione: integrazione profiler Tracy (ADR-015)
- Aggiunto **Tracy `v0.11.1`** via FetchContent (tag pinnato) + opzione CMake
  `USE_TRACY_PROFILER` (default **OFF**): build normali identiche e a costo zero (macro
  no-op, TracyClient stub). Linkato **solo a GFEngine** (ADR-002).
- Strumentazione: `FrameMark` a fine main loop; `ZoneScoped` in `World::tick`,
  `AiSystem::update`, `CombatSystem::update`; `ZoneScopedN("render.drawScene")` nel
  rendering. (Mappati sui nomi reali: `Application::tick/render` del piano non esistono —
  il loop è un'unica `Application::run()`.)
- Deviazione dal piano documentata in ADR-015: default esplicito OFF invece di "ON in
  RelWithDebInfo" (generatore multi-config VS → `CMAKE_BUILD_TYPE` vuoto).
- **Build-verified** entrambi i path (OFF e ON), 0 warning. Profiling reale: build
  RelWithDebInfo con `-DUSE_TRACY_PROFILER=ON`. **Da verificare a mano:** cattura live con
  Tracy GUI connessa. Fasi 2-4 (fixed-timestep, SoA, AI time-slice/spatial hash) NON ancora
  toccate — in attesa di verifica.

## 2026-07-11 (8) — Fix spike mouse al primo frame (controlli invertiti + arma flicker)
- **Sintomo:** alla PRIMA apertura del sandbox i controlli sembravano invertiti e l'arma
  spariva/riappariva; si "sistemava" dopo qualche secondo (coincidenza col cambio arma) e
  non si riproduceva più. Causa: al primo `SDL_GetRelativeMouseState` dopo aver abilitato
  la cattura, SDL restituisce il delta accumulato (spesso enorme) → la camera schizzava,
  facendo percepire i comandi come invertiti e facendo oscillare il viewmodel dentro/fuori
  vista (l'arma è agganciata alla camera).
- Fix: `Window::setMouseCaptured(true)` ora svuota subito l'accumulatore relativo
  (`SDL_PumpEvents` + `SDL_GetRelativeMouseState`), così il primo frame parte con delta 0.
- Build compiler-warning-free; sandbox avvio pulito. **Da verificare a mano:** primo avvio
  sandbox senza scatti di camera né flicker dell'arma.

## 2026-07-11 (7) — Analisi profonda: bug trovati e risolti
- **Mesh veicolo custom tinta col colore del box (bug visivo):** `spawnOne` moltiplicava
  il modello texturato per `vd->color` (colore del box di fallback, es. rosso scuro dello
  speeder) invece di bianco → il modello vero appariva tinto. Fix: tint BIANCO per mesh
  custom, `vd->color` solo per il box.
- **Mirino incoerente sui veicoli:** il crosshair usava una sfera di 0.7m per i veicoli
  mentre i colpi usano tutto l'OBB del mezzo → il mirino non diventava rosso puntando la
  carrozzeria dello speeder. Ora il mirino usa lo stesso volume di danno OBB (coerenza
  mirino=colpi, come KI #13).
- **Tinta blu al mount troppo forte:** copriva i colori del modello custom → ora è un
  moltiplicatore leggero (il mezzo resta riconoscibile ma leggermente azzurro alla guida).
- **Warning del compilatore azzerati:** HUD hitmarker (shadowing hr/hg/hb → mkR/mkG/mkB),
  `respawnDelay` inutilizzato marcato, e C4996 (fopen/strncpy, deprecation MSVC di funzioni
  standard) silenziati con `_CRT_SECURE_NO_WARNINGS` sui due target — così i warning REALI
  restano visibili. Build compiler-warning-free (restano solo deprecation CMake da
  SDL2/tinygltf, esterni).
- **`assets/models/default.obj` creato** (cubo unitario): elimina l'errore di parsing
  all'avvio. Resta il solo `default.png` mancante → fallback checkerboard (KI #14).
- Verifica: build pulita, `--sim` regolare (veicoli, combat), editor ok.

## 2026-07-11 (6) — Veicolo: "muro invisibile" (collisione non teneva conto dello yaw)
- **Speeder bloccato dove c'era spazio:** la fisica tratta il box in movimento
  come allineato agli assi del MONDO usando gli half LOCALI, senza ruotarlo per lo
  yaw del veicolo. Per la fanteria (quasi cubica) è irrilevante, ma lo speeder è
  lungo (halfZ 2.5 = 5m): guidando girato, i 5m di lunghezza restavano puntati lungo
  l'asse Z del mondo invece che lungo il muso → collisioni fantasma ai lati.
- Fix: VehicleDrive usa l'AABB AVVOLGENTE della sagoma ruotata
  (`halfX*|cos|+halfZ*|sin|`, ecc.), esatto a 0/90° (marcia dritta lungo qualsiasi
  corridoio) e conservativo in diagonale. Il veicolo-come-ostacolo era già corretto
  (computeWorldAABB gestisce la rotazione); si sistemava solo il veicolo-in-movimento.
- Nota: in diagonale il box è ancora l'AABB avvolgente (più grande dell'OBB reale a
  45°); se serve più margine, ridurre `half_z` nel Vehicle Editor. Una collisione OBB
  vera è un'estensione futura (Todo #23, forme oltre i box).
- Build pulita; sandbox/sim ok. **Da verificare a mano:** guidare lo speeder nei
  corridoi/aree dove prima si bloccava.

## 2026-07-11 (5) — Guida veicolo: rotazione visiva disaccoppiata dalla marcia
- **Correzione del (4):** il problema vero era AVANTI/INDIETRO (da cui dipendeva
  anche destra/sinistra). Causa: `mesh_rot_y` (rotazione VISIVA del modello) veniva
  bakizzato in `transform.ry`, che è anche la direzione di MARCIA. Ruotare il
  modello ruotava insieme muso E guida → il muso restava disallineato dalla marcia
  e non si poteva correggere (per questo il 180° "non cambiava nulla").
- Fix: `transform.ry` del veicolo = SOLO direzione di marcia (`vs.ry`); la
  correzione visiva del muso va in `MeshRendererComponent.yawOffsetDeg` (nuovo),
  applicata al render senza toccare la fisica. Ora ruotare il modello nel Vehicle
  Editor raddrizza il muso SENZA invertire la guida; W è sempre "avanti" (la camera
  segue la marcia dal fix (4)).
- Il valore `mesh_rot_y` che avevi messo ora agisce solo sul visivo: taralo nel
  Vehicle Editor finché il muso punta nella direzione di marcia.
- Build pulita; sandbox veicolo `[mesh]` ok. **Da verificare a mano:** guidando,
  W va avanti (dove punta la camera) e il muso del modello è allineato dopo aver
  regolato mesh_rot_y.

## 2026-07-11 (4) — Guida veicolo: sterzo non più invertito + scan progetto
- **Sterzo speeder "invertito":** in prima persona la camera restava orientata
  col MOUSE mentre il veicolo sterzava, quindi "destra/sinistra" erano relativi
  allo sguardo e non al mezzo → sembravano invertiti (e ruotare il modello 180°
  non cambiava nulla, perché è la camera il problema). Fix: alla guida la camera
  segue SEMPRE la direzione di marcia (anche in prima persona: `lookAt(pos+fwd)`);
  il mouse-look è sospeso durante la guida. Ora A = sinistra, D = destra, coerenti.
  (Lo sterzo era già corretto matematicamente per una camera che segue.)
- **Diagnostica viewmodel rimossa** (aveva confermato che le armi renderizzano).
- **Scan progetto:** `--sim` 40s = 43 hit / 4 kill / 9 roll / 4 veicoli, nessun
  errore, AI che cicla patrol/alert/hunt/search; sandbox + editor avviano puliti.
  Unico avviso residuo: `assets/textures/default.png` mancante → fallback
  checkerboard (cosmetico, KI #14). Build pulita.
- **Da verificare a mano:** guida speeder in prima e terza persona, sterzo
  corretto in entrambe.

## 2026-07-11 (3) — Armi in mano: anteprima editor = gioco (fix orientamento)
- **Bug: l'arma sistemata dritta nell'Entity Editor appariva storta in gioco**, di
  un angolo diverso per ogni arma. Causa: il runtime (`weaponattach::resolve`)
  applicava la correzione canonica dell'arma (`mesh_rot_y`), ma l'ANTEPRIMA
  dell'Entity Editor NON la applicava → si tarava la posa senza quella correzione,
  e in gioco si aggiungeva. L'angolo variava perché `mesh_rot_y` differisce per arma
  (DC-15A -180, ecc.).
- Fix: entrambi ora applicano la STESSA `baseFix` = `rotate(mesh_rot_y) *
  rotate(mesh_rot_x)` nello stesso punto della catena
  (`... R(pose) * scale * baseFix * T(-grip)`); il runtime ora include anche
  `mesh_rot_x` (prima solo Y). L'Entity Editor legge `mesh_rot_x/y` dell'arma e le
  usa nell'anteprima in mano.
- Effetto: ciò che è dritto nell'Entity Editor è dritto in sandbox/partita. Le pose
  già tarate vanno riviste UNA volta (l'anteprima ora è fedele al gioco), poi
  restano coerenti.
- Build pulita; smoke engine + editor ok. **Da verificare a mano:** dritta un'arma
  in mano nell'Entity Editor → dritta in sandbox, per armi con mesh_rot diversi.

## 2026-07-11 (2) — Fix editor: arma entità, scritte tagliate, box veicolo, tab armi
- **Arma dell'entità che non cambiava in gioco (bug):** l'arma in mano usava
  `weapon_display.id` (campo separato), mentre l'utente cambiava il loadout
  `weapons[]` — restavano disallineati (B1 Heavy: loadout E-5C ma display E-5).
  Fix: l'in-hand ora è l'arma PRIMARIA del loadout (`weaponattach` usa
  `primaryWeaponId()`; `weapon_display` dà solo la POSA). EntityEditor: il combo
  "Arma primaria" ora scrive `weapons[0]` (e l'anteprima parte da lì), quindi
  cambiare/togliere l'arma si riflette subito. Sistema anche i dati esistenti
  senza ri-salvare.
- **Scritte tagliate negli editor:** causa = `DragFloat(label)` a piena larghezza
  disegna l'etichetta a destra, fuori dal pannello. Nuovo helper
  `editor::ui::dragRow` (etichetta a SINISTRA, campo che riempie il resto: mai
  tagliata). Applicato ai stat del Weapon Editor e a tutto il Vehicle Editor.
- **Box veicolo confusi:** collisione (ora CIANO) e danno (GIALLO) hanno colori
  base brillanti e distinti (niente più highlight che li sbiadiva uguali); le
  sezioni proprietà sono colorate in tinta col box. Confermato che gli slider
  scrivono i campi giusti (nessuno scambio).
- **Altezza speeder:** controllo rinominato "Altezza mesh (su/giu)" con hint
  ("troppo alto? valori negativi"); i modelli con origine alla base vanno abbassati
  qui. Nuovi campi veicolo `mesh_rot_x`/`mesh_offset_y` esposti nell'editor.
- **Balance Editor: rimossa la tab "Armi"** (l'utente l'ha chiesto): due UI sugli
  stessi file senza live-sync creavano confusione. Il Weapon Editor (con viewport
  3D) è l'unico strumento armi. Le altre tab (AI/Mappe/Personaggio/Abilità) restano.
- Build pulita; smoke engine (veicolo `[mesh]`) + editor ok. **Da verificare a
  mano:** B1 Heavy con E-5C in mano; scritte leggibili ovunque nell'editor; i due
  box veicolo distinti (ciano/giallo); abbassare lo speeder con "Altezza mesh".

## 2026-07-11 — Veicoli: mesh custom renderizzata + hitbox di danno separata
- **BUG "lo speeder non si vede": i veicoli non caricavano MAI la mesh custom** —
  la cache mesh popolava solo enemies/allies/weapons, e `vehiclespawn::spawnOne`
  usava sempre il box di fallback. Fix: (a) Application carica anche le mesh dei
  veicoli nel cache; (b) `spawnOne` accetta il `MeshCache` e usa la mesh vera con
  `mesh_scale/mesh_rot_x/mesh_rot_y/mesh_offset_y`, box solo come fallback.
  Log spawn ora marca `[mesh]`/`[box]`. Verificato: speeder `[mesh]` in gioco.
- **Hitbox di danno del veicolo, separata dalla collisione** (richiesta utente): il
  box di collisione deve raggiungere il suolo per guidare, ma i colpi allo spazio
  VUOTO sotto un mezzo che fluttua non devono contare. `VehicleDef` +
  `VehicleComponent` hanno ora `hit_offset_y` + `hit_half_x/y/z` (0 = usa la
  collisione); CombatSystem `segmentHitsVehicle` usa questo volume. BARC Speeder
  tarato (offset 0.25, half_y 0.35) per escludere il gap sotto.
- **VehicleEditor**: aggiunti Rot X, Offset Y e la sezione "Volume di DANNO"; il
  viewport mostra DUE wireframe (collisione grigia + danno gialla) sovrapposti alla
  mesh, così il box di danno si tara a vista.
- **Armi: verificato che RENDERIZZANO** (diagnostica temporanea `viewmodel: ...
  cache=OK`): il viewmodel del giocatore e l'arma in mano alle AI disegnano la mesh.
  Se un'arma appare orientata male è tuning di `mesh_rot_y`, non un bug di rendering.
- Build pulita; smoke engine `[mesh]`+viewmodel OK, editor carica lo speeder GLB.
  **Da verificare a mano:** speeder visibile in sandbox/partita; box di danno giallo
  allineato al corpo nel Vehicle Editor; taratura fine di hit_offset_y sullo speeder.

## 2026-07-10 (28) — Rifinitura robustezza, tranche 2 (residuo A7, A9, arma attiva)
- **Arma attiva consolidata** (radice del KI #22): rimosso il membro-copia
  `PlayerController::weapon`; l'arma attiva è ora SEMPRE `weapons[activeWeapon]` via
  accessor `weapon()`. Sparite tutte le assegnazioni di sincronizzazione sparse in
  Application (5 punti) e l'hack di ri-scrittura allo switch: lo stato heat vive in un
  posto solo per costruzione.
- **Residuo A7 chiuso**: `resolveUnitArchetype(registry, id, team)` unico per nemici E
  alleati in ConquestMode (il path alleati re-implementava il resolve a mano). Fix
  concreto incluso: gli alleati ora prendono le stats proiettile (velocità/danno/vita)
  dalla loro ARMA reale invece degli 8/20/5 hardcoded.
- **A9 (KI #25)**: i campi salvati ma non consumati dal runtime sono ora marcati
  "(non attivo)" negli editor: min_range e mesh proiettile (Weapon/Balance editor),
  fov_deg/hearing_range/reposition_chance (tab AI), damage_scale (+ nota che
  move_speed è vinto dal profilo AI) nell'Entity Editor.
- **Verifica:** build pulita; smoke `--sim` 12s ok (spawn 6+1+3 post, ingaggi con
  hit/roll nel log). Smoke manuale: heat persistente allo switch (Q) — invariato
  rispetto a (27) ma ora garantito strutturalmente.

## 2026-07-10 (27) — Rifinitura robustezza: Todo A2-A8 in un colpo solo
- **A2 (KI #21)**: tutti i loader del registry ora derivano l'id SOLO dal filename stem
  (ADR-001); il campo `id`/`profile_id` in-file è ignorato (prima un id stantio
  registrava la definizione sotto la chiave sbagliata, rompendo le cross-ref in
  silenzio). Verificato zero mismatch id/stem nei dati correnti prima del cambio.
- **A3 (KI #22)**: lo switch arma riscrive lo stato runtime (heat/overheat) nell'arma
  riposta — chiuso l'exploit "switch avanti/indietro = arma fredda".
- **A4**: eliminato `UnitTemplate` in ConquestMode; `RespawnEntry` (ora con default) è
  l'UNICO spawn spec per spawn iniziale, tracking e respawn — copia integrale invece di
  ~60 righe di copie campo-per-campo (classe di bug già vista: respawn come cubo).
- **A5 (KI #23)**: collisione (SAT 2D + intervallo Y) e LOS (segmento in spazio locale)
  ora ESATTE sui collider ruotati attorno a Y, coerenti col test OBB dei proiettili;
  fast path invariato per box non ruotati. Prima un muro diagonale bloccava
  movimento/LOS col suo AABB gonfiato mentre i colpi morivano sul bordo vero.
- **A6 (KI #27)**: stb e ImGui pinnati ai commit esatti già in uso (31c1ad3 / 6029ee3).
- **A7**: `parseUnitDef` condiviso tra loadEnemies/loadAllies (via ~140 righe duplicate;
  default per-team preservati: colori, hp 80/60, move 4/1.8, fazione).
- **A8 (KI #24, #26)**: rimossi gli ultimi id di definizione hardcoded (SandboxMode:
  registry vuoto = log errore, non manichini fantasma); rimossi i preset armi morti da
  Weapon.hpp (resta solo `makeBlasterRifle` come fallback di ultima istanza,
  documentato); eliminati `data/definitions/*` (mai caricati) e le cartelle vuote
  `data/presets`, `data/runtime`.
- **Verifica:** build pulita (0 errori, riconfigure incluso per i pin); smoke runtime
  `--sim` 12s: registry completo, spawn 6+1+3 post+2 veicoli, AI in ingaggio
  (hit/roll/shield nel log), `user_presets/` creata. **Smoke manuali restanti:**
  heat allo switch (Q), muro diagonale (quando esisterà in una mappa).

## 2026-07-10 (26) — Audit qualità completo + fix preset (KI #19/#20, Todo A1)
- **Audit dell'intero progetto** (runtime, editor, build, dati): esiti registrati in
  08_KnownIssues #19-#27 e 06_Todo sezione "Robustezza" (A1-A10, in ordine di gravità).
- **Fix A1 — preset partita**: spostati da `<exe>/data/presets/match` (cancellata a OGNI
  build dal post-build CMake → perdita dati sistematica) a `<exe>/user_presets/match`,
  con migrazione automatica dei file legacy al primo load (e ri-salvataggio subito nella
  nuova posizione). Serializzazione riscritta con nlohmann::json (via il parser
  artigianale senza escaping); i preset ora persistono la mappa **per id** (`map_id`,
  non più l'indice fragile nella lista ordinata) e l'intero loadout (primaria,
  secondaria, abilità, gadget). `PreMatchMenu::applyPreset` risolve gli id in indici UI
  al caricamento; retrocompat: i file legacy con solo `map_index` vengono ancora letti.
  Rimosso `windows.h` da MatchSettings.hpp (via CreateDirectoryA → std::filesystem).
- Build pulita (GFEngine + GFEditor, 0 errori). **Da verificare a mano (smoke):**
  salvare un preset con mappa Outpost e loadout → rebuild → ricaricarlo e verificare
  che sopravviva e che mappa/armi/abilità tornino giuste.

## 2026-07-10 (25) — Fase B veicoli, tranche 2: respawn mezzi + authoring spawn
- **Respawn dei veicoli distrutti**: `vehiclespawn::RespawnTracker` (in
  VehicleSpawn.hpp, condiviso): un mezzo esploso torna al suo spawn dopo 15s
  (log chat "VEICOLO distrutto: torna tra Ns"). Attivo in Conquest (e derivate)
  e Sandbox — un match lungo non resta più senza mezzi.
- **Authoring `vehicle_spawns` nel Map Editor**: quarta sezione metadata
  ("[VS]" nella lista, range selezione -400): marker arancio a misura di mezzo
  con freccia direzione, gizmo Sposta, proprietà con COMBO veicolo dal registry
  (id da data/vehicles, mai testo libero), X/Z e Yaw a slider, salvataggio RMW
  insieme al resto. Chiusa l'ultima voce "solo JSON a mano" della Fase A.
- Build pulita; sim regolare, editor ok. **Da verificare a mano:** distruggere
  uno speeder e vederlo tornare dopo 15s; piazzare/salvare uno spawn veicolo
  dal Map Editor su Outpost.

## 2026-07-10 (24) — Fix osservatore "teleport+bersagliato" + audit allineamento
- **BUG osservatore risolto** (diagnosi dal sintomo "posso ancora volare ma i droidi
  mi sparano"): in simulazione l'entità player restava parcheggiata allo spawn a
  team 0 → i proiettili VAGANTI di entrambi i team la colpivano → la logica
  morte/respawn (non esclusa in osservazione) faceva `updateRespawn` →
  **teletrasporto della camera allo spawn + entità ricreata a team 1** → i droidi
  bersagliavano l'osservatore. Fix doppio: (a) morte/ticket/respawn del giocatore
  guardati con `!observerFly`; (b) in `startSimulation` l'entità viene parcheggiata
  a y=-100 (fuori campo: non intercetta più nemmeno i vaganti).
  Verifica: 60s di sim senza alcun Eliminato/Respawn nel log (prima entro ~1 min).
- **Roll su cloni e giocatore**: `Combat Roll` assegnato a Clone Trooper e Heavy
  Clone Trooper nei dati. Nota: il GIOCATORE ha già la schivata nativa
  (`Action::Roll`, rimappabile nelle opzioni) — nessun lavoro necessario.
- **Audit "stessa versione"** (richiesto): trovato il disallineamento segnalato —
  le opzioni Controlli elencavano solo mouse Sparo/Mira come voci fisse. Ora la
  schermata ha una **colonna destra "Tasti fissi"** completa: V (prima/terza
  persona), E (veicolo), L+PAGSU/PAGGIU (log eventi), TAB (menu sandbox),
  P (PreMatch da sandbox), 1-9 (armi rapide), F12 (dump stato), F11 (fullscreen);
  colonna sinistra = azioni rimappabili (layout a due colonne per starci).
  Verificati e già allineati: toast sandbox, footer SandboxMenu, pausa, docs 02/03.
- Build pulita; sim 60s regolare (23 hit, 5 roll). **Da verificare a mano:**
  osservare a lungo senza più teleport; schermata Controlli leggibile;
  cloni che rollano in sim.

## 2026-07-10 (23) — Prima abilità AI ATTIVA: Combat Roll (16_AiBehavior esteso)
- Lo scaffold `AbilityComponent` (fermo da giorni senza storage) è ora un componente
  vero: storage in World, `AbilityState` esteso con type/param/cooldownMax risolti
  dal AbilityDef allo spawn (ConquestMode, insieme allo shield).
- **Roll AI**: entrando in fase evasiva, se il cooldown è pronto, l'AI esegue uno
  scatto laterale (param1 = velocità m/s, param2 = durata s, cooldown dal def) che
  ha priorità sul movimento normale. Telemetria `roll:` + log chat "ROLL #id".
  Trigger volutamente semplice: l'ingresso in hide È il momento sotto pressione.
- Dati: nuova ability `Combat Roll` (10 m/s, 0.35s, cd 6s — bilanciabile dalla tab
  Abilità) assegnata al B1 Battle Droid.
- Verifica `--sim` 45s: 5 roll da entità diverse, cooldown rispettati, 23 hit.
  **Da verificare a mano:** in sim si vedono i droidi scattare di lato quando
  vanno in copertura; righe ROLL in log chat.

## 2026-07-10 (22) — Tre fix da playtest: spawn su veicoli, rotY armi, mappa in sandbox
- **Spawn incastrati sui veicoli** (visto su Outpost): nuova
  `physics::nudgeOutOfColliders` (8 direzioni × 3 raggi) applicata allo spawn di
  unità/manichini in Conquest e Sandbox — la vecchia decollisione (findFreeSpot)
  vede solo la geometria della MAPPA, non le entità solide come i mezzi.
- **Pistola renderizzata storta**: il WeaponDef aveva solo `mesh_rot_x` — aggiunto
  `mesh_rot_y` end-to-end: slider "RotY" nel Weapon Editor (con anteprima:
  `FreeCameraViewport::loadModel` ora accetta rotY), parse nel registry, viewmodel
  del giocatore (90° convenzione + rotY per-arma) e arma in mano alle unità
  (raddrizzamento base attorno al grip, PRIMA della posa weapon_display — le armi
  esistenti con rotY=0 sono invariate). Ora la pistola si raddrizza dall'editor.
- **Cambio mappa dalla sandbox**: pagina Simulazione del menu TAB con riga "Mappa"
  (SIN/DES su tutte le mappe del registry) usata sia dalla simulazione AI sia dalla
  nuova azione "Riavvia la SANDBOX sulla mappa scelta".
- Build pulita; smoke: sim su Outpost regolare, editor ok. **Da verificare a mano:**
  niente più unità sopra gli speeder su Outpost; slider RotY sulla pistola
  (raddrizza in viewport → uguale in sandbox); TAB → Mappa → riavvio su Outpost.

## 2026-07-10 (21) — Seconda mappa "Outpost" + selettore mappa (R3 chiuso)
- **R3 chiuso**: nessun game mode carica più "firebase" hardcoded. La mappa attiva
  viaggia in `MatchSettings.mapId` (risolta da Application); ConquestMode/SandboxMode
  hanno `m_mapId` da `applySettings` ("firebase" resta solo come fallback di default,
  nota aggiornata anche nel rename tool).
- **Selettore mappa nel PreMatch** (pagina Regole, riga "Mappa" con nomi dinamici
  dal registry — stesso pattern enum della Modalità); `map_index` salvato nei preset.
- **Nuovo flag CLI `--map <id>`**: mappa iniziale per sandbox/sim (test e debug).
- **Nuova mappa `data/maps/outpost.json`**: corridoio 30x64 con avamposto centrale
  rialzato, 3 post in linea (Nord/Centro/Sud), bunker sfalsati, strozzature laterali,
  metadata completi (4 cover, 2 danger zone sulle strozzature, ronda del centro a 6
  punti), 2 speeder, roster auto (liste vuote).
- Verifica: `--sim --map outpost` 45s → 22 hit, 1 kill, veicoli alle coordinate
  outpost, AI in patrol/alert/search — la mappa nuova funziona senza alcun codice
  dedicato, che era il vero test del "tutto data-driven".
- **Da verificare a mano:** partita su Outpost dal PreMatch (riga Mappa), feel del
  layout; la riga Mappa nei preset.

## 2026-07-10 (20) — Fase B veicoli, tranche 1: danno a sagoma piena + pilota protetto
- **Danno al veicolo su tutta la sagoma**: nuovo test segmento-vs-OBB del box del
  mezzo in CombatSystem (via `hittest::segmentInZone` con zona fittizia) — prima
  contava solo la sfera `k_hitRadius` al centro e i colpi ai bordi si fermavano sul
  collider senza infliggere danno. Zona telemetria/log chat: "veicolo".
- **R5 chiuso — pilota protetto**: il CombatSystem raccoglie i driver correnti
  (da `VehicleComponent.driver`) e li salta come bersagli diretti: finché guidi, i
  colpi danneggiano il MEZZO, non te. Alla distruzione vieni sganciato illeso
  (danno residuo al pilota: raffinamento futuro dichiarato in 19 Fase B).
- Build pulita; `--sim` 30s regolare. **Da verificare a mano:** sparare allo speeder
  ai bordi (ora fa danno, righe `zona=veicolo` nel log/chat); farsi sparare mentre
  si guida (gli HP del giocatore non calano, il mezzo sì).

## 2026-07-10 (19) — Rifinitura R4: VehicleEditor sul DefinitionRegistry
- `VehicleEditor::loadEntries` ora carica dal `DefinitionRegistry` (stesso parse del
  runtime) invece del parse JSON duplicato riga per riga: l'editor mostra ESATTAMENTE
  ciò che il gioco caricherà, per costruzione.
- Analisi R4 completata: Entity/Map editor restano su parser propri per scelta —
  leggono campi editor-only (label/type dei box, stato di editing) che il runtime
  non carica; unificarli significherebbe sporcare gli schema runtime. Documentato
  in 06_Todo R4 (chiuso salvo nuovi duplicati).
- Build pulita; GFEditor smoke ok.

## 2026-07-10 (18) — Rifinitura R1+R2+R6 (dalla diagnosi (17))
- **R1 — Spread e gittata del giocatore ATTIVI**: `Weapon` runtime porta i 5 spread +
  effective_range dal WeaponDef (`weaponFromDef`); in `updateShooting` la direzione
  viene dispersa per stato (fermo/movimento/corsa/aria; la mira col tasto destro
  scende all'adsSpread da fermi, riduce del 60% negli altri stati) e il lifetime del
  proiettile è cappato a `WEAPON_RANGE_GRACE(2.0) * effective_range / bullet_speed`
  (GameConfig — niente falloff del danno per ora, dichiarato). Tutti i valori del
  BalanceEditor tab Armi ora contano davvero per il feel.
- **R6 — Spawn veicoli deduplicato**: nuovo `game/VehicleSpawn.hpp`
  (`vehiclespawn::spawnFromMap`), usato da Conquest e Sandbox (erano 2 copie).
  Nota dal log: il secondo speeder ora spawna a (-5,-11) invece di (-5,-14) —
  conferma che PRIMA nasceva dentro un ostacolo.
- **R2 — Guida estratta da Application**: nuovo `game/VehicleDrive.hpp`
  (`vehicledrive::update`: input, sterzo, slide/step-up con excludeId, gravità,
  camera FPS/TPS, telemetria `drive:`); Application gestisce solo mount/dismount
  e messaggi. Application.cpp: 1120 → 1057 righe.
- Build pulita; `--sim` 30s regolare (8 hit, 1 kill, veicoli spawnati decollisi).
  **Da verificare a mano:** il feel dello spread (corsa vs mira), la gittata
  (i colpi svaniscono oltre ~2x il range effettivo dell'arma), guida invariata.

## 2026-07-10 (17) — Diagnosi pre-rifinitura (nessun cambio di codice)
- Audit mirato del progetto su richiesta utente. Trovate 7 voci di rifinitura,
  registrate in 06_Todo sezione "Rifinitura" (R1-R7): spread/gittata armi mai
  consumati dal player (R1, il più impattante sul feel), Application.cpp 1120 righe
  (R2), "firebase" hardcoded nei mode (R3), 3 parser JSON divergenti nell'editor
  (R4), pilota colpibile dentro il veicolo (R5), spawn veicoli duplicato (R6),
  igiene data/ (R7). Nessun difetto bloccante: build pulita, sim regolare.

## 2026-07-10 (16) — Proiettili fermati dai muri + veicoli solidi
- **BUG scoperto in diagnosi: i proiettili attraversavano i muri.** Nessun sistema li
  testava contro i ColliderComponent (solo contro le entità con HP): il giocatore
  poteva sparare attraverso le coperture e i colpi AI con spread passavano i muri.
  Fix in CombatSystem: se il segmento del tick attraversa un collider e non ha colpito
  un'entità, il proiettile muore lì (`physics::hasLineOfSight` riusato). Limite
  documentato: bersaglio e muro nello stesso segmento (~0.9m) → vince il bersaglio.
- **Veicoli solidi** (Fase B parziale, 19_Vehicles): i mezzi hanno ora un
  ColliderComponent → fanteria e AI non li attraversano, bloccano la linea di vista
  e fermano i proiettili (il danno al mezzo resta sul test-entità: hitbox veicoli
  vere in Fase B). `physics::hasCollision/slideMove/slideMoveWithStepUp` hanno un
  nuovo param `excludeId` (default 0 = comportamento invariato) usato dalla guida
  per non collidere col proprio collider.
- Build pulita; `--sim` 45s: 20 hit, 2 kill — combat vivo con i muri solidi.
  **Da verificare a mano:** sparare a una cassa/muro (il colpo si ferma, niente hit
  dietro); non poter più attraversare a piedi lo speeder; guida invariata.

## 2026-07-10 (15) — KI #13 risolto: hit test OBB condiviso mirino/proiettili
- Diagnosi da ProjectDocs: dopo il checkpoint, i difetti di codice aperti erano KI #13
  (rotazione zone ignorata nel combat) più due voci stale (#3, #5).
- **Nuovo `include/mini/physics/HitTest.hpp`** (header-only): `segPointDistSq`,
  `segAABB`, `segmentInZone` OBB-aware — il segmento viene portato nello spazio
  locale della zona (yaw entità * `eulerDeg` zona, ordine Y*X*Z come il wireframe
  editor) e testato contro ±halfExtents. Zone a euler zero: comportamento identico
  a prima.
- CombatSystem usa l'helper (rimosse le copie locali); il **mirino** in Application
  ora usa LO STESSO `segmentInZone` (raggio = segmento di 80m) — rimosso il
  `rayAABB` locale: mirino e proiettili concordano per costruzione, anche sulle
  zone inclinate (testa B1 a -58°).
- KI chiusi: **#13** (risolto), **#3** (assorbito dal fix churn FBO di #17),
  **#5** (Clone Trooper: risolto dall'utente il 2026-07-04, voce rimasta aperta
  per svista).
- Build pulita; `--sim` 30s: 10 hit, 1 kill — combat regolare col nuovo test.
  **Da verificare a mano:** headshot sulla testa inclinata del B1 in sandbox
  (mirino rosso e colpo devono coincidere anche ai bordi della zona).

## 2026-07-10 (14) — Viewmodel arma del giocatore (Todo #11 completo) + #10 chiuso
- **Todo #10 verificato già implementato**: `WeaponAttach` usa `WeaponDef.gripAttach`
  (attach "right_hand"/"grip", con right_hand prioritario) — resta solo autorare i
  punti nei GLB delle armi dal Weapon Editor (attività dati, non codice).
- **Viewmodel prima persona**: l'arma equipaggiata del giocatore è ora visibile in
  basso a destra dello schermo (mesh dal `WeaponDef.meshPath` via meshCache, offset
  camera-relative, yaw 90° per la convenzione GLB lungo +X, scala dal def). Attivo
  solo in FPS: niente viewmodel in TPS, alla guida, da osservatore o da morto.
  `Weapon` runtime ora porta `meshPath`/`meshScale` (copiati in `weaponFromDef`).
- Limite noto (classico dei viewmodel senza depth-hack): l'arma può compenetrare i
  muri a distanza ravvicinata — accettato per la Fase 1.
- Build pulita; sandbox smoke ok. **Da verificare a mano:** arma visibile in FPS
  (E-5/DC-17 hanno mesh; armi senza `mesh` nel JSON non mostrano nulla), cambio arma
  1-9/menu TAB aggiorna il modello, posizione/scala gradevoli (tarabili dal Weapon
  Editor con mesh_scale).

## 2026-07-10 (13) — CHECKPOINT: allineamento documentazione + preset modeIndex
- **Audit docs vs codice** (richiesto dall'utente prima della fase di rifinitura):
  - 10_ProjectMemory: indice Planned Feature riscritto (15/16/17/18/19 e ADR-010/011
    risultavano ancora "not yet"); aggiunti i vincoli confermati della sessione
    (input sintetici non arrivano a SDL → diagnosi via telemetria; pattern mailbox
    su World; risorse GL only-grow su aree ImGui oscillanti; liste roster vuote=auto).
  - 02_FileStructure: sezione nuovi file (vehicles/, SandboxMenu, Shield/Vehicle
    Component, VehicleEditor, doc 16-19) + flag CLI `--sim`.
  - 03_SystemReference: riferimento rapido dei sistemi aggiunti (shield, AI tattica,
    log chat, sandbox tools, metadata+consumo, veicoli, HUD, diagnostica); nota
    "Planned tooling ADR-010" corretta in IMPLEMENTATO.
- **Preset partita: salvato anche `mode_index`** (Conquista/Assalto/Difesa) — era il
  "minor" residuo di ADR-014; preset vecchi senza campo = Conquista, valore clampato.
- Build pulita; `--sim` regolare. Stato: **checkpoint raggiunto** — tutti i sistemi
  Fase 1 in piedi e documentazione allineata; pronta la fase di rifinitura (smoke
  manuali pendenti: guida veicoli, shield in chat, KI #17 memoria editor in uso).

## 2026-07-10 (12) — Fix incastri veicoli + roster firebase in auto
- **Spawn veicoli decolliso**: `findFreeSpot` (stessa decollisione della fanteria, con
  gli half del veicolo) in Conquest e Sandbox — lo spawn lato nemici finiva in parte
  dentro una barricata e il mezzo nasceva incastrato.
- **Dismount sicuro**: scendendo si sceglie il primo lato LIBERO attorno al mezzo
  (destra/sinistra/dietro/davanti, check `hasCollision` con gli half del giocatore) —
  prima 2.2m a destra alla cieca, anche dentro un muro.
- **firebase in modalità auto**: `enemy_types`/`ally_types` svuotati → il runtime usa
  TUTTE le definizioni registrate (round-robin ordinato). Il nuovo "Heavy clone
  trooper" (e ogni entità futura) entra in partita/sim senza altri passaggi; il
  pattern alternato B1/Heavy resta identico perché l'auto alterna i 2 tipi registrati.
  Pattern espliciti ricreabili in BalanceEditor → Mappe.
- Analisi problemi registrata in 19_Vehicles Out of Scope: niente respawn dei mezzi
  distrutti; i veicoli non fanno da ostacolo a fanteria/altri mezzi (si attraversano).
- Build pulita; `--sim` regolare. **Da verificare a mano:** veicolo nemico non più nel
  muro; Heavy clone trooper in sim (alza "Alleati AI" ad almeno 2 nel menu TAB).

## 2026-07-10 (11) — KI #17: fix churn FBO nel viewport editor
- Misura baseline: GFEditor sulla Home è PIATTO (67MB stabili 75s) → il leak
  segnalato (73→259MB/min) vive nei moduli col viewport 3D.
- Root cause: `resizeFBO` distruggeva e ricreava FBO+texture+RBO a ogni variazione
  di dimensione dell'area del pannello — che può oscillare di pochi px tra frame
  (scrollbar/separatori) → churn di risorse GL a 60Hz. Era il sospetto storico #3.
- Fix: la texture di rendering è allocata a multipli di 64px e viene SOLO
  ingrandita; il pannello mostra la sub-regione corretta via UV (flip incluso).
  Ogni realloc reale è loggato (`[Viewport] Realloc FBO ...`) — se ne vedi una
  raffica continua nel log console, il bug è altrove.
- Build pulita; editor stabile 45s con viewport-modulo default. **Da confermare:**
  sessione d'uso reale nei moduli (Entity/Map/Vehicle) con memoria heartbeat piatta.

## 2026-07-10 (10) — TPS in veicolo, roster mappa completo, modulo Vehicle Editor
- **Terza persona alla guida**: con V attivo la camera sta dietro/sopra il mezzo
  (offset dal forward del veicolo, lookAt sul mezzo); prima persona invariata.
- **Roster per mappa unificato (BalanceEditor → Mappe)**: la UI degli slot con
  dropdown+pattern ora vale ANCHE per gli alleati (`ally_types`, prima non editabile);
  aggiunti "Alleati in campo" (`ally_count`), pattern "Uno per ogni definizione" e
  "Svuota (auto)". Regola resa esplicita nella UI: **lista vuota = automatico, il
  runtime usa tutte le definizioni registrate** (fallback ADR-007) — è così che le
  nuove entità entrano in partita/sim senza toccare le mappe. `saveMap` ora scrive
  anche `ally_types`/`ally_count` (prima andavano persi al salvataggio!).
- **Nuovo modulo "Vehicle Editor"** (card in Home + menu Moduli): lista/creazione/
  rinomina (sweep `vehicle_spawns`), statistiche di guida, modello 3D con browse e
  **anteprima nel viewport** con il box di collisione in wireframe sovrapposto —
  si vede subito se il box combacia col modello. La tab Veicoli del BalanceEditor
  (temporanea, di ieri) è stata rimossa: unico posto di editing. Hitbox a zone e
  attach point per veicoli: Fase B dichiarata (19_Vehicles).
- Build pulita; GFEditor smoke 8s ok; `--sim` regolare con veicoli spawnati.
  **Da verificare a mano:** V alla guida; nuova entità → sim (con lista vuota o
  aggiungendola al roster); Vehicle Editor (crea, mesh, box, salva, rinomina).

## 2026-07-10 (9) — Veicoli: feedback/diagnostica + tab Veicoli nell'editor
- **Colore al mount**: il veicolo guidato diventa blu, al dismount torna al colore del
  suo VehicleDef (era il "resta rosso" segnalato — il feedback non esisteva).
- **Mount in terza persona**: il raggio ora è misurato da `tpsPlayerPos`, non dalla
  camera (in TPS la camera è arretrata: E poteva fallire pur essendo accanto al mezzo).
- **Diagnostica guida in telemetria**: `drive: v=... pos=... [BLOCCATO]` ~2/s alla
  guida, e `veicolo: E premuto, nessun mezzo in raggio (min Xm)` sui tentativi falliti.
  Il bug "W/S non muove" NON è riproducibile in automazione (gli input sintetici non
  raggiungono la finestra SDL senza focus reale): con queste righe il prossimo test
  manuale identifica la causa dal log. Uno spawn firebase spostato accanto alla base
  (2, 15.5) per testare al volo.
- **BalanceEditor: nuova tab "Veicoli"** — lista, creazione, nome, statistiche
  (HP/vel/accel/sterzata), mesh con browse file + scala/rotY, half extents, colore,
  salvataggio RMW, **Rinomina** con sweep dei `vehicle_spawns[].vehicle_id` nelle
  mappe (nuova `Category::Vehicle` in DefinitionRename, ADR-010).
- Build pulita; GFEditor smoke ok (8s), engine ok. **Da verificare a mano:** guidare
  lo speeder accanto allo spawn e, se non si muove, mandare le righe `drive:` del log;
  tab Veicoli (creare/salvare/rinominare).

## 2026-07-10 (8) — Veicoli Fase A (nuovo doc 19_Vehicles) + fix EntityEditor
- **EntityEditor: "+ Nuova entita'"** — campo nome + combo Nemico/Alleato sotto la
  lista; crea il JSON minimo (name/faction/stats/weapons/abilities) via saveJsonRMW
  con id = nome file (ADR-001), poi ricarica e seleziona.
- **Veicoli (Fase A, doc 19)**: `VehicleDef` (data/vehicles/<id>.json: hp, max_speed,
  accel, turn_rate_deg, half extents, colore) + loader/getter nel registry;
  `MapDef.vehicleSpawns` (chiave `vehicle_spawns`, additiva); spawn nei mode
  (Conquest+Sandbox: entità con Transform/Health/Team 0/VehicleComponent, box di
  fallback come mesh); **E** sale/scende (raggio in GameConfig), W/S accelera/frena,
  A/D sterza (invertito in retro), slide+step-up e gravità della fanteria, camera al
  posto di guida, mouse look invariato. Alla guida non si spara (Fase A). Salendo il
  veicolo passa a team 1 (bersagliabile), scendendo torna neutro. Se esplode sotto il
  giocatore: dismount automatico + toast.
- Dati di prova: `BARC Speeder` + 2 spawn su firebase (uno per base).
- Limite route annotato in 18 (ostacolo tra punti → inversione, serve pathfinding).
- Build pulita; smoke: sandbox ok, `--sim` spawna 2 veicoli e la battaglia resta viva.
  **Da verificare a mano:** salire (E vicino allo speeder), guidare per la mappa,
  scendere, farlo esplodere sotto di sé; "+ Nuova entita'" nell'EntityEditor.

## 2026-07-10 (7) — L'AI consuma i Map Metadata (nuovo doc 18_AiMapConsumption)
- **Canale dati**: `World::activeMap` (fwd decl `MapDef`, pattern mailbox — l'ECS non
  include header di gioco), settato da ConquestMode/SandboxMode in `start()`, azzerato
  in `World::initialize()`.
- **Cover point**: in fase "hide" l'AI sceglie il cover più vicino (≤12m) col fronte
  orientato verso il nemico, lo raggiunge e ci resta fino al prossimo peek; senza
  cover resta lo strafe evasivo. `height` non guida ancora pose (Todo #24).
- **Danger zone**: repulsione nel movimento fuori ingaggio (pesata su dangerLevel e
  vicinanza al centro); in Alert non si applica.
- **Patrol route**: se la mappa ne ha, ConquestMode assegna alle unità segmenti
  consecutivi delle route (round-robin) al posto dei waypoint verso i post. Limite
  documentato: AiComponent ha 2 waypoint → un segmento per unità.
- **firebase.json**: set minimo di metadata di prova (4 cover, 1 danger sul campo
  aperto est, route "perimetro_alpha" a 3 punti) — rifinibili dal Map Editor.
- Note utente registrate: cover più ricche/pose FPS → 15 Future Expansion + Todo #24;
  shape/collision oltre i box → Todo #23. Chiarimento: una route = PIÙ punti in
  sequenza (un punto solo non è un percorso).
- Build pulita; smoke `--sim` 50s: 33 hit, 3 kill, metadata parse-ati senza errori.
  **Da verificare a mano:** in sim, AI che si appostano ai cover durante il hide,
  pattuglia sul perimetro Alpha, evitamento della zona pericolosa a est.

## 2026-07-10 (6) — Map Metadata implementato (15_MapMetadata: schema+loader+authoring)
- `MapDef` esteso con `coverPoints[]` (x/y/z, facing_deg, height), `patrolRoutes[]`
  (id + points ordinati), `dangerZones[]` (x/y/z, radius, danger_level 0..1) — additivi,
  vuoti di default, zero impatto sulle mappe esistenti.
- `DefinitionRegistry::loadMaps`: parse delle nuove chiavi `cover_points`/
  `patrol_routes`/`danger_zones`; il log `[Registry] Map:` ora riporta i conteggi.
- MapEditor: sezione **Metadata AI** nella lista (cover verde-acqua con "naso"
  direzionale e altezza, danger zone disco arancione→rosso col livello, route con
  pilastrini viola e punto attivo evidenziato), selezione con range dedicati
  (-100/-200/-300), gizmo Sposta per cover/danger/punti route, pannelli proprietà a
  sliderRow, salvataggio via saveJsonRMW insieme a geometry/command_posts.
- **Come da Out of Scope del doc: nessun consumo AI** — il consumer è il lavoro
  tattico fase 2 (andrà documentato a parte prima di implementarlo).
- Build pulita; smoke: engine ok, GFEditor aperto 8s senza crash. **Da verificare a
  mano:** authoring completo su firebase (piazzare cover/route/danger, salvare,
  ricaricare, controllare il JSON).

## 2026-07-10 (5) — Sandbox semplificata: sim come prima classe, partita via PreMatch
- **Spiegato il "la partita non funziona"**: la pagina Partita del menu sandbox
  riavviava la SANDBOX (manichini fermi e ticket 999/0 by design), non una partita
  vera — confusione di responsabilità, non un bug del combat. Decisione (utente):
  la partita vera NON si avvia dentro la sandbox.
- **SandboxMenu ridotto a 2 pagine**: *Armi* (slot primaria/secondaria) e
  *Simulazione* completa — modalità (Conquista/Assalto/Difesa), alleati/nemici AI,
  ticket per team, respawn. La pagina Partita è stata rimossa.
- **Scorciatoia P** in sandbox → apre il PreMatch classico (loadout/regole/preset)
  per giocare una partita vera con il flusso standard.
- **Bugfix**: `startGame` ora resetta observerFly/simRunning/menu aperto — una
  partita avviata dopo una simulazione non eredita più il volo libero.
- Build pulita; smoke `--sim` (28 hit, 3 kill in 40s) e `--sandbox` ok.
  **Da verificare a mano:** P → PreMatch → partita completa; sim Assalto/Difesa
  con ticket personalizzati.

## 2026-07-10 (4) — FIX battaglia AI "spenta" + rifiniture Sandbox Tools
- **BUG "AI ferme come manichini" RISOLTO** (diagnosi empirica con il nuovo flag CLI
  `--sim`, che avvia direttamente la simulazione AI, + heartbeat `ai:` in telemetria).
  Tre cause concorrenti in AiSystem:
  1. `pickSearchPoint` usava coordinate GLOBALI hardcoded dell'arena pre-firebase
     (-8..+8): su firebase 50x40 tutte le AI convergevano al centro contro i muri.
     Ora cerca attorno alla lastKnown (±12m, mappa-agnostico).
  2. Search era uno stato senza uscita: dopo il primo contatto condiviso nessuno
     tornava MAI in pattuglia sui post. Ora dopo 15s infruttuosi → Patrol.
  3. Il roll evasivo (peek/hide) poteva sopprimere il primo colpo all'acquisizione.
     Ora una nuova acquisizione garantisce sempre una finestra di fuoco piena.
  Verifica: 50s di `--sim` → 28 hit, 3 kill, stati che ciclano patrol/alert/search.
- **Diagnostica permanente**: `[Conquest] spawn: N nemici, M alleati...` a ogni start;
  heartbeat `ai: N (patrol/alert/hunt/search/fermi)` ogni ~10s in telemetria.
- **Sandbox base**: almeno un manichino per OGNI definizione registrata (round-robin
  su enemies/allies ordinati) — ogni nuovo JSON è subito visibile in sandbox.
- **Menu sandbox**: pagina Armi con slot < PRIMARIA / SECONDARIA > (SIN/DES);
  pagina Partita estesa (ticket team1/team2, respawn delay); pagina Simulazione con
  scelta modalità (Conquista/Assalto/Difesa) — anche Assalto/Difesa ora osservabili
  AI-vs-AI. Gadget: rimandati a quando esisteranno lato giocatore (17 Out of Scope).
- **Log chat scorrevole**: PAGSU/PAGGIU nel pannello (storico portato a 200 righe);
  la vista resta ferma sui messaggi vecchi mentre ne arrivano di nuovi.
- Build pulita; smoke `--sim` (battaglia viva) e `--sandbox` ok. **Da verificare a
  mano:** feel della battaglia osservata, slot secondaria, scroll log, sim Assalto.

## 2026-07-10 (3) — Sandbox Tools (nuovo doc 17_SandboxTools)
- **Menu sandbox (TAB in partita, solo `--sandbox`)** — overlay Ui2D a 3 pagine
  (`SandboxMenu`, nuovo in render/): *Armi* (lista completa dal registry, scrollabile,
  INVIO equipaggia — supera il tetto dei tasti 1-9, che restano come scorciatoia);
  *Partita* (manichini alleati/nemici 0-10, HP giocatore, "Applica e riavvia" via
  `MatchSettings` → `SandboxMode::applySettings` ora legge anche team1/2AiCount);
  *Simulazione* (INVIO avvia/ferma).
- **Simulazione AI-vs-AI con osservatore**: crea una Conquista via factory, il player
  diventa team 0 (le AI lo ignorano), esito partita sospeso, camera in **volo libero**
  (WASD + SPAZIO/CTRL, velocità 14) sganciata dal PlayerController. Fermandola si torna
  alla sandbox normale.
- **Log chat in-game**: mailbox `World::eventFeed` (pushEvent dai sistemi: hit con zona
  e danno, assorbimenti scudo, kill; eventi sandbox da Application) drenata ogni frame
  nella HUD. Ultime 4 righe in basso a sinistra con fade (6s); **L** apre il pannello
  con lo storico (60 righe conservate). Attiva in tutte le modalità.
- Mentre il menu sandbox è aperto il giocatore non spara/si muove; il mouse-look è
  sospeso.
- Build pulita; sandbox smoke ok. **Da verificare a mano:** TAB→pagine e equip; riavvio
  con conteggi custom; simulazione (AI che combattono, volo, L per log, TAB per
  fermarla); shield sul B1 Heavy ora osservabile in chat ("SCUDO #id assorbe N").

## 2026-07-10 (2) — Shield end-to-end + tab Abilità + HUD top + mouse nel menu
- **Perché lo shield "non funzionava":** nessuna unità lo referenziava e non poteva
  essere assegnato — l'EntityEditor caricava/salvava `abilities[]` ma NON aveva UI; in
  più `SandboxMode::spawnDummy` non risolveva le abilità (solo ConquestMode). Fix:
  sezione "Abilita'" in EntityEditor (combo dal registry, + / X, slot vuoti filtrati al
  save) e risoluzione shield anche sui manichini sandbox. `B1 Heavy Droid` ora
  referenzia "Shield" nei dati (assegnazione di prova, modificabile dall'editor).
- **BalanceEditor: nuova tab "Abilita'"** — lista, creazione, nome, tipo da elenco
  (shield/roll/melee/jetpack/missile/command_aura, con nota su cosa è attivo nel
  runtime), param1/2/3 con etichette contestuali per shield, cooldown, passiva.
  Salvataggio via saveJsonRMW, `id` deprecato rimosso (ADR-001).
- **HUD alto ridisegnato:** i riquadri dei command post coprivano la riga ticket/vivi.
  Ora due pannelli fazione (ALLEATI blu a sinistra, NEMICI rosso a destra) ai lati
  dello spazio centrale riservato ai post: nessuna sovrapposizione possibile.
- **Mouse nei menu (primo passo):** MainMenu — hover evidenzia la voce, click sinistro
  attiva (geometria condivisa render/hit-test). PreMatch/Options restano da tastiera;
  estensione futura. Nota: coordinate mouse in spazio finestra 1:1 con la Ui2D 1280x720;
  in fullscreen con risoluzioni diverse potrebbe servire uno scaling (da verificare).
- Build pulita; sandbox smoke ok. **Da verificare a mano:** colpi su B1 Heavy → righe
  `shield:` nel log e morte ritardata; tab Abilità; HUD in Conquista; click nel menu.

## 2026-07-10 — AI: profilo tattico completo + ability shield (Todo #3, doc 16_AiBehavior)
- Nuovo Planned Feature doc `16_AiBehavior.md` (prerequisito CLAUDE.md §5), scope 1-5
  implementato nello stesso change set.
- AiComponent: campi tattici dal profilo (aggression, retreatHpThresh, coverPreference,
  peek/hide range, flankChance) + stato runtime (exposeTimer/evading/flank*). Risolti in
  `ConquestMode::spawnUnit` come i campi già esistenti; inclusi nel template di respawn.
- AiSystem: distanza d'ingaggio preferita da aggression (3-12m, arretra se troppo vicino);
  ritirata sotto retreat_hp_threshold (arretra sparando); ciclo peek/hide (in hide non
  spara, strafe evasivo); flanking all'ingresso in Hunt (punto laterale ~6m, poi lastKnown).
- Ability "shield" runtime: nuovo `ShieldComponent` (World storage completo); assegnato
  allo spawn se `abilities[]` dell'unità referenzia un AbilityDef con type "shield";
  CombatSystem: assorbimento prima degli HP + regen dopo regenDelay; telemetria per colpo.
  Nota: `AbilityComponent.hpp` resta uno scaffold non collegato (servirà per le abilità
  attive, Out of Scope per ora).
- Build pulita; sandbox smoke ok. **Da verificare a mano:** partita con droidi — distanze
  d'ingaggio diverse tra profili, pause di fuoco (hide), fiancheggiamenti; per lo shield
  assegnare "shield" a un'unità dall'EntityEditor e verificare assorbimento nel log.

## 2026-07-09 (12) — Spike split-screen (ADR-011 → Accepted, esito a) + fix riga Modalità
- PreMatchMenu: le righe enum (con `names`) non disegnano la barra di progresso — il testo
  ("Conquista" ecc.) finiva sotto la barra; freccia ">" spostata per far posto al nome.
- Renderer: `drawMeshFrom(const Camera&, ...)` (drawMesh vi delega), `setViewportRect`,
  `getDrawableSize`. Nessun cambio a shader/frame lifecycle (ADR-003 non toccato).
- Application: F9 in partita attiva lo spike — scena renderizzata due volte in viewport
  sinistro/destro con seconda Camera (copia + offset laterale); viewport ripristinato full
  prima di HUD/menu. Loop entità estratto in lambda `drawScene(const Camera&)`.
- Esito spike: **(a) fattibile, modifiche minori** — registrato in ADR-011 (ora Accepted),
  KnownIssues #12 chiuso, Todo #16 done.
- Build pulita; sandbox smoke ok (avvio, registry, mode, nessun errore GL nel log).
- **Da verificare a mano:** F9 in partita → due viste affiancate corrette, HUD intatto al
  ritorno; riga "Modalità" nel PreMatch senza barra e senza sovrapposizioni.

## 2026-07-09 (11) — Assalto/Difesa + HUD command post (ADR-014; Todo #4 e #6)
- `MatchOutcome` + `outcome()` nel mode (vittoria/sconfitta non più hardcoded in
  Application); hook `updateObjectiveRules` in ConquestMode; `AssaultMode`/`DefenseMode`
  in ObjectiveModes.{hpp,cpp} (factory: "assault"/"defense"); ownership iniziale post
  forzata dalla modalità (`CommandPosts::forceAllOwners`); selezione modalità nel
  PreMatch (riga "Modalita' di gioco", Row con etichette); HUD: barra post in alto
  (colore proprietario + lettera + progresso cattura del team che cattura).
- Dettagli e regole complete in ADR-014. Build pulita, sandbox smoke ok.
- **Da verificare a mano:** partita Assalto (post rossi all'avvio, ticket che calano,
  vittoria alla cattura del terzo post) e Difesa; leggibilità barra post.

## 2026-07-09 (10) — Sandbox: selettore armi 1-9 (Todo #0)
- In sandbox i tasti **1-9** equipaggiano l'arma corrispondente dal registry (lista
  completa ordinata per nome, incluse le armi separatiste — è un banco di prova).
  Toast col nome dell'arma, hint "tasti 1-9" all'avvio (5s), cambio loggato in telemetria.
  Attivo SOLO in sandbox (in partita resta il loadout del PreMatch).

## 2026-07-09 (9) — Anti-tunneling proiettili + hitmarker solo del giocatore
- **Hitbox "riconosciute dal mirino ma non colpite" — causa: tunneling.** I proiettili si
  muovono a step discreti (a 55 m/s ≈ 0.9 m per tick a 60 Hz) e il test era PUNTUALE:
  le zone piccole (testa B1: 0.12×0.44×0.15) venivano attraversate tra un tick e l'altro
  senza mai contenere il punto. Il mirino (raycast continuo) diceva giustamente
  "colpibile". Ora il CombatSystem testa il **segmento percorso nel tick** (posizione
  precedente ricavata dalla velocità → attuale): `segAABB` per le zone,
  `segPointDistSq` per broad-phase e fallback sferici. Mirino e proiettili ora
  concordano per costruzione.
- **Hitmarker solo per i colpi del GIOCATORE:** era legato a ownerTeam==1, quindi
  scattava anche per i colpi degli alleati AI. Nuovo flag `BulletComponent.fromPlayer`
  (true solo in PlayerController); il CombatFeedback lo usa. KI #13 (rotazione zona
  ignorata) resta valido anche per il test a segmento.

## 2026-07-09 (8) — Fix mode sandbox→partita + feedback a schermo (F12, mira, hitmarker)
- **Bug: sandbox → menu → nuova partita spawna manichini fermi.** Il game mode era creato
  UNA volta dal flag CLI: avviando con --sandbox e poi facendo Nuova Partita, initWorld
  riusava la SandboxMode (dummies senza AI). Ora `startGame()` ricrea SEMPRE il mode
  "conquest" (residuo ADR-008 annotato: in futuro l'id verrà da MapDef/PreMatch).
- **HUD feedback (nuove API `tick/setAimOnTarget/hitmarker/toast`):**
  - **Toast a schermo**: F12 ora mostra "F12: stato salvato in _telemetry_data/..." in
    alto al centro per 2.5s (prima il feedback era solo su terminale/log — in fullscreen
    invisibile). Nota Fn: dai log il tasto ARRIVA come F12 liscio su questo hardware.
  - **Mirino reattivo**: diventa ROSSO quando punta una hitbox nemica reale (ray-AABB con
    le stesse trasformazioni del CombatSystem: scala/yaw/meshOffset; fallback sfera 0.7).
  - **Hitmarker**: 4 tacche diagonali al colpo a segno (giallo=hit 0.18s, rosso=kill 0.45s)
    via `World::combatFeedback` (mailbox minimale scritta dal CombatSystem, consumata da
    Application — niente event bus).

## 2026-07-09 (7) — Prima diagnosi VIA telemetria: F12, log condiviso, clobber hitbox
- **F12 "non funziona" — smentito dai log:** input_history.log mostra 3 pressioni ricevute
  (frame 1627/1742/2455) e game_state.json scritto 3 volte. Problema reale: zero feedback
  visibile → ora il dump stampa "[F12] game_state.json scritto (frame N)" sul terminale.
- **Bug trovato DAI log: file condiviso tra processi.** Editor ed engine giravano insieme
  scrivendo lo stesso engine_run.log (truncate reciproco + righe intrecciate). Ora log
  per-app: engine_run.log / editor_run.log (+ editor_input_history.log). Verificato con
  entrambe le app simultanee.
- **Osservazione dai log (KnownIssues #17):** la memoria del GFEditor cresce 73→259 MB in
  ~1 minuto di uso. Possibile leak (sospetti: reload modelli viewport). Da profilare.
- **Incidente dati #2 — profilo hitbox B1 svuotato (causa del "i nemici non muoiono nel
  sandbox"):** cambiando `hitbox_profile` nel combo dell'EntityEditor, le zone in editing
  NON venivano ricaricate dal profilo selezionato → salvando si scrivevano le zone del
  profilo precedente (vuote, nel caso Heavy→B1) sul profilo condiviso. Con il profilo
  vuoto, i colpi alla testa cadevano nel fallback sferico (r=0.7 dal centro) → miss.
  **Recuperato dal `.bak` automatico (primo salvataggio reale del paracadute ADR-010)** +
  fix: il combo ora ricarica le zone dal profilo selezionato (`loadZonesFromProfile`).
- **CombatSystem su telemetria:** ogni hit (zona/moltiplicatore/danno/hp) a TRACE e ogni
  kill a INFO nel log — la prossima "non muoiono" si legge dal file.

## 2026-07-09 (6) — Telemetria e debugging estremo (ADR-013)
- Nuovo modulo `mini::telemetry` in entrambi i binari; artefatti SOLO in
  `_telemetry_data/` (auto-creata, gitignored): `engine_run.log` (spdlog, TRACE su file /
  WARN+ su console), `game_state.json` (tasto F12: camera/stato/entità/ticket/memoria),
  `input_history.log` (tasti+mouse col numero frame), `crash_report.txt` (cpptrace:
  SEH + std::terminate → stack trace anche a terminale).
- CMake: spdlog v1.14.1 + cpptrace v0.7.3 via FetchContent; opzione `GF_ENABLE_ASAN`
  (OFF default; MSVC solo ASan — UBSan non esiste su MSVC, attivo solo su altri toolchain).
- Strumentati: Application (flag avvio, registry, game mode, F12, shutdown), Window,
  Renderer, InputManager (recorder), battito memoria ogni ~10s.
- Smoke: `_telemetry_data/` creata alla root, log popolato con livelli. Da provare con
  eventi reali: F12 (serve input in finestra) e crash report (serve un crash vero).

## 2026-07-09 (5) — Fix tab Hitbox invisibile + pannelli ridimensionabili
- **Tab Hitbox (EntityEditor):** il pannello proprietà partiva con SameLine DOPO lista e
  bottoni → veniva schiacciato a ~0px di altezza: "Danno x", rotazioni ecc. erano
  invisibili. Nuovo layout verticale: lista zone (120px) → bottoni → proprietà a piena
  larghezza (con -64px riservati alla barra Salva/Ripristina). Lista con prefisso [B] e
  moltiplicatore visibile.
- **Pannelli ridimensionabili (`ImGuiChildFlags_ResizeX`, size persistita nell'ini):**
  EntityEditor (lista entità + colonna centrale), BalanceEditor (4 liste), WeaponEditor
  (lista + pannello destro, viewport ricalcolato dinamicamente, pannello default 320px),
  MapEditor (lista + proprietà, default 260px). Trascina il bordo destro del pannello.
  I testi tagliati si risolvono allargando; i pannelli usano la larghezza reale
  (`GetContentRegionAvail`) invece di costanti.

## 2026-07-09 (4) — Hotfix: crash all'avvio del GFEditor
- **Regressione introdotta dal batch (3):** rimuovendo la card Hitbox dalla Home era
  rimasto `k_moduleCount = 8` hardcoded con 7 card nell'array → lettura out-of-bounds al
  primo frame → la finestra si apriva e chiudeva subito.
- Fix: conteggio derivato da `sizeof(k_modules)/sizeof(k_modules[0])` — un array e il suo
  count non possono più divergere. Editor verificato vivo dopo 8s di run.

## 2026-07-09 (3) — Consolidamento hitbox in Entity Editor (ADR-012) + pulizie
- **Gap colmato prima della rimozione:** `debug_visible` ora è nel modello InlineHitZone
  dell'EntityEditor (load dal profilo, checkbox in UI, salvato — prima era hardcoded true).
- **HitboxEditor RIMOSSO:** file cpp/hpp eliminati, tolto da CMake, EditorApp (enum,
  membro, tick, render, menu) e HomeScreen (card). L'authoring hitbox vive SOLO
  nell'Entity Editor (zone, danno, rotazioni, bone, wireframe, gizmo — tutto già presente).
- **Hardcoded rimosso:** fallback `"grunt"` in `ConquestMode::spawnUnit` eliminato (l'id
  profilo è sempre risolto a monte; senza profilo → fallback sferico CombatSystem).
- **Dati:** eliminati i profili orfani `grunt/heavy/sniper` da data/hitboxes (zero
  riferimenti); `*.bak` aggiunto a .gitignore e ripulito il .bak esistente.
- **BalanceEditor ripulito:** rimossi i tab vestigiali Nemici/Alleati (erano redirect
  read-only) e i relativi saveEnemy/saveAlly + membri. Tab restanti: Armi, AI, Mappe,
  Personaggio.
- Smoke: 3 profili hitbox validi caricati (B1 2 zone, Heavy 0, Clone 2), mappa integra.
- Nota (KnownIssues #16): rename di profili hitbox standalone senza UI — accettato.

## 2026-07-09 (2) — AI: salto, precisione, reazione dal profilo (Todo #3 parziale, #7)
- **Salto anti-ostacolo:** se l'AI sta provando a muoversi, è a terra ed è ferma da metà
  del tempo anti-stuck, salta (`AI_JUMP_IMPULSE` in GameConfig) PRIMA che scatti
  l'inversione di rotta — supera casse/coperture basse. Gated su `jump_enabled` del profilo.
- **Precisione:** i colpi AI ora hanno dispersione `(1-accuracy)*AI_SPREAD_MAX` (prima
  erano perfetti); RNG leggero deterministico locale, niente <random>.
- **Tempo di reazione:** primo colpo dopo una nuova acquisizione ritardato di
  `reaction_time` del profilo.
- **Plumbing:** `RespawnEntry/UnitTemplate.aiProfileId` risolto in `spawnUnit`
  (seekSpeed/jumpEnabled/accuracy/reactionTime dal `AiProfileDef`; prima seekSpeed era
  hardcoded patSpd+1.5). Il respawn conserva il profilo.
- **Dato (Todo #7):** creato `data/ai/grunt.json` — il Clone Trooper non logga più
  "AiProfileDef non trovato". Smoke: 3 profili caricati.
- **Deferito con motivazione (CLAUDE.md §5):** abilità runtime (shield/roll/jetpack...) e
  comportamento per ruolo (cover/peek/hide) sono un SISTEMA nuovo lato engine: richiedono
  prima un documento Planned Feature (template 14/15) con scope Overview/Goal/Out-of-Scope.
  Todo #3 aggiornato di conseguenza.

## 2026-07-09 — "Messa in regola": ADR-010 implementato (Accepted)
- **`saveJsonRMW`** (`editor/include/util/JsonSave.hpp`): helper centralizzato RMW + backup
  `.bak`; patchFn ritorna false = no-op (nessuna scrittura).
- **Migrati TUTTI i save path** all'helper: BalanceEditor ×6, EntityEditor (entità +
  profilo hitbox), WeaponEditor, HitboxEditor, MapEditor. Zero scritture JSON dirette.
- **`id`/`profile_id` deprecati**: rimossi dai JSON a ogni salvataggio (ADR-001: il nome
  file è l'unico id).
- **Comando Rinomina** (`util/DefinitionRename.{hpp,cpp}`, in CMake): validazione,
  `fs::rename`, sweep cross-reference con mappa esplicita per categoria, warning per la
  mappa "firebase" (caricata hardcoded dai mode — residuo ADR-008). UI in WeaponEditor,
  EntityEditor (reload deferito frame-safe), HitboxEditor, MapEditor.
- **Audit dropdown (Todo #2) PASSATO**: nessun InputText assegna id esistenti; i residui
  sono creazione nuovi id, nomi/etichette, path mesh (legittimi).
- Il duplicato armi del 2026-07-09 risultava già ripulito a mano (data/weapons: 7 file,
  nessun near-duplicate).
- Verifica: build pulita; smoke runtime ok (22 box, 3 post). **Pendente smoke GUI del
  rename** (KnownIssues #7).

## 2026-07-08 — Incidente dati + 4 fix (clobber BalanceEditor, fallback morto, scala arma)
- **INCIDENTE:** `BalanceEditor::saveMap` scriveva un JSON nuovo con i soli campi del vecchio
  schema → un salvataggio dal tab Mappe ha CANCELLATO geometry (22 box), command_posts e
  ally_* da firebase.json. Sintomi a cascata: player+AI cadono nel vuoto (niente collider;
  l'arena hardcoded di fallback non copre gli spawn a z=±16), niente post, e cloni-cubo
  (senza ally_types scattava il fallback hardcoded "clone_trooper", id inesistente).
- **Fix 1 — dati:** firebase.json ricostruito (22 box + 3 post) preservando gli edit utente
  (spawn_team1 z=16.34, enemy_types alternati, y dei post).
- **Fix 2 — RMW:** saveMap/saveWeapon/saveEnemy/saveAlly del BalanceEditor ora fanno
  read-modify-write. Regola resa vincolante in 04_CodingStandards.
- **Fix 3 — fallback ally:** rimosso l'id morto "clone_trooper"; fallback dagli id registrati
  (come ADR-007 per i nemici); zero alleati se il registro è vuoto.
- **Fix 4 — scala arma:** l'arma in mano ereditava la mesh_scale del personaggio → sul clone
  (0.011) diventava microscopica/invisibile, in editor E in gioco. Ora la scala della posa è
  compensata (`disp.scale / charScale`) in WeaponAttach e nell'anteprima EntityEditor
  (formule identiche).
- Smoke: "Map: firebase (geometry: 22 box, 3 command post)" + geometria e post caricati.

## 2026-07-04 (8) — Armi visibili in mano + AI che cattura i post (dwell)
- **Runtime weapon-in-hand** (chiude Todo "arma in mano"): nuovo
  `include/mini/game/WeaponAttach.hpp` — risolve mesh+posa dell'arma dai metadata editor
  (EnemyDef.attachPoints[mano] + weapon_display + WeaponDef.gripAttach), stessa formula
  dell'anteprima EntityEditor: `T(mano+offset)*R*S(scala)*T(-grip)`.
  Schema runtime esteso: `WeaponDef.meshScale/meshRotX/gripAttach/muzzleAttach`,
  `EnemyDef.attachPoints` (mappa completa) + `EnemyDef.weaponDisplay` (parse nel registry).
  `MeshRendererComponent.attachMesh/attachLocal`; il render disegna l'attach con
  `model * attachLocal`. Mesh armi nella MeshCache. Conquest (con respawn fedele) + sandbox.
- **AI cattura i command post — causa:** in Patrol l'AI faceva ping-pong spawn↔post senza
  sostare: attraversava l'area in ~5s ma la cattura ne chiede 8 e il progresso decade.
  Nuovo `AiComponent.patrolDwell/waitTimer`: sosta ai waypoint (12s in Conquest, >
  capture_time), anti-stuck sospeso durante la sosta, raggio d'arrivo 0.6. 0 = legacy.
- Nota dati: le armi senza mesh (es. DC-15A del clone) non mostrano nulla in mano —
  assegnare il mesh nel Weapon Editor.

## 2026-07-04 (7) — Suolo data-driven, spawn liberi, patrol→post, armi reali per l'AI
- **Nuovo `include/mini/game/MapQuery.hpp`** (header-only): `groundHeightAt` (top del
  collider calpestabile più alto, esclude muri con top > 1.6), `overlapsObstacle`,
  `findFreeSpot` (spinge una posizione fuori dagli ostacoli lungo una direzione).
- **Piedi sottoterra — causa:** il pavimento firebase ha top a y=+0.1 ma gli spawn assumevano
  suolo a 0 → tutte le unità affondavano di 0.1 (in conquest restavano compenetrate, la
  gravità non può risolvere una compenetrazione iniziale). Ora ConquestMode e SandboxMode
  spawnano a `groundHeightAt + AI_HALF_Y`; meshOffsetY = -AI_HALF_Y (relativo, non assoluto).
- **Alleati nel muro — causa:** le file generate cadevano esattamente sulle barricate a
  z=±13. Ora ogni posizione è de-collisa con `findFreeSpot` verso il campo.
- **Patrol → command post:** i patrol point non sono più ±1.5m attorno allo spawn: ogni unità
  riceve come meta un command post (round-robin, con dispersione attorno al post) → l'AI
  marcia sugli obiettivi, li cattura, e ingaggia via shared awareness. Fallback al vecchio
  pacing se la mappa non ha post.
- **AI usa l'arma assegnata:** `RespawnEntry.weaponId` (risolto in spawnUnit dal WeaponDef):
  cadenza reale (scalata da `AI_FIRE_RATE_SCALE=0.35`), **surriscaldamento** (heat/colpo,
  raffreddamento, penalità overheat — l'AI spara a raffiche e pausa come il giocatore),
  proiettile (velocità/danno/vita/colore) dall'arma. `AiComponent` esteso; il respawn
  conserva l'arma.
- Smoke test: partita completa avviata (6 nemici, 1 alleato, 3 post), nessun crash.
- **Deferito (Todo):** salto per l'AI, uso abilità, comportamento tattico per ruolo.

## 2026-07-04 (6) — Fix scala unità in partita + hit-test trasformato + alleati in sandbox
- **Clone gigante in partita — causa:** `ConquestMode::spawnUnit` non applicava mai
  `meshScale/meshRotX/Y` dell'EnemyDef (la sandbox sì). Ora la trasformazione arriva da
  ResolvedEnemyArchetype/allyDef → RespawnEntry → transform (solo per mesh custom: il cubo
  placeholder resta a scala 1).
- **Respawn fedele (bug latente):** `UnitTemplate` non copiava entityMesh, trasformazione e
  stats proiettile → le unità respawnate tornavano cubi con stats default. Copiati tutti i
  campi in spawnUnit e checkDeaths.
- **CombatSystem hit-test trasformato (bug strutturale):** il test zone faceva
  `entityPos + zone.offset` ignorando scala, yaw e meshOffsetY → hitbox sballate per modelli
  scalati e ~0.5 troppo alte per tutti; inoltre il broad-phase r=1.2 rigettava gli headshot
  (testa B1 a Δ1.31). Ora: offset*scala, rotazione yaw, +meshOffsetY, halfExtents*scala,
  broad r=2.5. Nota: la rotazione per-zona (eulerDeg) resta ignorata nel test (AABB).
- **Sandbox: manichini alleati** (3, vicino allo spawn T1, blu, dal registro allies) oltre ai
  5 nemici; `DummyInfo.team`; hitbox profile applicato anche ai manichini (headshot testabili).
  `footY` ora scalato nel meshOffsetY.
- Smoke test: entrambi i GLB caricati (42 + 11 primitive), 3 post, sandbox ok.

## 2026-07-04 (5) — Command post riusabili (ADR-009)
- **Schema:** `CommandPostDef` + `MapDef.commandPosts` (`command_posts` nel JSON mappa),
  parse nel registry.
- **Sistema riusabile `CommandPosts`** (`src/game/CommandPosts.cpp`, in CMake): cattura per
  presenza esclusiva nel raggio (XZ), decay se conteso/vuoto, visual palo+piastra colorati
  per proprietario (grigio/blu/rosso).
- **Conquista:** maggioranza dei post → drena 1 ticket avversario ogni 6s (vera conquista,
  non solo deathmatch). **Sandbox:** post catturabili senza conseguenze, per testarli.
- **Map Editor:** sezione "Command Post" (lista colorata per team, + Post / - Rimuovi),
  selezione → gizmo Sposta + pannello proprietà (nome, team iniziale, XYZ, raggio, tempo
  cattura, tutto a slider), salvataggio in `command_posts`.
- **firebase.json:** 3 post (Alpha sulla collina centrale, Bravo/Charlie sulle torri O/E).
- Smoke test runtime: registry "3 command post", `[CommandPosts] 3 post inizializzati`.

## 2026-07-04 (4) — IGameMode + factory (ADR-008)
- Estratta l'interfaccia `IGameMode` (applySettings/start/update, accessor, ticket,
  `hasVictoryCondition`) implementata da ConquestMode e SandboxMode; `MeshCache` centralizzato
  nell'header dell'interfaccia. Nuova `GameModeFactory.cpp` (`createGameMode("conquest"|
  "sandbox")`, fallback+log su id ignoto) aggiunta a CMake.
- Application: rimosse le lambda di dispatch `useSandbox`; ora detiene `unique_ptr<IGameMode>`
  e chiama solo l'interfaccia; win check via `hasVictoryCondition()`. Effetto: nuova modalità
  = classe + riga di factory (KnownIssues #8 chiuso).
- Smoke test runtime: `GFEngine --sandbox` avvia via factory, carica geometria firebase
  (22 box) e profilo hitbox B1 (2 zone autorate in editor) — pipeline ADR-006 verificata
  end-to-end.

## 2026-07-04 (3) — Camera Unreal-style + WeaponEditor attach point nel viewport
- **Navigazione viewport riprogettata (FreeCameraViewport):** RMB tenuto = mouselook +
  WASD/QE volo (Shift veloce; rotella regola la velocità di volo, mostrata nella barra);
  rotella da sola = dolly avanti/indietro; MMB drag = pan. Rimossi i controlli "Pan H/V".
  TAB capture resta come modalità alternativa.
- **Fix "movimento caotico":** il volo WASD si attivava appena la finestra era focused —
  anche digitando nei campi di testo. Ora è attivo solo durante la navigazione
  (RMB o TAB capture) e mai con `WantTextInput`. Il click di selezione è ignorato durante
  la navigazione.
- **WeaponEditor allineato agli altri moduli:** gli attach point ora appaiono nel viewport
  come marker (box + croce + etichetta, visibili attraverso il modello), selezionabili con
  click, spostabili col gizmo (world→model via inversa di rotX*scala, stessa convenzione di
  loadModel); pannello a sliderRow3; sync su selezione arma/trasformazione/aggiunta/rimozione;
  vista Proiettile nasconde marker+gizmo (i punti appartengono alla mesh arma).
- Hint TAB obsoleti rimossi dai moduli (la barra hint è ora nel viewport stesso).

## 2026-07-04 (2) — Editor professionalization batch (gizmo multi-mode + slider UI)
- **FreeCameraViewport gizmo a 3 modalità** (`GizmoMode::Translate/Rotate/Scale`):
  frecce (Sposta), anelli per asse proiettati in world space (Ruota, drag angolare attorno
  al centro con segno corretto rispetto alla camera), maniglie quadrate per asse + quadrato
  centrale per scala uniforme (Scala). Scorciatoie tastiera 1/2/3 con mouse sul viewport.
  API: `setGizmoMode/getGizmoMode`, `setGizmoRotAxes` (maschera anelli),
  `setGizmoCanRotateScale` (capability per target), `popGizmoRotDelta`, `popGizmoScaleDelta`.
  Hit-test frecce migliorato (distanza punto-segmento, non solo punta). Corretta l'inversione
  verticale del drag di traslazione (dot con y-schermo ora col segno giusto).
- **Wireframe hitbox rotation-aware**: `setHitboxes` applica `eulerDeg` (ordine Y*X*Z) ai
  corner — le zone ruotate si vedono ruotate.
- **`editor/include/util/UiWidgets.hpp` (nuovo, header-only)**: `sliderRow` (slider + campo
  numerico + etichetta), `sliderRow3` (X/Y/Z), `gizmoModeBar` ([Sposta][Ruota][Scala] con
  stato attivo evidenziato e modalità disabilitate per target che non le supportano).
- **MapEditor**: barra modalità in toolbar; Ruota (solo anello Y) → `box.ry`; Scala →
  `sx/sy/sz` con clamp; spawn point limitati a Sposta; pannello proprietà interamente a
  sliderRow (posizione/rotazione/dimensioni, con grid snap preservato).
- **HitboxEditor**: barra modalità sopra il viewport; Ruota → `eulerDeg` (3 anelli); Scala →
  `halfExtents` (delta full-size/2, clamp 0.01); proprietà zona a sliderRow3.
- **EntityEditor**: le zone hitbox ora sono renderizzate come **wireframe 3D** nel viewport
  (trasformate dalla character transform, colorate per moltiplicatore danno) oltre ai marker;
  Ruota/Scala via gizmo sulle zone (scala riportata in model space dividendo per la scala
  personaggio); attach point restano Sposta-only (capability gating); proprietà a sliderRow.
- Nessun cambio a runtime/engine: batch interamente editor-side. Build pulita.

## 2026-07-04 — Debt-reduction batch (post-Vision-update analysis)
- **ADR-006 Hitbox unification:** EntityEditor Hitbox tab now loads zones from the shared
  PROFILE (`data/hitboxes/<hitbox_profile|entity id>.json`, runtime schema
  `damage_multiplier`) and saves back to it (auto-creating the profile and writing
  `hitbox_profile` into the entity JSON). Inline `hitbox_zones` deprecated: legacy fallback on
  read, erased on save. B1 Battle Droid entity JSON migrated (inline zones removed).
  Effect: editor-authored hitboxes now reach the game; one store instead of two.
- **ADR-007 Registry-derived mode fallback:** `ConquestMode::buildEnemySpawnList` fallback
  `{"grunt","heavy","sniper"}` (dead ids) replaced with sorted registry enemy ids; empty
  registry → zero spawns + error log; caller clamps `nEnemies` to the list size.
- **EntityEditor gizmo correctness:** added `charTransform()/toWorld()/deltaToLocal()`;
  all 10 `setGizmoTarget` sites now pass world-space targets and `tick()` converts drag deltas
  back to model space (also keeps the gizmo anchored during drag and updates the weapon pose).
- **Repo hygiene:** `.gitignore` was corrupted (contained an old CMakeLists.txt dump);
  rewritten with real ignore patterns. Untracked from index: `build/` (1113 files),
  `imgui.ini`, `presets.cfg`. Staged, not yet committed.
- Build verified clean (GFEngine + GFEditor).

## 2026-07-03 — Session (editor UX + map/sandbox + data pipeline)
- **ProjectDocs bootstrapped** (this memory set). Effect: durable cross-session state.
- **Attach points visualised** as wireframe boxes + text labels in the viewport; "+ joint"
  now creates an attach point at the bone's real position; per-point "aggancia a un osso"
  dropdown. Effect: attach points are now spatially authorable.
- **3-axis translation gizmo + through-model selection** wired into MapEditor (boxes/spawns)
  and HitboxEditor (zones); editing overlays drawn depth-always. Effect: uniform 3D editing UX.
- **Weapon-in-hand pose** in EntityEditor (`weapon_display` in entity JSON); FreeCameraViewport
  gained a secondary "attachment model". Effect: editor-side posing; runtime consumption TODO.
- **Floating-model fix:** `MeshRendererComponent.meshOffsetY` now applied in `Application`
  render; ConquestMode/SandboxMode set it for GLB units. Effect: units stand on the ground.
- **Bigger firebase map** (~50x40) authored as `MapDef.geometry`; ConquestMode + SandboxMode
  read geometry + spawn points; procedural unit spread. Effect: map fully data-driven.
- **SandboxMode** added and wired (`--sandbox`) with respawning dummies on the firebase map.
- **GLB loader** rewritten: node-hierarchy baking (non-skinned) / identity (skinned),
  `Model::merged()` for multi-primitive models, byteStride-correct reads. Effect: models no
  longer corrupted; bones align.
- **RigReader** computes real joint world positions for skinned AND non-skinned rigs.
- **HitboxEditor** rebuilt with 3D viewport + bones + 3-column layout; profile `B1 Battle
  Droid` head zone bound to `head_0` bone.
- **Data:** enemy/ally/weapon mesh paths assigned; duplicate `clone_trooper.json` removed;
  `firebase.json` gains geometry + wider spawns.

_Note: pre-2026-07-03 history predates this changelog; reconstruct from git if needed._
