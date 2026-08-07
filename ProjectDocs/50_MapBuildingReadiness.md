# 50 — Cosa manca per costruire la mappa grande (analisi + piano)

> Nasce da due osservazioni dell'utente (2026-08-06): *"hai messo le figure di scala, ma sono
> scomode, mi serve un modo per sapere le dimensioni tipo della mappa in maniera facile e chiara"* e
> *"l'editor strutture mi permette di modificare le strutture ma non di crearne di nuove … un sistema
> per far sì che io possa creare strutture che rispettino il navmesh, per fare anche magari strutture
> un po' più complesse"*.

## 0. La diagnosi in due righe

1. **Sulle misure**: le figure di scala sono un *surrogato*. Dicono "circa due metri"; non dicono
   "questo corridoio è 3,40". Manca la misura vera — e mancano le due cose che negli editor
   professionali la rendono immediata: la **vista ortografica** e un **righello libero**.
2. **Sulle strutture**: doc 48 dichiarava un'assunzione — *"creare strutture = creare TIPI (preset
   vincolati) delle primitive esistenti"* — e l'utente ha appena detto che non basta. L'assunzione
   era esplicita proprio perché potesse essere smentita: è successo, e va rifatta.

---

## 1. Misure — cosa fanno gli altri

- **Unreal, Measuring Tool**: trascinamento col tasto centrale, **con aggancio alla griglia**, e —
  dettaglio decisivo — **funziona solo in vista ORTOGRAFICA**. Non è un caso: in prospettiva una
  lunghezza sullo schermo non corrisponde a una lunghezza nel mondo, quindi misurare *guardando* è
  impossibile e serve un numero.
- **Hammer (Source)**: griglia in unità dichiarate (1 unità = 1 pollice) e viste ortografiche
  affiancate. La misura non è uno strumento: è il modo in cui si guarda la mappa.
- **Blockout modulare** (letteratura di level design): scala e pivot coerenti, angoli a **90° e 45°**,
  incrementi di griglia **grandi per stanze e muri, piccoli per la rifinitura**, e codifica a colori.

### Cosa abbiamo e cosa manca
| | stato |
|---|---|
| Griglia con aggancio (`m_gridSnap`, 0,5 m) | ✅ c'è |
| Figure di scala (clone + gigante 2,40) | ✅ c'è, ma è un surrogato |
| Righello | ⚠ solo fra **due elementi selezionati**: non misura uno **spazio vuoto**, che è il caso vero (larghezza di un corridoio, luce di un varco) |
| **Vista ortografica** | ❌ **non esiste**: `Camera::getProjection` è solo `glm::perspective` |
| **Dimensioni della mappa** | ❌ nessun punto della UI le dice |
| Dimensioni dell'elemento selezionato | ⚠ ci sono i campi sx/sy/sz, ma non l'ingombro di una selezione multipla |

> Prova a carico: **io stesso ho sbagliato due volte** la dimensione di Training Ground (prima
> "50 × 40", poi "154,9 × 91,9") prima di arrivare a 71,3 × 92,4 leggendo i limiti del navmesh. Se
> l'editor l'avesse scritto, non sarebbe successo — né a me né a chi costruisce.

---

## 2. Strutture — cosa fanno gli altri

- **AutoCAD, blocchi dinamici**: un blocco = geometria + **parametri** + **azioni** (stira, scala,
  specchia, serie) + **vincoli**, con l'opzione **elenco** che limita i valori ammessi e le *tabelle
  di blocco* per combinazioni predefinite. È il modello più vicino a ciò che serve qui, e conferma la
  direzione di ADR-055 (tipo con min/max) indicando il passo successivo: **le azioni**.
- **Revit, famiglie annidate**: una famiglia ospite che ne contiene altre, con la distinzione
  condivisa/non condivisa. Avvertenza esplicita della pratica: **"evitare l'eccesso di annidamento"**,
  perché diventa impossibile da gestire e da diagnosticare.

### Il nodo architetturale da decidere PRIMA di scrivere codice
Una "struttura composita" (più parti che formano una torre, un bunker, un edificio) somiglia
moltissimo a un **prefab** (ADR-048), che già oggi è *un insieme di box + posizioni tattiche piazzato
come unità*. Le differenze reali sono due:

| | prefab (ADR-048) | tipo struttura (ADR-055) |
|---|---|---|
| contenuto | box **fissi** + significato tattico | **ricetta** parametrica di UNA primitiva |
| al caricamento | espanso da un modello salvato | **rigenerato** dai parametri |
| vincoli | nessuno | `editable` + min/max, con pavimento fisico |

Un composito è esattamente **un prefab le cui parti sono ricette e che ha parametri propri**. Se
costruisco un "editor strutture composite" *e* un "editor prefab" separati, costruisco due volte la
stessa cosa — ed è precisamente ciò contro cui mette in guardia CLAUDE.md §5.

**Raccomandazione**: un solo sistema di **assemblaggi**. Le parti sono primitive o box liberi; un
prefab diventa il caso degenere senza parametri. Da decidere insieme prima di implementare: merita un
ADR, non una scelta presa di corsa.

