# 47 — Map building e geometrie: IL PIANO (Planned Feature)

> **Stato: STRUMENTI COMPLETI — G1-G7 implementate e verificate (2026-08-05, changelog 149-156).**
> Resta **G8** (riparazione di Training Ground con i nuovi strumenti), poi l.utente puo costruire la
> mappa 300 x 200. Documento di scope (CLAUDE.md §5).
> È il **prerequisito** di [46_MetadataPlan.md](46_MetadataPlan.md): serve a mettere l'utente in
> condizione di costruire la mappa **300 × 200 m** su cui poi si svilupperanno i metadata.
>
> **L'audit di §1 descrive lo stato PRIMA dell'implementazione** e si legge come storico: da
> 2026-08-05 l'undo esiste, il `type` arriva al runtime, e le primitive parametriche pure.
>
> Richiesto dall'utente il 2026-08-04: *"puoi partire con ricerche e pianificazione sul sistema di
> map building, geometrie eccetera"*, dopo aver deciso la taglia: *"idealmente 300 × 200 va bene...
> deve essere semplicemente grande abbastanza per testare su una mappa che sia effettivamente
> abbastanza grande e complessa per permettere di integrare e provare al meglio tutti i sistemi"*.

**Ordine di lavoro**: questo piano → si implementa → **l'utente costruisce la mappa** → si
implementa doc 46 (metadata) su quella mappa.

---

## 0. Il vincolo che orienta tutto

La mappa nuova è **9,1× Training Ground** (60.000 m² contro 6.595). Non è "un po' più grande": è un
cambio di regime. Alla densità di oggi significa **~1.520 box** contro 167.

Quindi la domanda di questo documento non è *"come si fanno geometrie più belle"*. È:

> **Cosa serve perché una persona sola possa costruire 1.520 box senza impazzire e senza
> sbagliare — e perché quello che costruisce sia percorribile per costruzione, non per fortuna?**

Ogni scelta qui sotto risponde a quella. Le due metà — **produttività** e **correttezza per
costruzione** — sono in §5 e §3.

---

## 1. Audit: cosa abbiamo davvero oggi (letto sul codice, non ricordato)

### 1.1 Il formato geometria è UN box, e non sa nemmeno inclinarsi

```cpp
struct MapGeometryBox {
    float x, y, z;              // centro
    float ry;                   // ← rotazione attorno a Y. E BASTA.
    float sx, sy, sz;           // dimensioni totali
    float r, g, b;  bool collider;
};
```

È l'intera espressività geometrica del gioco. Tre conseguenze, in ordine di gravità:

1. **Una superficie inclinata è INESPRIMIBILE.** Non c'è pitch né roll. Non si può autorare una
   rampa, non una collina, non un tetto spiovente, non un terrapieno.
