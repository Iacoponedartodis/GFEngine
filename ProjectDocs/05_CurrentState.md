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
  (aggira gli ostacoli → **AI-stuck su ostacoli risolto alla radice**), combattimento =
  velocità tattica + avoidance, fallback `aiMove` se il navmesh manca. `CrowdSystem` (dopo
  AiSystem) fa registra/reap/tick/write-back; `World::nav`, `AiComponent::crowdAgentIdx`.
  Aree semantiche DANGER/COVER dai metadata del MapDef con costi `dtQueryFilter` (danger
  aggirato). Il sensing ottimizzato resta ortogonale (decide CHI; il crowd decide COME muoversi).

**Risultato misurato:** ~40 AI in simulazione ora fluidi (prima il limite di fluidità era
~30-32); AI con pathfinding reale + crowd-avoidance + evita le danger zone.
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
