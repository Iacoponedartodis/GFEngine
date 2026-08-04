# 05 — Current State

## 2026-08-04 — Fotografia dopo il blocco AI + osservabilità (changelog 100-131)
Verificata contro il codice live durante l'audit di documentazione del 2026-08-04. Il documento era
fermo al 2026-07-22 e non conosceva **nulla** di quanto segue: è la deriva più grande trovata
nell'audit, ed è il motivo per cui questa sezione esiste.

**AI — `src/ecs/systems/` è ora tre file, non uno.** `AiSystem.cpp` (come la singola unità esegue),
`AiCommandLayer.cpp` (cosa decide il livello di comando: settori, torre, direttive, posizioni per
gli ordini) e `AiTrace.cpp` (osservazione per-agente). Seam privato in `AiInternal.hpp`.
- **Percezione completa** (A1-A2): cono visivo + fascia periferica + rivelazione dal lampo; udito
  event-driven con sensibilità *relativa*; confidenza sui contatti con decadimento esponenziale
  (sotto soglia il contatto diventa meta di perlustrazione, non bersaglio).
- **Soppressione** (A3) e **ruoli di combattimento** (A4: sopprime / aggira / avanza, per saturazione
  e affinità di profilo).
- **Utility formalizzata** (A5): gli 8 bilanci di pesi vivono in `include/mini/game/ai/AiUtility.hpp`.
  I valori sono ancora quelli storici — la *taratura* è lavoro aperto.
