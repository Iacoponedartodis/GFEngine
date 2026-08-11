# 53 — Strumenti di costruzione: portare il Map Editor a livello professionale

> Nasce da una richiesta esplicita dell'utente (2026-08-10): *"voglio che fai ricerche sul
> ProBuilder di Unity e programmi simili, per capire come migliorare in maniera significativa map
> editor ed editor strutture, migliorando anche proprio l'esperienza … possiamo creare mappe
> complesse anche direttamente dal map editor, sfruttando una geometria relativamente semplice per
> tenere la mappa leggera, inserendo poi le mesh fatte su blender … ma non con questo livello di
> professionalità del map editor"*.
>
> **Stato: Planned Feature.** Zero righe di codice scritte. Le fasi si implementano quando
> richieste, nello scope qui descritto (CLAUDE.md §4).

---

## 0. La conclusione della ricerca, in tre righe

1. **Non si deve copiare ProBuilder.** ProBuilder è editing di mesh (vertici, spigoli, facce,
   estrusione, bevel, UV). Applicarlo qui distruggerebbe le tre cose su cui poggia questo motore:
   navmesh diagnosticabile per box, semantica `BoxType`, e collisione AABB. Sarebbe il cambio più
   costoso e meno utile del catalogo.
2. **Si deve copiare CubeGrid** (Unreal 5, il sostituto ufficiale delle BSP brush) e le
   **interazioni** di TrenchBroom. Entrambi lavorano su geometria a scatole allineate alla griglia
   — esattamente il nostro mondo — e la differenza di velocità sta nel **gesto**, non nel modello
   dei dati.
3. **Il divario vero non è "mancano funzioni", è "ogni box costa troppi gesti".** Oggi creare un
   muro è: `+ Box` → nasce 2×2×2 → tre campi numerici o tre trascinate del gizmo di scala → una
   trascinata di posizione. Negli editor di riferimento è: **disegna un rettangolo sulla griglia,
   tira su la faccia**. Due gesti contro sei, moltiplicati per le centinaia di box di una mappa
   300 × 200.

---

## 1. Overview

Il Map Editor oggi sa fare le cose *giuste* (misure normative, verifica navmesh, composite,
metadata tattici) ma le fa con l'ergonomia di un pannello di proprietà, non di uno strumento di
costruzione. Questo documento raccoglie cosa fanno gli strumenti professionali, lo confronta con
il codice vivo, e propone un piano a fasi ordinato per **utilità nel costruire la mappa 300 × 200**.

## 2. Goal

Che costruire un edificio di venti box costi **un ordine di grandezza meno gesti** di oggi, senza
perdere niente di ciò che rende questo editor diverso da un editor generico: le metriche
normative, la verifica navmesh, e il fatto che ogni errore sia **detto** invece che scoperto.

## 3. Problem Solved

- **Costo per box.** Sei gesti invece di due. Su 300 × 200 è la differenza fra giorni e settimane.
- **Nessuna precisione senza contare a mente.** Non si può dire "spostalo di 4 m in X", né
  allineare due muri, né appoggiare un box su una faccia.
- **La selezione non regge la scala.** Nessun raggruppamento persistente, nessun isolamento,
  nessun modo di prendere l'oggetto dietro a un altro.
- **Il ciclo di prova è aperto.** La lezione più ripetuta della letteratura di level design è
  *"cammina, non volare"*: si valuta uno spazio solo attraversandolo alla velocità del giocatore.
  Oggi si lancia il gioco senza mappa e senza posizione.
- **Gli errori si scoprono a valle.** Il gate `--validate` esiste ma è headless: in editor non
  c'è un elenco vivo dei problemi con "portami lì".

---

## 4. Riferimenti professionali (regola dell'utente: guardare chi l'ha già risolto)

### 4.1 Unity ProBuilder — **da NON copiare, e vale la pena dire perché**
Modalità vertice / spigolo / faccia, estrusione, bevel, `Insert Edge Loop`, `Bridge Edges`,
`Merge Faces`, editor UV, export verso software esterni.

