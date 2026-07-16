# 05 — Current State

## 2026-07-11 → 07-14 — Ottimizzazione + telemetria + navigazione (ADR-015/016/017)
Tre grandi sistemi aggiunti, tutti verificati contro il codice live, build zero-warning,
docs aggiornati per change (07_Changelog / 13_ADR / 08_KnownIssues):

- **Profiling + frame pacing (ADR-015, Fasi 1-2):** Tracy opt-in (`USE_TRACY_PROFILER`,
  solo GFEngine, no-op se OFF); main loop con dt a doppia precisione da
  `SDL_GetPerformanceCounter` + frame-cap di sicurezza quando la VSync è spenta.
- **Ottimizzazione AI (Fasi 3-4):** ricerca target in array SoA contigui; sensing pesante
  (target+LOS) scaglionata per entità (`AI_SENSE_INTERVAL=6`, bersaglio cachato in
  `AiComponent`); LOS limitata ai K bersagli più vicini (`AI_MAX_LOS_CHECKS=8`). Costo tick
  a 100 AI: ~203 → ~36 ms (Debug). Stress test headless: `--stress N` (cap
  `MAX_AI_PER_TEAM=50`/team), spawn a griglia in-mappa.
- **Telemetria LLM-observable (ADR-016):** sink JSONL `session_latest.jsonl` ACCANTO a
  `engine_run.log` (non lo sostituisce — ADR-013 resta); `telemetry::event(Level,system,msg,
  json)` + `flushEvents`. Hook: GameMode (mode created, ticket bleed), CommandPost (cattura),
  AI (cambio stato, stuck WARN con coordinate). Dump stato completo per-entità su
  F12/fine-partita/crash (estende `dumpGameState`).