2. **E questo mentre il navmesh dichiara di accettare pendenze fino a 45°**
   (`kAgentSlope = 45.0f`). Abbiamo una capacità di navigazione che **nessun dato può attivare**:
   è la stessa forma di difetto di ADR-023 (*"se il combo dell'editor offre un sottoinsieme, la
   capacità di fatto non esiste"*), applicata alla geometria invece che alle classi.
3. **Le "scale" sono box impilati a mano**, ed è esattamente lì che è nato KI #95: alzate di
   **0,68-1,21 m** contro uno `STEP_HEIGHT` di **0,55**. L'autore le ha disegnate credendo fossero
   scale; niente e nessuno gli ha detto il contrario.

### 1.2 L'editor: selezione singola, nessun undo, nessun gruppo

Verificato in `editor/src/modules/MapEditor.cpp` (2.921 righe) e nel suo header:

| capacità | stato |
|---|---|
| selezione | **`int m_selBox` — UNA sola.** Non si può selezionare un edificio. |
| **undo/redo** | **non esiste.** Nessuno `undo`, nessun command stack, in tutto `editor/`. |
| gruppi / livelli | non esistono |
| array / duplica con offset | non esiste (`duplicateSelected` fa **una** copia) |
| snap alla griglia | ✅ **esiste**: Off / 0,25 / 0,5 / 1,0 / 2,0 |
| gizmo sposta/ruota/scala | ✅ esiste |
| primitive oltre il box | nessuna |
| misura / righello | non esiste |

Il set completo di comandi di costruzione della toolbar è: **`+ Box`, `Duplica`, `Elimina`**.

> Con questi tre pulsanti, 1.520 box significa circa **1.520 clic su "+ Box"** più il
> posizionamento manuale di ognuno, **senza poter annullare un errore**. Non è una stima
> pessimistica: è l'aritmetica degli strumenti che ci sono.

### 1.3 C'è già una semantica autorata che nessuno legge

L'editor scrive per ogni box un campo `type` fra **`floor` / `wall` / `platform` / `cover` /
`decoration`** — e su Training Ground è pure compilato con criterio: **75 `floor`, 74 `wall`,
18 `cover`**. Ma `MapGeometryBox` non ha un campo `type`: **il runtime lo scarta al parse.**

È un canale semantico **già autorato, già popolato e già gratuito**, buttato via. Doc 46 lo può
usare subito (un `platform` deve avere un accesso; un `cover` è candidato per la generazione; un
`decoration` non dovrebbe nemmeno essere un collider).

### 1.4 Non esistono metriche di riferimento

Non c'è un documento né una costante che dica *"un corridoio è largo così, una porta è alta così,
un'alzata è alta così"*. È il motivo strutturale per cui le scale di Training Ground sono sbagliate:
**non c'era un numero giusto da rispettare.**

---

## 2. Ricerca: come si costruiscono i livelli, davvero

### 2.1 Il processo di blockout (Level Design Book)

L'ordine raccomandato è preciso e non è quello che facciamo:

1. schizzo della pianta → 2. **piano quotato all'origine** → 3. **figura di scala umana in scena** →
4. un muro alto **150-200%** della figura → 5. duplicare fino a una stanza intera →
6. **provare in gioco subito**, camminando con gravità e collisione vere — *non* volando nell'editor.

Errori più comuni, in ordine: **problemi di scala** (il più frequente), poi *"costruire troppo prima
di provare"*. Il rimedio raccomandato è banale e noi non ce l'abbiamo: **più figure di scala sparse
nel livello**.

Per gli sparatutto multiplayer c'è una metrica di ritmo esplicita: **8-12 secondi dallo spawn a un
obiettivo**. È un numero che possiamo verificare da soli, perché il campo di distanza di percorso di
doc 46 §3.3 lo dà gratis.

### 2.2 Le metriche di riferimento

| misura | Unity | Unreal | Quake/Source | reale |
|---|---|---|---|---|
| agente (larghezza × altezza) | 1,0 × 1,8 m | 0,6 × 1,76 m | 32 × 72 in | 0,5 × 1,75 m |
| altezza muro | 3,0 m | 3,0 m | 128 in | 2,4 m |
| corridoio minimo | 2,0 m | 1,5 m | 64 in | 1,2 m |
| porta (L × H) | 1,25 × 2,5 m | 1,1 × 2,2 m | 56 × 112 in | 0,9 × 2,0 m |
| **scala (alzata × pedata)** | **0,10 × 0,15 m** | **0,15 × 0,25 m** | 8 × 12 in | 0,18 × 0,28 m |

Due regole trasversali: **il corridoio minimo è almeno il doppio della larghezza dell'agente**, e le
scale stanno su pendenze di **30-35°**.

> Il confronto che conta: le alzate reali stanno fra **0,10 e 0,18 m**. Le "scale" di Training Ground
> hanno alzate **da 0,68 a 1,21 m** — cioè da **4 a 8 volte** una scala vera. Non sono scale ripide:
> non sono scale.

### 2.3 I kit modulari

Regola di griglia: **si costruisce con snap pari a metà dell'ingombro del pezzo**, e ogni pezzo deve
stare dentro un **bounding box dichiarato** ("stay in footprint"). Il kit si valida con tre prove:
**Loopback** (i pezzi si chiudono ad anello), **Stack** (si impilano senza fessure), **Gap** (ci sono
i pezzi "di raccordo" per gli angoli non ortogonali).

Tipi di modulo canonici: pavimento, soffitto, muro, porta singola/doppia, finestra, angolo,
piattaforma, raccordi, archi, gusci.

### 2.4 Gli editor a brush (TrenchBroom, Hammer)

Cosa li rende produttivi su mappe grandi, in ordine di importanza dichiarata:
**undo/redo illimitato**; **livelli** (partizionare la mappa in aree nascondibili per ridurre il
disordine visivo); **gruppi** (fondere pochi oggetti in uno da editare insieme); manipolazione di
**più vertici insieme**; **ridimensionamento in massa** con gestione dei piani condivisi;
**duplicatore con offset progressivo** per N copie.

> Nota importante su cosa **non** prendere: il modello a *brush con CSG* (sottrazione booleana) è
> potente ma è un'altra rappresentazione del mondo — e noi abbiamo ADR-047 (**il box è la verità
> tattica**) proprio perché il box analitico è ciò che rende veloci LOS, navmesh e analisi. Adottare
> il CSG significherebbe rimettere in discussione l'intera fondazione tattica per un guadagno di
> comodità. **Prendiamo il flusso di lavoro degli editor a brush, non la loro geometria.**