**Perché non calza.** Produce mesh arbitrarie. Da noi:
- il navmesh è verificato e **attribuito per box** ("quale box non produce superficie" — doc 48);
  con una mesh arbitraria quella diagnosi non esiste più;
- `BoxType` (`floor`/`wall`/`platform`/`cover`) è letto dal navmesh e dalla derivazione dei
  metadata tattici: una faccia di mesh non ha semantica;
- la collisione e le posizioni tattiche ragionano su scatole.

Il pezzo di ProBuilder che ci riguarda è un altro, ed è la sua **premessa**: la geometria di
blockout si costruisce **dentro** l'editor del gioco e si esporta/sostituisce dopo con le mesh
vere. È esattamente l'approccio già scelto qui (geometria semplice + mesh Blender sopra), e la
ricerca lo conferma come standard.

### 4.2 Unreal 5 — **CubeGrid: il modello da copiare**
Sostituto ufficiale delle BSP brush. Interazioni (dalla documentazione Epic):

| Gesto | Effetto |
|---|---|
| clic / clic+trascina / Shift+clic | seleziona una cella, più celle, un intervallo |
| **E / Q** | **tira fuori / spingi dentro** la selezione (push-pull) |
| Ctrl+trascina | estrude o incava in continuo |
| Ctrl+E / Ctrl+Q | griglia più grande / più piccola |
| Z | modalità angolo (manipola i singoli angoli prima di applicare) |
| R, Ctrl+clic centrale | riposiziona/riorienta la griglia di lavoro |

**La lezione**: la griglia non è uno sfondo, è **la superficie su cui si costruisce**. Si seleziona
una zona di griglia e la si tira su. È tutto qui, ed è tutta la differenza di velocità.

