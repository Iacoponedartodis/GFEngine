# 07 — Changelog

Dated engineering changes and their architectural effect.

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