---

## 3. LA DECISIONE — primitive PARAMETRICHE che si espandono in box

### 3.1 Il principio

> **Una scala non è un oggetto: è una RICETTA.** L'autore dichiara *"da qui a lì, larga 4 m"*, e la
> macchina emette i box con l'alzata giusta. **L'alzata sbagliata diventa inesprimibile.**

Questo è precisamente il pattern che **abbiamo già e di cui ci fidiamo**:
- **ADR-048 (prefab)**: si autora il significato una volta, si espande per istanza al load.
- **ADR-033 (derivati)**: ciò che si espande **non si salva mai** — si rigenera, quindi non può
  diventare stantio.

Una primitiva geometrica è la stessa cosa applicata alla forma invece che al significato. **Zero
concetti nuovi**, zero nuove verità sul mondo, zero modifiche a collisione/LOS/navmesh/render:
a valle dell'espansione ci sono solo `MapGeometryBox`, esattamente come oggi.

### 3.2 Perché questo e non le alternative

| alternativa | perché no |
|---|---|
| **aggiungere pitch/roll al box** | rompe lo slab test analitico della LOS (`hasLineOfFire`), `appendBox` del navmesh, la collisione e ogni consumatore di `ry`. Toccherebbe la fondazione tattica (ADR-047) per una feature di authoring. |
| **mesh arbitrarie come collisione** | contraddice ADR-047 in modo frontale: la LOS analitica sui box è ciò che rende economica tutta l'analisi tattica. |
| **CSG / brush** | §2.4: altra rappresentazione del mondo, stesso problema. |
| **continuare a impilare box a mano** | è lo stato attuale, ed è ciò che ha prodotto KI #95. |

### 3.3 Come una rampa diventa percorribile senza pendenze

La rampa si espande in una **scalinata a passo fine**. Sembra un compromesso e invece è la scelta
tecnicamente migliore, per tre motivi verificabili:

1. **Aggira del tutto il limite di pendenza.** Le pedate sono orizzontali (0°), quindi
   `walkableSlopeAngle` non si applica mai: nessun rischio di superfici classificate non-walkable.
2. **`kCellHeight` è 0,10 m.** Un'alzata multipla di 0,10 è rappresentata **esattamente** nel campo
   di altezza di Recast, senza arrotondamenti — quindi la scala che vedi è la scala che il navmesh
   costruisce.
3. **Il visivo resta libero.** ADR-047 dice già che Blender fornisce l'aspetto: sopra i box
   scalettati può stare una mesh liscia. La verità tattica resta analitica, l'occhio vede una rampa.

**Alzata raccomandata: 0,20 m; pedata 0,30 m → 33,7°**, dentro la banda 30-35° della letteratura e
multiplo esatto di `kCellHeight`. Per le rampe veicolari: 0,10 × 0,40 → 14°.

Costo: una salita di 3 m sono **15 box**. Trascurabile sia in render (15 × 24 = 360 vertici, contro
i 161.000 di una singola mesh B1) sia in navmesh.

### 3.4 Le primitive da fare

Ognuna si **salva come parametri** e si **espande in box al load** (mai salvati, ADR-033).

