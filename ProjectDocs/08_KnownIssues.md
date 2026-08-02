# 08 — Known Issues

## 86. AI che non sparano pur avendo nemici davanti — DUE CAUSE STRUTTURALI TROVATE E CORRETTE, la terza resta aperta (2026-08-02, changelog 118)

**L'ipotesi principale del punto 86-orig qui sotto — "FOV senza scandaglio" — è SMENTITA dalla misura.**
Il funnel d'ingaggio (nuovo, permanente) separa i tronconi che a occhio si confondono:

| passaggio | misurato (6000 tick, Training Ground) | lettura |
|---|---|---|
| nemico entro l'aggro | 10.5k tick-sensing | il contatto **non** è raro (aggro = `sight_range` 45-75 m ≥ mappa) |
| ...dentro il campo visivo | **92-95%** | **il FOV costa il 5-8%, non è la causa** |
| ...con LOS → bersaglio | **28-39%** | **è QUI che muore l'ingaggio** |
| ha bersaglio → spara | 2,4% dei tick (il resto è cadenza di tiro, atteso) | il gate di fuoco funziona |

**Causa 1 — l'AI non vedeva bersagli che sapeva colpire (CORRETTA).** L'acquisizione mirava al `transform`
nudo, il gate di fuoco al **busto alto** (`+AI_HALF_Y*0.7`): il fix interim di KI #82 era stato applicato al
tiro e **mai** all'acquisizione. Un muretto vicino al bersaglio tagliava il raggio d'avvistamento ma non
quello di tiro. Misurato **sulla stessa traiettoria** (non fra run divergenti): **13,2% di TUTTE le
acquisizioni** esistono solo grazie al punto alzato. Stessa asimmetria corretta anche nel FocusFire.

**Causa 2 — la fase di hide si CONGELAVA (CORRETTA).** `exposeTimer` scorreva solo nel ramo "Alert +
bersaglio + non in manovra": ma nascondersi dietro una copertura è **esattamente** ciò che rompe il proprio
LOS → perso il bersaglio, il timer si fermava e `evading` (che chiude il gate di fuoco) restava attivo a
tempo indeterminato. Stesso congelamento entrando in manovra, in contraddizione col commento del codice che
prometteva *"il fuoco resta autonomo"*. A/B controllato sul sintomo:

| | tick-AI congelati (>3 s) | fase di hide più lunga |
|---|---|---|
| senza fix | **3549** | **26,6 s** (contro `hide_duration_max` = 1,8 s) |
| con fix | **0** | **1,8 s** = il massimo autorato |

È la spiegazione letterale del sintomo riportato: *"si piazzano dietro una copertura e restano lì"*.