- **Navigazione Recast/Detour (ADR-017, A+B+C):** navmesh single-tile da `MapDef.geometry`
  al load (`NavManager`, solo GFEngine); DetourCrowd muove le AI — traversata = pathfinding
  (`requestMoveTarget` sulla **destinazione reale**: `moveDX/DZ` è un versore, va riscalato per
  `moveDist` — vedi KI #33, dal 2026-07-15 la traversata ha davvero un path), combattimento =
  velocità tattica + avoidance (`requestMoveVelocity`, **non pianifica**), fallback `aiMove` se
  il navmesh manca. `CrowdSystem` (dopo AiSystem) fa registra/reap/tick/write-back; `World::nav`,
  `AiComponent::crowdAgentIdx`. Gli eventi `stuck` **non sono azzerati** (`--stress 10`: ~31):
  l'AI-stuck è mitigato, non "risolto alla radice" — la precedente formulazione era ottimistica.
  Aree semantiche DANGER/COVER dai metadata del MapDef con costi `dtQueryFilter` (danger
  aggirato). Il sensing ottimizzato resta ortogonale (decide CHI; il crowd decide COME muoversi).

- **Squadra e ordini — Phase A (ADR-020, doc 26, 2026-07-15):** `SquadComponent` +
  `SquadSystem`, registrato **fra Combat e Ai** (l'ordine dei sistemi è un vincolo: dopo Ai
  renderebbe gli alleati telecomandati). Ordine del tick: `Movement → Combat → Squad → Ai → Crowd`.
  Squadra alleata formata a runtime (leader = giocatore se entità valida di team 1, altrimenti la
  prima AI alleata; i respawn creano entità nuove → ri-arruolo ogni tick, via mailbox
  `World::playerEntity`). Ordine di default `Follow`; ciclo di vita con telemetria
  (`order issued/completed/failed`); gli ordini dichiarati ma non eseguiti falliscono **con causa
  esplicita**. **Modello a guinzaglio:** l'ordine ha precedenza solo fuori dal raggio di
  soddisfazione (Follow 8 m, HoldPosition 2 m, MoveTo 1.5 m); dentro, l'AI è autonoma. Vincola il
  **movimento**, mai mira/fuoco → il comando funziona *durante* il firefight.
- **Comando contestuale — Phase B (2026-07-15):** tasto **G** (`Action::SquadOrder`, rimappabile);
  il contesto lo decide il mirino — nemico → `FocusFire`, cover point reale del MapDef entro 4 m →
  `TakeCover`, altrimenti → `MoveTo`. Riusa l'entità già risolta dal loop del mirino (ciò che il
  mirino segna è ciò che la squadra riceve). Intenzione via mailbox `World::squadOrder`;
  **raggiungibilità verificata prima di impartire** con `findPath` (che su meta irraggiungibile
  ritorna un path *parziale*, non un errore → si confronta l'arrivo col punto chiesto).
  `FocusFire` vincola la **scelta del bersaglio** dentro il ramo di sensing (rispetta il
  time-slicing) e **non** il movimento; se l'AI non vede il designato resta autonoma.
  Mailbox `World::killedThisTick` (CombatSystem → Squad): senza, un bersaglio ucciso è
  indistinguibile da uno sparito e il successo verrebbe riportato come fallimento.
  HUD: pannello SQUADRA (membri/ordine/distanza) dallo stato reale dei membri; esiti e cause nel
  feed via `pushEvent`, una volta per ordine. **Non esistono ancora**: ruota di comando (livello 2)
  e Phase C ("a terra" + rianimazione).

- **Class System — Phase A (doc 14, 2026-07-15):** `ClassDef` (`data/classes/<id>.json`:
  primary/secondary weapon, abilities[], role) nel registry, id = filename stem. Consumo reale:
  **`MatchSettings.classId`** → risolto in `startGame()` dove l'arma viene già scelta; la classe
  riempie primaria/secondaria/abilità. Vuota = loadout manuale, **comportamento identico a prima**
  (additivo). Persistito nei preset; flag `--class <id>` (il PreMatch **non ha ancora un
  selettore**). `role` è solo un tag: nessun sistema AI lo consuma. Esempi: `trooper`, `marksman`.
  **Attenzione**: `classId` NON sta su `PlayerDef` (il doc 14 lo prescriveva): la classe e' il
  LOADOUT, il personaggio sono le STAT — due concetti distinti. `abilityIds` e' trasportato ma
  senza effetto (KI #32).

- **Personaggio del giocatore (KI #35, risolto 2026-07-15):** `PlayerDef`
  (`data/characters/<id>.json`: hp, move_speed, jump_height, sprint_mult, armor_rating) era
  autorato dal BalanceEditor e **letto da nessuno** — le stat non avevano effetto. Ora
  `MatchSettings.characterId` si risolve in **`initWorld`** (quindi vale per partita E sandbox) e
  atterra su `PlayerController` (moveSpeed/jumpMult/sprintMult/armorRating) e su
  `HealthComponent.armor` (generico; 1 = nessuna riduzione, applicato in CombatSystem dopo lo
  scudo). I default in codice sono **identici alle costanti storiche** → senza personaggio il
  comportamento e' invariato. Con **un solo** personaggio autorato viene scelto da solo; con piu'
  d'uno la selezione e' ambigua e viene richiesta esplicitamente (serve il selettore PreMatch).
  La costante hardcoded `SPRINT_MULT` e' stata rimossa: il valore vive nei dati (1.65).

- **Gate di validazione contenuti (ADR-018, doc 24, 2026-07-15):** `core/Result.hpp`
  (`Diagnostic` = severity + category + file + message + **suggestion** azionabile) e
  `game/data/ContentValidation` (`validateContent`, `validateMission`, `reportDiagnostics`).
  **Un solo posto per le regole, tre consumatori**: runtime (dopo `loadAll()`; un Error
  **blocca l'avvio** — niente fallback silenzioso), editor (*Moduli → Validazione contenuti*),
  headless (`GFEngine.exe --validate` → stampa + JSONL + **exit code ≠ 0**, senza finestra).
  `ContentValidation.cpp` è nella source list di **entrambi** i target: una copia più debole
  nell'editor è impossibile per costruzione. Gate: riferimenti incrociati, asset su disco,
  sanità armi/unità/mappe, **near-duplicate sui nomi visualizzati** (KI #7), missioni/obiettivi,
  orfani, **campi fantasma** (chiavi che nessun loader legge → refusi come `"fire_rat"`, che non
  falliscono ma fanno usare il default; via `DefinitionRegistry::unknownKeys()`, popolata dai
  loader mentre il JSON è ancora in mano → nessun re-parse). Legge solo il registry; i gate asset
  guardano il disco. **Limite noto**: i campi *letti ma non consumati* da nessun sistema
  (`min_range`, `fov_deg` — KI #25) non sono rilevabili da un gate sui dati: è un fatto sul
  codice. Restano annotati a mano negli editor ("(non attivo)", A9).

- **Selezione missione e classe nel PreMatch (2026-07-16):** righe **Missione** e **Classe** nel
  menu pre-partita (compaiono solo se esistono definizioni autorate). Indice 0 = "(nessuna)" →
  partita libera / loadout manuale = comportamento storico. **La missione impone mappa e modalità
  e il menu le aggiorna a vista** (`syncRowsToMission`): il giocatore non deve mai leggere una
  mappa e giocarne un'altra. Stesso pattern delle mappe (riga enum + lista dal registry); gli
  indici sono stato di UI, si persiste l'**ID** (KI #20). `missionId` nei preset;
  `applyPreset`/`setSettings` ricostruiscono gli indici dagli id, e un id non più risolvibile
  torna a "(nessuna)". `--mission`/`--class` **seminano** solo la scelta iniziale: la missione si
  risolve in `initWorld` da `currentSettings.missionId`.
  **Nota**: `characterId` NON ha ancora un selettore (con un solo personaggio autorato è
  automatico, KI #35).

- **Conseguenze degli obiettivi (doc 25, 2026-07-16):** `ObjectiveDef.on_success[]`/`on_failure[]`
  = liste di `{type, value, target}`. `ObjectiveSystem` le applica scrivendo **solo** su
  **`World::battleState`**; ogni sistema competente legge cio' che lo riguarda → nessun
  `if (objectiveId == ...)`, e aggiungere un tipo = enum + case + un lettore.
  Tipi: `block_enemy_reinforcements` (→ ConquestMode::checkDeaths: il nemico non rimpiazza piu'),
  `enemy_accuracy` (→ AiSystem, solo team 2; moltiplicativo), `ally_reinforcements` (→ delta
  consumato da ConquestMode, che possiede i ticket), `unlock_spawn`.
  **I valori nei dati sono segnaposto da bilanciare provando** (direttiva utente 07-16).
  Gate ADR-018: type sconosciuto = Error (resterebbe None → obiettivo che sembra avere un effetto
  e non ce l'ha). Verificato con effetto reale: 2 nemici uccisi col blocco attivo → 0 rimpiazzi.
  **Manca l'authoring nell'editor**: si scrivono a mano nei JSON.

- **Statistiche di missione + debrief (doc 25 / GDD 9.6, 2026-07-16):** `World::missionStats`
  (mailbox): playerKills, teamKills, alliesLost, playerDeaths, missionTime, objectivesDone/Failed.
  **Accumulate mentre i fatti accadono** (una kill esiste solo nell'istante in cui avviene e
  l'entita' viene distrutta subito dopo): CombatSystem le kill/perdite (`bullet.fromPlayer`
  attribuisce quelle del giocatore; il giocatore e' ESCLUSO da `alliesLost` perche' e' team 1),
  ObjectiveSystem tempo ed esiti, Application le morti del giocatore. Azzerate da
  `World::initialize()`: **per-missione, non per-sessione**.
  **Debrief** sulle schermate Win/Lose (che prima avevano testo cablato e ormai falso) + evento
  JSONL `match end` con tutti i valori. **Nessun punteggio calcolato**: il giudizio non e' un voto
  ma l'insieme dei fattori (GDD 9.6) — i pesi sono progressione (doc 27) e vanno decisi dal design.

- **Ticket = rinforzi, non vite del giocatore (KI #39, 2026-07-16):** i ticket sono la **riserva
  di rinforzi della squadra** (il campo ha un cap di AI; il resto entra man mano che le unita'
  cadono). Il meccanismo vive in `ConquestMode::checkDeaths` (unita' cade → ticket → rimpiazzo;
  a 0 → morte permanente) ed e' intatto. **Il giocatore NON consuma rinforzi morendo**: si perde
  solo cadendo quando non resta ne' un alleato vivo ne' un rinforzo in arrivo. Regola in un solo
  posto (`onPlayerDeath` in Application), valida anche per il respawn volontario (K).
  `IGameMode::consumeTeam1Ticket()` **rimosso**: rendeva possibile l'errore opposto.

- **Command post come obiettivi (2026-07-16):** `CaptureZone` (il post e' di actor_team) e
  `DefendZone` (tenerlo per hold_seconds; perderlo = fallimento immediato) **avvolgono ADR-009**
  senza duplicarne la logica: la cattura resta in `CommandPosts`, gli obiettivi ne leggono solo
  l'esito via mailbox **`World::commandPostStates`**, pubblicata da Application **fra
  `mode->update()` e `world.tick()`** (stesso tick, non quello prima). Riferimento per **label**
  (i post non hanno id): il gate ADR-018 la risolve nella **mappa della missione** e segnala le
  label duplicate. Esempio: missione `firebase_alpha` (cattura Alpha → tienilo 20s).

- **Obiettivi — Phase B: il collegamento (2026-07-16):** l'esito della missione **chiude la
  partita** (`ObjectiveSystem::outcome()` era codice morto: completare una missione non faceva
  nulla — KI #37). Precedenza al mode: se le regole della modalità hanno già deciso (ticket), la
  missione non ribalta; altrimenti l'esito della missione È l'esito della partita — coerente con
  la divisione di doc 25 (il mode decide le regole, gli obiettivi cosa fare). Application tiene un
  puntatore **non-proprietario** al sistema (i sistemi sopravvivono a `World::initialize()`).
  **HUD OBIETTIVI** (colonna sinistra): letto dallo stato reale del sistema, primari in evidenza,
  colore = stato, progresso solo dove esiste un conteggio vero (`EliminateTarget` N/M,
  `HoldAreaForDuration` s/s), `Inactive` nascosti. **La missione impone la sua mappa**
  (`MissionDef.mapId`; un `--map` contraddittorio viene segnalato). Rebind al riavvio del mondo
  via tick azzerato (KI #38). Verificato in partita vera: missione fallita → SCONFITTA, primario
  completato → VITTORIA.

- **Obiettivi e missioni — Phase A (ADR-019, doc 25, 2026-07-15):** `ObjectiveDef`
  (`data/objectives/<id>.json`) e `MissionDef` (`data/missions/<id>.json`) sono definizioni nel
  registry (id = filename stem). `ObjectiveSystem` gira **dopo Ai/Crowd** (valuta quando le unità
  si sono mosse) ed è **inerte senza missione** → i mode ADR-008/009/014 sono intatti: il
  framework si affianca, non li riscrive. Tipi implementati: `ReachArea`, `EliminateTarget`,
  `HoldAreaForDuration` (presenza continuativa: uscire azzera). Gli altri 6 del doc 25 falliscono
  con causa esplicita. Attivazione dichiarativa (`immediate`/`after_objective`/`after_time`) →
  dipendenze fra obiettivi **senza scripting**; `tier` è un campo, non tre sistemi. Regole di
  successo/fallimento dichiarate nel MissionDef — **nessun `if (missionId == ...)`**. **Gate**:
  regole mancanti/invalide, id inesistenti, tier incoerenti o zero obiettivi → missione
  **rifiutata con causa**, non avviata a metà. Mailbox `World::activeMission`/`objectiveDefs`;
  flag `--mission <id>` (unica selezione esistente: nessuna UI, come da scope doc 25).
  Esempio in repo: `firebase_ridge`. **Non esistono ancora**: CaptureZone/DefendZone
  (avvolgimento ADR-009), HUD obiettivi, Punti Comando.

**Risultato misurato:** ~40 AI in simulazione ora fluidi (prima il limite di fluidità era
~30-32); AI con pathfinding reale + crowd-avoidance + evita le danger zone. Squadra sotto ordine
`MoveTo` verificata convergente in `--sim` (8.0 → 1.3 m; ordini completati 0 → 1915). Missione
`firebase_ridge` verificata headless: dipendenze rispettate, `mission success` end-to-end, gate
che rifiuta le missioni invalide.
**Smoke manuali ancora dovuti** (headless non copre): restart sandbox dopo sim, glitch mouse
primo-frame, partita REALE (feel movimento/combattimento AI via crowd), navmesh/aree su outpost.

## 2026-07-10 (24) — Robustezza tranche 2: arma attiva unica + A7/A9 chiusi
- `PlayerController` senza più copia dell'arma attiva (accessor su weapons[active]) —
  KI #22 chiuso strutturalmente; resolve unità unico nemici/alleati in ConquestMode
  (alleati con stats proiettile dall'arma vera); campi non consumati marcati
  "(non attivo)" negli editor (KI #25 mitigato). Build + smoke --sim ok.

## 2026-07-10 (23) — Rifinitura robustezza completata (Todo A2-A8)
- id definizioni = SOLO filename stem in tutti i loader (KI #21 chiuso); heat persistente
  allo switch arma (KI #22); collisione/LOS esatte sui collider ruotati, coerenti coi
  proiettili (KI #23); spawn spec ConquestMode unificato (via UnitTemplate); loader
  nemici/alleati deduplicati (`parseUnitDef`); zero id hardcoded nei game mode (KI #24);
  dipendenze CMake pinnate (KI #27); dati morti eliminati (KI #26 in parte).
  Smoke `--sim` passato; restano gli smoke manuali elencati nel Changelog (27).

## 2026-07-10 (22) — Audit qualità + preset a prova di build (KI #19/#20)
- Audit completo del progetto: nuovi KI #19-#27 e roadmap robustezza in 06_Todo (A1-A10).
- Preset partita: ora in `<exe>/user_presets/` (fuori dalla data/ azzerata dalle build),
  formato nlohmann con `map_id` per id + loadout completo persistito; migrazione legacy
  automatica. Smoke manuale pendente (salva → rebuild → ricarica).

## 2026-07-10 (21) — Seconda mappa "Outpost" + selettore mappa (R3)
- Due mappe giocabili (firebase, outpost), selezionabili nel PreMatch (riga "Mappa");
  la mappa attiva viaggia in MatchSettings.mapId — zero id hardcoded nei mode.
  Flag `--map <id>` per test. Outpost verificata in sim SENZA codice dedicato:
  la promessa "nuova mappa = solo dati" è dimostrata.

## 2026-07-10 (16) — Integrità combat: muri solidi per i proiettili, veicoli solidi
- I proiettili muoiono sui collider (prima ATTRAVERSAVANO i muri — bug storico mai
  notato perché le AI sparano solo con LOS). I veicoli sono ostacoli reali (collider
  + excludeId per il movimento proprio) e bloccano LOS/colpi. Test OBB condiviso
  mirino/proiettili (KI #13). Viewmodel arma in prima persona.

## 2026-07-10 (10) — Veicoli rifiniti + roster mappa + Vehicle Editor
- TPS anche alla guida (V). Roster mappa: enemy_types E ally_types editabili con slot/
  pattern/auto (vuoto = tutte le definizioni registrate — le nuove entità entrano in
  partita da sole). Nuovo modulo Vehicle Editor (lista, stats, mesh con anteprima 3D e
  box collisione wireframe, rinomina con sweep). Diagnostica guida in telemetria
  (`drive:`, tentativi E falliti con distanza).

## 2026-07-10 (8) — Veicoli Fase A (19_Vehicles)
- VehicleDef data-driven + spawn da MapDef.vehicleSpawns + guida player (E sali/scendi,
  W/S/A/D, fisica slide/step-up condivisa, camera di guida). Niente armi di bordo/AI
  alla guida (Fase B). BARC Speeder su firebase (2 spawn). EntityEditor: creazione
  nuove entità dalla lista. Con questo la Fase 1 della Vision ha tutti i sistemi:
  resta l'iterazione "is it fun" e i debiti minori.

## 2026-07-10 (7) — L'AI consuma i Map Metadata (18_AiMapConsumption)
- `World::activeMap` (mailbox opaca) + AiSystem: hide → cover point orientati verso il
  nemico (fallback strafe), repulsione danger zone fuori ingaggio, pattuglie dai
  segmenti delle patrol route (ConquestMode). firebase ha un set minimo di metadata di
  prova. Il level design tattico ora ha effetto osservabile in `--sim`.

## 2026-07-10 (6) — Map Metadata (15_MapMetadata: Implementato, dati+authoring)
- `MapDef.coverPoints/patrolRoutes/dangerZones` + parse registry + sezione "Metadata AI"
  nel MapEditor (marker dedicati, gizmo, slider, RMW). Nessun consumer runtime ancora
  (scelta di scope: l'AI tattica fase 2 andrà documentata a parte). KI #11 chiuso lato
  dati; Todo #15 done.

## 2026-07-10 (4) — Battaglia AI viva + Sandbox Tools rifiniti
- Fix AiSystem (search hardcoded pre-firebase, Search senza uscita, primo colpo
  soppresso): la battaglia AI-vs-AI ora produce ingaggi/kill continui (verificato con
  `--sim`: flag CLI che avvia direttamente la simulazione osservatore). Heartbeat
  `ai:` e riepilogo spawn `[Conquest]` in telemetria. Sandbox: un manichino per ogni
  definizione; menu con slot arma primaria/secondaria, ticket/respawn, scelta modalità
  della simulazione; log chat scorrevole (PAGSU/PAGGIU, storico 200).

## 2026-07-10 (3) — Sandbox Tools (17_SandboxTools)
- TAB in sandbox apre il menu prova: tutte le armi (scroll+INVIO), parametri partita
  (manichini per team, HP, riavvio), simulazione AI-vs-AI con osservatore neutrale in
  volo libero (WASD+SPAZIO/CTRL). L = log chat eventi in-game (hit/scudi/kill), attiva
  in ogni modalità. `SandboxMode` ora legge i conteggi da MatchSettings.

## 2026-07-10 — Profilo tattico AI completo + ability shield (16_AiBehavior, Todo #3)
- Tutti i campi tattici di `AiProfileDef` ora hanno effetto: aggression (distanza
  d'ingaggio), retreat_hp_threshold (disimpegno con fuoco di copertura), peek/hide da
  cover_preference (in "hide" non spara), flank_chance (approccio laterale in Hunt).
- Ability runtime tipo "shield": `ShieldComponent` assorbe il danno prima degli HP e si
  rigenera (param1/2/3 dell'AbilityDef); assegnata allo spawn da `abilities[]` dell'unità.
  `B1 Heavy Droid` la referenzia; assegnabile dall'EntityEditor (sezione Abilita') e
  bilanciabile dalla tab Abilita' del BalanceEditor. Vale anche sui manichini sandbox.

## 2026-07-09 (12) — Spike split-screen (ADR-011): esito (a)
- Due viewport + seconda Camera sulla stessa scena live: funziona con sole aggiunte minori
  al Renderer (`drawMeshFrom(const Camera&)`, `setViewportRect`). Toggle debug F9 in
  partita. Il soft-gate ADR-011 decade; il lavoro futuro (input/HUD del secondo giocatore)
  è additivo. Fix minore: nel PreMatch la riga "Modalità" non disegna più la barra
  (il nome, es. "Conquista", finiva sotto la barra).

## 2026-07-09 (3) — Hitbox solo in Entity Editor (ADR-012)
- HitboxEditor rimosso; EntityEditor copre tutto (incl. debug_visible, gap colmato).
  BalanceEditor ripulito (via tab Nemici/Alleati vestigiali). Ultimo id hardcoded
  ("grunt" in spawnUnit) rimosso. Profili orfani eliminati. Tab Balance: Armi/AI/Mappe/
  Personaggio.

## 2026-07-09 (2) — AI dal profilo
- L'AI ora usa dal `AiProfileDef`: `jump_enabled` (salto anti-ostacolo quando bloccata a
  terra), `accuracy` (dispersione colpi), `reaction_time` (ritardo primo colpo),
  `seek_speed`. Profilo `grunt` creato (Todo #7). Abilità runtime e ruoli tattici deferiti
  a un documento Planned Feature (Todo #3).

## 2026-07-09 — Messa in regola (ADR-010 Accepted)
- `saveJsonRMW` centralizzato + `.bak`: unico canale di scrittura JSON editor (tutti i
  moduli migrati). Comando **Rinomina** con sweep cross-ref in Weapon/Entity/Hitbox/Map
  editor. Audit dropdown passato. `id`/`profile_id` deprecati in rimozione progressiva.
- Pendente: smoke GUI del rename (KnownIssues #7); poi chiudere #7.

_Last verified: 2026-07-04 (against live code)._

## 2026-07-09 (11) — Tre modalità reali (ADR-014)
- Conquista/Assalto/Difesa selezionabili nel PreMatch (pagina Regole); esito partita
  deciso dal mode via `outcome()`; HUD mostra proprietario+cattura dei command post.
  La promessa Fase 1 "modalità come configurazioni" è implementata.

## Position vs Vision roadmap (00_Vision)
**Fase 1 ("core playable") essenzialmente completa.** Presente: 2 mappe data-driven
(firebase, outpost), fanteria entrambe le fazioni, armi funzionanti, spawn, **IGameMode +
factory (ADR-008)**, **Conquista/Assalto/Difesa (ADR-014)**, **command post con ticket bleed
(ADR-009)**, **veicoli Fase A (19_Vehicles)**, **weapon-in-hand runtime + viewmodel**,
**HUD stato post**, split-screen feasibility verificata (ADR-011, esito (a)), Sandbox + tools.
Editor suite pro (gizmo 3 modalità, slider, camera Unreal-style, rename tooling ADR-010).
Sopra la Fase 1 sono stati aggiunti sistemi di respiro Fase 2/3: **telemetria LLM-observable
(ADR-016)**, **navigazione Recast/Detour con crowd (ADR-017)**, **ottimizzazione AI/loop
(ADR-015)**. Resta l'iterazione "is it fun" e i debiti tracciati in 06_Todo. Fasi 3-5 non
ancora avviate (by design), salvo la preparazione navigazione/AI appena fatta.

## Working
- DefinitionRegistry loads weapons/enemies/allies/ai/hitboxes/maps/abilities/characters
  from `data/`, id = filename stem.
- Two binaries build clean (GFEngine + GFEditor), Debug preset `windows-debug`.
- **Data-driven map:** `MapDef.geometry` authored in MapEditor, read by ConquestMode and
  SandboxMode. `firebase.json` now holds a ~50x40 arena + spawn points.
- **ConquestMode** reads `MapDef.spawnTeam1/2` (player + procedural unit spread) and
  `enemyTypes`/`allyTypes`. Units spawn at ground level.
- **SandboxMode** (`--sandbox`): firebase geometry, player at team1 spawn, respawning
  dummies at team2 spawn (stationary, damageable).
- **GLB pipeline:** node-hierarchy baking (non-skinned) / identity (skinned), multi-primitive
  merge, byteStride-correct accessor reads. `meshOffsetY` applied in render (no floating models).
- **EntityEditor:** mesh browse (+ saved), transform, rig bones visible/clickable, attach
  points (bone-bindable, rendered as boxes + text labels), inline hitbox zones (bone-bindable),
  weapon-in-hand pose persisted as `weapon_display`.
- **(HitboxEditor RIMOSSO — ADR-012):** l'authoring hitbox è nell'EntityEditor (tab Hitbox);
  il formato profilo runtime `data/hitboxes/*.json` resta invariato (ADR-006).
- **MapEditor & EntityEditor & VehicleEditor:** gizmo a 3 modalità (Sposta/Ruota/Scala,
  scorciatoie 1/2/3, barra [Sposta][Ruota][Scala] per modulo) + selezione visibile attraverso
  i modelli; pannelli proprietà a slider+campo numerico (`UiWidgets::sliderRow`); wireframe
  hitbox rotation-aware nell'EntityEditor.
- Weapon GLBs assigned: E5/E-5C -> e-5_blaster_rifle.glb, DC-17 -> dc-17.glb.
- Enemy/ally meshes assigned (B1 droids, Clone Trooper).

## Resolved 2026-07-04
- Hitbox authoring unified on the PROFILE (ADR-006); EntityEditor + HitboxEditor edit the same
  store the runtime reads. B1 inline zones migrated out.
- ConquestMode fallback ids now registry-derived (ADR-007).
- EntityEditor gizmo correct under scale/rotation (toWorld/deltaToLocal).
- Repo hygiene: .gitignore rewritten, build/+imgui.ini+presets.cfg untracked (to commit).

## Resolved 2026-07-04 (later batches)
- GameMode abstraction (ADR-008): `IGameMode` + factory; Application interface-only.
- Editor pro: gizmo 3 modalità, slider ovunque, camera Unreal-style (RMB look/fly, wheel,
  MMB pan, niente volo mentre si digita), WeaponEditor attach point nel viewport.
- Clone Trooper scale: risolto dall'utente via editor (nuovo GLB + mesh_scale 0.011).

## Partial / fragile
- **AI ignorano i veicoli** (KI #31, regressione nav Phase B): il crowd non conosce i veicoli.
- **Nessun sistema abilità/gadget lato giocatore** (KI #32): loadout non cablato al player.
- **Timestep misto:** world a fixedDt, player/sparo a dt variabile (A10 — rilevante per replay).
- **Application.cpp ~1250 righe:** candidato refactor R2 (estrarre VehicleDriver/SandboxSession).
- **Mode id dal flag CLI:** la scelta modalità dovrebbe venire da MapDef/PreMatch (nota ADR-008).

## Not implemented
- AI Editor, Asset Manager, UI/Interface Editor modules (moduli editor futuri).
- Sistema Classi giocatore (14_ClassSystem — schema definito, zero codice).
- Sistema abilità/gadget lato GIOCATORE (le abilità esistono solo per le AI — KI #32).
- Veicoli Fase B (armi di bordo, multi-posto, AI alla guida — 19_Vehicles).
- Split-screen vero (input/HUD 2° giocatore; feasibility verificata, ADR-011).
- Rename tooling: FATTO (ADR-010). Weapon-in-hand runtime + viewmodel: FATTO. (voci storiche
  qui sotto aggiornate — vedi sezione di stato 07-11→07-14 in cima al documento.)