| primitiva | parametri autorati | espande in | risolve |
|---|---|---|---|
| **Scala** ✅ | da (x,y,z), dislivello, larghezza, alzata, **pedata** | N gradini | KI #95 alla radice |
| **Rampa** ✅ | come sopra, alzata fine (0,10) | N gradini fini | la pendenza inesprimibile |
| **Vano scala** ⚠ **NON consegnato** | dislivello, larghezza, dislivello per rampa | N rampe + pianerottoli | dimezzerebbe l'ingombro (8 m di salita diritta = 12 m in pianta), ma su **sei torri di prova tre non sono percorribili** e il gate sui dati non lo vede — il difetto nasce nella **voxelizzazione**. Fuori dal menu finché non è affidabile; nel frattempo una torre si fa con `platform` + `stair` per livello (changelog 158) |
| **Muro** ✅ | lunghezza, altezza, spessore | 1 box orientato | evita il calcolo a mano di centro e `ry` |
| **Muro con apertura** ✅ | + larghezza/altezza/parapetto/scostamento | stipiti + architrave (+ parapetto) | porta o **finestra**; il parapetto è **copertura vera** |
| **Stanza (guscio)** ✅ | ingombro, altezza, spessore, soffitto, lati aperti | pavimento + 4 muri + soffitto, con le porte | il modulo più ripetuto di tutti; **un interno non nasce senza vie d'ingresso** |
| **Piattaforma** ✅ | ingombro, quota, **accessi obbligatori** | box + le scale dichiarate | *una piattaforma non può nascere irraggiungibile* |
| **Passerella** ✅ | lunghezza, larghezza, quota, parapetti | impalcato (+ parapetti `cover`) | un **corridoio in quota**: corsia tattica che domina il piano di sotto |
| **Linea di coperture** ✅ | lunghezza, lungh. elemento, varco, altezza | N box `cover` | terreno tattico da campo di battaglia, già nel formato che la derivazione (doc 46) cerca |

> **Regola generale emersa dall'implementazione (2026-08-05)**: due superfici che devono restare
> **connesse** vanno **sovrapposte**, non accostate. Recast erode `kAgentRadius` (0,40) da ogni bordo
> non camminabile: due ripiani a quote diverse che si toccano solo sul bordo restano separati da
> 0,80 m di vuoto e il navmesh non li collega. È il difetto che ha reso irraggiungibile la prima
> versione della scala con pianerottolo. Corollario: una scala deve **culminare sul bordo** di un
> ripiano, mai passarci sotto — le celle sotto un solaio perdono l'altezza libera e vengono scartate.

> L'ultima riga è la più importante del documento: **la piattaforma dichiara i suoi accessi come
> parte della propria definizione**. È il punto in cui "percorribile per costruzione" smette di
> essere uno slogan — non si tratta di verificare dopo, si tratta di rendere il difetto
> inesprimibile prima.

### 3.5 E il `type` torna a servire

Il campo di §1.3 diventa parte del contratto: `MapGeometryBox` guadagna `type`, il runtime lo legge,
e doc 46 lo usa come ingresso della derivazione. È il cambiamento più economico dell'intero piano —
il dato **esiste già** e su Training Ground è **già compilato**.

---

## 4. Le metriche di Galactic Front

### 4.1 Le taglie vere delle unità — MISURATE, non assunte (2026-08-05)

L'utente ha posto la domanda giusta: *"non sono sicuro al 100% che siano giuste, perché i dati tipo
l'altezza dei modelli li vedi tu"*. Quindi le ho misurate dalle hitbox reali (`data/hitboxes/`,
moltiplicate per il `mesh_scale` della definizione), invece di dedurle dalle costanti:

| unità | altezza modello | busto | testa a |
|---|---|---|---|
| **Clone Trooper** (`mesh_scale` 0,011) | **1,98 m** | 0,33 m | 1,85 m |
| **B1 Battle Droid** (`mesh_scale` 1,0) | **2,03 m** | 0,30 m | 1,81 m |

E qui è saltato fuori un difetto vero: **la stessa unità ha TRE altezze diverse nel motore.**

| dove | valore | a cosa serve |
|---|---|---|
| modello / hitbox | **1,98-2,03 m** | colpirla |
| agente navmesh (`kAgentHeight`) | **era 1,80 m** ⚠ | decidere dove passa |
| box di collisione (`AI_HALF_Y` 0,50) | **1,00 m** ⚠ | fisica |

Il navmesh dichiarava percorribile un sottopasso da 1,85 m in cui la testa dell'unità sarebbe
passata **dentro** il soffitto.

> **Corretto**: `kAgentHeight` **1,80 → 2,10 m**. Misurato su Training Ground: costa **4 poligoni su
> 1047**, raggiungibilità degli obiettivi identica, posizioni irraggiungibili identiche (8/169).
> **Resta aperto** il box di collisione a 1,00 m: è metà del modello, ed è un secondo disallineamento
> da valutare separatamente (tocca la fisica, non la navigazione).