Altri pezzi di Unreal utili: **Measuring Tool** solo in ortografica (in prospettiva una lunghezza
sullo schermo non è una lunghezza nel mondo), **Edit Pivot Tool** (pivot numerico e "aggancia il
pivot a un punto della mesh"), pannello Transform con input numerico world/local.

### 4.3 TrenchBroom — **le interazioni, indipendenti dal motore**
- **Estrusione trascinando una faccia** (Shift+trascina lungo la normale); Ctrl+Shift la *divide*
  in due brush. Facce identiche e allineate si estrudono **insieme**.
- **Selezione a trapano**: la rotellina cicla fra gli oggetti sovrapposti sotto il cursore. In 2D
  sceglie quello con l'area visibile *più piccola*, non il più vicino — così l'oggetto piccolo
  dietro a quello grande è raggiungibile.
- **Restrizione d'asse**: Alt durante il trascinamento blocca sull'asse su cui ci si è mossi di
  più; Ctrl passa all'asse verticale. **Linee di traccia** visibili durante lo spostamento.
- **Sposta di un offset**: finestra con il vettore numerico.
- **Duplica-e-sposta in un gesto**: Shift+trascina, oppure Shift+frecce.
- **Griglia da 0,125 a 256**, cambiata con Ctrl+rotellina, **proiettata sulle facce** in 3D.
- **Gruppi (annidabili) e livelli**, con visibilità e **blocco**.
- **Issue browser**: elenco vivo dei problemi della mappa, con **correzioni rapide**.
- **Appunti testuali**: si copia geometria come testo e la si incolla, anche fra mappe.

### 4.4 Letteratura di level design (leveldesignbook.com)
- Griglia = **larghezza del giocatore** (Unity: 1–2 m). Muri alti **150–200% della figura di
  scala**. Incrementi **grandi** per stanze e muri, **piccoli** per la rifinitura.
- L'errore più frequente in assoluto è **la scala**, e il rimedio è **camminare nello spazio
  durante la costruzione**, non guardarlo dall'alto: *"do a simple self-playtest: walk around the
  space, within the game engine, with full player gravity, collision, and speed"*.
- **Espandere o comprimere** i muri esistenti invece di rifarli.

### 4.5 Snapping modulare (confronto UE5, 2026)
La griglia da sola è **allineamento di posizione**, non **consapevolezza di connessione**: regge
finché il kit è piccolo, e *"si rompe intorno ai 50 pezzi"* perché i pezzi ruotati derivano. Il
passo successivo sono i **punti di aggancio tipizzati** con regole e validazione.
**Raccomandazione esplicita della fonte**: partire dalla griglia, passare ai punti di aggancio
solo quando il kit supera i ~50 pezzi o serve la validazione. → per noi: **non ora**, ma è la
direzione naturale delle composite.

### 4.6 "A Simpler 3D Level Editor" (Game Developer)
Un editor su misura per un gioco tattico: **l'estrusione di superficie come metodo primario di
costruzione**, più veloce del piazzare oggetti; doppio clic per selezionare un'intera superficie;
selezione rettangolare su un piano. Avvertenza onesta degli autori: la semplicità **rimanda** la
complessità, non la elimina — la rifinitura resta lavoro manuale.

---

## 5. Dove siamo davvero (verificato sul codice, non sulla documentazione)

| Capacità | Stato | Dove |
|---|---|---|
| Griglia con aggancio | ✅ 5 valori, `m_gridSnap` (0,5 m) | `MapEditor.cpp:619` |
| Griglia infinita ancorata al mondo | ✅ passo 2 m, ±1000 m | `FreeCameraViewport.cpp` |
| Viste ortografiche Alto/Fronte/Lato + `F` inquadra | ✅ | `ViewMode` |
| Righello libero a due clic con aggancio | ✅ | `setRulerActive` |
| Ingombro mappa e ingombro selezione | ✅ | pannello destro |
| Gizmo sposta / ruota / scala | ✅ | `GizmoMode` |
| Selezione multipla (Ctrl), Ctrl+A | ✅ | `m_multiSel` |
| Duplica, Serie (array) | ✅ | `duplicateSelected`, `makeArray` |
| Filtri di vista per categoria | ✅ 10 interruttori | `m_show*` |
| Composite: riferimenti, esplodi, raggruppa, modifica d'istanza | ✅ | ADR-056 |
| Verifica navmesh sulla struttura isolata | ✅ | `checkStructType` |
| **Disegnare un box sulla griglia** | ❌ | — |
| **Estrudere trascinando una faccia** | ❌ | — |
| **Spostamento numerico (offset)** | ❌ | — |
| **Allinea / distribuisci** | ❌ | — |
| **Appoggia su faccia / snap a superficie** | ❌ | — |
| **Copia / incolla** | ❌ (solo Duplica) | — |
| **Gruppi e livelli persistenti** | ❌ (le composite coprono il *riuso*, non l'*organizzazione*) | — |
| **Isola / nascondi la selezione** | ❌ (i filtri sono per categoria) | — |
| **Selezione a trapano (rotellina)** | ❌ | — |
| **Restrizione d'asse durante il trascinamento** | ❌ | — |
| **Elenco vivo dei problemi in editor** | ❌ (`--validate` è headless) | `ContentValidation.cpp` |
| **Provare la mappa da dove si sta guardando** | ⚠ `launchGame()` esiste ma lancia `--direct-prematch` **senza mappa e senza posizione** | `EditorApp.cpp:189` |
| Gizmo del Map Editor migrato al framework F1 | ❌ deliberato (doc 52) | — |

**Costo di una modifica** (misurato, `--editor-selftest`): 0,7 ms a 169 posizioni, ~21–34 ms alla
stima di 1500 per la mappa grande. Non blocca, si sente. Il rimedio è già identificato (doc 51 §1).

---

## 6. Scope — le fasi, in ordine di utilità per costruire la mappa

L'ordine non è per difficoltà: è per **quanto tempo fa risparmiare sulla 300 × 200**.

### L1 — Costruire con la griglia: disegna e tira (il salto grosso) · ✅ **FATTA 2026-08-10** (changelog 189)
Il modello CubeGrid, adattato al nostro mondo di scatole.

- **Disegna un box**: nella vista dall'alto (o su una faccia esistente), trascinare definisce il
  rettangolo in pianta; il box nasce con l'altezza corrente e la semantica corrente. Un gesto.
- **Estrudi trascinando una faccia**: afferrare la faccia di un box selezionato e tirarla lungo la
  sua normale, con aggancio alla griglia. È il gesto primario di costruzione secondo tre fonti su
  quattro.
- **Push/pull da tastiera**: `E`/`Q` spingono la faccia attiva di un passo di griglia — costruzione
  senza mouse, ripetibile, misurabile.
- **Passo di griglia con Ctrl+rotellina**, e il passo corrente **scritto sempre a schermo**.
- **Griglia proiettata sulla faccia** su cui si sta lavorando: senza, in 3D non si sa a cosa ci si
  aggancia.

**Perché prima di tutto**: è l'unica fase che cambia l'ordine di grandezza del lavoro. Le altre
migliorano, questa trasforma.

**Rischio dichiarato**: tocca il gizmo del Map Editor, che doc 52 ha deliberatamente NON migrato al
framework condiviso perché non c'era modo di collaudarlo senza mouse. Va affrontato prima: **L1
richiede che l'editing del viewport del Map Editor passi da `ViewportEditing`**, altrimenti si
costruisce la funzione nuova sul pezzo non condiviso e la divergenza raddoppia.

> #### ⚠ Come è andata davvero: il prerequisito (b) NON è stato fatto, e perché (2026-08-10)
>
> Avevo dichiarato la migrazione del gizmo come prerequisito di L1. Implementando ho cambiato
> idea, e vale la pena scriverlo invece di lasciar credere che sia stata fatta.
>
> **La ragione dichiarata era giusta ma il rimedio era sproporzionato.** Il rischio vero era
> *"una terza via ad hoc per i gesti nuovi"*. Quel rischio si evita mettendo i gesti nuovi
> **dentro `FreeCameraViewport`**, dove già vivono gizmo e righello, con lo stesso protocollo
> `pop…Delta` e la stessa coalescenza di `pushUndo` — cosa che è stata fatta. Non richiedeva di
> riscrivere il percorso di interazione più usato dell'editor.
>
> **Il rischio della migrazione era invece concreto e mal collocato nel tempo**: un rifacimento
> non collaudabile senza mouse, subito prima di settimane di uso intensivo. È il profilo esatto
> che ha già prodotto le regressioni di doc 49 (Serie, cambio modulo, modale invisibile).
>
> **Resta un debito**, non è sparito: il Map Editor ha ancora un proprio percorso di selezione e
> gizmo. Va fatto in un momento in cui l'utente non stia costruendo, e con i controlli headless
> delle operazioni già in piedi (oggi ce ne sono 13 in più, che allora non c'erano).

**Cosa è stato consegnato** (changelog 189):
- **Passo di griglia con Ctrl+rotella**, sette passi da 0,10 a 8,0 m — *lo stesso elenco* del
  combo nella barra, perché due elenchi divergenti mostrerebbero valori irraggiungibili.
- **Disegna**: strumento modale, impronta tracciata sul piano di lavoro con misure visibili
  durante il gesto, altezza e quota del piano nella barra. Il clic appartiene allo strumento e
  non seleziona; un clic senza trascinamento non crea geometria degenere.
- **Modalità Faccia**: sei maniglie sull'ingombro della selezione, **la faccia opposta resta
  ferma**. Su più elementi agisce sull'insieme; su una struttura parametrica tira la *misura*
  della ricetta (stessa regola del gizmo di scala, non una seconda). Clamp a 5 cm.
- **E / Q**: push-pull della faccia attiva di un passo, solo quando non si sta volando (in volo
  restano salita e discesa, come sempre). La faccia attiva ha il contorno bianco.
- **Aggancio obbligatorio sul tiro**: si emette solo a multipli interi del passo. Il grezzo
  darebbe muri da 3,47 m e fessure sotto la soglia di erosione — difetti che non si vedono.

**Non fatto di L1**: la griglia proiettata sulla faccia su cui si lavora. Utile, ma è disegno e
non interazione; si può aggiungere dopo senza toccare nulla di quanto sopra.

### L2 — Precisione: numeri, allineamento, appoggio · ⚠ **PARZIALE 2026-08-10** (changelog 189)

**Fatto**: sposta di un offset numerico (con "Un passo" che riempie il passo corrente), **allinea
a filo** min/centro/max sui tre assi — calcolato sui **bordi** e non sui centri, perché due muri
di spessore diverso allineati al centro restano sfalsati ed è proprio quella la fessura che il
navmesh non attraversa — e **distribuisci** a spazio uguale tenendo fermi gli estremi.

**Fatto nel secondo giro** (changelog 190): **appoggia / accosta** su sei direzioni. La selezione
si muove come **un corpo unico** — spostare ogni elemento fino al proprio contatto smonterebbe la
forma di ciò che si è selezionato — e se davanti non c'è niente non si muove nulla, invece di
mandare la geometria verso il nulla.

**Non fatto, con motivazione**:
- **Restrizione d'asse (Alt) + linee di traccia.** Qui lo spostamento avviene afferrando una
  freccia del gizmo, che è **già** vincolata a un asse: la restrizione risolverebbe un problema
  che questo editor non ha. Ciò che manca davvero è il **numero durante il trascinamento**
  ("sto spostando di 4,00 m"), ed è quello da fare al suo posto.
- **Pivot modificabile.** Utile (ruotare un muro attorno allo spigolo invece che al centro), ma
  introduce una modalità nuova con il suo stato, e la rotazione di gruppo usa già il baricentro.
  Da fare quando ci sarà un caso concreto, non per completezza dell'elenco.

### L4 — Chiudere il ciclo: provare camminando · ✅ **FATTA 2026-08-11** (changelog 190)

- **Motore**: `--at x,z` fa nascere il giocatore in quel punto. Applicato **sullo spawn della
  MapDef** e non dentro i game mode: `spawnTeam1` lo leggono in due posti (Conquest e Sandbox), e
  un override applicato in uno solo darebbe "Prova da qui" funzionante in una modalità e muto
  nell'altra. Con l'override sul dato lo vede ogni lettore, compreso quello che verrà.
  L'unico punto di scrittura è `DefinitionRegistry::overrideSpawn`, dichiarato come override di
  sviluppo e limitato alla durata del processo — nessun file toccato.
- **Editor**: `Prova da qui` salva la mappa e lancia
  `GFEngine --walk --map "<mappa>" --at x,z` sul punto che la telecamera sta guardando.
  *(Correzione 2026-08-11, changelog 192: la prima versione passava `--direct-prematch` e apriva
  un MENU. Un menu fra l'utente e la mappa toglie a questo comando l'unica cosa che deve avere.
  `--walk` = sandbox con zero manichini, non un game mode nuovo — e azzerare i conteggi non
  bastava, perché la sandbox spawna comunque un manichino per definizione registrata.)*
  Il modulo **dichiara l'intenzione**, l'avvio del processo resta in `EditorApp`: un modulo che
  lancia eseguibili sarebbe il secondo posto da cui si avvia il gioco.
- **Il flag dice se ha avuto effetto**, su stderr. Un flag che tace è indistinguibile da un flag
  ignorato — difetto già trovato in `--stress`.
- **Verificato con esecuzioni vere**, non solo compilato: percorso riuscito, argomento malformato
  (`--at 12.5`), e mappa inesistente. Tutti e tre riportano ciò che è successo.

**Nota tecnica confermata**: la risoluzione di `data/` preferisce la **sorgente** alla copia
accanto all'exe, quindi il giro editor → gioco vede subito le modifiche appena salvate. Il ciclo
è chiuso davvero.
- **Sposta di un offset** (finestra con ΔX/ΔY/ΔZ, come TrenchBroom) e **Ruota di N°**.
- **Allinea** la selezione (min/centro/max su X, Y, Z) e **distribuisci** a passo uguale.
- **Appoggia su faccia**: mettere un box a contatto con quello sotto/accanto senza compenetrazione
  né fessure. Le fessure da 2 cm sono un difetto classico del navmesh e oggi nessuno le vede.
- **Restrizione d'asse** (Alt) e **linee di traccia** durante il trascinamento.
- **Pivot modificabile** (centro / angolo / base): oggi ruotare un muro attorno al suo centro
  quando serviva l'angolo è una correzione a mano.

### L3 — Reggere la scala: organizzazione della mappa
- **Gruppi locali persistenti** — e qui c'è una scorciatoia architetturale: un gruppo è già
  esprimibile come **istanza composita con parti locali e senza tipo di libreria** (ADR-056, parti
  locali). Non serve un sistema nuovo: serve permettere `localParts` senza `type`, dargli un nome,
  e mostrarlo come nodo in elenco. Da valutare in ADR prima di implementare.
- **Isola / nascondi la selezione** (e "mostra tutto"), separato dai filtri per categoria che già
  esistono: i filtri dicono *che tipo*, l'isolamento dice *quali*.
- **Blocco**: un gruppo bloccato non si seleziona per sbaglio. Su 300 × 200 il terreno va bloccato
  il primo giorno.
- **Selezione a trapano** con la rotellina, e **selezione per area** ("tocca" / "contiene").
- **Copia / incolla**, anche **fra mappe**, con incolla alla posizione originale o al cursore.

### L4 — vedi sopra: **fatta** (changelog 190). Resta fuori solo `Torna all'editor`
Il ritorno (il gioco stampa la posizione all'uscita, l'editor ci porta la telecamera) non è stato
fatto: è la metà meno importante del giro, perché la posizione di uscita si ritrova con `F` e con
le coordinate ai bordi. Da aggiungere se camminando risulta scomodo.

**Perché conta**: è la raccomandazione numero uno della letteratura, ed è l'unico modo di prendere
gli errori di scala — l'errore più frequente in assoluto — mentre si costruisce invece che dopo.

### L5 — Gli errori si dicono, non si scoprono · ⚠ **QUASI TUTTA 2026-08-11** (changelog 194)

**Fatto**: "portami lì" (seleziona **e inquadra**); due controlli geometrici nuovi
(`TooSmallElevated`, `NarrowGap`) dentro `analyzeTacticalHealth`, quindi visti **anche da
`--validate`**; il gate dei dati (`validateContent`) richiamabile dal pannello del Map Editor,
filtrato su questa mappa.

**Lezione da non ri-scoprire**: la prima versione dei controlli geometrici produceva **412
segnalazioni**, tutte pedate di scala. Un controllo che grida al lupo su ogni gradino è peggio di
nessun controllo. Il criterio che le separa era già nel progetto — `REF_UNIT_WIDTH` (1,20 m):
sotto la larghezza dell'unità di riferimento non è un ripiano, è una pedata. **Ogni controllo
nuovo va misurato sul contenuto reale prima di dichiararlo**, e la soglia va presa da
`MapMetrics`, mai scelta a occhio.

**Non fatto**: le **correzioni rapide** sulle voci. Ci ho pensato e le ho lasciate fuori di
proposito: per i difetti trovati non esiste *una* azione corretta (un ripiano troppo piccolo si
può allargare in due direzioni, una fessura si può chiudere da due lati), e un pulsante che
sceglie per l'autore modifica la sua geometria in un modo che lui non ha deciso. Il testo di ogni
voce dice **cosa fare e con quale comando** — `Precisione → Appoggia` per le fessure — che è
l'aiuto giusto senza il rischio.
Manca anche l'attribuzione per-box della verifica navmesh VERA sulla mappa intera (esiste per la
singola struttura, doc 48): è la cosa più utile rimasta di L5.

### L5 — testo originale del piano
- **Pannello Problemi** nel Map Editor: lo **stesso** `validateContent` del gate (nessuna seconda
  verità), più i controlli geometrici che oggi vivono nella salute tattica. Ogni voce con
  **"portami lì"** (seleziona e inquadra) e, dove è possibile, una **correzione rapida**.
- Le voci che servono davvero, dai difetti già visti: box compenetrati, fessure sotto la soglia di
  erosione, superficie dichiarata calpestabile che il navmesh non produce, altezza libera sotto
  2,10, ripiano senza accessi, struttura con un `type` sparito.

### L6 — Il ponte con Blender (l'approccio già scelto, reso esplicito)
Geometria semplice per il navmesh e la collisione, mesh vere sopra per l'aspetto. Serve dichiararlo
nel modello dei dati:
- un box può portare una **mesh di rappresentazione** (`mesh_id`) che **sostituisce il disegno** ma
  **non** la collisione né il navmesh;
- l'inverso di ciò che fa ProBuilder con l'export, e più adatto qui: il blockout **resta** la
  verità funzionale, la mesh è vestizione. Così una modifica di forma non richiede di rifare il
  navmesh a mano.
- **Out of scope finché le animazioni sono bloccate** (memoria di progetto: pose e animazioni sono
  ferme in attesa del PC nuovo e di Blender).

---

## 7. Out of Scope (esplicito — da non anticipare)

- **Editing di mesh** (vertici/spigoli/facce, bevel, edge loop, UV). Vedi §4.1: rompe navmesh,
  semantica e collisione. Se un giorno servirà una forma che le scatole non esprimono, la strada è
  una **mesh da Blender** (L6), non un modellatore dentro l'editor.
- **CSG sottrattivo** (scavare un buco in un box). Tentante per porte e finestre, ma le primitive
  parametriche `Doorway`/`Room` già le esprimono **con le misure garantite**, e un CSG darebbe due
  modi di fare la stessa cosa — cosa che l'utente ha chiesto esplicitamente di evitare.
- **Punti di aggancio tipizzati** fra composite. La fonte stessa dice di non arrivarci prima dei
  ~50 pezzi di kit (§4.5). Oggi siamo a 10 tipi.
- **Quattro viste affiancate**. Si può già passare fra Prosp/Alto/Fronte/Lato; TrenchBroom stesso
  tiene le viste 2D **disattivate per difetto** perché *"i viewport 2D fanno pensare in 2D"*.
  Riconsiderare solo se richiesto.
- **Terreno / heightmap.** Sistema a sé, non un'estensione del Map Editor (CLAUDE.md §5).

---

## 8. Dependencies

- **doc 52 (framework editor)**: L1 richiede la migrazione del gizmo del Map Editor a
  `ViewportEditing`. È il prerequisito, non un rifacimento opzionale.
- **doc 47 §4 / `MapMetrics`**: le metriche normative restano la sorgente unica dei minimi.
- **ADR-053 / ADR-056**: primitive parametriche e composite non cambiano; L3 propone di
  **riusare** le parti locali per i gruppi invece di aggiungere un sistema.
- **ADR-018 / `ContentValidation`**: L5 usa lo stesso gate, non una seconda verità.
- **ADR-003**: l'estrusione e la griglia proiettata disegnano di più; restano array client-side.
- **doc 51 §1**: prima di L1, togliere il ricalcolo dei derivati *durante* il trascinamento —
  altrimenti l'estrusione continua eredita il caso peggiore misurato.

---

## 9. Osservabilità (ADR-050, §5-bis — nasce con il sistema)

Nessuna fase è finita senza tutti e tre:

- **Sintomo**: *gesti per box*. Un contatore che dice quanti trascinamenti/clic/campi numerici sono
  serviti per creare e dimensionare l'ultimo box. È la misura diretta di ciò che questo documento
  promette, e senza si ottimizza a sensazione.
- **Funnel**: `clic nel viewport → colpito qualcosa → selezionato → gesto iniziato → gesto
  applicato`. Dove muore un'interazione: un clic che non seleziona e uno che seleziona la cosa
  sbagliata sono difetti diversi e oggi si confondono.
- **Singola entità**: `--trace-edit <indice>` — cosa è successo a QUEL box, operazione per
  operazione. È il pezzo che ha chiuso tutte le indagini finora.
- **Gate di authoring**: L5 *è* l'osservabilità di authoring di tutto il resto.

## 10. Invarianti (non negoziabili)

1. **Il mondo resta scatole e primitive.** Ogni gesto nuovo produce `MapGeometryBox` o
   `StructureDef`, mai una mesh arbitraria. È ciò che tiene la mappa leggera e il navmesh
   diagnosticabile.
2. **Una sola strada per ogni cosa.** Se `Doorway` esprime già una porta, non arriva un secondo
   modo di fare una porta. Vincolo confermato dall'utente: *"almeno non si fa confusione"*.
3. **Il pavimento fisico vince sempre.** Nessun gesto rapido può produrre geometria sotto i minimi
   normativi senza dirlo.
4. **Ogni gesto è annullabile con Ctrl+Z**, e un trascinamento intero è **una** voce.
5. **Niente comando tagliato, niente capacità dietro una sezione chiusa** (§6-bis, ADR-023).
   Ogni fase dichiara **dove si clicca** e cosa si vede quando non è ancora stata usata.
6. **`data/help/*.md` si aggiorna nello stesso change set** di ogni comando nuovo.

## 11. Ordine consigliato e perché

```
doc 51 §1 (togliere il ricalcolo durante il trascinamento)   ← prerequisito tecnico, piccolo
        ↓
doc 52 F1 sul Map Editor (gizmo condiviso)                   ← prerequisito strutturale di L1
        ↓
L1  disegna e tira            ← il salto di produttività
        ↓
L2  precisione                ← rende L1 affidabile invece che solo veloce
        ↓
L4  prova camminando          ← piccolo, e prende gli errori di scala mentre si costruisce
        ↓
L3  organizzazione            ← serve quando la mappa è già grande, non prima
        ↓
L5  pannello Problemi         ← massimo valore quando c'è molto da controllare
        ↓
L6  ponte Blender             ← quando le mesh esistono (oggi bloccate)
```

**L4 è deliberatamente prima di L3**: è piccola, chiude il ciclo di iterazione, e la letteratura
è unanime sul fatto che gli errori di scala presi in corsa costano una frazione di quelli presi
alla fine. L3 invece paga solo quando la mappa è già affollata.

---

## 12. Fonti

- [ProBuilder — Geometry / Face actions](https://docs.unity3d.com/Packages/com.unity.probuilder@5.0/manual/menu-geometry.html)
- [ProBuilder — manuale](https://docs.unity3d.com/Packages/com.unity.probuilder@5.0/manual/index.html)
- [Unreal — CubeGrid Tool](https://dev.epicgames.com/documentation/unreal-engine/cubegrid-tool-in-unreal-engine?lang=en-US)
- [Unreal — Modeling Mode](https://dev.epicgames.com/documentation/unreal-engine/modeling-mode-in-unreal-engine)
- [Unreal — Edit Pivot Tool](https://dev.epicgames.com/documentation/unreal-engine/edit-pivot-tool-in-unreal-engine?lang=en-US)
- [TrenchBroom — manuale di riferimento](https://trenchbroom.github.io/manual/latest/)
- [The Level Design Book — Blockout](https://book.leveldesignbook.com/process/blockout)
- [StraySpark — Modular Kit Snapping in UE5 (2026)](https://www.strayspark.studio/blog/modular-kit-snapping-ue5-comparison-2026)
- [Game Developer — A Simpler 3D Level Editor](https://www.gamedeveloper.com/design/a-simpler-3d-level-editor)