**Causa 3 — APERTA: il 61-72% di perdita fra "nel cono" e "con LOS".** `acq_tutti_bloccati` coincide
esattamente con quella perdita: tutti i K candidati più vicini bloccati. **Non** è saturazione dei K (K=8 ≥
nemici vivi per lato) — è geometria reale. Da decidere se sia fisiologico per uno sparatutto a coperture o
se manchi una reazione: oggi un'AI senza LOS **non manovra per procurarselo**. Prossimo passo prima di
toccare codice: distinguere "coperto da una cover autorata" (corretto: c'è un contro-gioco) da "coperto da
geometria muta" (difetto di mappa → 41_TacticalWorldData/B7).

**Nota di metodo.** Gli eventi di combattimento aggregati **non** decidono questo bug: fra due run la
simulazione diverge e la differenza non è attribuibile (un primo tentativo di fix, che azzerava anche
`hasCover`, faceva calare il combattimento del 17% — sfrattava le AI dalle posizioni autorate). La decisione
è stata presa su un contatore che misura **il sintomo**, non l'esito. Volume di combattimento finale
240 → 236: invariato entro la divergenza.

**Guardie permanenti**: `occ_in_raggio`/`occ_nel_cono`/`occ_acquisito`, `gate_*`,
`evasivo_congelato_tick`, `evasivo_durata_max_s`. Quest'ultima deve restare ≤ `hide_duration_max`: se
risale, il congelamento è tornato.

## 86-orig. (storico) AI che non sparano — ipotesi "regressione del FOV" (CRITICO, 2026-08-02) — SMENTITA
- **Sintomo (utente, osservato col rallentatore)**: *"moltissimi bot, sia cloni che droidi, spesso si fermano
  e non sparano più; a volte si piazzano dietro una copertura e restano lì; è **estremamente comune** vedere
  AI che stanno davanti a dei nemici senza sparare"*.
- **Dati (telemetria esistente)**: `fuori_campo_visivo` **10.4k-16.2k** contro `tot_candidati` 14.8k-17.4k;
  `acq_tutti_bloccati` **1141-1573** contro `tot_acquisizioni` 973-1422 → **circa metà delle volte** un'AI ha
  candidati in gittata e non ne acquisisce nessuno.
- **Causa più probabile**: il campo visivo (changelog 100, A1) è stato introdotto **senza lo SCANDAGLIO**.
  Il `facing` dell'AI è legato al movimento o al bersaglio già ingaggiato: chi cammina verso un waypoint ha
  un cono fisso davanti e un **angolo cieco permanente** ai lati. Prima vedevano a 360° (sbagliato ma
  funzionale); ora vedono correttamente ma **non guardano** — peggio di entrambi.
- **Direzioni di fix da valutare** (nell'ordine): (a) **scandaglio**: rotazione lenta della vista fuori dal
  combattimento, come un soldato che perlustra; (b) allargare la fascia periferica
  (`PERCEPTION_PERIPHERAL_RADIUS`, oggi 6 m) e/o il FOV dei profili; (c) rivedere il fatto che il facing è
  incatenato al movimento (testa e corpo dovrebbero essere disaccoppiati).
- **Sospetti secondari da escludere** per il "si fermano dietro copertura": il ruolo *sopprime* (A4) che
  blocca la manovra, e la fase `evading`/hide — entrambi devono comunque lasciar SPARARE. Da verificare con
  un funnel dedicato (candidati → acquisiti → gate di fuoco) prima di intervenire.
- **PRIMA EVIDENZA dall'ispettore** (changelog 116, dump `sim-ticks` su Training Ground): 12 AI totali →
  **8 in Alert** ma **7 senza bersaglio acquisito** (`target: 0`), e **5 in `evading`** (fase di copertura,
  che per design non spara). Quindi le unità *sanno* di essere in combattimento ma non acquisiscono nessuno:
  il gate che fallisce è l'**ACQUISIZIONE**, non il tiro — coerente con l'ipotesi FOV-senza-scandaglio.
  Concausa da quantificare: quanto pesa `evading` (5/12 è molto) rispetto al FOV.
- **Prossimo passo diagnostico**: incrociare `facing_deg` con la posizione dei nemici nello stesso dump per
  misurare quanti bersagli cadono fuori dal cono, e separare il contributo di `evading`.
- **Decisione utente 2026-08-02**: rimandato, si procede con A5. **Ma è il primo item da riprendere**: è un
  difetto di gameplay fondamentale (le AI non combattono) e ogni sistema aggiunto sopra lo rende più
  difficile da bisezionare.

## 87. Performance: il numero massimo di AI senza lag sta CALANDO (2026-08-02)
- **Sintomo (utente)**: *"dovremo lavorare ancora di più con l'ottimizzazione perché il numero di AI
  contemporanee massimo per non avere lag sta iniziando a scendere"*.
- **Cause plausibili accumulate** (da misurare, non da assumere): FOV+udito per candidato (A1), la LOS extra
  del funnel, il quadro tattico della torre O(posizioni × nemici) ogni 0.33 s, `buildTacticalLinks` O(N²) al
  load, il near-miss della soppressione per proiettile × unità (A3).
- **Metodo**: **misurare prima** con Tracy (ADR-015, già integrato) invece di ottimizzare a intuito; poi le
  leve già identificate in doc 40 §11 / doc 41 §9 — AI LOD, sleeping agents, job del precompute, e il filtro
  spaziale per `buildTacticalLinks` (soglia ~300 posizioni).
- **Nota**: `--sim-ticks` misura il COMPORTAMENTO a tick fissi, non il costo per frame: per la performance
  serve il profiler, non quel contatore.

## 84. Map Editor: la barra "nome mappa" sembra inefficace — ✅ RISOLTO 2026-08-02 (changelog 109)
- **Fix**: rimosse ENTRAMBE le caselle di testo (nome visualizzato e nuovo-nome); resta un solo pulsante
  "Rinomina…" con popup. Il rename allinea filename, id e `name` → **un nome solo, uguale ovunque**.
  Corretta anche la violazione di ADR-001 (`me.id` ora è sempre il filename stem, mai letto dal contenuto).
- Diagnosi originale sotto.

## 84-orig. (storico) Map Editor: la barra "nome mappa" sembra inefficace — DIAGNOSTICATO 2026-08-02
- **Sintomo (utente)**: nel Map Editor c'è una barra col nome della mappa, accanto a quella del rename;
  scrivendoci il nome **non cambia né nell'elenco dell'editor né in partita**, mentre il campo accanto al
  tasto *Rinomina* funziona. Sospetto dell'utente: rimasuglio della prima versione del rename. Confermato in
  parte, ma la causa è un'altra e più seria.
- **Causa 1 — violazione di ADR-001 (l'id è il filename stem)**: `MapEditor::loadMaps` fa
  `me.id = j.value("id", filename_stem)` → legge l'`id` **dal contenuto del file**, mentre il runtime
  (`DefinitionRegistry::loadMaps`) usa sempre il filename. Un JSON con un campo `id` stantio fa mostrare
  all'elenco dell'editor un nome diverso da quello reale — è **esattamente KI #21**, che quel fallback
  doveva eliminare. Il save fa già `j.erase("id")`, quindi il campo è deprecato: la lettura non deve esserci.
  **Fix: una riga** — `me.id = entry.path().stem().string();`
- **Causa 2 — l'elenco mostra l'ID, non il nome**: `MapEntry` ha solo `{id, path}`, nessun `name`. Quindi
  cambiare la barra del nome non può cambiare l'elenco, *by design* ma in modo poco leggibile. In partita il
  nome è usato (`sbMaps.push_back({me.id, me.name})`), quindi lì dovrebbe cambiare: da verificare se il
  sintomo riguarda anche la partita o solo l'elenco.
- **Ridondanza reale**: due campi di testo adiacenti che sembrano fare la stessa cosa ma non la fanno
  (uno è il NOME VISUALIZZATO, l'altro l'IDENTITÀ del file). Vanno separati visivamente e etichettati, o
  unificati nel flusso di rename.
- **Correlato**: R1/R4 di doc 39 (coerenza dei moduli) e il futuro `AssetBrowser`, che centralizzerà
  identità e rename e toglierà l'occasione di ripetere questo errore.

## 85. Testi ancora TAGLIATI in alcuni punti dell'editor — DA CONTROLLARE (utente 2026-08-02)
- Segnalazione: restano scritte tagliate in alcune schermate. Da individuare e correggere applicando R8
  ([[ui-no-clipping-use-dropdowns]]): raggruppare in sezioni/tendine invece di far uscire i comandi.
- Da fare durante la migrazione dei moduli a `ModuleShell` (ADR-049), che è l'occasione naturale.

## 83. Verticalità: l'AI è CORRETTA, il limite è la GEOMETRIA della mappa — MISURATO 2026-07-27
- **Domanda utente**: "i cloni sul ponte non sembra sparino, i droidi sotto non prendono di mira quelli sopra —
  c'è un modo per verificare ESATTAMENTE?" (i proiettili lenti riempivano lo schermo di tracce non attribuibili).
- **Strumento**: funnel a 3 stadi nella telemetria (`tactical decisions`), **permanente** e a costo ~zero —
  `vert_candidati`/`tot_candidati` → `vert_acquisiti`/`tot_acquisizioni` → `vert_colpi`, più
  `vert_tiro_bloccato`, `piano_colpi` (controllo) e `acq_tutti_bloccati`. Separa le TRE cause che a occhio
  danno lo stesso sintomo: non vede / vede ma non mira / mira ma il colpo è bloccato.
- **Misurato** (`--sim` Release, 5 report): opportunità cross-quota **19-22%** di tutte le opportunità, ma
  acquisizioni cross-quota **0-6%**. E il dato che chiude la questione: **candidati cross-quota VISIBILI =
  acquisiti, 1:1** (17→17, 28→28) e **`vert_tiro_bloccato` = 0 sempre**.
  → **L'AI non ha bug di verticalità**: tutto ciò che vede lo ingaggia, e quando ingaggia spara. Il collo di
  bottiglia è la VISIBILITÀ: solo l'**1-3%** dei nemici a quota diversa è visibile.
- **Causa (geometria della mappa, non codice)**: le piattaforme di Training Ground sono **blocchi PIENI**
  (`sx=7.8, sz=8.3, top≈3.1 m`), non impalcature. Chi ci sta sopra ha gli occhi a ~4.3 m e il PROPRIO bordo a
  ~3.9 m, alto 3.1 m: il raggio deve scendere solo 1.2 m in 3.9 m → vincolo geometrico **distanza orizzontale
  ≳ 12 m** per vedere qualcuno a terra. Coerente col misurato: bersagli **<8 m sempre bloccati (0%)** — è il
  proprio blocco a nasconderli; e i **≥15 m**, che passerebbero, cadono sulle numerose **pareti alte 3 m**
  (`sx=3.6, sz=0.1, top=3.02`) diffuse sulla mappa. La LOS (slab test 3D, rotazione, clamp) è stata riletta:
  corretta.
- **Non è quindi un fix di codice.** Leve di AUTHORING: posizioni tattiche **sul bordo** delle piattaforme;
  parapetti **bassi** (≤1 m) invece di pareti 3 m sul perimetro; piattaforme con sporgenza/balcone; oppure
  accettare che il piano alto domini solo le lunghe distanze (scelta di design legittima).
- **Aperto**: manca uno strumento in EDITOR che mostri, per ogni posizione tattica, quante posizioni a quota
  diversa VEDE (riusando `worldintel::hasLineOfFire`, la stessa del runtime → nessuna doppia verità), così
  l'authoring verticale è guidato dai dati invece che a occhio. Vedi 06_Todo. [[editor-accessibility-priority]]
- **Nota di metodo**: un'ipotesi iniziale (saturazione dei K=8 candidati più vicini) è stata **smentita** dalla
  misura — con 8 nemici in campo i candidati sono già tutti. Registrata per non riproporla.

## 82. Combattimento cross-quota (sparare sopra/sotto) — FIX 2026-07-25 (changelog 81+82; conferma visiva in sospeso)
- **Radice finale (changelog 82)**: `bestFiringPosition`/`bestFlankingPosition` valutavano la LOS di tiro su un
  PIANO ORIZZONTALE (bersaglio a `p.y+1.2`, la quota della posizione) invece che alla quota REALE del nemico →
  la scelta della posizione era cieca alla verticalità. Fix: `targetY` reale in entrambe le query + tutti i
  chiamanti. Misurato: LOS cross-quota 5%→10% (2×), ingaggi cross-quota quasi raddoppiati.
- **Prima (changelog 81)**: positioning enemy-aware (scegliere una firing position col nemico noto invece del
  terreno solo-importante) — necessario ma insufficiente da solo, perché la firing position era scelta col bug
  della quota. I due fix insieme abilitano il combattimento verticale.
- **Da confermare visivamente**: cloni che sparano dal ponte, droidi che sparano in su. Se ancora scarso, resta
  la densità della mappa (molte coppie a lunga gittata bloccate) e il transito — ma la capacità c'è.
- **Muretti bassi (changelog 83, fix INTERIM)**: la mira era al centro-corpo (~0.5 m) → raggio in discesa
  tagliato da muretti bassi vicini al bersaglio. Interim: mira al busto alto (~0.85 m, dentro la hitbox 1 m) +
  proiettile dal muzzle stimato (0.5 m avanti). **Band-aid**: la soluzione vera (muzzle canna + parti corpo
  colpibili reali) richiede le POSE ([[animations-blocked]], tenute in pausa) → da sostituire quando si sbloccano.

## 82-orig. (storico) I cloni sul PONTE non sparano: il DECK blocca la LOS verso i nemici a terra (HIGH) — DIAGNOSTICATO 2026-07-25
- **Sintomo (utente, ricorrente)**: buona parte dei cloni sale sul ponte ma non spara; togliere i muretti NON
  cambia nulla.
- **Diagnosi strumentata** (DIAG temporaneo, poi rimosso; --sim Training Ground): per i team-1 in alto (y≈3.8,
  occhio ~4.6) la LOS verso il nemico più vicino è **bloccata il ~99%** (5 su 637). aggroRange=50 (gittata ok),
  nemici a 26-30 m (in portata). Il **primo collider bloccante**: nel **46%** è un box piatto sottile a Y≈3.2,
  alt 0.2 = **il DECK del ponte**; 25% un muro centrale (Y1.7, alt3.6); 22% struttura a livello ponte. Blocco
  nella **prima metà del raggio** (blk_frac 0.15-0.5).
- **Causa**: un clone su un deck solido, con l'occhio a ~4.6 m, per battere un nemico a terra (~1.2 m) deve
  tirare in DISCESA; il raggio scende sotto il livello del deck (3.3) mentre è ancora SOPRA il deck → colpisce
  il proprio deck. **Non può battere nulla sotto il livello del deck** a meno di stare proprio sul bordo. I
  muretti erano irrilevanti: il blocco è il deck. (Differenza con le "rialzate iniziali" che affacciavano
  un'area aperta più in basso: tiro in discesa libero.)
- **AGGIORNAMENTO (verificato col grafo tattico)**: la MAPPA È A POSTO. Delle 43 posizioni elevate, **39
  hanno LOS verso il basso** (426 link verso posizioni più basse) — l'autore le ha piazzate bene, scavalcano
  il deck. Quindi il clone che sta su una di quelle posizioni VEDE il terreno. Il problema è che i cloni **non
  occupano quelle 39 posizioni**: stanno in punti CIECHI del deck (centro-settore dove li manda la torre, o
  una posizione scelta per importanza ma senza LOS sul nemico corrente).
- **Vero fix = AI (positioning enemy-aware)**: quando un'unità contende un'area con nemici, deve occupare una
  posizione di TIRO con LOS reale sul nemico (una delle buone), non un punto cieco. Rompe il catch-22 (cieco →
  niente bersaglio → niente Alert → niente reposition) muovendosi verso una posizione con LOS in base ai
  CONTATTI noti, non solo alla LOS diretta. È il "refinement enemy-aware" rimandato (KI #79/#80) — ora è LA
  chiave, e generalizza (verticalità: sparare da ovunque ci sia una buona posizione).
- Collegato: [[combat-los-eye-height]] (LOS d'ingaggio corretta), [[orders-design-vision]].

## 81. `importance` di settori E posizioni tattiche SCHIACCIATA a [0,1] dal loader — RISOLTO 2026-07-24 (changelog 78)
- **Fix**: rimosso `clamp01` sull'importanza nel loader (settori + posizioni tattiche), sostituito con floor a
  0 (nessun tetto). `importance` è ora un PESO tattico relativo (≥0), non [0,1]. Commenti in `Definitions.hpp`
  aggiornati. Audit consumatori: tutti lineari (`w=importance`, `importance×k`, soglia `>0.5`) → nessuno
  assumeva [0,1], nessuna formula rotta. L'editor legge l'importanza raw dal JSON (indipendente dal loader) →
  UX invariata. Validazione contenuti OK.
- **Verificato in runtime** (telemetria `sector distribution`): Alpha 5.0, fronti 4.0, flanchi 2.5, spawn 6.0
  (prima tutti 1.0). La gradazione dell'autore ora arriva ai pesi di torre/comandante/query tattiche. Nessuna
  regressione (combattimento letale, fermi=0, no crash).
- **Aperto a valle**: ora che i pesi sono corretti, va OSSERVATO se la distribuzione migliora davvero e se
  l'importanza (ora fino a 6) OVER-domina protezione/pericolo in alcune query tattiche (`bestCoverToward`,
  `bestAdvantageInArea`) → eventuale ri-bilanciamento misurato. Ricerca #1 continua su dati puliti.

## 81-orig. (storico) `importance` SCHIACCIATA a [0,1] — diagnosi
- **Trovato con l'osservabilità della distribuzione (ricerca #1/#2, changelog 78)**: la telemetria `sector
  distribution` mostrava importanza runtime **1.0** per tutti i fronti; il JSON invece grada **0.5–6**
  (Training Ground: Alpha=5, Bravo-Charlie/Delta-Echo=4, flanchi=2-2.5, spawn=6, corsie=0.5; e le 170
  posizioni tattiche 0.5–6.0).
- **Causa**: `DefinitionRegistry` applica `clamp01()` all'importanza — settori a
  [DefinitionRegistry.cpp:512](../src/game/data/DefinitionRegistry.cpp#L512), posizioni tattiche a
  [DefinitionRegistry.cpp:412](../src/game/data/DefinitionRegistry.cpp#L412). Tutto ciò che è ≥1 → 1.0. La
  convenzione dichiarata in `Definitions.hpp` è "0..1", ma **l'autore ha usato una scala 0–6** (più alto =
  più importante). Mismatch mentale autore↔sistema.
- **Effetto (bug silenzioso)**: l'AI non può prioritizzare. Tutti i consumatori di `importance` — pesi dei
  settori (torre/comandante), `bestAdvantageInArea` (importance×1.5), `bestHoldPosition` — vedono il fronte
  centrale chiave uguale a un flanco o a uno spawn. La regia tattica dell'autore non arriva all'AI.
- **Fix (DA DECIDERE, non ancora fatto — tocca molti consumatori)**: scegliere la scala. O (a) allargare la
  convenzione a un range esplicito (es. 0–1 normalizzato o 0–N) e **ri-tarare i consumatori** che assumono
  [0,1]; o (b) rinormalizzare i dati dell'autore a [0,1]; o (c) far usare all'autore [0,1]. Va valutato con
  cura misurata (before/after sulla distribuzione via [[world-tactical-intelligence]]).
- **Minori correlati (stesso audit)**: settori quasi non sovrapposti (griglia 3×3, doppio-conteggio solo ai
  bordi — mia prima ipotesi SOVRASTIMATA); etichette spawn scambiate ("Separatist Spawn" è dove spawna la
  Repubblica) — cosmetico ma segnala metadata da ripulire.

## 80b. Ordini custom (Hold/Advance/reposition) REVERTATI — ripensare come bias sull'AI autonoma — 2026-07-24
- Il Hold/Advance custom e il reposition-gate (changelog 72/74/76) sono stati **revertati** (changelog 77):
  scavalcavano l'AI autonoma che già bounda/ingaggia bene, disconnettendo le unità dai bersagli. Base tornata
  stabile (autonoma + LOS #75 + ruota #70).
- **Prossimo approccio ordini**: NON override. Gli ordini devono **influenzare** l'AI autonoma (che già fa
  bounding overwatch): es. Advance = spingere il fronte/aggro delle unità verso una direzione; Hold = ridurre
  l'aggressività e ancorare l'area; Follow = seguire il leader. Lasciare che siano `enterHunt`/reposition a
  scegliere le posizioni (che affacciano i nemici), non una LOS parallela in SquadSystem.
- Aperto anche: enemy-aware nella scelta posizione (KI #79), e valutare se l'AI autonoma stessa "tiene" di più
  le posizioni produttive (il churn osservato) SENZA scollegarsi dai bersagli.

## 80. "Feel" degli ordini + residui da rifinire dopo il set completo (LOW/tuning) — SUPERSEDED 2026-07-24 (vedi 80b)
- **Churn / "non tengono le cover, si muovono libere"** — CAUSA TROVATA E ATTACCATA (changelog 76): il
  reposition (ADR-035) era a timer, non a "posizione esaurita", e ignorava gli ordini; il (75) l'aveva
  amplificato. Fix: `wantMove = justEngaged || !canEngageHere` (tieni se hai LOS sul nemico da qui) + reposition
  soppresso sotto Hold/Advance. Calo churn moderato in telemetria; **conferma visiva utente in sospeso**.
- Residui da rifinire (dopo il set completo di ordini):
  - **Feeling avanzata cover-in-cover** ancora da valutare visivamente col fix (76). Se debole: allungare la
    sosta produttiva o `kAdvanceStep`/soglie.
  - **Alcuni cloni restano indietro**: verificare se persiste col (76) (che li fa tenere le posizioni); se sì,
    telemetria di stuck (nav/leash).
  - Detection molto alta (contatti ~40-55) col (75): se causa ancora nervosismo, moderare il raggio.
- **Decisione (utente)**: completare prima TUTTI gli ordini (Retreat/Regroup/Follow), poi un giro unico di
  rifinitura/tuning. Vedi [[orders-design-vision]].

## 79. "Le AI non sparano da cover/rialzato" → in realtà è POSIZIONAMENTO, non tiro (HIGH) — CHIARITO 2026-07-24
- **Diagnosi strumentata (changelog 77)**: il tiro FUNZIONA. Sul ponte (y>3) le AI sparano 29/31 quando pronte;
  con ordine Advance 97/99. Il problema NON era "non sanno sparare dalle cover".
- **Vero problema**: le unità (specie sotto il mio Advance custom) stavano su posizioni **senza bersaglio
  ingaggiabile** (in Alert solo il 19% del tempo con Advance) → ferme e mute perché non c'è nulla da sparare da
  lì. È **selezione della posizione scollegata da dove sono i nemici**.
- **Fix TENUTO (changelog 75)**: LOS d'ingaggio origine=occhi (`COMBAT_EYE_HEIGHT` 1.2 m) / bersaglio=corpo —
  fa sparare dalle cover quando c'è un bersaglio (provato dai dati). Resta valido.
- **Aperto → confluisce nel rework ordini**: far sì che le unità (autonome e ordinate) prendano posizioni che
  AFFACCIANO davvero i nemici del momento (enemy-aware). Vedi KI #80 e [[orders-design-vision]].
- **Sintomo** (ricorrente, più playtest): unità che occupano cover — specie **rialzate/ponte** — poi **non
  sparano** ai nemici. Mina Hold, Advance e l'AI autonoma: occupare una posizione è inutile se non si ingaggia.
- **Causa** (diagnosi contro il codice): la LOS d'INGAGGIO ([AiSystem.cpp:1111](../src/ecs/systems/AiSystem.cpp#L1111),
  e il sensing a 954) parte dal punto GREZZO dell'unità (`ePos = {et->x, et->y, et->z}`, centro ~0.9 m) verso il
  bersaglio, e `physics::hasLineOfSight` è un raggio netto **senza altezza-occhi né peek**. Il **peek** (changelog
  69) è solo nella SELEZIONE (`worldintel::bestFiringPosition`), NON nell'ingaggio → l'unità sceglie una posizione
  "da cui col peek si spara", poi la LOS reale dal suo punto è **bloccata dalla sua stessa cover/parapetto** →
  non acquisisce il bersaglio → non spara. Stesso difetto nel mio Advance (`hasLineOfFire` dal punto fermo dice
  "niente bersaglio qui" → salta via invece di combattere).
- **Fix candidato** (delicato, impatto ampio su TUTTO il combattimento): dare all'ingaggio lo stesso principio del
  peek — es. **alzare l'origine della LOS a un'altezza-occhi/peek-over** (scavalca la cover bassa, un muro alto
  resta bloccante) e/o mirare al corpo del bersaglio (non ai piedi). ATTENZIONE: un peek ingenuo (offset
  orizzontale fisso) rischia di far "sparare attraverso" un muro adiacente — già notato per la selezione. Da
  fare con verifica misurata (`tiro_trovato/assente`, e che non spuntino colpi attraverso i muri).
- **Perché prima degli altri ordini**: è la **radice comune** dietro "non sparano" e parte di "restano indietro".
  Costruirci sopra Retreat/Regroup/Follow moltiplicherebbe il sintomo e complicherebbe la diagnosi.

## 78. Global-buffer-overflow nel testo UI coi caratteri accentati (MEDIUM) — RISOLTO 2026-07-23
- **Trovato da AddressSanitizer** (prima simulazione ASan, changelog 66): `global-buffer-overflow` in
  `stb_easy_font_print` (stb_easy_font.h:213) da `Ui2D::text` (Ui2D.cpp:61).
- **Causa**: stb_easy_font gestisce solo ASCII 32-126 e indicizza `stb_easy_font_charinfo[*text-32]` con
  `*text` di tipo **`char` SIGNED**. Un byte >127 — tipico dei **caratteri accentati UTF-8** del testo
  italiano della UI (à, è, °, …) — diventa NEGATIVO → indice negativo → lettura fuori dai limiti
  dell'array globale. UB: in Debug ASan aborta; in Release è silenzioso (glifo sbagliato, raramente
  crash). Presente da sempre, mai notato perché "innocuo" in release.
- **Fix**: `Ui2D::text` igienizza la stringa a ASCII stampabile (i byte non-ASCII → '?') prima di
  passarla a stb. La UI mostra '?' al posto degli accenti — accettabile (stb non li disegna comunque);
  per gli accenti veri servirebbe un font atlas (futuro).
- **Lezione**: primo giro di sanitizer = primo bug trovato. Il resto del codice AI/nav è risultato pulito
  (60s di sim ASan senza altri errori). Vale la pena rigirare ASan dopo modifiche grosse ([[verify-effect-not-data]]).

## 77. `--map <id>` ignorato in `--sim`/sandbox (MEDIUM) — RISOLTO 2026-07-22
- **Trovato durante la verifica di ADR-046**: il contatore `obs_vista_estesa` restava 0 anche lanciando
  `--sim --map firebase`. Il debug ha rivelato che il runtime caricava **14 posizioni tattiche
  (Training Ground)**, non le 60 di firebase — cioè `--map firebase` **non aveva effetto** sul sim.
- **Causa**: nel path `startSimulation` (Application.cpp), `currentSettings.mapId =
  sbMenu.selectedMapId()` sovrascriveva il valore già impostato da `--map`. `SandboxMenu` parte con
  `m_mapSel = 0` (default), quindi il sim girava sempre sulla mappa d'**indice 0** dell'elenco
  (Training Ground), arbitraria e non quella richiesta.
- **Fix**: nuovo `SandboxMenu::selectMapById(id)`; dopo `sbMenu.setMaps(...)`, se `--map` è stato
  passato si sincronizza la selezione del menu sandbox con quell'id. Ora il flag vale sia in partita sia
  in sim/sandbox.
- **Impatto retroattivo**: le misure `--sim` fatte PRIMA di questo fix (ADR-045 e prima) giravano su
  Training Ground, non sulla mappa nominata nel flag. Restano valide come misure (Training Ground è una
  mappa reale, con route e comando) ma **non sulla mappa che il comando indicava**. Da qui in avanti il
  flag è affidabile.
- **Lezione**: un default silenzioso (`m_mapSel = 0`) che vince su un input esplicito è peggio di un
  errore — la misura sembra valida ma è su un altro soggetto. Coerente con la memoria
  [[verify-effect-not-data]]: verificare CHE mappa è viva, non solo che il flag è stato letto.

## 74. `.items()` su json temporaneo → crash del salva-gameplay (HIGH) — RISOLTO 2026-07-22
- **Segnalato dall'utente**: il pulsante "Salva gameplay" (tab nuovo, ADR-043) faceva **crashare
  l'editor**.
- **Causa**: `for (auto& [k,v] : gameplayBalanceToJson(m_gameplay).items())` — `.items()` chiamato su
  un json **temporaneo**. È il footgun documentato di nlohmann: il proxy di `.items()` referenzia il
  json, ma il temporaneo viene distrutto dopo l'inizializzazione del range → **use-after-free**.
- **Fix**: iterare su una **variabile** nominata (`const json patch = ...; for (it=patch.begin()...)`).
- **Regola**: mai `.items()`/`.at()` su un valore json restituito per valore da una funzione. Vale
  ovunque nell'editor.

## 75. Mouse "sfasato" in fullscreen (MEDIUM) — RISOLTO 2026-07-22 (2° tentativo, causa vera)
- **Segnalato dall'utente**: a schermo intero il mouse "funziona malissimo, come se fosse
  completamente sfasato". In finestra è a posto. Il primo fix (ri-applicare la modalità mouse
  relativa) **NON ha funzionato** — perché la diagnosi era sbagliata.
- **CAUSA VERA (diagnosi corretta)**: **non** è la camera (usa delta relativi, indipendenti dalla
  risoluzione — mai rotta). È la **UI 2D**: `Ui2D::begin` faceva `glOrtho(0, m_w, m_h)` con `m_w/m_h`
  **fissi alla creazione** (1280×720), mentre il viewport GL è il drawable reale (1920×1080 in
  fullscreen). L'UI si stirava a schermo (sembrava ok), ma il **mouse arriva in pixel reali** → le UI
  cursore (respawn map, pausa, menu sandbox, pre-partita) erano sfasate del rapporto di scala.
- **Fix**: `Ui2D::begin` sincronizza `m_w/m_h` dal viewport GL reale (`glGetIntegerv(GL_VIEWPORT)`).
  Un punto solo, sistema tutte e 6 le istanze Ui2D. Coordinate UI == pixel finestra == mouse.
- **Lezione**: la diagnosi "sfasato = modalità relativa" era plausibile ma non verificata. La camera
  FPS in fullscreen non era mai rotta; il sintomo veniva dalle UI a cursore. Verificare QUALE mouse
  (relativo vs assoluto/UI) prima di scegliere la causa. Da confermare a mano.

## 75b. (primo tentativo, superato) Mouse fullscreen = modalità relativa — DIAGNOSI SBAGLIATA 2026-07-22
- Il re-assert della modalità relativa in `toggleFullscreen` è rimasto (difensivo, innocuo) ma NON
  era la causa. Vedi #75.

## 76. Rename mappa non aggiornava il nome mostrato (LOW) — RISOLTO 2026-07-22 (2 giri)
- **Segnalato dall'utente**: rinominando una mappa, partita e sandbox mostravano ancora il nome
  vecchio; dopo il 1° fix, ancora "outpost" (rinominata in "Training Ground").
- **Causa**: il rename cambia il **filename/id** (ADR-001), ma partita/sandbox mostrano `MapDef.name`,
  un campo interno separato (fallback id). Il 1° fix (sync `name` al rename) era **corretto** ma la
  mappa era stata rinominata la sera prima con l'editor pre-fix → `name` era rimasto "Outpost".
- **Fix definitivo** (non dipende dal timing del rename): **campo "Nome" nella toolbar del MapEditor**
  che edita `name` direttamente (salvato con la mappa via RMW); più la sync al rename; più il fix
  diretto del file già rinominato. `saveMap` ora scrive sempre `name` (vuoto → id), così non resta
  mai un residuo vecchio.

## Audit codice completo 2026-07-10 (issue #19-#27)

## 19. Ogni build cancella i preset partita dell'utente (HIGH) — RISOLTO 2026-07-10
- I preset erano salvati in `<exe>/data/presets/match` (MatchSettings.hpp), ma il post-build
  CMake fa `remove_directory` dell'intera `data/` di output e la ricopia dalla sorgente →
  ogni ricompilazione distruggeva i preset. Stessa classe di incidente "perdita dati" di
  ADR-010, su un altro canale.
- **Fix:** preset spostati in `<exe>/user_presets/match` (fuori da `data/`), con migrazione
  automatica best-effort dei file legacy al primo caricamento. Da smoke-testare: salvare un
  preset, ricompilare, verificare che sopravviva.

## 20. Formato preset fragile (HIGH) — RISOLTO 2026-07-10
- Tre difetti nel vecchio formato: (a) persisteva `map_index` (indice nella lista ordinata:
  aggiungere/rinominare una mappa cambiava silenziosamente la mappa dei preset — contro lo
  spirito di ADR-001); (b) parser/writer JSON artigianale a ricerca di sottostringhe, senza
  escaping (un `"` nel nome corrompeva il file); (c) il loadout (armi/abilità/gadget) non
  veniva persistito affatto.
- **Fix:** serializzazione via nlohmann::json (già linkato), i preset salvano `map_id` per id
  + l'intero loadout (`primary_weapon`, `secondary_weapon`, `abilities[]`, `gadget`);
  PreMatchMenu risolve id → indici al caricamento. I vecchi file con `map_index` vengono
  ancora letti (retrocompatibilità un-verso).

## 21. ~~`id` in-file autoritativo nei loader del registry~~ — RISOLTO 2026-07-10
- Tutti i loader ora usano SOLO il filename stem (ADR-001); il campo `id`/`profile_id`
  in-file è ignorato. Verificato prima del fix che nessun file in data/ avesse un id
  divergente dallo stem (zero mismatch) → nessun cambio di chiave effettivo.
  Smoke `--sim`: registry completo (7 armi, 2+2 unità, 2 mappe), cross-ref risolte.

## 22. ~~Cambiare arma azzera il surriscaldamento~~ — RISOLTO 2026-07-10
- Fix definitivo 2026-07-10 (28): il membro-copia `weapon` è stato RIMOSSO; l'arma
  attiva è sempre `weapons[activeWeapon]` (accessor `weapon()`) — lo stato heat vive
  in un posto solo per costruzione, nessuna sincronizzazione possibile da dimenticare.
  Smoke manuale: scaldare la primaria, Q-switch avanti/indietro → il calore resta.

## 23. ~~Collider ruotati: movimento/LOS e proiettili non concordano~~ — RISOLTO 2026-07-10
- `hasCollision` ora fa SAT 2D esatto (XZ, 4 assi) + intervallo Y sui collider ruotati
  (broad-phase AABB conservativa mantenuta come early-out); `hasLineOfSight` porta il
  segmento nello spazio locale del box (test esatto, coerente con HitTest OBB dei
  proiettili). Fast path invariato per ry = 0/90/180. Smoke manuale consigliato alla
  prima mappa con muri diagonali: scivolare lungo un muro ruotato + sparargli contro.

## 24. ~~Ultimi id di definizione hardcoded: SandboxMode~~ — RISOLTO 2026-07-10
- Fallback `"B1 Battle Droid"`/`"Clone Trooper"` rimossi: registry vuoto = nessun
  manichino + log errore (come ConquestMode, ADR-007). Zero id di definizione hardcoded
  rimasti nei game mode.

## 25. Campi editabili ma non consumati — MITIGATO 2026-07-10 (28)
- Ora marcati "(non attivo)" nella UI degli editor (Weapon/Balance/Entity) con nota
  KI #25. Il gap resta finché i campi non vengono consumati; l'aspettativa dell'autore
  però non viene più tradita in silenzio. Dettaglio originale sotto.

## 25b. (dettaglio originale) Campi editabili nell'editor ma MAI consumati dal runtime
- **✅ `fov_deg` e `hearing_range` RISOLTI 2026-07-27 (changelog 100)**: ora arrivano ad `AiComponent` e
  guidano campo visivo e udito. Erano in questa lista **dal 2026-07-10** — cioè il difetto era noto e
  tracciato, ma nessuno l'aveva chiuso, e intanto l'AI restava onnisciente e sorda. Lezione: un campo in
  questa lista non è "un dettaglio di UI", può essere un **buco di gameplay**; questa lista va riletta
  quando si progetta sopra quei sistemi.
- `min_range`, `projectile_mesh`, `reposition_chance`,
  `damage_scale`, `MapDef.metadata/navmesh/max_tickets/enemy_count/ally_count`,
  `EnemyDef.texture`, `AbilityDef.passive`. Inoltre `EnemyDef.moveSpeed` è sovrascritto dal
  `patrol_speed` del profilo AI quando il profilo esiste. Il problema è che l'editor li
  espone come funzionanti: un designer li tara e non osserva alcun effetto. Minimo: marcare
  "(non ancora attivo)" nella UI o mantenere questo elenco aggiornato.

## 26. Dati/codice morti che possono creare fallback problematici — RISOLTO IN PARTE 2026-07-10
- ~~`data/definitions/*.json`~~ eliminati (nessun codice li caricava; verificato con grep).
  Rimossi anche `data/presets/` e `data/runtime/` vuoti dalla data/ sorgente.
- ~~Preset armi hardcoded in Weapon.hpp~~: rimossi `makeBlasterPistol/HeavyBlaster/
  SniperRifle` (inutilizzati). Resta SOLO `makeBlasterRifle` come fallback di ultima
  istanza per id orfani, documentato come tale nel header.
- RESTA (scelta): geometria fallback firebase hardcoded in ConquestMode — è il fallback
  documentato da ADR-004; rimuoverlo richiede una decisione (aggiornare ADR-004).
- RESTA: `data/versions.json` non referenziato da alcun codice — valutare rimozione.

## 27. ~~Dipendenze CMake non pinnate~~ — RISOLTO 2026-07-10
- `stb` e ImGui pinnati ai commit ESATTI già in uso in `_deps` (stb 31c1ad3, imgui
  docking 6029ee3): un configure da zero riproduce la build corrente. Riconfigure +
  build verificati.

## 1. ~~Two divergent hitbox systems~~ — RESOLVED 2026-07-04 (ADR-006) + 2026-07-09 (ADR-012)
- Profile = single source of truth (ADR-006). Il residuo "last save wins" tra HitboxEditor
  ed EntityEditor è chiuso: HitboxEditor RIMOSSO, authoring solo in EntityEditor (ADR-012).

## 16. Rename profilo hitbox senza UI dedicata (LOW — accettato da ADR-012)
- Con l'HitboxEditor rimosso non c'è UI per rinominare un profilo hitbox standalone, e il
  rename di un'entità NON rinomina il profilo che referenzia (il riferimento resta valido).
  `DefinitionRename` supporta già Category::HitboxProfile: se servirà, basta esporre la UI
  nell'Entity Editor.

## 2. ~~ConquestMode dead fallback archetype ids~~ — RESOLVED 2026-07-04 (ADR-007)
- Fallback is now registry-derived (sorted enemy ids); empty registry → no spawns + error log.
- Note: the hitbox fallback `"grunt"` for team2 in `spawnUnit` still exists in `data/hitboxes/`
  and remains valid.

## 3. FreeCameraViewport GL/FBO state — CHIUSO 2026-07-10 (assorbito da #17)
- Il sospetto storico si è concretizzato come churn FBO da oscillazione dimensioni
  (KI #17, fixato con texture only-grow). Il render path FBO resta da tenere d'occhio
  solo se cambia la gestione multi-viewport ImGui.

## 4. ~~EntityEditor gizmo/marker only correct at identity transform~~ — RESOLVED 2026-07-04
- Gizmo targets are now set in world space via `toWorld()` (M = rotX*scale) and drag deltas
  are converted back with `deltaToLocal()` (inverse of M) at every call site + in `tick()`.
  Needs one manual verification pass with a scaled/rotated model (e.g. clone trooper).

## 5. ~~Clone Trooper oversized~~ — RISOLTO 2026-07-04 dall'utente
- Nuovo GLB + `mesh_scale` 0.011 autorato via editor (vedi 05_CurrentState "Resolved
  2026-07-04 (later batches)"). Voce rimasta aperta per svista, chiusa 2026-07-10.

## 6. ~~Repo hygiene~~ — RESOLVED 2026-07-04
- `.gitignore` was corrupted (contained an old CMakeLists.txt copy, no ignore patterns) —
  rewritten with real patterns. `build/` (1113 files), `imgui.ini`, `presets.cfg` untracked
  from the index (`git rm --cached`); files remain on disk. Ready to commit.

## 7. Near-duplicate data files (P0 — RILEVAMENTO AUTOMATICO dal 2026-07-15)
- **Aggiornamento 2026-07-15 (ADR-018):** il near-duplicate non è più invisibile. Il gate
  `validateContent` lo segnala automaticamente (Warning) confrontando i **nomi visualizzati**
  — identici, o l'uno prefisso dell'altro, che è esattamente come si manifestò ("DC-15A Blaster"
  / "DC-15A Blaster Rifle"). Un gate sugli **id** non avrebbe trovato nulla: con `id = filename
  stem` (ADR-001) due file hanno per forza id diversi.
  Visibile in tre posti: `GFEngine.exe --validate`, pannello editor, telemetria JSONL.
  **Sui dati attuali del progetto: 0 near-duplicate** (verificato 2026-07-15) — la bonifica
  manuale è stata fatta; ora la regressione viene intercettata da sola.
  Resta **Warning e non Error** di proposito: due contenuti possono legittimamente avere nomi
  simili, e bloccare l'avvio su un cosmetico renderebbe l'authoring ostile (policy doc 24).
  La causa radice (rename senza tooling) è chiusa da ADR-010; questo chiude il *rilevamento*.

### Storico (2026-07-09)
- **Confirmed in production data (user-reported 2026-07-09):** renaming a weapon's name/id
  without a dedicated tool produced duplicate-looking entries in the in-game loadout menu
  (e.g. "DC-15A Blaster" / "DC-15A Blaster Rifle" appearing as near-identical separate rows).
- **Root cause:** `id = filename stem` (ADR-001) is correct and must NOT change, but there is
  currently no in-editor rename command. The only way to "rename" today is to create a new
  file with a new filename/id and abandon the old one — the old file remains on disk as a
  live, loaded, near-duplicate definition. This is a process gap, not a registry bug.
- **Secondary contributing cause:** where free-text id input still exists instead of registry
  dropdowns, it is possible to reference or create ids that don't correspond cleanly to a
  single canonical file, worsening the duplicate risk.
- **Resolution path:** 06_Todo #1 (rename tooling, P0) + 06_Todo #2 (dropdown-only audit, P0)
  + ADR-010 (Proposed, see 13_ADR). Until implemented: manage via careful manual file
  deletion + full cross-reference grep (see 04_CodingStandards, rename discipline).
- Status: **IMPLEMENTED 2026-07-09 (ADR-010 Accepted) — awaiting manual GUI smoke.**
  Il comando Rinomina esiste in WeaponEditor/EntityEditor/HitboxEditor/MapEditor con sweep
  cross-reference automatico. Il duplicato originale era già stato ripulito a mano, quindi
  lo smoke previsto ("fix di un duplicato reale") va sostituito con: rinominare una
  definizione qualsiasi dall'editor e verificare che (a) il file sia rinominato, (b) i
  riferimenti nei JSON siano aggiornati, (c) il gioco carichi senza id orfani. Chiudere
  dopo questo test manuale.

## 8. ~~No game-mode abstraction~~ — RESOLVED 2026-07-04 (ADR-008)
- `IGameMode` + `createGameMode()` factory; Application usa solo l'interfaccia. Aggiungere
  una modalità = una classe + una riga nella factory. Smoke test runtime `--sandbox` passato.
- Residuo (LOW): l'id modalità viene ancora dal flag CLI (`--sandbox`); in futuro dovrebbe
  arrivare da MapDef/PreMatch.

## 9. ~~No Objective / Command Post abstraction~~ — RESOLVED 2026-07-04 (ADR-009)
- `CommandPosts` riusabile + `MapDef.commandPosts` + authoring nel Map Editor + ticket bleed
  in Conquista + catturabili in sandbox. Smoke test passato (3 post caricati e inizializzati).
- Residuo (LOW): il progresso di cattura non è ancora visibile nell'HUD (solo colore
  bandiera al completamento).

## 10. No Class concept distinct from Weapon (MEDIUM)
- `EnemyDef`/`PlayerDef` reference `weaponIds[]` directly; there is no "Class" definition
  (weapon + equipment + role composition) as its own entity. This will need to be introduced
  before Phase 3 progression (grades, unlocks) without coupling class identity to a single
  weapon id. Blocks Todo #14.

## 11. MapDef lacks AI-relevant tactical metadata — RISOLTO 2026-07-10 (lato dati)
- 15_MapMetadata implementato: `MapDef.coverPoints/patrolRoutes/dangerZones`, parse nel
  registry, authoring nel MapEditor (sezione "Metadata AI"). Il CONSUMER runtime (AI
  tattica fase 2) resta da progettare/documentare — per scelta di scope, non per gap.

## 12. Split-screen/multi-viewport support unverified — RISOLTO 2026-07-09 (caso a)
- Spike ADR-011 eseguito: due viewport + seconda Camera sulla stessa scena live funzionano
  con modifiche minori al Renderer (`drawMeshFrom`/`setViewportRect`). Toggle debug F9 in
  partita. Il lavoro restante per la feature vera (secondo input locale, secondo HUD) è
  additivo, non un redesign. Conferma visiva manuale (F9) in carico allo sviluppatore.

## 17. Memoria GFEditor crescente — FIX PROBABILE 2026-07-10 (verifica al prossimo uso)
- Misura: sulla Home la memoria è PIATTA (67MB per 75s) → il leak era nei moduli col
  viewport 3D. Root cause identificata: `FreeCameraViewport::resizeFBO` ricreava
  FBO+texture+renderbuffer a OGNI oscillazione di pixel dell'area disponibile
  (scrollbar/separatori) → churn GL continuo (era anche il vecchio sospetto #3).
  Fix: texture allocata a multipli di 64 e SOLO ingrandita, pannello mostrato come
  sub-regione via UV; ogni realloc reale è loggato su stdout. **Da confermare:** una
  sessione d'uso normale nei moduli con l'heartbeat memoria stabile.

## 18. Sandbox: verifica post-fix "nemici non muoiono" (PENDING SMOKE)
- Causa identificata e corretta (profilo hitbox svuotato → headshot nel vuoto, vedi
  Changelog 2026-07-09 (7)). Profilo B1 ripristinato dal .bak. Da confermare al prossimo
  playtest sandbox: colpi a testa/corpo → log `hit:`/`kill:` in engine_run.log.

## 13. Hitbox zone rotation ignorata — RISOLTO 2026-07-10 (test OBB condiviso)
- Nuovo `physics/HitTest.hpp`: segmento-vs-OBB (yaw entità * eulerDeg zona, ordine
  Y*X*Z identico al wireframe editor), usato SIA dal CombatSystem SIA dal mirino
  (che ora è un segmento di 80m con lo stesso helper): mirino e proiettili
  concordano per costruzione anche su zone inclinate (testa B1 a -58°).

## 14. Asset default mancanti (LOW) — PARZIALE 2026-07-11
- `assets/models/default.obj` CREATO (cubo unitario): niente più errore di parsing.
  Resta `assets/textures/default.png` mancante → fallback checkerboard (una riga di log
  cosmetica all'avvio). Basso: il fallback funziona.

## 15. ~~Salvataggi distruttivi (classe di bug)~~ — RESOLVED 2026-07-09 (ADR-010)
- `saveJsonRMW` centralizzato (`util/JsonSave.hpp`) con backup `.bak` automatico; **tutti**
  i save path di tutti i moduli migrati (nessun `ofstream` JSON fuori dall'helper). La
  regola RMW è ora un vincolo strutturale. I nuovi moduli DEVONO usare l'helper
  (04_CodingStandards).

## 28. Spawn giocatore a Y fissa → incastro nel pavimento / respawn sospeso — RISOLTO 2026-07-13
- Player e respawn usavano `SPAWN_Y=0.86` hardcoded (piedi a y=0); il "Pavimento" di firebase
  ha il top a y=0.1 → nascita incastrata → step-up lo lanciava a occhi 1.42 poi assestamento.
  Evidente dopo una simulazione ("respawn sopra un muro, sospeso anche muovendosi").
- **Fix:** `mapquery::groundedSpawn` (suolo reale via `groundHeightAt` + `findFreeSpot`) in
  Sandbox/Conquest `start`. Probe runtime: camY stabile 0.95 dal frame 0. Vedi 07_Changelog.

## 29. Veicoli si bloccano in corrispondenza delle 4 casse laterali (LOW) — RISOLTO 2026-07-19
- Segnalato 2026-07-13: lo speeder restava bloccato passando vicino alle casse ai lati della
  mappa anche con spazio apparente. Causa: la collisione trattava il box in movimento come **AABB
  allineata al mondo** (VehicleDrive usava l'AABB avvolgente della sagoma ruotata) → agli angoli
  ≠ 0/90° il box si gonfiava e urtava "aria".
- **Fix**: `hasCollision`/`slideMove`/`slideMoveWithStepUp` hanno ora un parametro **`queryYawRad`**
  opzionale (default 0 = AABB, comportamento **identico** per fanteria/proiettili → rischio zero per
  i chiamanti esistenti). Con yaw ≠ 0 il test è **OBB-vs-OBB esatto** (SAT 2D a 4 assi,
  `obbIntersectsRotatedCollider`). VehicleDrive passa il box REALE dello speeder + `yr` invece
  dell'AABB avvolgente. A 0/90° il risultato è identico a prima. Build + `--validate` + `--sim`
  senza crash; il "non si blocca più con spazio apparente" resta da smoke manuale (guidare fra le
  casse in diagonale).

## 30. Sim AI: "impossibile muovere la visuale in osservatore" — NON RIPRODOTTO 2026-07-13
- Segnalato insieme al bug #28. Probe runtime in simulazione: tutti i gate corretti
  (`mouseCap=1, observerFly=1, thirdPerson=0, sbMenuOpen=0, state=Playing`) → `processMouse`
  VIENE chiamato e ruota la camera. Possibile confusione col rimbalzo di spawn (#28, ora
  risolto) o col glitch mouse primo-frame (già mitigato dal flush in `Window::setMouseCaptured`).
  **Da ri-testare a mano** dopo questi fix; se persiste, catturare un caso concreto.

## 31. AI attraversano i veicoli (regressione nav Phase B, LOW) — RISOLTO 2026-07-19
- Con DetourCrowd (ADR-017) il movimento AI non usa più `hasCollision`; il navmesh è costruito
  solo dalla geometria statica di `MapDef` e il crowd non conosce i veicoli (entità dinamiche
  con collider). Quindi le AI CAMMINAVANO ATTRAVERSO gli speeder parcheggiati.
- **Fix (opzione "check collisione nel movimento AI")**: nel write-back del `CrowdSystem` (dopo che
  il crowd ha aggiornato le posizioni), ogni AI viene spinta fuori dall'OBB di ciascun veicolo lungo
  l'asse di **minima penetrazione** (deterministico, niente jitter; convenzione assi identica a
  `physics::hasCollision`). Risolve SOLO la penetrazione nei veicoli — la geometria statica resta
  gestita dal navmesh. Così l'AI scivola lungo lo speeder invece di attraversarlo. Veicoli raccolti
  una volta per tick (sono pochi e fermi → costo trascurabile). Build + `--sim` senza crash;
  comportamento da smoke manuale (veicolo in partita).

## 32. Nessun sistema abilità/gadget lato GIOCATORE (MEDIUM) — APERTO 2026-07-14
- Il PreMatch fa scegliere `abilityIds`/`gadgetId` al giocatore ma NON vengono applicati
  all'entità player (le abilità esistono solo per le AI: `ShieldComponent`/`AbilityComponent`
  allo spawn da `def->abilityIds`). Righe loadout marcate "(non attiva/o)" (KI #25) finché il
  sistema non esiste. Chiarimenti dall'utente per il futuro: la schivata/roll è un COMANDO BASE
  (già funzionante via tasto), non un'abilità; lo shield va concepito come GADGET, non abilità.
  Lavoro futuro (abilità/gadget player-side).

## 71. Le strutture erano "autorabili" solo sulla carta (HIGH) — RISOLTO 2026-07-21
**Quattro difetti segnalati dall'utente in una volta, tutti confermati.** Avevo dichiarato
"rotazione e scala autorabili con gizmo" in ADR-036 e nel changelog (33): **non era vero**, e non
l'avevo verificato. Le quattro cause erano indipendenti fra loro.
1. **Barra gizmo**: `gizmoModeBar(m_viewport, boxSel, boxSel)` abilitava Ruota/Scala **solo per i box
   della geometria**. Su un bersaglio i pulsanti restavano grigi e la modalità ricadeva su Sposta.
   I gestori dei delta per i bersagli **esistevano già** in `tick()` — non venivano mai raggiunti.
   → Ora ogni tipo di selezione dichiara cosa sa fare.
2. **Viewport dell'editor**: i bersagli erano disegnati con `ry = 0` e lato **fisso 2.5**, ignorando
   `t.ry` e `t.scale`. Gli slider cambiavano il dato e chiamavano `updateViewport()`, ma il disegno
   non li leggeva → sembrava che non facessero nulla. → Ora usa scala e rotazione autorate.
3. **Runtime**: `sc = box ? 2.5f : meshScale` — con il box di fallback (nessuna mesh) `meshScale` era
   **ignorata del tutto**, quindi la scala non aveva effetto nemmeno in gioco. → Ora la scala
   **moltiplica** la base 2.5 (default 1.0 → comportamento invariato).
4. **Sandbox**: `SandboxMode` **non spawnava affatto** le strutture — il codice viveva solo dentro
   `ConquestMode`. Erano dati della mappa che in sandbox non esistevano. → Estratto in
   `structures::spawnAll` (header condiviso), chiamato da entrambi i mode.
- **Lezione**: tre di questi quattro sono "il dato cambia ma non si vede". Avevo verificato che il
  campo si salvasse nel JSON e ho chiamato la feature fatta. **Il criterio giusto non è "il dato è
  scritto" ma "si vede l'effetto"**, e quello richiede lo smoke manuale che avevo dichiarato dovuto
  e mai preteso prima di dire "fatto".

## 72. Le AI attraversavano le strutture: il collider non tocca il navmesh (HIGH) — RISOLTO 2026-07-21
- **Segnalato dall'utente** già la prima volta e da me "risolto" in ADR-036 aggiungendo il
  `ColliderComponent`. **Il collider era necessario ma non sufficiente**: governa giocatore e
  proiettili, mentre **le AI si muovono sul navmesh via DetourCrowd**. `NavManager::build`
  voxelizzava **solo `map.geometry`**, quindi sotto la torre il navmesh non aveva alcun buco e il
  crowd ci passava dentro tranquillamente.
- **Fix**: le strutture entrano nell'input del navmesh usando **esattamente i semiassi del collider**
  (derivazione unica in `StrategicTargetDef::solidHalfExtents`), così collisione e navigazione non
  possono divergere.
- **Misurato**: `input_tris` **264 → 288** su firebase, cioè (22 box + **2 strutture**) × 12.
- **Lezione**: "solido" in questo motore ha **due** significati indipendenti — collisione e
  navigazione. Aggiungerne uno solo e dichiarare la cosa risolta è stato un errore di comprensione
  del sistema, non una svista.

## 70. Le AI non prendono di mira le strutture (MEDIUM) — RISOLTO 2026-07-20 (ADR-039, doc 35)
- **La causa NON era di design** (avevo ipotizzato che il selettore di bersaglio ignorasse le
  strutture): erano **tre bug in fila**, ognuno sufficiente da solo a rendere una struttura
  inattaccabile.
  1. **`hasLineOfSight` non escludeva il bersaglio** → il collider aggiunto da ADR-036 **bloccava la
     visuale verso il centro della struttura stessa**. Regressione mia, introdotta senza accorgermene.
  2. **Si mirava all'origine del transform**, che per una struttura sta **a terra**: il segmento
     raschiava il collider del pavimento.
  3. **Il LOS al momento del tiro** aveva entrambi i difetti → dopo aver corretto la sola *selezione*,
     la telemetria mostrava **396 ingaggi per finestra e zero danni**: le AI sceglievano la torre e poi
     non le sparavano.
- **Fix**: `ignore` in `hasLineOfSight`; punto di mira sul **corpo** (`y + hy/2`) in selezione e tiro;
  strutture fuori dalla lista bersagli-unità e reintrodotte solo dal percorso autorato (doc 35).
- **Misurato**: prima **0** ingaggi possibili; dopo, torre distrutta dalle AI a t=41.5 s con HP reali
  (300) → rete di comunicazione degradata di conseguenza.
- **Lezione**: avevo classificato questo come "decisione di design da prendere" quando era un difetto
  da diagnosticare. La differenza l'ha fatta misurare invece di ragionare — e i tre bug si
  nascondevano l'uno dietro l'altro: sistemarne uno solo non produceva **nessun** cambiamento visibile.

## 70b. (diagnosi originale, superata) Le AI non prendono di mira le strutture — APERTO 2026-07-20
- **Emerso** verificando ADR-038: in `--sim` una torre di comunicazione da 300 HP **non viene mai
  distrutta**, nemmeno piazzata dentro lo spawn avversario. Abbassata a 5 HP cade dopo ~30 s — cioè
  muore di **fuoco vagante**, non perché qualcuno la stia attaccando.
- **Causa probabile** (non ancora confermata leggendo il selettore di bersaglio): l'AI sceglie il
  bersaglio più vicino fra le entità con Team+Transform, ma una struttura non si comporta come
  un'unità (non spara, non si muove) e in pratica non entra mai nel ciclo di ingaggio — resta un
  bersaglio del **giocatore**.
- **Perché conta ora**: con la rete di comunicazione (doc 34) la torre è un obiettivo che *cambia la
  battaglia*. Se solo il giocatore può abbatterla, l'intero sistema esiste solo nella direzione
  "il giocatore attacca i droidi", mai il contrario: i droidi non minacceranno mai la torre dei cloni.
- **Non è una svista da correggere di corsa**: "le AI devono attaccare le strutture" è una **decisione
  di design** (con quale priorità? solo su ordine? solo alcune classi?). Va deciso, non improvvisato.

## 69. Il `Follow` fisso era la causa del "si muovono tutti insieme" (HIGH) — RISOLTO 2026-07-20 (ADR-037)
- **Segnalato dall'utente** in più forme: *"si muovono tutti insieme, facendo sempre le stesse strade
  e finendo tutti abbastanza aggregati"*; e la direttiva finale, *"dobbiamo togliere il comando
  'follow' fisso"*.
- **Causa**: `SquadSystem` assegnava `Follow` a **chiunque non avesse un ordine attivo** — un
  placeholder di Phase A mai rimosso. Non era un difetto dell'AI: era l'ordine Follow che faceva
  correttamente il suo mestiere su **tutta la squadra, tutto il tempo**. Telemetria: `sq_follow` 4-9
  su 9 membri. Da qui discendevano anche (a) la rianimazione troppo efficace, perché i membri
  restavano ammassati e c'era sempre un soccorritore a portata, e (b) il fatto che i **cloni fossero
  meno indipendenti dei droidi**, che non avendo squadra non hanno guinzaglio.
- **Fix (ADR-037)**: nessun ordine di default (`OrderType::None` → Patrol/Alert/Hunt normali, truppa
  indipendente); `Follow` diventa il 4° settore della ruota di comando, che legge **LIBERI** e revoca
  quando la squadra sta già seguendo; ramo esplicito di revoca in `SquadSystem` (il blocco di
  assegnazione filtra su `isImplemented()`, che `None` non soddisfa); HUD che dichiara `LIBERI`.
- **Misurato**: `sq_follow` **0 per tutta la partita**; a t=4 s 10/10 senza ordini e 21 unità in
  patrol insieme; al picco 3 manovre avviate su 6 valutate — i cloni ora manovrano come i droidi.
- **Lezione**: un default messo "per far funzionare qualcosa" diventa invisibile e viene scambiato per
  comportamento emergente. Andava rimosso quando è nato il vero sistema di ordini, non due fasi dopo.

## 73. La torre di controllo ammassa i cloni quando i segnali sono pochi (MEDIUM) — RISOLTO 2026-07-21
- **Segnalato dall'utente**: *"dopo un po' non so perché hanno iniziato ad aggregarsi tutti lì
  vicino"* (vicino alla torre di comunicazione nemica).
- **Causa**: la scelta decorrelata dal `bias` (ADR-040) disperdeva i cloni **solo se c'erano
  abbastanza segnali**. A fine partita i settori tenuti venivano filtrati e ne restavano 1-2: tutti i
  cloni convergevano lì — esattamente il comportamento che ADR-040 esisteva per evitare.
- **Fix (saturazione)**: ogni segnale conta le truppe già presenti (`Signal.crowd`); oltre
  `ALLY_SIGNAL_CAPACITY` (=3) **smette di attirarne altri**. Chi è già dentro un segnale ci **resta**
  (stabilità: senza, un segnale saturo verrebbe abbandonato → si svuota → tutti tornano → pendolo).
  Quando tutti i segnali sono coperti, i cloni in più **tornano alla propria pattuglia** invece di
  ammassarsi. La dispersione non è più solo emergente dal numero di segnali.
- **Misurato**: `segnale_affollamento_max` **0-1** per tutta la partita (prima tutti sullo stesso
  punto); a fine partita 7-8 cloni pattugliano invece di pilarsi su 1-2 segnali. **Non-regressione**:
  il comportamento "ultimo bersaglio" (ramo separato) resta — struttura finale distrutta a t=103.
- **Leva**: `ALLY_SIGNAL_CAPACITY` (basso = più dispersione). Sensazione da rifinire in partita.

## 68. `Hunt` non scade mai (MEDIUM) — RISOLTO 2026-07-21 (riformulato lo stesso giorno)
- **Fix**: dopo `AI_HUNT_TIMEOUT` (20 s) l'unità degrada a **Search**, non direttamente a Patrol —
  guardarsi intorno prima di rinunciare è il comportamento sensato, e Search ha già il suo timeout
  (15 s) verso Patrol. La catena completa Alert → Hunt → Search → Patrol ora si chiude sempre.
- **Misurato** (sim 150 s): `in_hunt` 3 → `in_search` 6 → `in_patrol` 7 nelle finestre successive.
  Prima: `in_hunt` inchiodato a 1 per centinaia di secondi.
- Il valore 20 s è il doppio della sosta in Search: inseguire deve durare più del cercare, ma non
  per sempre. È una leva da rifinire provando.

### Diagnosi originale e sua correzione (storico)
> ⚠️ **La diagnosi originale era sbagliata.** Avevo scritto "la partita non finisce quando una
> fazione è spazzata via". **La partita finisce**: quello che prosegue è la **simulazione in
> sandbox** — e *deve* proseguire, serve all'utente per osservare il comportamento delle AI
> (chiarito dall'utente il 2026-07-21). Avevo scambiato lo strumento di osservazione per un difetto
> del game mode. Resta valido **solo** il secondo difetto, qui sotto.
- **`Hunt` non ha timeout**: un'unità insegue un `lastKnown` indefinitamente invece di degradare a
  Search e poi a Patrol. In sandbox si vede bene: unità che restano in `hunt` per centinaia di
  secondi su un contatto che non esiste più. Vale per entrambe le fazioni.

## 68b. (diagnosi originale, SUPERATA) La partita non finisce quando una fazione è spazzata via
- **Osservato** nella run 10v10 di ADR-037: distrutta la torre, i rinforzi nemici si bloccano
  (consequenza voluta) e a ~130 s i separatisti sono azzerati. Ma la partita **prosegue per oltre 400
  secondi**: 4 alleati in patrol e 1 in `hunt` permanente su un contatto che non esiste più, con
  `stuck` 4-5/minuto. Nessuna condizione di vittoria scatta.
- **Due difetti distinti, da non confondere**:
  1. **Nessuna condizione di annientamento** nel game mode: la vittoria è legata a post/rinforzi, non
     al "nemico azzerato e senza rinforzi".
  2. **`Hunt` non scade**: un'unità insegue un `lastKnown` indefinitamente invece di degradare a
     Search e poi a Patrol. Vale per entrambe le fazioni.
- **Non ancora affrontato.** Il punto 2 è il più rilevante per il comportamento; il punto 1 è di
  regola del mode.

## 67. La simulazione `--sim` non era rappresentativa (HIGH — strumento di misura) — RISOLTO 2026-07-20
- `MatchSettings.team1AiCount = 1` di default e `--sim` lo usava → **1 alleato contro 6 nemici**.
  Tutte le misure sul comportamento degli alleati (squadra, ordini, manovre) erano prese su uno
  scenario che in partita non esiste: per questo i problemi riportati dall'utente non comparivano nei
  dati. **Il difetto era nello strumento, non nel gioco.**
- **Fix**: `--sim` prende `ally_count`/`enemy_count` **dalla mappa**. Lezione: un tool di misura va
  validato prima di fidarsi delle sue conclusioni.

## 66. Bersagli strategici senza collisione, team cablato, non ruotabili (MEDIUM) — RISOLTO 2026-07-20
- **Segnalato dall'utente** ("le AI ci passano in mezzo"). Confermato: lo spawn dava Transform, Team,
  Health, MeshRenderer e Hitbox ma **nessun `ColliderComponent`**.
- Inoltre il **team era cablato a 2**: una torre dei cloni sarebbe nata separatista. E la rotazione
  era fissa a 0, la scala non autorabile dall'editor.
- **Fix**: collider (semiassi autorabili, 0 = dalla scala; altezza piena per compensare l'offset di
  grounding della mesh), `team` autorato, `ry` + scala autorabili con gizmo ruota/scala.
  Prerequisito delle torri di comunicazione/controllo per entrambe le fazioni.

## 64. Il guinzaglio di squadra annullava le manovre tattiche dei cloni (HIGH) — RISOLTO 2026-07-20
- **Sintomo (utente)**: nessun cambiamento visibile nel comportamento; cloni ammassati che fanno
  avanti-indietro o "girano su sé stessi all'infinito"; **"i droidi sembrano funzionare un po' meglio
  dei cloni"**.
- **Causa**: il blocco del guinzaglio (`Follow`) gira **DOPO** il blocco Alert e **sovrascrive**
  `moveDX/moveDZ`. Un clone che avviava una manovra tattica (ADR-035) veniva riagganciato verso il
  leader al primo passo fuori raggio → la manovra non partiva mai e l'unità oscillava. **I droidi non
  hanno squadra, quindi nessun guinzaglio**: ecco perché sembravano più intelligenti. Il sintomo
  "girano all'infinito" è il facing ricalcolato su un delta che cambia segno a ogni tick.
- **Fix**: il guinzaglio **non si applica** durante una manovra attiva (`!repositionActive`), e in
  **Alert** il raggio del Follow si allarga (8 → 15 m): un membro ingaggiato deve poter manovrare e
  chiudere la distanza, pur senza inseguire attraverso la mappa.
- **Lezione di processo**: il bug è esistito per più incrementi perché si misuravano solo crash e
  stuck, mai le DECISIONI. Vedi KI #65.

## 65. Nessuna osservabilità sulle decisioni tattiche dell'AI (MEDIUM) — RISOLTO 2026-07-20
- **Segnalato dall'utente**: *"non so nemmeno come controllare se le AI stanno davvero leggendo e
  capendo i metadata, o se li stanno effettivamente utilizzando"*. Valeva anche per me: si misuravano
  crash, stuck e cambi di stato — nulla che dicesse se le query tattiche venissero chiamate, cosa
  rispondessero e cosa l'AI decidesse. Si sono così accumulati incrementi non verificati.
- **Fix**: evento telemetria periodico **`AI / tactical decisions`** con: censimento degli stati
  (patrol/alert/hunt/search/fermi/in_manovra), approcci scelti (diretto/fianco/tiro/dominante),
  manovre valutate/avviate/bloccate, e soprattutto **hit vs miss delle query** (`tiro_trovato` /
  `tiro_assente`, idem fianco) — che distingue "il mondo non offre nulla" da "l'AI non chiede".
- **Primo uso**: ha mostrato che i metadata **funzionano** (`tiro_assente` sempre 0) ma che
  `hunt`/`search` erano **sempre 0** → tutti in contatto permanente nello stesso punto: il problema
  non era nelle query ma nell'aggregazione. Da lì i fix #64 e il raggio di condivisione contatti.

## 63. Battaglia sempre uguale: due blocchi sullo stesso fronte (MEDIUM) — RISOLTO 2026-07-20
- **Segnalato dall'utente**: ogni partita identica, tutti in gruppo sullo stesso fronte nonostante le
  route coprissero la mappa; a fine scontro i cloni ammassati oscillavano su 1-2 m.
- **Causa principale**: `AiSystem` aveva una **shared awareness a livello di esercito** — UNA sola
  `lastKnown` per team propagata a TUTTE le unità. Un solo avvistamento mandava l'intero esercito in
  Hunt sullo stesso punto → nessuna indipendenza possibile, per costruzione.
- **Fix**: contatti come **lista con posizione**, adottati solo entro `AI_CONTACT_SHARE_RADIUS` (20 m)
  → fronti indipendenti. Inoltre: in `Advance` ogni droide sceglie il **proprio** post catturabile più
  vicino (non il focus unico del comandante); i membri in `Follow` non pattugliano (fine
  dell'oscillazione da tira-e-molla col guinzaglio); punti di Search **clampati ai confini mappa**.
- **Misurato**: unità sparse su tutta la mappa in `--sim`; `stuck` 5 → 3. Dinamicità reale = giudizio
  in partita. **Leva di tuning**: `AI_CONTACT_SHARE_RADIUS` (più basso = più indipendenza/caos).

## 62. AI non seguivano i percorsi in modo fluido (MEDIUM) — RISOLTO 2026-07-20 (4 cause)
- **Segnalato dall'utente** più volte ("non riescono a usare in maniera fluida i path"). Diagnosi
  partendo dalla telemetria `stuck` (stato + posizione + durata), non da ipotesi. Quattro cause:
  1. **Anti-stuck a soglia fissa** (0.05 m/tick) vs `patrol_speed 2.5` → 0.042 m/tick a 60 Hz: un
     droide in marcia normale risultava bloccato dopo 1.2 s e `advancePatrol` lo faceva **saltare al
     segmento successivo senza arrivarci**. → soglia ora **proporzionale alla velocità**.
  2. **`patrolDwell = 12 s` applicata a OGNI waypoint** (serviva solo a catturare i post): pattuglie
     ferme quasi sempre. → sosta lunga **solo sui command post** (`worldintel::nearCommandPost`).
  3. **`requestMoveTarget` scartava in silenzio** i target non agganciabili col piccolo extent del
     crowd (lastKnown in Hunt, punto casuale in Search) → agente immobile. → **extent crescenti**
     (2/6/14 m) + confronto "stesso target" sul punto agganciato (evita replan a ogni frame).
  4. **Segnale `stuck` inaffidabile**: in Alert il timer accumulava (log soppresso ma non il timer) →
     falsi positivi da ~2.8 s riportati all'uscita dallo stato. → in Alert il timer si **azzera**.
- **Misurato**: `--sim` 20 s, eventi `stuck` **da 9 a 0**. NB: la sim entra in combattimento quasi
  subito → non misura bene la pattuglia; i punti 1-2 restano da valutare **in partita**.

## 61. HUD contava strutture e comandante come "nemici vivi" (MEDIUM) — RISOLTO 2026-07-20
- **Segnalato dall'utente**: con in campo solo il Droide Tattico l'HUD segnava **2 nemici vivi**.
  Ipotesi dell'utente (corretta): venivano contati il **comandante** e la **torre comunicazioni**.
- **Causa**: il conteggio in `Application` sommava **qualunque** entità con `Team` + `Health` viva e
  non-proiettile → includeva bersagli strategici (strutture), veicoli e il comandante.
- **Fix**: si contano solo le **truppe** — esclusi `strategicTargets`, entità con `VehicleComponent`
  e con `CommanderComponent` (il Droide Tattico è un obiettivo vivente, non una truppa: doc 32).

## 60. Ruota/scala non abilitate sui marker metadata (LOW — authoring) — RISOLTO 2026-07-20 (ADR-025)
- Segnalato dall'utente: selezionando una **copertura** non si sceglieva il tool di rotazione.
  Diagnosi: non era una rottura generale (i box di geometria ruotano/scalano); i **marker metadata**
  erano solo-sposta by design, e il `facing` del cover si regolava solo via slider.
- **Cap abilitati (Fase 0, ADR-025)**: gizmo ruota/scala sui metadata dove esiste un campo —
  cover→ruota(`facing`), veicolo→ruota(`ry`), danger→scala(`radius`), post→scala(`radius`), tactical
  point→ruota (ADR-027). Gli altri (spawn, route, target) restano solo-sposta.
- **Fix vero (2026-07-20)**: l'utente segnalava che ruota/scala "ancora non si selezionano" sui
  metadata. I cap erano corretti: il problema era **l'accesso al tool** (solo scorciatoia tastiera
  1/2/3, con viewport in hover + mouse libero — poco affidabile). Aggiunti **pulsanti cliccabili**
  Sposta/Ruota/Scala nel viewport (disabilitati/grigi sui target non supportati). Ora la selezione è
  affidabile e scopribile. Build-verified; **smoke manuale**: cliccare Ruota/Scala e trascinare.

## 59. ~~Nessun editor di abilità~~ — NON VALIDO (rettificato 2026-07-20)
- Annotazione errata: **esiste** un editor di abilità nel `BalanceEditor` (tab Abilità) con dropdown
  del tipo (`shield`/`roll`/`melee`/`jetpack`/`missile`/`command`) e drag dei param. L'authoring delle
  abilità è coperto; il ClassEditor assegna l'ability alla classe. Nessun buco reale — voce ritirata.

## 58. Il Droide Tattico non "resta nelle retrovie" (MEDIUM) — RISOLTO v1 2026-07-20 (ADR-024, doc 32)
- Design (GDD App. B + chiarimento utente): il comandante deve stare **nascosto e protetto**, al
  massimo autodifendersi. La v0 lo spawnava come truppa del roster (molti + avanzavano).
- **Risolto v1**: è un **singolo obiettivo vivente per mappa** (campo `MapDef.commander`, non
  `enemy_types`), piazzato nelle **retrovie** e spawnato **stationary** → AiSystem non lo muove mai
  (movimento sotto `!ai->stationary`); fronteggia/spara solo a chi vede. Profilo AI autorato
  (`Tactical Droid`, aggression 0). Resta come **tuning-dati** (non bug): abbassare `sight_range` per
  ingaggiare solo minacce vicine; l'ingaggio "solo-se-attaccato" vero e il ripiego-in-copertura sono
  raffinamenti futuri (doc 32 Out of Scope). Il comportamento pieno (gerarchia) → [[command-rank-system]].

## 57. Respawn dentro la geometria di un command post → giocatore incastrato (MEDIUM) — RISOLTO 2026-07-18
- **Segnalato dal playtest**: rientrando da un command post si spawnava DENTRO il cubo del crinale
  del post e si restava bloccati.
- **Causa**: il punto di spawn di un post è il centro della bandiera (`def.x/z`), che può coincidere
  con la geometria autorata lì (box/copertura). Spawnare esatti sul centro = dentro un collider.
- **Fix generale (non per-mappa)**: la posizione di respawn passa da `physics::nudgeOutOfColliders`
  prima di schierare. Vale per QUALSIASI ostacolo (muri, veicoli, geometria dei post) su qualsiasi
  mappa — niente fix ad hoc che domani sarebbero inutili.
- **Migliorato 2026-07-18 (2° giro)**: con una lastra RIALZATA e più larga del raggio orizzontale (la
  base di Alpha) la ricerca solo-orizzontale falliva → si nasceva ancora incastrati. Ora
  `nudgeOutOfColliders` è una ricerca **3D a livelli di quota**: prima l'orizzontale a terra (muri: di
  lato), poi — se tutto il livello è bloccato — sale e riprova, così ci si posiziona SOPRA la
  piattaforma. Sempre generale (nessun caso speciale per Alpha/firebase), vale anche per gli spawn AI.
- Lezione: uno spawn su una posizione autorata da dati (post, waypoint) non è mai garantito libero;
  il de-clip va fatto al momento dello spawn, in un helper condiviso, non tarando le coordinate — e
  deve poter risolvere anche **in verticale**, non solo sul piano.

## 56. Respawn automatico allo scadere del timer: la scelta del punto non aveva priorità (MEDIUM) — RISOLTO 2026-07-18
- **Segnalato dall'utente**: con `respawnDelay` basso (0.5–1 s, usato di proposito per far rientrare
  in fretta AI e nemici) il giocatore veniva rigenerato allo spawn di default PRIMA di poter scegliere
  il punto — la scelta introdotta il 07-18 diventava inutile.
- **Causa**: il respawn del giocatore scattava da solo quando `respawnTimer <= 0`, allo spawn
  correntemente selezionato (default = Base), senza attendere una conferma.
- **Fix**: con **2+ punti disponibili** il timer è solo l'**attesa minima**; il rientro avviene in
  `deployPlayerRespawn` quando il giocatore **conferma** (click sinistro / Invio / Spazio). Finché non
  conferma resta a terra con l'overlay di scelta attivo (A/D o frecce per cambiare punto). Con **un
  solo punto** (nessun post catturato) non c'è nulla da scegliere → si torna al respawn automatico
  allo scadere del timer, per non aggiungere attrito a chi gioca con respawn brevi. Così `respawnDelay`
  può restare basso per le AI senza mai penalizzare la scelta del giocatore quando conta.

## 55. Gli HP impostati nelle regole partita valevano solo al primo spawn, non al respawn (MEDIUM) — RISOLTO 2026-07-18
- **Segnalato dall'utente**: impostando HP ≠ 100 nelle regole, il valore valeva solo alla prima
  comparsa; morendo e rinascendo la vita tornava a 100.
- **Causa**: `initWorld` risovrascriveva `currentSettings.playerHp = pd->hp` (PlayerDef) DOPO che il
  mode aveva già creato l'entità iniziale col valore dello slider → primo spawn = slider, ma HUD e
  respawn = PlayerDef. Due fonti di verità per gli HP del giocatore.
- **Fix**: `PlayerDef.hp` ora **semina** lo slider "HP giocatore" del PreMatch UNA volta all'avvio;
  da lì lo slider (`currentSettings.playerHp`) è l'**autorità unica**, usata identica per spawn
  iniziale e respawn. Rimosso il clobber in `initWorld` (le altre stat del personaggio — velocità,
  salto, sprint, armatura — restano applicate). `PlayerDef.hp` resta significativo (è il default).

## 54. Bersaglio strategico = "cubo volante" con hitbox sfasata (MEDIUM) — RISOLTO 2026-07-18
- **Segnalato dal playtest**: il bersaglio appariva come un cubo che fluttuava nella metà nemica, e
  non si riusciva a distruggerlo (né compariva un messaggio).
- **Causa doppia**: (1) `meshOffsetY = 2.0` sul box di fallback → il cubo galleggiava 2 m sopra il
  suolo; (2) l'hitbox sintetico `__strategic_target` era un box enorme con offset y=+2 → sommato al
  meshOffsetY finiva a +4, **completamente scollegato dal cubo visibile** → i colpi non lo toccavano
  (quindi niente distruzione → niente messaggio, che invece esisteva già).
- **Fix**: hitbox sintetico = cubo unitario (offset 0, extents 0.5) che scala con l'entità e usa lo
  STESSO `meshOffsetY` del rendering → **visibile == colpibile**. Grounding corretto: box 2.5 m con
  la base appoggiata al suolo (`meshOffsetY = 0.5·scala`), mesh reale senza offset.
- Lezione: quando una hitbox è sintetizzata separatamente dal mesh, DEVE condividere scala e
  meshOffsetY con il rendering — sono la stessa formula (`testHit` vs `toModelMatrix`), non due.

## 53. Bersagli strategici (DestroyTarget) non autorabili dal MapEditor (MEDIUM) — RISOLTO 2026-07-18
- **FATTO**: il MapEditor ora autora i bersagli strategici — lista "Bersagli strategici" con
  aggiungi/rimuovi, gizmo Sposta (range selezione -500), pannello proprietà (label, X/Z, HP), box
  arancione nel viewport, load/save di `strategic_targets[]` via RMW. Mirroring dei command post.
  DestroyTarget è ora completo anche lato authoring (regola 10_ProjectMemory).
- DestroyTarget è runtime-completo (schema, spawn, obiettivo, conseguenza, gate) ma i bersagli
  strategici (`MapDef.strategicTargets[]`) si aggiungono/spostano **solo a mano nel JSON della mappa**.
- Viola la regola "un tipo nuovo non è finito finché non si autora dall'editor" (10_ProjectMemory):
  finché il MapEditor non li piazza col gizmo (come command post / cover / danger zone), la feature
  non è "finita" per l'utente.
- **Da fare**: aggiungere i bersagli strategici al MapEditor — lista + gizmo Sposta + campi (label,
  hp, mesh, colore) + save/load, mirroring dei command post. È l'immediato follow-up di DestroyTarget.

## 52. Il binding mouse/rotella non mostrava il nome nelle opzioni (LOW) — RISOLTO 2026-07-17
- **Segnalato dal playtest**: assegnando un input del mouse nel keybinding, la riga non mostrava
  nessun nome (appariva "—").
- **Causa**: `OptionsMenu::renderControls` leggeva `input.getScancode()` + `SDL_GetScancodeName()`.
  Un binding non-tastiera (pulsante mouse / rotella) ha `getScancode() == UNKNOWN` → nome vuoto.
- **Fix**: usa `input.getKeyName()`, che descrive OGNI tipo ("Mouse Centrale", "Rotella su",
  "Mouse 4/5"). Aggiornati anche i prompt del rebind ("tasto / mouse / rotella").
- Nota: i **tasti laterali** (X1/X2) erano già gestiti (SDL li dà come pulsanti 4/5); il nome ora
  appare anche per loro.

## 50. Le unità a terra si muovevano ancora sotto ordine (MEDIUM) — RISOLTO 2026-07-17
- **Segnalato dal playtest**: con due compagni a terra, un ordine di movimento li faceva spostare
  tutti — anche i caduti.
- **Causa**: `SquadSystem` salta i caduti nell'assegnazione ordini (non ricevono il MoveTo), e
  `AiSystem` li salta (niente decisione). Ma l'**agente crowd conserva l'ultimo target** e il
  `CrowdSystem` lo muove comunque: skippare l'AI non basta, va **fermato attivamente**.
- **Fix**: per un'unità a terra, `AiSystem` ora chiama `requestMoveVelocity(agent, {0,0,0})` ogni
  tick prima di saltarla → l'agente si inchioda dov'è caduto finché non è rianimato o distrutto.

## 51. I keybinding si perdevano alla chiusura del gioco (MEDIUM) — RISOLTO 2026-07-17
- **Segnalato dal playtest**: rimappare un tasto nelle opzioni e poi chiudere/riaprire → modifica
  persa.
- **Causa**: `InputManager` impostava i default nel costruttore e `rebind()` cambiava solo la mappa
  in memoria. **Nessuna persistenza**: ogni avvio ripartiva dai default.
- **Fix**: `loadBindings()`/`saveBindings()` in `InputManager`. `load` al costruttore (i salvati
  vincono sui default), `save` a ogni `rebind()` — come un preset. File in
  `<exe>/user_presets/keybindings.json` (fuori da `data/`, sopravvive alle rebuild — stessa
  convenzione dei preset partita, KI #19). Serializzato per **nome azione**: robusto al riordino
  dell'enum `Action`. **Smoke dovuto**: cambia un tasto, riavvia, verifica che resti.

## 49. La posa/scala dell'arma in mano è per-entità ma tarata su un'arma fissa (MEDIUM) — RISOLTO 2026-07-17
**Aggiornamento: MIGRAZIONE FATTA.** La posa in mano ora vive sull'`WeaponDef` (`hand_scale`,
`hand_rot`, `hand_offset`), non più sull'entità. Vale per chiunque impugni l'arma → una classe che
cambia arma mostra sempre la scala giusta.
- **Schema**: `WeaponDef` + loader (`hand_scale`/`hand_rot`/`hand_offset`); `handScale<=0` = non
  autorata → fallback al `weapon_display` legacy dell'entità (transizione documentata).
- **Risoluzione** (`WeaponAttach` + anteprima `EntityEditor`): l'arma VINCE se `handScale>0`; la
  MANO (attach point) resta del personaggio. Una sola formula, come sempre.
- **Authoring**: Weapon Editor → tab Mesh → "Posa in mano". L'EntityEditor mostra la posa in SOLA
  LETTURA quando è dell'arma (evita la trappola KI #25 di editare un campo ignorato); resta
  editabile solo la mano e, in modalità legacy, la posa vecchia.
- **Migrazione dati (RMW-chirurgica, solo aggiunte)**: le 4 armi impugnate hanno preso la posa dai
  rispettivi `weapon_display` — DC-15A 0.4, E-5 1.2, Z-6 **80**, E-5C **0.0015** (range nativo di
  53000×: la prova che la scala è una proprietà dell'ARMA). Visivamente identico a prima; corretto
  ora anche riassegnando le classi.
- **Gate aggiornato**: avvisa se l'arma effettiva di un'unità non ha `hand_scale` autorato (userebbe
  il fallback legacy). Sui dati attuali: 0 warning.
- **APERTO (rifinitura, non bug)**: se in futuro la STESSA arma servirà a personaggi con mani molto
  diverse (clone vs droide) e servirà una posa per-personaggio, si aggiungerà un override opzionale
  sull'entità — additivo, non blocca nulla oggi.

### Storia (diagnosi originale)
- **È il "sistema scala che si è rotto" segnalato dall'utente.** L'arma in mano dell'Heavy Clone
  Trooper appariva enorme *"nonostante la scala l'avessi già sistemata"*.
- **Diagnosi (numeri reali).** La scala compensa la dimensione nativa del mesh dell'arma:
  Z-6 richiede `weapon_display.scale = 80`, DC-15A ne richiede `0.4` — **200× di differenza**.
  Da KI #43 il modello impugnato è l'arma **effettiva** (classe → loadout, via `WeaponAttach`),
  ma la posa/scala resta in `weapon_display` sull'**entità**, tarata su `weapon_display.id`. Quando
  l'Heavy aveva `class: trooper` (→ DC-15A) col display ancora sullo Z-6 (scala 80), il DC-15A veniva
  reso a scala 80 → **200× troppo grande**. L'utente ha aggirato creando la classe `Heavy Trooper`
  (Z-6), che riallinea display ed effettiva.
- **La matematica NON è il problema**: la formula `effScale = weapon_display.scale / mesh_scale` è
  **identica** tra anteprima editor (`EntityEditor::updateWeaponTransform`) e runtime
  (`WeaponAttach::resolve`) — quindi "ciò che vedi nell'editor è ciò che vedi in gioco" **regge**.
  Il buco è che il dato giusto è sull'oggetto sbagliato.
- **Fatto ora (guard)**: il gate (ADR-018) avvisa quando `weapon_display.id ≠ arma effettiva`
  (risolta con `classres`): il "scala per l'arma sbagliata" non è più silenzioso. Sui dati attuali
  dell'utente: **0 mismatch** (tutto riallineato a mano).
- **APERTO — decisione di design (non decisa scrivendo codice, GDD 21.4).** La scala/posa in mano è
  una proprietà del **(mesh dell'arma)**, non dell'entità: la stessa arma sta in mano uguale a
  chiunque la impugni. Andrebbe **spostata su `WeaponDef`** (scala/rot/offset/grip in mano), con
  l'entità che fornisce solo l'attach point della mano. Così qualunque arma una classe assegni si
  vede corretta, senza ri-tarare ogni entità. È una modifica di schema (WeaponDef + WeaponEditor per
  autorarla + WeaponAttach + anteprima EntityEditor + migrazione dati): da proporre come scope prima
  di implementarla. Finché non è fatta, tenere `weapon_display.id` allineato all'arma della classe.

## 46. Mouse catturato (Tab) restava bloccato lasciando un modulo con viewport (HIGH) — RISOLTO 2026-07-17
- **Segnalato dall'utente, mai registrato prima.** Tab nel viewport cattura il mouse
  (`SDL_SetRelativeMouseMode(TRUE)`, cursore nascosto). Uscendo dal modulo **senza** premere Tab di
  nuovo, il cursore restava invisibile e **non liberabile** se non riaprendo un modulo con viewport.
- **Causa strutturale**: `SDL_SetRelativeMouseMode` è uno stato **globale** SDL, ma l'unico codice
  che lo spegne è il **Tab dentro `FreeCameraViewport::tick()`** — e il tick di un modulo smette di
  girare appena non è più attivo (i moduli persistono, non vengono distrutti: `EditorApp::tick`
  chiama solo il tick del modulo attivo). Quindi la cattura restava accesa senza nessuno a spegnerla.
  Lo stesso `if (!m_focused) return` all'inizio del tick rendeva il Tab inefficace anche perdendo
  solo il focus.
- **Fix, imposto dove si SA del cambio**: `FreeCameraViewport::releaseMouseCapture()` (idempotente,
  chiamata anche dal distruttore per la chiusura app); ogni modulo con viewport la inoltra;
  `EditorApp::tick` tiene `m_prevActive` e, **al cambio modulo**, rilascia la cattura su tutti i
  viewport. L'invariante ("solo il modulo attivo può tenere il mouse") vive in EditorApp, che è
  l'unico a sapere quando il modulo cambia — il tick per-modulo da solo non può mantenerlo.
- **Bonus**: Esc→Home passa dal cambio modulo, quindi **Esc diventa un'uscita d'emergenza** dal mouse.
- Build-verified + editor avviato senza crash (5 viewport inizializzati). **Smoke manuale dovuto**:
  Tab per catturare → cambia modulo dal menu → il cursore deve riapparire.

## 47. ClassEditor nascondeva le armi separatiste: impossibile armare le classi nemiche (MEDIUM) — RISOLTO 2026-07-17
- **Segnalato dall'utente** creando le classi per B1 Battle Droid / B1 Heavy Droid: il dropdown armi
  mostrava **solo** le repubblicane. Filtro `if (w.faction != Faction::Separatist)` in due punti
  (dropdown + default della nuova classe).
- **Residuo di un assunto superato**: la classe era nata "player/ally-facing" (doc 14 vecchio), ma
  ADR-022 la rende valida **anche per gli NPC nemici** — una classe per un B1 **deve** poter usare
  l'E-5. Il filtro impediva authoring legittimo.
- **Fix**: rimosso il filtro; il dropdown mostra **tutte** le armi con la **fazione in etichetta**
  (`DC-15A [republic]`, `E5 Blaster Rifle [separatist]`) — la fazione resta visibile senza togliere
  scelte (principio dell'utente: "più cose posso modificare dall'editor, meglio è"). Il default di
  una nuova classe è la prima arma disponibile, di qualunque fazione.

## 48. Viewport: FBO invalidato a dimensione stabile non veniva mai ricostruito (MEDIUM) — RISOLTO 2026-07-17
- **Probabile causa del "viewport dava problemi, poi si sistemava chiudendo e riaprendo".** Trovato
  rivedendo il codice su segnalazione vaga dell'utente (non riproducibile a comando).
- `resizeFBO` usciva subito se la dimensione richiesta == quella corrente — **anche con l'FBO rotto**
  (`m_fboOk == false`). Un realloc fallito (intoppo del driver Intel — ADR-003, primi frame, o
  minimize/restore che perde le risorse GL) lasciava `m_fboOk=false`; al frame dopo, pannello di
  dimensione stabile → uscita immediata → **viewport rotto fino a un resize o al riavvio**.
- **Fix**: la guardia di uscita ora richiede anche `m_fboOk` — se l'FBO è invalido si **ritenta la
  ricostruzione** al frame successivo e si auto-ripara, senza restart. Nel caso normale (`m_fboOk`
  true) la guardia esce come prima: nessun churn. Il log del realloc distingue crescita vera
  (KI #17) da ricostruzione a parità di dimensione, per non spammare né simulare un leak.
- **Non riprodotto** (è transitorio e driver-dipendente): il meccanismo è dimostrato per lettura del
  codice, non da un crash catturato. Da confermare sul campo se il sintomo non si ripresenta.

## 43. Con una classe assegnata, l'unità impugnava un'arma e ne sparava un'altra (HIGH) — RISOLTO 2026-07-17
- **Il bug che avrebbe fatto dire "le classi non funzionano" appena assegnate.** ADR-022 applicava
  la classe **solo dentro** `ConquestMode::resolveUnitArchetype` (`out.weaponId`), ma due
  consumatori a valle continuavano a leggere il campo **grezzo** dell'`EnemyDef`:
  - `ConquestMode.cpp` — **bullet stats** (danno, velocità, lifetime, colore) da
    `enemy->primaryWeaponId()`;
  - `WeaponAttach.hpp` — **modello in mano** da `def->primaryWeaponId()`.
  Risultato su un'unità con classe: **spara** l'arma della classe, **impugna** quella dell'entità,
  **coi danni** di quest'ultima. Tre risposte diverse alla stessa domanda.
- **Stessa famiglia del bug 2026-07-11** (`weapon_display.id` che vinceva sul loadout), un livello
  più in alto — e il commento che documentava quella correzione era proprio sulla riga incriminata.
- **Fix strutturale, non locale**: nuova `mini::classres` (`include/mini/game/ClassResolve.hpp`) —
  **unica** implementazione di "la classe vince se valorizzata". La usano runtime (ConquestMode),
  render dell'arma in mano (WeaponAttach) **e l'editor** (anteprima). La cura non è ricordarsi di
  applicare la regola ovunque: è che la domanda abbia una sola implementazione (ADR-018).
- **VERIFICATO SUI DATI REALI dell'utente**, non per lettura del codice. `Heavy Clone Trooper` ha
  `weapons[0]="Z-6 Rotary Blaster"` (danno 15) ma `class: trooper` → `DC-15A` (danno 20). Aggiunto
  l'evento telemetrico `unit class resolved` (ADR-016) — `std::cout` è bufferizzato e un run
  headless ucciso lo perde, quindi non era osservabile.
  - col fix: `weapon: DC-15A, damage: 20.0` → **coerenti**;
  - **col bug reintrodotto apposta**: `weapon: DC-15A, damage: 15.0` → **incoerenti**, la firma
    esatta del guasto. Il test è stato **visto fallire** prima di essere creduto.
- Nota: `out.weaponId` era già class-aware **prima** del fix, quindi il solo campo `weapon` non
  provava nulla — è `damage` a rendere il bug visibile. Un evento che mostra un solo lato di
  un'incoerenza non la può rivelare.

## 44. Entity Editor: campi editabili che il gioco ignorava (MEDIUM) — RISOLTO 2026-07-17
- Richiesta dell'utente ("togliere dall'Entity Editor tutto ciò che non decide l'entità"), ma
  l'audit ha trovato più di quanto la richiesta implicasse. Rimossi dal tab Statistiche:
  - **Armi** — le decide la CLASSE (ADR-022). Era anche una **lista bugiarda**: il runtime legge
    solo `weapons[0]` (`EnemyDef::primaryWeaponId()`), quindi "+ Arma" prometteva un arsenale che
    il gioco ignora.
  - **Abilità**, **AI Profile** — le decide la CLASSE.
  - **Velocità** — **morta**: `resolveUnitArchetype` la sovrascrive **sempre** con
    `ai->patrolSpeed`. Si edita nel profilo AI. (Nei dati dell'utente `Heavy Clone Trooper` aveva
    `move_speed: 4.0` e `Clone Trooper` `1.8`: due numeri che non hanno mai fatto nulla.)
  - **Danno Scale** — **morta**: ZERO consumatori nel runtime (classe KI #25).
- **`weapon_display` nel tab Visuale era il caso peggiore**: il combo "Arma primaria" scriveva
  `weapons[0]`, cioè era **l'editor del loadout travestito da posa**. Ora è in **sola lettura** e
  mostra l'arma risolta con `classres` — la stessa funzione del runtime. La **posa**
  (mano/rotazione/offset/scala) resta dell'entità: quella sì che dipende dal corpo.
- **Nessun dato toccato**: `saveSelected` ha smesso di **rivendicare** i campi che non edita più —
  RMW (ADR-010) preserva quelli già nel file. Trappola evitata: `m_weaponId` ora è l'arma
  **risolta**, quindi continuare a scriverla in `weapon_display.id`/`dispWeaponId` avrebbe copiato
  l'arma della CLASSE dentro l'ENTITÀ al primo salvataggio — ricreando nei dati la confusione che
  ADR-022 elimina.
- **Gate**: nuovo warning "nessuna CLASSE assegnata" — un'unità classless ha un loadout che il gioco
  usa e che l'editor non mostra più: va detto, o diventa invisibile.

## 45. Risoluzione di `data/` duplicata in 8 punti e GIÀ divergente (MEDIUM) — RISOLTO 2026-07-17 (06_Todo R8)
- Il debito era registrato come teorico. **Non lo era**: delle otto copie, **quattro** verificavano
  `data/weapons` prima di accettare il percorso e **quattro** si accontentavano che una cartella di
  nome `data` esistesse (EntityEditor, MapEditor, VehicleEditor, WeaponEditor). Basta che
  `exeDir/../../../data` risolva a una directory qualunque perché metà dei moduli scriva in una
  `data/` e l'altra metà in un'altra — un editor che salva dove il gioco non legge.
- Il pannello di **Validazione** era il punto più grave: avrebbe validato una `data/` diversa da
  quella caricata, cioè avrebbe **mentito** (il commento nel codice lo aveva previsto).
- **Fix**: `editor/util/DataPath` — una sola risoluzione, che adotta il controllo **forte**.

## 40. Il gate dei campi fantasma era cieco su hitboxes/maps/vehicles (MEDIUM) — RISOLTO 2026-07-16
- **La regola c'era, il file non la vedeva mai.** `validateContent` avvisa da tempo sul campo
  `profile_id`/`id` (ADR-001) e sui refusi — ma il rilevatore lavora su `reg.unknownKeys()`, che
  viene popolato da `noteUnknownKeys()`, **chiamato da 9 loader su 12**: mancava in
  `loadHitboxProfiles`, `loadMaps`, `loadVehicles`.
- **Conseguenza reale**: `data/hitboxes/B1 Heavy Droid.json` conteneva `"profile_id"` — violazione
  ADR-001 in piena vista — e il gate riportava **0 warning**. Il campo è stato rimosso (con `sed`,
  non riscrivendo il JSON: riformattare i float è il danno che RMW previene).
- **Lezione, la stessa di sempre**: *un detector mai visto scattare non è verificato*. La regola
  esisteva dal giorno 1 e non aveva mai potuto fallire.
- **Verificato con una sonda**: iniettati `profile_id`, un refuso top-level e un refuso dentro una
  zona → il gate li segnala tutti e tre, quello di zona col nome della zona. File ripristinato
  byte-identico via `git checkout`. La sonda ha anche smascherato un **mio** falso negativo: la
  prima l'avevo messa nella `data/` di *output*, ma **entrambi i binari preferiscono la `data/`
  sorgente** (`exeDir/../../../data`, `Application.cpp:91` e `EditorApp.cpp:301`) — non veniva
  nemmeno letta.
- **LIMITE APERTO**: su `maps` sono coperte solo le chiavi di **primo livello**. Le sotto-strutture
  (`geometry`, `command_posts`, `cover_points`, `patrol_routes`, `danger_zones`, `vehicle_spawns`)
  hanno ognuna il proprio set: un refuso **dentro** un box di geometry passa tuttora liscio. Le
  liste di chiavi note vanno **lette dal loader, mai dedotte con grep**: derivandole a grep avevo
  mancato `color` (veicoli) e `enemy_types`/`ally_types` (mappe) — e una chiave mancante dalla
  lista fa dire al gate di cancellare un campo funzionante, cioè è peggio del silenzio.

## 41. Profilo hitbox vuoto → colpito come una sfera, in silenzio (MEDIUM) — GATE FATTO, CONTENUTO APERTO
- `zones: []` non è fatale: `testHit()` (`CombatSystem.cpp:57`) cade sul **fallback sferico**
  (`k_hitRadius`, moltiplicatore 1.0). L'unità resta colpibile, ma **senza zone, senza headshot e
  senza colpi di striscio** — si comporta diversamente da ciò che il profilo promette, e nessuno
  lo dice. Era invisibile senza leggere il codice del combat.
- **Fatto**: il gate ora avvisa sul profilo vuoto, e va in Error su `half_extents <= 0` (zona di
  volume nullo, mai colpibile) e in Warn su `damage_multiplier <= 0` (zona che non fa danno).
- **~~APERTO~~ → CHIUSO 2026-07-19**: `hitboxes/B1 Heavy Droid.json` e `hitboxes/Heavy Clone Trooper.json`
  erano entrambi profili VUOTI e **orfani** — l'utente ha assegnato agli Heavy i profili con zone
  (`B1 Heavy Battle Droid` → `hitbox_profile: "B1 Battle Droid"`; `Heavy Clone Trooper` →
  `"Clone Trooper"`), quindi in gioco si colpiscono correttamente. Verificato che nulla li referenziava
  ed **eliminati** → `--validate` ora **0/0** (era 0/2). Il gate resta a segnalare eventuali FUTURI
  profili vuoti (comportamento voluto).

## 42. `MapDef.navmeshPath`: campo scritto e mai letto (LOW) — RISOLTO 2026-07-16
- Anello morto completo: `BalanceEditor` **scriveva** `j["navmesh"]`, **nessun loader lo rileggeva**,
  e `navmeshPath` non lo consumava nessun sistema — quindi valeva `""` per sempre e si ri-salvava `""`.
- **Contrario ad ADR-004**: la navmesh la *genera Recast a runtime* dai box di `MapDef.geometry`;
  non esiste una navmesh caricata da file. Era residuo di un'idea anteriore all'ADR.
- **Rimosso in tutti e tre i punti**: campo in `Definitions.hpp`, scrittura in `BalanceEditor.cpp`,
  e la chiave `"navmesh": ""` in `data/maps/firebase.json` — quest'ultima necessaria, perché RMW
  (giustamente) preserva le chiavi che non gestisce: lasciandola, il gate avrebbe avvisato in eterno.
- **Trovato dal gate di KI #40 appena riparato**, non a occhio: è la prova che serviva.

## 39. La morte del giocatore bruciava i rinforzi della squadra (HIGH) — RISOLTO 2026-07-16
- **Disallineamento design↔codice**, emerso da un chiarimento dell'utente: i **ticket sono la
  riserva di rinforzi della squadra** (il campo ha un cap di AI; il resto entra man mano che le
  unità cadono), **non le vite del giocatore**.
- **Com'era**: ogni morte del giocatore consumava un ticket (3 punti in Application, logica
  duplicata), e a ticket 0 la morte era **sconfitta secca anche con la squadra intatta**.
- **Com'è**: regola in **un solo posto** (`onPlayerDeath`) — il giocatore non consuma rinforzi;
  si perde **solo** cadendo quando non resta né un alleato vivo né un rinforzo in arrivo. Vale
  anche per il respawn volontario (K), che è una morte come le altre.
- **`IGameMode::consumeTeam1Ticket()` rimosso**: era rimasto codice morto e la sua sola esistenza
  invitava a rifare l'errore. Senza il metodo la regola è **strutturale**, non una convenzione.
- Il meccanismo dei rinforzi era già corretto e **non è stato toccato**:
  `ConquestMode::checkDeaths` (unità cade → ticket → rimpiazzo dalla riserva; a 0 → morte
  permanente).
- **Verificato**: morte del giocatore con 5 rinforzi → restano 5; alleati 0 + rinforzi 2 →
  respawn; alleati 0 + rinforzi 0 → *"SCONFITTA: squadra annientata e nessun rinforzo"*.

## 37. Framework obiettivi isolato: esito morto, invisibile, mappa scollegata (HIGH) — RISOLTO 2026-07-16
- **Trovato per analisi guidata dal GDD** (21.2 "evitare i sistemi isolati"), non da un test.
  Il sistema obiettivi (ADR-019, Phase A) era completo e verificato headless, ma per il giocatore
  **non esisteva**:
  1. `ObjectiveSystem::outcome()` non era chiamato da nessuno → **codice morto**: completare una
     missione non faceva nulla. (Stesso difetto del ramo FocusFire, KI risolto il 07-15.)
  2. **Nessun HUD**: gli obiettivi non erano visibili in partita.
  3. `MissionDef.mapId` ignorato: la missione non imponeva la propria mappa → obiettivi a
     coordinate arbitrarie su un'altra mappa.
- **Fix**: puntatore non-proprietario al sistema in Application (i sistemi sopravvivono a
  `World::initialize()`); esito missione → esito partita **con precedenza al mode** (doc 25);
  pannello OBIETTIVI dallo stato reale; `mapId` della missione vince con avviso esplicito.
- **Verificato**: fallimento a tempo → "SCONFITTA (obiettivo perso)"; primario completato →
  "VITTORIA"; `--map` contraddittorio segnalato; non-regressione senza missione.

## 38. Missione congelata al riavvio della partita (MEDIUM) — RISOLTO 2026-07-16
- I sistemi **sopravvivono** a `World::initialize()` (che azzera solo entità e mailbox).
  `ObjectiveSystem` ri-bindava solo se cambiava il *puntatore* alla missione: al riavvio della
  stessa missione nessun rebind → obiettivi ancora "completati", e con `m_outcome != Ongoing`
  l'update usciva subito **per sempre**. Riavviare una missione completata non la ricominciava.
- **Fix**: rebind anche quando `getTickCount()` torna indietro — è il segnale di restart
  (`initialize()` azzera `m_tickCount`). **Vincolo generale**: un sistema ECS che tiene stato
  fra i tick deve gestire il restart del mondo, perché non viene distrutto con le entità.

## 36. Classe e personaggio azzerati all'avvio partita (HIGH) — RISOLTO 2026-07-16
- **Sintomo (segnalato dall'utente):** `--class marksman` non cambiava l'arma. Non era il design:
  era un bug.
- **Causa:** all'ENTER del PreMatch, `currentSettings = preMatchMenu.getSettings()` **sovrascriveva
  la struct intera**, azzerando `classId` e `characterId` — campi che il PreMatch non possiede
  (non ha selettori) e che erano stati risolti all'avvio. `startGame()` partiva coi campi vuoti.
- **Classe del bug:** identica a quella della regola READ-MODIFY-WRITE (ADR-010) — costruire un
  oggetto nuovo e sovrascrivere invece di modificare solo i propri campi. Lì su file, qui in
  memoria: **la disciplina RMW è documentata per i save JSON, ma il pattern è più generale.**
- **Conseguenza nascosta:** azzerava anche `characterId` → **nemmeno KI #35 funzionava in partita**.
  La verifica era stata fatta in **sandbox**, che non passa da quella riga, e generalizzata al
  percorso reale. Il "feeling identico" confermato dall'utente era corretto per il motivo
  sbagliato: in partita il personaggio non veniva applicato e valevano i default del codice.
- **Fix:** `startFromPreMatch()` come punto unico; se il PreMatch ha un valore (es. da un preset,
  che serializza `"class"`) vince lui, altrimenti si tiene quello risolto all'avvio.
- **Lezione di metodo:** la prima sonda di test **replicava** la logica del fix invece di
  eseguirla — sarebbe passata anche col gioco rotto. Un test che non passa per il codice di
  produzione non prova niente. Riscritta per chiamare la stessa funzione del tasto ENTER.
- **Verificato sul percorso reale:** `--class marksman` → `primary: DC-15X`; `--class trooper` →
  `DC-15A`; `character equipped` presente in entrambi.

## 35. `PlayerDef` è autorato ma NESSUN sistema lo legge (HIGH) — RISOLTO 2026-07-15
- **Decisione presa: opzione (a) — renderlo vivo, non cancellarlo.** Il fatto decisivo: i valori
  autorati **coincidevano già** con quelli che il gioco usava, quindi consumarli è a variazione
  **zero** — cambia qualcosa solo quando l'utente modifica i dati, che è lo scopo del pannello.
  Cancellare (opzione b) avrebbe distrutto contenuto che la Fase 3 (personaggi/progressione,
  doc 27) dovrà comunque ricreare.
- **Fix:** `MatchSettings.characterId` → risolto in **`initWorld`** (non in `startGame`: vale per
  partita *e* sandbox — il giocatore non può comportarsi diversamente a seconda di come è
  entrato). `PlayerController` guadagna `moveSpeed`/`jumpMult`/`sprintMult`/`armorRating`, con
  **default identici alle vecchie costanti** → senza personaggio il comportamento è invariato per
  costruzione. `HealthComponent.armor` (generico, 1 = nessuna riduzione) applicato in CombatSystem.
- **Selezione**: con **un solo** personaggio autorato non c'è nulla da scegliere → è il giocatore,
  e il pannello diventa vivo senza UI. Con più personaggi la scelta è ambigua: **non si indovina**
  (sceglierne uno a caso sarebbe il fallback hardcoded di ADR-007) — si logga che serve il
  selettore nel PreMatch (14_ClassSystem Phase B).
- **Trappola evitata**: i dati dicevano `sprint_mult: 1.5` ma il gioco girava con la costante
  hardcoded `SPRINT_MULT = 1.65f` (in `PlayerController.cpp`, contro CLAUDE.md). Applicare i dati
  alla cieca avrebbe cambiato il feel dello sprint. **La verità è il comportamento**: il dato è
  stato allineato a 1.65, poi reso autoritativo; la costante è stata rimossa.
- **Verificato**: `character equipped` in sandbox con hp 100 / move_speed 5.0 / sprint_mult 1.65 /
  armor 1.0 = esattamente i valori storici. Gate ADR-018 esteso ai personaggi (hp/move_speed/
  armor <= 0 → Error; sprint_mult < 1 → Warn).

### Storico (diagnosi 2026-07-15)
- **Verificato**: nessuna riga in `src/` o `include/` referenzia `PlayerDef` fuori da
  `Definitions.hpp`/`DefinitionRegistry.cpp`. L'unico consumatore è il **BalanceEditor**, che lo
  scrive. Il tipo si carica da `data/characters/<id>.json` (oggi: `clone_trooper.json`).
- **Conseguenza per l'utente**: ogni stat regolata in quel pannello — `hp`, `move_speed`,
  `jump_height`, `sprint_mult`, `armor_rating` — **non ha alcun effetto in partita**. L'hp del
  giocatore viene da `MatchSettings.playerHp` (impostato nel PreMatch); velocità, salto e sprint
  vivono in `core/GameConfig.hpp`/`PlayerController`.
- È la classe di bug di KI #25 alla massima scala: non un campo fantasma, un **tipo intero**
  fantasma. Il gate ADR-018 non può vederlo: i dati sono validi, è il *codice* che non li legge
  (vedi il limite documentato in 24).
- **Ha già deviato il design di un altro sistema**: 14_ClassSystem prescriveva
  `PlayerDef.classId`; attaccarcelo avrebbe prodotto una funzionalità senza effetto. La classe è
  stata messa su `MatchSettings` (che il gioco legge davvero) — vedi 07_Changelog 2026-07-15.
- **Decisione da prendere** (non presa qui): (a) far consumare `PlayerDef` al runtime — il
  PreMatch sceglie un personaggio e le sue stat si applicano davvero; oppure (b) eliminare il
  tipo e il pannello, e tenere le stat dove sono. La (a) è coerente col GDD (personaggi/classi);
  la (b) è onesta se il concetto non serve. Nel frattempo il pannello del BalanceEditor andrebbe
  marcato "(non attivo)" come da convenzione KI #25.

## 33. La traversata col crowd non ha mai avuto pathfinding (HIGH) — RISOLTO 2026-07-15
- `AiSystem` passava a `requestMoveTarget` il punto `et + moveDX/moveDZ`, ma `norm2D()`
  **normalizza `moveDX/DZ` in place**: Detour riceveva sempre una "carota" a **1 metro**
  dall'agente, mai la destinazione reale. Un path a 1 m non può aggirare un ostacolo → l'agente
  pianificava dentro il muro e ci spingeva contro. Presente da ADR-017 Phase B.
- **Il commento nel codice affermava il contrario** ("i rami traversal impostano moveDX/DZ =
  destinazione − posizione"): descriveva un'intenzione mai implementata. Hunt/Search/Patrol
  chiamano tutti `norm2D` e ne scartavano il valore di ritorno.
- **Fix:** variabile `moveDist` (distanza reale) valorizzata dai rami traversal; il crowd riceve
  `et + moveDir * moveDist`. Più flag `orderTravel`: un ordine di squadra che fa percorrere
  distanza usa `requestMoveTarget` **anche in Alert** (il ramo Alert usa `requestMoveVelocity`,
  che non pianifica ed è pensato solo per lo strafe a corto raggio).
- **Effetto misurato onesto:** sblocca la traversata a lungo raggio (dimostrato: squadra sotto
  ordine converge 8.0 → 1.3 m, ordini completati 0 → 1915). Sugli eventi `stuck` l'effetto è
  **modesto e non concludente** (`--stress 10`: 35 → 31, singolo run, entro il possibile rumore).
- Da verificare manualmente: in partita vera le AI dovrebbero aggirare gli ostacoli grandi invece
  di strisciarci contro. Possibile interazione con KI #31 (AI attraversano i veicoli), non misurata.

## 34. Il centro di firebase (0,0) è irraggiungibile dal pavimento (INFO) — NON UN BUG 2026-07-15
- La piattaforma **"Collina Centrale"** (10×10, `sy=1`, `collider: true`) è centrata esattamente in
  (0,0). Con `kAgentClimb = STEP_HEIGHT` un gradino di 1 m non è scalabile → sul navmesh il centro
  mappa **non è connesso** al pavimento circostante, e i 4 cover a ±6 lo recintano.
- Non è un difetto: è design della mappa. È annotato perché ha **invalidato un A/B di movimento**
  (un `MoveTo(0,0)` non può completarsi *per costruzione*, e gli agenti si accumulano su un anello
  a ~7.5 m sembrando "bloccati"). Chi testa movimento/ordini su firebase usi un punto libero —
  es. **(12,0)**, margine ≥2.5 m da ogni box — non il centro mappa.