### 4.2 Il GIGANTE DI RIFERIMENTO — dove sta il margine, e perché lì

Richiesta esplicita dell'utente: *"l'importante è che siamo sicuri siano perfetti e lascino un minimo
di margine in caso magari di truppe che siano un po' più alte o larghe"*.

Il vincolo strutturale che decide come dare quel margine: **`kAgentRadius` e `kAgentHeight` sono
costanti globali e il navmesh si costruisce UNA volta, per UNA taglia.** Non esiste oggi un navmesh
per taglia. Quindi un'unità più larga di quella con cui il navmesh è stato costruito **non entra**
nei passaggi, e non c'è nulla che l'AI possa fare a runtime.

Da cui la regola che governa tutto questo capitolo:

> **Il margine va messo nella GEOMETRIA, non nelle costanti.**
> Cambiare `kAgentRadius` è una riga. Allargare i corridoi di una mappa da 60.000 m² già costruita è
> rifarla. Quindi la mappa si dimensiona sul gigante; il motore resta tarato sull'unità di oggi.

**Gigante di riferimento: 2,40 m di altezza × 1,20 m di larghezza.** Non è un numero inventato:
copre con margine reale il repertorio plausibile di questo gioco — Super Battle Droid, Magnaguard,
Droideka in cammino, Wookiee — contro i 2,03 m e 0,80 m di oggi. Sono **+18% in altezza e +50% in
larghezza**.

Quando servirà davvero, ci sono due strade, entrambe già aperte da questa scelta:
alzare le costanti (una riga, e la mappa lo regge già), oppure **un secondo navmesh per taglia**
(Recast lo supporta; costo misurato: +1,4 s di build sulla mappa grande).

### 4.3 Le regole normative — dimensionate sul gigante

| elemento | valore | dimensionamento |
|---|---|---|
| griglia di costruzione | **0,5 m**, moduli su multipli di **2 m** | snap già presente in editor |
| alzata scala | **0,20 m** | multiplo esatto di `kCellHeight` (0,10); con pedata 0,30 → 33,7°, dentro la banda 30-35° |
| pedata scala | **0,30 m** min, poi **libera verso l.alto** | sotto i 0,30 Recast fatica a collegare le pedate. È la leva per rendere una scala più dolce **senza toccare l.alzata**: il numero di gradini dipende solo dal dislivello |
| **mai un dislivello fra 0,55 e 1,29 m senza scala** | regola | sopra 0,55 l'AI **si ferma** (non salta); sotto 1,29 il giocatore salta → posto dove lui sale e l'AI resta a sbattere |
| **corridoio minimo** | **2,4 m** | 1,20 (gigante) + 2 × 0,60 di erosione navmesh. Con 2,0 m un agente da 0,60 di raggio avrebbe 0,8 m di navmesh: passa a stento e il crowd si incastra |
| corridoio principale (due di fronte) | **3,6 m** | 2 × 1,20 + margine di manovra |
| **porta** | **1,8 × 2,8 m** | larghezza: il gigante (1,20) + mezzo metro per non incastrare il crowd. Altezza: 2,40 + 0,40 di franco |
| **altezza libera al coperto** | **≥ 2,8 m** | ⚠ era 2,4: **troppo poco**, perché i modelli sono già 2,03 e il gigante è 2,40 |
| muro standard | **3,2 m** di altezza, **0,25 m** di spessore | copre il gigante in piedi e resta non scavalcabile (> 1,29 m di salto). Lo spessore era 0,40 fisso e risultava grosso a vista (utente, 2026-08-05): 0,25 resta ben sopra la cella del navmesh (0,20), sotto la quale un muro rischia di non essere voxelizzato come ostacolo continuo |
| copertura bassa / alta | **1,0 m / 1,7 m** | 1,0 ripara accovacciati e permette di sporgersi; 1,7 ripara in piedi anche il gigante |
| **spawn → obiettivo** | **8-12 s di cammino** | metrica di ritmo (§2.1), verificabile col campo di distanza di doc 46 |

**Cosa è cambiato rispetto alla prima stesura, e perché**: corridoio 2,0 → **2,4**, porta 1,5 × 2,4 →
**1,8 × 2,8**, altezza libera 2,4 → **2,8**, muro 3,0 → **3,2**, copertura alta 1,6 → **1,7**. Tutte
e cinque erano dimensionate sull'unità di **oggi**; ora lo sono sul gigante. Costano solo disciplina
in fase di costruzione, e fanno la differenza fra una mappa che regge il futuro e una da rifare.

