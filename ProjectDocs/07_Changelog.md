# 07 — Changelog

Dated engineering changes and their architectural effect.

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