### Perché non basta "un tipo = una primitiva"
Le nove primitive esprimono elementi, non edifici. Una torre con scala interna, un bunker con feritoie,
un magazzino su due livelli: nessuna è una primitiva, tutte sono **assemblaggi** di primitive più
qualche box. E l'assemblaggio è esattamente il punto in cui il navmesh si rompe (giunzioni che si
sfiorano, altezza libera sotto un solaio, accessi che non si toccano) — cioè dove la verifica isolata
di doc 48 serve di più.

---

## 3. Piano proposto, in ordine di utilità per iniziare la mappa

### Subito, per non restare fermi (piccolo, sblocca il lavoro)
- **M1 — Dimensioni della mappa sempre in vista**: ingombro X × Z, quota minima e massima, conteggio
  degli elementi. È la risposta letterale a *"sapere le dimensioni della mappa in maniera facile"*.
- **M2 — Ingombro della selezione**: larghezza × profondità × altezza di ciò che è selezionato, anche
  per selezioni multiple. Confrontabile a colpo d'occhio con le metriche normative.

### Poi, il salto vero sulle misure
- **M3 — Vista ORTOGRAFICA dall'alto** (e fronte/lato), con griglia etichettata. È la modifica che
  cambia davvero il modo di lavorare: in prospettiva non si misura, si stima. Richiede
  `Camera::getProjection` con un ramo ortografico e i comandi di vista.
- **M4 — Righello libero**: due clic nel viewport, con aggancio alla griglia, misura in pianta e
  totale, confronto immediato con corridoio/porta/scalino. Come Unreal, e legato a M3.

### Strutture composite (il sistema nuovo)
- **C0 — ADR sull'unificazione** assemblaggio/prefab. **Prima del codice**: è la decisione che evita
  di costruire due volte lo stesso sistema.
- **C1 — Assemblaggio fisso**: nel tab struttura si piazzano più parti (primitive + box liberi), si
  salva come tipo, e la verifica navmesh gira **sull'insieme**. Già questo dà "strutture complesse che
  rispettano il navmesh", che è la richiesta letterale.
- **C2 — Parametri dell'assemblaggio**: un parametro nominato che pilota più parti (es. "altezza
  torre" → muri **e** numero di rampe). È il livello "azioni" dei blocchi dinamici.
- **C3 — Elenco di valori ammessi** (l'opzione *List* di AutoCAD): per certe misure non serve un
  intervallo continuo ma poche varianti valide e già verificate.

**Da NON fare**: annidare assemblaggi dentro assemblaggi. La pratica Revit lo sconsiglia
esplicitamente, e qui moltiplicherebbe i modi in cui il navmesh può rompersi senza che si capisca dove.

## 3-bis. Stato (2026-08-06)

- **M1 ✅** ingombro della mappa sempre in vista (changelog 167) — con la rotazione, verificata per
  controincrocio: 71,3 × 92,4 su Training Ground, uguale ai limiti del navmesh.
- **M2 ✅** ingombro della selezione, con l'avviso sotto la misura del corridoio.
- **C0 ✅** decisione presa e approvata: **ADR-056**, un solo sistema di assemblaggi.
- **M3 ✅** vista ortografica Alto/Fronte/Lato (changelog 168), verificata con 7 controlli sulla
  matematica della proiezione.
- **M4 ✅** righello libero a due clic con aggancio alla griglia.
- **C1 ✅** assemblaggi (changelog 171): parti primitive **o** box liberi, espansione unica condivisa
  con registry e gate, verifica navmesh **sull'insieme**. Dimostrato su un caso reale in cui tre
  parti legali producevano un ripiano irraggiungibile.
- **C2/C3** (parametri dell'assemblaggio, elenchi di valori): solo se serviranno.

- **M5 ✅** (chiesto dall'utente dopo la prova sul campo, changelog 170): **barra di scala** sempre
  in vista + **coordinate ai bordi** con linee di riferimento in ortografica, e **"Inquadra tutto"**
  (F). Erano previste come "da decidere": l'uso reale le ha decise.

**Nota di metodo**: la griglia etichettata era stata rimandata perché *"l'inquadratura in metri e il
righello coprono il caso d'uso"*. Alla prova pratica non era vero — il righello dice la distanza fra
due punti **scelti**, la barra di scala dice quanto è grande ciò che si **guarda**, e servono
entrambe. Una previsione sbagliata smentita nel modo giusto: usando lo strumento.

## 4. Ordine consigliato
**M1 + M2 subito** (piccoli, e servono dal primo giorno di costruzione) → **C0** (la decisione) →
**M3 + M4** → **C1** → C2/C3 solo se serviranno davvero.

Il motivo dell'ordine: M1/M2 costano poco e tolgono attrito immediato; C0 è una decisione, non
lavoro; M3/M4 cambiano il modo di misurare; C1 è il sistema nuovo e va fatto quando la decisione
architetturale è presa.