> Nota sulla riga in grassetto: è una regola che **nasce da una nostra misura**, non dalla
> letteratura. Il giocatore supera 0,55 m saltando, fino a 1,29 m; **l'AI non salta affatto** e
> sopra 0,55 si ferma. Qualunque dislivello in quella fascia produce un posto dove il giocatore va e
> l'AI resta a sbattere contro il bordo. È esattamente il sintomo che l'utente ha descritto
> (*"molti sono rimasti giù ad andare contro il muro"*), espresso come vincolo di costruzione
> invece che come bug da inseguire caso per caso.

---

## 5. L'editor: le sette cose senza cui 1.520 box non si costruiscono

In ordine di **valore per ora di lavoro**, non di difficoltà.

| # | cosa | perché è lì in classifica |
|---|---|---|
| **E1** | **UNDO/REDO** | è il #1 dichiarato dagli editor a brush, e non ce l'abbiamo **affatto**. Senza, ogni esperimento su una mappa grande è irreversibile — e la paura di sbagliare è ciò che rende lenti. |
| **E2** | **Selezione multipla** (+ gruppi) | oggi non si può spostare un edificio. Con 1.520 box è la differenza fra modificare la mappa e ricostruirla. |
| **E3** | **Primitive parametriche** (§3.4) | 15 box di una scala diventano 1 oggetto con 3 parametri, **e giusti per costruzione**. |
| **E4** | **Duplica con offset / array** | il "duplicatore N copie con offset progressivo" degli editor a brush: una fila di 12 colonne è un comando, non 12 operazioni. |
| **E5** | **Livelli e filtri di visibilità** | su 1.520 box il problema diventa **vedere**. Nascondere per tipo/quota/zona è ciò che rende lavorabile un interno. |
| **E6** | **Figura di scala + righello** | il rimedio raccomandato all'errore più comune (§2.1). Costa pochissimo: una silhouette 0,8 × 1,8 m piazzabile. |
| **E7** | **Validazione dal vivo nel viewport** | §6. |

Note di attuazione, per non violare le regole del progetto:
- **E1 va progettato per primo anche se lo si implementa presto**: un undo aggiunto dopo a un editor
  che muta lo stato in mille punti è una riscrittura. Il modo economico è **snapshot del documento**
  (la mappa è ~180 KB di JSON: uno snapshot per operazione è banale in memoria), non un command
  pattern su ogni campo.
- **E2/E3 toccano lo schema salvato** → si applica il READ-MODIFY-WRITE obbligatorio (`saveJsonRMW`,
  ADR-010): le primitive aggiungono una sezione, non riscrivono il file.
- Vale [[ui-no-clipping-use-dropdowns]]: la toolbar ha già 8 controlli; questi sette comandi vanno
  **raggruppati in menu**, non aggiunti in fila finché la barra si taglia.

---

## 6. La validazione mentre si costruisce

Oggi `--validate` esiste e trova i difetti (`UnmarkedCover`, `UnreachablePoint`, e su Training
Ground riporta 13 problemi con i numeri concreti). Il limite è **quando** parla: a mappa finita, da
riga di comando. Serve che parli **mentre costruisci**, nel viewport.

| controllo | quando | cosa mostra |
|---|---|---|
| **ripiano senza accesso** | dal vivo | il box si colora di rosso, con l'alzata mancante in metri |
| **dislivello nella banda proibita** (0,55-1,29 m) | dal vivo | evidenzia il bordo: *"il giocatore sale, l'AI no"* |
| **corridoio più stretto del minimo** | dal vivo | la strozzatura si colora — è il caso che l'erosione del navmesh cancella |
| **connettività vera dallo spawn** | a richiesta (build navmesh) | colora per **componente connessa**: le isole si vedono a colpo d'occhio |
| **tempo di cammino spawn → obiettivo** | a richiesta | il numero della metrica di ritmo (8-12 s) |

> La quarta riga è quella che chiude KI #95 in modo definitivo, ed è **esattamente il `componentId`
> del livello B di doc 46**: lo stesso dato serve all'AI a runtime e all'autore nell'editor. Un
> calcolo, due consumatori — che è la regola d'oro dei tre livelli applicata all'authoring.