- **Ingaggio** (KI #86, quattro cause corrette): punto di mira allineato fra acquisizione, tiro e
  FocusFire (`+AI_HALF_Y*0.7`); la fase di hide non si congela più; la ricerca chiede al mondo una
  posizione da cui la zona si vede; la manovra prosegue senza bersaglio.

**Mondo tattico**: prefab (ADR-048) autorati per asset ed espansi al load; salute tattica nell'editor
e nel gate `--validate`, incluso `UnmarkedCover`.

**Editor**: scheletro comune `ModuleShell` + `AssetBrowser` (ADR-049); anteprima **arma in mano** nel
Weapon Editor con la stessa formula del runtime (`WeaponHandPose.hpp`, una sola implementazione per
tre consumatori); il roster di mappa accetta le **classi** oltre alle entità (ADR-023, KI #88).

**Osservabilità (ADR-050, doc 42) — copertura completa su tutti i sistemi vivi.** Profiler sempre
attivo con zone annidate; telemetria divisa per dominio (`perf`/`ai`/`combat`/`world`/`content`) con
archivio `storico/`; verbosità a runtime (`--telemetry-verbose`); funnel di navigazione, di fuoco, di
missione, di rendering; scatola nera per-agente (`--trace-ai <id>`); inventari di avvio e asset.
Costo misurato: **0,01% del frame**.

**Performance (KI #87, risolto come diagnosi)**: il collo di bottiglia è il **rendering**, non l'AI —
scena 3D 95% del frame, simulazione 2,9%. Causa: 1,45 M vertici/frame, di cui due terzi dal mesh del
B1 (161k vertici). Nessuna ottimizzazione ancora intrapresa: vedi doc 43.

## 2026-07-22 — Metadata↔AI: integrazione completata (audit doc 38 chiuso, ADR-045/046)
Fotografia del **seam `mini::worldintel`** e di come l'AI (`AiSystem`) consuma i metadata di `MapDef`,
dopo la chiusura dell'audit doc 38. Verificato contro il codice live; build 0/0, `--validate` 0/0.
- **Route** (ADR-045): `advancePatrol` bidirezionale (`patrolReverse`, `patrolSeg` = indice del punto);
  `joinNearestRoute` aggancia la route più vicina dal vertice più vicino; le route obbediscono al
  comando (Advance/Retreat sovrascrivono la pattuglia per **tutti**, non solo per chi è senza route).
  Telemetria `su_route`.
- **Ruoli tattici, tutti e 5 consumati** (ADR-046): `cover`→copertura difensiva (`bestCoverToward`);
  `vantage`→posizione di tiro (`bestFiringPosition`); `observation`→vista estesa locale (`aggroRange
  ×1.5` entro 10 m); `defensive`/`chokepoint`→posizioni da tenere sotto `Hold` (`bestHoldPosition`).
  Telemetria `obs_vista_estesa`, `hold_su_posizione`.
- **Danger zone consumate** (ADR-046): `bestCoverToward`/`bestFiringPosition` sottraggono `dangerAt`
  dal punteggio → a parità di protezione l'AI sceglie la copertura fuori dalla zona pericolosa.
  `dangerAt` non è più una query morta.
- **Grafo tattico**: `buildTacticalLinks` (al load) + `bestOverwatchForPosition` (overwatch verso un
  punto che un compagno sta occupando). `bestFlankingPosition` per l'aggiramento.
- **Codice morto rimosso**: `bestOverwatchFor` (variante vecchia non-Position) e `pickObjectiveSector`
  (residuo comando v1). Restano solo note documentali su `height` (solo visivo) e filtri navmesh
  per-ruolo (marcati ma non cablati).
- **Harness**: `--map <id>` ora vale anche in `--sim`/sandbox (`SandboxMenu::selectMapById`, KI #77).

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
  json)` + `flushEvents`. Hook: GameMode (mode created, respawn-slow/rinforzi), CommandPost (cattura),
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
  `World::playerEntity`). **Nessun ordine di default** (ADR-037, 2026-07-20): senza ordine il membro
  resta `OrderType::None` e si muove come **truppa indipendente** (Patrol/Alert/Hunt normali);
  `Follow` è un ordine impartito dalla ruota di comando, revocabile dallo stesso settore. Ciclo di
  vita con telemetria (`order issued/cleared/completed/failed`); gli ordini dichiarati ma non
  eseguiti falliscono **con causa esplicita**. **Modello a guinzaglio:** l'ordine ha precedenza solo
  fuori dal raggio di soddisfazione (Follow 8 m — **15 m in Alert**, HoldPosition 2 m, MoveTo 1.5 m;
  sospeso durante una manovra attiva, KI #64); dentro, l'AI è autonoma. Vincola il
  **movimento**, mai mira/fuoco → il comando funziona *durante* il firefight.
- **Rete di comunicazione (2026-07-20, ADR-038 / doc 34):** una struttura strategica con
  `role: "comms"` è la **torre di comunicazione** della sua fazione. Finché è viva quella fazione
  comunica normalmente; quando cade, `World::comms[team]` degrada e **rallenta** — raggio di
  condivisione dei contatti ×0.5, avviso con **2.5 s di ritardo** (si accorre dove il nemico *era*),
  direttiva del comando rivalutata 2.5× più di rado, rimpiazzi ×1.6. **Nulla viene mai bloccato.**
  Si degrada solo chi la torre l'aveva (`hadTower`) → mappe senza torri invariate. I contatti
  condivisi sono ora **persistenti e datati** (finestra `[ritardo, ritardo+1 s]`, deduplicati per
  area+recenza). Cadenza di decisione del comando: `COMMAND_DECISION_PERIOD` 3 s (prima: ogni tick).
- **Ciclo di stato AI chiuso (2026-07-21, KI #68):** Alert → Hunt → Search → Patrol **termina
  sempre**. `Hunt` scade dopo `AI_HUNT_TIMEOUT` (20 s) degradando a Search (che a sua volta scade a
  15 s verso Patrol). Prima Hunt non aveva timeout e un'unità inseguiva un contatto inesistente per
  centinaia di secondi — ben visibile nella simulazione sandbox, che **di proposito non si ferma**
  (serve a osservare le AI; non è un difetto del game mode, la partita vera finisce).
- **Torre di controllo dei cloni (2026-07-21, ADR-040 / doc 36):** `role: "control"` → `World::allyIntel`
  pubblica una **lista** di segnali (settori non saldamente tenuti + strutture nemiche vive).
  Un clone **senza ordini e senza route** ne sceglie uno **decorrelato dal proprio `bias`** — non il
  migliore — e decide da sé il punto dentro l'area. **Nessun ordine, nessuna destinazione imposta**:
  è il canale opposto a `enemyCommand` (un intento unico su cui tutti i droidi convergono), e i due
  non vanno mai fusi. Senza torre viva il canale è spento e i cloni restano puramente autonomi.
  **Saturazione (KI #73)**: ogni segnale conta le presenze (`Signal.crowd`); oltre
  `ALLY_SIGNAL_CAPACITY` non ne attira altri e i cloni in più pattugliano — così non si ammassano
  tutti sui pochi segnali di fine partita. Chi è già in un segnale ci resta (niente oscillazione).
- **Strutture: solide in DUE sensi (2026-07-21, KI #72):** una struttura ha un `ColliderComponent`
  (giocatore, proiettili, LOS) **e** entra nell'input di `NavManager::build` come ostacolo del
  navmesh (AI, che si muovono via DetourCrowd). I due volumi derivano dalla **stessa** funzione
  (`StrategicTargetDef::solidHalfExtents`) e non possono divergere. Il solo collider **non** basta a
  fermare le AI. Spawn condiviso fra i mode: `structures::spawnAll` (Conquest **e** Sandbox).
- **Bilanciamento globale data-driven (2026-07-21, ADR-043):** rianimazione (6 parametri) e degrado
  comunicazioni (4) vivono in **`data/config/gameplay.json`**, caricato all'avvio
  (`mini::gameplay()`, header-only condiviso runtime/editor). Default = vecchie costanti; file assente
  → invariato. NB: il runtime legge la **`data/` sorgente** del repo (la copia in build è fallback).
  Editor: tab **Gameplay** (squadra/rianimazione) e tab **Comando** (`COMMS_LOST_*` + CommanderDef).
- **Editor "Comando" (2026-07-22):** tab nel BalanceEditor per autorare i **CommanderDef**
  (dropdown corpo/arma/profilo AI dal registry, checkbox abilità, hp/velocità/tinta) e i parametri
  globali del degrado comunicazioni. Le **strutture** (torri/bersagli) restano **per-mappa** nel Map
  Editor (istanze piazzate, non definizioni globali).
- **Comando nemico v2 (2026-07-21, ADR-042):** `World::enemyCommand` è una **lista di direttive**, non
  un intento singolo. Il Droide Tattico tiene i **3 fronti** più preziosi (settori + strutture), con
  stance **per-settore** dal bilancio locale (TIENI dove controlla ma è pressato, SPINGI altrove); i
  droidi si **distribuiscono** sui fronti (pesati dal bias) e seguono la stance del proprio. Unico
  override globale: **ripiegamento** se i droidi calano sotto metà. Fine del "sempre avanzata".
- **Droide Tattico = CommanderDef, non una classe (2026-07-22, ADR-044):** vive in
  `data/commanders/<id>.json`, fuori dal roster classi (non più spawnabile come truppa in sandbox).
  Riusa un `base_entity` per il corpo + override (hp assoluti, arma di autodifesa, profilo AI, abilità,
  tinta). `MapDef.commander.unit` referenzia il CommanderDef; MapEditor con dropdown da `commanders/`.
  Fallback legacy (classe-comandante) durante la transizione. `resolveCommanderArchetype` delega il
  corpo a `resolveUnitArchetype` (niente duplicazione).
- **Droide Tattico con leash (2026-07-21, ADR-041 Fase 1):** il campo `commander` ha ora
  `leash_radius` — area circolare da cui il comandante non esce (0 = fermo, legacy). Con raggio > 0
  non è più `stationary`: si muove per difendersi ma **non insegue obiettivi/segnali** e un clamp
  universale lo tiene nel raggio (misurato: deriva ≤ raggio). Autorabile nel MapEditor (marker viola +
  disco, pannello con classe dal registry, gizmo Sposta/Scala). Resta una **classe** su corpo B1
  (migrazione a entità propria = fase successiva).
- **Bersagli a priorità bassa + FocusFire (2026-07-21, doc 35):** una struttura si ingaggia solo se
  l'unità **non ha un bersaglio-unità**; entra in gioco entro `engage_radius` (proattivo) o come
  **ultimo bersaglio** quando non restano unità nemiche (entro l'aggro) — poi, distrutta, si
  ripattuglia. `FocusFire` (e comandi futuri) può forzarne la priorità. `AiProfileDef::huntTimeout`
  porta la pazienza di Hunt nel profilo (carattere).
- **Bounding overwatch esplicito (2026-07-21, ADR-032):** il grafo `positionCovers` è finalmente
  **consumato** — `worldintel::bestOverwatchForPosition` trova una posizione che copre il punto verso
  cui un compagno avanza; chi non avanza in una valutazione si sposta a coprirlo. Accanto
  all'overwatch **emergente** (ADR-035), che resta. Su firebase scatta di rado (posizioni con poca
  copertura reciproca).
- **Strutture come fatto tattico (2026-07-20, ADR-039 / doc 35):** `World::strategicTargets` è la
  **sorgente unica di intel** sulle strutture (posizione, fazione, ruolo, `priority`,
  `engage_radius`) — la leggono AiSystem, il comando nemico e la futura torre di controllo. Un'unità
  ingaggia una struttura nemica **solo se non ha bersagli-unità** ed è entro `engage_radius`
  (**default 0 = mai di iniziativa**: il sistema è inerte finché non lo si autora). Le strutture sono
  **escluse** dalla lista bersagli-unità e dal rapporto di forze del comandante.
  `physics::hasLineOfSight` accetta ora un'entità da **ignorare** e si mira al **corpo** del bersaglio
  (`y + hy/2`): senza queste due cose una struttura con collider era invisibile e incolpibile (KI #70).
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
  feed via `pushEvent`, una volta per ordine.

- **Stato "a terra" + rianimazione — Phase C (doc 26, 2026-07-17):** un membro della squadra alleata
  (non il giocatore) invece di morire va **a terra** con bleed-out 20s (`CombatSystem` intercetta il
  colpo letale, additivo; un colpo su un già-a-terra lo finisce). Rianimazione per **prossimità**
  (compagno/giocatore entro 2.5m per 3s → 50% HP) e **auto-soccorso** (il membro libero più vicino
  è dispacciato con un ordine `Revive`). Nessun soccorso in tempo → morte, contata come perdita solo
  allora (`missionStats`). Unità a terra inerme in `AiSystem` (fermata anche nel crowd, KI #50);
  **tint ROSSO** sul caduto + HUD `[A TERRA n — Xs]`; costanti in `GameConfig`. Verificato in
  `--sim`: down/revive/bleed-out tutti e tre.

- **Comandi mirati + ruota di comando (doc 26, 2026-07-17):** oltre a FocusFire (nemico) e agli
  ordini a terra, il mirino su un COMPAGNO dà `Revive` (a terra) o `CoveringFire` (vivo); il
  **mirino è verde** sugli alleati. **Ruota di comando** (tasto `CommandWheel`, default B, tenuto):
  camera congelata, il mouse sceglie Regroup/Hold/Advance, HUD radiale. `SquadOrderRequest` distingue
  ordine di squadra e ordine a un singolo membro (`directedMember`).

- **Input configurabile esteso (2026-07-17):** `InputBinding{type, code}` — un'azione può stare su
  tasto, PULSANTE MOUSE o ROTELLA (su/giù). Rimappabile dalle opzioni (cattura anche mouse/rotella),
  persistito in `<exe>/user_presets/keybindings.json` per nome azione (retrocompatibile col vecchio
  formato solo-tastiera). `getKeyName` descrive ogni tipo, nome mostrato anche per mouse/rotella
  (KI #52). `CoveringFire` = soppressione (l'alleato non si copre e spara di più).
  **Non esistono ancora**: posa prone (in attesa di pose/animazioni, tooling dell'utente),
  bilanciamento tempi/raggio Phase C.

- **Classi — meta' NPC (ADR-022 riscritto, 2026-07-16):** il modello reale ha **tre parti**:
  (1) **NPC** — la classe da' abilita', **comportamento**, loadout, aspetto → si instanzia;
  (2) **giocatore** — **non ne sceglie una**: tutte esistono insieme e si **livellano** giocando
  (GDD 11.3 "la classe non e' una scelta rigida all'inizio, ma un'identita' che emerge dal
  comportamento") → Fase 3; (3) **specializzazioni** (ARC, Commando) sbloccate da obiettivi,
  **non livellate** → terzo asse, non ancora progettato.
  **In force**: `ClassDef.aiProfileId` (e' cio' che la rende una PROFESSIONE e non un pacchetto di
  armi) + `EnemyDef.classId` → l'unita' referenzia una classe invece di ripetere
  loadout+profilo+abilita'. Ogni campo della classe vince **solo se valorizzato**; nessuna classe
  → tutto come prima (additivo). Gate ADR-018 su entrambi i riferimenti; dropdown nell'editor.
  Verificato: un alleato con `weapons:["DC-15A"]`+`ai_profile:"B1 Battle Droid"` referenziando una
  classe Heavy risolve `arma=Z-6 Rotary Blaster profiloAI=B1 Heavy Droid`.
  **`mini::classres` (`ClassResolve.hpp`) e' l'UNICA fonte della regola "la classe vince"**: la
  usano ConquestMode, WeaponAttach (modello in mano) e l'EntityEditor (anteprima). Averla scritta
  solo dentro `resolveUnitArchetype` aveva prodotto KI #43: unita' che **impugnava un'arma e ne
  sparava un'altra, coi danni di una terza**.
  **Stato dei dati (2026-07-17)**: i due alleati hanno `class: trooper`; i **due droidi nemici sono
  ancora senza classe** (il gate li segnala) e usano i campi legacy.
  **Aggiornamento (2026-07-19, ADR-023 + ADR-024)**: modello **entità=corpo / classe=professione**
  in force. `ClassDef.baseEntityId` + `hpMult/speedMult/damageMult` + `colorMult`; una classe con
  `baseEntityId` è **istanziabile da sola** (tipo-unità nei roster). `classres::effectiveUnit`
  sintetizza il corpo effettivo + overlay classe (abilità, tinta). I roster firebase/outpost
  referenziano **classi** (es. `Heavy Trooper`, `B1 Heavy Battle Droid`, `Tactical Droid`); entità
  Heavy ridondanti eliminate. **Bestiario — comandante nemico (ADR-024 riscritto, doc 32, 2026-07-20)**:
  il **Droide Tattico serie T** (classe su corpo B1) è la **controparte del comando giocatore** — uno
  stratega, non un buff. **Uno per mappa**: non è nel roster ma nel campo `MapDef.commander{unit,x,z}`
  (ConquestMode ne spawna uno solo, **stationary**, nelle retrovie → sta fermo e si difende soltanto).
  Ability `type "command"` → `CommanderComponent`; finché è vivo, `AiSystem` pubblica un **focus**
  (`World::enemyCommand`, il command post non-separatista più vicino) e i droidi in pattuglia vi
  **convergono**; ucciderlo spegne la direttiva (feed + ritorno alla pattuglia). Gate: warning se un
  comandante finisce in `enemy_types`. Verificato via `--sim`: **esattamente 1** `class=Tactical Droid`
  (col profilo AI autorato). v1 base: ordini singoli, gerarchia gradi + entità-a-sé futuri (doc 32).

- **Il giocatore NON sceglie una classe (2026-07-17, ADR-022 §4):** la riga **"Classe" del PreMatch
  e' stata RIMOSSA**, insieme a `setClassList`/`getSelectedClassId`/`ClassEntry` — senza i metodi la
  regola e' **strutturale**. Contraddiceva GDD 11.3 (*"non e' una scelta rigida all'inizio"*) e
  **sovrascriveva in silenzio** le righe *Arma primaria/secondaria* dello stesso menu. Il loadout del
  giocatore **sono** quelle righe: non si e' persa nessuna funzione.
  **`MatchSettings.classId` sopravvive** solo come **override di test** via `--class <id>`,
  dichiarato tale nel codice e annunciato in telemetria; e' preservato in `startFromPreMatch()`
  come `characterId` (lasciarlo assegnare da un indice rimosso lo avrebbe azzerato — KI #36 in
  miniatura). La meta' giocatore (XP/livelli/perk) e' Fase 3, doc 27.

- **Class System — schema (doc 14):** `ClassDef` (`data/classes/<id>.json`: primary/secondary
  weapon, abilities[], **ai_profile**, role) nel registry, id = filename stem.
  `role` è solo un tag: **nessun sistema lo consuma** (fantasma di secondo tipo, invisibile al gate
  — classe KI #25). Esempi: `trooper`, `marksman`. `abilityIds` per il giocatore e' trasportato ma
  senza effetto (KI #32). Doc 14 riscritto il 2026-07-17: dichiara stato **MISTO**.

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

- **Editor — modulo "Missioni e obiettivi" (2026-07-16):** authoring di missioni, obiettivi e
  **conseguenze** (Moduli → "Missioni e obiettivi"). Due tab: un obiettivo esiste di per se' e puo'
  essere usato da piu' missioni. **Dropdown-only dal registry** (CLAUDE.md): obiettivi composti da
  lista, mappa dal registry, **command post dalla mappa DELLA MISSIONE** (il riferimento e' una
  label: a mano sarebbe un riferimento rotto in attesa). `saveJsonRMW` (ADR-010) per ogni
  scrittura; **`id` mai scritto nel JSON** (ADR-001). **Rinomina con sweep**: nuove categorie
  `rename::Category::Objective` (→ missions.primary/optional_objectives[],
  objectives.activation.objective, linked_objectives[]) e `::Mission`.
  Ogni tipo mostra solo i campi che USA; i tipi dichiarati ma non eseguiti dal runtime sono
  selezionabili **con avviso**. Creazione = minimo valido per il gate ADR-018.
  Modulo **"Classi"** (2026-07-16): nome, ruolo, armi e abilita' da dropdown; avvisi espliciti su
  `role` non consumato (ADR-022) e abilita' senza effetto (KI #32). **Authoring dei contenuti
  completo**: nessun tipo di definizione richiede piu' di scrivere JSON a mano.

- **Conseguenze degli obiettivi (doc 25, 2026-07-16):** `ObjectiveDef.on_success[]`/`on_failure[]`
  = liste di `{type, value, target}`. `ObjectiveSystem` le applica scrivendo **solo** su
  **`World::battleState`**; ogni sistema competente legge cio' che lo riguarda → nessun
  `if (objectiveId == ...)`, e aggiungere un tipo = enum + case + un lettore.
  Tipi: `block_enemy_reinforcements` (→ ConquestMode::checkDeaths: il nemico non rimpiazza piu'),
  `enemy_accuracy` (→ AiSystem, solo team 2; moltiplicativo), `ally_reinforcements` (→ delta
  consumato da ConquestMode, che possiede i ticket), `unlock_spawn` (→ ConquestMode::spawnUnit:
  i rinforzi alleati nascono al post conquistato invece che allo spawn di mappa — completato
  2026-07-18, prima scriveva un valore che nessuno leggeva). **Tutte e 4 con un consumatore reale.**
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

- **Vantaggio dei command post = respawn-slow, non ticket-bleed (2026-07-18):** il vecchio
  "chi ha più post drena i ticket avversari" e' **rimosso** da Conquista (`updateObjectiveRules`
  ora e' un gancio vuoto). Il vantaggio ora e' sul **ritmo dei rinforzi**: in
  `ConquestMode::checkDeaths` il timer di respawn di un'unita' e'
  `respawnDelay * (1 + POST_RESPAWN_SLOW * postiNemici)` — ogni post avversario aggiunge il 15%
  (`config::POST_RESPAWN_SLOW`). **Assalto/Difesa (ObjectiveModes, ADR-014) NON toccate**: usano
  `m_bleedTimer/m_bleedInterval` come proprio timer di vittoria — i membri restano nella base.

- **Scelta del punto di respawn — mappa top-down (2026-07-18, doc 30 Phase 1):** stile Battlefront II
  2005. `IGameMode::availableSpawns()` → `[{label,pos}]` (default = spawn base; `ConquestMode` = base +
  ogni post **posseduto dagli alleati** via `CommandPosts::ownedByTeam`). Da morti (con 2+ punti),
  `Application` mostra una **mappa dall'alto** (tutta 2D in `Ui2D`, nessuna telecamera 3D → ADR-003
  intatto): pannello mappa, pareti dai box `geometry`, marker dei punti disponibili proiettati, marker
  "caduto". La cattura mouse è rilasciata: **hover + click** seleziona e schiera; `A/D`/frecce + Invio
  come fallback tastiera. Proiezione mondo→schermo unica (`rmProj`) condivisa fra render e picking.
  Il rientro **NON è automatico** con 2+ punti (il timer è solo l'attesa minima, così `respawnDelay`
  resta basso per le AI): si schiera alla **conferma** via `deployPlayerRespawn` (KI #56). Con un solo
  punto (nessun post) resta il respawn automatico. Posizione de-clippata con `nudgeOutOfColliders`
  (KI #57); HP dal setting partita (KI #55). Out of scope (doc 30): mappa tattica generale con pausa,
  post nemici sulla mappa, ordini dalla mappa.

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
  Esempio in repo: `firebase_ridge`.
  **Tipi implementati**: ReachArea, EliminateTarget, HoldAreaForDuration, CaptureZone, DefendZone,
  e **DestroyTarget** (2026-07-18). Un bersaglio strategico (`MapDef.strategicTargets[]`) è una
  struttura statica distruttibile team 2 (Health + hitbox sintetico `__strategic_target`); l'obiettivo
  la referenzia per label, la distruzione (via mailbox `World::strategicTargets` + `killedThisTick`)
  lo completa e scatena la conseguenza. Esempio: `firebase_sabotage` (torre → `enemy_accuracy`).
  Bersagli autorati nel **MapEditor** (lista + gizmo + label/HP, KI #53); box di fallback grounded
  con hitbox coincidente (KI #54). **Non esistono ancora**: EscortEntity/SurviveWave/InteractHack,
  Punti Comando.
- **Ruota di comando in slow-motion (2026-07-18):** con la ruota aperta il tempo di gioco rallenta a
  `WHEEL_TIME_SCALE = 0.15×` (non pausa) — si scala il tempo reale che alimenta l'accumulatore a
  timestep fisso; camera/selezione a velocità reale.

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
factory (ADR-008)**, **Conquista/Assalto/Difesa (ADR-014)**, **command post con respawn-slow +
scelta del punto di respawn (ADR-009)**, **veicoli Fase A (19_Vehicles)**, **weapon-in-hand runtime + viewmodel**,
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
  points (bone-bindable, rendered as boxes + text labels), inline hitbox zones (bone-bindable).
  L'arma in mano è sola-lettura (la mostra risolta dalla classe); la MANO (attach point) resta
  editabile. La POSA/scala dell'arma è sull'`WeaponDef` (`hand_scale`/`hand_rot`/`hand_offset`,
  KI #49), autorata nel Weapon Editor → "Posa in mano"; `weapon_display` sull'entità è il fallback
  legacy quando l'arma non ha posa. Runtime e anteprima risolvono con la stessa formula (WeaponAttach).
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