---

## 7. Costi e budget — misurati

Dalla mappa sintetica 300 × 200 con 1.520 box (doc 46 §7):

| | Training Ground | 300 × 200 misurata |
|---|---|---|
| build navmesh | 0,113 s | **1,385 s** |
| poligoni navmesh | 1.047 | 5.806 |
| triangoli in input | 2.040 | 18.240 |

**Il navmesh a tile singola regge** (`ok: true`) e non serve alcun cambio architetturale. Ma il costo
cresce **più che linearmente** (12,3× per 9,1× di area), quindi:

- **budget di load dichiarato per la mappa nuova: ≤ 4 s** (1,4 navmesh + ~0,7 grafo tattico dopo
  M0-bis + margine). Va **misurato a ogni fase**, con la zona di profiler, non stimato.
- **budget di render**: 1.520 box ≈ 36.500 vertici. Contro i 161.000 vertici della sola mesh B1
  (doc 43) è irrilevante: **la geometria della mappa non è il problema di rendering**, i personaggi
  lo sono. Il frustum culling già fatto lavora a favore.

---

## 8. Osservabilità (ADR-050)

- *sintomo*: **`box_orfani`** — box che non contribuiscono al navmesh né bloccano nulla (né
  camminabili, né ostacoli). Su una mappa grande è il modo in cui il lavoro inutile si accumula
  invisibile.
- *funnel*: `box autorati → box espansi da primitive → triangoli in input → span camminabili →
  poligoni → componenti connesse`. Ogni gradino ha un denominatore, e dice **dove** la geometria
  smette di diventare spazio giocabile.
- *singola entità*: `--dump-box <id>` (dimensioni, tipo, quanti poligoni genera, a quale componente
  appartiene) e la colorazione per componente nel viewport (§6).
- Il **gate `--validate`** guadagna le regole metriche di §4 come difetti di contenuto, con l'azione
  concreta: non *"corridoio stretto"* ma *"corridoio 1,4 m, minimo 2,0 — allarga di 0,6"*.

---

## 9. Cosa NON facciamo

- **Niente CSG/brush** (§2.4) e **niente collisione a mesh**: ADR-047 resta.
- **Niente pitch/roll sul box**: la rampa si risolve per scalettatura (§3.3).
- **Niente terreno continuo / heightmap**: è un altro sistema, con un'altra pipeline di navmesh e di
  render. Se servirà, sarà un ADR suo.
- **Niente generazione procedurale di layout.** Il layout è design, ed è dell'utente (doc 41 §5.3).
- **Niente streaming / tiling del navmesh**: misurato non necessario a 300 × 200 (§7). Diventerà
  necessario oltre, e allora sarà una decisione informata.
- **Niente alzata di `STEP_HEIGHT`.** 0,9 m non è un gradino, è un salto: cambierebbe il movimento
  ovunque per tappare un difetto di authoring. Il rimedio è la primitiva, non la costante.

---

## 10. Fasi, con criteri di accettazione

| # | fase | criterio di accettazione |
|---|---|---|
| **G1** ✅ | **Metriche di Galactic Front** (§4) scritte, e `--validate` le applica | su Training Ground il gate riporta le violazioni **con l'azione concreta**; i 13 problemi noti restano coerenti |
| **G2** ✅ | **Undo/redo** a snapshot (E1) | 50 operazioni consecutive annullate e rifatte senza divergenza; il JSON salvato dopo undo/redo è **byte-identico** all'originale |
| **G3** ✅ | **Selezione multipla** (E2) | si sposta un edificio di 20 box in un'operazione sola; RMW rispettato al salvataggio |
| **G4** ✅ | **Primitive parametriche** (E3): scala, rampa, muro, piattaforma-con-accessi | una scala da 3 m generata dalla primitiva ha **tutte** le alzate ≤ 0,55 e il navmesh la collega — verificato con la connettività, non a occhio |
| **G5** ✅ | **`type` letto dal runtime** (§3.5) | `MapGeometryBox.type` popolato su Training Ground senza toccare i dati (è già scritto); nessun cambio di comportamento |
| **G6** ✅ | **Array/duplica con offset + livelli + figura di scala** (E4-E6) | una fila di 12 elementi in un comando; la mappa resta leggibile con 1.500 box |
| **G7** ✅ | **Validazione dal vivo nel viewport** (§6, E7) | i box difettosi si colorano (rosso = problema, ambra = avviso) mentre si costruisce, dalla **stessa `analyzeTacticalHealth`** del gate `--validate`; contatore di salute sempre visibile in toolbar. I due controlli su **navmesh** (connettività dallo spawn, tempo di cammino) restano al motore: l'editor non linka Recast (ADR-002), e le sonde `objective reachability` / `posizioni irraggiungibili` (changelog 147) li riportano già al load |
| **G8** | **Riparazione di Training Ground con i nuovi strumenti** | le sue scale rifatte con la primitiva: 0 violazioni, e le AI salgono sulle piattaforme (misurato con `--sim-ticks`, non a occhio) |

> **G8 è il vero collaudo.** Riparare la mappa che ci ha insegnato il problema, con gli strumenti
> nati per non farlo ripetere, è la prova che gli strumenti funzionano — e lo fa su una mappa che
> conosciamo, prima di investirci 60.000 m² di lavoro nuovo.

---

## 11. Rischi

| rischio | disinnesco |
|---|---|
| **Undo aggiunto tardi = riscrittura dell'editor** | G2 subito dopo G1, e a **snapshot** (la mappa è ~180 KB: costa nulla) |
| **Le primitive rendono i dati illeggibili senza l'editor** | si salvano i **parametri**, non i box: il JSON diventa *più* leggibile, non meno |
| **L'espansione al load rompe mappe esistenti** | le primitive sono una sezione **nuova**; i box a mano continuano a funzionare identici (fallback documentato, CLAUDE.md §2) |
| **Il budget di load sfora** | zona di profiler da G1 e numero nell'inventario di avvio; budget dichiarato ≤ 4 s (§7) |
| **Si costruisce la mappa grande prima che gli strumenti siano pronti** | è il rischio più costoso di tutti: 60.000 m² costruiti a mano andrebbero rifatti. **G1-G7 prima**, poi si costruisce |
| **Le metriche si rivelano sbagliate a mappa fatta** | G8: si collaudano prima su Training Ground, che è già costruita |

---

## 12. Decisioni per l'utente

1. ~~**Le metriche di §4 vanno bene?**~~ → **CONFERMATE dall'utente (2026-08-05)**, con la richiesta
   di *"un minimo di margine in caso di truppe un po' più alte o larghe"*. Recepita: taglie misurate
   dalle hitbox (§4.1, e ha fatto emergere `kAgentHeight` più basso dei modelli, corretto), e
   **gigante di riferimento 2,40 × 1,20 m** con il margine messo nella **geometria** e non nelle
   costanti (§4.2). Cinque metriche allargate di conseguenza (§4.3).
2. ~~**Quante primitive nel primo giro?**~~ → **più del minimo** (utente): **scala, rampa, muro,
   piattaforma-con-accessi**. La stanza/guscio resta per un secondo giro.
2-bis. **Resta aperto un disallineamento**: il box di collisione delle AI è alto **1,00 m**
   (`AI_HALF_Y` 0,50) contro modelli da ~2,0 m. Tocca la fisica, non la navigazione, e va valutato
   a parte — non l'ho cambiato.
3. **Vuoi che ripari Training Ground (G8) prima che tu inizi la mappa nuova?** Io lo consiglio: è il
   collaudo degli strumenti su una mappa di cui conosciamo già i difetti, e ti restituisce una mappa
   di test funzionante nel frattempo.

---

## Fonti

- [Blockout — The Level Design Book](https://book.leveldesignbook.com/process/blockout)
- [Metrics — The Level Design Book](https://book.leveldesignbook.com/process/blockout/metrics)
- [Modular kit design — The Level Design Book](https://book.leveldesignbook.com/process/blockout/metrics/modular)
- [TrenchBroom Reference Manual (livelli, gruppi, undo, mappe grandi)](https://trenchbroom.github.io/manual/latest/)
- [TrenchBroom — The Level Design Book](https://book.leveldesignbook.com/appendix/tools/trenchbroom)
- [HammerForge — duplicatore con offset progressivo](https://github.com/saworbit/hammerforge)
- [Hammer Source: player scale and world dimensions](https://www.worldofleveldesign.com/categories/sourcesdk-authoringtools/hammer-source-player-scale-world-dimensions.php)
- [Modular level kit geometry — Ian London](https://ianlondon.github.io/posts/modular-level-kit-geometry/)
