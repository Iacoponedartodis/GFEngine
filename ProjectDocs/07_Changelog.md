# 07 — Changelog

Dated engineering changes and their architectural effect.

## 2026-08-08 (184) — Editor strutture: comandi tagliati e pannelli non ridimensionabili

Tre segnalazioni, una sola causa di fondo: **avevo scritto gli strumenti condivisi e poi non li
avevo usati proprio qui**.

- **"Centra origine" non si vedeva.** Non era assente: stava sulla stessa riga di un testo che
  riempiva già la larghezza del pannello, quindi veniva spinto oltre il bordo. Ora il testo va **a
  capo** (`TextWrapped`, e più corto) e il pulsante sta su una **riga sua**.
- **La riga di intestazione tagliava.** Nome, categoria, primitiva, Salva, Salva come copia,
  Verifica e la spunta del navmesh: **otto controlli in fila** in un pannello che non li contiene.
  Separati in due righe — identità sopra, comandi sotto. È la regola d'uso già confermata
  dall'utente: *mai far tagliare i comandi*.
- **I pannelli laterali non si ridimensionavano.** Larghezze fisse (320 e 300), quindi un testo
  lungo restava tagliato senza rimedio. Ora usano `editor::ui::panelSplitter` — che esiste dal
  changelog 169 e che **non avevo applicato a questo tab**, pur avendolo scritto proprio per
  chiudere questa classe di difetto.

Nota su di me: è la seconda volta in tre giorni che un componente condiviso esiste e il codice
nuovo non lo adotta. Il framework riduce i difetti solo dove viene usato, e ricordarsene è
disciplina — cioè la cosa che funziona peggio. Vale come voce per il prossimo giro di audit.

## 2026-08-08 (183) — Le parti nascono dove guardi; e l'ORIGINE di un assemblaggio

Due richieste dell'utente che si sono rivelate **lo stesso difetto**.

### La causa comune
`placePartClear` metteva ogni parte nuova **a destra dell'ingombro esistente**, con un metro di
stacco. Risolveva il problema per cui nascevano tutte una dentro l'altra — ma ne creava uno
peggiore: **l'assemblaggio cresceva sempre verso destra**, e il suo centro finiva lontano
dall'origine.

In mappa l'origine è il **perno di rotazione** e il punto in cui compare il **gizmo**. Da qui il
sintomo riferito: *"mi considera il centro della struttura tre metri fuori dalla struttura stessa,
con quindi la rotazione sballata e le freccette del gizmo messe in punti scomodi"*.

### 1. Le parti nascono dove si guarda
Come ogni elemento del Map Editor (che usava già `groundFocusPoint` in 12 punti). Costruendo
attorno all'origine, l'assemblaggio resta centrato da solo — la causa sparisce invece di essere
compensata.

### 2. L'origine, visibile e correggibile
- Una **croce ciano** a terra nel viewport della struttura: un perno invisibile è un concetto
  astratto finché non ci si sbatte contro.
- Sopra l'elenco delle parti, quando serve: *"origine a 3,2 m dal centro della struttura"*.
- **"Centra origine"** sposta tutte le parti insieme. **Non cambia la forma**: cambia dove sta il
  perno. È l'*Origin to Geometry* di Blender, stesso gesto e stesso motivo.

**Esplicito e non automatico al salvataggio**: spostare i dati dell'autore senza che l'abbia chiesto
è il tipo di sorpresa che fa perdere fiducia nello strumento.

### Collaudo
Cinque controlli, di cui il più importante è *"la FORMA non cambia, cambia solo dove sta il perno"*:
un centraggio che deformasse la struttura sarebbe molto peggio del problema che risolve. Più
l'ingombro calcolato **con la rotazione**, che sulle parti ruotate darebbe altrimenti un centro
sbagliato — lo stesso errore che avevo già fatto sulle dimensioni della mappa.

## 2026-08-08 (182) — "Salva come copia": varianti di una composita senza rifarla da zero

Richiesta dell'utente, che ne aveva proposte due forme e ha chiesto di scegliere la migliore.

### Scelta: UNA strada, "Salva come copia" nel tab
L'altra ipotesi era un secondo pulsante *"Modifica come copia"* accanto a "Modifica un tipo".
Scartata perché **due modi per la stessa cosa** sono ciò che l'utente stesso ha già chiesto di
evitare (*"almeno non si fa confusione"*, sul Ctrl+Esc), e perché "Salva come copia" copre
entrambi i momenti:
- **prima** di modificare (apri e salvi subito come copia);
- **dopo** aver modificato — che è il caso in cui serve davvero, perché è l'unico in cui
  l'alternativa sarebbe sovrascrivere l'originale.

Riferimenti: è la stessa scelta di Revit (*Duplicate* sul tipo) e del *Save As* di qualunque
editor; la duplicazione dal browser è una scorciatoia di un passo, non una capacità in più.

### Semantica del salva-con-nome, che è meno ovvia di quanto sembri
- l'**originale su disco non si tocca** — è tutto il punto;
- il **tab passa a lavorare sulla copia**: senza, il salvataggio successivo tornerebbe a
  sovrascrivere l'originale, che è la trappola classica;
- la copia nasce **non verificata**: la sua geometria può già essere diversa da quella verificata, e
  marcarla verificata sarebbe una dichiarazione che nessuno ha controllato;
- la **cronologia di annullamento si azzera**: era dell'originale.

Accanto a **Salva** il suggerimento dice ora **quale file si sta per sovrascrivere** — l'unica difesa
contro il "volevo una variante e ho salvato sull'originale".

### La regola dell'id, condivisa e collaudata
`idFromLabel` è ora una sola funzione (la usavano in due, il primo salvataggio e la copia): due
normalizzazioni diverse porterebbero lo stesso nome a due file diversi. **Sei controlli** nuovi,
fra cui *"due nomi diversi restano due file diversi"* — se collidessero, "Salva come copia"
sovrascriverebbe l'originale invece di affiancarlo, cioè farebbe il danno da cui deve proteggere.

## 2026-08-08 (181) — I campi a zero dicono quanto valgono; e la Y non significa la stessa cosa

### Uno 0 non diceva QUANTO
Molti campi valgono 0 per dire *"usa la misura normativa"*, ma il campo mostrava solo `0`: l'altezza
di una porta non faceva capire che vale 2,80. Ora il formato del campo scrive il valore reale
(`normativo: 2.80`) **senza cambiarlo** — si sa da cosa si parte prima di toccarlo. Usa la
`effectiveParam` introdotta per il gizmo, quindi una verità sola.

### La causa vera delle "piccole differenze" accostando un box a un muro
Non è la lunghezza — quella è esatta: un muro `length = 6` emette un box con `sx = 6`.
**È la Y, che non significa la stessa cosa:**
- un **box** ha la Y al **centro** (`slab` converte base→centro con `y = baseY + sy/2`);
- una **primitiva** (muro, scala, stanza) ha la Y alla **base**;
- una **piattaforma/passerella** ha la Y al **ripiano calpestabile**.

Un box alto 2 m a `y = 0` è **mezzo sottoterra**; un muro a `y = 0` poggia. Tre convenzioni
coesistono per buone ragioni (ognuna è la più naturale per il suo oggetto), ma **nessuna era
scritta nell'interfaccia**.

Ora l'etichetta del campo lo dice — `Y (centro)` · `Y (base)` · `Y (ripiano)` — e sotto è scritto
da dove a dove arriva davvero (`a y=1.00 va da 0.00 a 2.00`).

### Unità: verificate, non affermate
Controllato sul codice: **tutto è in metri**, senza conversioni fra moduli — metriche, righello,
barra di scala, celle del navmesh (0,20 m), raggio dell'unità (0,40 m). Documentato in
`data/help/30_NavmeshEMetriche.md` insieme alle tre convenzioni della Y.

## 2026-08-08 (180) — Pedata, overlay navmesh, e gli ultimi tre moduli scoperti

Tre segnalazioni dall'uso reale, tutte confermate.

### La pedata non si allungava col gizmo
`scalePrimitivePart` rifiutava di scalare un parametro che valesse 0 — e **0 significa
"normativo"**, che è il valore predefinito della pedata. Larghezza (2.0) e dislivello (2.0)
funzionavano, la pedata no: esattamente il sintomo riferito.

Aggiunta `mapstructures::effectiveParam`, che risolve lo 0 nel valore realmente in uso (pedata
0,30 · alzata 0,20 · muro 3,20 · spessore 0,25 · porta …). Ora si parte da lì e si somma il delta,
col clamp al pavimento fisico. Vale per **tutti** i campi "0 = normativo", non solo la pedata.

### Il navmesh non si spegneva più
I triangoli venivano dati al viewport e mai tolti. E il viewport delle strutture è **uno solo**,
condiviso da tutti i tab: cambiando struttura si vedeva il navmesh di quella precedente.

Ora i triangoli vivono nel **tab**, non nel viewport, con un interruttore **"Mostra navmesh"**
(acceso da solo dopo una verifica) e un riporto dell'overlay a ogni disegno. Spegnerlo non perde
l'esito, che resta scritto nel pannello. *Un risultato che non si può togliere smette di essere
un'informazione e diventa un ostacolo.*

### Gli ultimi tre moduli senza avviso di uscita
- **Classi** e **Missioni e obiettivi** non avevano **nessun** rilevamento: le modifiche restavano
  in memoria fino al pulsante "Salva", e chiudere le buttava via in silenzio. Aggiunto, con
  `savePending` che salva la voce del tab corrente.
- **Balance Editor** era già collegato, e il suo stato si sporca solo sui valori di gameplay: armi,
  profili e abilità li scrive **subito**. Quindi lì il silenzio era corretto — ma non era
  distinguibile da un difetto, e adesso lo è.

Il rilevamento di Classi e Missioni è **volutamente prudente**: basta che un campo diventi attivo
per marcare il modulo. Fra un falso *"vuoi salvare?"* (costa un clic) e un falso *"niente da
salvare"* (costa il lavoro), si sceglie l'errore che non fa danni.

### Il controllo che conta i moduli al posto dell'utente
La copertura della guardia era 1 su 5, poi 5 su 7, e **i due mancanti li ha trovati l'utente**.
Ora un controllo automatico verifica che tutti e sette i moduli siano interrogabili, e che senza
modifiche non venga chiesto nulla — *un avviso che compare sempre è un avviso che si impara a
chiudere senza leggere*.

## 2026-08-07 (179) — Composita in mappa: PERDITA DI DATI riparata, e chiusa la classe di difetto

Segnalazione dell'utente: *"ho creato una struttura composita ma una volta salvata se la carico in
una mappa appare solo una scala"*. Erano **due difetti**, e il secondo perdeva dati.

### 1. L'espansione ignorava il tipo (visualizzazione)
Tre chiamate su quattro usavano `expand` invece di `expandInstance`: anteprima, disegno nel viewport
e conteggio dei box in lista. Tutte mostravano la sola **primitiva di base**.
→ Un solo punto, `MapEditor::expandStructureAt`, così la quinta chiamata non possa dimenticarsene.

### 2. Il caricatore dell'editor non rileggeva `type` (PERDITA DI DATI)
L'istanza perdeva il legame con l'assemblaggio, tornava a essere la primitiva nuda, e il
salvataggio **successivo** rendeva la perdita **permanente**. Non un difetto di visualizzazione:
il lavoro spariva.

### La causa vera: DUE lettori
Il campo `type` era stato aggiunto al lettore del registry e non a quello dell'editor — **terza
volta** che un campo nuovo arriva in un lettore su due. La causa non è la distrazione: sono due
lettori.

Introdotto **`mini::structjson`** — `fromJson` / `toJson` in un posto solo, usato **dal registry e
dall'editor**, in lettura e in scrittura. Stesso principio di ADR-018 (un solo gate), ADR-032 (una
sola LOS), ADR-053 (una sola espansione): *una verità sola*. Chi aggiunge un campo ora lo aggiunge
dove entrambi lo vedono.

### Collaudo che avrebbe fermato tutto al primo build
Cinque controlli nuovi, fra cui il **giro salva→ricarica** — che doc 52 indicava come *"l'invariante
più prezioso e oggi non collaudato da nulla"*. Usa la **serializzazione vera**, non una copia
scritta nel test: una copia avrebbe verificato il contratto e non il codice, cioè la trappola del
banco che misura sé stesso, che su questo progetto ha già falsato tre diagnosi.

L'ultimo controllo è letteralmente il sintomo dell'utente: *dopo il giro si espande ancora INTERO*.

## 2026-08-07 (178) — F1 generalizzato; MapEditor NON migrato, con il motivo scritto

`ViewportEditing` lavora ora su una **selezione come insieme**, con `valid(int)` al posto di un
`count` (i codici del Map Editor non sono contigui: sono intervalli negativi per tipo) e un
`anchor(selezione)` per il baricentro del gizmo. L'interfaccia **copre anche il Map Editor**, e il
tab strutture ha guadagnato la predisposizione alla selezione multipla senza scriverne una riga.

Aggiunta una cosa che nessun modulo aveva: i codici diventati **non validi escono da soli** dalla
selezione. Una selezione che punta a ciò che non c'è più è la sorgente naturale degli "ha spostato
l'elemento sbagliato" — cioè del difetto di "Serie" di due giorni fa.

### Il confine, scritto dentro il componente
**Instrada EVENTI, non decide SEMANTICA.** Le operazioni ricevono l'intera selezione e il modulo
decide cosa significhi, perché le tre politiche sono davvero diverse e tutte e tre giuste: il Map
Editor fa **orbitare** il gruppo attorno al baricentro (o un edificio girerebbe sul posto pezzo per
pezzo), il tab strutture gira ogni parte su sé stessa, l'Entity Editor lavora in model space.
Portarle qui dentro ne avrebbe fatto un contenitore di casi particolari — il framework rigido
contro cui mette in guardia ADR-049.

### Perché il Map Editor non è stato migrato
Deliberato, non dimenticato. Il suo percorso del gizmo è ~80 righe di casi particolari maturi,
**funziona**, e **nessun collaudo automatico può confermarne la migrazione** — al contrario
dell'undo, dove un controllo esisteva ed è per quello che è stato migrato per ultimo e senza rischi.

Criterio per farlo, scritto in doc 52: quando esisterà un modo di verificare il gizmo senza mouse.
Fino ad allora il guadagno (meno duplicazione in un modulo) non paga il rischio. Riscrivere codice
funzionante e non verificabile è il modo più elegante di introdurre una regressione — e questa
settimana ne ho già introdotte abbastanza scoperte dall'utente.

## 2026-08-07 (177) — Il Map Editor adotta la pila condivisa: zero implementazioni duplicate di undo

Ultimo passo di F2, e fatto per **ultimo** di proposito: `UndoStack` era stato estratto **da qui**,
quindi migrare il Map Editor non cambia comportamento — cambia chi lo implementa.

`pushUndo`, `doUndo` e `doRedo` sono passate da ~35 righe di logica (coalescenza, taglio del ramo di
ripristino, profondità) a **tre chiamate**. Undici punti che toccavano `m_undo`/`m_redo` a mano ora
ne toccano uno solo.

**Verificato automaticamente**: il controllo *"annulla: riporta al conteggio precedente"* del
self-test esercita proprio il percorso migrato e continua a passare. È il motivo per cui il Map
Editor è stato lasciato per ultimo — era l'unico modulo la cui migrazione un collaudo poteva
confermare da solo.

Una capacità è emersa dall'adozione, come per `rotate(vec3)` due giorni fa: `push` accetta una
**finestra negativa** = *niente coalescenza*. Serve al gancio sui widget, che consegna una fotografia
presa **prima** che il widget diventasse attivo: fonderla col gesto precedente perderebbe uno stato
che l'utente ha davvero attraversato. Un controllo nuovo la copre — **11 controlli sulla pila**.

**Stato di F2: completo.** Tre moduli (Map Editor, tab strutture, Entity Editor) su una sola
implementazione, dove due giorni fa ce n'era una sola in un solo modulo.

## 2026-08-07 (176) — F3/F4: il lavoro non si perde più in NESSUN modulo (doc 52)

### F3 — `DirtyGuard`: il difetto era riparato in un quinto dei casi
**Cinque moduli su sette** tenevano uno stato "modificato"; **uno solo** lo dichiarava. Uscendo da
GFEditor con modifiche in **Entity, Weapon, Vehicle o Balance Editor**, quelle modifiche sparivano
**in silenzio** — mentre per il Map Editor l'avviso c'era da due giorni.

*Un avviso che vale in un posto solo è peggio di nessun avviso*: insegna a fidarsi. Ora l'uscita
interroga tutti i moduli, elenca in chiaro **cosa** non è salvato ("la mappa X, l'entità Y e
un'arma") e salva tutto ciò che sa salvarsi.

Il **Balance Editor** si dichiara **non salvabile a comando**, perché scrive per singola definizione
e non ha un "salva tutto": in quel caso l'uscita **non offre** "Salva ed esci". Una promessa
mantenuta a metà è peggio di un'offerta assente.

I moduli si dichiarano **al momento della domanda**, non registrando callback che sopravvivono a
loro: stessa disciplina di `UndoStack` e `ViewportEditing`, e per lo stesso motivo.

### F4 — `Dialogs`: il modale invisibile diventa inesprimibile
`confirmDestructive`, `saveDiscardCancel`, `errorBox`. La difesa è **nella forma**, non nella
disciplina: `OpenPopup` e `BeginPopupModal` stanno nella **stessa funzione**, quindi nello stesso
livello di ID **per costruzione**. Il difetto del changelog 164 — apertura dentro `BeginTabBar` e
disegno fuori, finestra aperta e mai disegnata, clic bloccati ovunque — non è più "da evitare".

Adottati dall'uscita di GFEditor e dalla chiusura di un tab struttura: i modali scritti a mano in
`EditorApp.cpp` sono passati da 1 a **0**.

Verificato con 48 transizioni fra sei moduli con un tab struttura aperto. Build pulite, self-test
verde, Training Ground 0 problemi.

## 2026-08-07 (175) — Entity Editor guadagna l'ANNULLAMENTO che non aveva mai avuto (doc 52 F2)

### La promessa del framework, verificata
`UndoStack` era stato estratto ieri per il tab strutture. Oggi l'ha adottato **Entity Editor**, che
**non aveva alcun annullamento**: si spostava una zona hitbox o un attach point col gizmo e non
c'era modo di tornare indietro.

Costo dell'adozione: **una dichiarazione, uno `snapshot()`, tre chiamate.** È esattamente la metrica
di successo dichiarata in doc 52 — *"una capacità aggiunta al componente deve comparire in un altro
modulo senza riscriverla"*.

Coperti sia il **gizmo** (fotografia all'inizio del gesto, così un trascinamento intero è una voce
sola) sia i **campi numerici**. E la pila si azzera al cambio di entità: conservarla farebbe
applicare a un'entità lo stato di un'altra — un annullamento che invece di riparare rompe.

### ⚠ Confine di F1 trovato e dichiarato
Tentata anche l'adozione di `ViewportEditing` su Entity Editor, e **respinta con motivo**: lì la
selezione è per **nome** (`"hit:2"`, `"right_hand"`), le coordinate sono in **model space**, e il
picking è su joint/marker proiettati, non sui box del viewport.

Forzarcelo dentro avrebbe richiesto una mappatura indice→nome, cioè reintrodurre l'**identità
posizionale** che KI #100 ci ha insegnato a evitare. **Il confine di F1 è "selezione per indice su
box disegnati"**, ed è meglio dirlo ora che scoprirlo al terzo adottante: un'astrazione che copre
due casi bene vale più di una che ne copre tre male.

L'adozione tentata ha comunque migliorato il componente: `rotateY(int, float)` è diventata
`rotate(int, vec3)` — il viewport fornisce tre assi, e il prossimo adottante non deve dover
modificare il componente per usarli.

## 2026-08-07 (174) — F1/F2: i primi componenti condivisi veri (doc 52, correzione ADR-049)

Cinque difetti segnalati dall'utente, tutti reali — ma il quinto era architetturale e ha guidato
il resto: *"ti avevo detto di creare delle strutture condivise… la viewport del map editor è
evidentemente molto più avanti di quella dell'editor strutture"*.

### L'audit, misurato (doc 52)
`ModuleShell`/`AssetBrowser` **esistevano già** da ADR-049 e sono adottati da **1 modulo su 7**.
VehicleEditor (che li usa) è **349 righe**; MapEditor (che non li usa) ne ha **6258**. L'undo
esisteva in **un solo modulo**. La migrazione si era fermata dopo il pilota.

E ADR-049 conteneva un errore, ora corretto nell'ADR stesso: *"il viewport è già condiviso, non si
tocca"* è vero per la **classe** e falso per la **capacità**. La prova: il ray-picking era già in
`FreeCameraViewport`, e al tab strutture mancava **la riga che lo chiama**.

### F2 — `UndoStack` condiviso
Estratto dalla semantica matura del Map Editor, non inventato: coalescenza per etichetta (un
trascinamento = una voce) e taglio del ramo di ripristino. **Ctrl+Z / Ctrl+Y ora funzionano nel tab
strutture**, con storico proprio.

Progettato **senza callback memorizzate** dopo che il primo abbozzo si è rivelato pericoloso: teneva
`std::function` che catturavano un elemento di `std::vector<StructTab>`, e bastava aprire un altro
tab perché la riallocazione le facesse puntare a memoria morta. Ora lo stato entra ed esce dai
metodi e il componente non possiede riferimenti a nulla.

**10 controlli automatici**, il primo pezzo di framework verificabile senza aprire una finestra —
che è metà della ragione per averlo condiviso.

### F1 — `ViewportEditing`
Selezione (ray-picking), gizmo e applicazione dei delta in un componente solo. Il modulo dichiara
**come si legge e si scrive** il proprio stato con quattro funzioni; non eredita nulla (ADR-049).
`beginGesture` è separata da `move` di proposito: il gizmo produce un delta per frame, e fotografare
a ogni delta riempirebbe la pila di annullamento.

Adottato dal tab strutture: **~50 righe di wiring a mano sostituite da una dichiarazione**. Prossimi
adottanti: EntityEditor, poi MapEditor (per ultimo — si estrae da lui, non si riscrive lui).

### Gli altri quattro difetti
- **Selezione dalla viewport** nel tab strutture: ogni box porta l'indice della sua parte.
- **Metriche sparite**: il pavimento fisico era mostrato solo nel tipo semplice, non nelle parti —
  e senza, un valore che non scende sembra un comando rotto. Rimesso.
- **Pedata non allungabile col gizmo**: `scalePrimitivePart` toccava larghezza e dislivello ma non
  la pedata. Allungarla è sempre lecito, accorciarla resta bloccata dal minimo.
- **Avviso di uscita**: guardava solo la mappa. Ora copre i tab, dice **cosa** non è salvato, e
  "Salva ed esci" salva tutto. *Un avviso che vale in un posto solo è peggio di nessun avviso.*
- **Categorie** libere per i tipi (una nuova compare da sola), Libreria a sottomenu, e il segno
  `[+]` che distingue una composita da un preset di primitiva.

## 2026-08-07 (173) — Assemblaggi componibili: gizmo sulle parti, e la verifica che finalmente resta

Quattro difetti segnalati dall'utente provando davvero il tab, tutti reali.

### 1. "L'ho verificata col navmesh ma non cambia nulla" — DUE cause
- L'esito restava **in memoria**. La Libreria del menu legge dal REGISTRY, che legge dal FILE:
  finché non si salvava (e finché il registry non veniva riletto), il tipo restava giallo.
  `verified` è un **risultato**, non una scelta d'autore: ora la verifica **salva da sola** e
  ricarica la libreria. Chiedere un salvataggio per conservare un risultato è un passo che
  nessuno indovina.
- E il salvataggio **falliva comunque**: `getDataDir() + "structures/"` → `datastructures/`.
  **Quarto bug di percorso della stessa famiglia** (dopo i tre di ieri): `getDataDir()` e
  `datapath::root()` NON hanno lo slash finale. L'errore finiva solo su stderr, quindi
  dall'editor sembrava semplicemente che verificare non servisse a niente.
  Ora un salvataggio fallito **si vede in rosso** accanto al pulsante e il tab resta marcato
  non salvato — un salvataggio che fallisce in silenzio è la classe di difetto peggiore.

### 2. Le parti nascevano tutte nello stesso punto
Una dentro l'altra all'origine: finché non le separavi a mano non si capiva nemmeno quante
fossero. Ora una parte nuova nasce **accanto** all'ingombro attuale, con un metro di stacco.

### 3. Niente gizmo sulle parti
Un assemblaggio non era componibile, era *compilabile*: le parti si potevano solo digitare a
numeri. Ora **1 Sposta / 2 Ruota / 3 Scala** agiscono sulla parte selezionata, come in mappa.

Su un **box** la scala cambia le tre dimensioni. Su una **primitiva** agisce sulle sue
**misure** (larghezza, dislivello, lunghezza), non su un fattore moltiplicativo: scalare una
scala del 30% produrrebbe alzate fuori norma — esattamente l'errore che ADR-053 rende
inesprimibile. Ogni misura resta clampata al suo pavimento fisico.

### 4. Il combo della primitiva confondeva
Su un assemblaggio la "primitiva di base" non governa nulla, ma restando visibile sembrava che
aggiungere una parte la **sostituisse**. Ora sparisce e lascia la scritta ASSEMBLAGGIO. Un
comando che non fa nulla ma cambia aspetto è peggio di un comando assente.

Rimossa anche la scritta "crea un TIPO NUOVO..." dalla voce di menu (superflua).
Guida `data/help/20_Strutture.md` aggiornata nello stesso change set, come da CLAUDE.md §6-bis.

## 2026-08-07 (172) — GUIDA in-editor (F1) + gli assemblaggi resi trovabili (CLAUDE.md §6-bis)

### Il fallimento che ha generato tutto
Gli assemblaggi erano implementati, collaudati con 5 controlli e documentati in ProjectDocs.
L'utente non è riuscito a usarli: *"non ho trovato il modo per fare un assemblaggio… nell'editor
strutture non è cambiato nulla"*. La capacità era dietro un'intestazione **chiusa** che diceva
"Parti (0)".

**Una funzione che l'utente non trova non esiste.** È la stessa lezione di ADR-023 (un dropdown
incompleto rende la capacità inesistente), ed è ora una regola operativa: **CLAUDE.md §6-bis**.

Riparato: sezione **sempre aperta**, con lo stato vuoto che *spiega cosa sono gli assemblaggi e a
cosa servono* invece di mostrare un contatore a zero; pulsanti normali invece di `SmallButton`;
il campo senza etichetta ora dice "Nome del tipo" e spiega dove finirà; la voce di menu è
"Editor strutture: crea un TIPO NUOVO...".

### La guida (menu **Guida**, o **F1**)
Finestra con indice a sinistra e contenuto a destra, ricerca che filtra capitoli **e sezioni**
(una ricerca che poi obbliga a cercare a occhio non ha cercato nulla), e "Ricarica" per rileggere
senza ricompilare.

Contenuti in `data/help/*.md`, uno per capitolo, ordinati dal prefisso numerico: Map Editor ·
Strutture e assemblaggi · Navmesh e metriche · Riga di comando e diagnostica.
**4 capitoli, 23 sezioni.**

**Due documentazioni diverse, che non si sostituiscono**: ProjectDocs spiega **perché** (è per me,
per non ripetere gli errori); `data/help/` spiega **come** (è per chi costruisce, dentro l'editor).

### Tre bug di percorso trovati verificando
`datapath::root()` è la radice di `data/` **senza slash finale**, non la radice del progetto. Le
mie concatenazioni erano sbagliate in tre punti, tutti scritti negli ultimi due giri:
- la guida cercava in `data/data/help` → non avrebbe trovato nulla;
- le preferenze d'aspetto finivano in `dataeditor_appearance.json` → sembravano salvarsi e non
  tornavano più;
- il salvataggio automatico scriveva in `data_autosave/`, una cartella sorella creata per errore.

Trovati perché ho aggiunto un controllo che verifica che la guida **carichi davvero** i suoi file:
una guida vuota sembra un problema di contenuti mancanti, non di cartella sbagliata.
**30 controlli, verdi.**

## 2026-08-06 (171) — C1: ASSEMBLAGGI, strutture fatte di più parti (ADR-056, doc 50 C1)

Il pezzo che mancava. Un **tipo** non è più per forza una primitiva sola: può essere un
**assemblaggio** di parti, e le parti sono **primitive parametriche o box liberi**.

Perché entrambi: le primitive garantiscono le misure (un'alzata sbagliata resta inesprimibile,
ADR-053) ma non esprimono tutto — un contrafforte, un parapetto storto, una feritoia sono box.
Ammettere solo primitive avrebbe reso inesprimibili proprio le *"strutture un po' più complesse"*
che sono il motivo per cui l'assemblaggio esiste.

### La dimostrazione che il sistema serve
Assemblaggio di prova a tre parti: ripiano a 3 m con accesso, parapetto, e un'insegna decorativa.
**21 box** da 3 parti. Esito della verifica sull'insieme:
```
2 componenti, 7.2 / 71.2 m2 raggiungibili, 1 muto -> NON verificata
```
Il ripiano era isolato dalle sue stesse scale. Causa: **l'insegna** (y = 4,6, sopra l'arrivo della
scala) toglieva l'altezza libera — 1,2 m contro i 2,10 richiesti. Alzandola a 6,2:
```
1 componente, 71.2 / 71.2 m2 -> VERIFICATA
```
**Le tre parti erano tutte legali singolarmente.** Solo l'assemblaggio era rotto, e nessun controllo
sui dati poteva vederlo: è esattamente ciò che ADR-056 prometteva — *"l'assemblaggio è il punto in cui
il navmesh si rompe"*.

### Una sola decisione, un solo posto
`mapstructures::expandInstance(inst, type, out)` decide se espandere come assemblaggio o come
primitiva. Ci passano **registry, editor e gate**: un secondo criterio avrebbe fatto divergere
l'anteprima dal gioco, che è il difetto che ADR-018/032/053 esistono per impedire.

### Trappole chiuse
- **Collisione di chiavi JSON**: il discriminatore delle parti è `part`, non `type` — `type` è già
  la semantica del box (`floor`/`wall`/…) **e** l'id del tipo di una struttura. Riusarlo avrebbe
  fatto leggere a `parseGeometryBox` la parola "box" come semantica, cioè box sempre di tipo muro.
- **Le parti si salvano riscrivendo l'intero array**: una fusione campo-per-campo col file su disco
  avrebbe lasciato parti fantasma alla cancellazione. Il RMW continua a proteggere il resto del file.
- **Riferimento a un tipo inesistente**: non passa in silenzio. L'istanza ripiega sulla primitiva
  nuda — che produce una forma diversa da quella vista nell'editor — e il registry lo dichiara.

### Collaudo
Cinque controlli nuovi, fra cui la **convenzione di rotazione**: una parte a (4, 0) locale con
origine (10, 20) e ry 90° deve finire **esattamente** a (10, 16). Col segno sbagliato finirebbe a
z = 24 — dall'altra parte — e a occhio sembrerebbe "quasi giusto". Al primo giro il controllo ha
fallito per una condizione sbagliata **del collaudo stesso**, non del codice. **29 controlli, verdi.**

Nessuna regressione: un tipo senza parti si comporta esattamente come prima (verificato), e
Training Ground resta a 0 problemi.

## 2026-08-06 (170) — Inquadratura riparata + misure sempre in vista (segnalazioni utente)

### "L'inquadratura viene spostata troppo lontana, a volte viene inquadrato il nulla"
Due cause distinte, **entrambe trovate dal collaudo prima che servisse un altro giro di
segnalazioni**:
1. **La vista prospettica non veniva conservata.** Tornando da una vista ortografica, la camera
   restava dove l'aveva messa l'ortografica — centinaia di metri in aria — a inquadrare il nulla,
   senza un modo ovvio di rimettersi a posto. Ora posizione e angoli si salvano all'uscita dalla
   prospettiva e si ripristinano al rientro.
2. **Il contenuto cadeva oltre il piano di taglio.** La camera ortografica veniva messa a
   `dist = 500` con `far = 500`: il contenuto finiva **esattamente** oltre il piano lontano.
   Ora la distanza è modesta (100) e l'intervallo simmetrico `[-far, +far]` tiene dentro ciò che sta
   davanti **e** dietro. In proiezione parallela la distanza non cambia l'immagine — serve solo a
   stare fuori dalla geometria, e confonderla col piano di taglio era l'errore.
3. Corollario: `frameHalfHeightFor` usava l'aspect del **pannello** invece che quello della
   **camera**. Al primo frame (e in collaudo headless) il pannello non è ancora disegnato e valeva
   zero: si inquadrava un rettangolo diverso da quello che si proietta.

**Aggiunto "Inquadra tutto"** (pulsante + tasto **F**), il rimedio che ogni editor 3D ha perché
perdersi è normale, e `setContentBounds` con cui il Map Editor dice al viewport **dov'è la roba** —
cambiare vista senza saperlo è il modo sicuro di inquadrare il vuoto.

**Sei controlli nuovi** nel self-test: gli otto angoli del contenuto devono cadere dentro lo schermo
in Alto/Fronte/Lato, la camera non deve restare lontanissima al ritorno in prospettiva, la proiezione
deve tornare prospettica, e "Inquadra tutto" deve funzionare anche in prospettiva. Al primo giro
**tre fallivano** — è così che le due cause sono venute fuori. **24 controlli totali, verdi.**

### "Misure visibili sulla griglia in maniera chiara evidente"
- **Barra di scala** in basso a sinistra, con la lunghezza scritta ("50 m"), come sulle carte
  geografiche: dice la scala di ciò che si sta guardando **sempre**, senza doverla chiedere. Passi
  "tondi" (1/2/5 × 10ⁿ): un passo di 37,4 m sarebbe corretto e illeggibile.
- **Coordinate ai bordi** con le linee di riferimento, in vista dall'alto: X sopra, Z a sinistra, con
  l'asse Z **ribaltato** perché in vista dall'alto "su" sullo schermo è −Z.
- Compaiono **solo in ortografica**: in prospettiva la scala cambia con la profondità, e una tacca
  "ogni 10 m" sarebbe una bugia. Interruttore **Misure ON/OFF**.
- Disegnate in sovrimpressione con la draw list di ImGui: è testo, e il testo nella scena 3D andrebbe
  ruotato e riscalato a ogni frame.

## 2026-08-06 (169) — AUDIT dell'editor: protezione del lavoro, UI adattiva, strumento aspetto (doc 51)

Richiesta dell'utente: non solo eseguire i suoi esempi, ma **analizzare il progetto e trovare da solo**
cosa manca prima della mappa grande.

### Il rilievo che nessun esempio toccava
**Il Map Editor non aveva né salvataggio automatico né avviso all'uscita.** Chiudere la finestra
buttava via le modifiche non salvate **in silenzio** — e non serviva un crash, bastava la X. Con
KI #98 ancora aperto e una mappa 300 × 200 da costruire in più giorni, era il rischio più grosso
dell'editor.
- Copia di recupero ogni 2 minuti in `_autosave/`, **fuori** da `data/maps/` (dentro sarebbe
  diventata una mappa fantasma nell'elenco). Riusa la stessa serializzazione di `saveMap`, perché un
  secondo scrittore avrebbe salvato qualcosa di diverso dal file vero: un recupero che non recupera.
- La copia **non azzera** lo stato "modificato": non è un salvataggio, e far credere il contrario
  sarebbe peggio del problema.
- Uscita protetta su **ogni** via (X della finestra e voce di menu): Salva ed esci / Esci senza
  salvare / Annulla.

### Scalabilità: misurata invece che temuta
`updateViewport()` ricalcola l'esposizione a ogni modifica — anche a ogni frame durante un
trascinamento — e `buildTacticalLinks` è O(n²). Banco aggiunto al self-test:
**169 → 0,7 ms · 500 → 3,5 ms · 1000 → 9,5-14,6 ms · 1500 → 21-34 ms**.
Verdetto: **non è un blocco** alla densità prevista; lo diventa oltre. Rimedio noto e non urgente
(non ricalcolare durante il trascinamento; poi la griglia spaziale di doc 46 M0-bis).

### Interfaccia
- **`sliderRow` adattiva** (usata in **178 punti**): sotto la larghezza utile mette l'etichetta sopra
  e i controlli sotto invece di tagliare, e misura l'etichetta con `CalcTextSize` invece di assumere
  58 px. In Dear ImGui non esiste layout a vincoli — il ramo va scritto a mano — quindi la leva è il
  widget condiviso, non i moduli.
- **`editor::ui::panelSplitter`** condiviso, e Weapon Editor riparato: `ImGuiChildFlags_ResizeX` mette
  il grip sul bordo destro, che per un pannello di destra è il bordo finestra — una volta stretto non
  c'era più nulla da afferrare. **La riparazione esisteva già nel Map Editor e non era stata
  propagata.** Verificati tutti gli altri usi: sono su pannelli sinistra/centro, dove funziona.
- **Menu Aspetto → Interfaccia**: dimensione testo e densità, effetto immediato, conservate. Si salva
  **solo** questo e non `ImGuiStyle` grezzo: scaricare quella struct non è affidabile fra versioni di
  ImGui (#8659, #101), e preferenze che si rompono a ogni aggiornamento sono peggio di niente.
- **`imgui_demo.cpp` nel build**: non è "la demo", è dove vivono `ShowStyleEditor` e
  `ShowMetricsWindow` — con l'**ID Stack Tool**, lo strumento che diagnostica i conflitti di
  identificatore che ci sono già costati un modale invisibile (changelog 164).

**Dichiarato apertamente**: lo strumento aspetto **non sposta i comandi**. Per "tasti troppo in mezzo"
servirebbe una barra configurabile — feature a sé, da decidere.

## 2026-08-06 (168) — VISTA ORTOGRAFICA + righello libero (doc 50 M3/M4) e ADR-056

### ADR-056 — assemblaggio e prefab sono UN sistema
Decisione approvata dall'utente. Un assemblaggio è un insieme di **parti** (primitive parametriche o
box liberi) con parametri e vincoli propri; il **prefab diventa il caso degenere** senza parametri.
Due sistemi separati avrebbero significato due formati, due editor e due verifiche destinati a
divergere. Limite esplicito preso dalla pratica Revit: **un assemblaggio non può contenerne altri** —
annidare moltiplicherebbe i modi in cui il navmesh si rompe senza che si capisca dove.

### M3 — La vista ortografica (che non esisteva)
`Camera` guadagna un ramo ortografico (`setOrthographic`, `setOrthoHalfHeight`), e il viewport quattro
modi: **Prosp / Alto / Fronte / Lato**. In prospettiva una lunghezza sullo schermo **non** corrisponde
a una lunghezza nel mondo: si stima, non si misura. È il motivo per cui il righello di Unreal funziona
solo in ortografica e per cui Hammer/Radiant lavorano su viste ortografiche.

In ortografica il tasto destro **sposta** invece di ruotare (ruotare una vista assiale la disallinea
dagli assi e le toglie l'unica cosa per cui esiste) e la rotella cambia l'inquadratura, mostrata in
metri.

**Due trappole trovate e chiuse prima che mordessero:**
- `panCamera` faceva `cross(forward, {0,1,0})`: guardando **dritto in basso** è il vettore nullo, e
  normalizzarlo dà NaN — la camera sarebbe sparita alla prima trascinata nella vista dall'alto.
- Il piano vicino della proiezione ortografica è **negativo** di proposito: altrimenti tutto ciò che
  sta *sopra* la camera verrebbe tagliato, e guardando una mappa dall'alto sparirebbe.

**Verificata sulla matematica**, visto che non posso guardare lo schermo: 7 controlli nuovi nel
self-test — niente NaN, il centro del mondo al centro dello schermo, la scala che rispetta l'aspect,
−Z in alto, **la distanza che non cambia con la quota** (il senso stesso della vista), ciò che sta
sopra la camera che resta visibile, più la controprova in prospettiva. **18 controlli totali, verdi.**

### M4 — Righello libero
Due clic sul terreno, con aggancio alla **stessa** griglia con cui si costruisce, misura in tempo
reale mentre si cerca il secondo punto, e il confronto normativo detto (*"sotto il corridoio (2,40)"*)
invece di lasciato a mente. Disegnato **senza test di profondità**: una misura che sparisce dietro un
muro non misura niente.

Il righello che c'era misurava solo fra **due elementi selezionati**, quindi non poteva misurare uno
spazio **vuoto** — cioè il caso vero: la larghezza di un varco, la luce di un passaggio.
`screenToPlane` sproietta la matrice e vale in entrambe le proiezioni; i segni sono stati verificati
invertendo la `projectToScreen` già usata dal picking.

## 2026-08-06 (167) — Misure: dimensioni della mappa e ingombro della selezione (doc 50 M1/M2)

L'utente: *"hai messo le figure di scala, ma sono scomode, mi serve un modo per sapere le dimensioni
tipo della mappa in maniera facile e chiara"*. Ha ragione: le figure di scala sono un **surrogato** —
dicono "circa due metri", non "questo corridoio è 3,40".

- **M1 — ingombro della mappa sempre in vista**, in cima al pannello sinistro: X × Z e l'escursione
  di quota, su box a mano **più** quelli generati dalle primitive.
- **M2 — ingombro della selezione**: larghezza × profondità × altezza del gruppo, con l'avviso quando
  il lato minore scende sotto la misura del corridoio.

### La rotazione conta, e me ne ero già dimenticato una volta
La prima versione di M1 ignorava `ry`. Su Training Ground — che ha due passerelle da 90 m ruotate —
questo dà **154,9 × 91,9** invece di **71,3 × 92,4**: è *esattamente* il numero sbagliato che avevo
riportato all'utente settimane fa. Corretta con l'AABB completo di un box ruotato attorno a Y, e
**verificata per controincrocio**: calcolo indipendente sul JSON = 71,3 × 92,4, uguale al valore
ricavato dai limiti del navmesh. Tre fonti concordi.

Un dato che nessuno mostra è un dato che si sbaglia — l'avevo sbagliato io due volte.

### Ricerca e piano (doc 50)
Confronti: **Unreal Measuring Tool** (aggancio alla griglia, e funziona **solo in ortografica** —
in prospettiva non si misura, si stima), **Hammer** (viste ortografiche + griglia in unità dichiarate),
**blocchi dinamici AutoCAD** (parametri + azioni + vincoli + elenco di valori ammessi),
**famiglie annidate Revit** (con l'avvertenza esplicita a *non* eccedere con l'annidamento),
letteratura sul **blockout modulare** (90°/45°, incrementi di griglia grandi per le stanze e piccoli
per la rifinitura).

Rilievi principali: **non esiste una camera ortografica** (`Camera::getProjection` è solo
`glm::perspective`) e il righello esistente misura **solo fra due elementi selezionati**, quindi non
può misurare uno spazio vuoto — che è il caso vero. Piano a fasi in doc 50.

## 2026-08-06 (166) — KI #98: non riprodotto, ma la rete di diagnosi ora funziona davvero

Obiettivo: chiudere il crash di Entity Editor. **Non chiuso** — non si riproduce. Ma la posizione è
cambiata: da "crash illeggibile" a "crash che al prossimo colpo si racconta da solo".

### Il difetto del mio metodo, prima del difetto del codice
Tutte le riproduzioni automatiche fatte finora aprivano Entity Editor con `m_sel = -1`, cioè **senza
alcun modello caricato**: collaudavo una viewport vuota, non lo scenario dell'utente. Aggiunto
`--entity <id|indice>`, che seleziona all'avvio e fa girare mesh + rig + ossa + arma in mano.
Con il B1 Battle Droid (42 primitive) e l'arma: **7 transizioni fra moduli, nessun crash**, e
**nessun errore ASan** nemmeno in Debug+ASan sullo stesso percorso.

### La FASE nel crash report
`mini::telemetry::setPhase()` / `ScopedPhase` — costo una store di puntatore. Uno stack trace dice
**dove si è fermato il processore**, non **cosa stava facendo il programma**; e se il crash avviene
dentro la DLL del driver grafico, il "dove" non ha nemmeno nomi nostri. Marcate: cambio modulo, tick
e draw di ciascun modulo.

**Verificata, non dichiarata**: `--crash-test` provoca un access violation dentro una fase nota.
```
fase:   crash-test volontario
#8  main(int, int) at ...\editor\src\main.cpp:44
```
dove il report originale di KI #98 diceva `#9 0x00007ff75ff533a0 in ??`. Un miglioramento al crash
reporting che non si è mai visto funzionare è una speranza, non uno strumento.

### Due guardie e un accesso fuori limite reale
Con gli array client-side (ADR-003) è il **driver** a leggere la nostra memoria durante
`glDrawArrays`: un conteggio di vertici maggiore dei dati diventa una lettura oltre il limite
**dentro la DLL del driver**, che ASan non può vedere. Ora `FreeCameraViewport::drawArray` e
`Mesh::draw` **rifiutano il disegno** e lo dicono.

E un accesso fuori limite vero, riparato (non dimostrato essere questo crash):
`FreeCameraViewport::loadModel` faceva `raw.data() + i*11` fidandosi di `getVertexCount()`, che è
memorizzato **separatamente** dai dati — un'invariante che oggi regge per costruzione ma che nessuno
impone, e che `Mesh(vector<float>, int)` lascia interamente al chiamante.

## 2026-08-06 (165) — Guardia sui tetti (R1) + liste raggruppate: chiuso il terzo dei tre lavori

### R1 — La guardia sui tetti dei codici (KI #100)
Ci sono **26 punti** che creano elementi: una guardia in ciascuno significa dimenticarne uno, ed è
proprio la dimenticanza il difetto che si vuole evitare. Quindi **un controllo solo**,
`capacityReport()`, valutato a ogni frame — che copre anche il caricamento da disco e la
duplicazione, cioè le strade da cui una mappa può arrivare già oltre il tetto.

Compare **solo sopra l'80%**: un avviso sempre acceso diventa arredamento. Al tetto diventa rosso.

Tre controlli nuovi nel self-test impediscono alla tabella dei tetti di divergere dagli intervalli
veri: verificano che l'ultimo indice lecito risolva ancora, che il tetto dichiarato coincida con
l'inizio dell'intervallo successivo, e che il superamento venga segnalato. **11 controlli, tutti verdi.**

### Liste del Map Editor raggruppate (terzo lavoro chiesto il 2026-08-05)
- **Box per tipo** — Pavimenti / Muri / Piattaforme / Coperture / Decorazioni, con il conteggio
  nell'intestazione. Le categorie sono quelle di `BoxType` e dei filtri di Vista: un secondo
  vocabolario per dire le stesse cose farebbe solo confusione.
- **Filtro per nome** sopra la lista: quando sai come si chiama, la strada più corta non è la
  categoria giusta. Col filtro attivo le categorie **si aprono da sole** — cercare e poi dover
  anche aprire il cassetto vanificherebbe la ricerca — e i conteggi sono quelli filtrati, altrimenti
  l'intestazione promette righe che non ci sono.
- **Posizioni tattiche per ruolo**, con i ruoli ricavati dai **dati** e non da un elenco fisso: un
  ruolo nuovo compare da solo invece di finire in un "altro" che nessuno guarda. Nell'intestazione
  anche il numero di posizioni **cieche** del gruppo, così si vede da fuori dove c'è un problema
  senza aprirli tutti.

Su Training Ground sono 167 box e 169 posizioni; sulla mappa 300 × 200 saranno molte di più.

## 2026-08-05 (164) — Fermata per stabilità: tre difetti chiusi + il primo collaudo headless (doc 49)

L'utente ha fermato la costruzione: *"penso che abbiamo toccato qualcosa che ha rotto la stabilità
dell'editor … costruire adesso potrebbe peggiorare le cose"*. Aveva ragione su due difetti su tre, e
la terza intuizione — che il problema fosse **strutturale** — è quella giusta (analisi in doc 49).

### I tre difetti
1. **"Serie" spostava un elemento SBAGLIATO** (preesistente, `bc9a281`). `duplicateBox` inserisce la
   copia *accanto* all'originale, ma `makeArray` deduceva il codice della copia con *"sarà l'ultima
   del vettore"*: offset e rotazione finivano su un'altra box, che veniva trascinata via **in
   silenzio**. Stesso errore in `duplicateSelected`, con un commento che affermava il contrario.
   → `duplicateOne` ora **restituisce** il codice della copia, e i codici raccolti prima del ciclo si
   aggiornano dopo ogni inserimento.
2. **Modale invisibile che blocca i clic** (mio, changelog 163). `OpenPopup` dentro `BeginTabBar`
   (che spinge un livello di ID, `imgui_widgets.cpp:9849`) e `BeginPopupModal` fuori: due ID diversi,
   finestra aperta e mai disegnata. Tranello noto di ImGui (issue #331). Nella stessa riparazione:
   la modale agiva sul tab **attivo** invece che su quello in chiusura.
3. **Viewport della mappa che poteva congelarsi** (mio, changelog 163). `m_activeTab` resta al valore
   vecchio nei frame in cui i `BeginTabItem` non girano; puntato a un tab inesistente, la viewport
   della mappa non avanzava e **non rilasciava il mouse** — il sintomo *"non riesco più a cambiare
   modulo, torno alla home"*.

### `--editor-selftest`
Sette controlli sulle operazioni (duplica, serie, serie con rotazione, annulla) su uno stato
sintetico, senza finestra e senza frame. Include *"serie non sposta elementi fuori dalla selezione"*,
cioè esattamente il difetto 1. Riferimenti: *Edit Mode tests* di Unity, *Automation Framework* di
Unreal — entrambi collaudano **l'editor**, non solo il gioco.

Non è vuoto: al primo giro ha fallito su un difetto vero **del banco di prova** (un `resize` che
lasciava in testa le copie del caso precedente — lo stesso errore che ha già falsato tre diagnosi su
questo progetto). Ora ricostruisce lo stato da zero prima di ogni caso.

### ⚠ Il rilievo che riguarda la prossima mappa
L'identità di un elemento è **la sua posizione in un array**, dentro un `int` il cui significato
dipende dall'intervallo. Ne derivano **tetti silenziosi**: 1.000 posizioni tattiche, 1.000 settori,
100 percorsi, 90 command post. Superarli non dà errore — il codice **cambia significato**.
Training Ground ha 169 posizioni su ~71 × 92 m; la prossima è **300 × 200**, nove volte l'area, con
l'obiettivo dichiarato di *"moltissimi metadata"*. Nessun controllo lo segnala oggi (doc 49 R1).

## 2026-08-05 (163) — EDITOR STRUTTURE: un tab, non un modulo; i tipi hanno vincoli (ADR-055, doc 48)

Secondo dei tre lavori sugli strumenti. Vincolo esplicito dell'utente sulla forma: *"che apre un
altro tab nel map editor, tipo i tab di google, così non apre un altro modulo, ma posso rimanere in
map editor avendo una viewport separata"*.

### Ricerca prima di decidere
- **Revit, famiglie**: *i parametri di tipo e di istanza si cambiano senza aprire la famiglia*. È la
  spina dorsale adottata — il **tipo** dichiara quali misure esistono e con che limiti, l'**istanza**
  in mappa ne fissa i valori.
- **Unity, Prefab Mode**: si entra nella definizione **da dove la si usa**, si edita in isolamento,
  e all'uscita c'è un contratto esplicito sulle modifiche non salvate. Da qui il tasto in fondo al
  menu `+ Struttura` e il popup salva/scarta alla chiusura del tab.
- **AutoCAD, REFEDIT**: ridefinire un blocco ridefinisce **tutte** le sue inserzioni. Da qui il
  contatore *"usato da N strutture in questa mappa"*, mostrato **prima** della modifica.

### Cosa esiste ora
Barra tab nel Map Editor (`Mappa` + tipi aperti, chiudibili), **viewport separata** che mostra la
struttura da sola con la figura di scala accanto, e un nuovo tipo di definizione
`data/structures/<id>.json` (id = filename stem). Per ogni misura si decide se sarà **modificabile**
in mappa e fra quali **min/max**.

**Il pavimento fisico non è autorabile** (ADR-055): `minWidthFor`, il clamp di `STEP_HEIGHT` e di
`STAIR_TREAD` restano nel codice. Un tipo può essere più severo, mai più permissivo; un `min` sotto
il pavimento viene alzato e l'editor lo dichiara in chiaro. Renderlo autorabile avrebbe restituito
all'autore esattamente l'errore che ADR-053 gli aveva tolto.

**Fallback**: un'istanza senza `type` si comporta come prima, e il campo si scrive solo se c'è —
nessuna mappa esistente cambia di un byte.

### Osservabilità, verificata sul campo (non dichiarata)
Il pannello dà il **sintomo** (`superficie persa %`), il **funnel** (box → triangoli → componenti) e
la **singola entità** (quale box resta muto). Provato con una coppia di casi opposti:
- piattaforma a 3 m **senza accessi** → `0.0/36.0 m² raggiungibili, 2 componenti, 1 muto` → **NON
  verificata**;
- stessa piattaforma **con un accesso** → `16 box, 43.2/43.2 m², 1 componente` → **verificata**;
- scala normativa → `15 box, 10.8/10.8 m²` — e 15 × 2,4 × 0,30 = 10,8 esatti.

Un validatore che dice sempre "va bene" non vale nulla: il caso negativo è stato costruito apposta.

### Tre difetti trovati provando, non leggendo
1. **Muri e barricate non sarebbero mai stati verificabili.** Non dichiarano superficie calpestabile,
   e la prima regola (`superficie tutta raggiungibile`) li avrebbe marcati non verificati per sempre.
   Per un ostacolo la domanda giusta è l'opposta: *non ostruisce tutto?* E il numero di componenti
   non è un difetto — un muro che divide il piano sta facendo il suo mestiere.
2. **Il banco di prova stava per inventare il risultato.** Il piano d'appoggio era messo sotto il
   punto più basso della struttura: per una piattaforma sospesa a 3 m gli si incollava sotto,
   rendendola raggiungibile per finta. Ora sta alla **base** (`min(0, minY)`).
3. **`width` sulla piattaforma era un comando che non fa nulla**: l'espansione fissa le scale
   d'accesso a `STAIR_MIN_WIDTH` e ignora il campo. Tolto dall'elenco invece di lasciarlo mentire.
   Renderlo efficace cambierebbe la geometria delle piattaforme già in mappa — decisione dell'utente,
   non effetto collaterale.

### `--struct-tab <id>`
Apre un tab struttura all'avvio ed esegue la verifica, stampandone l'esito. Serve a **eseguire** un
percorso di UI che altrimenti vive solo dietro un clic: è così che i tre difetti sopra sono venuti
fuori, invece di essere scoperti in mano all'utente (la lezione di KI #98).

## 2026-08-05 (162) — Crash di Entity Editor: riprodotto, non risolto; e gli strumenti che mancavano

Interruzione richiesta dall'utente: *"provare ad aprire entity editor fa crashare GFEditor"*. Il crash
è **ancora aperto** (KI #98). Quello che è cambiato è che ora è **indagabile**, e non lo era.

### Il problema dietro il problema
Un crash di modulo si riproduceva solo col mouse, e la traccia che produceva era illeggibile:
```
#9  0x00007ff75ff533a0 in  ??
```
Il build **Release non generava PDB**. Cioè: l'unica configurazione in cui questo crash si presenta
era anche l'unica in cui non si poteva sapere *dove* fosse. Due strumenti mancanti, entrambi chiusi:

- **`GFEditor.exe --module <a,b,c>` (+ `--module-frames N`)** — apre i moduli indicati senza passare
  dal mouse. La forma con la **lista** è quella che conta: quasi nessun difetto di modulo sta
  nell'apertura, sta nel **passaggio** da un modulo all'altro con lo stato che il precedente ha
  lasciato. È così che il crash è stato riprodotto, ed è ciò che ha permesso 240 transizioni
  automatiche in un colpo solo.
- **PDB anche in Release** (`/Zi` + `/DEBUG`, con `/OPT:REF /OPT:ICF` a rimettere le ottimizzazioni
  di link). Il binario resta quello di prima, i simboli vivono a parte, il costo a runtime è zero.
  Il prossimo `crash_report.txt` conterrà i **nomi delle funzioni**.

### Cosa si sa del crash
Riprodotto **una volta** (`--module home,weapon,entity`, Release, frame 480 = il frame esatto della
transizione). Dopo la ricompilazione **non si riproduce più**: 6 esecuzioni e un martellamento di 240
transizioni, pulite. È quindi **sensibile alla disposizione in memoria** — mascherato, non risolto.

**ASan non lo rileva**, né in Debug né in **Release+ASan**. Non è un'assoluzione ma un indizio: la
lettura illecita non avviene nel nostro codice compilato. Il candidato coerente con tutti e tre i
fatti (solo Release, invisibile ad ASan, crash dentro un modulo esterno) è una lettura del **driver
OpenGL** su un array *client-side* (ADR-003) — ASan strumenta il nostro codice, non la DLL del driver.

Scagionati con verifica, non per esclusione: loader idempotenti di `DefinitionRegistry` (ciascuno
azzera solo il proprio contenitore), puntatori nel registry conservati da EntityEditor (nessuno),
membri non inizializzati (nessuno), contatori di vertici del viewport (tutti derivati da `size()/6`,
non possono eccedere i dati), igiene degli attributi in `drawArray` e `Mesh::draw` (corretta).

### Riparato per strada
`FreeCameraViewport::resizeFBO` verificava otto puntatori a funzione OpenGL e poi ne **chiamava altri
due mai verificati** (`s_delFBO`, `s_delRBO`) — mentre il distruttore li protegge, segno che nulli
possono esserlo. Un puntatore a funzione nullo chiamato produce lo stesso identico access violation
che stavamo cercando. Difetto reale, chiuso; **non dimostrato essere questo crash**.

### Filtro vista per i marcatori (richiesta utente)
*"guardare il navmesh con tutte quelle cose colorate in mezzo diventa difficile"*. In **Vista** ora ci
sono quattro spunte separate dalla geometria — posizioni tattiche, settori e zone di pericolo,
percorsi, punti di gioco — e il pulsante **"Solo geometria"** che le spegne tutte in un colpo: su
Training Ground sono 169 posizioni più settori e percorsi davanti alle superfici che l'overlay deve
mostrare. L'asterisco di "filtri attivi" tiene conto anche di queste, così una mappa che sembra vuota
si spiega da sé.

## 2026-08-05 (161) — VALIDAZIONE NAVMESH nell'editor: si vede dove si cammina davvero (ADR-054)

Primo dei tre lavori chiesti dall'utente sugli strumenti, e il suo abilitante: *"la validazione
navmesh è importante, perché le mappe non saranno tutte strutture già testate, quindi poter vedere da
solo cosa è problematico mi aiuta"*.

### Ricerca prima di decidere
Recast fornisce già `DebugUtils` e, nel suo demo, un `NavMeshTesterTool`; Unreal mostra il navmesh nel
viewport (tasto **P**) e lo si collauda mandandoci un agente. Il pattern comune è chiaro: **si guarda
il navmesh, non lo si deduce**. Da lì la forma della funzione.

### La decisione (ADR-054): l'editor costruisce il navmesh VERO
Il GFEditor ora linka **`NavManager` + Recast/Detour** e costruisce il navmesh con **lo stesso codice
del gioco**, sullo stato in editing — box a mano **più** i box generati dalle primitive.

Niente approssimazioni "economiche" per l'editor: è lo stesso principio di ADR-018 (stesso gate) e
ADR-032 (una sola `hasLineOfFire`). Il verso opposto di ADR-002 è già la norma.

> La spunta **"Area navigabile"** è stata **rimossa**: coloriva i box di tipo `floor`, cioè
> l'*intenzione* dell'autore, non ciò che l'AI può calpestare. Mostrava una cosa e ne suggeriva
> un'altra — meglio niente che un indicatore che mente.

### Cosa si vede ora
**Verde** = ci si arriva dallo spawn. **Rosso** = isola. Più un pannello con poligoni, isole, tempo di
costruzione, e l'elenco **cliccabile** degli elementi che il navmesh non raggiunge (post e posizioni):
si passa da *"c'è un problema"* a *"guarda questo"*.

Il risultato **invecchia da solo**: un'impronta della geometria ricalcolata a ogni frame marca la
verifica come da rifare. Un flag da alzare a mano nei venti punti che modificano la mappa prima o poi
resta basso, e mostrare un navmesh stantio come buono è peggio che non mostrarlo.

### Due metodi nuovi su `NavManager`, che servono anche al runtime
`debugTriangles()` (poligoni + **componente connessa**) e `componentAt(p)`. La componente connessa
**è** il `componentId` che doc 46 M1 vuole come dato di primo livello: nasce qui, una sola
implementazione.

### Verificato per CONTROINCROCIO, non per fiducia
Il conteggio delle posizioni irraggiungibili su Training Ground dà **1 con `isReachable`** (path
Detour) e **1 con il confronto di componente** (analisi del grafo). Due metodi indipendenti, stesso
verdetto — ed entrambi ora in telemetria, così una divergenza futura si legge da un numero invece che
da mezza giornata d'indagine.

### E ha già trovato qualcosa che non sapevamo
Su Training Ground: **13 componenti connesse** — cioè **12 isole** — e **136 triangoli su 2173**
fuori dalla zona raggiungibile dallo spawn. Una è il recinto Droid CT (KI #97); le altre **undici non
le conosceva nessuno**, perché solo una di esse ospita una posizione tattica e quindi era l'unica che
le sonde segnalavano.

**Build-verified** Release e Debug, 0 warning. Training Ground: `--validate` 0 problemi, 1041
poligoni, 6000 tick con 0 ERROR e 0 stalli.
**Da verificare a mano**: il pulsante "Verifica navmesh", l'overlay verde/rosso nel viewport e
l'elenco cliccabile.

**Restano** gli altri due lavori chiesti: l'**editor strutture** come *tab* dentro il Map Editor
(e poi lo stesso per i prefab), e l'**ordinamento in categorie** delle liste.

---

## 2026-08-05 (160) — G8: Training Ground non andava riparata, ma gli strumenti hanno trovato ciò che nessun dato vedeva

Ultima fase di doc 47. Il suo presupposto originale — *"le sue scale sono sbagliate, rifalle con la
primitiva"* — **era caduto** con la correzione del gate (changelog 147): `--validate` dà 0 problemi e
tutti e 5 i command post sono raggiungibili. Quindi G8 è diventata un'altra cosa: **mettere alla
prova gli strumenti su una mappa che conosciamo**.

### Due correzioni applicate, piccole e verificate
- **`Aplha` → `Alpha`.** Non era solo un refuso estetico: gli obiettivi `capture_alpha` e
  `hold_alpha` cercano `"post": "Alpha"`, quindi su questa mappa il riferimento sarebbe stato
  **rotto in silenzio**. (Oggi la sola missione esistente punta a `firebase`, che il post giusto ce
  l'ha — era una trappola armata, non un guasto in corso.)
- **Posizione tattica #166 spostata di 0,45 m verso l'interno.** Stava a **26 cm** dal bordo del
  ripiano, dentro la fascia che il navmesh erode (0,40): irraggiungibile per una questione di
  centimetri.

### E una diagnosi che vale più delle correzioni (KI #97)
Sondando ho scoperto che **l'intero recinto "Droid CT" è irraggiungibile sopra il suolo** — non solo
quella posizione. Dal primo gradino (0,44) al ripiano (2,59), **nessuno** è raggiungibile; il suolo
dentro il recinto invece sì, la porta funziona.

E il punto che conta: **`--validate` dice 0 problemi**. Per i dati la scala è perfetta — alzate
0,10-0,20 m, tutte ben sotto il massimo. È il navmesh a non costruirla, per **tre cause sovrapposte**:
rampe larghe 1,50 m (sotto l'area minima di regione dopo cigli ed erosione), una giunzione fra due
gradini di **3 cm**, e un muro che **attraversa la scala** con un varco di 17 cm.

Verificato che non basta togliere una causa: aperto il muro su una copia di lavoro, resta chiuso.

### Non l'ho riparato, e perché
Rifare quella scala significa ridisegnare un angolo della mappa — rampe più larghe **e** un varco nel
muro trasversale — e sono scelte di design. Ho provato il solo allargamento dei gradini (11 box) e
l'ho **annullato**: da solo non sblocca nulla e cambia l'aspetto. In KI #97 c'è la ricetta esatta per
quando vorrai farlo, con la primitiva **Scala** che rende inesprimibili tutte e tre le cause.
Impatto reale basso: nel recinto non spawna nessuno e non ci sono obiettivi.

### Il vero esito di G8
> Gli strumenti hanno trovato, su una mappa "sana" e validata, un difetto che **nessun controllo sui
> dati può vedere** — e lo hanno localizzato gradino per gradino. È esattamente ciò per cui la
> sonda di raggiungibilità esiste, e la conferma sul campo di `ELEVATED_MIN_SPAN`: la stessa
> aritmetica derivata in laboratorio spiega un difetto reale, autorato mesi fa.

**Verificato**: `--validate` 0 problemi · 5/5 command post raggiungibili · 1/169 posizione
irraggiungibile (#166, dentro il recinto di KI #97) · 6000 tick con **0 eventi ERROR e 0 stalli**
(fermo massimo 1,0 s) · geometria **identica all'originale** (0 box modificati), 1 sola posizione
spostata. Mappe di prova cancellate.

**Con G8 il piano di doc 47 è COMPLETO (G1-G8).** Gli strumenti ci sono: si può costruire la
300 × 200.

---

## 2026-08-05 (159) — Il gizmo agiva sull'oggetto sbagliato; e i minimi diventano una regola

Quattro segnalazioni dell'utente, tutte centrate.

### Bug: creando una struttura, il gizmo si disegnava sulla nuova e MUOVEVA la vecchia
`addStructure` (e `addBox`) scrivevano `m_selStruct`/`m_selBox` **a mano**, lasciando vivo
`m_multiSel` con la selezione precedente. Il bersaglio del gizmo guarda il primario, ma
`applyMove` itera `selectionCodes()` — cioè l'insieme, ancora quello vecchio.

Corretto passando da **`setSelection`**, che cambia primario e insieme **insieme**. È esattamente il
motivo per cui quella funzione esiste: due strade per la stessa cosa e una si dimentica un pezzo.

### La regola sui minimi, come l'ha formulata l'utente
> *"devo poter modificare le grandezze che non rompono quella struttura"*

Recepita alla lettera: si **clampa solo ciò che romperebbe** la struttura, e tutto il resto resta
libero (allungare una passerella o un muro non ha limiti; stringerli sotto la soglia sì).

E i minimi non sono scelti a occhio — si **derivano dai filtri del navmesh**. Nuova metrica
**`ELEVATED_MIN_SPAN = 3,00 m`**, con il conto verificato: una superficie in quota perde una cella
per lato allo sfoltimento dei cigli, `AGENT_RADIUS` (0,40) per lato all'erosione, e sotto ~2,56 m² la
regione viene scartata del tutto. Per un ripiano quadrato di lato *s* resta (s − 1,20)² > 2,56
→ **s ≥ 2,80**; arrotondato a 3,00.

| struttura | clampato | libero |
|---|---|---|
| scala / rampa | larghezza ≥ 1,60 · alzata ≤ 0,55 · pedata ≥ 0,30 | dislivello, lunghezza |
| muro / porta | spessore ≥ 0,20 (sotto la cella del navmesh non è un ostacolo continuo) | lunghezza, altezza |
| **porta** | ≥ **1,80 × 2,40** (ci passa il gigante) | — |
| finestra | ≥ 0,40 × 0,30 (non si attraversa: non serve il gigante) | parapetto |
| stanza | interno ≥ corridoio (2,40) + spessore muri | tutto il resto |
| **piattaforma** | lato ≥ **3,00** | quota |
| **passerella** | larghezza ≥ **2,40** (è un corridoio in quota) | **lunghezza** |

### Porta ↔ finestra come SCELTA, non come numero
Erano due cose diverse — una si attraversa, l'altra è un riparo da cui sporgersi — travestite da
"metti un valore nel parapetto". Ora sono due pulsanti: passando a *Finestra* arrivano il parapetto
alla quota di copertura bassa e misure sensate; tornando a *Porta*, le misure normative. E i minimi
cambiano con la scelta, perché a una finestra non serve farci passare il gigante.

### Verificato che i clamp SALVINO davvero
Mappa di prova con **tutto chiesto sotto misura**: piattaforma a 1,5 m di lato, passerella a 1,0 m,
stanza da 1,0 × 1,0, porta 0,4 × 1,0. Tutte e tre le mete **raggiungibili** dopo il clamp — senza,
sarebbero sparite dal navmesh restando perfette nei dati.

**Build-verified** Release e Debug, 0 warning. Training Ground invariata (1043 poligoni, 5/5 command
post, `--validate` 0 problemi). Mappa di prova cancellata.

---

## 2026-08-05 (158) — Il vano scala: cinque difetti veri trovati, e la decisione di NON consegnarlo

L'utente ha segnalato che la scala con pianerottolo *"non sarebbe funzionale"*: la seconda rampa
messa **di profilo** contro il pianerottolo, senza spazio per salire il primo gradino, e soprattutto
**non componibile** — *"non sembrano comode da combinare per ottenere le scale interne di una torre"*.
Aveva ragione su entrambi i punti, e il secondo era quello importante.

### Riprogettata come VANO SCALA
Non "due rampanti" ma **quante rampe servono**, dentro una pianta che **non cambia con l'altezza** —
è ciò che serve per una torre. Due corsie affiancate, rampe pari in salita su una e dispari in
discesa sull'altra, pianerottoli quadrati che coprono entrambe.

### Cinque difetti veri, trovati misurando
Ognuno reale, ognuno **insufficiente da solo**:
1. il pianerottolo deve coprire **entrambe** le corsie (2w), o l'erosione del navmesh le stacca di
   0,80 m — la salita si fermava alla prima rampa;
2. ma **non** deve essere profondo il doppio all'indietro: seppelliva gli ultimi gradini della rampa
   in arrivo sotto il proprio ripiano, creando un salto di 0,60 m;
3. le rampe vanno lasciate **sospese**: renderle piene dal suolo faceva sì che la rampa di ritorno
   murasse il pianerottolo da cui parte;
4. il muro d'anima va **solo fra le rampe**, mai dentro i pianerottoli — lì si attraversa da una
   corsia all'altra, e un muro che li taglia chiude proprio il passaggio;
5. la corsia vuole larghezza da **corridoio (2,40)**, non da scala (1,60): misurato, una torre a tre
   rampe da 1,60 si interrompe alla terza, la stessa a 2,40 arriva in cima.

Il punto 5 è diventato una metrica: **`STAIRWELL_MIN_WIDTH`**, con il clamp che rende il difetto
inesprimibile come già per l'alzata.

### La decisione: fuori dal menu, non consegnato
Verifica finale su sei torri (4/8/12/20 m, rotazioni 0/90/215): **tre percorribili fino in cima,
tre no**. E il gate sui dati dice **0 problemi**, perché il difetto nasce nella **voxelizzazione**,
non nella geometria dichiarata — quindi nemmeno l'autore se ne accorgerebbe prima di provare.

> Consegnare una primitiva che produce **in silenzio** torri non percorribili è l'esatto contrario
> della promessa di ADR-053 ("il difetto diventa inesprimibile"). Meglio non averla.

Il codice resta, con tutti e cinque i rimedi e i tre casi ancora aperti annotati sul posto (due sole
rampe, molte rampe, rotazioni non ortogonali). Vanno affrontati con una **passata dedicata**, non a
tentativi. Nel frattempo una torre si costruisce con `platform` + `stair` per livello, che sono
verificate.

**La libreria consegnata è di OTTO primitive**: scala, rampa, muro, muro con apertura, stanza,
piattaforma con accessi, passerella, linea di coperture.

### Cosa mi porto dietro da questa indagine
Il navmesh non è una funzione della geometria dichiarata: fra le due ci sono **erosione**
(`kAgentRadius` per lato), **sfoltimento dei cigli** (`rcFilterLedgeSpans` toglie le celle sul
bordo di uno strapiombo), **altezza libera** e **area minima di regione**. Una superficie stretta e
sopraelevata può sparire pur essendo perfetta sulla carta — ed è per questo che l'unica prova che
conta è **chiedere al navmesh**, non guardare i box.

**Build-verified** Release e Debug, 0 warning. Training Ground invariata: `--validate` 0 problemi,
1043 poligoni, 5/5 command post raggiungibili. Mappe di prova cancellate.

---

## 2026-08-05 (157) — Libreria di strutture a NOVE primitive, e la sonda di raggiungibilità mentiva

Controlli di sicurezza richiesti dall'utente + ampliamento della libreria "in vista della prossima
mappa, che alzerà parecchio il livello di complessità".

### Controlli di sicurezza: tutti verdi, tranne un ADR rimasto indietro
Build Release e Debug **0 errori e 0 warning**; Training Ground `--validate` 0 problemi; 6002 tick di
simulazione con **0 eventi ERROR** e 2 stalli (max 5,1 s); geometria di Training Ground **byte per
byte identica** al pre-modifica. Trovato invece che **ADR-047** era ancora *Proposed* pur essendo la
premessa su cui ADR-053 respinge il pitch sul box e la collisione a mesh: la sua condizione
("quando la pipeline prefab sarà implementata e verificata") è soddisfatta da ADR-048 dal 2026-08-04.
**Promosso ad Accepted.**

### Cinque primitive nuove, scelte dal fabbisogno e non per completismo
Il numero che decide: **una salita di 8 m occupa 12 m di sviluppo diritto** alle nostre metriche.
Dentro un edificio è insostenibile — e da lì nasce la prima delle cinque.

| primitiva | perché serve |
|---|---|
| **Scala con pianerottolo** | due rampanti + pianerottolo: **dimezza l'ingombro**. È ciò che rende possibile la verticalità in uno spazio denso |
| **Muro con apertura** | porta o finestra con stipiti e architrave giusti. Con il parapetto diventa una **finestra**, e il parapetto è **copertura vera** (box di tipo `cover`) |
| **Stanza (guscio)** | pavimento + 4 muri + soffitto opzionale, con una porta per ogni lato dichiarato: il modulo canonico dei kit modulari, e **un interno non nasce senza vie d'ingresso** |
| **Passerella** | un **corridoio in quota**: non decorazione, ma una corsia tattica che domina il piano di sotto. Parapetti opzionali (riparano ma accecano verso il basso — KI #83) |
| **Linea di coperture** | barricata a intervalli. Emette box `cover`, cioè **proprio ciò che la derivazione dei metadata (doc 46) cerca** |

Nove in tutto. Ogni misura non dichiarata prende il valore **normativo** di `MapMetrics`, e il
pannello avvisa quando una scelta esce dai limiti ("il gigante non ci passa", "sotto il corridoio
minimo", "altezza sotto il minimo al coperto").

### Il banco di prova ha trovato un difetto vero nella scala con pianerottolo
Mappa sintetica con tutte e nove le primitive e quattro obiettivi. Verdetto iniziale: la cima della
scala doppia **irraggiungibile**. Causa, calcolata e non indovinata:

> Il pianerottolo era largo quanto **un solo** rampante, quindi i due si toccavano **sul bordo**.
> L'erosione del navmesh (raggio agente 0,40 per lato) li separava di **0,80 m**: il navmesh saliva
> il primo rampante e si fermava lì.

**Regola generale che ne esce**: due superfici che devono restare connesse vanno **sovrapposte**, non
accostate. Primo rimedio (pianerottolo 2w × 2w) sbagliato a sua volta — estendendosi all'indietro
seppelliva gli ultimi due gradini sotto il proprio ripiano e creava un salto di 0,60 m. Corretto:
**largo il doppio, profondo uno solo**.

### E ha trovato un difetto MIO nella sonda di raggiungibilità
La passerella, costruita **senza alcun accesso**, risultava «raggiungibile, arriva a 10 cm». Falso:
- `findPath` di Detour restituisce percorsi **PARZIALI** (`DT_PARTIAL_RESULT`) che si fermano al
  poligono più vicino raggiungibile, e `findStraightPath` appende comunque il punto richiesto;
- quindi sia `found` sia `miss_by` erano veri anche per un'isola.

E il punto peggiore: **`NavManager::isReachable` esisteva già**, controllava il parziale *e* che il
path toccasse il poligono destinazione, ed è quello che usa l'AI a runtime. Avevo scritto una seconda
verità sulla stessa domanda. Le sonde ora la chiamano.

### Conseguenza sui numeri già pubblicati: KI #96 ridimensionato
Con il criterio giusto Training Ground passa da **8 posizioni irraggiungibili a 1**. Le altre 7-8
non sono irraggiungibili: sono autorate **a mezz'aria sotto i ponti** (y 1,55-1,77), l'unità ci
arriva stando sotto, ma il grafo tattico è calcolato con la LOS da quella quota — **descrive un punto
di vista che nessuno occupa**. Difetto più insidioso, non minore. Dettaglio in KI #96.

**Build-verified** Release e Debug, 0 warning. **Verificato per navmesh**: tutte e quattro le mete
del banco di prova raggiungibili dopo le correzioni; Training Ground invariata (1043 poligoni,
5/5 command post, `--validate` 0 problemi). Mappe di prova cancellate.

---

## 2026-08-05 (156) — G7: i difetti si vedono MENTRE costruisci

Ultima fase degli strumenti prima della mappa grande (doc 47 E7).

### Una sola analisi, mostrata prima
I box con difetti si colorano nel viewport: **rosso** = problema (il navmesh non ci sale, nessuno ci
arriva), **ambra** = avviso. La sorgente è `m_issues`, cioè **la stessa `analyzeTacticalHealth` che
usa `--validate`** — non una seconda analisi "per l'editor", che prima o poi darebbe un verdetto
diverso da quello del gioco. È il difetto che ci è già costato di più (changelog 77: due verità sullo
stesso mondo).

Acceso di default: un controllo che va ricordato di accendere è un controllo che non si usa.

### Il numero sott'occhio
In toolbar compare lo stato di salute — *"N problemi" / "N avvisi" / "nessun difetto"* — ricalcolato
a ogni modifica. Prima quel conteggio esisteva solo dentro un pannello da aprire; ora se
un'operazione introduce un problema, **il numero sale nello stesso istante**. È la differenza fra
correggere un box e rifare una zona.

### Il confine, dichiarato invece che aggirato
Doc 47 §6 elencava anche due controlli **su navmesh**: connettività vera dallo spawn e tempo di
cammino verso gli obiettivi. **Restano fuori dall'editor**, e non per pigrizia: il Map Editor linka
`ContentValidation` e `WorldIntel` ma **non** Recast/Detour (CMake, ADR-002) — costruire il navmesh
nell'editor è una decisione a sé, non un dettaglio di questa fase.

Quei due controlli **esistono già**, dal lato motore: le sonde `objective reachability` e
`posizioni irraggiungibili` (changelog 147) li riportano al caricamento, in modo deterministico. E
con doc 46 M1 la connettività diventa un dato di prima classe (`componentId` per poligono),
consumato sia dall'AI sia dall'editor. Fare qui una terza strada sarebbe stato il contrario della
regola che questa fase applica.

**Build-verified** Release e Debug. Training Ground `--validate` 0 problemi.
**Da verificare a mano**: i box rossi/ambra nel viewport e il contatore in toolbar.

**Con G7 il piano degli strumenti è chiuso salvo G8** (riparazione di Training Ground).

---

## 2026-08-05 (155) — Il taglio in quota diventa una SEZIONE vera (e via la lastra)

Due difetti segnalati dall'utente su 154, entrambi centrati.

### "Le cose sopra non vengono tagliate"
La regola era *"nascondi il box se la sua BASE sta sopra la quota"*. Conseguenza: **ogni muro che
parte da terra restava in piedi qualunque fosse il taglio**, e muovendo lo slider non si vedeva
tagliare quasi nulla. Il difetto era la semantica, non il codice.

Ora il box viene **sezionato**: sopra la quota sparisce, e quello a cavallo si disegna **solo fino
al piano**. È una vista in sezione vera — costa due sottrazioni per box, e i **dati non si toccano**
(vale anche per i gradini delle strutture, che si sezionano come tutto il resto).

### "La lastra gialla mi farebbe da tetto"
Vero, ed era controproducente: la lastra che segnava il piano di taglio diventava essa stessa una
superficie che copriva la vista — l'opposto di ciò per cui si taglia. **Rimossa.** Il piano non ha
bisogno di essere disegnato: si vede da sé, è la quota a cui la geometria risulta sezionata.

**Build-verified** Release e Debug. Training Ground `--validate` 0 problemi, navmesh 1043 poligoni,
5/5 command post raggiungibili.

---

## 2026-08-05 (154) — I filtri di vista non si vedevano: mancava il ridisegno

Segnalazione dell'utente: *"la vista non sembra funzionare"*. Esatta.

### La causa
Le spunte cambiavano `m_showType`/`m_hideAboveY` e **nessuno richiamava `updateViewport()`**: lo
stato cambiava, la scena no, finché non si toccava qualcos'altro. Il filtro sembrava inerte.

### E perché non bastava chiamarlo e basta
`updateViewport()` comincia con `recomputeExposure()`, che è **O(n²) sulle posizioni tattiche**.
Rifarlo a ogni frame mentre si trascina uno slider significherebbe pagare un'analisi tattica
completa per nascondere un tetto. Quindi la funzione prende ora un parametro:
`updateViewport(recomputeDerived = false)` per i cambi di **sola vista**. È la distinzione fra
"il mondo è cambiato" e "sto guardando il mondo in un altro modo".

### Le due richieste dell'utente sul taglio in quota
- **Slider oltre al numero preciso.** Ora è uno `SliderFloat` con estremi presi dall'**estensione
  vera della mappa** (da −Y del box più basso a +Y del più alto): uno slider da −5 a 1000 sarebbe
  stato inutilizzabile. Ctrl+click sullo slider consente comunque di digitare il valore esatto —
  una cosa sola, entrambi i modi. Sotto, l'etichetta ricorda la quota corrente e il range della mappa.
- **Vedere il taglio in tempo reale.** Con il ridisegno immediato il taglio si aggiorna mentre si
  trascina, e nel viewport compare una **lastra gialla alla quota del taglio**, larga quanto la
  mappa: si porta il piano dove serve invece di provare un numero alla volta.

Semantica dichiarata nel tooltip, perché non è ovvia: si nasconde ciò che ha la **base** sopra la
quota. È quella giusta per lavorare dentro un edificio — sparisce il tetto, restano i muri della
stanza.

**Build-verified** Release e Debug. Training Ground `--validate` 0 problemi.
**Da verificare a mano**: spunte per tipo, slider del taglio con la lastra gialla, figura di scala.

---

## 2026-08-05 (153) — G6: serie con offset, filtri di vista, figura di scala, righello

### Prima: Ctrl+A diventa un INTERRUTTORE
Segnalazione dell'utente: Esc non era affidabile (Windows se lo prende in certe combinazioni), e un
pulsante "Deseleziona" separato era un secondo modo di fare la stessa cosa. Ora **Ctrl+A seleziona
tutto e, ripremuto, deseleziona**; il pulsante è stato tolto e resta il solo contatore
`"N selezionati"`, che spiega le scorciatoie nel tooltip. Un tasto, due versi, nessuna ambiguità.

### Serie con offset progressivo (E4)
`Serie...` crea N copie della **selezione intera** con offset *progressivo* — la copia i-esima sta a
**i × offset** dall'originale. La differenza con "Duplica" ripetuto è che una fila di dodici colonne
resta allineata invece di accumulare l'errore di dodici trascinamenti a mano. C'è anche un passo di
rotazione per copia, che basta a disporre elementi in arco.

Dettaglio non ovvio: `duplicateOne` mette la copia a +2/+2 di default, quindi la serie la **riporta
sull'originale** prima di applicare l'offset dichiarato — altrimenti ogni copia porterebbe dentro uno
spostamento che nessuno ha chiesto.

### Filtri di vista (E5)
`Vista` nasconde per **tipo** (pavimenti, muri, piattaforme, coperture, decorazioni), nasconde le
strutture, e taglia **sopra una quota**: è il modo di lavorare dentro un edificio senza il tetto
davanti agli occhi. Solo visivo, non tocca i dati e non si salva. Il pulsante mostra un asterisco
quando qualcosa è nascosto — senza, ci si dimentica di aver filtrato e si crede che un box sia
sparito.

### Figura di scala (E6)
Due sagome affiancate: l'unità di oggi (**2,0 m**) e il **gigante di riferimento** (2,40 × 1,20) su
cui sono dimensionate le metriche. Serve al difetto più comune del blockout — gli sbagli di scala —
e mostra a colpo d'occhio se una porta o un corridoio reggono anche il caso peggiore.

Si **ancora dove la piazzi** invece di seguire la telecamera: `updateViewport` ricalcola anche
l'esposizione (O(n²) sulle posizioni), quindi rinfrescarla a ogni frame costerebbe caro — e una
sagoma che insegue lo sguardo distrae invece di aiutare. C'è "Riposiziona qui" per spostarla.

### Righello, senza uno strumento nuovo (E6)
Con **esattamente due elementi selezionati** il pannello mostra la distanza in pianta, quella totale
e i tre delta. Non serviva un modo separato: la selezione multipla è già il gesto giusto. E se c'è un
dislivello oltre lo scalino massimo, lo dice con l'azione: *"servono N gradini"*.

**Build-verified** Release e Debug. Nessuna regressione: Training Ground `--validate` 0 problemi,
navmesh 1043 poligoni, 5/5 command post raggiungibili, 8/169 posizioni irraggiungibili.
**Da verificare a mano**: Serie, filtri, figura di scala, righello a due selezioni.
**Restano G7** (validazione dal vivo nel viewport) **e G8** (riparazione di Training Ground).

---

## 2026-08-05 (152) — G3: selezione multipla, e la fine di una catena duplicata quattro volte

Ultima fase mancante fra quelle che rendono costruibile una mappa da 1.520 box (doc 47 E2).

### Il modello: un INSIEME di codici, con un primario
`m_multiSel` contiene codici di selezione (la stessa codifica di `m_selBox`, più **-6000-i** per le
strutture). `m_selBox`/`m_selStruct` restano il **primario** — l'ultimo cliccato — ed è quello che il
pannello proprietà mostra. **Con un solo elemento il comportamento è identico a prima**: la selezione
multipla è additiva, non sostitutiva.

- **Ctrl+click** aggiunge/toglie, nel viewport e in entrambe le liste. Al primo Ctrl+click l'insieme
  parte da ciò che era già selezionato, altrimenti si perderebbe.
- **Ctrl+A** seleziona tutti i box e tutte le strutture; **Esc** deseleziona.

### Cosa agisce sul gruppo, e cosa no — con il motivo
| operazione | gruppo? |
|---|---|
| **Sposta** | ✅ tutti |
| **Ruota** | ✅ tutti, **orbitando attorno al baricentro** |
| **Elimina** | ✅ tutti |
| **Duplica** | ✅ tutti |
| **Scala** | ❌ resta al primario |

La rotazione di gruppo non poteva limitarsi a sommare lo yaw a ciascun elemento: avrebbe fatto
**girare ogni pezzo su sé stesso**, che è l'opposto di ciò che serve per ruotare un edificio. Ogni
elemento orbita attorno al baricentro comune *e* gira su di sé.

La scala resta esclusa di proposito: su un gruppo misto un **raggio**, un'**altezza** e un `facing`
non si scalano allo stesso modo, e un comportamento "ragionevole" inventato lì sarebbe una sorpresa.
Il pannello lo dice esplicitamente invece di lasciarlo scoprire.

### Il refactoring che è servito per farlo
La catena `if/else if` che mappa un codice di selezione sul suo elemento era ripetuta **quattro
volte** (sposta, ruota, scala, duplica), con guardie leggermente diverse. Con la selezione multipla
sarebbe diventata cinque. Estratte:
- `applyMove(code, delta)` — la catena dello spostamento, ora con un chiamante per elemento;
- `codePosition(code)` / `codeYaw(code)` — accesso ai campi comuni, usati da rotazione di gruppo,
  baricentro del gizmo ed evidenziazione;
- `duplicateOne(code)` + `duplicateSelected()` che gli itera sopra;
- `deleteSelection()` che raggruppa per contenitore e cancella **in ordine decrescente** — in avanti
  invaliderebbe gli indici successivi e si eliminerebbe l'elemento sbagliato.

### Dettagli che evitano sorprese
- Il gizmo si posiziona al **baricentro** del gruppo: è il punto attorno a cui ruota.
- **Tutti** gli elementi selezionati si evidenziano, non solo il primario — altrimenti non si vede
  cosa si sta per spostare.
- Contatore in toolbar (`"N selezionati"`) con pulsante Deseleziona.
- Dopo `Duplica` la selezione **non** segue le copie: un secondo Duplica farebbe una copia della
  copia, che non è mai ciò che si vuole.
- Spawn, route e comandante restano fuori da elimina/duplica di gruppo: sono singoli o liste di
  punti, e "eliminarli in blocco" non è un'operazione sensata.

**Build-verified** Release e Debug. Training Ground `--validate` 0 problemi, navmesh 1043 poligoni,
5/5 command post raggiungibili, 8/169 posizioni irraggiungibili (KI #96, invariate).
**Da verificare a mano**: Ctrl+click, Ctrl+A, Esc, rotazione di un gruppo di box, elimina e duplica
multipli, e che l'undo copra ogni operazione di gruppo in una voce sola.

**Con G3 sono chiuse G1-G5**. Restano G6 (array/duplica con offset, livelli, figura di scala),
G7 (validazione dal vivo nel viewport), G8 (riparazione di Training Ground).

---

## 2026-08-05 (151) — La causa vera dell'"elimina asset non funziona": NESSUN loader era idempotente

Tre segnalazioni dell'utente su 150. La seconda ha portato a un difetto strutturale del registry.

### `--- il difetto ---` I `loadX` non azzeravano il proprio contenitore
L'utente: *"elimina asset non sembra funzionare, i prefab rimangono nella lista e posso comunque
usarli... ovviamente se chiudo e riapro l'editor i prefab cancellati non ci sono più, quindi il
comando in sé funziona"*. Diagnosi esatta da parte sua, e la causa era una riga più in profondità:

> **`m_prefabs.clear()` viveva solo in `loadAll`.** Ogni `loadX` singolo **sommava** al vecchio stato
> invece di sostituirlo. Ricaricare i prefab dopo aver cancellato un file lasciava l'asset in
> memoria — quindi ancora nel menu, ancora piazzabile.

Non era un difetto del comando di eliminazione: **non era autoritativo il ricaricamento**. E valeva
per **tutte e 14 le categorie**, non solo i prefab: qualunque editor ricarichi la propria categoria
avrebbe visto dati fantasma. Corretto alla radice — ogni `loadX` ora azzera il proprio contenitore,
quindi "ricarica" significa davvero "rileggi il disco".

Con questo, la ricarica è **automatica** dopo creazione ed eliminazione (`reloadPrefabAssets`, un
punto solo, usato da entrambe): niente riavvio e nessun pulsante "ricarica" da ricordarsi di premere,
come chiesto dall'utente.

### Le strutture non si selezionavano dal viewport
I loro box derivati avevano `pickId = -1`, che non è un codice valido. Ora hanno **`-6000 - indice`**:
cliccare un gradino seleziona **la scala**, che è l'unica cosa modificabile.

### "Elimina asset" spostato dove ha senso
Era stretto fra `+` e `-`, che aggiungono e tolgono un'**istanza in questa mappa** — accostare quei
due gesti a uno che cancella un file dal disco era un invito all'errore. Ora è su una riga propria
sotto il menu a tendina e agisce sul prefab lì selezionato. Tolta la nota sulle altre mappe, come
chiesto.

### Nota di contenuto: i due prefab sono stati cancellati
`data/prefabs/` è **vuota**: "Muro triplo" e "sandbag_nest" non ci sono più, e `Prefab Test` ha perso
anche le sue 2 istanze. È coerente con i test del pulsante da parte dell'utente. **Sono entrambi
tracciati in git e recuperabili** con `git checkout -- data/prefabs/`. Non li ho ripristinati: la
cancellazione sembra voluta, ed è una scelta di contenuto.
Verificato con un prefab usa-e-getta che il **caricamento funziona ancora** (`Prefab: __probe`).

**Build-verified** Release e Debug. Training Ground `--validate` 0 problemi, navmesh 1043 poligoni,
5/5 command post raggiungibili. `Prefab Test` ora è una mappa vuota (1 box): conseguenza della
cancellazione, non una regressione.

---

## 2026-08-05 (150) — Le strutture si manipolano, i prefab si eliminano, e il rombo se ne va

Quattro segnalazioni dell'utente dopo la prova in gioco di 149. Tutte risolte.

### 1. Le strutture non si potevano né spostare né ruotare — bug vero
Il gizmo agiva solo su `m_selBox`, e le strutture usano `m_selStruct`: la selezione c'era ma nessuna
delle tre modalità la raggiungeva. Ora sposta/ruota/scala funzionano sulla **ricetta**, ed è proprio
il motivo per cui conviene: si sposta la scala intera, non quindici gradini.

Con una regola sulla scala: **agisce sui parametri del tipo, non su un box**, quindi
i gradini non si possono rompere — allargare una scala non tocca l'alzata, e alzarla **aggiunge
gradini** invece di renderli più ripidi. X/Y/Z mappano su larghezza/dislivello/pedata (scala e rampa),
lunghezza/altezza/spessore (muro), lato X/quota/lato Z (piattaforma).

Aggiunta anche la mutua esclusione fra le due selezioni: sceglierne una azzera l'altra, altrimenti il
gizmo avrebbe agito sul bersaglio sbagliato.

### 2. "I muri sono un po' larghi" + misure regolabili senza rompere i gradini
- Nuova metrica **`WALL_THICKNESS` 0,25 m** (era 0,40 fisso, visibilmente grosso). Resta ben sopra la
  cella del navmesh (0,20), sotto la quale un muro rischia di non essere voxelizzato come ostacolo
  continuo. Lo spessore autorato a 0 usa la normativa.
- Nuovo parametro **`tread` (pedata)** su scala e rampa: allunga o accorcia la scalinata **senza
  toccare l'alzata**. Il numero di gradini dipende solo dal dislivello, quindi è una leva che non può
  romperla. Limitata verso il basso a `STAIR_TREAD` — sotto quella il navmesh fatica a collegare i
  gradini, ed è la stessa soglia che usa il gate.
- Il pannello ora mostra **pendenza in gradi** e avvisa oltre i 35°: *"più ripida di una scala vera,
  allunga la pedata"*.

### 3. I prefab non si potevano eliminare
Mancava del tutto: i prefab di prova restavano per sempre nel dropdown. Aggiunto **"Elimina asset..."**
con conferma che dice **quante istanze si romperebbero in questa mappa**, e ammette il proprio limite
(*"altre mappe non le posso controllare da qui"*). Il `.bak` di `saveJsonRMW` resta di proposito: è la
rete se l'eliminazione era un errore.

### 4. Il rombo sui blocchi selezionati → colore
Durante la creazione di un prefab gli elementi inclusi avevano un rombo sospeso sopra. Ora il **box
stesso diventa ciano** e gli esclusi si smorzano al 45%: il contrasto è più netto e non si aggiunge
geometria alla scena. Il rombo resta solo per le **posizioni tattiche**, che sono punti e non hanno
un volume da colorare.

Aggiunta nella stessa passata: **la struttura selezionata si illumina tutta** (tutti i suoi box
insieme), così si vede che una scala è un oggetto solo.

**Build-verified** Release e Debug. **Nessuna regressione**: Training Ground `--validate` 0 problemi,
navmesh 1043 poligoni, 5/5 command post raggiungibili, 8/169 posizioni irraggiungibili (KI #96).
**Da verificare a mano**: il gizmo sulle strutture, l'eliminazione di un prefab e il nuovo
evidenziamento a colore.

---

## 2026-08-05 (149) — G1/G2/G4/G5 implementate: le primitive parametriche esistono, e l'editor ha finalmente l'undo

Primo blocco di implementazione del piano di map building (doc 47). Quattro fasi su otto.

### G1 — Le metriche diventano codice: `mini/game/MapMetrics.hpp`
Sorgente **unica** per tre consumatori: gate, editor, generazione. Prima non esistevano da
nessuna parte, ed è la ragione strutturale per cui le scale erano sbagliate — l'autore non aveva
sbagliato un numero, non ne aveva nessuno.
`AGENT_HEIGHT`/`AGENT_RADIUS` si trasferiscono qui da `NavManager.cpp`: le legge anche il gate, e
due copie sarebbero due verità sullo stesso mondo.

### G5 — Il `type` del box arriva al runtime
`MapGeometryBox` guadagna `type` (`floor`/`wall`/`platform`/`cover`/`decoration`), che l'editor
**scriveva dal primo giorno e il parser scartava**. Con `boxShouldBeReachable()` il gate smette di
segnalare come difetto i muri e i cubi-ostacolo: **Training Ground passa da 4 problemi a 0**.

### G4 — Primitive parametriche (ADR-053): `mini/game/MapStructures.hpp`
> Una scala non è un oggetto: è una **ricetta**.

Quattro tipi, come deciso: **scala, rampa, muro, piattaforma-con-accessi**. Si salva la ricetta
(`"structures"` nel JSON di mappa), i box si rigenerano al load — mai salvati, come i prefab
(ADR-048) e ogni dato derivato (ADR-033). **Una sola implementazione**: `mapstructures::expand` è
chiamata sia dal registry al load sia dall'editor per l'anteprima, così viewport e gioco non possono
divergere.

**Le due promesse, verificate su una mappa di prova e non affermate:**
1. *L'alzata sbagliata è inesprimibile.* Chiesta un'alzata da **2,0 m**: `effectiveRiser` la limita a
   `STEP_HEIGHT`, la scala esce con 6 gradini da 0,50 e **il gate non la segnala**.
2. *Una piattaforma con accesso dichiarato non può nascere irraggiungibile.* Piattaforma a 3 m con un
   solo lato di accesso, command post in cima → **`found:true`, arrivo a 10 cm** (misurato dal
   navmesh, non dedotto). La stessa piattaforma con `access` tutti falsi **viene segnalata**.

### E il test ha trovato due difetti nel gate — che contraddiceva le metriche che deve far rispettare
- La soglia minima del gradino era **0,6 m scelti a mano**, e scartava le scale generate dalle nostre
  stesse primitive (pedate da **0,30**, la misura normativa). Ora la soglia **è** `STAIR_TREAD`.
- Il controllo dei command post chiedeva *"c'è un gradino accanto a questo campione?"*: su una
  piattaforma 8×8 con la scala su un lato, tutti i campioni del raggio di cattura distavano più di
  mezzo metro dalla scala → falso allarme, mentre il navmesh ci arrivava benissimo. La domanda giusta
  è **"il box che regge questo punto ha un accesso?"**, ed è ora una lambda sola condivisa dai due
  controlli.

### G2 — UNDO/REDO nel Map Editor (non esisteva affatto)
A **snapshot del documento**, non a comandi: l'editor muta lo stato in decine di punti sparsi, e un
command pattern avrebbe richiesto di riscriverli tutti. Una mappa in memoria sono poche decine di KB.
Profondità 64, `Ctrl+Z` / `Ctrl+Y` / `Ctrl+Shift+Z`, più due pulsanti in toolbar col conteggio.

Il pezzo che lo rende utile davvero: **un aggancio unico** su `ImGui::IsAnyItemActive()`. Invece di
infilare una chiamata accanto a ogni `DragFloat` — decine di punti, e il primo dimenticato è
un'operazione non annullabile — si fotografa lo stato quando un widget diventa attivo e lo si
consegna solo se qualcosa è cambiato davvero. **Un intero trascinamento = una sola voce di undo.**
Stessa coalescenza per il gizmo (delta per frame fusi entro 0,6 s).

### Nessuna regressione
Training Ground: navmesh **1043 poligoni**, tutti e 5 i command post raggiungibili (detour ≤ 1,07),
8/169 posizioni irraggiungibili (invariate: sono KI #96). `--validate`: **0 problemi**.
**Build-verified** Release e Debug, 0 errori.
**Da verificare a mano dall'utente** (non ho eseguito la GUI): il pannello delle strutture, il
dropdown "+ Struttura", l'anteprima nel viewport e il comportamento di Ctrl+Z.

**Resta da fare**: G3 selezione multipla, G6 array/livelli/figura di scala, G7 validazione dal vivo.

---

## 2026-08-05 (148) — Metriche confermate: le taglie MISURATE, e il margine messo nella geometria

L'utente ha confermato le metriche di doc 47 §4 con una riserva precisa: *"non sono sicuro al 100%
che siano giuste perché i dati tipo l'altezza dei modelli li vedi tu... l'importante è che siamo
sicuri siano perfetti e lascino un minimo di margine in caso magari di truppe che siano un po' più
alte o larghe"*. Riserva giustissima: le avevo dedotte dalle costanti, non misurate.

### Le taglie vere, dalle hitbox × `mesh_scale`
| unità | altezza modello | busto |
|---|---|---|
| Clone Trooper | **1,98 m** | 0,33 m |
| B1 Battle Droid | **2,03 m** | 0,30 m |

### E la misura ha fatto emergere un difetto: la stessa unità ha TRE altezze
modello **1,98-2,03 m** · agente navmesh **1,80 m** · box di collisione **1,00 m** (`AI_HALF_Y` 0,50).
Il navmesh dichiarava percorribile un sottopasso da 1,85 m in cui la testa passava **dentro** il
soffitto.

**Corretto**: `kAgentHeight` **1,80 → 2,10 m**. Misurato prima di adottarlo: costa **4 poligoni su
1047** su Training Ground, raggiungibilità dei 5 command post invariata (detour ≤ 1,07), posizioni
irraggiungibili invariate (8/169). Il box di collisione a 1,00 m **resta aperto**: tocca la fisica,
non la navigazione, e va valutato a parte.

### Dove va il margine — la decisione strutturale
`kAgentRadius`/`kAgentHeight` sono costanti **globali** e il navmesh si costruisce **una volta, per
una taglia**: un'unità più larga semplicemente non entra, e non c'è nulla che l'AI possa fare a
runtime. Da cui:

> **Il margine va nella GEOMETRIA, non nelle costanti.** Cambiare `kAgentRadius` è una riga;
> allargare i corridoi di una mappa da 60.000 m² già costruita è rifarla.

**Gigante di riferimento: 2,40 × 1,20 m** — +18% in altezza e +50% in larghezza sull'unità di oggi,
sufficiente per Super Battle Droid, Magnaguard, Droideka, Wookiee. La mappa si dimensiona su di lui;
il motore resta tarato sull'unità attuale. Quando servirà: una riga di costante, oppure un secondo
navmesh per taglia (Recast lo supporta, +1,4 s di build sulla mappa grande).

### Cinque metriche allargate di conseguenza
corridoio **2,0 → 2,4 m** · porta **1,5 × 2,4 → 1,8 × 2,8** · altezza libera al coperto
**2,4 → 2,8** · muro **3,0 → 3,2** · copertura alta **1,6 → 1,7**. Erano tutte dimensionate
sull'unità di oggi. Costano solo disciplina in costruzione.

**Build-verified** Release e Debug, 0 errori. **Misurato**: navmesh 1043 poligoni, 5/5 obiettivi
raggiungibili, 8/169 posizioni irraggiungibili (invariate, sono KI #96).
**Deciso anche**: primitive del primo giro = **scala, rampa, muro, piattaforma-con-accessi**.

---

## 2026-08-05 (147) — Training Ground non era rotta: era il GATE a mentire (e una riparazione annullata)

Il piano era *"prima sistemo Training Ground, poi gli strumenti"* (decisione dell'utente). Ho
sistemato — e ho scoperto che non c'era quasi nulla da sistemare.

### Cos'è successo, in ordine
1. Ho **riparato le due scalinate "C Box"** (alzate 0,75-0,78 m contro un massimo di 0,55) dimezzando
   la profondità delle pedate e inserendo 8 gradini intermedi: 4 gradini alti → 8 bassi, stesso
   ingombro, stesse quote dei ripiani.
2. Poi ho misurato l'**effetto** invece del dato — ed era **nullo**. Alpha era già raggiungibile
   prima; nessuna posizione tattica dipendeva da quelle terrazze; il conto delle posizioni
   irraggiungibili era identico (8/169) prima e dopo.
3. **Ho annullato la modifica alla mappa.** La geometria di Training Ground è tornata byte per byte
   com'era. Cambiare il contenuto dell'utente per un difetto che non blocca nulla, senza poter
   dimostrare un miglioramento, è esattamente ciò che le regole del progetto vietano.

### Il vero difetto era nel gate: 13 problemi → 4, e 9 erano falsi allarmi
Le scale che il gate dichiarava inesistenti **c'erano già**: "Stair 1-4" per i Big Box (alzata 0,11),
"CT stair 10-11" per il Droid CT Floor (alzata 0,47). Tre difetti, tutti corretti in
`ContentValidation.cpp`:
- **slab sepolti** — una scalinata si costruisce impilando slab e il gate segnalava anche quelli
  sotto (10 falsi allarmi). Ora chiede se c'è **altezza per starci in piedi** (1,80 m), campionando
  la superficie 5×5 invece di guardare il solo centro. Due tentativi sbagliati prima di azzeccarlo:
  il test sul centro cade sul confine fra due mezze pedate, e filtrare per *base* scarta proprio il
  box che seppellisce, perché gli slab autorati a mano **si compenetrano di 3 cm** — "sta sopra" si
  giudica dal top;
- **pedate strette** — il gradino da cui si sale doveva essere largo 1,2 m come un pavimento, così le
  pedate da 0,8 m non contavano. Ora la soglia per il **gradino** è 0,6 m: basta appoggiarci un
  piede, perché i gradini si concatenano;
- **post in quota** — `[post Aplha]` era dichiarato incatturabile perché il controllo accettava solo
  `top ≤ STEP_HEIGHT`: **puniva ogni obiettivo sopraelevato**, cioè esattamente la verticalità che
  stiamo cercando di abilitare.

### KI #94 smentita — e l'utente aveva ragione
Il post `Aplha` **non è incatturabile**: `found:true`, percorso 40,2 m contro 37,4 in linea d'aria
(**detour 1,07**), arrivo a **10 cm**. L'utente lo aveva detto — *"in realtà ci possono andare in
alpha e anche catturarlo"* — e la diagnosi sbagliata veniva da una guardia che non sapeva salire le
scale. Resta valido solo il refuso nel nome.

### Osservabilità nuova, permanente (ADR-050): due sonde che mancavano
Il punto di svolta è stato smettere di contare **esiti** (catture, eventi di combattimento — che
divergono fra run e non dicono nulla sulla geometria) e misurare il **sintomo**, deterministico:
- **`objective reachability`** — per ogni command post: percorso trovato?, lunghezza, distanza in
  linea d'aria, **detour** (rapporto fra le due) e **`miss_by`** (di quanto il percorso manca il
  bersaglio: se Detour ripiega sul poligono più vicino, `found` è `true` ma non ci si arriva);
- **`posizioni irraggiungibili`** — quante delle posizioni tattiche il navmesh non raggiunge, con le
  8 peggiori e il loro scarto.

### E la seconda sonda ha trovato subito un difetto vero (KI #96)
**8 posizioni su 169 sono sospese in aria**: `chokepoint` #2-5 e `cover` #44-47, tutte **sotto** i
ponti, con `y` fra 1,55 e 1,77 m mentre il suolo lì è a ~0. Il percorso si ferma 1,6-1,8 m sotto di
loro. L'AI le vede, le può scegliere, e non ci arriva. È contenuto: da correggere all'utente.

### Correzione a doc 47 e ADR-053: l'AI non salta affatto
Avevo scritto *"salto giocatore 1,29 m, salto AI 1,08 m"*, deducendoli dalle costanti. Verificato in
`AiSystem.cpp`: il salto è dentro un ramo `if (!useCrowd && ai->jumpEnabled …)` e `useCrowd =
navActive` → **con il navmesh attivo, cioè sempre, l'AI non salta mai**. La regola giusta è più
netta: sopra **0,55 m** l'AI si ferma, punto; fra 0,55 e 1,29 il giocatore sale e l'AI no.

**Build-verified** Release e Debug, 0 errori. **Misurato**: 13→4 problemi, raggiungibilità di tutti e
5 i command post, 8/169 posizioni irraggiungibili. **Non verificato a mano**: l'aspetto in gioco (non
ho toccato la geometria, quindi non c'è nulla da guardare).

---

## 2026-08-04 (146) — Map building: il PIANO (doc 47, ADR-053) e due misure corrette

**Nessun codice** (a parte quanto già in 145). L'utente ha deciso la taglia della mappa nuova —
**300 × 200 m** — e chiesto ricerca + pianificazione su map building e geometrie. Risultato: **doc 47**.

### La misura che ho sbagliato due volte, e come si è chiusa
In 145 avevo "corretto" le dimensioni di Training Ground a 154,9 × 91,9 m. **Sbagliato anche quello**:
calcolavo l'ingombro dei box **ignorando la rotazione `ry`**, e due "Side Bridge" lunghi 90 m ruotati
di 90° risultavano estesi lungo X invece che lungo Z. Il valore vero, confrontato con i `bmin`/`bmax`
che il motore stampa per il navmesh:

> **Training Ground = 71,3 × 92,4 m = 6.595 m², quota −0,4…15,5 m.** 169 posizioni = **una ogni 39 m²**.

Il dato del motore era giusto, il mio calcolo no. Leggere il JSON non è misurare la mappa: la
verifica è contro ciò che il motore costruisce davvero. Corretti doc 44, 46, 13_ADR, 10_ProjectMemory
e l'entry 145.

### La mappa da 300 × 200: misurata, non stimata
Generata una mappa sintetica 300 × 200 con la stessa densità di box di Training Ground (1.520 box),
caricata in Release:

| | Training Ground | sintetica 300 × 200 | rapporto |
|---|---|---|---|
| area | 6.595 m² | 60.000 m² | 9,1× |
| poligoni navmesh | 1.047 | 5.806 | 5,5× |
| **build navmesh** | **0,113 s** | **1,385 s** | **12,3×** |

**Verdetto: 300 × 200 regge a tile singola**, `ok: true`, nessun cambio architetturale, ~1,4 s di
load. E il costo cresce **più che linearmente** con l'area → 300 × 200 è anche il limite naturale
della tile singola: è la taglia giusta tecnicamente, non solo di design. (Mappa di prova cancellata
dopo la misura.)

### Audit dell'editor: i tre pulsanti contro i 1.520 box
Letto sul codice, non ricordato. `MapEditor.cpp` (2.921 righe) offre per costruire: **`+ Box`,
`Duplica`, `Elimina`**. E inoltre:
- **selezione singola** (`int m_selBox`): non si può spostare un edificio;
- **nessun undo/redo**, in tutto `editor/`;
- nessun gruppo, nessun livello, nessun array, nessuna primitiva oltre il box;
- ✅ snap alla griglia e gizmo esistono già.

> Con questi strumenti, 1.520 box significa ~1.520 clic su "+ Box" **senza poter annullare un
> errore**. Non è pessimismo: è l'aritmetica degli strumenti che ci sono.

### Il difetto strutturale: una pendenza è INESPRIMIBILE
`MapGeometryBox` ha `ry` e basta — nessun pitch, nessun roll. Non si può autorare una rampa. E questo
**mentre il navmesh dichiara di accettare pendenze fino a 45°** (`kAgentSlope`): una capacità che
nessun dato può attivare — la stessa forma di difetto di ADR-023, applicata alla geometria.

### La decisione: primitive parametriche che si espandono in box (ADR-053, Proposed)
> **Una scala non è un oggetto: è una RICETTA.** L'autore dichiara "da qui a lì, larga 4 m"; la
> macchina emette i box con l'alzata giusta. **L'alzata sbagliata diventa inesprimibile.**

Parametri salvati, box espansi **mai** (rigenerati al load, come i prefab ADR-048 e i derivati
ADR-033). Le pendenze si risolvono per **scalettatura fine** — alzata 0,20 m = multiplo esatto di
`kCellHeight`, pedata 0,30 → 33,7°, dentro la banda 30-35° della letteratura — **non** aggiungendo
pitch al box, che romperebbe LOS analitica, navmesh e collisione (cioè ADR-047) per una feature di
authoring. E **la piattaforma dichiara i propri accessi**: non si verifica dopo che sia
raggiungibile, si rende il difetto inesprimibile prima.

### Una regola che nasce da una nostra misura, non dalla letteratura
Salto giocatore **1,29 m**; **l'AI non salta** col navmesh attivo (corretto in 147). Quindi:
**mai un dislivello fra 0,55 e 1,29 m senza una scala** — sopra 0,55 non si cammina, sotto 1,29 il
giocatore salta e **l'AI no**. È il sintomo che l'utente ha descritto (*"molti sono rimasti giù ad
andare contro il muro"*) espresso come vincolo di costruzione invece che come bug da inseguire.

### Un canale semantico già autorato e buttato via
L'editor scrive per ogni box un `type` (`floor`/`wall`/`platform`/`cover`/`decoration`) e su Training
Ground è **già compilato con criterio**: 75 floor, 74 wall, 18 cover. Ma `MapGeometryBox` non ha quel
campo: **il runtime lo scarta al parse**. È il cambiamento più economico del piano — il dato esiste già.

**Documenti**: nuovo 47; 13_ADR +1 (053, Proposed); 44 §W1 rinvia a 47; 46 §0/§7/§12 aggiornati con
la misura del 300 × 200 e la decisione dell'utente; 06_Todo aggiornato.
**Restano tre scelte all'utente** (doc 47 §12): le metriche normative, quante primitive nel primo
giro, e se riparare Training Ground come collaudo prima di costruire la mappa nuova.

---

## 2026-08-04 (145) — Metadata tattici: dalla ricerca al PIANO (doc 46, ADR-051/052)

**Nessun codice.** Su richiesta dell'utente: prima ricerca pura (doc 45), poi *"una pianificazione
nel dettaglio della miglior combinazione possibile di sistemi"*. Il risultato è **doc 46**.

### Ricerca completata con i due riferimenti indicati dall'utente
Aggiunti a doc 45 **F.E.A.R.** e **Arma 3**, e sono i due che hanno cambiato la conclusione.
- **F.E.A.R.** dà il modello del **sensore per-agente**: *"each A.I. already has sensors keeping an
  up to date list of potentially valid cover positions nearby... All the squad behavior needs to do
  is select one node that the A.I. knows about"*. La squadra non analizza la mappa; rivendica.
- **Arma 3** conferma i prefab (`buildingPos` = ADR-048) ma è soprattutto il **controesempio**:
  annota gli edifici e **non il terreno**, ed è precisamente lì che la sua AI è criticata da vent'anni.

### La lezione che ha deciso l'architettura
> Se l'unico livello di metadata è "posizioni su oggetti autorati", **lo spazio fra gli oggetti resta
> muto** — e non si tappa aggiungendo posizioni.

### Correzione di un numero che avevo sbagliato in doc 44 e 45
Avevo scritto che Training Ground è *"una mappa di prova da 50×40"*. Misurata sulla geometria:
**71,3 × 92,4 m, quota massima 15,5 m**. Il dato che ne esce è **più severo**, non meno: 169
posizioni su 6.595 m² = **una ogni 39 m²**. Il mondo è quasi vuoto di significato, ed è coerente con
il sintomo che stiamo inseguendo (AI che non sanno cosa farci con lo spazio).

### La decisione: substrato a TRE livelli (ADR-051, Proposed)
La domanda aperta dell'utente era *posizioni discrete o poligoni navmesh*. La risposta è che la
domanda era mal posta — **nessun sistema che funziona usa una granularità sola**:
- **A — griglia d'influenza** (2 m, dinamica): *"com'è messa quest'area, adesso?"*
- **B — poligoni navmesh** (statico, derivato al load): *"che tipo di luogo è, e come ci si arriva?"*
- **C — posizioni tattiche** (semantico, autorabile): *"dove mi metto, e cosa ci faccio?"*

**Regola d'oro**: un dato vive nel livello più basso che può calcolarlo, e in nessun altro — è
questa, non i tre livelli, a impedire le verità parallele (il difetto del changelog 77).

Effetto: la mappa passa da 169 dati tattici a **decine di migliaia**, e l'autore **non scrive una
riga in più di oggi**.

### Il pezzo che risolve la verticalità, e costa quasi nulla
`distToObjective` come **campo di Dijkstra per obiettivo** sul grafo dei poligoni: una passata al
load dà la **distanza di cammino** da ogni punto della mappa, e il **gradiente dello stesso campo è
la via d'accesso**. "Alpha è a 8 m in linea d'aria ma a 40 m di cammino perché si sale solo dalla
scala" smette di essere una cosa che l'AI non può sapere e diventa la lettura di un `float`.

### Query in tre sezioni + sensore per-agente (ADR-052, Proposed)
Da CryEngine: `Generation` / `Conditions` / `Weights` separate — oggi le mescoliamo, ed è il motivo
per cui non so dire *perché* una query ha scelto quel punto. Il **funnel di ADR-050 verrebbe gratis**
dalla struttura. Col sensore F.E.A.R. il costo per decisione passa da `O(posizioni)` a `O(20)`:
**smette di dipendere dal numero di posizioni**, che è la precondizione perché mappa grande e
generazione automatica siano possibili.

### Le 5 domande aperte di doc 45 sono tutte chiuse
Influence map **sì** (utente); granularità → **tre livelli**; protezione **per direzione**, 8 settori
da 45° (8 byte/posizione, 8 raycast al load); distanza **di cammino** via Dijkstra; ispezione via
overlay + viewport + `--validate` + `--trace-ai`.

### Il gate contro il fallimento già visto (ADR-026)
La generazione automatica si adotta **solo se** ritrova ≥ 60% delle 169 posizioni autorate a mano su
Training Ground. Se la macchina non ritrova ciò che l'autore ha scelto, non ha capito la mappa.

### Il numero misurato che ha cambiato il piano
Misurando invece di stimare (`--sim-ticks 60`, Release): Training Ground ha **1.047 poligoni
navmesh** (metà della mia stima — il livello B costa ancora meno del previsto), ma
**`buildTacticalLinks` impiega 8,1423 ms per 2.323 link su 169 posizioni**, ed è **O(n²)**:
~0,29 µs a coppia. Proiezione a 2.000 posizioni generate: **~1,14 secondi di load**.

> **Conseguenza dura**: M7 (generazione automatica) **non esiste** finché il grafo resta quadratico.
> Non è un'ottimizzazione da rimandare, è un prerequisito — aggiunto come **M0-bis**, con il rimedio
> noto (indice spaziale a griglia + limite di `fireRange`, ~10× su questa mappa).
> Senza questa misura avrei messo M7 in fondo come "nice to have" e l'avrei scoperto irrealizzabile
> **dopo** aver costruito tutto il resto.

### Correzione di codice (l'unica del change set): il log mentiva sulla mappa
`ConquestMode.cpp` e `SandboxMode.cpp` stampavano stringhe **hardcoded** `"Firebase"` / `"firebase"`
a ogni avvio, qualunque mappa fosse caricata. Caricando Training Ground il motore diceva
`[SandboxMode] Avvio — mappa firebase...` e poi `[Sandbox] Geometria firebase: 167 box` — che sono i
box di Training Ground. Un log che nomina la cosa sbagliata è peggio di nessun log: la memoria
operativa diceva *"conferma sempre quale mappa carica"*, e la riga che serviva a confermarlo
**confermava il falso**. Ora stampano `m_mapId`.
**Build-verified** (Debug e Release, 0 errori) e **verificato in esecuzione**:
`[ConquestMode] Caricamento mappa 'Training Ground'...`.

**Documenti**: nuovo 46; 45 esteso (F.E.A.R., Arma 3, domande chiuse); 44 corretto (dimensioni reali,
rinvio a 46); 13_ADR +2 (051, 052, Proposed); 06_Todo aggiornato.
**Prossimo passo concordato**: piano di **map building/geometrie (doc 47)**, poi l'utente costruisce
la mappa, poi si implementa doc 46.

---

## 2026-08-04 (144) — Le scale di Training Ground sono TROPPO RIPIDE (e il gate ora lo dice)

Segnalazione dell'utente: *"quando do il MoveTo su Alpha mi appare posizione irraggiungibile, alcuni
ci vanno ma molti sono rimasti giù ad andare contro il muro"*. **Il messaggio era corretto**, ed è
la conferma della guardia aggiunta ieri.

Su **Training Ground** (l'unica mappa su cui si fanno test — firebase va ignorata) le scale
**esistono**: box impilati a z=±5,3 con ripiani a **0,63 → 1,44 → 2,20 → 2,96**. Ma ogni alzata è di
**0,68-0,81 m**, e in un punto **1,21 m**, contro uno `STEP_HEIGHT` di **0,55**. Il navmesh non le
collega: la piattaforma centrale (alta 3,5 m) resta un'isola, MoveTo risponde correttamente
"irraggiungibile", e chi ci prova finisce contro il muro. Le zone rialzate vicino allo spawn
funzionano meglio perché hanno dislivelli minori — coerente con l'osservazione dell'utente.

**Guardia resa AZIONABILE.** Diceva solo *"manca un gradino adiacente"*, che qui manderebbe a
costruire scale già esistenti. Ora distingue i due casi e dà i numeri:
> `[geometria 39] ripiano a 2.96 m: il gradino adiacente più alto è a 2.20 m, ALZATA 0.76 m contro un
> massimo di 0.55 → troppo ripido, il navmesh non ci sale. Servono gradini intermedi`

Su Training Ground: **13 problemi**, fra cui il post `Aplha` senza alcun punto calpestabile nel suo
raggio di cattura (3,6 m) — cioè esattamente il posto dove l'utente ha provato il MoveTo.

**Alzare `STEP_HEIGHT` non è la risposta**: 0,9-1,2 m non è un gradino ma un salto, e la costante è
usata anche dal movimento fisico (`slideMoveWithStepUp`) — cambierebbe il gioco ovunque. Il rimedio
è nei dati (gradini intermedi), oppure un giorno una **capacità di arrampicata dichiarata**.

### Prossimo blocco deciso: la FONDAZIONE DEL MONDO (nuovo doc 44)

Ordine scelto dall'utente, e i dati lo confermano: **prima la fondazione, poi gli ordini** — ruota,
ordini rapidi e mappa tattica si appoggiano tutti a settori, posizioni e obiettivi, e costruirli
sopra metadata scarni significherebbe rifarli. Nuovo **`44_WorldBuildingFoundation.md`** (Planned
Feature, zero codice): **W1** geometria percorribile per costruzione (scale/rampe come primitive che
non possono sbagliare l'alzata, verifica nel viewport, connettività vera via navmesh) · **W2**
metadata **derivati** da geometria/ambiente/posizione, con all'autore il solo INTENTO · **W3**
generazione automatica delle posizioni tattiche come suggerimento correggibile · **W4** la mappa
grande come banco di prova, **solo dopo** gli strumenti.

Il rischio principale è dichiarato nel documento: **sovra-derivazione**. Un metadato che l'autore
non capisce e non può correggere è peggio di uno assente. Ogni dato derivato dovrà essere
ispezionabile e sovrascrivibile.

## 2026-08-04 (143) — Verticalità sbloccata: tre difetti nel motore, e su firebase MANCANO LE SCALE

Tre correzioni sul riconoscimento delle superfici e del movimento in quota, più la causa vera del
caso `firebase` — che non era nessuna delle tre.

**1. `groundHeightAt` aveva un tetto a 1,6 m.** Un box contava come suolo solo se il suo ripiano
stava sotto quella quota, per escludere i muri. Ma **l'altezza non distingue un muro da un
pavimento**: le piattaforme laterali di firebase stanno a **2,5 m** e per tutto il gioco **non erano
suolo**. È letteralmente ciò che l'utente descriveva — *"le superfici camminabili non vengono
riconosciute"* — e da lì nascevano quote di mira sbagliate, punti di ricerca a terra sotto una
piattaforma e spawn alla quota errata. Il criterio ora è la **pianta**: un muro è sottile (0,5 m),
un pavimento è largo abbastanza da starci sopra (soglia 1,2 m, agente ~0,8) — **a qualunque quota**.

**2. Il test di "arrivato" era 2D** (`rdx² + rdz² < 3²`): chi stava *sotto* una piattaforma, a 2 m in
pianta, si credeva arrivato e mollava il waypoint senza mai salire. Ora include la quota.

**3. La destinazione di movimento non portava la sua quota**: `requestMoveTarget` passava `et->y`,
l'altezza di **chi cammina**, quindi `findNearestPoly` agganciava il pavimento *sotto* la
piattaforma invece del ripiano sopra. L'unità ci andava a terra credendo di aver obbedito — ed è il
motivo per cui un ordine MoveTo funzionava (lì la quota arriva col punto) e l'iniziativa no.

**Misurato su Training Ground**, che ha geometria salibile: le unità raggiungono **quota 3,05**
(prima: nessuna sopra 0,60 in una sim su firebase), combattimento **248 → 267**, stalli **3 → 2**.

### E su firebase non salgono comunque: MANCANO LE SCALE

Elencati tutti i 22 box: **non esiste un solo gradino**. Il salto dal pavimento (top 0,10) a
qualunque piattaforma (top 1,00) è di **0,90 m**, contro uno `STEP_HEIGHT` di **0,55**. Le
piattaforme sono **isole scollegate** nel navmesh: nessuna unità potrà mai salirci, con qualunque AI.
Le scale che si vedono esistono nel **modello visivo**, non nei box che generano la navigazione — è
esattamente il divario contro cui mette in guardia ADR-047 (*la verità tattica sono i box*).

Nota per l'utente: i **muretti attorno alla piattaforma centrale sono ancora nel file** (quattro box
con ripiano a 2,00 m attorno ad Alpha), anche se ricordava di averli tolti.

**Nuova guardia** `UnreachablePoint` estesa alla geometria: per ogni ripiano calpestabile sopra lo
scalino massimo, esiste un gradino adiacente che permetta di salirci? Su firebase ne segnala
**11**; su Training Ground **13**. Sono difetti reali di mappa che nessuno poteva vedere, se non
guardando le AI girare in tondo.

## 2026-08-04 (142) — Verticalità: "arrivato" era un test 2D, e la meta di movimento non ha quota

**Correzione dell'utente che ribalta il changelog 141**: su firebase i muretti sulle zone rialzate
**non ci sono più da tempo**, ad Alpha ci si arriva e lo si cattura, e con un ordine **MoveTo** le
unità ci vanno. Quindi *non* è irraggiungibile: la mia diagnosi "è la geometria" era **sbagliata**,
costruita su una lettura del MapDef senza verificare il comportamento reale. Il problema che l'utente
descrive è un altro: *"hanno problemi a capire che possono salire solo dalle scale"*.

**La traccia lo conferma in modo brutale**: il clone 13 resta a **quota 0,60 per 9000 tick** — non
sale **mai** — mentre la distanza da Alpha oscilla fra 7,5 e 20 m. Non è fermo: **vaga a terra**.

### Primo difetto trovato e corretto: "arrivato" ignorava la quota

Il test di raggiungimento del waypoint era `rdx² + rdz² < 3²` — **puramente orizzontale**. Un clone
fermo *sotto* una piattaforma, a 2 m in pianta dal punto, risultava **arrivato**: mollava il segnale
e ripartiva senza essere mai salito. Su una mappa con dislivelli ogni obiettivo in quota diventava un
giro a vuoto perpetuo. Aggiunta la quota al test (`allySigY`, tolleranza 2×`STEP_HEIGHT`) — stessa
famiglia del punto di mira di KI #86: **un'assunzione 2D in un mondo 3D**.

Effetto misurato su Training Ground (che ha posizioni elevate): **215 → 248 eventi** di
combattimento, ripetibile 248/248, e **stalli 4 → 3**. Cambio di comportamento voluto, non
regressione: le unità smettono di dichiararsi arrivate a un piano di distanza.

### Secondo difetto, IDENTIFICATO ma NON ancora corretto

Le unità continuano a non salire, e la causa successiva è visibile nel codice: la richiesta di
movimento passa al navmesh **la quota dell'agente**, non quella della destinazione —
`requestMoveTarget(idx, {x + dx*dist, et->y, z + dz*dist})`. `findNearestPoly` cerca quindi il
poligono più vicino **all'altezza di chi cammina**: puntando un posto su una piattaforma, aggancia
il pavimento *sotto* invece del ripiano sopra, e l'unità ci va a terra credendo di aver obbedito.

**Non l'ho corretto**: è una modifica al percorso di movimento di *ogni* unità in *ogni* stato, e
non ho margine per verificarla come si deve in questo giro. Farla senza misurarla sarebbe
esattamente l'errore che questo progetto ha già pagato più volte. **È il primo passo del prossimo
giro**, con la verifica già chiara: la quota del clone 13 deve superare 1,0.

*Nota su KI #90 e sulla guardia `UnreachablePoint` (141): la guardia resta valida e ha trovato un
difetto vero su Training Ground (post `Aplha` incatturabile, KI #94), ma la diagnosi su firebase era
sbagliata — corretta in KI #90.*

## 2026-08-04 (141) — A6 parte 2: la scatola nera chiude l'indagine — non era l'AI, è la GEOMETRIA

Puntata la scatola nera su un singolo clone, come previsto. La traccia dice tutto in dieci righe:
il clone **orbita** attorno ad Alpha a raggio costante — 8,2 → 8,4 → 8,7 → 9,2 → 9,9 m — muovendosi
in arco senza mai avvicinarsi. Non è fermo, non è in stallo: **gira intorno**.

**Perché**: ad Alpha non c'è uno spiazzo. C'è una **piattaforma 10×10 alta 1,0 m** circondata da
**muretti a ±6 m** — è la firebase. Il navmesh scala al massimo `STEP_HEIGHT` = **0,55 m**, quindi
la piattaforma è un ostacolo e Detour ci passa **intorno**. Dentro il perimetro resta una fascia di
terreno larga **1 m** fra il bordo della piattaforma (±5) e i muretti (±6): con raggio agente 0,4 m
e l'erosione del navmesh, quella fascia si assottiglia sotto la soglia e sparisce.

**Quindi il post è irraggiungibile a piedi, e l'AI stava facendo la cosa giusta**: ci andava il più
vicino possibile. Le tre cuciture del changelog 140 erano necessarie e corrette — senza, i cloni non
si sarebbero nemmeno avvicinati (restavano sulle route). Il difetto residuo è di **mappa**.

### La guardia che avrebbe dovuto dirlo — e il mio primo tentativo sbagliato

Nuovo difetto `UnreachablePoint` in `analyzeTacticalHealth` (quindi anche in `--validate`).
**Primo criterio: sbagliato.** Chiedeva "il centro del post sta su un ripiano alto?" e segnalava
anche Bravo e Charlie, che nelle simulazioni vengono catturati regolarmente — stanno su un rialzo ma
hanno terreno normale dentro il raggio. *Una guardia che grida al lupo si smette di leggere*: è la
terza volta che questa lezione si presenta (settori di transito, funnel di missione, ora questa).

**Criterio corretto**: campionare il disco del raggio di cattura e chiedere *"esiste almeno un punto
in cui un'unità a terra può stare?"*. Risultato: **firebase 0 problemi**, e un difetto **vero** e
mai visto su Training Ground — il post **`Aplha`** (con il refuso nell'etichetta) non ha **nessun**
punto calpestabile nei suoi 4 m: è incatturabile da chiunque.

**Limite dichiarato**: il controllo è sui DATI, quindi vede "c'è terreno" ma non "quel terreno è
CONNESSO al resto". Su Alpha il terreno esiste ed è chiuso dai muretti — per questo firebase risulta
pulito mentre in partita il post resta inaccessibile. La guardia definitiva è a livello di navmesh
(`nav->isReachable` dallo spawn), ed è il naturale passo successivo: qui serve il runtime, non il
MapDef.

**Nessuna regressione**: Training Ground 215 eventi, 4 stalli — identico.

## 2026-08-04 (140) — A6 parte 1: l'AI non è più cieca alle missioni (ma KI #90 non è chiuso)

Prima di oggi il livello AI aveva **zero riferimenti** a `activeMission`/`objectiveDefs`: il
comandante ordinava i fronti per solo peso tattico e l'obiettivo di missione non entrava nel
calcolo. Su firebase il post richiesto sta in un settore di importanza 0,5 — la più bassa — quindi
non ci andava **mai** nessuno (KI #90).

**Tre cuciture, ognuna trovata perché la precedente non bastava** — e ognuna verificata con la
telemetria invece che supposta:

1. **Mailbox `World::activeObjectives`**, scritta da `ObjectiveSystem` e letta dall'AI: stesso
   idioma di `sectorStates`/`allyIntel`, così il livello AI non si accoppia al framework missioni.
   Per Capture/Defend il punto pubblicato è il **command post**, non il centro dell'obiettivo.
   Nuovo termine `objectivePrimary/Secondary` in `sectorTacticalWeight` — **bias, non comando**
   (ADR-020): 1,5 mette un primario alla pari di una contesa piena senza superarla.
   *Risultato: nessun cambiamento.* Misurato: `torre di controllo: assente` su firebase.
2. **L'obiettivo non passa dalla torre.** `updateAllyIntel` esce subito senza torre, e questo è
   giusto per l'INTEL tattica — ma un obiettivo di missione è ciò che alla squadra è stato detto
   *prima* di schierarsi: non dipende da un'antenna. Aggiunto come segnale in `pickAllySignal`.
   *Risultato: ancora nessun cambiamento.* Misurato: il ramo era chiuso da `allyIntel.active`,
   quindi la funzione non veniva **mai chiamata**.
3. **Aperto il gate**: il ramo dei cloni ora vale con la torre **oppure** con un obiettivo attivo.
   *Risultato: comportamento cambiato davvero* — `su_route` 10 → **0**, i cloni lasciano le
   pattuglie e convergono su Alpha.

### Dove si è fermato, detto con precisione

I cloni **convergono ma non catturano**: si dispongono in un anello a **7,2-15,7 m** da un post che
ha raggio **6 m**. Mancano gli ultimi due metri. Ho aggiunto anche la divisione del compito col
`bias` (una parte va *sul* punto, il resto copre dai dintorni — perché `bestOrderPosition` mandava
tutti su posizioni tattiche *vicine*, lettura giusta per un segnale della torre e sbagliata per una
cattura), e non è bastato.

**Non ho chiuso KI #90 e non fingo di averlo fatto.** Quattro tentativi, tre dei quali hanno
prodotto un cambiamento misurabile e nessuno la cattura. I sospetti restanti, in ordine: la
**separazione del crowd** (dieci agenti che puntano lo stesso punto si impacchettano in un anello),
il **guinzaglio** al leader, o una soglia d'arrivo nel movimento. Il passo giusto è la scatola nera
su un singolo clone (`--trace-ai`), che è esattamente lo strumento costruito per questo — e che non
ho ancora puntato qui.

**Nessuna regressione**: Training Ground senza missione dà 215 eventi e 4 stalli, identico al
baseline — le cuciture sono inerti quando non c'è una missione, che è il comportamento voluto.

## 2026-08-04 (139) — Il culling TOCCA già le unità, le armi sono il 24%, e il render è VERTEX-BOUND (provato)

Domanda dell'utente: *"non si può applicare il frustum culling anche alle entità come i droidi?"*.
**Si applica già** — il ciclo passa su ogni entità con una mesh. Che nel changelog 138 le 35 scartate
fossero quasi tutte box di mappa era un esito della *scena*, non un'esclusione. Ma il contatore
aggregato non permetteva di dirlo, quindi l'ho reso capace di rispondere: **unità esaminate 12,4/frame,
unità scartate 1,6 (13%)**. Le unità vengono cullate; semplicemente in quella scena l'86% è inquadrato.

### Le armi in mano sono il 24% dei vertici → primo LOD

Lo stesso contatore, diviso per tipo: **corpi 1.165k (76%) · armi 376k (24%)**. I mesh delle armi
vanno da 6k a 57k vertici e se ne disegna uno per unità. A 35 m un'arma è pochi pixel: nuova
`WEAPON_DRAW_DISTANCE`, oltre la quale l'arma non si disegna. **Armi 376k → 39k (−90%), totale
−22%.** Il **corpo si disegna sempre**: un soldato che sparisce è un difetto di gioco, un'arma che
sparisce a 35 m è un dettaglio.

### E ho dovuto smentire una mia affermazione del changelog 138

Lì avevo scritto che "il tempo di rendering segue i vertici, ~15-20 ns ciascuno", deducendolo da due
scene diverse. Poi il LOD delle armi ha tolto il 22% dei vertici **senza cambiare il tempo** — e a
quel punto avevo prove contraddittorie e stavo per mandare l'utente a decimare modelli in Blender
sulla base di un modello non verificato.

**Esperimento decisivo** (sonda temporanea, taglio brutale dei corpi oltre 25 m, stessa scena):

| vertici/frame | draw call | `render.scena` |
|---|---|---|
| 1.210.000 | 161 | **31,3 ms** |
| **83.000** | 154 | **3,0 ms** |

**Il render È limitato dai vertici**: ~25 ns ciascuno, ×10 di velocità per un taglio del 93%, con le
draw call praticamente invariate. Il modello regge; a non reggere era il *confronto* precedente.

**La trappola di misura, da ricordare**: `--sim-ticks N` fissa i **tick di simulazione**, ma la
finestra del profilo è di 300 **frame**. Con un renderer più lento passano meno frame negli stessi
tick, quindi "l'ultima finestra" di due build copre **momenti diversi della partita** — e con essi
un numero diverso di B1 vivi, che è ciò che determina i vertici. Per confronti di rendering serve
una scena fissa, non due run a pari tick.

**Conseguenza per l'utente**: R1 (decimazione dei modelli) **pagherà, e molto** — ora è dimostrato,
non ipotizzato. Portare il B1 da 161k a ~16k vertici è la differenza fra 31 ms e pochi millisecondi
di scena. Vale la pena farlo quando arriva la scheda grafica nuova.

## 2026-08-04 (138) — R2 frustum culling: implementato, corretto, e onestamente poco utile QUI

Primo lavoro di doc 43. Nuovo `include/mini/render/Frustum.hpp`: estrazione dei sei piani dalla
matrice view-projection (Gribb/Hartmann) — quindi corretto per qualunque proiezione, split-screen
compreso — e test a **sfera**, deliberatamente conservativo: un falso positivo costa una draw call,
un falso negativo fa **sparire** un oggetto dallo schermo, che è il difetto peggiore che un culling
possa avere. Raggio d'ingombro calcolato **una volta** alla costruzione della mesh, dall'origine
(la matrice modello ruota e scala attorno all'origine, quindi la sfera resta valida senza ricalcoli).

**Risultato, detto senza abbellirlo**:

| | baseline | con culling |
|---|---|---|
| entità esaminate → disegnate | 195 → 194 | 195 → **159** (35 scartate) |
| draw call | 206 | **170** |
| **vertici per frame** | 1.648.849 | **1.567.618** (−5%) |
| `render.scena` | 30,28 ms | **30,3 ms** (3 run: 31,26 / 30,28 / 30,26) |

**Il culling è gratuito ma non guadagna quasi nulla qui.** Scarta il 18% delle entità e solo il 5%
dei vertici, perché ciò che scarta sono i **box della mappa** — geometria a poche decine di vertici —
mentre il costo sta nelle **unità**, che la camera inquadra quasi sempre. Un primo campione dava
+3 ms: era rumore, tre run successive danno 30,3 ms come il baseline.

**E una conferma che vale più del culling stesso.** Con `--stress 40`: **737k vertici → 9,6 ms**;
con 12 unità: **1,57 M vertici → 30 ms**. Il tempo di rendering **segue i vertici** (~15-20 ns per
vertice), e il numero di vertici segue **quanti B1 sono vivi** — il mesh da 161k vertici, dieci volte
il Clone Trooper. La relazione è ora quantificata, non dedotta.

**Conclusione operativa**: R2 resta (è corretto, costa zero, e conterà su mappe più grandi o quando
la camera guarda altrove), ma **il lavoro che paga è R1**, la decimazione degli asset — esattamente
come diceva la misura di KI #87 e come avevo previsto proponendolo. Portare il B1 da 161k a ~16k
vertici toglierebbe circa il 90% del traffico quando i droidi sono in campo.

**Da verificare a mano**: guardarsi intorno in gioco e controllare che **nulla sparisca o
sfarfalli** ai bordi dello schermo. È l'unico rischio di questo cambio, ed è l'unica cosa che io
non posso vedere.

## 2026-08-04 (137) — Il caduto era invulnerabile E calamita d'attenzione (KI #93)

Segnalazione dell'utente da una simulazione con molti più droidi che cloni: *"rimaneva un clone a
terra che non moriva, attorno al quale giravano i droidi, che gli sparavano anche"*. Erano **due
bug che si alimentavano a vicenda**, e insieme spiegano la scena esattamente.

1. **`CombatSystem` non poteva colpirlo.** Il filtro bersagli scartava chi ha `current <= 0`, e un
   caduto ha esattamente `current == 0`. Il commento nel codice affermava il contrario — *"un colpo
   su un già-a-terra lo finisce"* — ed era **falso da sempre**.
2. **`AiSystem` lo sceglieva.** La lista bersagli **non filtrava gli HP**: un caduto restava un
   bersaglio a pieno titolo, quindi i droidi lo acquisivano e ci sparavano.

Un bersaglio inerme, invulnerabile **e** attraente: la peggiore calamita d'attenzione possibile —
i nemici si inchiodavano su di lui ignorando la battaglia.

**Corretto in tandem**: nessuna AI sceglie più un caduto (non è una minaccia), ma il **fuoco
incrociato può finirlo**. È la lettura minima di *"priorità molto minore rispetto a chi è in piedi"*
(doc 26 Phase D, visione dell'utente); la priorità graduata vera arriverà con quel lavoro.

**Verificato**: 8 caduti → 4 morti di bleed-out (a ~10,0 s esatti, KI #91 tiene), **4 finiti dal
fuoco**, **0 caduti ancora vivi a fine simulazione**. Il sintomo riportato non si riproduce più.
Eventi di combattimento 265 → 215: atteso e voluto — le AI non sprecano più raffiche su un bersaglio
che non potevano colpire.

## 2026-08-04 (136) — A5 taratura: la curva raccomandata da doc 40 §6 è stata PROVATA e RIFIUTATA

Ultimo pezzo di A5. Doc 40 §6 raccomandava curve di risposta non lineari al posto dei termini
lineari: valore del terreno concavo (`imp^0.7`), rischio convesso (`exp^1.5`), prossimità iperbolica.
Implementate come funzioni dichiarate in `AiUtility.hpp` e **misurate**, invece di adottate perché
scritte in un documento.

### Valore del terreno concavo: rifiutato

A/B sullo stesso binario, `--sim-ticks 6000`:

| metrica | lineare | `^0.85` | `^0.7` |
|---|---|---|---|
| acquisizioni | 2362 (29% del cono) | 3272 (37%) | 3298 (37%) |
| colpi sparati | 379 | 440 | 415 |
| **eventi di combattimento** | **265** | 222 | 211 |

Più acquisizioni e più colpi, ma **meno colpi a segno**: comprimendo l'importanza le unità scelgono
posizioni più vicine e peggiori, sparano di più e colpiscono di meno. E soprattutto va **contro il
modello del progetto**: l'importanza è la dichiarazione tattica dell'**autore** (ADR-030/033), il
segnale con cui un mondo intelligente guida AI semplici — attenuarlo sposta la decisione dal
designer alla formula. **Ripristinato il lineare** (verificato: 265 eventi, identico al baseline).

Un errore lungo la strada, utile da registrare: al primo tentativo avevo applicato la curva anche
all'importanza dei **settori**, e `hold_su_posizione` è crollato da 360 a **0**. Stavo per attribuirlo
alla curva — ma `cmd_tieni` era **1** già nel baseline: quei 360 vengono da **una singola direttiva
rara** che persiste per molti tick. Un evento raro non è una metrica; per poco non rifiutavo la
curva per la ragione sbagliata (l'ho rifiutata poi per quella giusta, l'accuratezza).

### Rischio convesso: adottato, ma è INERTE oggi

Misurato da solo: **265 eventi, acquisizioni e colpi identici al lineare** — cifra per cifra. Tocca
solo `bestFlankingPosition` e `bestOverwatchForPosition`, che in 6000 tick capitano **5 volte**.
Tenuto perché è la forma giusta e costa zero, ma il valore di quella riga oggi è **sapere che è
inerte** invece di crederla efficace: conterà quando l'aggiramento sarà frequente.

### Cosa resta di A5

La formalizzazione (117) e l'ispettore (116) restano il guadagno vero: i pesi sono visibili,
discutibili e in un posto solo. La *taratura* ha prodotto un risultato negativo — ed è un risultato,
non un fallimento: ora sappiamo che la raccomandazione teorica del documento non regge alla misura,
e la prossima volta non la si rifà. Le funzioni restano in `AiUtility.hpp` documentate con i numeri,
come **record e non come trappola**.

## 2026-08-04 (135) — Il bleed-out si FERMAVA con un soccorritore vicino (il dubbio dell'utente era fondato)

L'utente ha chiesto se un clone fosse davvero rimasto a terra 33 secondi, dicendo di sospettare da
tempo che bleed-out e rianimazione non funzionassero come sulla carta. **Aveva ragione**, e la
telemetria l'ha quantificato in due minuti: entità 253 **a terra 34,5 s** con
`squad_bleedout_time` = **10 s**; entità 14 morta a **12,2 s** invece di 10.

**Causa**: il decremento del timer viveva nel ramo `else`, cioè scorreva **solo se NON c'era un
soccorritore vicino**. Bastava che qualcuno arrivasse a portata perché il caduto diventasse
**immortale**; con un soccorritore che entrava e usciva dal raggio, il progresso di rianimazione si
azzerava a ogni uscita ma l'orologio restava fermo.

Il difetto toglieva alla meccanica la sua unica tensione — la **corsa** fra chi soccorre e il tempo —
e rendeva **insensata la taratura**: abbassare il valore non cambiava nulla proprio nei casi in cui
qualcuno accorreva. *L'utente ha tarato quel numero mentre la meccanica era rotta.* Rende anche vera
un'affermazione che avevo scritto in changelog 122 sul soccorso differito ("il bleed-out continua a
scorrere, quindi a volte l'uomo si perde"), che allora era **falsa**.

**Corretto**: il bleed-out scorre sempre; la rianimazione accumula in parallelo. La morte si valuta
**dopo** la rianimazione — altrimenti una canalizzazione completata nello stesso tick uccideva l'uomo
sul fotogramma della salvezza. Verificato: **10,0 s esatti**.

### E la correzione fa emergere una domanda di bilanciamento (KI #92)

Con il timer che finalmente scorre, il **differimento del soccorso** (122) e il bleed-out si
sommano: caduto a t=14,1 → soccorso differito **8,9 s** perché la zona è contesa → dispacciato a
t=23,0 → restano **1,1 s** per arrivare *e* canalizzare **4,5 s** → morto a t=24,0. **Zero
rianimazioni in 12.000 tick**: in area contesa è aritmeticamente impossibile.

Non ho toccato i valori: sono scelte di gioco e stanno nel BalanceEditor. Leve e numeri in KI #92 —
la più sensata è alzare `squad_bleedout_time` (10 → 20-25 s), che è ciò che rende il differimento
una scelta tattica invece di una condanna.

## 2026-08-04 (134) — Regola R1 chiusa: *Elimina* c'è in tutti i moduli che gestiscono definizioni

Arretrato di doc 39: *Elimina* mancava in **5 moduli su 7**. Si poteva creare e rinominare una
definizione ma non toglierla, e l'unico modo era cancellare il file fuori dall'editor — cioè
scavalcando le regole del progetto.

Nuovo comando condiviso **`editor::rename::deleteDefinition`**, accanto a `renameDefinition` perché
è lo **stesso dominio**: entrambi manipolano il file che *porta* l'id (ADR-001), e separarli avrebbe
significato due idee diverse di dove vivano le definizioni. Agganciato a Class, Entity e Weapon
Editor con conferma esplicita; Vehicle lo ha già via `AssetBrowser`; Map e Mission l'avevano.
Balance non gestisce definizioni, quindi non si applica. **Copertura completa.**

**Cosa NON fa, dichiarato nel popup e nel codice**: non ripulisce i cross-reference. È deliberato —
un riferimento rotto lo segnala `--validate` col file e il campo, mentre una pulizia automatica
cancellerebbe **in silenzio** scelte dell'autore (un roster che perde una riga senza dirlo). La
rinomina li aggiorna perché lì l'intento è *"ora si chiama così"*; qui è *"non c'è più"*, e cosa
farne altrove è una decisione di chi autora. Il `.bak` che accompagna la definizione se ne va con
lei, altrimenti un ripristino futuro resusciterebbe un id tolto apposta.

**Build-verified.** **Smoke test dovuto**: eliminare una definizione di prova in ciascuno dei tre
moduli e verificare che la lista si rigeneri e la selezione resti sensata.

## 2026-08-04 (133) — KI #86 residuo CHIUSO: stalli 18 → 3, e un falso positivo del mio strumento

Ripreso il filo lasciato prima della telemetria, usando la scatola nera costruita apposta. Flusso a
tre passi come da doc 12: leggi `stalli per causa` → prendi un id → `--trace-ai <id>`.

**Causa 5 — "Alert senza bersaglio" non aveva ANCORA un ramo di movimento.** La traccia di una
singola unità lo mostra come una riga di cronaca: immobile dal tick **1128 al 1206**, sguardo
congelato a −154°, e ripartita **nell'istante esatto** in cui è passata a Hunt. In changelog 121
avevo corretto solo il sotto-caso "stava manovrando"; questo è lo stesso difetto **senza** manovra —
perso il contatto, l'unità resta in Alert per `alertTimer` (3 s) e in quello stato cade fuori da
tutte le condizioni della catena di movimento.

Corretto estendendo il ramo Hunt a `Alert && nearest == 0 && hasLastKnown`. La pausa prima
dell'inseguimento è voluta (`alertTimer` = esitazione), ma **esitare non è pietrificarsi**: ora
l'unità avanza verso l'ultimo punto noto, a velocità ridotta perché il contatto è appena andato
perso. Le **transizioni di stato restano di chi è in Hunt**: un'unità in Alert che arriva sul punto
non salta a Search da qui, altrimenti scavalcherebbe `enterHunt`, che è dove si sceglie l'approccio.

**E un falso positivo del rilevatore, che valeva l'episodio più lungo mai registrato.** Dopo il fix
restava uno stallo da **33,5 s**: era l'entità 253, un alleato **messo a terra** al frame 699. Un
caduto non può muoversi né sparare *per definizione* — il rilevatore lo contava per tutta la
finestra di bleed-out più rianimazione. Escluso: lo stato "a terra" ha già la sua telemetria
(`member downed`, `soccorso differito`), duplicarlo come anomalia era solo rumore. **Seconda volta
in due giorni** che una guardia leggeva il dato sbagliato (la prima fu il funnel di missione).

### Il bilancio dell'intera indagine KI #86

| | episodi | tempo perso | più lungo |
|---|---|---|---|
| all'inizio (changelog 121) | **41** | ~122 s-AI | **26,6 s** |
| dopo mira + hide + manovra | 13-18 | ~46 s-AI | 16,3 s |
| **ora** | **3** | **9 s-AI** | **3,0 s** |

3,0 s è **esattamente la soglia di rilevamento**: nessun episodio la supera più.

**Nuovo riferimento per `--sim-ticks`: 3000 tick → 146 eventi** (era 124), ripetibile 146/146. Il
vecchio 124 **non è più valido e non è una regressione**: il fix cambia deliberatamente il
comportamento — le unità si muovono invece di restare ferme — quindi la traiettoria della
simulazione è un'altra. A 6000 tick il totale resta 211: a seconda della finestra il combattimento
sale o resta pari, che è il comportamento atteso di un sistema caotico e il motivo per cui la
decisione è stata presa sul contatore degli **stalli**, non su questo numero.

## 2026-08-04 (132) — Audit della documentazione: sei ADR erano fermi a "Proposed" da settimane

Passata di coerenza su ProjectDocs, verificata **contro il codice** e non a memoria. Tre difetti
reali, tutti della stessa famiglia — documenti che descrivevano un progetto più giovane di quello
che esiste:

**Venti ADR con lo stato sbagliato — e il mio primo passaggio ne aveva trovati solo sei.** Avevo
guardato la coda dell'elenco e concluso; poi la verifica finale ha mostrato altri **sedici**
`Proposed`. Correggo l'errore dicendolo: *un audit che guarda solo la fine della lista non è un
audit*.

Metodo di verifica, per non ripetere il giudizio a occhio: quante volte ogni ADR è **citato nel
codice**. `ADR-025` (WorldIntel) 4 file, `ADR-030`/`031`/`033`/`034` 7 file ciascuno, `ADR-032` 5,
fino a `ADR-039` (strutture) 9 file. Tutti implementati e misurati da settimane → portati ad
**Accepted — in force**: 025-038 più 039, 040, 042, 048, 049.
`ADR-041` (Droide Tattico come entità) era *Proposed* ma **superato** da ADR-044 → `SUPERSEDED`.

Restano *Proposed* **solo due**, ed entrambi correttamente: `ADR-021` (save di carriera, **0**
citazioni nel codice) e `ADR-047` (pipeline Blender, non implementata).

**`05_CurrentState` era fermo al 22 luglio.** Non conosceva le fasi AI A1-A5, lo split di `AiSystem`
in tre file, i prefab, `ModuleShell`, l'anteprima arma in mano, le classi nel roster, **né l'intero
livello di osservabilità**. Aggiunta una fotografia al 2026-08-04, verificata contro il codice.

**Le direzioni sul rendering non erano scritte da nessuna parte.** Nuovo **doc 43 —
`43_RenderScalability.md`** (Planned Feature, zero righe di codice): LOD degli asset, frustum
culling, soglia di distanza, simulazione a distanza; e cosa è **fuori scope**, cioè il passaggio a
VBO. `ADR-003` aggiornato con la decisione dell'utente — la build resta ottimizzata per questa
macchina, la compatibilità universale su Windows è un obiettivo **successivo e separato**, ed è
quello il momento in cui l'ADR andrà riaperto.

**Vincolo di macchina registrato** in `10_ProjectMemory`: il PC attuale è vecchio e verrà sostituito
a breve. I **numeri assoluti** delle misure di performance valgono solo qui e vanno ri-misurati; i
**rapporti** restano validi. Non si progetta l'architettura attorno ai limiti di una macchina in
uscita — ma 161k vertici per un fante restano sbagliati su qualunque hardware.

## 2026-08-04 (131) — O2 + O6: la mappa dei punti ciechi è CHIUSA, e KI #87 ha una risposta

Ultimi due buchi di doc 42, e insieme danno la risposta alla domanda aperta da due giorni.

**O2 completo** — il render era una zona unica: aggiunte `render.scena` (disegno 3D) e `render.ui`
(menu/HUD), più il conteggio dei **vertici spediti per frame**. Quest'ultimo è il numero che conta
davvero con il rendering client-side-array (ADR-003): senza VBO i dati risalgono alla GPU a **ogni**
draw call, quindi il costo segue i vertici, non le chiamate. "230 draw call" e "230 draw call da
161k vertici l'una" si ottimizzano in modi opposti, e senza questo campo erano indistinguibili.

**O6 asset** — inventario delle mesh al caricamento: quante, quanti vertici, quanta memoria, e le
otto più pesanti.

### La risposta a KI #87

| zona | ms | quota |
|---|---|---|
| frame | 40,6 | 100% |
| **render.scena** | **38,6** | **95,1%** |
| render.ui | 0,7 | 1,7% |
| **simulazione** (AI + crowd + tutto) | **1,17** | **2,9%** |

**1.447.949 vertici per frame** su 205 draw call ≈ **1,5 GB/s** verso la GPU. E l'inventario dice
di chi è la colpa: 337.626 vertici totali caricati, di cui **161.304 nel solo B1 Battle Droid** —
**dieci volte** il Clone Trooper (15.627). Con ~6 droidi in campo, il B1 vale da solo i **due terzi**
del traffico.

**Quindi "il massimo di AI cala" perché ogni droide aggiunto costa 161k vertici per frame.** Lineare
e brutale, e senza alcun rapporto con la complessità dell'AI — che pesa il 2,9% del frame. Per due
giorni abbiamo sospettato il sistema sbagliato, e nessuno dei due poteva saperlo: **non esisteva la
misura**. È esattamente la tesi di ADR-050.

Il rimedio più grande è di **contenuto** (decimare il B1), non di motore. Secondo: il **frustum
culling non esiste** — 205 disegnate su 206 esaminate, si disegna tutto anche fuori campo.
Direzioni e rischi in KI #87; nessuna intrapresa, perché la telemetria era la richiesta.

**Copertura ADR-050 completa**: O1 nav · O2 render · O3 sessione · O4 combat · O5 ability/veicoli ·
O6 asset · O7 missioni. Resta solo l'attribuzione per ARMA nel funnel di fuoco (serve un id sul
proiettile).

## 2026-08-04 (130) — O7: il funnel di MISSIONE, che trova un difetto al primo utilizzo

Gli eventi sugli obiettivi c'erano già, ma scattano solo sui **salti** (attivato / completato /
fallito). Conseguenza: una missione che **si impianta** non produce alcun evento — il caso che più
conta, quello in cui "non succede niente", era invisibile per costruzione.

Nuovo `Objective/stato missione` a cadenza fissa: per ogni obiettivo stato, tier, da quanti secondi
è attivo, il **motivo del fallimento** se fallito, e una **misura specifica del suo tipo**. Più il
contatore `attivi_senza_progresso`.

**Due correzioni durante la costruzione, e la seconda è la lezione.** La prima versione leggeva
`progress` e `holdTime` per tutti i tipi: ma `CaptureZone` non li tocca affatto (il progresso vive
nel command post, ADR-009), quindi segnalava come "ferma da 40 s" una missione che funzionava.
Corretto il testo mostrato... e non bastava: **anche la soglia d'allarme leggeva i campi sbagliati**,
quindi la riga diceva "0%" e il contatore continuava a mentire per conto suo. Ora ogni tipo produce
un avanzamento normalizzato 0..1 (o −1 = "non accumula nulla, non ha senso chiedersi se è fermo"), e
sia la misura sia l'allarme leggono quello. *Una guardia che mente è peggio di nessuna guardia.*

**E poi ha trovato un difetto vero** (KI #90): `firebase_alpha` non avanza **mai**. Per 150 secondi
di simulazione `capture_alpha` resta *attivo, post 'Alpha' team 0 al 0%*, e `hold_alpha` non si
attiva nemmeno perché dipende dal primo. **Non è la cattura a essere rotta**: nella stessa run ci
sono 15 eventi `CommandPost` con contese su Bravo e Charlie — è **solo Alpha** che nessuno raggiunge
mai. Il funnel dice *che* non avanza; il *perché* è lavoro di gameplay, annotato in KI #90.

## 2026-08-04 (129) — O5: ability e veicoli escono dal buio (e NON sono peso morto)

Due sistemi interi mai osservati. La lettura del codice dava un sospetto forte: in tutto il
progetto esiste **un solo punto di attivazione** di un'ability a runtime — `roll`, in `AiSystem` —
mentre `shield` e `command` vengono convertiti in componenti **allo spawn** e la loro
`AbilityState` non la guarda più nessuno. Sembrava il caso da manuale della domanda dell'utente:
*"cosa non funziona e aggiunge solo peso"*.

**La misura lo smentisce, ed è il motivo per cui si misura.** In una sim da 6000 tick:

| | valore |
|---|---|
| entità con ability / stati totali | 7 / 7 |
| stati **attivabili** a runtime | 7 |
| **roll attivati** | **48** |
| scudi attivi | 2 |
| **danno assorbito dagli scudi** | **339** |
| veicoli presenti / guidati | 1 / **0** |

Il roll parte 48 volte e gli scudi assorbono 339 danni: il sistema **lavora**. Quello che *non*
lavora è il veicolo — presente in mappa, mai guidato in tutta la simulazione: costa collider,
push-out del crowd e draw call per un'entità che nessuno usa. Non lo tocco (le AI non hanno ancora
un comportamento di guida, doc 19 Fase B), ma ora è un fatto misurato invece che un'impressione.

Nuovi eventi: `Combat/ability e veicoli` (entità con ability, stati totali/attivabili/in cooldown,
scudi e carica, veicoli e quanti guidati) + `roll_attivati` nel battito dell'AI + `scudo_assorbito`
nel funnel di fuoco. **`stati_attivabili` è la guardia che conta**: se un giorno va a zero mentre
delle entità portano ability, il sistema è diventato inerte e lo si vede subito.

## 2026-08-04 (128) — O4: il COMBATTIMENTO smette di essere una regex su stdout

Fino a ieri la mia metrica di esito principale — quella su cui ho deciso metà dell'indagine di
KI #86 — era **una regex su stdout**: `[Combat] Colpito!`. Si rompe se qualcuno cambia una stringa,
non dice chi ha colpito chi, e soprattutto **non ha denominatore**: "40 colpi a segno" non
significa nulla se non si sa se erano 50 colpi sparati o 500. Era l'anello più debole di ogni
misura fatta finora, ed è quello che ho chiuso.

**Nuovo funnel `Combat/funnel di fuoco`**, per team: sparati → a segno (di cui su zona hitbox) →
danno → a terra → uccisi, più i proiettili fermati dalla **geometria** e quelli **spenti nel
vuoto**. Il denominatore (`shotsFired`) vive in `World` perché i proiettili nascono in `AiSystem` e
`PlayerController` mentre gli impatti li vede solo `CombatSystem`: senza, il funnel partirebbe a
metà. Cadenza allineata a AI e navigazione (600 tick) — i tre battiti coincidono, ed è metà del
loro valore: *"in questa finestra hanno acquisito poco **e** l'accuratezza è crollata"* è una frase
che si può scrivere solo se le letture sono sincrone.

**Lo strumento si valida da sé**: 193 a segno + 235 sulla geometria + 1 spento = **429 = i colpi
sparati**. Il funnel chiude esattamente, quindi non perde eventi per strada.

### Primi numeri, e sono già rivelatori

| team | sparati | a segno | su zona | danno | uccisi |
|---|---|---|---|---|---|
| 1 (cloni) | 245 | 129 (**52,7%**) | 22 | 1.733 | **16** |
| 2 (droidi) | 184 | 64 (**34,8%**) | 9 | 702 | **2** |

**Asimmetria enorme e mai vista prima**: i cloni colpiscono una volta e mezza più spesso e fanno
**otto volte** le uccisioni. Parte è di design (i droidi sono cattivi tiratori), ma il divario in
uccisioni è molto più grande di quello in accuratezza — cioè non è spiegato dalla sola mira. Da
indagare quando si tornerà al bilanciamento; prima non era nemmeno una domanda formulabile.

**Il 55% di tutti i colpi finisce sulla geometria** (235 su 429). Il gate di fuoco verifica la LOS
prima di sparare, quindi questi sono colpi partiti su linea libera e finiti in una copertura: è la
dispersione dell'arma che li porta lì. Realistico, ma dice anche che oltre metà del volume di fuoco
lavora sulla copertura del bersaglio invece che sul bersaglio.

*Nota*: il vecchio conteggio a regex dava 211, il funnel 193 impatti. Le due cifre non coincidevano
perché lo stdout mescolava impatti, uccisioni e danni alle strutture in un unico prefisso — la
prova che la metrica era approssimativa oltre che fragile.

### O3 — la SESSIONE GIOCATA, che finora non esisteva nei dati

Nuovo evento `Player/sessione`, stessa cadenza degli altri tre: vivo/hp/posizione, colpi sparati,
ordini impartiti, metri percorsi, secondi vivo / morto / in mira, arma, uccisioni, alleati persi.
Le quattro letture (AI, navigazione, combattimento, giocatore) ora **coincidono nel tempo**, ed è
metà del loro valore: *"il giocatore era morto in quella finestra"* spiega da solo un crollo
dell'accuratezza di squadra, ma solo se le righe sono allineate.

**In `--sim` esce tutto a zero** — colpi 0, metri 0, secondi 0 — perché il giocatore è un
osservatore che non spara e non si muove. Non è un difetto: è la **conferma che lo strumento
distingue una simulazione da una partita**, che è precisamente la distinzione che mancava a ogni
conclusione di bilanciamento presa finora su battaglie fra soli bot.

**Verifica**: `--sim-ticks 3000` → 124 eventi, invariato. `--validate` pulito.
**Da verificare a mano**: giocare qualche minuto e controllare che `combat.jsonl` contenga righe
`sessione` con valori sensati (metri, colpi, tempo in mira).

## 2026-08-03 (127) — O1: la NAVIGAZIONE esce dal buio, e trova subito il 60% di query inutili

Chiuso il buco O1 di doc 42: `CrowdSystem` e `NavManager` avevano **zero eventi**, pur essendo il
secondo costo della simulazione e la causa residua sospettata degli stalli.

**Nuovo funnel `Nav/navigazione`**: richieste di movimento → a quale tolleranza si sono agganciate
al navmesh (2 m / 6 m / 14 m) → scartate perché fuori mesh; più le query di path e i loro
fallimenti; più lo stato degli agenti adesso (`agenti` / `con_meta` / `in_moto`). Quest'ultima
terna è la distinzione che mancava fra le tre cause dello stesso sintomo visibile — un'unità ferma:
*nessuno le ha chiesto di muoversi*, *glielo si è chiesto ma non c'è percorso*, *si sta muovendo e
il problema è altrove*.

### Un'ipotesi in meno e un'ottimizzazione in più

**Le mete dell'AI sono tutte camminabili**: 100% agganciate alla prima tolleranza, **zero** scartate,
zero path falliti. L'ipotesi "l'AI chiede di andare dove non si può camminare" — che sarebbe stata
la spiegazione naturale degli stalli residui — **è smentita**. Il problema non è lì.

**Ma il funnel ha mostrato altro**: su 34.040 richieste in 6000 tick, **23.302 (68%)** venivano
scartate come "stesso bersaglio" — *dopo* aver già fatto fino a tre `findNearestPoly`, che sono
ricerche spaziali. L'uscita anticipata esisteva, ma era posizionata dopo il lavoro che doveva
evitare. Aggiunto un confronto sulla richiesta **grezza** (5 cm) prima di qualunque query:
**query spaziali 100% → 40%**.

**Una trappola evitata, e come.** La prima versione confrontava solo la richiesta: sbagliata, perché
un agente il cui bersaglio decade non ne otterrebbe **mai più** uno — la cache direbbe "già chiesto"
per sempre. La condizione corretta è "stessa richiesta **e** l'agente ha ancora un bersaglio
valido". Stessa attenzione sull'invalidazione: gli indici agente si riusano, quindi la cache si
azzera in `removeAgent` e in `clear`, altrimenti il prossimo occupante eredita la meta del morto e
resta fermo.

**Verifica**: `--sim-ticks 3000` → **124 eventi, identico al baseline**. Le richieste di movimento
sono **34.040 prima e dopo**: stesso numero di decisioni, quindi la traiettoria non è cambiata.
*Onestà sul guadagno*: le query eliminate sono reali e misurate, ma il **costo del crowd non è
calato in modo misurabile** (0,11 → 0,13 ms, dentro il rumore fra finestre). `findNearestPoly` a
2 m è evidentemente economico rispetto al resto del crowd: il guadagno è in operazioni, non ancora
in millisecondi, e conterà a scala maggiore. Non lo vendo come una vittoria di performance.

*Nota di metodo*: per un attimo ho creduto a una regressione (combat 124 → 211) — stavo confrontando
una run da 3000 tick con una da 6000. A parità di lunghezza: 124 = 124.

## 2026-08-03 (126) — Il mistero del rallentamento ha una spiegazione semplice + verbosità a runtime

### Il degrado a ~100 ms: era l'EDITOR

L'utente ha chiarito di aver lanciato l'engine **dal GFEditor**, quindi con due processi attivi in
contemporanea — l'editor renderizza il suo viewport di continuo. Questo spiega esattamente ciò che
i tempi da soli non potevano spiegare: **render, simulazione e crowd rallentati tutti dello stesso
fattore ~10x**, e una finestra con **zero AI** e render a 85,8 ms. Non era il renderer: era il
processo che girava con metà macchina.

**Nel regime pulito il gioco sta a 16,7 ms con 6-8 ms di margine.** La conclusione operativa su
KI #87 cambia di segno: allo stato attuale **non c'è un problema di performance**, e la zona
`taratura_cpu` (changelog 125) serve ora a confermarlo alla prossima sessione — se sale insieme al
resto, contesa; se resta piatta, siamo noi.

*Nota di metodo*: ho scritto in 125 che dai soli tempi non si poteva concludere, invece di
attribuire al renderer. Era la scelta giusta — la causa vera non era nemmeno nell'elenco dei
sospetti.

### Quanto costa osservare — misurato, non stimato

Alla domanda *"limitiamo la telemetria pesante alla build Debug?"* ho risposto con una misura, non
con un'opinione: il costo della telemetria è ora esso stesso una zona di profilo.

| modalità | `session_latest` | costo | quota del frame |
|---|---|---|---|
| normale | **43 KB** | 0,0025 ms/frame | **0,01%** |
| `--telemetry-verbose` | 64 KB | 0,0161 ms/frame | 0,05% |

**La build Debug è la leva sbagliata**, per due motivi. *Non costa*: un centesimo di percento. E
*Debug è il posto sbagliato* — i bug che contano si presentano giocando, e si gioca in Release;
un'osservabilità che vive in Debug non c'è mai quando il problema si presenta (qui, per giunta, la
build Debug è inutilizzabile per via di ASan).

Il problema era di **leggibilità**: `AI/state change` da solo occupava il **39% del file** — 743
righe che non sono mai servite a una diagnosi, perché la storia di un agente la racconta meglio
`--trace-ai`. Declassato a `Debug`: **467 KB → 43 KB, −91%**, senza perdere nulla di ciò che leggo.

**La leva è la verbosità a runtime**: `Info` sempre acceso (aggregati e guardie), `Debug` con
`--telemetry-verbose` (o implicito con `--trace-ai`, altrimenti quel flag sarebbe silenzioso — lo
stesso difetto appena scoperto in `--stress`). Un evento sotto soglia esce **prima di
serializzare**: costa un confronto. È questo che ci permette di spingere molto più in là sulla
precisione senza pagarla quando è spenta.

## 2026-08-03 (125) — Prima sessione GIOCATA letta + telemetria organizzata in flussi

### Cosa dice la sessione dell'utente

29 finestre di profilo su una partita vera. **Due regimi nettissimi**:

| regime | frame | render (lavoro) | attesa vsync | simulazione |
|---|---|---|---|---|
| sano (finestre 9-24) | **16,7 ms** | 7-10 ms | 6-8 ms | 0,8-2,4 ms |
| degradato (3-7, 25-29) | **91-107 ms** | 86-102 ms | ~0,5 ms | 2-10 ms |

Nel regime sano il gioco è **agganciato ai 60 FPS con 6-8 ms di margine**: non c'è alcun problema
di performance. Nel degradato il frame è a ~10 FPS.

**L'ipotesi facile è sbagliata, e va detto.** Sembrava "scala col numero di AI" (40 AI → 103 ms;
11 AI → 16,7 ms), ma due fatti la smontano: (a) nella finestra 29 ci sono **zero AI** e il render
costa comunque **85,8 ms**; (b) render, simulazione e crowd rallentano **tutti dello stesso fattore
~10x**. Un fattore comune su sottosistemi indipendenti non è "il renderer è lento": è il processo
che gira più piano — throttling, contesa con un altro processo, stato del driver. Dai soli tempi
**non si distingue**, e concludere sarebbe stato inventare.

### Quindi ho costruito lo strumento che distingue

**Zona `taratura_cpu`**: una quantità **fissa** di lavoro puramente CPU (~10 µs), misurata ogni
frame. Se il suo costo sale insieme al resto → è la macchina. Se resta piatto mentre il render
esplode → è il nostro codice. Costa 10 µs e senza di essa le misure di performance restano ambigue
per sempre.

**Funnel di rendering** (buco O2): entità esaminate → disegnate → draw call. Prima misura:
**206 esaminate, 205 disegnate, 230 draw call/frame** a ~30 AI. Il rapporto 205/206 dice una cosa
sola e importante: **non esiste alcun culling** — si disegna tutto, sempre, visibile o no.

### Telemetria organizzata in flussi (richiesta dell'utente)

Un solo `session_latest.jsonl` erano **1943 righe / 658 KB** per pochi minuti di gioco, in cui il
profilo (29 righe) annegava fra 1043 cambi di stato dell'AI — e si troncava a ogni avvio, quindi
"fare un po' di prove diverse" significava conservarne una. Ora: `perf.jsonl`, `ai.jsonl`,
`combat.jsonl`, `world.jsonl`, `content.jsonl`, più `session_latest.jsonl` che riceve **tutto** come
indice cronologico (serve a correlare fra domini: è così che si è visto il legame coi cambi mappa).
La regola d'instradamento sta in **un posto solo**; un `system` non mappato resta fuori dai file di
dominio, così un sistema nuovo senza flusso **si nota**. All'avvio la sessione precedente va in
`storico/<data-ora>/` col suo `game_state.json`: nessuna prova si perde più.

**Cosa serve ora**: rigioca fino a rivedere il rallentamento e mandami `perf.jsonl`. Se
`taratura_cpu` sale con tutto il resto → è la macchina (e cerchiamo cosa contende). Se resta piatta
→ è il nostro render, e il primo sospetto è già identificato: zero culling su 230 draw call.

## 2026-08-03 (124) — Audit dei punti ciechi + PROFILER: la prima misura di costo mai fatta

Applicazione di ADR-050 su tutto il progetto, su richiesta dell'utente: *"capire quante cose non
puoi vedere"*. Nuovo **doc 42_Observability** con la mappa completa. Sintesi dell'audit, contata sul
codice: `MovementSystem`, `CrowdSystem`, `NavManager`, `Collision`, `DefinitionRegistry`,
`PlayerController` e **tutti i 13 file di `render/`** hanno **zero** telemetria; e in tutto il motore
esistevano **4 zone Tracy**. Nessuna misura di costo da nessuna parte — motivo per cui KI #87
("il numero massimo di AI sta calando") era una domanda *non rispondibile*, non "difficile".

**Nuovo `core/Profiler`**: zone annidate, sempre attive, report periodico nel JSONL con ms/frame,
ms di picco, quota del frame e chiamate. Copre `frame ⊃ render ⊃ attesa_vsync` e
`frame ⊃ simulazione ⊃ mondo ⊃ world.tick ⊃ {ai, crowd, combat, squad, movement, objective}`.
Non sostituisce Tracy (ADR-015): Tracy è una GUI che io non posso aprire né confrontare in uno
script — questo produce numeri nello stesso flusso di tutto il resto. Costo: due letture d'orologio
per zona. Più un evento **`inventario avvio`**: registry in 30 ms, e la dimensione reale di ogni
mappa (Training Ground: 167 box, **169 posizioni tattiche**, 23 settori, 22 route).

### Tre cose trovate il primo giorno

**`--stress N` era INERTE, da sempre.** Impostava i conteggi e il blocco `--sim` li sovrascriveva
con quelli della mappa — e `--stress` implica `--sim`. Richieste da 10 o da 100 AI davano identiche
**12 unità**. L'ho notato perché il costo non cambiava di un microsecondo al variare di N, che per
un test di scalabilità è impossibile. **Ogni profilazione "a scala" fatta finora non misurava
nulla.** Corretto: l'override esplicito dell'operatore vince sui conteggi di mappa.

**`attesa_vsync` va separata dal render.** Il primo risultato diceva "render = 97% del frame":
vero e completamente fuorviante — 7,3 ms su 16,7 sono attesa del vblank, non lavoro. Senza quella
separazione la conclusione sarebbe stata "ottimizzare il renderer", su un renderer che aspetta.

**C'è un transitorio di riscaldamento enorme**: prima finestra da 300 frame a **87 ms/frame**,
convergenza a ~16,7 entro la quinta. Al primo tentativo di test di scalabilità ho confrontato run a
maturità diversa e ottenuto il risultato assurdo che il costo *scendeva* all'aumentare delle AI.
Regola scritta in doc 42: `--sim-ticks ≥ 4200`, si legge **l'ultima** finestra.

### Il primo dato su KI #87

| AI vive | simulazione | ai | crowd |
|---|---|---|---|
| 9 | 0,73 ms | 0,16 | 0,25 |
| 18 | 0,81 ms | 0,26 | 0,36 |
| 31 | 1,66 ms | 0,39 | **1,05** |
| 36 | 1,51 ms | 0,38 | 0,93 |

A 36 AI l'**intera simulazione costa 1,5 ms** su un frame da 17-30 ms: il collo di bottiglia **non
è la simulazione**. E dentro la simulazione il `crowd` cresce più in fretta dell'AI (×4,2 di costo
per ×3,4 di unità, contro ×2,4). **Provvisorio**: le misure di render in headless variano fra 7 e
20 ms tra run, quindi la prova decisiva è una sessione giocata vera — ma ora basta giocare, il
profilo si scrive da solo nel JSONL.

**Da verificare a mano**: gioca una partita normale di qualche minuto e mandami il JSONL, così leggo
il profilo del caso che conta davvero invece che di una sim.

## 2026-08-02 (123) — ADR-050 (osservabilità obbligatoria) + la posa in mano si tara guardandola

### La regola nuova: ogni sistema nasce con la sua osservabilità (ADR-050, CLAUDE.md §5-bis)

Richiesta dell'utente: accanto agli strumenti di *authoring* servono strumenti di **osservazione
profonda**, pensati **soprattutto per l'agente AI**. La motivazione è documentata e misurata: su KI #86
**tre diagnosi consecutive** sono state fuorviate da metriche aggregate, e ogni volta la risposta è
arrivata solo guardando **una** unità. L'osservabilità non è comodità: è il canale sensoriale
dell'agente — ciò che non è strumentato viene rimpiazzato da ipotesi plausibili e sbagliate.

Tre livelli obbligatori prima di dichiarare completo un sistema: **sintomo** (non esito), **funnel con
denominatori**, **discesa alla singola entità**. Più le regole d'igiene: l'osservatore non decide, il suo
stato vive sul componente, le sonde costose si tolgono lasciando scritto quale risposta hanno dato, e
`--validate` è l'osservabilità dell'authoring. Copertura attuale: l'AI è strumentata; **navigazione, game
mode, missioni, ability e veicoli non lo sono**.

### Il DC-15X minuscolo in mano: tre difetti in fila

- **Causa immediata**: `DC-15X` non ha `hand_scale` (KI #49 sposta la posa in mano sull'ARMA). Senza,
  ricade sul `weapon_display` del corpo, tarato per il DC-15A → 0.4 su un mesh di dimensione nativa
  diversa. La dispersione dei valori spiega perché non può essere ereditata: 0.0015 (E-5C), 0.4 (DC-15A),
  1.2 (E5), **80** (Z-6) — quattro ordini di grandezza.
- **Perché non era stato preso dal gate**: il controllo `hand_scale` esisteva già, ma **solo sulle
  ENTITÀ**. Il caso in cui la posa sbaglia è precisamente *la CLASSE che cambia l'arma*, che non era
  coperto. Esteso alle classi (primaria e secondaria): il gate ora segnala **DC-15X, DC-17, T-21**, con
  l'azione concreta per correggerli.
- **Perché non si poteva correggere**: la posa si scriveva nel Weapon Editor e si guardava nell'Entity
  Editor. Con una scala che compensa la dimensione nativa del mesh, tararla senza vederla non è
  realistico. **Nuova anteprima "in mano" nel Weapon Editor**: carica un CORPO a scelta come modello
  principale e l'arma come attachment, slider live, scala **logaritmica** (con un drag lineare da 1.0 non
  si arriva mai a 0.0015). Se la posa non è autorata mostra il fallback che userebbe il runtime, con
  avviso — l'anteprima deve far vedere anche il problema, non solo la soluzione.

**E una copia chiusa mentre c'ero.** La formula della posa esisteva due volte — runtime ed Entity Editor —
allineate da un commento *"DEVE combaciare con..."*. Un commento non è un vincolo, e un'anteprima che
diverge dal gioco è peggio di nessuna anteprima: mostra una cosa e ne salva un'altra. Estratta in
`include/mini/game/WeaponHandPose.hpp` (solo glm, nessun peso di runtime), ora ha **tre consumatori e una
implementazione**.

**Da verificare a mano**: tarare DC-15X, DC-17 e T-21 con la nuova anteprima e controllare in partita che
combaci (l'anteprima usa la stessa funzione del runtime, ma non l'ho vista a schermo).

## 2026-08-02 (122) — Soccorso sotto tiro, e le CLASSI schierabili dall'editor

Due segnalazioni dal playtest, di natura molto diversa.

### 1. "Si buttano a sparare a qualcuno ignorando i nemici vicinissimi" — MISURATO, non è un bug

Prima di toccare la percezione l'ho quantificato: al momento del tiro, quanti nemici erano **più vicini**
del bersaglio scelto, e quel nemico era **colpibile**? Risultato: il **15%** dei tiri ha un nemico più
vicino entro 10 m, ma **0%** di quei nemici era colpibile — erano *tutti* dietro copertura. L'acquisizione
prende per costruzione il più vicino con LOS, e sta facendo la cosa giusta.

**È un problema di LETTURA, non di targeting**: dalla telecamera un nemico a 8 m dietro un muretto sembra
"lì", mentre l'unità genuinamente non può colpirlo. Nessun cambiamento al FOV — che sarebbe stato il fix
sbagliato per il problema sbagliato, e avrebbe ucciso il valore dell'aggiramento. Diagnostica rimossa dopo
la risposta.

### 2. "Vanno a rianimare buttandosi in mezzo alla mischia" — difetto vero, corretto

Questo sì. L'auto-soccorso dispacciava il compagno libero più vicino **senza guardare cosa ci fosse attorno
al caduto**: nessuna query esistente serviva allo scopo, perché `dangerAt` legge le danger zone *autorate*,
non i nemici vivi. Il soccorso è l'unica decisione della squadra che manda deliberatamente un uomo in un
punto preciso, ed era l'unica presa alla cieca.

- **Prima di partire** si contano i nemici vivi entro `squad_rescue_threat_radius` (12 m): oltre
  `squad_rescue_max_threats` (1) il soccorso è **DIFFERITO**, non annullato — il bleed-out continua a
  scorrere, quindi a volte l'uomo si perde davvero. È il costo della scelta, coerente con
  [[structures-degrade-not-block]]: si degrada una capacità, non la si spegne.
- **Durante il viaggio** la zona può scaldarsi: ora si annulla la corsa invece di arrivare in mezzo. Chi è
  **già accanto** al caduto non viene interrotto — buttare via canalizzazione e uomo sarebbe peggio.
- Si annuncia UNA volta per caduto (*"Zona troppo calda: soccorso in attesa"*): senza, sembra che la squadra
  ignori il compagno. Il flag vive sul `SquadComponent` del caduto, non nel sistema — la trappola
  [[systems-survive-world-initialize]].
- Entrambe le soglie sono **data-driven** (`data/config/gameplay.json`, ADR-043) **e esposte nel
  BalanceEditor**: `Raggio minaccia` = 0 disattiva il controllo e ripristina il comportamento storico.

Misurato (`--sim-ticks 6000`): 4 caduti, **5 soccorsi differiti** (con 2-4 nemici entro 12 m: esattamente i
casi descritti), 2 rianimazioni completate. Il meccanismo differisce quando è caldo e parte quando si
sgombra — non affama le rianimazioni. Eventi di combattimento 281 → 251: **conseguenza attesa** della
scelta (meno recuperi → meno fucili in campo), non una regressione.

### 3. Editor: si poteva schierare solo l'unità base — le CLASSI erano irraggiungibili

Segnalato provando ad aggiungere il Marksman a Training Ground. **ADR-023 prevede già** che un id di roster
sia un'entità-corpo **o una CLASSE** con `base_entity` (`classres::effectiveUnit` la risolve), e il gate
`--validate` lo accetta da sempre con la stessa regola. Era il **dropdown** del BalanceEditor a offrire solo
`allies()`/`enemies()`: con una sola entità alleata definita, l'unico schierabile era il Clone Trooper.
Prova del disallineamento: `ally_types` di Training Ground conteneva già `Heavy Trooper`, **che è una
classe** — visibile ma non riproducibile dalla UI.

Ora il combo elenca anche le classi, raggruppate sotto un separatore e marcate `[classe: id]`, con il lato
(alleato/nemico) dedotto dal `base_entity` — la stessa regola già usata dalla validazione, non una seconda
copia. Verificato end-to-end aggiungendo `marksman` al roster: spawn corretto, `--validate` pulito, mappa
poi ripristinata (l'ordine del roster è la sequenza di spawn: è una scelta di design dell'autore).

**Da verificare a mano**: che il Marksman schierato dall'editor si comporti da tiratore in partita.

## 2026-08-02 (121) — SCATOLA NERA per-agente, e il terzo "stato che avanza solo col bersaglio"

Richiesta dell'utente dopo il playtest (*"c'è un miglioramento ma non così evidente, alcune AI ogni tanto
rimangono comunque ferme"*): **poter vedere precisamente il comportamento delle singole AI**. È la correzione
giusta al metodo — le due diagnosi precedenti erano state fuorviate proprio da metriche aggregate.

**Nuovo `src/ecs/systems/AiTrace.cpp`** (terzo file del seam AiInternal, dopo lo split del monolite):
- **Rilevatore di stallo, sempre attivo, costo ~zero.** Un'AI in contesto di combattimento che per 3 s non
  si sposta (< 0,6 m) e non spara non è vita normale: è il sintomo. Alla soglia si registra UN evento con
  **tutto** il contesto decisionale (stato, bersaglio, evasivo + residuo di hide, manovra + distanza dalla
  meta, presidio, segnale torre, route, ruolo, soppressione, stuck, ultimo noto, ordine) e una **causa
  sospetta** attribuita dai flag. A fine episodio un secondo evento con la **durata vera** — senza, l'evento
  di soglia riporta sempre ~3 s e "il più lungo 29 s" resta un numero che non si può incrociare con nessuna
  unità. Aggregato per causa a ogni battito.
- **Traccia per-agente `--trace-ai <id>`** (spenta di default, `-1` = tutte): ogni decisione di quell'unità,
  campionata all'intervallo di sensing. È il microscopio da puntare **dopo** che il rilevatore ha detto quale
  unità guardare.
- Lo stato di osservazione vive in `AiComponent`, non nel sistema: la memoria di progetto ricorda che lo
  stato dentro un sistema sopravvive a `initialize()`; dentro il componente muore con l'entità, quindi una
  partita nuova non eredita stalli della vecchia. **Non decide nulla**: nessun ramo di comportamento lo legge.

**Cosa ha trovato, al primo colpo.** 41 episodi di stallo, il 63% **senza alcun flag che li spiegasse**.
Puntando la traccia su una singola unità (id 414) il bug si legge come una riga di cronaca: ingaggia al tick
2838 → **perde il contatto al 2850** → resta immobile fino al 2988, `repositionActive` acceso, posizione **e
sguardo** congelati.

**Causa: tutta la manovra viveva dentro il ramo `Alert && nearest != 0`.** Al primo LOS che si rompe — e si
rompe nel 61-72% dei casi (changelog 119) — lo spostamento si fermava, il suo timer smetteva di scorrere, e
**non esisteva alcun ramo di movimento per "Alert senza bersaglio"**: paralisi totale per ~3 s, fino alla
scadenza di `alertTimer` che portava a `enterHunt` e ripuliva il flag. Lo sguardo congelato peggiorava tutto:
il campo visivo segue il facing, quindi l'unità era anche cieca.

**È la TERZA istanza dello stesso schema** (fase di hide, changelog 118; fuoco durante la manovra, 118; ora
la manovra stessa): *stato che avanza solo dentro un ramo condizionato al bersaglio*. Risolto alla radice —
la manovra è uno spostamento verso un **posto**, e perdere di vista il nemico non annulla il terreno che si
voleva prendere: ora prosegue, arriva, e proprio lì ha la migliore probabilità di ri-acquisire. Senza
bersaglio si guarda dove si va.

**Misurato** (`--sim-ticks 6000`, ripetibile 281/281):

| | prima | dopo |
|---|---|---|
| episodi di stallo | 41 | **13** (−68%) |
| di cui senza causa evidente | 26 | **8** (−69%) |
| tempo-AI perso fermo | ~122 s | **46 s** (−62%) |
| acquisizione (% del cono) | 32% | **36%** |
| eventi di combattimento | 233 | **281** (+21%) |

Il combattimento sale **insieme** al sintomo: è la prima volta in questa indagine che i due si muovono nella
stessa direzione, e per questo il +21% è credibile invece che deriva di traiettoria.

**Restano 13 episodi** (8 senza causa evidente): il tetto residuo, da guardare col microscopio quando
tornerà utile. **Da verificare a mano**: col rallentatore, se le pause residue sono ancora percepibili.

## 2026-08-02 (120) — La RICERCA smette di tirare a caso: l'unica decisione che ignorava il mondo tattico

Cercando dove l'AI potesse "procurarsi una LOS" ho **smentito una mia affermazione del changelog 119**:
avevo scritto che un'unità senza LOS non fa nulla per ottenerla. Falso — `enterHunt` pesa già quattro
opzioni d'approccio (assalto diretto, aggiramento, **posizione di tiro** verificata con la linea, punto
dominante), pesate dal profilo e decorrelate dal `bias`. Quel pezzo funziona.

**Il buco era nella RICERCA, ed era netto.** `pickSearchPoint` sceglieva un punto **uniformemente casuale**
in 24×24 m attorno all'ultimo contatto: su una mappa con **169 posizioni autorate**, il grafo delle
coperture e i settori, era l'unica decisione dell'AI che ignorava per intero il mondo tattico. Ed è la
decisione peggiore da lasciare al caso proprio alla luce di quanto misurato in 118-119: il collo di
bottiglia è la **LOS**, non il campo visivo — e cercare da un punto che non vede la zona è cercare senza
poter trovare.

Ora la ricerca **chiede al mondo**: `bestFiringPosition` verso l'ultimo contatto (verifica linea, settore e
gittata — è il motivo per cui si usa questa e non una copertura qualsiasi). Il centro è **jitterato** di
±8 m: senza, tutte le unità e tutti i tentativi successivi ricadrebbero sulla stessa "migliore" posizione e
la ricerca smetterebbe di essere una ricerca. **Fallback conservato** al punto casuale quando il mondo non
offre nulla che veda quella zona — mappa povera di posizioni, o zona non coperta.

**Misurato** (`--sim-ticks 6000`, Training Ground): **76%** delle ricerche usa il mondo, 24% cade nel
fallback; acquisizione **28% → 32%** dei nemici nel cono; `fermi` 0, congelamenti 0, hide max 1,8 s.
Eventi di combattimento 236 → 233, cioè invariato entro la divergenza — e coerentemente col metodo fissato
in 118, **non è su quel numero che si decide**.

Nuove guardie permanenti `ricerca_tattica` / `ricerca_a_caso`: se un giorno il fallback dominasse, il
difetto sarebbe di **autoring** (la mappa non ha posizioni che vedano le zone contese), non di AI — e si
legge subito lì invece di dedurlo dal comportamento.

**Da verificare a mano**: col rallentatore, che le unità che perdono il contatto si portino su posizioni
sensate invece di vagare.

## 2026-08-02 (119) — KI #86 causa 3: NON è la mappa. E un mio numero sbagliato, corretto

Restava da capire dove muore il 61-72% delle occasioni fra "nemico nel cono" e "LOS ok". Ho aggiunto un
out-param facoltativo `outBlocker` a `physics::hasLineOfSight` (gratis se inutilizzato) e classificato i
bloccanti a runtime. **Il primo risultato era sbagliato e va detto**: usavo un raggio **fisso** di 3 m dal
CENTRO del bloccante per decidere se fosse "geometria autorata" o "muta", e concludevo **57% muta**. Per un
muro largo 7 m — figuriamoci l'impalcato del ponte, largo 31 m — quella soglia non ha senso: le coperture
autorate stanno ai BORDI. Verificato sui dati della mappa: i tre bloccanti principali (impalcato 1277
blocchi, muro nord 1182, blocco centrale 597) hanno posizioni tattiche a **3,3-5,4 m**. Tutte contate come
assenti. **Il 57% era un artefatto del metodo, non una proprietà della mappa.**

**Lo stesso controllo, fatto bene, sta nell'editor.** Nuovo difetto `UnmarkedCover` in
`analyzeTacticalHealth` (quindi anche nel gate `--validate`): un box con collider che taglia davvero un tiro
al busto (mezza altezza ≥ 0.45 m, pianta ≥ 1.5 m) e **senza** posizione tattica entro `mezza pianta + 2,5 m`
— soglia **proporzionata all'oggetto**, che è la lezione dell'errore qui sopra. Gli impalcati orizzontali
sono esclusi: sono pavimenti, li giudica `BlindVertical`. Cliccabile come gli altri avvisi (nuova banda di
selezione `Target::Geometry` → indice diretto del box). Su **Training Ground: 4 ostacoli**, pannelli sottili
agli angoli, tutti severità 0. **La mappa è autorata bene.**

**Conclusione — la causa 3 non è un difetto di mappa, ed è in gran parte fisiologica**: è uno sparatutto a
coperture, i bersagli stanno dietro coperture messe apposta. **Il buco vero è sul lato AI**: un'unità senza
LOS oggi non fa **nulla** per procurarsela — la manovra (ADR-035) si valuta solo quando si HA già un
bersaglio. Diventa la voce **A6-bis "manovra per acquisire"**, da decidere come design prima che come codice:
è esattamente il tipo di aggiunta che, fatta d'istinto, produce AI che si espongono a caso.

La diagnostica a runtime è stata rimossa (costava una LOS extra e una mappa per-entità su ogni acquisizione
fallita, con KI #87 aperto). Resta `outBlocker`, che non costa nulla, e il controllo statico, che costa zero
a runtime perché gira in fase di autoring.

**Verificato**: build Release pulita; `--validate` esegue il nuovo controllo (0 problemi, 23 avvisi su
Training Ground). **Da verificare a mano**: che gli avvisi di geometria siano cliccabili e selezionino il
box giusto nel Map Editor.

## 2026-08-02 (118) — KI #86: il funnel d'ingaggio, e due difetti che a occhio si confondevano

L'utente riportava *"è estremamente comune vedere AI che stanno davanti a dei nemici senza sparare"* e
*"si piazzano dietro una copertura e restano lì"*. La mia ipotesi principale era **FOV senza scandaglio**
(l'AI guarda dove cammina → angoli ciechi permanenti). **La misura l'ha smentita.**

**Lo strumento prima del fix.** Nuovo funnel d'ingaggio nella telemetria: `occ_in_raggio` → `occ_nel_cono`
→ `occ_acquisito`, poi il gate di fuoco decomposto per CAUSA (`gate_stato`, `gate_evasivo`,
`gate_cooldown`, `gate_los_tiro`, `gate_sparato`). Serve a separare **"non vede"** da **"vede e non spara"**
— due bug diversi, in due punti diversi, indistinguibili guardando lo schermo. Risultato: il campo visivo
costa il **5-8%** (non è la causa), mentre fra "nel cono" e "acquisito" si perde il **61-72%**.

**Difetto 1 — l'AI non VEDEVA bersagli che sapeva COLPIRE.** L'acquisizione mirava al `transform` nudo, il
gate di fuoco al busto alto (`+AI_HALF_Y*0.7`): il fix interim di KI #82 era stato applicato al tiro e mai
all'acquisizione. Un muretto vicino al bersaglio tagliava il raggio d'avvistamento ma non quello di tiro.
Misurato **sulla stessa traiettoria** (calcolando entrambi i punti nella stessa run, perché fra run diverse
la simulazione diverge): **13,2% di tutte le acquisizioni** esistevano solo grazie al punto alzato. Corretta
la stessa asimmetria nel FocusFire, dove un designato dietro un muretto non veniva mai preso.

**Difetto 2 — la fase di hide si CONGELAVA.** `exposeTimer` scorreva solo nel ramo "Alert + bersaglio + non
in manovra" — ma nascondersi dietro una copertura è **esattamente** ciò che rompe il proprio LOS: perso il
bersaglio, il timer si fermava e `evading` (che chiude il gate di fuoco) restava attivo a tempo
indeterminato. Stesso congelamento entrando in manovra, dove il commento del codice prometteva *"il fuoco
resta autonomo"* — promessa falsa per chiunque manovrasse mentre era in hide (fino a 1492 tick-AI per
finestra). A/B controllato: **senza fix 3549 tick congelati e fase di hide fino a 26,6 s** (contro
`hide_duration_max` = 1,8 s); **con fix 0 e massimo 1,8 s**. È la spiegazione letterale del sintomo.

**Un tentativo di fix è stato scartato dalla misura.** La prima versione azzerava anche `hasCover` alla
perdita del bersaglio: il combattimento è **calato del 17%** (255→212 eventi) perché senza bersaglio il LOS
sfarfalla di continuo e le AI venivano sfrattate dalle posizioni autorate — che sono proprio quelle con le
buone linee di tiro. Sostituita con la versione minima: il timer scorre, la copertura resta.

**Metodo, esplicito.** Gli eventi di combattimento aggregati **non** decidono questo bug: fra due run la
simulazione diverge e la differenza non è attribuibile a un cambio. La decisione è stata presa su
`evasivo_durata_max_s`, che misura **il sintomo** invece dell'esito. Volume finale 240 → 236: invariato
entro la divergenza — il guadagno qui è "le AI non si bloccano più", non "si spara di più".

**Verificato**: build Release pulita; `--sim-ticks 6000` con 0 congelamenti e hide max 1,8 s.
**Da verificare a mano**: lo smoke test col rallentatore (`[`/`]`) — se le AI ancora si piantano dietro le
coperture, la causa non è più questa. **Resta aperta la causa 3** (61-72% di perdita in LOS, vedi KI #86).

## 2026-08-02 (117) — A5 (parte 2): i PESI dell'utility in un posto solo — `AiUtility.hpp`

L'AI di questo progetto **era già una Utility AI**, ma non lo si poteva vedere: ogni scelta (quale copertura,
quale posizione di tiro, quale fronte rinforzare) nasceva da un punteggio i cui pesi erano **numeri magici
sparsi in nove formule su tre file**. Non esisteva un punto da cui rispondere a *"quanto conta il riparo
rispetto all'importanza autorata?"*, e tararli significava cercarli a grep sperando di trovarli tutti — è
esattamente il modo in cui due formule che dovevano essere coerenti divergono in silenzio (già successo:
torre vs comandante, changelog 88).

- **Nuovo `include/mini/game/ai/AiUtility.hpp`**: gli **8 bilanci** dichiarati insieme e commentati per
  *intento*, non per valore — `kCover` (mi riparo), `kFlank` (aggiro), `kOverwatch` (copro chi avanza),
  `kFiring` (colpisco restando coperto), `kHold` (presidio), `kAdvantage` (buon terreno), `kPicture` (quadro
  tattico della torre), `kSector` (quanto conta questo fronte). **Le differenze fra i blocchi SONO il
  design**: Hold pesa protezione e importanza alla pari, Advantage premia l'importanza, Flank premia
  l'angolo nuovo. Affiancate diventano discutibili invece che implicite.
- **Convertite**: `bestCoverToward`, `bestFlankingPosition`, `bestOverwatchForPosition`, `bestFiringPosition`,
  `bestHoldPosition`, `bestAdvantageInArea` (WorldIntel.cpp) + `sectorTacticalWeight`, `updateAllyTactical`,
  `bestOrderPosition` e il bonus di stance del comandante (AiCommandLayer.cpp). Zero formule con costanti
  inline rimaste nei due file.
- **`TAC_FIRE_BONUS` resta in GameConfig** di proposito: è una leva di *gameplay* già esposta, chi tara la
  cerca lì. `AiUtility` ospita i pesi *interni* al ragionamento.

**Comportamento invariato — e questa volta misurato bene.** I valori sono **esattamente** quelli già in uso:
non ho sostituito le curve con quelle "raccomandate" da doc 40 §6, perché farlo insieme al refactor avrebbe
reso impossibile attribuire una differenza all'uno o all'altra — e c'è un bug aperto (KI #86) proprio sul
comportamento di ingaggio. Verifica: `--sim-ticks 3000 --map "Training Ground"` → **128 eventi `[Combat]`,
identico al baseline**, e ripetibile (111 "Colpito" su due run consecutive). Build Release pulita.

*Cambiare i pesi è il passo successivo e separato: ora si fa in un file, e si misura con lo stesso comando.*

**Cosa NON è verificato**: nulla di visivo cambia, quindi non serve smoke test dedicato — ma la prossima
partita è comunque l'occasione per l'osservazione di KI #86 (AI in Alert senza bersaglio).

## 2026-08-02 (116) — A5 (parte 1): ISPETTORE del ragionamento AI — si vede il perché, non solo il risultato

Il piano prevedeva *utility formalizzata + ispettore*. Ho invertito le priorità dentro A5 e fatto prima
l'**ispettore**, perché l'utente ha appena segnalato che *"è estremamente comune vedere AI che stanno davanti
a dei nemici senza sparare"* (KI #86) — e quella è un'osservazione che senza strumenti **non si può
indagare**: si vede il risultato, mai la causa.
- **Nel dump di stato, per ogni AI**: `facing_deg` (dove GUARDA) + `fov_deg` (quanto è ampio il suo cono),
  `target` (0 = nessun bersaglio), `suppression`, `role`, `evading`, `reposition`. Con questi si incrocia
  dove guarda un'unità con dove sono i nemici e si capisce **quale gate la sta bloccando**.
- **Collegato all'uscita di `--sim-ticks`**: ogni misura lascia ora anche uno snapshot ispezionabile. Senza,
  l'ispettore sarebbe raggiungibile solo premendo F12 al momento giusto in partita — inutile per una
  diagnosi headless.
- **Ha già prodotto la prima evidenza su KI #86** (in trenta secondi): 12 AI → 8 in Alert, **7 senza
  bersaglio**, 5 in `evading`. Le unità sanno di essere in combattimento ma non acquisiscono: **il gate che
  fallisce è l'ACQUISIZIONE, non il tiro**. Annotato in KI #86 col prossimo passo diagnostico.
- Build-verified (Release). **Resta di A5**: la formalizzazione delle curve di utility in un modulo
  dichiarativo e tarabile — refactor a comportamento invariato, verificabile con `--sim-ticks`.

## 2026-08-02 (115) — Velocità della simulazione: rallentare il MONDO, non il giocatore

Richiesta utente: poter rallentare (e un po' accelerare) la simulazione per osservare movimenti e dettagli,
**restando liberi di muoversi normalmente**, con un menu in alto a destra destinato a crescere.
- **Cinque livelli** (`SIM_SPEEDS` 0.1x · 0.25x · 0.5x · 1x · 2x) con indicatore in alto a destra che
  evidenzia quello attivo.
- **Distinzione chiave rispetto alla slow-mo della ruota** (che esisteva già): quella rallenta TUTTO,
  giocatore incluso, ed è una scelta di *gameplay*; questa scala solo il **mondo** e lascia il giocatore a
  velocità piena — è uno *strumento di osservazione*. Senza la distinzione, rallentare per guardare da vicino
  avrebbe reso lentissimo anche il camminare, e raggiungere il punto da osservare un'eternità.
  Implementazione: `timeScale` (mondo) separato da `simElapsed` (giocatore), che ora usa solo `wheelScale`.
- **Comandata da TASTI `[` e `]`**, non solo dal pannello: in partita/osservazione il mouse è **catturato**
  per guardarsi intorno, quindi un menu solo-cliccabile sarebbe inutilizzabile proprio nel momento in cui
  serve. Toast di conferma a ogni cambio.
- Il pannello è pensato come **contenitore per futuri comandi di osservazione**, non come indicatore isolato.
- Build-verified (Release); `--sim-ticks` invariata (128 eventi, `fermi` 0) → nessun effetto sulla
  simulazione headless, che gira a velocità 1x.

## 2026-08-02 (114) — A4: RUOLI di combattimento — la squadra si divide i compiti (doc 40 Fase 2 COMPLETA)

Ultimo passo della Fase 2. Prima ogni soldato **tirava i dadi da solo** (`aiRand01() < flankChance`) per
decidere se aggirare: con 6 unità potevano aggirare **tutte insieme** (nessuno che fissa il nemico) o
**nessuna** (assalto frontale in massa). Il coordinamento era un caso statistico, non una decisione.
- **Ruolo assegnato all'INGAGGIO per SATURAZIONE**: 1 sopprime · 2 aggira · 3 avanza. Si sceglie fra i ruoli
  non ancora saturi, per **affinità col profilo** (chi ama la copertura sopprime, chi è aggressivo e incline
  al fianco aggira — parametri già autorati, nessun nuovo dato). Stesso principio dell'occupancy delle
  posizioni: chi sceglie dopo trova occupato ciò che è già preso.
- **Composizione desiderata** ~½ sopprime, ⅓ aggira, resto avanza: una squadra che aggira tutta lascia il
  nemico libero di muoversi, una che sopprime e basta non conclude nulla.
- **Chi SOPPRIME non manovra più** dopo essersi sistemato: resta a tenere il nemico sotto tiro — ed è
  esattamente ciò che rende possibile l'aggiramento altrui. Ha senso **solo perché A3 morde davvero**.
- **Il ruolo si RILASCIA perdendo il contatto** (`enterHunt`): al prossimo scontro la squadra si
  ridistribuisce sulla situazione nuova invece di trascinarsi compiti di una battaglia finita.
- **Leggibilità scritta INSIEME alla meccanica** (regola imposta nel 113, non rifinitura successiva): si
  annuncia solo l'aggiramento di un compagno — l'informazione che cambia le scelte del giocatore ("qualcuno
  gira, io tengo il fronte") — e solo per la sua squadra, altrimenti il feed diventa un bollettino.
- **Misurato** (`--sim-ticks 3000`): tutti e tre i ruoli assegnati — sopprime 6-11, aggira 3-4, avanza 2-3
  (≈55/30/18%, vicino alla composizione voluta); `manovre_avviate` **salite** a 7-11 (da 5-10),
  `fianco_trovato` vivo, `fermi` **0**.
- **Onestà sul dato che è calato**: eventi di combattimento 151 → **128**. Interpretazione *plausibile ma non
  dimostrata*: più unità in manovra d'aggiramento e chi sopprime fermo a fissare = meno duello frontale, più
  combattimento posizionale. Se in playtest risultasse "fiacco" invece che "manovrato", le leve sono la
  composizione (quota di chi sopprime) e `SUPPRESSION_PINNED`.
- Build-verified (Release). **Fase 2 della roadmap AI COMPLETA** (A3 soppressione + A4 ruoli).

## 2026-08-02 (113) — Soppressione LEGGIBILE: un sistema che non si vede, per il giocatore non esiste

Riscontro utente dopo il (112): *"mi sembra ci sia una differenza, ma non riesco a dirti in maniera sicura se
funziona"*. **Questo è un dato, non un'incertezza da liquidare**: se chi sa cosa cercare non riesce a vederlo,
per un giocatore quel sistema non esiste — e il requisito del progetto è *soldati credibili*, cioè
PERCEPIBILI. C'è anche un problema pratico: senza leggibilità la soppressione non è tarabile a occhio.
- **Annuncio della TRANSIZIONE** (non dello stato, che spammerebbe ogni tick) nel feed eventi già esistente:
  *"Nemico INCHIODATO: non avanza finché lo tieni sotto tiro"* / *"Compagno INCHIODATO dal fuoco nemico"*.
  Il giocatore capisce sia perché un nemico ha smesso di avanzare, sia che il proprio fuoco di soppressione
  sta funzionando. Nessuna posa richiesta ([[animations-blocked]]).
- **Misurato che informa e non spamma**: 5-12 annunci per finestra contro 446-1060 tick inchiodati → circa
  **1 messaggio ogni 60-90 tick**. Un canale che spamma smette di essere letto, quindi il rapporto è la
  metrica giusta, non il totale.
- **Errore di metodo mio, corretto**: il primo test cercava la stringa su **stdout**, ma `pushEvent` scrive in
  `eventFeed` (letto dall'HUD) e non passa da stdout né dalla telemetria → il test dava "0 eventi" **in
  qualunque caso**. Un test che non può fallire non è una verifica. Sostituito con un contatore
  (`annunci_inchiodato`), misurabile headless.
- **Nota di indirizzo**: percezione (A1), confidenza (A2) e soppressione (A3) funzionano nei numeri ma sono
  quasi invisibili in gioco. Sommandone altri si otterrebbe un comportamento "diverso" di cui nessuno sa dire
  il perché — e a quel punto un bug sarebbe indistinguibile da un sistema che lavora. La leggibilità va
  trattata come parte della feature, non come rifinitura successiva.

## 2026-08-02 (112) — A3: SOPPRESSIONE — il fuoco che non colpisce conta lo stesso (doc 40 Fase 2)

Il salto "militare". Finora essere sotto tiro non cambiava nulla se non venivi colpito: una sparatoria era un
duello a chi mira meglio. Ora un colpo che **manca** ma passa entro `SUPPRESSION_NEAR_MISS` (2.2 m) accumula
soppressione, che decade da sé (`τ` = 2.5 s).
- **Rilevamento a costo ~zero**: riusa il SEGMENTO del proiettile già calcolato in `CombatSystem` per
  l'anti-tunneling (distanza punto-segmento), nel ramo "ha mancato" che prima faceva solo `continue`.
- **TRE effetti**, perché un numero senza conseguenze sarebbe l'ennesimo dato inerte (lezione KI #25b):
  · **si mira peggio** (`SUPPRESSION_ACC_PENALTY`) → il fuoco di soppressione diventa una tattica reale
    invece che munizioni sprecate; · **ci si copre molto più volentieri** (`SUPPRESSION_COVER_BONUS`) → una
  raffica fa abbassare la testa; · **chi è INCHIODATO non attraversa lo scoperto** per manovrare
  (`SUPPRESSION_PINNED`) → è ciò che rende possibile "una squadra fissa, l'altra aggira".
- **Il rischio era la taratura** (dal piano: *"tarare senza paralizzare le AI"*), quindi DUE contatori invece
  di uno: il numero che conta non è `tick_soppressi` ma il **rapporto** con `tick_inchiodati`.
- **Misurato** (`--sim-ticks 3000`): soppressi 2721-4076, inchiodati 446-1060 → **12-26%**, cioè la pressione
  preme senza paralizzare. `manovre_avviate` 5-10 (non crollate), `fermi` **0**, eventi di combattimento
  128 → **151**: il combattimento si è INTENSIFICATO, non bloccato. Taratura buona al primo tentativo.
- Telemetria permanente `tick_soppressi`/`tick_inchiodati`. Build-verified (Release).
- **Smoke test dovuto**: sparare vicino a un nemico senza colpirlo e vedere se si abbassa/perde mira; con una
  squadra, verificare che i soppressi smettano di manovrare allo scoperto.

## 2026-08-02 (111) — A2: CONFIDENZA sui contatti — un'informazione che invecchia (doc 40 Fase 1)

Secondo passo del binario AI. Finora un contatto era un **fatto**: "il nemico è lì", che l'avessi visto un
istante fa o sentito sparare dieci secondi prima, a qualunque distanza. Ora è un'**informazione con una
qualità**, che decade: `c(t) = c₀·e^(−t/τ)` (τ = `CONTACT_CONFIDENCE_TAU`, 6 s).
- **Sorgenti con qualità diversa**: la VISTA dà posizione e identità (confidenza 1.0), l'UDITO solo una
  direzione approssimata (0.4). È ciò che rende l'udito utile **senza renderlo onnisciente**.
- **La confidenza CAMBIA IL COMPORTAMENTO** (altrimenti sarebbe l'ennesimo dato inerte, lezione KI #25b):
  sopra `CONTACT_CONFIDENCE_ENGAGE` → "so dov'è", ci si va per ingaggiare (comportamento storico); sotto →
  "lì è successo qualcosa", il punto diventa una META DI PERLUSTRAZIONE (`searchX/Z`). È la differenza fra un
  soldato che **indaga** e uno che rincorre un fantasma con assoluta certezza.
- **Misurato** (`--sim-ticks 3000`, deterministico): entrambi i rami vivi — `contatti_certi` 790-2177,
  `contatti_da_indagare` 307-1612. Dinamica sensata: nel primo report **53% investigazioni** (si sente
  sparare e si va a vedere), poi prevale l'ingaggio quando il contatto visivo si consolida. `fermi` 0;
  eventi di combattimento 116 → **128** (più unità che si muovono verso i rumori).
- Telemetria permanente: `contatti_certi` / `contatti_da_indagare`. Build-verified (Release).
- **Fase 1 della roadmap AI COMPLETA** (A1 percezione + A2 confidenza). Prossimo: **A3 soppressione**.

## 2026-08-02 (110) — Creazione prefab: zona VISIBILE e selezione rifinibile con Shift+click

Segnalazione utente: la creazione da zona usava un raggio **invisibile** — si doveva indovinare cosa sarebbe
finito nell'asset — e mancava un modo per correggere la presa. Esattamente il "lavorare a tentativi" che
questo ciclo di lavoro punta a eliminare.
- **Zona visibile**: disco ciano sul terreno col raggio reale, e un **rombo sopra ogni elemento incluso** —
  leggibile anche quando gli oggetti si sovrappongono.
- **Rifinitura con Shift/Ctrl+click** (come nei file manager e nei software 3D): il raggio fa la presa
  grossolana, il click aggiunge/toglie il singolo elemento. Al primo ritocco la selezione del raggio viene
  **congelata** (si parte da ciò che si sta già vedendo, non da una lista vuota) e da lì comanda la lista
  manuale; un pulsante "Torna al raggio" annulla i ritocchi.
- **Centro CONGELATO all'apertura** del popup, non il focus corrente della telecamera: altrimenti la
  selezione cambierebbe sotto gli occhi mentre si scrive il nome.
- **Una sola verità**: `prefabZoneCollect` decide cosa entra ed è usata **sia** dall'anteprima nel viewport
  **sia** dal salvataggio. Con due funzioni separate si vedrebbe evidenziata una cosa e se ne otterrebbe
  un'altra — la divergenza che questo progetto paga sempre caro.
- Uscita dalla modalità gestita anche quando il popup si chiude cliccando fuori (niente disco orfano).
- Build-verified (Release), `--validate` 0 errori. **Smoke test dovuto**: aprire "Crea prefab da zona…",
  regolare il raggio, rifinire con Shift+click, creare e verificare il contenuto dell'asset.

## 2026-08-02 (109) — UN SOLO NOME per mappa + `AssetBrowser` adottato nel pilota (KI #84)

Due interventi con lo stesso principio: **un'identità sola, uguale da qualunque parte la si guardi**.
- **Map Editor, rename unificato** (richiesta utente): c'erano **due caselle di testo affiancate** con
  semantiche diverse — una cambiava il *nome visualizzato* (campo `name`), l'altra faceva il *rename vero*
  (file + cross-reference). Due modi di "cambiare nome" con effetti diversi sono una trappola, e occupavano
  la toolbar in permanenza. Ora: **un pulsante "Rinomina…" + popup**, come Nuova mappa ed Elimina. Il rename
  allinea filename, id e `name`; il campo `m_mapDisplayName` è stato **rimosso** (niente residuo che
  ricrei la divergenza) e il save scrive `j["name"] = m_mapId`.
- **KI #84 risolto (violazione ADR-001)**: `MapEditor::loadMaps` leggeva l'id **dal contenuto del file**
  (`j.value("id", …)`) mentre il runtime usa il filename → un `id` stantio faceva mostrare all'elenco un nome
  diverso da quello reale. Era **letteralmente KI #21**, il bug che ADR-001 doveva eliminare. Fix: una riga.
- **`AssetBrowser` adottato in `VehicleEditor`** (prima adozione, ADR-049 R1): il modulo aveva "Nuovo veicolo"
  ma **non** Duplica, Rinomina, Elimina — ora li ha tutti, con le regole applicate per costruzione. La
  selezione locale si allinea **per ID** e non per indice: gli indici non sopravvivono a una creazione o a
  un'eliminazione, l'id sì. Rimosso il vecchio blocco "Nuovo veicolo", ora ridondante.
- **Build-verified** (Release, GFEngine + GFEditor, 0 errori) e `--validate`: **0 errori**, 4 mappe caricate,
  prefab espansi (2 istanze → 6 box + 6 posizioni), salute tattica **0 problemi** su tutte le mappe.
  *(La prima build era fallita con `LNK1104` perché l'editor era aperto — lock noto, CLAUDE.md.)*
- **Smoke test dovuto**: Map Editor → "Rinomina…" cambia il nome ovunque (elenco, file, partita);
  Vehicle Editor → Nuovo/Duplica/Rinomina/Elimina funzionano e la selezione resta coerente.

## 2026-08-02 (108) — `AssetBrowser`: ciclo di vita di una definizione in un posto solo (ADR-049, R1)

Secondo componente dello scheletro. Colma il buco maggiore dell'audit: **Elimina manca in 5 moduli su 7**,
*Duplica* in 3, e ogni modulo si riscrive scansione della cartella e pulsanti.
- **Crea · Duplica · Rinomina · Elimina** in `editor/include/framework/AssetBrowser.hpp` (header-only),
  parametrizzato su cartella, titolo, categoria di rinomina e contenuto di default. Non conosce il CONTENUTO
  delle definizioni: resta composizione (ADR-049), non un framework che impone la forma del modulo.
- **Le regole del progetto diventano strutturali, non memoria**: id = filename stem (ADR-001, mai letto dal
  contenuto — è l'errore di KI #21 e #84); rinominare passa dal COMANDO `renameDefinition` (ADR-010), mai
  "salva con nome nuovo" che lascia orfani (KI #7); ogni scrittura da `saveJsonRMW`; duplicare aggiorna il
  nome visualizzato (altrimenti near-duplicate); eliminare **chiede conferma** e dichiara ciò che NON fa (i
  riferimenti altrui restano rotti, li segnala `--validate`).
- **Ritrovamento che conferma la tesi dell'utente**: `getDataDir()` è duplicato come helper locale in **4
  moduli**, e `DataPath.hpp` documenta che lo stesso problema era già esploso — *"le otto copie erano già
  divergenti […] un editor che salva dove il gioco non legge"*. Il ciclo è documentato e **stava già
  ricrescendo**: è la ragione per cui lo scheletro non è cosmesi.
- **STATO ONESTO: il componente è compilato ma NON ancora adottato da nessun modulo** — quindi finché non lo
  è, è **debito, non valore**. Adozione al primo modulo che si tocca (candidato: `VehicleEditor`, che è già
  su `ModuleShell` e non ha *Elimina*), con il suo smoke test. Non l'ho forzata ora perché la riscrittura
  della lista di un modulo GUI non è verificabile con `--sim` e il pilota `ModuleShell` attende ancora la
  prova dell'utente: due cambi non verificati impilati sarebbero difficili da bisezionare.
- Build-verified (Release, compile-check dedicato poi rimosso).

## 2026-08-02 (107) — `ModuleShell`: scheletro di layout condiviso fra i moduli editor (ADR-049)

Proposta dell'utente: *"creare uno scheletro comune dei moduli, così le funzioni condivise si costruiscono
bene una volta; migliorarle vorrebbe dire migliorarle per tutti"*. Accolta — con una precisazione emersa
dall'audit: **metà del lavoro era già fatto**. `FreeCameraViewport` (viewport 3D + gizmo sposta/ruota/scala)
è già condiviso da 4 moduli, come `DefinitionRename`, `saveJsonRMW` e `UiWidgets`. Le UTILITY erano
fattorizzate; mancava lo **scheletro** che le compone.
- **`editor::ModuleShell`** (header-only, `editor/include/framework/`): layout *lista | contenuto |
  proprietà* con **splitter esplicito**, clamp [180 px, metà finestra] e helper per il margine di scroll —
  cioè le regole R5/R6 di doc 39 rese **strutturali** invece che disciplina da ricordare.
- **COMPOSIZIONE, non ereditarietà** (ADR-049): non è una classe base da estendere ma un membro che il modulo
  usa. I moduli sono strutturalmente diversi (viewport 3D con dieci tipi selezionabili / tabelle di numeri /
  form): una gerarchia con virtual per ogni fase diventerebbe un framework da combattere, e imporrebbe di
  riscrivere tutti e sette insieme. Decisivo perché **l'editor è GUI e non è verificabile con `--sim`**: la
  migrazione dev'essere incrementale, un modulo alla volta col suo smoke test.
- **Pilota: `VehicleEditor`** migrato. E la migrazione ha **trovato lo stesso bug del pannello** già corretto
  nel Map Editor (`ChildFlags_ResizeX` su pannello ancorato a destra → una volta stretto non si riallarga):
  era replicato lì e nessuno l'aveva notato. Conferma concreta della tesi dell'utente — *una funzione scritta
  due volte è un bug scritto due volte*.
- Build-verified (Release). **Smoke test dovuto**: aprire il Vehicle Editor, verificare lista/viewport/
  proprietà e che il pannello destro si stringa **e si riallarghi**.
- **Prossimo componente**: `AssetBrowser` (Crea/Duplica/Rinomina/Elimina in un posto solo) — il buco più
  grande dell'audit: *Elimina* manca in **5 moduli su 7**.

## 2026-08-02 (106) — Editor: creazione prefab + tre fix UI (feedback utente)

Quattro segnalazioni dal primo uso del sistema prefab. La più importante era che **mancava del tutto il modo
di CREARE un prefab**: si potevano solo piazzare asset scritti a mano nel JSON — sistema monco.
- **"Crea prefab da zona"**: box e posizioni tattiche entro un raggio dal punto inquadrato diventano un asset
  in coordinate locali (`data/prefabs/<id>.json`, id = filename stem ADR-001), subito piazzabile. Flusso
  standard *costruisci in mappa → promuovi ad asset* (come Unity/Unreal). Di default **sostituisce gli
  originali con un'istanza**: senza, resterebbero due copie — una "cotta" nella mappa e una dal prefab —
  destinate a divergere. Alternativa scartata: un modulo-editor separato per i prefab, che avrebbe duplicato
  metà Map Editor per costruire le stesse cose.
- **Gizmo sui prefab**: mancava il ramo nel dispatch → selezionandone uno sparivano sposta/ruota/scala. Ora
  sposta + ruota (yaw). **Scala esclusa deliberatamente**: scalare un'istanza deformerebbe le posizioni
  tattiche che porta con sé, che sono dati tattici, non geometria.
- **Pannello destro che non si riallargava**: usava `ImGuiChildFlags_ResizeX`, il cui grip sta sul bordo
  DESTRO del child — che qui coincide col bordo della finestra: una volta stretto non c'era più nulla da
  afferrare. Sostituito con uno **splitter esplicito** a sinistra + clamp [180 px, metà finestra], così non
  può incastrarsi in uno stato irreversibile.
- **Scroll della home tagliato**: le card usano coordinate ASSOLUTE (`SetCursorPos`), quindi il contenuto
  finiva esattamente sul loro bordo inferiore. Aggiunto un margine sotto l'ultima riga.
- Build-verified (Release). **Smoke test dovuto** su tutti e quattro.
- **Causa comune, da affrontare a parte**: sono sintomi della stessa deriva segnalata dall'utente — *"le
  stesse funzioni stanno in un modulo e non in un altro, alcune cose che dovrebbero essere uguali cambiano"*.
  Serve un audit di coerenza dell'editor con **regole vincolanti** (doc 39), come già esistono per i dati
  (dropdown dal registry, RMW, id = filename). Senza regole scritte la deriva riparte a ogni modulo nuovo.

## 2026-08-02 (105) — Editor: piazzamento dei PREFAB (doc 41 B5) — il binario dati è chiuso

Ultimo tassello del sistema prefab: finora vivevano solo nei dati (si potevano usare scrivendo il JSON a
mano). Ora si piazzano dall'editor.
- **Lista + combo + "+ Piazza"** (nasce sul focus del viewport) + rimozione; selezione `-4000-i`.
- **Anteprima nel viewport** con la **STESSA trasformazione** dell'espansione del motore (rotazione attorno a
  Y + traslazione) → ciò che si vede in editor è ciò che esisterà in partita. Tinta viola: è contenuto
  DERIVATO, non box della mappa, e non si edita lì.
- **Gizmo** per spostare l'istanza (il contenuto la segue) + slider di rotazione nel pannello, che mostra
  anche quanti box/posizioni porta il prefab.
- **Riferimento rotto VISIBILE**: marker rosso nel viewport e `!` in lista, invece di sparire in silenzio
  (stesso principio del messaggio esplicito nel loader, ADR-018).
- **Save: solo i RIFERIMENTI** (`prefabs: [{id,x,y,z,ry}]`). Box e posizioni derivati non finiscono mai nel
  JSON della mappa: congelarli creerebbe una copia che diverge appena il prefab cambia (ADR-033/048).
- **`DefinitionRegistry::loadPrefabs` resa pubblica**: l'editor carica i prefab col loader del RUNTIME invece
  di un parser proprio — un secondo parser divergerebbe al primo campo aggiunto (stessa ragione per cui i
  parser di box e posizioni sono condivisi, changelog 101).
- **Attenzione al dispatch**: il ramo "settore" usa `m_selBox <= -2000` come catch-all e avrebbe catturato i
  codici dei prefab; il pannello prefab va **prima**. (I codici di selezione dell'editor sono un range unico
  e crescente: è una fragilità nota da tenere a mente quando se ne aggiungono.)
- Build-verified (Release). **Smoke test manuale dovuto**: piazzare un prefab, spostarlo/ruotarlo, salvare,
  riaprire; verificare che in partita compaiano collisione e posizioni tattiche attese.

## 2026-08-02 (104) — Salute tattica: rumore ridotto e avvisi raggruppati (feedback playtest)

Playtest utente del (103): *"funziona molto bene, ma alcuni avvisi non sono necessari — mi segna ridondanti
due vantage sullo stesso punto ma in versi OPPOSTI; e sarebbe comodo raggruppare gli avvisi per tipo, con una
tendina per chiudere quelli che non interessano."* Tutte e tre le osservazioni erano corrette.
- **BUG della regola di ridondanza (falso positivo vero)**: confrontava solo la distanza, **ignorando il
  `facing`**. Due posizioni sovrapposte con versi opposti — es. due `vantage` schiena a schiena su una
  torretta — coprono archi opposti e sono due opzioni tattiche legittime. Ora sono ridondanti solo se
  vicine **e** con fronti entro 45°. **Impatto molto maggiore del previsto**: Training Ground **53 → 19
  avvisi (−64%)**, firebase 15 → 5. I falsi positivi erano la maggioranza degli avvisi.
- **Raggruppamento per TIPO** con tendine richiudibili (`Kind` sul difetto, condiviso col gate): i gruppi con
  PROBLEMI si aprono da soli, quelli di soli avvisi restano chiusi, le categorie vuote non compaiono. Così le
  famiglie intenzionali per una data mappa (i settori di solo transito) si chiudono una volta e non
  disturbano più — **senza disattivare la regola**, che resta viva se un giorno un settore importante resta
  scoperto. Compromesso fra "niente rumore" e "niente segnali persi".
- **Raffinata anche `NoCoverage`**, grazie a un difetto emerso sul prefab: *"non copre altre posizioni"* NON
  basta a dire che una posizione è inutile — una di **prima linea** copre il terreno e l'avvicinamento, non
  altri nodi. Il difetto vero è l'**ISOLAMENTO**: non copre nessuno **e** nessuno la batte (esposizione 0) →
  fuori dalla rete tattica. Se è esposta, è nel gioco: avviso, non problema.
- **Prefab `sandbag_nest` corretto (errore mio)**: il muretto frontale è a `z=-1.6` ma le posizioni avevano
  `facing_deg: 0`, che punta a **+Z** → **davano le spalle al riparo**. Corretto a 180°. Nota: il gate ha
  trovato un errore di authoring commesso da me su un asset che credevo scritto bene — ed è esattamente il
  caso d'uso di ADR-048, perché un difetto sull'ASSET si moltiplica su ogni istanza.
- Le mappe dell'utente non sono state toccate. Build-verified (Release, GFEngine + GFEditor).

## 2026-07-27 (103) — SALUTE TATTICA: i difetti della mappa si leggono, non si cercano (doc 41 B4)

I controlli tattici esistevano (esposizione ADR-033, visuale verticale changelog 97, grafo ADR-032) ma erano
**sparsi**: per scoprire che una posizione era cieca o inutile bisognava selezionarla. Con 167 posizioni —
e a maggior ragione con mappe profonde — è impraticabile. Ora un elenco unico dice **cosa non va**.
- **Regole in UN SOLO POSTO**: `mini::analyzeTacticalHealth(const MapDef&)` in `ContentValidation`, che il
  CMake compila in **entrambi** i binari ("STESSO gate del runtime, mai una copia", ADR-018). Le avevo
  inizialmente scritte dentro l'editor: sarebbe stata l'ennesima **doppia verità** di questo progetto (la
  stessa classe di errore del revert changelog 77 e della divergenza torre↔comandante del changelog 88).
  Unica regola rimasta nell'editor, deliberatamente: la visuale verticale, i cui dati vivono solo lì.
- **Cinque difetti**: posizione che non copre nessuno (grafo) · cieca verso le altre quote (KI #83) · molto
  esposta ≥55% · ridondante (<2 m, stesso ruolo) · settore senza posizioni tattiche.
- **Due consumatori**: pannello editor in cima alla lista, **chiuso di default** (mostra "Salute tattica: OK"
  quando non c'è nulla) con voci **cliccabili** che selezionano l'elemento colpevole; e **`--validate`**
  headless → utilizzabile in CI, senza aprire l'editor, e verificabile da me.
- **Misurato e CALIBRATO sul dato**: la prima passata dava Training Ground 13 problemi/45 avvisi, ma **9 dei
  13 erano le corsie di transito `SR`/`SS`** (importanza 0.5), dove è normale non avere posizioni. Severità
  resa **proporzionale all'importanza autorata** (≥1.0 = problema, sotto = avviso "transito?") →
  **5 problemi / 53 avvisi**. I 5 residui sono tutti azionabili: 4 `vantage` che non coprono nulla (coerenti
  con KI #83) e il settore **Separatist Spawn con importanza 3.0 e nessuna posizione** — difetto vero: i
  droidi nascono lì senza coperture. firebase 2/15, Prefab Test 2/0.
  *Il punto non è il numero ma il rapporto segnale/rumore: un elenco con 9 falsi positivi su 13 si smette
  di leggere, e uno strumento che non si legge non esiste.*
- **Ritrovamento**: `Prefab Test` ha 2 problemi con sole 6 posizioni, tutte generate dal prefab → il prefab
  `sandbag_nest` ha un difetto tattico proprio. È la dimostrazione del modello ADR-048 in entrambe le
  direzioni: il significato si moltiplica per istanza, **e anche il difetto** — quindi va colto sull'ASSET.
- Build-verified (Release, GFEngine + GFEditor). **Smoke test manuale dovuto**: aprire l'editor e verificare
  che il pannello compaia e che cliccare una voce selezioni l'elemento.

## 2026-07-27 (102) — Copertura dall'ALTO come dato derivato (doc 41 B3) + correzione della stima di scala

Terzo passo del binario B. Il piano lo chiamava "indoor/outdoor"; implementandolo ho verificato che **quel
raycast non misura l'interno**: misura se c'è **qualcosa sopra**. Un sottopasso, un balcone e una tettoia
danno lo stesso risultato di una stanza chiusa.
- **Nome deliberato `hasOverheadCover` / `hasOverhead`, NON `indoor`.** Chiamarlo `indoor` avrebbe creato un
  falso amico: progettando i comportamenti da interno (distanze corte, granate, sgombero), il campo avrebbe
  detto "interno" sotto un ponte all'aperto e l'AI avrebbe applicato tattiche da edificio in campo aperto —
  un bug difficile da diagnosticare, nato da un nome. L'**interno vero richiede il rilevamento della
  CHIUSURA** (pareti attorno), analisi diversa che resta a B8. Annotato nel codice e nell'header.
- **Query pura** `worldintel::hasOverheadCover(map, x,y,z, probe)`: sonda verticale dalla testa (1.8 m) fino a
  `OVERHEAD_PROBE_HEIGHT` (8 m). Partire dai piedi avrebbe intersecato il pavimento e dato "coperto" ovunque.
- **Dato DERIVATO** calcolato in `buildTacticalLinks` insieme agli altri: vive e muore con la geometria, mai
  autorato, mai salvato (ADR-033). Costo: un raycast per posizione, trascurabile.
- **Con CONSUMATORI dal primo giorno**, applicando la lezione di KI #25b (un campo che nessuno legge è debito,
  non valore): pannello posizione dell'editor ("Coperta dall'alto: sì/no", stessa funzione del runtime) e
  conteggio per mappa nel log del registry.
- **Verificato**: Training Ground **25/167** coperte (15% — bassa ma non nulla, coerente con una mappa aperta
  con ponti); firebase 0/60 (tutta all'aperto); Prefab Test 0/6 (corretto). `--validate` 0 errori.
- **Correzione di una stima**: il log ha dato il costo reale del bake — **167 posizioni → 7.9 ms** (60 → 0.43
  ms). Il doc 41 stimava ~20 ms a 500 posizioni; la realtà è **~70 ms, ~3× peggio**. Soglia d'intervento per
  la griglia spaziale **abbassata da ~400 a ~300 posizioni** (doc 41 §9 e 06_Todo aggiornati).

## 2026-07-27 (101) — PREFAB tattici: il significato si autora per ASSET, non per istanza (ADR-047/048, doc 41 B1+B2)

Primo passo del binario B (mondo tattico). Problema: Training Ground ha **167 posizioni piazzate a mano**;
mappe profonde ne vorrebbero 1000+ → l'authoring per istanza non scala. La generazione automatica dalla
geometria è già stata provata e rimossa (ADR-026). Terza via: **autorare una volta per asset**.
- **`PrefabDef`** (`data/prefabs/<id>.json`, id = filename stem ADR-001): mesh visiva (solo visiva, ADR-047),
  **proxy di collisione** in box (la verità fisica E tattica), **posizioni tattiche** in coordinate LOCALI, tag.
- **`PrefabInstanceDef`** nella mappa (`"prefabs": [{id, x, y, z, ry}]`): la mappa **referenzia**, non duplica.
- **Espansione al load**, con rotazione corretta di box e `facing_deg`, eseguita **PRIMA di
  `buildTacticalLinks`** → le posizioni dei prefab partecipano a coperture/esposizione come tutte le altre.
  I dati espansi sono **DERIVATI**: non tornano mai sul file della mappa, quindi aggiornare un prefab aggiorna
  tutte le sue istanze al load successivo (stesso principio di ADR-033: non possono diventare stale).
- **`TacticalPositionDef.fromPrefab`** (B2): distingue derivato da autorato. È ciò che permetterà di
  rigenerare i prefab **senza cancellare** il lavoro manuale.
- **Parser CONDIVISI** `parseGeometryBox`/`parseTacticalPosition` estratti dalle lambda di `loadMaps`: mappa e
  prefab leggono lo **stesso schema**. Con due parser separati sarebbero divergiti al primo campo aggiunto —
  è la stessa "doppia verità" che causò il revert del changelog 77.
- **Riferimento rotto = messaggio esplicito** (non silenzio), coerente col gate ADR-018.
- **Verificato** (`--validate`, Release): prefab `sandbag_nest` (3 box, 3 posizioni) × 2 istanze ruotate 0°/90°
  → **6 box + 6 posizioni**; mappa di prova 7 box totali; **0 errori**. **Training Ground invariata**
  (167 box, 167 posizioni) → nessuna regressione sulle mappe esistenti.
- **Limite dichiarato**: l'editor **non conosce ancora i prefab** (UI = B5). Aprendo una mappa con prefab
  l'editor non li mostra, ma **non li perde**: `saveJsonRMW` (ADR-010) preserva le chiavi che non possiede —
  verificato leggendo l'implementazione.
- Nessun binario Debug eseguibile in questo ambiente (ASan senza `clang_rt.asan_dynamic-x86_64.dll` sul PATH,
  già noto in 06_Todo): le verifiche girano in **Release**.

## 2026-07-27 (100) — PERCEZIONE Fase 1: campo visivo + udito (doc 40) — le AI smettono di essere onniscienti e sorde

Prima fase del piano AI (doc 40). **`fov_deg` e `hearing_range` erano autorati in ogni profilo ma non
raggiungevano mai l'AI** — difetto **già noto e tracciato in KI #25b dal 2026-07-10**, non una scoperta:
era lì da due settimane e nessuno l'aveva chiuso, mentre l'AI restava onnisciente e sorda. Dettaglio — `hearingRange` compariva SOLO nel parser, `fovDeg` nel parser e
nella Camera di rendering. `AiComponent` non aveva nemmeno i campi. Conseguenza in gioco: i soldati
**vedevano a 360°** (arrivare alle spalle non valeva nulla) ed erano **sordi** (uno sparo non allertava
nessuno, le battaglie non si propagavano).
- **Percorso dei dati riparato**: `fovDeg`/`hearingRange` aggiunti ad `AiComponent` e copiati dal profilo
  allo spawn (`ConquestMode`). Nessun nuovo authoring: i valori esistevano già.
- **Campo visivo**, NON come taglio netto (darebbe il difetto "mi sta di fianco e non mi vede"): fuori dal
  cono restano percepibili chi è **ravvicinato** (`PERCEPTION_PERIPHERAL_RADIUS` 6 m) e chi **sta sparando**
  (`PERCEPTION_MUZZLE_REVEAL` 35 m) — il lampo/fragore rivela. Misurato: 2000-3100 bersagli/report scartati
  perché fuori campo → il fianco e le spalle ORA contano.
- **Udito event-driven**: nuova mailbox `World::sounds` (pattern doc 10, costo zero quando nessuno spara).
  Emettono AI (`AiSystem`) e **giocatore** (`PlayerController`); consuma `AiSystem` generando un contatto
  **impreciso** (posizione disturbata da `SOUND_CONTACT_SCATTER`: si sa da DOVE, non CHI) che alimenta la
  ricerca già esistente. Riusa `SharedContact` (età, TTL, dedup) — nessun sistema parallelo.
- **Correzione della semantica di `hearing_range`, guidata dalla misura**: preso come raggio assoluto, il
  valore autorato 12 m rendeva l'udito **inerte** (si sente a 12 m ciò che si vede a 50) → `spari_uditi` 0-16
  e battaglie che si spegnevano. Ora è una **sensibilità relativa** (`HEARING_REFERENCE`): raggio udibile =
  forza dell'evento × (hearing_range / 12). I dati già autorati acquistano senso senza riscriverli.
  Misurato dopo: `spari_uditi` **72-138** (×10), `contatti_vivi` fino a 154, `in_alert` fino a 11, `fermi` 0.
- **Telemetria**: `fuori_campo_visivo` e `spari_uditi` come guardie permanenti (costo: due contatori).
- **Effetto sul volume di fuoco**: eventi di combattimento per 3000 tick 155 → **116** (−25%). Atteso e
  ritenuto corretto: i 155 erano il valore con AI **onniscienti**; togliendo la vista a 360° compaiono
  ricerca, avvicinamento e sorpresa. **Ma è un cambio di FEEL e va giudicato giocando**: se le sparatorie
  risultassero troppo rade, le leve sono `PERCEPTION_MUZZLE_REVEAL`, `SOUND_GUNSHOT_RADIUS` e `hearing_range`
  dei profili.
- Build-verified (Debug + Release); misure con `--sim-ticks 3000` (deterministico, changelog 99).
  **Smoke test manuale da fare**: aggirare un nemico e verificare che non ti veda finché non entri nel cono;
  sparare e vedere i nemici lontani orientarsi/accorrere.

## 2026-07-27 (99) — `--sim-ticks N`: la simulazione diventa MISURABILE (verifiche deterministiche)

Trovato misurando: il conteggio degli eventi di `--sim` **non era confrontabile**. La stessa identica build
produce 211/215/224/224, perché `--sim` gira finché un timeout esterno lo interrompe → il numero di tick
simulati dipende dal carico della macchina. Con ±10% di rumore, una piccola regressione è invisibile e una
fluttuazione sembra una regressione: tutte le verifiche quantitative di questa sessione poggiavano su una base
più debole di quanto dichiarato (corretto anche nel (95)).
- **`--sim-ticks N`**: la sim esce dopo N **tick di simulazione** invece che a tempo (conta
  `World::getTickCount()`, che avanza solo quando il mondo è simulato — i menu non lo muovono). Due run fanno
  esattamente la stessa quantità di mondo.
- **Verificato**: 3 run con `--sim-ticks 3000` → **155, 155, 155**. Determinismo perfetto: il rumore era
  interamente dovuto alla durata variabile, non al gioco. Ora un confronto prima/dopo un refactor ha valore
  probatorio; prima no.
- Uso: `GFEngine.exe --sim-ticks 3000 --map "Training Ground"` (Release; le virgolette sulla mappa con spazi
  sono obbligatorie, [[powershell-quote-args-with-spaces]]). `--sim` resta per l'osservazione a occhio.
- Build-verified (Debug + Release). File: `main.cpp` (flag), `Application.hpp/.cpp` (parametro + uscita a fine
  frame). Nessun effetto sul gioco normale (`simTicks = 0` = comportamento invariato).

## 2026-07-27 (98) — Ordini a MEMBRI SPECIFICI: selezione dei compagni (ordini diversi a gruppi diversi)

Ripresa del rework ordini sul filone "ordini rapidi precisi" ([[orders-design-vision]]): finora un ordine
andava a TUTTA la squadra, tranne Revive/CoveringFire che puntavano un compagno. Ora il giocatore può
**selezionare** i compagni e comandarli separatamente. Scelte di UX confermate dall'utente: selezione
mirando il compagno; ambito invariato (tutti gli alleati restano la squadra — nessun refactor strutturale).
- **`SquadOrderRequest.directedMember` → `directedMembers` (lista)**: era un singolo destinatario (bastava a
  Revive/CoveringFire); ora è una lista, perché è ciò che abilita "ordini diversi a più membri". Filtro in
  `SquadSystem`: lista vuota = tutta la squadra (comportamento storico), altrimenti solo i membri elencati.
- **Gesto**: mirare un compagno VIVO + tasto ordini = **seleziona/deseleziona** (toggle, con toast). Poi lo
  stesso tasto su un nemico → FocusFire, o su un punto → MoveTo/TakeCover, va **solo ai selezionati**. Anche la
  **RUOTA** rispetta la selezione → si tengono due gruppi su posture diverse (es. #1-#2 in HOLD, #3 in ADVANCE).
  Selezione vuota = tutto come prima.
- **Conseguenza dichiarata**: quel gesto prima dava **CoveringFire**, che non è più raggiungibile dal tasto
  rapido. Si ottiene selezionando il compagno e dandogli **HOLD** dalla ruota (ancora la posizione e ci
  combatte). L'ordine resta implementato; se servirà un accesso diretto, va riesposto senza affollare la ruota
  ([[ui-no-clipping-use-dropdowns]]).
- **Robustezza**: la selezione si auto-pulisce ogni frame da entità invalide/non più in squadra/a terra — i
  caduti respawnano come entità NUOVE, quindi una selezione stale avrebbe mandato ordini a fantasmi **in
  silenzio**, col sintomo "gli ordini non funzionano". HUD: tag `[SEL n]` accanto allo stato squadra, così è
  sempre chiaro se si sta comandando la squadra o un gruppo scelto.
- Build-verified (Debug + Release). `--sim`: nessun crash e conteggio nella normale dispersione della misura
  (211-224 sulla stessa build — il conteggio `[Combat]` NON è deterministico, vedi correzione nel (95)); la sim
  non impartisce ordini del player, quindi serve solo a escludere che il cambio di mailbox abbia rotto la
  squadra. **Smoke test manuale da fare**: selezionare 1-2 compagni, dare ordini diversi, verificare `[SEL n]`
  e che i non selezionati restino liberi.

## 2026-07-27 (97) — Editor: strumento "VISUALE VERTICALE" — l'authoring verticale non è più a tentativi (KI #83)

Conseguenza diretta del (96): il combattimento cross-quota è limitato dalla GEOMETRIA, non dall'AI — ma finora
l'unico modo di sapere se una posizione elevata "vedeva" davvero era provarla in partita. Ora si vede in editor.
- **Dato derivato `m_vertSight` / `m_vertPairs`** (paralleli a `m_positions`, mai salvati): per ogni posizione,
  quante posizioni a QUOTA DIVERSA vede, su quante ne esistono. Calcolato in `recomputeExposure()` — che già
  costruiva il MapDef temporaneo — con la **stessa `worldintel::hasLineOfFire` del runtime** e lo stesso modello
  di combattimento (origine OCCHI `COMBAT_EYE_HEIGHT`, bersaglio CORPO `AI_HALF_Y`, [[combat-los-eye-height]]):
  nessuna doppia verità fra editor e gioco. Il filtro sul dislivello (`VERTICAL_ENGAGE_DY`) viene PRIMA della
  LOS → si pagano solo le coppie cross-quota.
- **Il denominatore conta**: "vede 0 su 0" (nessuna altra quota in giro) è irrilevante, "vede 0 su 24" è il
  difetto. Senza `m_vertPairs` i due casi si confondevano.
- **UI a tre livelli**, per trovare i problemi senza cercarli: riepilogo `Verticale: N/M cieche` sopra la lista
  (stato di salute della mappa a colpo d'occhio) → prefisso `!` sulle voci di lista difettose → nel pannello
  della posizione `Visuale verticale: X / Y` con colore e il rimedio suggerito (avvicinare al bordo, abbassare
  il parapetto). Nel **viewport**: rombo rosso sospeso SOLO sulle posizioni cieche — il colore del corpo resta
  quello del RUOLO, che non va perso.
- Build-verified (Debug + Release) e **smoke test manuale CONFERMATO dall'utente (2026-07-27)**: il conteggio
  "valide su totale" compare e **cambia spostando le posizioni in alto/in basso** → il dato reagisce davvero
  alla geometria, l'authoring verticale non è più a tentativi. Ciclo di lavoro: sposti/abbassi → il numero
  cambia subito in editor → la telemetria a funnel conferma poi in partita.

## 2026-07-27 (96) — Verticalità: telemetria a funnel + verdetto (l'AI è corretta, è la mappa) — KI #83

L'utente chiede se esista un modo per verificare ESATTAMENTE il combattimento cross-quota (i proiettili lenti
riempivano lo schermo di tracce non attribuibili, e tre bug diversi danno lo stesso sintomo visivo).
- **Funnel di verticalità PERMANENTE** nell'evento `tactical decisions` (costo ~zero, solo confronti):
  `vert_candidati`/`tot_candidati` → `vert_acquisiti`/`tot_acquisizioni` → `vert_colpi`, più
  `vert_tiro_bloccato`, `piano_colpi` (controllo) e `acq_tutti_bloccati`. Nuova costante
  `VERTICAL_ENGAGE_DY` (1.5 m). Guardia riusabile dopo ogni modifica a mappa o codice.
- **Verdetto misurato**: opportunità cross-quota 19-22%, acquisizioni 0-6%; ma **visibili = acquisiti 1:1** e
  **`vert_tiro_bloccato` = 0 sempre** → l'AI ingaggia tutto ciò che vede e spara quando ingaggia: **nessun bug
  di verticalità**. Il limite è la VISIBILITÀ (1-3% dei cross-quota visibile), causata dalla GEOMETRIA: le
  piattaforme sono blocchi pieni 7.8×8.3×3.1 → vincolo di ~12 m orizzontali per vedere a terra, più le pareti
  alte 3 m diffuse. LOS riletta: corretta. Dettaglio completo e leve di authoring in KI #83.
- **Onestà metodologica**: due ipotesi mie sono state SMENTITE dalla misura prima di diventare fix — la
  saturazione dei K candidati (con 8 nemici i candidati sono già tutti) e "a raggio piatto deve vedersi" (è il
  contrario: un raggio piatto fatica di più a superare il proprio bordo). Registrate in KI #83.
- Le misure diagnostiche costose (LOS extra per candidato, bucket per distanza) sono state **rimosse** dopo aver
  stabilito la causa; resta solo il funnel a costo zero. Build-verified (Debug + Release), `--sim` senza
  regressioni.

## 2026-07-27 (95) — Split del monolite AiSystem: nasce AiCommandLayer (audit #7)

Il debito più grosso emerso dall'audit (94): `AiSystem.cpp` = **2578 righe** con sensing, combattimento,
manovra, comando, torre, ordini e movimento in un solo file, e una `update()` da **1740 righe**. Era il punto
più a rischio per qualunque modifica futura. Separato in due unità di traduzione lungo una cucitura tematica.
- **`AiCommandLayer.cpp` (843 righe)** — il "COSA si decide a livello di teatro": stato dei settori (ADR-034) e
  peso tattico condiviso (88), **torre di controllo** dei cloni (doc 36) e **quadro tattico** torre-hub (93),
  **direttive del Droide Tattico** (doc 32 v2) inclusa la loro ricostruzione periodica, selezione della
  posizione per gli **ordini di postura** del player.
- **`AiSystem.cpp` (1805 righe, −773)** — il "COME la singola unità esegue": sensing, ingaggio, manovra
  (ADR-035), pattuglia, guinzagli, movimento, ciclo per-entità.
- **`AiInternal.hpp`** — seam PRIVATO fra le due (in `src/ecs/systems/`, non esposto in `include/`): dichiara i
  12 helper del command layer in `namespace mini::aicmd`. `AiSystem.cpp` fa `using namespace aicmd` → **i
  chiamanti non cambiano di una riga**. Il blocco di decisione del comandante è diventato il metodo privato
  `AiSystem::updateEnemyCommand(world, snap, dt)` (ritorna la deriva del comandante, che serve alla telemetria).
- **Comportamento INVARIATO per costruzione**: spostamento **verbatim** (estrazione via `sed`, nessuna
  riscrittura a mano), zero modifiche di logica. Taglio scelto su evidenza: le funzioni spostate **non** usano
  la telemetria `g_tac` né le utility di movimento (`enterHunt`, `aiRand01`, `norm2D`…), quindi il confine è
  netto e senza stato condiviso.
- **Verificato**: baseline `--sim` PRIMA del refactor (206, 206) e DOPO ogni passo → 206. Build pulita Debug +
  Release (GFEngine + GFEditor). CMakeLists aggiornato; `AiSystem.hpp` ora include `Entity.hpp` (serviva a
  `EntityId` nella firma del nuovo metodo).
  **CORREZIONE (2026-07-27, misurata dopo)**: il conteggio `[Combat]` **NON è deterministico** — la stessa
  identica build produce 211/215/224/224, perché `--sim` gira a tempo (timeout) e il numero di tick simulati
  dipende dal carico della macchina. I "206, 206" erano condizioni di carico simili, non determinismo: quindi
  quel confronto **non dimostrava** l'invarianza come affermato qui. L'invarianza dello split resta fondata su
  ciò che la garantisce davvero — spostamento **verbatim** (estrazione `sed`, zero modifiche di logica) e
  confine senza stato condiviso (verificato: niente `g_tac`, niente utility di movimento). Per confronti
  quantitativi futuri servono metriche indipendenti dalla durata (rapporti, `fermi`, assenza di crash) o una
  run a TICK FISSI, che oggi non esiste.
- **NON toccato (deliberato)**: il ciclo per-entità (~1200 righe) resta in `update()`. Spezzarlo richiede di
  sciogliere decine di variabili locali condivise fra le fasi (bersagli SoA, `teamAlive`, `repositioning`,
  `moveDX/DZ`…): è un refactor a sé, con un rapporto rischio/beneficio diverso. Registrato in 06_Todo.

## 2026-07-27 (94) — Audit post-torre-hub: fix occupancy + coerenza ruoli + costanti autorabili

Esame profondo richiesto dall'utente dopo il blocco ordini/torre-hub. Findings prioritizzati (report completo
in conversazione); sistemati i chiari a basso rischio, in ordine:
- **#1+#2 Occupancy dei cloni AUTONOMI (correttezza)**: durante il commitment il clone autonomo riusava la sua
  posizione senza ri-rivendicarla in `allyTac.claimed` → un compagno poteva prenderla → ammasso residuo (il
  tower-hub risolveva l'occupancy solo per gli ordini). Ora il ramo autonomo salva l'indice in `ai->allySigIdx`
  e lo RI-RIVENDICA a ogni tick di commitment. Occupancy ora completa per ordini E autonomi.
- **#3 Coerenza ruoli (integrazione/fragilità)**: il filtro "ruolo utile" era duplicato in 3 punti con set
  DIVERSI (`bestAdvantageInArea` dimenticava `observation`). Unificato in `worldintel::isTacticalHoldRole`
  (cover/vantage/defensive/chokepoint/observation). Ora droidi e tower-hub vedono le stesse posizioni;
  `bestAdvantageInArea` include finalmente le `observation`. `bestHoldPosition` resta a parte di proposito
  (solo defensive/chokepoint = presidio difensivo droidi).
- **#5 Costanti di feel → GameConfig (autorabilità)**: portate a leve regolabili le costanti prima inline:
  `ORDER_COMMIT_TIME`/`ORDER_COMMIT_FALLBACK`/`REGROUP_COMMIT_TIME`/`ALLYSIG_COMMIT_TIME` (impegno su una
  posizione, il "commitment threshold" che l'utente voleva tarare), `ORDER_ENEMY_SCAN`, `TAC_PICTURE_PERIOD`,
  `TAC_FIRE_BONUS`, `POSITION_DEFAULT_FIRE_RANGE`.
- **#10** commento stale (`advanceWaypoint` → `selectOrderWaypoint`) corretto.
- **Build-verified** (Debug + Release), sanity `--sim` senza regressione/stuck.
- **Rimandati (per decisione)**: #4 (la retry di reachability marca claimed le irraggiungibili per tutto il
  tick — impatto basso), #6 (quadro torre stale ≤0.33 s), **#7 (AiSystem.cpp = 2578 righe, MONOLITE**: candidato
  a split `AiCombat`/`AiOrders`/`AiCommandLayer` — refactor grande, da valutare a parte), #11 (Regroup no-op se
  nessun settore conteso). #8/#9 già noti (band-aid muzzle attende pose; tower-hub solo lato cloni).

## 2026-07-27 (93) — Rifinitura ordini: fix combattimento + TORRE-HUB (la torre pre-calcola i dati tattici)

Playtest del (92): Retreat tendeva ad andare AVANTI, alcuni cloni restavano fermi indietro, alcuni sparavano
meno. Due fix + una scelta architetturale (concordata con l'utente).
- **Fix Retreat in combattimento**: il movimento in Alert è guidato dalla logica d'ingaggio (che CHIUDE verso il
  nemico), non dal waypoint dell'ordine → l'ordine "indietreggia" era ignorato. Ora `OrderType::Retreat` attiva
  lo stesso disimpegno del flag `retreating` (arretra dal bersaglio con fuoco di copertura) anche a piena salute.
- **Fix "fermi indietro"**: nel selettore, se non trovava una posizione libera "più avanti" il fallback era la
  posizione DEL MEMBRO stesso (`areaX=px` per Advance) → restava fermo. Ora il fallback è una destinazione
  sensata per postura (Advance→verso il nemico, Follow→leader, Retreat→retro, Hold→centro area).
- **TORRE-HUB (Fase concordata, "dati + occupancy centralizzata")** — la torre fa il lavoro pesante UNA volta,
  i cloni ragionano meno ([[world-tactical-intelligence]]):
  - **`updateAllyTactical`** (torre, cadenza 0.33 s): per ogni posizione tattica utile calcola se BATTE un
    nemico ORA (LOS verificata) e un `score` (importanza+protezione−pericolo, **+bonus se batte**). Dati in
    `World::allyTac` (paralleli a `tacticalPositions`). Attivo solo con torre di controllo team-1 VIVA →
    altrimenti i cloni ricadono sul punteggio locale ([[structures-degrade-not-block]]).
  - **`bestOrderPosition`** (AiSystem): sceglie la posizione LIBERA a score massimo (tower-aware), con
    **occupancy CENTRALE** (`allyTac.claimed`, azzerata ogni tick, condivisa da ordini e cloni autonomi) e
    filtro di direzione. Rimpiazza la LOS per-clone sia negli ordini (`selectOrderWaypoint`) sia nel ramo
    autonomo dei cloni (torre). Fix "sparano meno": ora scelgono posizioni da cui SPARANO davvero, non cieche.
  - Rimossa `worldintel::bestFreePosition` (la selezione ha bisogno di World/nav/allyTac → sta in AiSystem;
    worldintel resta puro su MapDef). Occupancy spostata da `m_claimedPositions` (AiSystem) a `World::allyTac`.
- **Misurato** (`--sim`, DIAG poi rimosso): la torre calcola `canFire` 43-122 su 167 posizioni (dinamico coi
  nemici che si muovono); combattimento attivo (206), 0 crash, 0 stuck → il ramo AUTONOMO dei cloni (sim-testabile)
  usa il tower-hub senza regressione. **Build-verified** (Debug + Release). Gli ORDINI del player restano da
  validare **visivamente** (non sim-testabili): Retreat che arretra, nessuno fermo, Hold/Advance su posizioni che
  sparano, distribuzione senza ammasso.

## 2026-07-27 (92) — Ordini del player: MOTORE UNIFICATO occupancy-aware (correzione dopo playtest utente)

Il playtest degli ordini (89-91) ha mostrato che erano APPROSSIMAZIONI, non fedeli alla visione: Retreat andava
addirittura AVANTI (mandava al "settore controllato più vicino", che può essere in prima linea); Hold cercava
solo posizioni di difesa e si ammassava (spread solo via bias, non occupancy vera); Advance/Follow poco fedeli.
Riscritti attorno a **un unico motore** ([[orders-design-vision]]):
- **`worldintel::bestFreePosition`** (nuova primitiva): fra i ruoli utili (cover/vantage/defensive/chokepoint/
  **observation** — prima observation era escluso) dentro l'area, la posizione LIBERA a **priorità massima**
  (importanza+protezione), con **occupancy vera** (salta gli indici in `claimed`) e filtro di **direzione**
  (`dirToThreat` +1 avanti / -1 indietro / 0 qualunque). "Scegli la posizione libera migliore; se occupata,
  un'altra" — esattamente lo spec utente.
- **`selectOrderWaypoint`** (AiSystem): motore per-membro con **commitment** (non ri-sceglie ogni tick) +
  reachability, che imposta il waypoint per postura:
  - **Hold** → miglior posizione libera nell'area, poi la DIFENDE (clamp di presidio).
  - **Advance** → posizioni più VICINE al nemico (a sbalzi verso il fronte designato).
  - **Retreat** → posizioni più LONTANE dal nemico (indietreggia; combatte fronteggiandolo). Corretto il bug
    "andavano avanti".
  - **Follow** → coperture vicino al leader verso il nemico (di cover in cover, non più formazione fissa);
    tiene il leash come sicurezza per non restare indietro.
  - **Regroup** → settore CONTESO a **peso massimo** in quel momento (`sectorTacticalWeight`), non più il punto
    mirato.
- **Occupancy** in AiSystem (`m_claimedPositions`, azzerato ogni tick; indice rivendicato in `ai.allySigIdx`):
  la squadra si DISTRIBUISCE sulle migliori posizioni libere invece di ammassarsi. Il ramo waypoint alleato e il
  leash sono stati unificati (le posture usano il waypoint/clamp; solo MoveTo/TakeCover/CoveringFire/Follow usano
  il leash — Follow come sola sicurezza). Rimosso `advanceWaypoint` (superato).
- **Nessun override**: mira/fuoco/reposition restano autonomi; l'ordine dà solo intento+posizione. File:
  WorldIntel(.hpp/.cpp), GameConfig (ORDER_BOUND_STEP, FOLLOW_COVER_RADIUS), AiComponent (allySigIdx), AiSystem
  (.hpp m_claimedPositions; selectOrderWaypoint + selezione + ramo waypoint + leash), SquadSystem (Regroup
  implementato), Application (Regroup senza ancora).
- **Build-verified** (Debug + Release); sanity `--sim` per NESSUNA regressione autonoma (droidi/torre non toccati).
  **Non testabile in `--sim`** (ordini del player) → **smoke test visivo** necessario, per postura.

## 2026-07-26 (91) — Ordine RETREAT del player + ruota a 6 settori

Terzo ordine del rework (dopo Hold 89, Advance 90). **Retreat** (nuovo nella visione): "indietreggiano alla
zona sicura più vicina". Stessa filosofia bias-non-override.
- **`OrderType::Retreat`**: il membro ripiega alla **zona sicura più vicina** — un settore controllato dalla
  PROPRIA fazione, altrimenti lo spawn. Riusa `retreatPointForTeam` (generalizzato da `retreatPointForTeam2` al
  parametro `team`; il chiamante droidi ora passa `2` → nessun cambio di comportamento lato droidi). Il
  combattimento resta autonomo mentre ripiega (spara cadendo indietro). Precede la torre; escluso dal leash di
  squadra (usa il waypoint della zona sicura, non un target). Persiste come postura.
- **Ruota a 6 settori**: ADVANCE, HOLD, FOLLOW, REGROUP, **RETREAT**, FREE (era 5). RETREAT non richiede ancora
  (la zona sicura si calcola dalle posizioni). Angoli condivisi Application/HUD.
- **Build-verified** (Debug + Release); sanity `--sim` per NESSUNA regressione (droidi/ripiego per-fronte usano
  lo stesso helper generalizzato). **Non testabile in `--sim`** (ordine del player) → **smoke test visivo**:
  dare RETREAT e verificare che la squadra ripieghi alla zona tenuta più vicina continuando a sparare.
- Restano da rifinire: **Regroup** (oggi MoveTo sull'ancora → settore a peso max) e **Follow** (formazione →
  cover-to-cover). Con questi il rework ordini-ruota è completo per questa fase.

## 2026-07-26 (90) — Ordine ADVANCE del player (avanzata tattica) + ruota a 5 settori con FREE dedicato

Secondo ordine del rework (dopo Hold, 89), stessa filosofia bias-non-override. Prima ADVANCE era `MoveTo`
sul punto mirato → "vai lì e poi combatti", one-shot. La visione ([[orders-design-vision]]) lo vuole "avanzano
di posizione tattica in posizione tattica con visuale sul nemico, alla successiva quando non c'è più nulla da
colpire". Ora è un ORDINE VERO che riusa la macchina dei droidi:
- **`OrderType::Advance`** (nuovo) + helper condiviso **`advanceWaypoint`** (AiSystem): occupa una posizione di
  TIRO verso il nemico più vicino all'area designata (LOS verificata da `bestFiringPosition`, anche elevata),
  con COMMITMENT (timer, no oscillazione); sbalza alla successiva quando la raggiunge/scade. Senza nemico noto
  → miglior terreno dell'area. È la stessa logica del ramo Advance dei droidi MENO la cattura-post (roba loro) →
  il ramo droidi resta intatto (nessuna regressione sul lato appena validato). Il player Advance PRECEDE la
  torre nel ramo waypoint; combattimento/mira/reposition restano autonomi.
- **Guinzaglio LARGO** all'area (`ADVANCE_AREA_RADIUS`+6): non trattiene l'avanzata (le firing position stanno
  nel raggio), rientra solo se il combattimento porta oltre l'area → niente caccia all'infinito. Advance
  PERSISTE come postura (SquadSystem: `default` non lo spegne), non è un MoveTo che si chiude all'arrivo.
- **Ruota a 5 settori** (generalizzata a N radiale, prima 4 fisse sulle diagonali): ADVANCE, HOLD, FOLLOW,
  REGROUP, **FREE**. FREE è ora un settore DEDICATO che libera SEMPRE dagli ordini (prima "Liberi" era solo il
  toggle di Follow → non potevi liberarli se erano in Hold/Advance/Regroup — richiesta utente). Stessi angoli
  fra Application (selezione) e HUD (disegno). Follow non ha più senso in osservatore (avvisa).
- Nuova costante `ADVANCE_AREA_RADIUS`. File: SquadComponent (enum+orderName), SquadSystem (isImplemented;
  persiste in `default`), AiSystem (advanceWaypoint + ramo waypoint + leash), Application (ruota), Hud (ruota).
- **Build-verified** (Debug + Release); sanity `--sim` per NESSUNA regressione autonoma. **Ordini del player NON
  testabili in `--sim`** (niente giocatore/ruota) → Advance e la ruota-a-5 richiedono **smoke test visivo**
  (osservatore/partita): la squadra avanza di posizione di tiro in posizione di tiro, e FREE li libera da qualsiasi
  ordine. Prossimi: Retreat (6° settore), Regroup su settore a peso max, Follow cover-to-cover.

## 2026-07-26 (89) — Ordine HOLD del player rifatto BENE: controlla l'area (bias, non override)

Ripresa del rework ordini (dopo il revert changelog 77), ora che l'AI autonoma è solida. Primo ordine, il
FONDAMENTALE: **Hold**. Prima era `HoldPosition` sulla posizione CORRENTE del membro, leash 2 → "congelati
dove siete". La visione ([[orders-design-vision]]) lo vuole "controllano l'AREA distribuendosi sulle migliori
posizioni, non ammassati". Rifatto come **bias leggero sull'AI autonoma**, MAI override (la lezione del 77):
- **L'ordine dà l'area, l'AI sceglie il posto**: `sq->target` è ora il CENTRO dell'area (punto mirato in
  osservatore / posizione del giocatore in partita); ogni membro sceglie da sé la miglior posizione lì dentro
  (`worldintel::bestAdvantageInArea`, QUALSIASI ruolo) e la presidia. Il combattimento resta autonomo.
- **Anti-ammasso via bias**: punto di ricerca RUOTATO per membro (`bias`) → membri diversi puntano zone diverse
  dell'area → si distribuiscono invece di convergere sulla stessa posizione. Stesso principio decorrelato del
  droide/torre.
- **Riuso del pattern presidio droidi** (ADR-046): `ai->holdX/Z` + clamp `holdRadius` (HOLD_ANCHOR_RADIUS 4 m)
  tiene il membro sulla posizione scelta mentre combatte. Hold PRECEDE la torre nel ramo waypoint (l'ordine
  diretto vince sul segnale) ed è escluso dal leash di squadra (lo gestisce il clamp di presidio) → nessun
  doppio vincolo. Nessuna LOS/tattica parallela in SquadSystem.
- Nuove costanti `HOLD_AREA_RADIUS`/`HOLD_ANCHOR_RADIUS`. File toccati: GameConfig, AiSystem (presidio +
  waypoint + leash), SquadSystem (centro area), Application (ruota passa il centro).
- **Build-verified** (Debug + Release); sanity `--sim` per verificare NESSUNA regressione di droidi/cloni
  autonomi (il codice presidio/waypoint è condiviso). **Gli ordini del player NON sono testabili in `--sim`**
  (niente giocatore/ruota) → l'effetto del Hold richiede **smoke test visivo** (osservatore sandbox o partita):
  la squadra deve occupare posizioni diverse nell'area e combattere da lì, non congelarsi.
- Prossimi ordini (stessa filosofia, uno alla volta): Advance (sbalzi su firing position), Retreat (zona sicura),
  Regroup (settore a peso max), Follow (cover-to-cover).

## 2026-07-26 (88) — DRY: analisi settori CONDIVISA da torre e comandante (`sectorTacticalWeight`)

Chiusura della revisione droide tattico + torre: le due analisi dei settori erano **due implementazioni della
stessa idea** e già derivate a mano due volte (la 85 fu proprio un riallineamento dopo che erano divergute).
Ora sono **una sola funzione**, così non possono più divergere per costruzione.
- **`sectorTacticalWeight(sec, st, myTeam)`** (statico in AiSystem.cpp, dove vivono ENTRAMBE le funzioni):
  `importanza + pressione×2 + minoranza(foe>mine)×0.8 + possesso-nemico 0.6 + opportunità 0.4`, parametrizzato
  sulla fazione (`mine`/`foe` derivati da `myTeam`; allies=team1, enemies=team2). La torre chiama
  `(…, 1)`; il comandante `(…, 2)` + il bonus di stance `hold conteso 0.5` (specifico del comandante, resta fuori).
- **Comportamento IDENTICO** (termine per termine, verificato per costruzione): la torre e il comandante
  producono gli stessi pesi di prima — è un refactor di pulizia, non un cambio di comportamento. Un futuro
  ritocco della formula ora vale automaticamente per entrambi i lati.
- **Perché non in worldintel**: quel layer deve restare puro su MapDef (niente `World::SectorState`); e le due
  funzioni sono nello stesso file → un helper statico è il posto pulito, senza nuovo accoppiamento.
- **Build-verified** (Debug + Release), sanity sim senza crash. Chiude il "candidato residuo" della 87.

## 2026-07-26 (87) — Copertura strutturale delle CORSIE nella scelta dei fronti del comandante

Terzo passo della revisione droide tattico + torre. Chiuso il gap "distribuzione emergente e sperata" notato
nella 85/86: il comandante troncava ai **3 pesi più alti**, che potevano cadere tutti nella **stessa corsia**
lasciandone una scoperta. Ora la copertura è **strutturale**.
- **Primitiva condivisa `worldintel::lateralCoord`**: proiezione di (x,z) sull'asse PERPENDICOLARE alla
  direzione d'attacco (spawnTeam1→spawnTeam2). Due punti con `lat` vicina = stessa corsia, a prescindere dalla
  profondità (fondamentale: due settori della stessa corsia a profondità diverse sono ~24 m distanti in
  euclidea ma stessa corsia). Pura geometria della mappa, nel layer condiviso → usabile anche dalla torre.
- **Selezione lane-diverse nel comandante**: ordina per peso, poi 1) prende fronti in CORSIE diverse
  (|Δlat| ≥ `COMMAND_LANE_SEP`=16 m), 2) riempie gli slot rimasti coi pesi più alti. Cap 3 invariato
  (distribuzione validata). Su mappa a 3 corsie contese → un fronte per corsia; se una corsia sola è calda →
  concentrazione lì (fallback). La **torre NON è toccata**: segnala già tutti i settori (copertura implicita
  lato cloni), quindi zero rischio di regressione sul lato appena validato.
- **Misurato** (`--sim` Release, DIAG temporaneo poi rimosso): i top-3 scelti cadono sempre in tre corsie
  distinte — es. `Separatist Spawn(lat 0)` + `Charlie(lat 24)` + `Alpha L(lat -24)` (centro/destra/sinistra).
  Lo spawn separatista compare come fronte-centro solo quando i cloni lo attaccano (→ Hold difensivo, corretto).
  Combattimento attivo, nessun crash.
- **Build-verified** (Debug + Release). Nuova costante `COMMAND_LANE_SEP` in GameConfig. Smoke test visivo
  Release consigliato (i droidi coprono tutte e 3 le corsie contese). [[world-tactical-intelligence]]
- Nota DRY: la primitiva delle corsie ora è condivisa; l'estrazione completa di uno `scoreSectors` unico
  (torre + comandante) resta un refactor a parte, a bassa urgenza (i pesi sono già allineati, 85).

## 2026-07-26 (86) — Assegnazione SPAZIALE delle direttive + ripiego PER-FRONTE (droide tattico)

Secondo passo della revisione droide tattico + torre. Sbloccata la precondizione notata nella 85.
- **Assegnazione spaziale (`pickEnemyDirective`)**: prima ogni droide sceglieva il fronte con una roulette
  pesata per weight e **indicizzata dal solo bias** — agnostica alla posizione (il bias fissava il fronte
  ovunque il droide fosse). Ora il peso strategico è modulato dalla **prossimità** del fronte al droide:
  `eff = weight × 1/(1 + dist/COMMAND_PROXIMITY_HALFDIST)` (HALFDIST=30 m, gentile → un fronte molto più caldo
  lontano vince ancora, e la decorrelazione da bias resta → distribuzione preservata). Effetto: i droidi
  servono il fronte rilevante **per dove si trovano**, e una direttiva di ripiego viene raccolta dai droidi
  **vicini** a quel fronte. Nuova costante `COMMAND_PROXIMITY_HALFDIST` in GameConfig.
- **Ripiego PER-FRONTE**: il comandante marca un settore che **collassa** (droidi presenti ma cloni ≥ droidi+2,
  su terreno in mano nemica, non nostro da tenere) con stance **Retreat** e peso modesto (`importanza+pressione`,
  senza i boost minoranza/opportunità: si lascia, non si rinforza). La soglia +2 separa "collassa → abbandona"
  da "poco sotto → rinforza" (il termine minoranza), evitando il conflitto. Il consumo del Retreat (globale o
  per-fronte) ora cade sul **settore controllato più vicino** (`retreatPointForTeam2`), non fino allo spawn.
- **Misurato** (`--sim` Release, DIAG temporaneo poi rimosso): a 6v6 su Training Ground gli sbilanci locali
  restano ±1 (spesso i **droidi dominano**: Delta-Echo 4v1, Alpha 4v2) → il collasso +2 **non si verifica in
  gioco normale**: il ripiego per-fronte è una **valvola di sicurezza** per rout veri/battaglie più grandi, non
  un comportamento quotidiano (corretto: non l'ho forzato abbassando la soglia, renderebbe i droidi timidi).
  **Percorso collaudato forzando la soglia**: emit + esecuzione OK, i droidi cadono su un settore controllato
  `(-24,-24)` (non lo spawn), combattimento ancora attivo (206 eventi), nessun crash, yo-yo trascurabile.
- **Build-verified** (Debug + Release); sim finale (soglia reale): 224 eventi combat, respawn regolari, nessun
  crash/assert. Smoke test visivo Release consigliato. [[world-tactical-intelligence]] [[droide-tattico-concept]]

## 2026-07-26 (85) — Droide tattico: peso delle direttive coerente con la torre (contesa + minoranza + opportunità)

Prima tappa della revisione **droide tattico + torre**. Trovata un'asimmetria: la torre (cloni) era già
"guidata dalla contesa" (peso `importanza + pressione×2 + minoranza + opportunità`, changelog 80), ma il
comandante (droidi) era rimasto "guidato dall'importanza statica" (`importanza × (1+pressione)`, la pressione
solo come moltiplicatore) e privo dei termini minoranza/opportunità. → **i cloni seguivano il fuoco, i droidi
seguivano i pesi a mappa ferma**: due lati incoerenti, e i droidi si massavano meno dove si combatte.
- **Fix peso (contesa-dominante, additivo)**: `importanza + pressione×2 + (terreno nemico 0.6) + (hold 0.5)`
  — stessa filosofia della torre. La pressione guida sia la SCELTA dei top-3 fronti sia la distribuzione dei
  droidi tra essi.
- **Termini speculari alla torre** aggiunti al comandante (`allies`=team1 cloni, `enemies`=team2 droidi,
  [AiSystem.cpp:318]): `+ (allies-enemies)×0.8` se i droidi sono in **minoranza** nel settore (rinforza dove
  siamo sotto) e `+0.4` **opportunità** (settore di valore con ≤1 clone e non in mano loro → sfrutta). Ora i
  droidi rinforzano e sfruttano, non solo "seguono il peso".
- **Build-verified** (Debug + Release); `--sim` Training Ground 90 s: nessun crash, combattimento attivo su
  entrambi i lati, respawn/salute regolari. Richiede smoke test visivo in Release (droidi che si massano sui
  fronti caldi e coprono i lati, non solo l'alta-importanza a riposo).
- **Ritrovamento (ripiego PER-FRONTE, rimandato con motivo)**: le direttive NON sono assegnate spazialmente —
  ogni droide sceglie UNA direttiva via `pickEnemyDirective(bias)`, pesata per weight, **non per dove si
  trova**; e `Retreat` manda comunque allo `spawnTeam2` ignorando `d.x/d.z`. Un ripiego per-fronte ingenuo
  farebbe ripiegare droidi scelti dal bias, non quelli davvero sul fronte perdente → incoerente. Serve prima
  l'**assegnazione spaziale** delle direttive (chi è su/vicino al settore che collassa ripiega). Registrato in
  06_Todo, non forzato come estensione ad-hoc (CLAUDE.md §5.3).
- Nota di revisione: torre e comandante restano DUE implementazioni della stessa idea ("valuta i settori →
  distribuisci le forze"), già derivate a mano due volte. Candidato futuro: layer condiviso `worldintel`
  di scoring dei settori con raggruppamento in FRONTI/corsie (copertura strutturale, non emergente).
  [[world-tactical-intelligence]] [[control-tower-informs-not-orders]]

## 2026-07-26 (84) — Multi-spawn: UI editor (nella sezione Spawn) + fix distribuzione in ConquestMode

Completato e SISTEMATO il multi-spawn (79 aveva schema+loader+spawn; mancava l'UI, e la distribuzione non
funzionava in partita).
- **UI editor** nella **sezione "Spawn"** (con i due spawn originali, non in fondo agli altri metadata —
  feedback utente sull'ordine): lista `[T1/T2] punto #i` + "+ punto T1/T2" (nasce sul focus del viewport) + "-".
  Punti selezionabili dal viewport (pickId team1 -3000-i, team2 -3100-i) e spostabili col **gizmo**. Croci
  azzurre (team1)/arancio (team2). Storage `m_spawnPoints1/2`; save via `saveJsonRMW` (ADR-010): array se
  presenti, campo rimosso se vuoti.
- **Fix distribuzione (era rotta)**: `--sim` gira in **ConquestMode**, non in SandboxMode. Avevo aggiornato solo
  SandboxMode (spawn del bootstrap, poi SCARTATO) → le AI spawnavano comunque tutte al centro. Corretto in
  `ConquestMode::genPositions` (round-robin sui punti). **Verificato**: su Training Ground gli alleati partono
  ora su Charlie (sx)/Bravo-Charlie (centro)/Bravo (dx), le 3 corsie, non più ammassati al centro.
- Build-verified (GFEngine + GFEditor, Debug + Release). Lezione: verificare il MODE reale, non il bootstrap
  ([[verify-effect-not-data]]).

## 2026-07-26 (83) — Muretti bassi + muzzle: fix INTERIM del tiro (senza pose) (KI #82)

Dopo il (82) i cloni sparano dalle rialzate, ma NON dove un muretto basso è nel raggio, e il proiettile esce
dal petto (non dall'arma). Causa: la LOS/tiro va da occhio (1.2 m) al CORPO del bersaglio (~0.5 m) → raggio in
DISCESA che un muretto vicino al bersaglio taglia; e il modello AI è alto 1 m (occhio a 1.2 m, sopra il
modello) → finestra di mira stretta. Fix interim scelto dall'utente (pose rimandate, [[animations-blocked]]):
- **Mira al BUSTO ALTO** (unità): `aimY = tt->y + AI_HALF_Y*0.7` (~ground+0.85 m) invece del centro-corpo →
  il raggio resta più alto e SCAVALCA i muretti bassi, restando dentro la hitbox (~1 m). Strutture invariate
  (mira al collider).
- **Muzzle stimato**: il proiettile parte 0.5 m AVANTI lungo la linea di tiro già verificata (non dal petto) →
  sembra uscire dall'arma e supera un ostacolo a ridosso del tiratore. Sicuro (punto sul raggio LOS-ok).
- **Band-aid dichiarato**: la soluzione vera (muzzle reale della canna + parti del corpo colpibili) richiede le
  POSE, tenute in pausa. Quando si sbloccano, questo va sostituito.
- Nessuna regressione (`--sim`: 1 downed, fermi=0, contatti 46, no crash). Build-verified (Debug + Release).
  Conferma visiva utente: cloni che sparano scavalcando i muretti bassi + proiettile dall'arma.

## 2026-07-25 (82) — VERTICALITÀ: la scelta della posizione di tiro valuta la quota REALE del bersaglio (KI #82)

Il vero motivo per cui NESSUNO sparava cross-quota (cloni sopra↔droidi sotto, "vedono solo un piano
orizzontale"): `bestFiringPosition` e `bestFlankingPosition` controllavano la LOS di tiro con **`p.y + 1.2`
per ENTRAMBI gli estremi** — origine E bersaglio alla quota della POSIZIONE. Cioè valutavano la scelta su un
**piano orizzontale**: una posizione elevata veniva promossa/scartata in base a una LOS a 4.5 m orizzontale,
non verso il nemico a terra → le unità finivano su posti che "sembravano" da tiro ma non battevano sopra/sotto
(l'ingaggio reale, con LOS 3D corretta, poi falliva). La graph dei link (139) usava già le quote reali (per
questo 39/43 elevate risultavano corrette), ma le QUERY di selezione no.
- **Fix**: aggiunto `targetY` a `bestFiringPosition`/`bestFlankingPosition`; la LOS di tiro va ora dall'occhio
  sulla posizione (`p.y+1.2`) alla **quota reale del bersaglio** (`targetY+1`). Aggiornati TUTTI i chiamanti
  (reposition: `tt->y`; enterHunt: suolo alla XZ del contatto via `groundHeightAt`; enemy-aware cloni/droidi:
  `nearestEnemyNear` ora restituisce anche la y). Generale: vale per qualsiasi dislivello.
- **Misurato** (`--sim`, DIAG temporaneo poi rimosso): LOS cross-quota **5%→10%** (2×) e le situazioni di
  ingaggio cross-quota quasi raddoppiate (3724→7128 coppie) — le unità ora si posizionano per il combattimento
  verticale. Salute ok (1 downed, fermi=0).
- **Onestà**: 10% è ancora la media su TUTTE le coppie in gittata (incluse le lontane bloccate dalla mappa
  densa); il punto è il RADDOPPIO e il fatto che ora scelgono posizioni che battono cross-quota. Conferma
  visiva utente: i cloni sparano dal ponte / i droidi sparano in su.
- Build-verified (Debug + Release). Collegato: [[combat-los-eye-height]].

## 2026-07-25 (81) — Positioning ENEMY-AWARE: le unità occupano posizioni con LOS sul nemico (fix "non sparano dal ponte", KI #82)

Diagnosi lunga e rigorosa del "cloni sul ponte non sparano" (KI #82): NON è la mappa (39/43 posizioni elevate
hanno LOS verso il basso) né la gittata (aggro 50) né i muretti. È che i cloni **non occupano le posizioni
buone**: sceglievano il waypoint con `bestAdvantageInArea` (importanza/protezione, **cieco al nemico**) → si
piantavano su punti senza LOS sul bersaglio, col catch-22 (cieco→no target→no Alert→no reposition).
- **Fix** (`AiSystem`, scelta waypoint di cloni E droidi): se c'è un nemico noto nell'area (`nearestEnemyNear`,
  contatti di fatto condivisi), scegli una **posizione di TIRO che lo batte** (`bestFiringPosition`, LOS
  verificata dal peek) invece del terreno solo-importante; fallback a `bestAdvantageInArea` (proattivo) senza
  nemico. Generalizza a tutta la **verticalità** ("sparare da ovunque ci sia una buona posizione").
- **Misurato** (`--sim`, cloni sul ponte, DIAG temporaneo poi rimosso): LOS libera **1%→17%** (~20×), bersaglio
  acquisito **18%→27%**, e frame-cloni bloccati sul ponte cieco **637→353** (~metà). Combattimento sano
  (1 downed, fermi=0).
- **Onestà**: miglioramento chiaro ma PARZIALE — ~83% dei frame-ponte ancora senza LOS (cloni in TRANSITO verso
  la posizione, ciechi mentre si muovono; alcuni non la raggiungono del tutto). Conferma visiva utente.
- Collegato: [[combat-los-eye-height]], [[orders-design-vision]]. Build-verified (Debug + Release).

## 2026-07-25 (80) — Torre di controllo: la CONTESA guida la distribuzione (le forze seguono il combattimento)

Dopo l'analisi della torre (changelog 79): la distribuzione concentrava perché il peso-segnale era dominato
dall'importanza STATICA. Lever #1 (contesa): l'importanza resta il valore di base, ma la **pressione/combattimento
pesa forte** → i cloni affluiscono dove si combatte davvero, spalmandosi coi nemici (che ora spawnano distribuiti).
- **Cambio** (`AiSystem::updateAllyIntel`, peso segnale): `pressione ×0.6→×2.0`, `minoranza ×0.4→×0.8`, `in mano
  nemica ×0.3→×0.6`. Importanza-baseline invariata. [[control-tower-informs-not-orders]]
- **Misurato** (`--sim` Training Ground, 60s): fronti coperti per campione **~2 → 3-4** (ora anche **Alpha R/L**
  e Bravo/Charlie, non solo il centro); gli alleati vanno sui settori **contesi** (Alpha L press=1.0, Alpha
  press=0.67). Nessuna regressione (2 downed, fermi=0). Delta-Echo (profondo, lato nemico) resta scoperto (atteso).
- **Nota**: sinergico col riequilibrio importanze dell'utente (flanchi 2.75 > centro 2.5) e col multi-spawn (79).
- **Aperto**: conferma VISIVA utente; eventuali leve successive (capacità per-segnale 3→2, semantica spawn).
- Build-verified (Debug + Release).

## 2026-07-24 (79) — Multi-spawn per fazione (opzionale) — feature che funziona, ma rivela il vero collo di bottiglia

Richiesta utente: poter aggiungere PIÙ punti di spawn per fazione per migliorare la distribuzione iniziale.
- **Feature (retrocompatibile)**: nuovo campo mappa `spawn_points_team1/2` (array di [x,y,z]); se presente, le
  unità AI si distribuiscono sui punti (`SandboxMode`, un gruppo per punto), altrimenti spawn singolo su
  `spawnTeamN` (invariato). `MapDef.spawnPointsTeam1/2`, parsing nel loader, known-keys aggiornate. Training
  Ground: 3 punti per fazione (corsie sinistra/centro/destra).
- **Verificato (DIAG temporaneo, poi rimosso)**: i punti si caricano (3+3) e gli alleati spawnano DAVVERO
  sparsi su x (−32 … +10), non più tutti al centro. La feature fa ciò che deve.
- **MA finding onesto (il metodo paga)**: nella distribuzione osservata restano comunque quasi al centro. Causa:
  spawnano nel "gap" a z≈35 (fuori dai settori), poi la **torre li ri-converge su Alpha** (imp 5) prima che
  entrino nei settori. → il multi-spawn (causa 1, spawn iniziale) è **necessario ma insufficiente**: il vero
  collo di bottiglia dei "fronti laterali ignorati" è la **concentrazione della torre** (causa 2), che pesa
  `w = importance + …` e coi soli 6 cloni riempie i settori top senza spalmarsi.
- **Prossimo target di ricerca**: la logica di distribuzione torre/comandante — spalmare vs concentrare.
- Build-verified (Debug + Release). ConquestMode non ancora esteso al multi-spawn (segue, se serve).

## 2026-07-24 (78) — Ricerca/stabilizzazione: osservabilità distribuzione + fix del clamp sull'importanza (KI #81)

Fase di ricerca cauta (metodo: osservabilità → misura scarto → una causa/un fix), dopo il revert (77).
- **Osservabilità distribuzione (PERMANENTE)**: nuovo evento telemetria `sector distribution` nell'heartbeat AI
  (occupazione alleati/nemici + importanza + pressione per settore, nel tempo). Rende MISURABILE se l'AI si
  spalma sui settori o si ammassa — rete anti-regressione contro i bug silenziosi di distribuzione.
- **Bug silenzioso trovato e fixato (KI #81)**: il loader applicava `clamp01` all'`importance` di settori E
  delle 170 posizioni tattiche → l'autore l'aveva graduata 0.5–6 (Alpha=5, spawn=6, fronti=4…) ma il runtime
  vedeva **1.0 per tutto ≥1**: la regia tattica per priorità non arrivava all'AI. Fix: floor a 0, niente tetto.
  Verificato: runtime ora Alpha 5.0 / fronti 4.0 / flanchi 2.5 (prima 1.0). Consumatori lineari, nessuno
  assumeva [0,1]; editor legge raw (UX invariata); validazione OK; nessuna regressione (fermi=0, letale).
- **Metodo che ha pagato**: l'audit geometrico ha CORRETTO una mia prima ipotesi sovrastimata (sovrapposizioni
  di settori: in realtà griglia 3×3 adiacente, doppio-conteggio solo ai bordi) prima che diventasse un fix
  sbagliato. È il valore del "capire la causa prima di toccare".
- **Aperto**: osservare la distribuzione su dati ora corretti (ricerca #1 continua); valutare se l'importanza
  (fino a 6) over-domina protezione/pericolo in alcune query → ri-bilanciamento misurato. Build-verified.

## 2026-07-24 (77) — REVERT del Hold/Advance/reposition custom: gli ordini scavalcavano l'AI che già funziona

Diagnosi strumentata (contatori DIAG temporanei in `--sim`, poi rimossi) che ha chiuso il ciclo fix-su-fix:
- **Le AI SPARANO da cover/rialzato**: sul ponte (y>3) 29/31 quando pronte; per unità con ordine Advance 97/99.
  Il tiro NON è mai stato il problema.
- **MA le unità con ordine ADVANCE erano SENZA bersaglio l'81% del tempo** (in Alert solo 19%): il mio Advance
  custom le piazzava/teneva a posizioni scelte con una LOS/gittata (`hasLineOfFire` MapDef, 28 m) **scollegata**
  da come l'AI acquisisce davvero (`hasLineOfSight` sui collider + `aggroRange`). E #76 **spegneva il reposition
  autonomo** sotto ordine — ma quel reposition **è già il bounding overwatch che funziona**. In breve: avevo
  riscritto peggio un sistema che già funzionava, e i fix su fix aggravavano (rischio stabilità segnalato).
- **Decisione utente: REVERT del custom, TIENI LOS + ruota.** Rimossi: `OrderType::Advance` + Advance continuo
  (73/74), distribuzione Hold per valore di controllo (72), `worldintel::controlPositionsInArea`/`ControlPos`,
  gate reposition "a esaurimento"/soppressione sotto ordine (76), tutta la strumentazione DIAG. `HoldPosition`
  torna al comportamento base (ognuno tiene la propria posizione); la ruota Advance torna a `MoveTo`.
- **TENUTI**: LOS d'ingaggio ad altezza occhi (75) — i dati provano che fa sparare dalle cover — e la ruota
  comandi in osservazione sandbox (70).
- **Direzione futura** (concordata): gli ordini vanno ripensati come **bias leggero sull'AI autonoma** (che già
  bounda/ingaggia bene), non come override che la scavalca. Vedi [[orders-design-vision]], KI #79/#80.
- **Build-verified** (Debug + Release, 0 errori). Base tornata stabile.

## 2026-07-24 (76) — Le AI TENGONO la posizione invece di fare churn: reposition "a esaurimento", non a timer (KI #80)

Feedback dopo il (75): *"non usano le cover, si muovono tutte libere, invece di prendere una posizione, usarla
per eliminare i nemici e passare alla successiva".* **Diagnosi** (sul codice): il riposizionamento (ADR-035) si
attivava a **timer** (3-6s), non a "posizione esaurita", e **ignorava gli ordini di squadra**. Il (75) ha
raddoppiato il rilevamento (contatti ~20→~40) → unità quasi sempre in Alert → reposition di continuo = **churn**;
e l'Advance/Hold veniva scavalcato dal reposition autonomo.
- **Fix (AiSystem, valutazione reposition)**: `wantMove = justEngaged || !canEngageHere`. `canEngageHere` = ho
  LINEA DI VISTA sul bersaglio dalla posizione attuale (dall'occhio, come l'ingaggio). AVANZATA **e** OVERWATCH
  ora richiedono `wantMove` → l'unità si sposta solo al **primo ingaggio** (per coprirsi) o quando la posizione è
  **esaurita** (nemico non più battibile da lì); altrimenti **TIENE e spara**. È il "prendi una posizione, usala,
  poi passa alla prossima" chiesto dall'utente.
- **Ordini**: sotto `HoldPosition`/`Advance` (`positionOrder`) il reposition autonomo è **soppresso** — la
  posizione la decide l'ORDINE (distribuzione Hold / bounding Advance), non la manovra → fine del conflitto/churn.
- **Misurato** (`--sim` 45s): `manovre_avviate` 6-9 (era 8-15) con `manovra_valutata` 13-17 → ~metà delle
  valutazioni ora è "tieni"; `contatti` 33-55, `fermi`=0, **2 downed** (ancora letale), `stuck` 3, 0 crash.
- **Onestà**: il calo del churn è MODERATO in telemetria; il "feeling" (tengono davvero le cover, poi balzano)
  è una **conferma visiva** dell'utente. Se serve più stabilità: allungare la sosta produttiva o moderare il
  raggio di rilevamento sono le leve successive.
- **Build-verified** (Debug + Release).

## 2026-07-24 (75) — Le AI SPARANO da cover/rialzato: LOS d'ingaggio ad altezza occhi (KI #79, radice comune)

Fix della radice comune dietro "occupano le cover ma non sparano" (specie sul ponte/posizioni rialzate) — mina
Hold, Advance e l'AI autonoma insieme. **Causa**: la LOS di rilevamento/acquisizione/sparo (`AiSystem`) partiva
dal punto GREZZO dell'unità (centro corpo, ground+`AI_HALF_Y`=0.5 m), sotto qualsiasi cover → la propria
copertura/parapetto la bloccava. Il peek (changelog 69) era solo nella SELEZIONE posizioni, non nell'ingaggio.
- **Modello**: **origine = OCCHI** (`COMBAT_EYE_HEIGHT`=1.2 m dal suolo — l'unità si sporge/scavalca la propria
  cover; un muro più alto resta bloccante), **bersaglio = CORPO** (transform ~0.5 m → il colpo non passa sopra la
  testa, e un nemico DENTRO la sua cover resta protetto perché la LOS è bassa vicino a lui).
- **Coerenza end-to-end** (`AiSystem`): sensing, acquisizione, LOS di sparo, **spawn E traiettoria del proiettile**
  ora tutti dall'occhio verso il corpo → l'unità spara SOPRA la propria cover, non pianta il colpo nel muro davanti.
  Struttura ancora mirata al collider (corpo). Nuova costante `config::COMBAT_EYE_HEIGHT`.
- **Coerenza `SquadSystem`**: il "posso ingaggiare da qui" e `nextAdvanceFiringPos` dell'Advance usano le stesse
  quote (origine occhi, nemico corpo).
- **Misurato** (`--sim` Training Ground, 45s, corretto): `contatti_vivi` 37-40, `fermi`=0, `tiro_trovato` 5-10 /
  `tiro_assente` 0-2, **3 "member downed"** (combattimento LETALE: i colpi colpiscono), `stuck`=3, 0 crash. Tasso
  eliminazioni moderato → nessun sintomo di "tiro attraverso i muri".
- **Nota di percorso**: un primo tentativo alzava anche il BERSAGLIO a 1.2 m → i colpi passavano sopra la testa
  (hitbox ~1 m); corretto a origine-occhi/bersaglio-corpo prima della misura buona.
- **Build-verified** (Debug + Release). Conferma VISIVA all'utente: i cloni sul ponte sparano ai nemici sotto, e
  nessun colpo attraversa muri pieni.

## 2026-07-23 (74) — ADVANCE: avanzata a balzi CONTINUA (bounding overwatch) — rework dopo playtest

Feedback sul (73): *"avanzano di un tot ad ogni comando, devo ridarlo; alcuni cloni restano indietro
incagliati sulle cover"*. Chiarita l'intenzione: ADVANCE deve essere **autonomo e continuo** — ognuno prende
una posizione con **linea di tiro sul nemico**, combatte, e appena non ha più bersagli da lì **salta da solo
alla successiva più avanti**. Ripensato da balzo one-shot a comportamento continuo nel ciclo di vita.
- **`SquadComponent`**: nuovi `advGoalX/Z` (direzione comandata, ripiego quando non c'è nemico visibile).
- **`SquadSystem` ciclo di vita `case Advance`** (per-membro, throttlato dall'ARRIVO per non ripianificare a
  ogni tick): se sto viaggiando non cambio meta; arrivato, se ho un nemico ingaggiabile (gittata +
  `worldintel::hasLineOfFire`) **resto e combatto**; altrimenti **balzo** alla prossima posizione di tiro.
- **`nextAdvanceFiringPos`**: fra le posizioni entro un balzo che (a) avanzano verso il nemico, (b) hanno
  LINEA DI TIRO su di lui, (c) non sono già presidiate da un compagno (`occupiedByOther`, anti-ammasso), la
  migliore per protezione + avvicinamento. **Scartata se irraggiungibile dal navmesh** (`nav->isReachable`) →
  attacca la causa dell'"incaglio su cover" (posizioni non raggiungibili). Niente posizione utile → passo
  dritto verso il nemico (avanza comunque, non si blocca).
- **`nearestEnemy`** guida il balzo verso il nemico più vicino; ri-emettere Advance non serve più (è continuo).
- Rimossi `distributeAdvance` + costanti del balzo one-shot; leash `AiSystem` di Advance resta 2 m.
- **Nota**: la SCELTA fra i nemici è "il più vicino" (non ancora vincolata alla direzione comandata) — possibile
  refinement se avanzano verso un nemico alle spalle. Il refinement enemy-aware del valore di controllo resta
  differito per HOLD/Retreat/Regroup ([[orders-design-vision]]).
- **Build-verified** (Debug + Release, 0 errori). Smoke test: ADVANCE una volta sola → la squadra avanza a
  balzi da posizione di tiro a posizione di tiro verso il nemico, combattendo, senza restare indietro.

## 2026-07-23 (73) — Ordini Stadio 2: ADVANCE tattico (avanza occupando le posizioni di controllo)

Secondo comando della ruota reso tattico, **riusando i mattoni dell'HOLD** (changelog 72) come promesso:
la distribuzione su posizioni di controllo è ora un primitivo condiviso e ADVANCE cambia solo *dove* mette
l'area.
- **Nuovo `OrderType::Advance`** (prima la ruota Advance era un `MoveTo` grezzo a un punto). La ruota (settore
  alto-dx, tasto B) lo emette col target come DIREZIONE d'avanzata (mira/camera).
- **`SquadSystem::distributeAdvance`**: baricentro squadra → direzione verso il target → area **un balzo avanti**
  (`kAdvanceBound` 15 m, raggio 14 m) → `worldintel::controlPositionsInArea` + `assignDistinct` (gli stessi di
  HOLD). I membri avanzano su posizioni di controllo DISTINTE verso il nemico, non in linea retta a un punto.
  Ri-emesso = balzo successivo (bounding). Fallback senza candidato: il punto comandato (avanza comunque).
- **Refactor**: estratti `collectSquadMembers` + `distributeOverArea` condivisi (HOLD e ADVANCE ora sono
  wrapper sottili). Lifecycle: Advance è PERSISTENTE (occupa il terreno avanzato e combatte da lì, come Hold);
  leash `AiSystem` 2 m come Hold; `isImplemented` aggiornato.
- **Limite noto (uguale a HOLD)**: il valore di controllo non pesa ancora *dove sono i nemici* oltre alla
  direzione d'avanzata — refinement trasversale differito ([[orders-design-vision]]), da applicare a tutti i
  comandi dopo averli messi tutti (decisione utente: "non incagliarci qua").
- **Build-verified** (Debug + Release, 0 errori). Smoke test: ADVANCE in osservazione → la squadra avanza
  distribuendosi sulle posizioni di controllo verso il punto mirato, poi combatte da lì; ri-emetti per il balzo dopo.

## 2026-07-23 (72) — HOLD = CONTROLLO D'AREA distribuito (rework dello Stadio 1 dopo playtest)

Feedback utente sul (71): i cloni *correvano solo alla zona di difesa più vicina, non usavano le cover, e
si ammassavano*. Chiarita l'intenzione: **HOLD non è "difendi" ma "controlla l'area in cui ti trovi"** →
qualsiasi posizione strategica (non solo defensive), scelta per la miglior CAPACITÀ DI CONTROLLO, e i
membri devono DISTRIBUIRSI, non accalcarsi. È la base riusabile per tutti gli ordini.
- **Nuova query pura** `worldintel::controlPositionsInArea` (ADR-025/032/033): tutte le posizioni entro
  l'area, ognuna col **valore di controllo** = importanza autorata ×2 + **dominio** (quante posizioni batte,
  via il grafo `positionCovers`, clampato) + protezione − **esposizione** (`positionExposure`) − pericolo.
  Ordinate per controllo. Ranking sui metadata, riusabile da ogni ordine.
- **Distribuzione di squadra** (`SquadSystem::assignDistinct`, primitivo riusabile): ogni membro prende una
  posizione DISTINTA pesando controllo + prossimità e con **penalità di ammassamento** (spread 6 m) → si
  spargono sulle migliori posizioni invece di impilarsi; se una è presa, il successivo va altrove.
  `distributeHold` fa da colla: baricentro della squadra, raggio adattivo alla dispersione, poi assegna.
- **Modulare**: ranking in `worldintel` (puro MapDef), assegnazione in `SquadSystem` (coordinamento squadra).
  Advance/Retreat/Regroup riuseranno `controlPositionsInArea` + `assignDistinct` cambiando solo l'AREA.
- **Supera** l'approccio difensiva-sola del (71) (`bestHoldPosition` + fallback cover, rimosso dal path HOLD).
- **Build-verified** (Debug + Release, 0 errori). Smoke test: HOLD → i cloni si distribuiscono sulle
  posizioni che controllano meglio l'area (cover/vantage/defensive/…), senza ammassarsi, e difendono da lì.

## 2026-07-23 (71) — Ordini più tattici, Stadio 1: HOLD occupa la difensiva vicina (non "congelati sul posto")

Prima tappa del rework ordini ([[orders-design-vision]]): la ruota deve dare **posture tattiche** che usano i
metadata, non ordini geometrici. Oggi il `SquadSystem` traduceva HOLD come *"ognuno tiene la posizione
CORRENTE"* (`sq->targetX/Z = tr->x/z`) → le unità si congelavano allo scoperto invece di ripararsi.
- **Fix** (`SquadSystem`, emissione ordine): HOLD ora risolve per ogni membro la **migliore posizione
  difensiva vicina** via `worldintel::bestHoldPosition` (defensive/chokepoint, ADR-046, entro 14 m, pesata
  protezione+importanza−distanza−pericolo). Fallback: `nearestPositionByRole("cover")` se non c'è difensiva
  vicina (la spec include "cover, defense eccetera"); ultimo fallback la posizione corrente → **mai peggio di
  prima**. Poi l'AI difende da lì (il combattimento resta suo: ingaggia da coperto; il guinzaglio HoldPosition
  di 2 m in `AiSystem` la tiene sulla posizione).
- **Modulare, non fuso**: la traduzione postura→destinazione passa dal seam `worldintel` — lo **stesso
  "cervello" tattico** che l'AI autonoma già usa. Ordini e autonomia condividono l'intelligenza sui metadata,
  non due implementazioni parallele.
- **Limite noto (Stadio 1)**: la scelta è per-membro senza prenotazione → due membri molto vicini possono
  puntare la stessa posizione (la separazione del crowd evita l'impilamento; una riserva arriverà con gli
  stadi successivi). Prossimi stadi: Advance (bounding cover→vantage), Retreat (nuovo ordine), Regroup (zona
  a priorità massima), Follow tattico.
- **Build-verified** (Debug + Release, 0 errori). Smoke test manuale: in osservazione, HOLD sulla ruota →
  i cloni raggiungono le posizioni difensive vicine e difendono.

## 2026-07-23 (70) — Ruota comandi + ordini rapidi disponibili in OSSERVAZIONE sandbox (osservatore-comandante)

Richiesta utente: *poter usare la ruota dei comandi e gli ordini rapidi anche mentre si guarda una
simulazione in sandbox, per vedere dall'esterno come reagiscono le AI.* Implementato **riusando l'intera
pipeline ordini esistente** (ADR-020, doc 26) — modulare, non fuso: cambiati solo il *gate* dell'input e
l'*ancora* dell'ordine.
- **Scoperta chiave**: `SquadSystem::formAlliedSquad` gestiva GIÀ la modalità sim (se il player è team 0 /
  parcheggiato, il leader diventa la prima AI alleata e tutta la squadra team-1 entra in `kAlliedSquadId`).
  Quindi squadra + mailbox `world.squadOrder` + consumo in `AiSystem` (override di `moveDX/DZ` verso
  `sq->targetX/Z`, riga ~1901; FocusFire nel targeting) **funzionavano già in sim**: mancava solo l'input.
- **Sblocco input** (`Application.cpp`): la ruota (tasto **B**) e l'ordine rapido (tasto **G**) non sono più
  bloccati da `!observerFly`. Nessun conflitto con i controlli osservatore (WASD/Spazio/Ctrl/mouse).
- **Ancora osservatore-comandante**: nuova lambda `crosshairGround` che marcia il raggio della camera fino al
  suolo calpestabile del `MapDef` (`mapquery::groundHeightAt`). In partita l'ancora resta il giocatore-leader;
  in osservazione è il **punto mirato a terra**. Ruota: Regroup→*RADUNA QUI* (MoveTo sul punto), Hold invariato,
  Advance→*avanza 8 m oltre il punto mirato*, 4° settore→*LIBERI* (seguire una camera in volo non ha senso).
  Ordine rapido: nemico inquadrato→FocusFire, compagno→CoveringFire/Revive, punto a terra→MoveTo/TakeCover
  (cover-point reale entro 4 m). Pre-check di raggiungibilità: origine = prima unità alleata reale, non il
  player parcheggiato. Il volo osservatore si congela mentre la ruota è aperta (come la camera).
- **HUD**: nessuna modifica necessaria — crosshair, visual della ruota e pannello "SQUADRA (N) ORDINE dist"
  giravano già in `state==Playing` (che vale anche in osservazione); prima `wheelOpen` restava solo sempre falso.
- **Build-verified** (`GFEngine.exe`, 0 errori). **Smoke test manuale** da fare dall'autore: avviare un sim,
  tenere **B** e rilasciare su un settore / premere **G** mirando; verificare che la squadra alleata reagisca
  (RADUNA/ADVANCE/HOLD/FocusFire) e che il pannello squadra rifletta l'ordine.

## 2026-07-23 (69) — Modello copertura: la LOS di tiro "si sporge" (peek), non parte dal centro (playtest)

Intuizione dell'utente: *una copertura BLOCCA per definizione parte della visuale dal suo centro
(altrimenti non ripara), ma l'unità si SPORGE per sparare* — quindi testare la LOS di tiro dal centro
della posizione la scartava sempre, anche se il posto ha ottime linee di tiro (studiate dall'autore).
- **Fix**: in `bestFiringPosition`/`bestFlankingPosition` la LOS parte ora da un punto di **PEEK** — avanti
  di `kPeek` (1.5 m) verso il bersaglio — non dal centro dietro la cover. Così la **propria** copertura
  (dietro il peek) non blocca il tiro, ma un muro/edificio **davanti** (fra unità e bersaglio) sì.
  Un solo punto, entrambe le query; è la query layer che diventa più "intelligente" sui metadata.
- **Misurato** (`--sim`): `tiro_trovato` **48 / tiro_assente 0** (100%, era 92%), fianco 26/0 (100%),
  `manovre_avviate` 60, combattimento sano (260), `fermi=0`, stuck 2. Le AI usano ora le cover/vantage
  che prima venivano scartate perché "bloccate" dalla loro stessa copertura.
- **Nota**: rischio teorico di "sparare oltre un muro vicino" (il peek salta i primi 1.5 m) — trascurabile
  perché il bersaglio deve comunque essere in arco+gittata e l'autore non piazza una posizione di tiro
  dietro un muro che la affaccia. Il grafo overwatch (`buildTacticalLinks`, LOS centro-a-centro) NON è
  toccato: possibile rifinitura futura con lo stesso principio.

## 2026-07-23 (68) — Selezione posizioni di tiro: arco morbido + importanza (ADR-031/032, playtest)

Fix (2 di 2): "le AI non sfruttano le posizioni elevate/vantage, usano poco i metadata". **Causa**:
`bestFiringPosition`/`bestFlankingPosition` **ESCLUDEVANO** una posizione se il bersaglio era fuori
dall'arco di fuoco AUTORATO (`facingDeg ± fireArcDeg/2`) — ma un'unità in copertura **si gira** per mirare,
quindi l'arco autorato è più severo della realtà; e le due query **non pesavano l'importanza**, quindi le
vantage elevate ad alta importanza non erano preferite.
- **Arco come PREFERENZA, non esclusione**: la posizione non è più scartata se fuori arco; l'orientamento
  diventa un termine di punteggio (`arcPref`: pieno dentro l'arco, sfuma fuori). La LOS resta un requisito
  duro → non si spara comunque attraverso la propria cover.
- **Importanza nel punteggio** di `bestFiringPosition`: `+ importance*0.5` → il buon terreno autorato
  (anche elevato) conta ora anche in combattimento, non solo in avvicinamento.
- **Misurato** (`--sim`): `tiro_trovato` **49** / `tiro_assente` 4 (92%, era 63% con la LOS accurata /
  90% prima), `fianco_trovato` 23 / 0 (100%), `manovre_avviate` **65** (col fix 1 ~35, di base ~19-25 →
  triplicato). Combattimento sano (255), `fermi=0`. Le AI si spostano molto più spesso su cover/vantage.
- **Scartato (con nota)**: avevo provato la LOS di tiro con la y REALE del bersaglio (linea in discesa
  corretta) → più accurata MA rendeva valide MENO posizioni (rivela che molte elevate non hanno davvero
  tiro pulito verso i bersagli a terra, bloccate dal bordo del ponte) → CONTRO l'obiettivo "usa più
  coperture". Tenuta la LOS permissiva (`p.y+1.2` per entrambi gli estremi). Se in futuro si vuole
  precisione, va accompagnata da authoring/geometria coerente.

## 2026-07-23 (67) — L'AI cerca copertura APPENA ingaggia (ADR-035, playtest)

Fix (1 di 2) al problema "le AI stanno allo scoperto a spararsi, usano poco cover/vantage" (playtest
utente). **Causa**: il riposizionamento in copertura (ADR-035) partiva solo ogni 3-6 s, a probabilità
(`coverPreference`) → spesso l'unità restava dov'era ingaggiata (allo scoperto).
- **Fix (integrato, non fuso)**: quando un'unità ENTRA in Alert (nuovo ingaggio), fa **una** valutazione
  proattiva: cerca subito una posizione di tiro coperta (`bestFiringPosition`) invece di aspettare il
  timer. Realizzato riusando il sistema di riposizionamento esistente (nuovo flag `justEngaged` +
  `repositionTimer=0` all'ingresso in Alert; la valutazione d'ingaggio bypassa la probabilità, il flanking
  resta a personalità). Nessun sistema nuovo, nessuna query nuova — solo un trigger in più. Modulare.
- **Misurato** (`--sim`): `manovra_valutata` 81 (era ~38), `tiro_trovato` **46** / `tiro_assente` 5 (90%
  trova copertura valida), `manovre_avviate` 35 (era ~19-25). Combattimento sano (contatti 273),
  `fermi=0`, nessuna regressione. **Effetto visivo (unità che si mettono al riparo appena ingaggiano) da
  confermare in playtest.**
- **Resta il fix (2)**: l'arco di fuoco autorato è più severo della realtà (un'unità si GIRA per mirare) →
  posizioni con facing "sbagliato" non vengono usate. È il motivo per cui elevate/laterali sono ignorate
  se orientate male. Prossimo giro, separato.

## 2026-07-23 (66) — #2b: anche i droidi usano le posizioni vantaggiose (+ cleanup, sanitizer)

Estende ai **droidi** ciò che i cloni avevano già (changelog 65): il ramo Advance del comandante ora, per
ogni fronte, occupa la miglior posizione vantaggiosa (`bestAdvantageInArea`: vantage/cover per importanza,
anche ELEVATE) **se raggiungibile** (`isReachable`), con **commitment su waypoint + timer 12s** — così i
droidi sfruttano il buon terreno (ponti/zone rialzate) senza oscillare né finire su isole. Riusa i campi
`allySig*` (un'unità è di una sola fazione → nessun conflitto col ramo clone).
- **Misurato** (`--sim`): nessuna regressione — combattimento sano (contatti 327), comando multi-fronte
  (25 fronti, 18 avanzate), hold/route/segnali attivi, `fermi=0`, stuck auto-recuperati. Build 0/0.
- **Cleanup**: risolto il warning C4189 preesistente in `SandboxMenu::handleMouse` (`PH` non usato).
- **Sanitizer (CMake) — ora funziona**: componente installato nella VS 2022; superato il thunk lib,
  emergeva `LNK2038 'annotate_vector' mismatch` (lib vcpkg prebuilt senza annotazioni ASan della STL) →
  aggiunti `_DISABLE_VECTOR_ANNOTATION`/`_DISABLE_STRING_ANNOTATION` sui nostri target quando ASan è ON.
- **BUG trovato da ASan e RISOLTO (global-buffer-overflow)**: `Ui2D::text` passava le stringhe grezze a
  `stb_easy_font_print`, che indicizza `stb_easy_font_charinfo[*text-32]` con `*text` di tipo `char`
  SIGNED → i byte >127 dei **caratteri accentati UTF-8** del testo italiano (à, è, °, …) davano indice
  NEGATIVO → lettura fuori dai limiti (UB silenzioso in release: glifo sbagliato o crash). Fix: si
  igienizza la stringa a ASCII stampabile (non-ASCII → '?') prima di passarla a stb. **Va anche in
  Release** (bug reale del gioco, non solo di test). Vedi KI.
- **Validazione ASan**: dopo il fix, `--sim` gira l'intero AI/nav/combattimento per 60 s **senza un solo
  errore ASan** → tutto il codice toccato di recente (griglia collisioni, `isReachable`, commitment,
  danger, navmesh, #2b) è pulito in memoria.

## 2026-07-23 (65) — Navmesh & AI: danger non-bloccante, raggiungibilità, recupero stuck (playtest)

Analisi profonda richiesta dall'utente dopo il playtest (AI che ignorano ponti/zone elevate, cloni "in
trappola", danger che bloccano). Cause radice trovate nel codice, non supposizioni.
- **Danger = blocco del pathfinding (risolto)**: le danger zone vengono cotte nel **navmesh** come area
  `kAreaDanger` con **costo 10×** (`kCostDanger`). Detour le aggirava quasi sempre → una danger sull'unica
  via (scale centrali di Alpha) diventava un **muro**. Abbassato a **2×**: scoraggia, non blocca. La
  penalità "non sostare in copertura nel pericolo" resta separata (`dangerAt`, ADR-046).
- **AI mandate su target IRRAGGIUNGIBILI → in trappola (risolto)**: le query tattiche sceglievano per
  importanza/protezione **senza controllare la raggiungibilità**; `requestMoveTarget` agganciava il
  target al poligono più vicino (fino a 14m) → un vantage su un'ISOLA navmesh (scala ripida) o su un
  passaggio eroso mandava l'unità contro un muro, e lo **stuck era solo LOGGATO, mai recuperato**. Fix:
  (a) nuovo `NavManager::isReachable` (path Detour non-parziale che tocca il poligono destinazione); i
  cloni usano un vantage **solo se raggiungibile**, altrimenti il centro-settore (a terra); (b) commitment
  su un WAYPOINT (già raggiungibile) + timer di ri-valutazione → niente oscillazione e `isReachable`
  a bassa frequenza (non a ogni tick); (c) **recupero stuck**: un bot bloccato ora ABBANDONA i target
  impegnati (segnale/manovra/àncora/route) e ri-valuta. **Follow-up playtest**: il timer di ri-valutazione
  4s→**12s** perché a 4s scattava a metà di una salita e faceva tornare indietro il clone (l'utente:
  "salgono le scale e a metà tornano"); ora arriva prima di ri-valutare (lo stuck-recovery copre il "non
  arriva mai").
- **Navmesh più fedele alla larghezza reale dell'AI**: voxel XZ **0.30 → 0.20** → erosione
  `walkableRadius` esatta (0.40m = raggio agente) invece di 0.60m arrotondati. I passaggi larghi ~0.8-1.2m
  (dove l'AI passa) non spariscono più. Il **climb (0.55m) era già = allo step-up fisico** dell'AI: scale
  con gradini >0.55m sono non-percorribili anche fisicamente (authoring o `STEP_HEIGHT` globale).
- **Sanitizer**: due blocchi. (1) `/RTC1` (default MSVC Debug, incompatibile con `/fsanitize=address`) —
  **tolto** quando `GF_ENABLE_ASAN=ON`; il configure ora passa. (2) **Mismatch di installazioni VS**: la
  build usa **VS 2022 Community (MSVC 14.44)**, ma il componente ASan è installato in un'ALTRA
  installazione ("18"/MSVC 14.51). Nella VS 2022 mancano i lib ASan → link error `LNK1104: cannot open
  clang_rt.asan_dynamic_runtime_thunk-x86_64.lib`. **Azione utente**: installare "C++ AddressSanitizer"
  nell'installazione **VS 2022 Community** (VS Installer → Modifica → Componenti individuali). Dopo,
  `-DGF_ENABLE_ASAN=ON` builda; a runtime la DLL `clang_rt.asan_dynamic-x86_64.dll` va sul PATH.
- **Verificato**: build 0/0 (un warning C4189 preesistente in SandboxMenu, non da qui); navmesh più fine
  costruisce senza fail; sistemi tutti sani (`fermi=0`, combattimento/comando/hold/route/osservazione);
  gli stuck ora triggerano il recupero. **Effetto visivo (niente più danger-muro, niente più cloni in
  trappola, uso dei passaggi stretti raggiungibili) da confermare in playtest.**

## 2026-07-23 (64) — Torre di controllo: analisi + commitment, non ordini (ADR-040, doc 36)

Rework su richiesta dell'utente dopo il playtest: alcuni cloni restavano indietro "avanti-indietro"
mentre i droidi no. **Diagnosi (codice)**: `pickAllySignal` ricalcolava la scelta a OGNI tick con un
filtro-saturazione volatile → nessun **commitment** → oscillazione; i droidi non oscillano perché
`pickEnemyDirective` non ha saturazione (scelta stabile per bias fisso). L'utente ha inquadrato il ruolo
giusto della torre: **informa/analizza, non ordina** ([[control-tower-informs-not-orders]]).
- **Commitment del clone**: nuovi campi `allySig{X,Z,Valid}` in `AiComponent`. Il clone si **impegna** sul
  segnale scelto e ci resta finché non lo raggiunge o il segnale sparisce (`allySignalExists`), invece di
  ri-sceglierlo ogni tick. Fine oscillazione. (I droidi erano già stabili.)
- **Analisi tattica più ricca**: il peso di un segnale-settore ora legge il campo — non solo importanza +
  pressione, ma: **alleati in minoranza** (`enemies>allies` → peso ↑, rinforza), **in mano nemica**
  (riprendere), **terreno di valore poco difeso** (importanza alta + pochi nemici → sfrutta). La torre
  "dice cosa conta e perché", i cloni si distribuiscono da soli.
- **Verificato**: build 0/0; `segnali_seguiti` alto e continuo, combattimento sano (contatti 257),
  `fermi=0`, nessuna regressione. **Effetto visivo (niente più oscillazione, distribuzione tattica) da
  confermare in playtest.**
- **Resta**: gli ORDINI diretti (destinazioni imposte) verranno dal giocatore / chain of command, non
  dalla torre; e la ruota-ordini in osservazione sandbox (richiesta utente, tracciata in 06_Todo).

## 2026-07-23 (63) — Torre di controllo: avanzata coerente + posizioni vantaggiose (ADR-040, doc 36)

Affronta il problema #3 del playtest utente: "alcuni cloni restano vicino allo spawn a fare avanti e
indietro mentre gli altri ingaggiano". Diagnosi: `pickAllySignal` ritornava `false` quando **tutti** i
segnali della torre erano saturi (`crowd ≥ ALLY_SIGNAL_CAPACITY=3`) → il clone ripiegava sulla pattuglia
locale (idle vicino allo spawn).
- **Fix A — nessun idle, avanzata coerente**: quando tutti i segnali sono saturi, il clone ora
  **rinforza il fronte più vicino** (seconda ondata) invece di pattugliare. La distribuzione fra fronti
  la garantisce già il passo pesato non-saturo; questo elimina l'idle. **Misurato**: `segnali_seguiti`
  passa da spesso-0 a **600-3457**/finestra — i cloni avanzano verso i fronti in continuazione.
- **Fix B — posizioni vantaggiose**: nuova query `worldintel::bestAdvantageInArea` (cover/vantage/
  defensive/chokepoint per **importanza autorata** + protezione, vicina, fuori dal pericolo). Seguendo
  un segnale, il clone punta ora la miglior posizione vantaggiosa del fronte — incluse le **elevate**
  marcate ad alta importanza — invece del bare centro del settore. Realizza "individua i punti
  vantaggiosi" (richiesta utente) e inizia a sfruttare le zone rialzate (parte di #2).
- **Ambito**: applicato ai **cloni** (contesto #3). Estensione ai **droidi** (Advance) e la
  raggiungibilità navmesh dei ponti (#2a) restano per quando affronteremo #2 esplicitamente.
- **Verificato**: build 0/0; combattimento sano (contatti 296), `fermi=0`, torre attiva, nessuna
  regressione. Effetto visivo (cloni sul buon terreno/elevato) da confermare in playtest.

## 2026-07-23 (62) — Perf: indice spaziale collisioni/LOS + `snap` solo-team + build Release (ADR-015)

Su segnalazione utente (lag già con ~25 AI sulla mappa più grande, "per via della mappa più grande e dei
metadati").
- **Fix — griglia dei collider**: `physics::hasLineOfSight` e `hasCollision` iteravano **tutte** le
  entità del mondo (~200 su Training Ground: 175 box + unità) per ogni chiamata, con lookup su hash-map e
  `computeWorldAABB` ricalcolata ogni volta. Ora una **griglia uniforme XZ** (celle 5 m), ricostruita
  1×/tick, riduce le query a O(celle vicine) con AABB precalcolate. Snapshot dei componenti PER VALORE →
  nessun puntatore pendente se un'entità muore a metà tick. File-static, single-thread; `(world,tick)`
  distingue mondi diversi. **Verificato corretto**: i test esatti sono invariati (la griglia cambia solo
  quali candidati testare, superset conservativo), `fermi=0`, tutti i sistemi funzionano. Build 0/0.
- **Diagnosi (misurata, timer temporaneo)**: il tempo dell'update AI **non scala col numero di AI** (12
  AI ≈ 1250 µs, 50 AI ≈ 1370 µs: **+10 % per 4× unità**). Scala col **numero totale di entità** (i box di
  geometria): il pre-loop AI aveva ~10 passaggi che iteravano TUTTE le entità ogni frame.
- **Fix 2 — `snap` solo team**: `snap` (la lista che tutti i passaggi AI scorrono) ora contiene solo le
  entità con un `Team` (unità/strutture/player), **non** i ~175 box di geometria. Quei passaggi già
  saltavano ciò che non ha team → **comportamento invariato**, ma toglie ~175 iterazioni × ~10 passaggi
  per frame su Training Ground: è il costo che scalava con la DIMENSIONE della mappa. Verificato: tutti i
  sistemi scattano identici (hold, overwatch, comando, route, osservazione), `fermi=0`.
- **Build Release**: le misure erano in **Debug** (`build/windows-debug`, non ottimizzata); in Release il
  costo crolla di 10-30×. Costruita e verificata `build/windows-release/Release/` — **l'utente conferma
  lag "migliorato in maniera evidente"** giocando la Release. Il lag era in gran parte artefatto Debug.
- **Resta per il futuro** (dichiarato dall'utente: più AI e mappe più complesse in arrivo): indice
  spaziale anche per le query tattiche/`hasLineOfFire`, e profilazione degli altri sistemi (crowd,
  rendering) quando il numero di AI crescerà oltre.

## 2026-07-23 (61) — Overwatch: il segnale d'avanzata PERSISTE (ADR-032, F5 completa)

Chiude l'ultimo gap di F5: `overwatch_avviati` sempre **0**. **Diagnosi** (contatori temporanei): il
grafo `positionCovers` è ben popolato (1364 link su Training Ground), le avanzate avvengono (~19), ma il
ramo overwatch non trovava MAI un'avanzata di un compagno vicina (`dbg_ow_adv=0`). **Causa**: il segnale
d'avanzata viveva **un solo tick** (`m_advancesPrev.swap(m_advances)`), ma la manovra dura ~6 s e le
valutazioni di chi copre sono sparse (ogni 3-6 s per unità) → non coincidevano quasi mai.
- **Fix**: ogni `Advance` ha ora un **TTL** (~5 s) e persiste nel pool finché non scade, invece di
  sparire dopo un tick. Le avanzate nuove entrano dopo il decadimento → visibili dal tick successivo (la
  scelta di chi copre resta indipendente dall'ordine di iterazione, ma il segnale dura abbastanza).
- **Misurato** (`--sim "Training Ground"` 6v6, 90 s): `overwatch_avviati` **4** (era 0) — chi non avanza
  si sposta su una posizione che, per il grafo, copre il compagno in avanzata. Modesto ma reale (serve
  compagno-in-avanzata + non-avanzante vicino + copritore entro 18 m). Build 0/0, contatori debug rimossi.
- **F5 — tutti i sistemi scattano INSIEME** su Training Ground: overwatch 4, hold 543, osservazione 597,
  route 54, manovre 19, combattimento (contatti 295), `fermi=0`. La mappa è ora il banco di prova voluto.

## 2026-07-22 (60) — Presidio completo: il comando ordina TIENI e i droidi tengono la linea (ADR-042/046, F5)

Affronta il gap "il comandante non emette mai Hold" (F5, doc 39). **Causa**: la stance Hold richiedeva
una maggioranza di *unità* droidi nel settore (`controllingTeam==2 && allies>0`) — evento raro in 6v6, e
comunque il Hold pesava meno degli Advance → sempre troncato dai top-3.
- **Fix (AiSystem, directive building)**: un settore che ospita un **command post posseduto dai droidi**
  ed è **minacciato** (cloni presenti) diventa un fronte da TENERE, non da attraversare. Il possesso del
  post è una condizione **stabile** (dura finché non te lo riprendono), a differenza della maggioranza di
  unità. Peso del Hold aumentato (+0.35) così difendere un obiettivo conteso compete coi fronti d'attacco.
- **Fix droide-side (opzione A, scelta utente)**: un droide assegnato a un fronte TIENI si **àncora**
  alla miglior posizione difensiva/chokepoint dell'area (`bestHoldPosition`) e ci **combatte da lì senza
  inseguire** — forma una linea difensiva invece di caricare allo scoperto. Meccanismo: nuovo campo
  àncora `holdX/Z/Radius` in `AiComponent` + clamp analogo al leash del comandante (rientra sulla
  posizione se supera il raggio). Valutato a INIZIO tick (prima dei rami di combattimento): il TIENI
  scatta durante la minaccia, quindi il ramo waypoint — raggiunto solo fuori dal combattimento — non
  bastava. Attiva finalmente i ruoli `defensive`/`chokepoint` (ADR-046).
- **Misurato** (`--sim "Training Ground"` 6v6, 110 s): `cmd_tieni` scatta (5/11 finestre, prima **0**);
  **`hold_su_posizione` ora scatta** (318 e 225 quando il Hold è su un obiettivo d'angolo con chokepoint;
  0 quando è su Alpha, che non ha posizioni difensive vicine — corretto). Combattimento sano (contatti
  fino a 55), `fermi = 0` (l'àncora non blocca nessuno), Advance dominante (`cmd_avanzata` 2-3): nessuna
  passività. Build 0/0.
- **Nota mappa**: perché il presidio scatti anche sul settore centrale servono posizioni `defensive`/
  `chokepoint` vicine ad Alpha — authoring dell'utente, non codice.

## 2026-07-22 (59) — Training Ground: misura banco-di-prova + 7 danger zones (doc 39, fase F5)

Primo passo del "salto di complessità" (F5). **Misura `--sim` (6v6)**: la mappa già **funziona** —
comandante attivo a 3 fronti (Alpha/Bravo/Delta), combattimento, route/osservazione/manovre scattano.
(Un primo allarme "sistemi a 0" era un falso: l'harness PowerShell non quotava lo spazio in "Training
Ground" → sim su mappa sbagliata. Lezione in [[powershell-quote-args-with-spaces]] e già in KI #77.)
- **Aggiunte 7 danger zones** su Training Ground (unico gap di contenuto: erano 0) per esercitare
  cover-evita-pericolo (ADR-046): artiglieria sul centro Alpha, 2 corsie di fuoco N/S, 4 chokepoint
  minati agli obiettivi d'angolo. Poste vicino al cover così la scelta della copertura le evita.
  RMW: toccato solo `danger_zones`. Mappa valida (7 danger), sim funziona senza crash.
- **Effetto AI da osservare a mano**: non esiste un contatore telemetria per "copertura che evita la
  danger"; l'utente rivede/aggiusta le pose nell'editor (ora con rendering solido F3).
- **Gap ancora aperti** (non contenuto): `overwatch` non scatta mai (posizioni che non si coprono),
  `hold` mai (comandante sempre Advance). Vedi doc 39 F5.

## 2026-07-22 (58) — Editor UX: duplicazione di qualsiasi metadato (doc 39, fase F4)

Fase F4 di doc 39: "metadata senza attrito". Inserire i metadata era laborioso perché ogni nuovo
elemento partiva dai default e andava ri-regolato campo per campo.
- **"Duplica" generalizzato**: prima funzionava solo sui box. Ora `duplicateSelected()` duplica
  **qualsiasi** elemento selezionato — posizione tattica (con ruolo/arco/gittata/protezione), settore,
  danger, bersaglio, command post, percorso (tutti i punti), spawn veicolo — copiando **tutti** i campi
  autorati, spostato di +2 in XZ per non sovrapporre, e seleziona la copia. Spawn e comandante (unici)
  restano non duplicabili. Si autora una volta e si posa una serie.
- **Default già sensati**: le nuove posizioni nascono cover/arco 120°/gittata 25 m; i pannelli hanno
  tooltip esplicativi. Non serviva altro qui.
- *Rimane possibile* (non fatto): un vero "pennello" click-per-posare in serie — la duplicazione ne è la
  versione pragmatica. Da valutare dopo il collaudo.
- **Verificato**: build 0/0; editor si avvia. **Da collaudare a mano**: Duplica su una posizione tattica
  ne crea una gemella accanto con gli stessi valori.

## 2026-07-22 (57) — Editor UX: superfici visibili (facce piene), non solo wireframe (doc 39, fase F3)

Fase F3 di doc 39. La viewport disegnava i box a **sole linee** → volumi e coperture difficili da leggere
("le superfici sono completamente invisibili", utente).
- **Facce piene ombreggiate**: `setMapBoxes` costruisce ora, oltre al wireframe, le 6 facce di ogni box
  (2 triangoli l'una) con una **finta luce** per-faccia (alto chiaro → sotto scuro) così i volumi si
  leggono senza normali/illuminazione nello shader. Disegnate PRIMA del wireframe con `glPolygonOffset`
  → gli spigoli restano nitidi sopra, niente z-fighting.
- **Nessun rischio compat**: opaco, **nessun blending**, stesso shader e stesso path client-side-array /
  OpenGL 3.3 Compatibility — ADR-003 rispettato (aggiunta di triangoli, non cambio di pipeline).
- **Toggle "Solido"** nella toolbar (default ON): si torna al solo-wireframe con un clic.
- **Verificato**: build 0/0; editor si avvia senza crash GL. **Da collaudare a mano** (visivo): le
  superfici si vedono piene e i volumi si leggono; il toggle Solido/wireframe funziona.

## 2026-07-22 (56) — Editor UX: alzare le strutture + creare davanti alla camera (doc 39, fase F2)

Fase F2 di doc 39: "alzare e posizionare". Due frizioni concrete segnalate dall'utente.
- **Strutture strategiche alzabili (Y)**: `StrategicTargetDef` ha ora un campo `y` = altezza sopra il
  suolo (0 = a terra, retro-compatibile). Il game mode piazza la struttura a `groundHeightAt(x,z) + y`.
  Cablato end-to-end: schema runtime + loader (`DefinitionRegistry`) + `structures::spawnAll` + editor
  (struct `TargetEntry`, load/save RMW, gizmo Sposta su Y, slider "Y (altezza)", anteprima alla quota
  giusta). **Effetto verificato** (`--sim`): torre con `y:5` autorato → Y mondo 5.10; torri senza `y` →
  0.10 (invariate). Nuova telemetria: `y` nell'evento "strategic target spawned".
- **Perché solo le strutture, non il comandante/veicoli**: comandante e veicoli sono unità con
  **gravità** (`AI_GRAVITY`) — spawnate in aria cadrebbero. Alzare la loro Y sarebbe un dato senza
  effetto ([[verify-effect-not-data]]); per metterli in alto si usa una piattaforma (la gravità li
  appoggia sopra). Le strutture sono statiche → la Y è efficace. I settori sono aree 2D (XZ): niente Y.
- **Creare davanti alla camera, non al centro** (F2b): nuovo `FreeCameraViewport::groundFocusPoint()`
  (intersezione dello sguardo col suolo, ripiego a distanza fissa). Tutti i "+ Aggiungi" del Map Editor
  (box, post, bersaglio, posizione, settore, pericolo, percorso, veicolo, comandante) creano l'oggetto
  **dove stai guardando**, non da trascinare dal centro.
- **Verificato**: build 0/0; `--validate` contenuto valido; editor si avvia. **Da collaudare a mano**:
  gli oggetti nascono davanti alla camera; lo slider/gizmo Y alza le torri nel viewport.

## 2026-07-22 (55) — Editor UX: igiene toolbar + click-through gizmo (doc 39, fase F1)

Su feedback dell'utente: aggiungendo "Nuova mappa" (v54) la toolbar in alto è andata in overflow,
tagliando fuori i pulsanti Sposta/Ruota/Scala. Apre la fase **F1 di doc 39 (Editor UX)**.
- **Toolbar sfoltita**: rimossi i pulsanti modalità gizmo (Sposta/Ruota/Scala) dalla toolbar — erano un
  **duplicato** dell'overlay che appare in alto a sinistra sulla viewport quando selezioni un oggetto.
  Resta solo l'overlay. Le capacità ruota/scala per tipo le imposta già `updateViewport()` ogni frame.
- **"Nuova mappa" spostata in coda al dropdown delle mappe**: voce "＋ Nuova mappa…" → **popup di
  conferma** (nome + Conferma/Annulla), invece del pulsante+InputText sciolto sulla barra.
- **Bug click-through risolto**: cliccando i pulsanti Sposta/Ruota/Scala dell'overlay si cambiava
  modalità **e** si selezionava l'oggetto dietro in prospettiva. Ora la barra è un gruppo ImGui e, se il
  click ci cade sopra, la selezione a raggio sottostante viene annullata (`m_gizmoBarHovered`).
- **Principio registrato** ([[ui-no-clipping-use-dropdowns]]): non far tagliare comandi; raggruppare in
  dropdown quando la barra è satura; l'ordine e il raggruppamento contano.
- **Verificato**: build 0/0; editor si avvia senza crash. **Da collaudare a mano**: overflow risolto,
  popup nuova-mappa, click sui pulsanti overlay che non seleziona più dietro.

## 2026-07-22 (54) — Editor: creazione di nuove mappe

Colma una lacuna segnalata dall'utente: dall'editor si potevano **solo modificare** le mappe esistenti,
non crearne di nuove.
- **Creazione nuova mappa** dal Map Editor (voce "＋ Nuova mappa…" in coda al dropdown delle mappe →
  popup nome/conferma; vedi v55 per la collocazione finale). Crea `data/maps/<id>.json` con lo **schema
  minimo valido e giocabile** — un box `floor` 50×40 (senza, niente navmesh: `ContentValidation` lo
  rifiuterebbe e le unità cadrebbero nel vuoto) più i due spawn — poi passa alla mappa nuova. Geometria,
  metadata, roster e comandante si autorano dopo.
- **`InputText` legittimo**: nominare un file NUOVO è l'eccezione documentata alla regola "dropdown-only"
  (come "+ Nuova entità" in EntityEditor). Guardia sull'id: no separatori di percorso/caratteri illegali,
  no sovrascrittura di una mappa esistente.
- **Verificato**: build 0/0; una mappa con questo schema minimo carica nel registry, passa `--validate`
  (0 errori) e gira in `--sim` senza crash. **Da collaudare a mano**: il click nell'editor (ImGui).

## 2026-07-22 (53) — Comportamento ai ruoli tattici + cover evita il pericolo (ADR-046, audit P3)

Chiude l'ultimo blocco dell'audit doc 38: i metadata autorabili nell'editor tornano tutti letti
dall'AI, e sparisce il codice morto.
- **`observation` → vista estesa**: un'unità entro 10 m da un punto di osservazione vede più lontano
  (`aggroRange ×1.5`) — chi presidia ingaggia prima. Telemetria `obs_vista_estesa`.
- **`defensive`/`chokepoint` → posizioni da tenere**: nuova query `bestHoldPosition`; sotto comando
  `Hold` le unità del fronte puntano la miglior posizione difensiva/di strozzatura dell'area (protezione
  + importanza, fuori dalle danger zone) invece di un punto qualunque. Telemetria `hold_su_posizione`.
- **La copertura evita il pericolo**: `bestCoverToward`/`bestFiringPosition` sottraggono `dangerAt` dal
  punteggio → a parità di protezione si sceglie la copertura fuori zona di fuoco. `dangerAt` non è più
  inerte (chiude B2).
- **Pulizia**: rimossi `bestOverwatchFor` e `pickObjectiveSector` (codice morto, B3).
- **Bug harness risolto (KI #77)**: `--map <id>` era ignorato in `--sim`/sandbox (vinceva il default
  `m_mapSel=0`) — le misure `--sim` giravano su Training Ground invece che sulla mappa nominata. Ora il
  flag vale ovunque. È il motivo per cui `obs_vista_estesa` risultava 0 prima del fix.
- **Misurato** (`--sim --map firebase`, ora davvero firebase): `obs_vista_estesa` **333–1069**/finestra;
  C3 sempre attivo; `hold_su_posizione` 0 in un 10v10 bilanciato (Hold raro — logica corretta, da vedere
  in scenario di presidio). Build 0/0; `--validate` 0/0.

## 2026-07-22 (52) — Route fluide e obbedienti al comando (ADR-045, audit P1+P2)

I due attriti più grossi dell'audit (doc 38), affrontati insieme perché sono un unico ripensamento del
ramo pattuglia.
- **P1 — le route obbediscono al comando**: prima le unità con una route (~metà forza) ignoravano
  comandante e torre (il ramo richiedeva `patrolRoute < 0`). Ora **Advance/Retreat valgono per tutti**;
  Hold/nessun comando → pattuglia. Metà forza non è più sorda.
- **P2 — route fluide**: percorse **bidirezionalmente** (`patrolReverse`, si inverte agli estremi
  invece del salto-teletrasporto); **raccolte dal punto più vicino** (`joinNearestRoute`, non solo
  dagli estremi); **cambiabili** (uscendo da Search l'unità si sgancia e riaggancia la più vicina).
  `patrolSeg` è ora l'indice del punto-obiettivo, non del segmento.
- **Misurato** (10v10): `su_route` **5-10** unità agganciate a una route lungo la partita (join/cambio
  dinamici); il comando sovrascrive (a t=93 "Ripiegamento" → lasciano le route); `stuck` 9, nessuno
  spike. Build 0/0; `--validate` 0/0. Nuova telemetria `su_route`.
- **Restano** (audit P3): dare senso ai 3 ruoli decorativi + far evitare le danger alla scelta di
  cover; poi la pulizia del codice morto.

## 2026-07-22 (51) — Audit integrazione Metadata↔AI (analisi, doc 38)

Su richiesta dell'utente ("far funzionare insieme i sistemi"), audit di cosa i metadata autorati
producono davvero nel comportamento AI. Nessun codice cambiato — è la base per decidere cosa rifinire.
Risultato completo in **38_AuditIntegrazioneMetadata.md**. In sintesi:
- **Sano nel complesso**: la maggior parte dei metadata è consumata, nessun sistema si contraddice.
- **Attriti (sistemi non insieme)**: le **route ignorano il comandante/torre** (~metà forza sorda al
  comando); le route sono **rigide** (solo avanti, no join-da-qualsiasi-punto, no cambio); la scelta
  di cover/tiro **non evita le danger zone**.
- **Authoring sprecato**: **3 ruoli su 5** delle posizioni tattiche (`defensive`/`chokepoint`/
  `observation` — 17 istanze autorate) non hanno alcun consumatore per-ruolo; `dangerAt` e
  `bestOverwatchFor` sono query **morte**.
- **Priorità proposte**: P1 route↔comando, P2 route fluide, P3 dare senso ai 3 ruoli + cover-evita-
  danger; poi pulizia. Realizza il "far funzionare insieme" senza aggiungere sistemi.

## 2026-07-22 (50) — Editor "Comando" (ADR-041 §4 completata)

Chiude la parte del rework del comandante: la **casa d'authoring** per il comando. Tab **"Comando"**
nel BalanceEditor.
- **Editor del CommanderDef**: lista dei comandanti + edit di tutti i campi con **dropdown dal
  registry** (corpo/`base_entity`, arma di autodifesa, profilo AI), **checkbox** per le abilità,
  slider hp/velocità/scala, color picker per la tinta, fazione. Salva via RMW. Prima il CommanderDef
  (creato da ADR-044) si modificava solo a mano nel JSON.
- **Parametri `COMMS_LOST_*` spostati qui** dal tab Gameplay (come previsto da ADR-041 §4: "casa dei
  COMMS_LOST_*"). Il tab Gameplay tiene squadra/rianimazione; il tab Comando tiene comandanti + degrado
  comunicazioni.
- **Strutture (torri/bersagli)**: restano **per-mappa** nel Map Editor (sono istanze piazzate, non def
  globali) — il tab lo dichiara esplicitamente per non confondere.
- Build 0/0; `--validate` 0/0. **Da verificare a mano**: aprire il tab Comando, modificare il Droide
  Tattico (hp/arma/tinta), salvare e riavviare.

## 2026-07-22 (49) — Il Droide Tattico esce dal sistema classi (ADR-044, ADR-041 Fase 2)

La parte più architetturale del rework del comandante: **migrarlo fuori da `class`**. Una classe è una
professione istanziabile (ADR-023); il comandante è un'unità unica a ruolo strategico che non combatte
— non ci stava.
- **Nuovo tipo `CommanderDef`** (`data/commanders/tactical_droid.json`): `base_entity` per il corpo +
  override del comandante (hp assoluti, arma di autodifesa, profilo AI, abilità, tinta, scala).
  Loader + accessor nel registry.
- **Riuso, niente duplicazione**: `resolveCommanderArchetype` delega a `resolveUnitArchetype(base_entity)`
  per il corpo e sovrascrive gli override — la risoluzione arma/proiettile resta l'unica esistente.
- **Authoring**: `MapDef.commander.unit` → CommanderDef; dropdown del MapEditor da `data/commanders/`.
  **Fallback** (transizione): una classe-comandante legacy è ancora accettata da spawn e validazione.
- **Validazione estesa**: base_entity valido + ability "command" sul CommanderDef; o classe legacy.
- **Classe rimossa**: migrate firebase e Training Ground a `tactical_droid`, tolto
  `data/classes/Tactical Droid.json` → non compare più nel roster (né spawnabile come truppa in sandbox).
- **Verificato end-to-end**: comandante spawnato dal CommanderDef, **dirige** (`cmd_fronti` 3), resta nel
  **leash** (`cmd_deriva_m` 1.0), e continua **dopo la rimozione della classe**. Build 0/0; `--validate`
  0/0.
- **Restano** (raffinamenti, non bloccanti): ruolo comando implicito nel tipo (oggi via ability, rischio
  zero); entità-a-sé con corpo proprio; editor "Strutture & Comando" dedicato.

## 2026-07-22 (48) — Diagnosi corrette: mouse fullscreen e rename (le prime erano sbagliate)

Analisi d'integrità richiesta dall'utente dopo che due fix del giro (47) non avevano funzionato. Le
**diagnosi originali erano errate**; ecco le cause vere.
- **Mouse "sfasato" in fullscreen — CAUSA VERA**: non era la modalità mouse relativa (la camera usa
  delta relativi, indipendenti dalla risoluzione — non era mai rotta). Era la **UI 2D**: `Ui2D::begin`
  faceva `glOrtho(0, m_w, m_h)` con `m_w/m_h` **fissi alla creazione** (es. 1280×720), mentre il
  viewport GL è il drawable reale (1920×1080 in fullscreen). L'UI si stirava a schermo (sembrava ok),
  ma il **mouse arriva in pixel reali** → click/hover sfasati del rapporto di scala. Colpiva TUTTE le
  UI cursore (respawn map, pausa, menu sandbox, pre-partita). **Fix in un punto solo**: `Ui2D::begin`
  ora sincronizza `m_w/m_h` dal **viewport GL reale** (`glGetIntegerv(GL_VIEWPORT)`) → coordinate UI
  == pixel finestra == coordinate mouse. Sistema tutte e 6 le istanze Ui2D insieme.
- **Rename mappa — la sync era corretta ma non l'avevo esercitata**: il file era stato rinominato la
  sera prima (con l'editor pre-fix), quindi il campo `name` era rimasto "Outpost". Ma per non
  dipendere dal timing del rename ho aggiunto un **campo "Nome" nella toolbar del MapEditor**: edita
  direttamente `name` (ciò che partita/sandbox mostrano), salvato con la mappa via RMW. Più il rename
  che sincronizza `name` col nuovo id, più il fix diretto del file già rinominato (`Training Ground`).
- **Analisi d'integrità**: nessun altro consumatore usa dimensioni finestra stantie (`getWidth/Height`
  solo su Texture, non-UI); tutti i mouse-coord vengono da eventi SDL in pixel finestra, coerenti con
  la UI ora viewport-based; nessun raycast mondo da posizione mouse. `m_width/m_height` di Window
  restano non aggiornati al resize ma è benigno (la UI 2D usa il viewport, il 3D il drawable).
- Build 0/0; `--validate` 0/0. **Da verificare a mano**: mouse in fullscreen ora allineato, e il
  campo Nome del MapEditor.

## 2026-07-22 (47) — Quattro fix segnalati dall'utente (crash salva-gameplay + tre difetti)

- **CRASH del "Salva gameplay" (grave, ADR-043)**: chiamavo `.items()` su un json **temporaneo**
  (`gameplayBalanceToJson(m_gameplay).items()`) dentro un range-for. È il footgun noto di nlohmann: il
  temporaneo muore prima del loop, il proxy resta appeso → **use-after-free**. Corretto iterando su una
  **variabile** (`const json patch = ...; for (it = patch.begin()...)`). Difetto tutto mio, introdotto
  nel turno precedente.
- **Rename mappa non cambiava il nome in partita/sandbox**: il rename cambia il **filename/id** ma
  partita e sandbox mostrano `MapDef.name` (campo interno separato, fallback id). Ora il rename
  **sincronizza `name` col nuovo id** via RMW.
- **Griglia del MapEditor troppo piccola** per mappe grandi: `buildGrid(40 m)` → **120 m** (passo 2 m
  invariato). Copre mappe ben oltre firebase (50×40).
- **Mouse "sfasato" in fullscreen** (gioco): il toggle fullscreen ridimensiona la finestra a livello
  SDL e destabilizza la **modalità mouse relativa** → aim sballato. `toggleFullscreen` ora, se il mouse
  era catturato, **spegne e riaccende la modalità relativa** e svuota il delta accumulato. *(Fix di
  logica: non verificabile headless — richiede smoke manuale del toggle in partita.)*
- Build 0/0; `--validate` 0/0; il balance si ricarica pulito (rimosso anche un BOM residuo nel JSON
  sorgente dai miei test PowerShell).

## 2026-07-21 (46) — Bilanciamento globale autorabile: `gameplay.json` + tab Gameplay (ADR-043)

Inizio della fase authoring chiesta dall'utente ("rendere tutto il più autorabile possibile"), dal
caso guida dell'audit: per tarare la rianimazione bisognava **ricompilare**.
- **`data/config/gameplay.json`** + `mini::GameplayBalance` (header-only, condiviso runtime/editor via
  FILE — ADR-002): **10 parametri migrati** da `constexpr` — i 6 della rianimazione
  (`squad_bleedout_time`, `squad_revive_radius/time/hp`, `squad_down_lethal_hit_frac`,
  `squad_max_revives`) e i 4 del degrado comunicazioni (`comms_lost_*`). Default = vecchie costanti;
  file assente → comportamento invariato. Le costanti sono state **rimosse** da GameConfig (restano
  puntatori): una costante morta ma compilabile è una trappola.
- **Tab "Gameplay" nel BalanceEditor**: slider con spiegazioni, salva (RMW), ripristina default.
- **Verificato in modo deterministico**: `squad_max_revives = 0` nel JSON → log `max_revives=0` →
  **zero** `member downed/revived` in 110 s (chi cade muore). Ripristinato 1 → baseline identica
  (8/5/3, `revives_used` sempre 1). Build 0/0; `--validate` 0/0.
- **Trappola scoperta verificando** (ora in memoria): il runtime **preferisce la `data/` SORGENTE**
  (3 livelli su dall'exe); la copia in `build/.../Debug/data/` è solo fallback. Ho editato per tre run
  il file che l'exe non legge, diagnosticando prima un falso colpevole (BOM). Il loader ora è comunque
  BOM-tolerante e il log stampa **i valori caricati**, non solo "caricato" — così questo errore la
  prossima volta si vede subito.

## 2026-07-21 (45) — Comando nemico v2: il Droide Tattico gestisce più fronti (ADR-042, doc 32)

La "Direzione v2" del doc 32, su direttiva dell'utente: il Droide Tattico coerente con Star Wars —
imposta priorità e ordini **più alla volta**, gestendo più settori insieme, restando al livello delle
risorse e non del micro.
- **`enemyCommand` da intento singolo a LISTA di direttive.** Il comandante gestisce **più fronti**,
  ognuno con la sua stance dal bilancio **locale** del settore (tieni dove domini ma sei pressato,
  spingi altrove) — fine del "sempre avanzata" (era un termostato sul conteggio delle teste).
- **Concentra su 3 fronti** (i più preziosi: importanza × contesa + priorità strutture), non disperde.
  **I droidi si distribuiscono** sui fronti (scelta pesata dal bias, come la torre di controllo):
  la forza si divide invece di convergere tutta su un punto.
- **Ripiegamento globale** come unico override quando i droidi vanno sotto metà — l'unica decisione
  che resta giustamente globale. Fallback senza settori = post più vicino (comportamento v1).
- **Misurato** (10v10): 3 fronti gestiti insieme; a t=33 s **2 AVANZATA + 1 TIENI** contemporaneamente,
  obiettivo prioritario che cambia (Ally 1 → Enemy 2 → Charlie), **RIPIEGAMENTO** a t=73 s quando la
  forza cala. Nuova telemetria `cmd_fronti`/`cmd_avanzata`/`cmd_tieni`/`cmd_ripiega`. Build 0/0;
  `--validate` 0/0; `stuck` 7.
- **Superficie contenuta**: solo AiSystem + World. Il **grado intermedio** (che interpreterà le
  direttive per il micro del suo gruppo) resta il livello sotto, futuro.

## 2026-07-21 (44) — Droide Tattico: spawn dedicato con raggio di leash, autorabile (ADR-041 Fase 1)

Prima fase della ristrutturazione del comandante. Direttiva utente: *"deve stare in un luogo sicuro...
con la possibilità di muoversi solo in un piccolo raggio... rendere lo spawn un'area circolare dalla
quale non può uscire, così da poter autorare l'ampiezza del raggio"* + *"uno spawn dedicato da gestire
sul map editor"*.
- **`leash_radius` sul campo `commander`** (schema + loader): area circolare da cui non esce. `0` =
  fermo sul posto (comportamento `stationary` legacy → retrocompatibile).
- **Comportamento AI**: con un raggio > 0 il comandante non è più congelato — si muove per difendersi,
  ma (a) **non insegue obiettivi né segnali** (tiene la sua area) e (b) un **clamp universale** prima
  del movimento gli impedisce di uscire dal raggio, qualunque cosa voglia fare.
- **Editor completo**: marker viola + disco del raggio nel viewport; pannello con **classe dal
  registry** (dropdown, mai testo libero), posizione, slider del raggio; gizmo Sposta/Scala (scala =
  raggio); selezione dal viewport; save via RMW. **Chiude l'item audit "UI del commander nel
  MapEditor"** — prima si autorava solo a mano nel JSON.
- **Misurato** (leash 6 su firebase): la deriva del comandante dalla casa resta **0 → 5.5 → 6.0**, mai
  oltre il raggio autorato. Nuova metrica `cmd_deriva_m` nella telemetria. Build 0/0; `--validate` 0/0.
- **Non è ancora** la migrazione fuori da `class`, né la stance v2: quelle restano fasi successive di
  ADR-041 / doc 32.

## 2026-07-21 (43) — La torre di controllo non ammassa più (KI #73)

L'ultimo nodo di design aperto dell'audit, l'unico che avevo lasciato "da decidere". Scelta
l'opzione **saturazione** (fra le due che avevo proposto).
- **`Signal.crowd`**: ogni segnale conta le truppe team-1 già presenti; oltre `ALLY_SIGNAL_CAPACITY`
  (=3) **smette di attirarne**. I cloni si distribuiscono sui vari segnali; quando è tutto coperto,
  i restanti **tornano alla pattuglia** invece di pilarsi sul poco che resta.
- **Stabilità contro l'oscillazione**: chi è già dentro un segnale ci resta. Senza, un segnale saturo
  verrebbe abbandonato → si svuota → tutti tornano → pendolo. `pickAllySignal` ora prende anche la
  posizione del clone per questa regola.
- **Misurato**: `segnale_affollamento_max` **0-1** per tutta la partita (prima: tutti sullo stesso
  punto); a fine partita 7-8 cloni in pattuglia invece che ammassati. Il comportamento "ultimo
  bersaglio" (ramo separato) **non regredisce**: struttura finale ancora distrutta (t=103), poi
  ripattuglia. Build 0/0; `--validate` 0/0.

## 2026-07-21 (42) — Cap di rianimazioni: un uomo non si rialza all'infinito

Feedback ripetuto dell'utente ("la rianimazione è ancora troppo efficace... non muore mai nessuno"),
e — come sospettato nell'audit — la causa non era un numero ma una **regola mancante**: si poteva
rianimare lo stesso soldato **infinite volte**. Tempi più lunghi e HP più bassi (già fatti) non
possono risolvere un ciclo senza limite.
- **`SQUAD_MAX_REVIVES` (=1)**: una vita può essere rianimata al massimo N volte; esaurito il cap, la
  caduta successiva è **letale** (va al ramo morte invece che "a terra"). Si azzera col respawn (nuova
  entità → nuovo `SquadComponent`, `revivesUsed = 0`).
- **Additivo e sicuro**: `SquadComponent.revivesUsed` incrementato alla rianimazione; la condizione di
  caduta guadagna `revivesLeft`. Il giocatore è già escluso (ha il suo respawn).
- **Misurato** (sim 10v10): **tutte** le rianimazioni a `revives_used: 1` — nessun membro rialzato due
  volte; 8 a terra, 5 rianimati, 3 morti per bleedout. Prima lo stesso soldato poteva tornare in
  piedi indefinitamente. Build 0/0; `--validate` 0/0.
- **Authoring futuro**: candidato globale, o **per-classe** (un "medico" potrà rialzare più volte o
  alzare il cap dei compagni) — allineato alla decisione sulle case dei dati (doc 37 §E).

## 2026-07-21 (41) — Bersagli come priorità bassa, FocusFire su strutture, grafo consumato

Decisioni dell'utente sui punti aperti dell'audit, tradotte in comportamento.
- **Le strutture sono bersagli a PRIORITÀ PIÙ BASSA delle unità (doc 35)**: si scelgono solo quando
  non c'è un bersaglio-unità. Due modi di diventare eleggibili: entro `engage_radius` (peel-off
  proattivo), oppure — quando **non restano unità nemiche** — come **ultimo bersaglio**, entro
  l'aggro. **Misurato** su mappa di prova (torre 3000 HP, `engage_radius 0`, lontana): i cloni
  sterminano i droidi, poi **si avvicinano e la distruggono a t=97.8**, e da t=113 **tornano in
  patrol** — esattamente il ciclo descritto dall'utente, e con raggio 0, cioè puramente per la regola
  "ultimo bersaglio".
- **FocusFire funziona sulle strutture**: il giocatore può forzarne la priorità (e comandi più
  avanzati in futuro). Prima il LOS del designato non mirava al corpo né ignorava sé stesso → una
  struttura era designabile ma il colpo falliva sempre.
- **`hunt_timeout` migrato nei profili AI** (`AiProfileDef::huntTimeout`): è una scelta di **carattere**
  (un cecchino paziente insegue meno di un'unità aggressiva), non una costante globale. La costante
  resta solo come default documentato.
- **Il grafo "chi copre chi" (ADR-032) ora è CONSUMATO** — era il sistema mai usato dell'audit (B1).
  `bestOverwatchFor` delegava a `bestFiringPosition` senza toccare `positionCovers`: aggiunta
  `bestOverwatchForPosition`, che legge il grafo per trovare una posizione che copre il punto verso
  cui un compagno avanza. Cablato: chi non avanza in una valutazione si sposta a coprire chi avanza
  (bounding overwatch **esplicito**, accanto a quello emergente di ADR-035).
  **Risultato onesto**: il meccanismo funziona (`overwatch_avviati` > 0) ma su firebase scatta **di
  rado** — la maggior parte delle unità avanza invece di restare, e la copertura reciproca fra le
  posizioni autorate è sparsa. È un dato utile: se lo si vuole vedere di più servono posizioni
  autorate che si coprano meglio, o una quota maggiore di unità che restano.
- Build 0/0; `--validate` 0 errori / 1 warning; `stuck` 1-10 fra run (varianza di combattimento,
  **non** clusterizzato alle strutture).

## 2026-07-21 (40) — Chiusura autonoma dei punti dell'audit

Applicato tutto ciò che l'audit (doc 37) aveva trovato e che **non richiedeva una decisione di
design**.
- **`Hunt` ora scade (KI #68 chiuso)**: prima non aveva timeout mentre `Search` sì (15 s) — un'unità
  inseguiva un `lastKnown` inesistente **per centinaia di secondi**. Ora dopo `AI_HUNT_TIMEOUT`
  (20 s) degrada a **Search**, non direttamente a Patrol: ha senso guardarsi intorno prima di
  rinunciare. **Misurato**: `in_hunt` 3 → `in_search` 6 → `in_patrol` 7 nelle finestre successive;
  prima restava inchiodato a `in_hunt` 1 per tutta la sim.
- **Gate di validazione esteso alle strutture** (buchi C1-C3 dell'audit). Il loader **non normalizza
  più il `role` in silenzio**: conserva il valore grezzo e il gate segnala i refusi come **errore**
  (prima `"comm"` diventava `"generic"` e l'autore credeva di avere una torre). Aggiunti: `hp <= 0`,
  `engage_radius` troppo piccolo, torre di controllo di team 2 (inefficace), torri duplicate per
  fazione, asimmetria involontaria fra le fazioni.
  **Effetto immediato**: il gate ha subito segnalato le **3 strutture di firebase con
  `engage_radius: 1`** — cioè esattamente l'errore che aveva fatto perdere un test all'utente.
- **Editor più chiaro**: lo slider ora dice `Raggio ingaggio (m)`, formatta `%.0f m`, avvisa in
  arancione sotto i 3 m e cita la dimensione di firebase (50×40 m) come riferimento. La causa del
  test a vuoto era che il campo **non dichiarava l'unità di misura**.
- Build 0/0; `--validate` 0 errori / 3 warning (tutti reali e voluti); sim 150 s, `stuck` 1.

## 2026-07-21 (39) — Audit dei sistemi nuovi (doc 37)

Analisi richiesta dall'utente prima di aprire la fase di authoring. Risultato completo in
**37_AuditNuoviSistemi.md**.
- **Due bug corretti, stessa radice**: **i sistemi sopravvivono a `World::initialize()`, lo stato del
  World no.** `initialize()` non azzerava `comms` → una fazione che aveva perso la torre iniziava
  **già degradata la partita dopo**, senza causa visibile. E `AiSystem::m_contacts` portava i contatti
  della battaglia precedente nella successiva. Corretti entrambi.
- **Un sistema costruito e mai consumato**: il grafo "chi copre chi" (ADR-032) è calcolato a ogni
  load e letto solo da `bestOverwatchFor`, **che nessuno chiama**. È il "metadato decorativo" che il
  progetto stesso si era ripromesso di evitare. Va consumato o rimosso — non lasciato lì.
- **Tre buchi nel gate di validazione**: `role` non validato (un refuso diventa `"generic"` in
  silenzio), nessun controllo di coerenza fra le torri, nessun avviso su `engage_radius` inerte.
- **Correzione di documentazione (KI #68)**: avevo scritto "la partita non finisce". **Finisce**: a
  non fermarsi è la **simulazione sandbox**, che *deve* proseguire perché serve a osservare le AI.
  Avevo scambiato lo strumento di osservazione per un difetto. Resta valido solo `Hunt` senza timeout.
- **KI #73 nuovo**: con pochi segnali la torre di controllo **ammassa** i cloni invece di disperderli —
  limite del modello di ADR-040, non della sua implementazione.
- **Il problema strutturale**: **17 costanti di gameplay** introdotte fra ADR-035 e ADR-040, **zero**
  raggiungibili dall'editor. Per bilanciare la rianimazione bisogna ricompilare — la diagnosi
  dell'utente era esatta. Più il campo `MapDef.commander`, che non ha UI.

## 2026-07-21 (38) — Torre di controllo: segnala, non comanda (ADR-040, doc 36)

Terza e ultima direttiva utente. Il vincolo era esplicito: la torre *"al massimo può segnalare i vari
possibili obiettivi, ma non indirizzare i cloni in un punto specifico o dare direttamente ordini"*.
- **Due canali separati, mai fusi**: `enemyCommand` pubblica **UN** intento e tutti i droidi vi
  convergono; `allyIntel` pubblica una **LISTA** di segnali e **ogni clone sceglie da sé**. Non
  condividono struttura né codice — è la separazione a impedire che uno diventi l'altro per deriva.
- **La scelta è decorrelata dal `bias`**, non il segnale migliore. Far scegliere a tutti il massimo
  sarebbe sembrato "più intelligente" e avrebbe ricostruito un comando unico sotto un altro nome,
  annullando ADR-037. **La dispersione È la feature.**
- **Segnali da**: settori non saldamente tenuti (importanza × pressione) e strutture nemiche vive
  (`priority`, premio alla torre di comunicazione, dalla sorgente unica di doc 35).
- **Gate sull'indipendenza**: vale solo per un clone **senza ordini e senza route**. Un ordine del
  giocatore ha sempre la precedenza.
- **Autorata su firebase** una "Torre Controllo Repubblica" in posizione **segnaposto** — da
  spostare in editor (ora il gizmo funziona davvero).
- **Misurato**: torre `attiva`, **2-6 segnali** pubblicati e seguiti dai cloni in pattuglia; navmesh
  `input_tris` **300** = (22 box + 3 strutture) × 12; `stuck` 1. Build 0/0; `--validate` 0/0.
- **Nota onesta**: `segnali_seguiti` conta i **tick**, non le unità — dice che il ramo è esercitato,
  non quanti cloni lo seguono.

## 2026-07-21 (37) — Le strutture erano autorabili solo sulla carta (KI #71, #72)

L'utente segnala quattro cose in una volta: scala e rotazione non funzionano né dal menu gizmo né
dagli slider, le torri non compaiono in sandbox, e le AI ci passano ancora attraverso. **Tutte e
quattro confermate.** Avevo dichiarato "rotazione e scala autorabili con gizmo" in ADR-036 e nel
changelog (33) **senza verificarlo**.
- **Barra gizmo**: `gizmoModeBar(m_viewport, boxSel, boxSel)` abilitava Ruota/Scala **solo per i box**
  della geometria. I gestori dei delta per i bersagli **esistevano già** in `tick()`, non venivano mai
  raggiunti. Ora ogni tipo di selezione dichiara cosa sa fare.
- **Viewport editor**: i bersagli erano disegnati con `ry = 0` e lato **fisso 2.5**. Gli slider
  cambiavano il dato, il disegno non lo leggeva → "non funziona".
- **Runtime**: con il box di fallback `meshScale` era **ignorata**, quindi la scala non agiva neanche
  in gioco. Ora **moltiplica** la base 2.5 (default 1.0 → invariato).
- **Sandbox**: `SandboxMode` **non spawnava affatto** le strutture — il codice viveva solo in
  `ConquestMode`. Estratto in `structures::spawnAll`, usato da entrambi.
- **Navmesh (KI #72)**: il `ColliderComponent` di ADR-036 era **necessario ma non sufficiente** —
  governa giocatore e proiettili, ma **le AI camminano sul navmesh**, che `NavManager::build`
  costruiva **solo da `map.geometry`**. Ora le strutture sono ostacoli anche lì, con **gli stessi
  semiassi del collider** (derivazione unica in `StrategicTargetDef`), così collisione e navigazione
  non possono divergere. Misurato: `input_tris` **264 → 288** = (22 box + 2 strutture) × 12.
- **Verificato**: build 0/0; `--validate` 0/0; sandbox su firebase → **2 strutture spawnate**
  (prima 0); navmesh con i triangoli attesi; sim senza crash, `stuck` 2.
- **Lezione (KI #71)**: tre difetti su quattro erano "il dato cambia ma non si vede". Avevo
  controllato che il campo finisse nel JSON e chiamato la feature fatta. Il criterio giusto non è
  *"il dato è scritto"* ma *"si vede l'effetto"* — ed è esattamente lo smoke manuale che dichiaravo
  dovuto e non pretendevo prima di dire "fatto".

## 2026-07-20 (36) — Le strutture diventano un fatto tattico autorabile (ADR-039, doc 35)

Direttiva utente: costruire **il sistema di interazione** AI↔strutture **e il sistema per autorarlo**
— l'authoring dei valori lo farà lui. Con la nota che queste informazioni servono anche alla torre di
controllo e al Droide Tattico.
- **KI #70 non era una decisione di design mancante, erano TRE BUG in fila** — ognuno sufficiente da
  solo a rendere una struttura inattaccabile, e nascosti l'uno dietro l'altro:
  1. `hasLineOfSight` **non escludeva il bersaglio** → il collider di ADR-036 **bloccava la visuale
     verso il centro della struttura stessa**. Regressione mia, non vista quando l'ho introdotta.
  2. Si mirava all'**origine del transform**, che per una struttura sta **a terra** → il segmento
     raschiava il collider del pavimento.
  3. Il **LOS al tiro** aveva entrambi i difetti: corretta la sola selezione, la telemetria segnava
     **396 ingaggi per finestra e zero danni**.
- **Fix**: `hasLineOfSight(..., ignore)`; punto di mira sul **corpo** (`y + hy/2`) in selezione e tiro.
- **Authoring**: `priority` (0..1) e `engage_radius` su `StrategicTargetDef`, con slider e spiegazione
  in editor. **`engage_radius = 0` è il default e significa "mai ingaggiata di iniziativa"**: lo
  strumento è pronto e **inerte** finché non lo si autora — le mappe già bilanciate non cambiano.
- **Le strutture escono dalla lista bersagli-unità**: corretto il LOS sarebbero state ingaggiate per
  vicinanza come un soldato, scavalcando il raggio autorato. Rientrano solo dal percorso
  opportunistico, e **solo se l'unità non ha bersagli-unità** — una struttura non spara.
- **`World::strategicTargets` è la sorgente unica di intel** (posizione, fazione, ruolo, priorità,
  raggio): la leggono AI, comando nemico e la futura torre di controllo. Una sola lista.
- **Il comando considera le strutture** fra gli obiettivi, pesate da `priority` (premio alla torre di
  comunicazione). E **non le conta più come truppe** nel rapporto di forze — le gonfiava `nFoes` e
  falsava la stance (stesso difetto di KI #61 sull'HUD).
- **Misurato** su mappa di prova usa-e-getta (poi rimossa; firebase resta ai default): comandante che
  sceglie "Torre Comunicazioni Repubblica" come obiettivo; **46 ingaggi** per finestra al picco
  (erano 0 — impossibili); **torre distrutta dalle AI a t=41.5 s** con HP reali, e da lì
  `comms_droidi: "degradate"`. La catena struttura → ingaggio → distruzione → degrado della rete gira
  **end-to-end**. Build 0/0; `--validate` 0/0.

## 2026-07-20 (35) — Rete di comunicazione: la torre degrada, non spegne (ADR-038, doc 34)

Direttiva dell'utente: le torri **non devono bloccare i rinforzi**; senza torre informazioni, ordini
e rinforzi **rallentano** — compreso il raggio entro cui un'AI avverte i compagni di un contatto, e
il ritardo con cui l'avviso arriva. Nuovo sistema, con doc di scope (**34**) prima del codice.
- **Perché serviva un sistema e non una costante**: la comunicazione non era modellata affatto.
  `AI_CONTACT_SHARE_RADIUS` era uguale per tutti e per sempre, gli avvistamenti si propagavano
  **istantaneamente** (i contatti si ricostruivano da zero ogni tick) e la direttiva del comandante
  si ricalcolava **ogni tick**. Non esisteva alcuna grandezza su cui una torre potesse agire.
- **`role: "comms"`** sulla struttura strategica (whitelist nel loader, **combo** nell'editor mai
  testo libero) + **`World::comms[team]`** come mailbox: `hadTower`/`towerAlive` e quattro
  moltiplicatori. **Si degrada solo chi la torre l'aveva e l'ha persa** — chi non ne autora nessuna
  comunica normalmente, quindi nessuna mappa esistente cambia comportamento.
- **I contatti diventano persistenti e datati.** Un'unità adotta un contatto solo nella finestra
  `[ritardo, ritardo + 1 s]`: con torre viva è `[0, 1 s]`, cioè gli avvistamenti correnti — nominale
  invariato. Senza torre la finestra **si sposta** invece di allargarsi: non si sa di più, si sa più
  tardi, e la posizione è quella di allora → **si accorre dove il nemico era**. Raggio dimezzato.
- **Il comando acquista una cadenza** (`COMMAND_DECISION_PERIOD` 3 s, ×2.5 senza torre): i droidi
  eseguono più a lungo un intento vecchio. La **morte** del comandante resta invece istantanea — è un
  fatto, non un ordine.
- **Rimpiazzi ×1.6**, mai bloccati. La conseguenza `block_enemy_reinforcements` resta nel framework
  obiettivi (la usa `hold_alpha`), ma **non è più il modello per le strutture**.
- **Costo trovato misurando**: i contatti persistenti sono esplosi a **1066 vivi** in 10v10 (ogni
  unità, ogni tick di sensing, un campione). Deduplica per area+recenza → **112** al picco, stesso
  comportamento.
- **Autorate su firebase**: la torre separatista ha ora `role: "comms"`, e ne esiste una
  **della Repubblica** (team 1) in posizione speculare — **da riposizionare in editor**, è un
  segnaposto scelto da me, non una decisione di layout.
- **Misurato sul ramo degradato**: in `--sim` normale le torri non vengono distrutte (KI #70), quindi
  verificato su una mappa di prova usa-e-getta con torri a 5 HP, poi rimossa. Torre di team 2 distrutta
  a t=29.8 s → `comms_droidi: "degradate"`, `comms_cloni: "ok"`: il degrado è **per fazione**.
  Rinforzi `6.4s` invece di `4.0s` (×1.6) e **continuano ad arrivare**. Build 0/0; `--validate` 0/0.

## 2026-07-20 (34) — Via il Follow fisso: le truppe sono indipendenti per default (ADR-037)

Direttiva dell'utente, e il nodo che la telemetria indicava da due sessioni.
- **`SquadSystem` imponeva `Follow` a chiunque non avesse un ordine.** Era un placeholder di Phase A,
  non una scelta di design. Da lì discendeva quasi tutto ciò che l'utente riportava — "si muovono
  tutti insieme", "sempre le stesse strade", "finiscono tutti aggregati": non difetti dell'AI, ma
  **l'ordine Follow che faceva il suo mestiere su tutta la squadra, tutto il tempo**. Rendeva anche i
  **cloni meno indipendenti dei droidi**, che non avendo squadra non hanno guinzaglio — l'opposto
  della differenza di fazione voluta.
- **Ora il default è nessun ordine**: `OrderType::None` → l'unità ricade su Patrol/Alert/Hunt normali
  e si muove come truppa indipendente. È lo stato **normale**, non un fallback.
- **`Follow` entra nella ruota di comando** come 4° settore (le 4 diagonali: Regroup, Hold, Advance,
  Segui). Se la squadra sta già seguendo il settore legge **LIBERI** e **revoca** — si torna sempre
  allo stato indipendente.
- **La revoca ha richiesto un ramo esplicito** in `SquadSystem`: il blocco di assegnazione filtra su
  `isImplemented()`, che `None` non soddisfa — senza il ramo la revoca sarebbe stata ignorata in
  silenzio.
- **L'HUD dichiara `LIBERI`** invece di lasciare la riga vuota: uno stato di design invisibile si
  legge come un bug.
- **Misurato (10v10, ~550 s)**: `sq_follow` **0 per tutta la partita** (era 4-9); a t=4 s
  `sq_senza_ordine` **10/10** e **21 unità in patrol insieme**. Al picco (t=64 s) `in_alert` 9,
  6 manovre valutate → **3 avviate**, `tiro_trovato` 3: i cloni manovrano davvero, non più solo i
  droidi. `stuck` ~4-5/minuto, invariato. Build 0/0, `--validate` 0/0.
- **Osservazione emersa dalla stessa run (non risolta, vedi KI #68)**: distrutta la torre
  (`strategic target destroyed`) i rinforzi nemici si bloccano come previsto, la fazione nemica viene
  spazzata via a ~130 s — ma **la partita non finisce**: 400 s di 4 alleati in patrol e 1 in `hunt`
  permanente su un bersaglio che non esiste più.

## 2026-07-20 (33) — Strutture solide, autorabili e per fazione + sim rappresentativa

**Lettura telemetria (priorità dell'utente)** e primi fix che ne discendono.
- **La simulazione non era rappresentativa**: `MatchSettings.team1AiCount = 1` di default, e `--sim`
  lo usava → si misurava un **1-contro-6**. Ogni misura sul comportamento degli ALLEATI (squadra,
  ordini, manovre) era presa su uno scenario inesistente in partita. Ora `--sim` prende le forze
  **dalla mappa**. È anche il motivo per cui non vedevo i problemi riportati dall'utente.
- **Bersagli strategici: mancava del tutto il `ColliderComponent`** — confermato nel codice: avevano
  Transform/Team/Health/MeshRenderer/Hitbox ma nessuna collisione, quindi AI e giocatore ci passavano
  attraverso (segnalato dall'utente). Aggiunto, con semiassi autorabili (0 = ricavati dalla scala) e
  altezza piena per compensare l'offset di grounding della mesh.
- **Il team era CABLATO a 2**: una torre dei CLONI sarebbe nata comunque nemica. Ora `team` è
  autorato (Repubblica/Separatisti) — prerequisito per le strutture di entrambe le fazioni.
- **Rotazione e scala autorabili** (`ry`, `mesh_scale`). ⚠️ **AFFERMAZIONE FALSA, corretta il
  2026-07-21 (KI #71)**: il gizmo ruota/scala **non** era abilitato sui bersagli, il viewport li
  disegnava a rotazione 0 e scala fissa, e il runtime ignorava `mesh_scale` col box di fallback.
  Solo i campi JSON erano stati aggiunti. Vedi changelog (37).
- **Fix dati**: lo sniper (`marksman`) non compariva in partita perché assente da `ally_types`.
- **Misurato dopo (10v10, forze reali)**: cloni finalmente vivi e presenti (`follow` 9→4 invece di 0);
  manovre avviate **13/12/10** per finestra (erano 3-6); il comandante **cambia obiettivo nel tempo**
  (Ally 1 → Bravo → Enemy 1), leggibile grazie ai settori nominati dall'utente. 0 errori, stuck 2.
- **Resta il nodo principale**: `follow` 4-9 — **i cloni sono tutti agganciati al Follow fisso**, che è
  esattamente ciò che va rimosso (prossimo passo: stato senza ordini + Follow nella ruota).

## 2026-07-20 (32) — Osservabilità delle decisioni AI, e il bug che annullava tutto

L'utente riporta nessun cambiamento visibile e dice, giustamente, di **non avere modo di verificare
se le AI usino i metadata**. Valeva anche per me: fin qui si misuravano crash, stuck e cambi di stato
— mai le **decisioni**. Prima di aggiungere altro si è costruita l'osservabilità, poi si è
diagnosticato **con i dati**.
- **Nuovo evento `AI / tactical decisions`** (periodico): censimento stati (patrol/alert/hunt/search/
  fermi/in_manovra), approcci scelti, manovre valutate/avviate/bloccate, e **hit vs miss delle query**
  tattiche — che distingue "il mondo non offre nulla" da "l'AI non chiede". È lo strumento che
  mancava a entrambi (KI #65).
- **Cosa hanno detto i dati**: i metadata **funzionano** (`tiro_assente` sempre 0, `tiro_trovato` 2-4
  per finestra; manovre avviate 3-4). Ma `hunt` e `search` erano **SEMPRE 0** con `alert` 6-7 su 9:
  tutti in contatto permanente nello stesso punto. Le manovre erano una perturbazione su una
  situazione già degenerata.
- **Il bug vero (KI #64)**: il **guinzaglio di squadra gira DOPO il blocco Alert e sovrascrive il
  movimento**. Un clone che avviava una manovra veniva riagganciato al leader al primo passo → la
  manovra non partiva mai, e l'unità oscillava avanti-indietro girando su sé stessa. **I droidi non
  hanno squadra, quindi nessun guinzaglio**: è esattamente il motivo per cui l'utente li vedeva
  "funzionare un po' meglio dei cloni".
- **Fix**: (a) il guinzaglio non si applica durante una manovra attiva; (b) in Alert il raggio del
  Follow si allarga 8 → 15 m (un membro ingaggiato deve poter manovrare); (c)
  `AI_CONTACT_SHARE_RADIUS` 20 → **10 m** — su una mappa 50×40, 20 m copriva quasi tutto il campo e
  richiamava comunque l'intera forza.
- **Misurato dopo i fix**: `hunt` passa da **sempre 0** a **3 → 1** — le unità ora perdono davvero il
  contatto e devono cercare: il blocco permanente si rompe. 0 errori, stuck 3.
- **Fix dati**: lo **sniper** (classe `marksman`) non compariva in partita perché assente da
  `firebase.ally_types` — la sandbox spawna tutte le classi, la partita solo il roster della mappa.
  Aggiunto.

## 2026-07-20 (31) — FASE AI: manovra in combattimento (ADR-035)

Primo incremento della fase AI, dopo il completamento dei metadata. Colma il gap che rendeva
inutilizzato tutto il lavoro precedente **proprio quando serviva**.
- **Il difetto**: entrando in `Alert` l'AI azzerava l'approccio (`flankActive = false`) e da lì
  strafava sul posto, usando la copertura solo come nascondiglio. Tutti i metadata tattici erano
  consumati **solo prima del contatto** → due gruppi che si sparano da fermi ("finto e meccanico").
- **Ora l'AI ingaggiata valuta periodicamente se spostarsi** (timer sfasato dal `bias`, quindi non
  tutte insieme): **aggiramento** (`bestFlankingPosition`, peso ∝ `flank_chance`) oppure **posizione
  di tiro** (`bestFiringPosition`, peso ∝ `cover_preference`). Soglia minima di 3 m: spostarsi di due
  metri sembrerebbe solo indecisione.
- **Muoversi non smette di combattere**: durante la manovra si continua a mirare e sparare — si
  vincola il MOVIMENTO, mai il fuoco (stesso principio del guinzaglio-ordine e della direttiva del
  comandante). Il tragitto usa il **pathfinding**, non lo steering: deve aggirare gli ostacoli.
- **Bounding overwatch EMERGENTE**: cap al numero di unità della stessa squadra che manovrano insieme.
  Alcune si spostano, le altre restano a fare fuoco — l'effetto "ci copriamo a vicenda" **senza
  coordinamento esplicito**, coerente con "AI semplici in un mondo intelligente".
- Non si manovra in **ritirata**, sotto **`CoveringFire`** ("stand and deliver") o se `stationary`
  (il comandante non lascia la sua posizione).
- **Verificato**: build 0/0, `--validate` 0/0, `--sim` 25 s → **33 cambi di stato** (erano 11: AI
  molto più attiva), **`stuck` 1** (le manovre non creano ingorghi), 0 errori.

## 2026-07-20 (30) — Settori: il livello su cui ragiona il comandante (ADR-034) — METADATA COMPLETI

Ultimo incremento del percorso metadata (doc 33 §5-bis). Ricollega tutto il lavoro all'obiettivo
iniziale: dare al **Droide Tattico** qualcosa su cui ragionare.
- **`SectorDef` autorato** (`label`, posizione, `radius`, `importance`): aree con significato tattico.
  Autorate a mano perché sono **scelte di design**, non dati derivabili — e sono poche.
- **`World::sectorStates` (runtime)**: presenze alleate/nemiche, chi controlla, **pressione** (quanto
  la zona è realmente contesa). Una sola passata per tick, contando solo le **truppe** (strutture,
  veicoli e comandante esclusi — stessa regola del fix al conteggio nemici).
- **Il comandante passa da un dato binario puntiforme a una lettura della situazione**: l'obiettivo
  non è più "il post non-separatista più vicino" ma il **settore di maggior valore** (importanza +
  contesa, saltando le zone già saldamente sue).
- **I droidi restano padroni del COME** (vincolo ADR-024 v2): in `Advance` ciascuno sceglie il post
  catturabile più vicino **a sé** *dentro il settore indicato*; se lì non ce n'è, il più vicino in
  assoluto. Il comandante dà la direzione, il droide sceglie il punto — niente ammassamenti.
- **Editor**: lista settori + pannello (nome, area, importanza) + disco viola nel viewport, con
  scala → raggio.
- **Additivo**: senza settori autorati il comportamento è identico a prima. **Firebase non ne ha
  ancora**: vanno autorati per vedere l'effetto.
- **Verificato**: build 0/0, `--validate` 0/0, `--sim` senza crash.

## 2026-07-20 (29) — Aggiramento: le "corsie" senza autorarle (ADR-033)

Chiude il pezzo "corsie di avvicinamento / aggirare restando coperti" **senza** aggiungere un tipo di
dato da disegnare a mano. Autorare corsie avrebbe significato centinaia di dati per mappa — ciò che si
era deciso di evitare. Dopo ADR-032 il grafo permette di **derivare** ciò che serve.
- **`positionExposure` (derivata)**: quanto ogni posizione è allo scoperto = frazione delle altre che
  possono batterla, ottenuta **invertendo** il grafo già costruito → costo nullo, mai stale.
- **`bestFlankingPosition`**: fra le posizioni che possono colpire il bersaglio, quella che lo attacca
  **da una direzione diversa** rispetto a dove è già ingaggiato **e** che è **meno esposta**. La corsia
  d'aggiramento espressa come **destinazione** invece che come tracciato.
- **Editor**: esposizione mostrata **in sola lettura** sulla posizione selezionata ("battuta da metà
  mappa" / "riparata"). Calcolata con la **stessa funzione del runtime** — la regola vive in un posto
  solo, l'editor non ne ha una copia.
- **Distinzione importante**: non è l'auto-gen fallita. Lì si *inventavano* posizioni dalla geometria;
  qui si *deriva una relazione da posizioni autorate a mano*.
- **Limite dichiarato**: l'esposizione è relativa alle posizioni autorate, quindi riflette la qualità
  della copertura di quella mappa — è un'euristica utile, non una verità fisica.
- **Verificato**: build 0/0, `--validate` 0/0 (638 link / 60 posizioni, 1.2 ms), `--sim` senza crash.

## 2026-07-20 (28) — Rete tattica: linea di tiro reale + grafo "chi copre chi" (ADR-032, M3+M4)

**M3 e M4 fatti insieme**, perché sono la stessa computazione: costruire un grafo geometrico e poi
rifarlo con la visibilità sarebbe stato costruirlo due volte (stessa logica che ha portato a unificare
prima in ADR-030).
- **`worldintel::hasLineOfFire`**: segmento contro i box `collider` della mappa (slab test nel frame
  locale, gestisce `ry`). Lavora su `MapDef` e non sul `World` → utilizzabile al load, dove il World
  non esiste. Era il mattone mancante.
- **Cade il limite dichiarato di ADR-031**: `bestFiringPosition` ora verifica la linea di tiro, quindi
  una posizione non "batte" più un bersaglio attraverso un muro. Tutte le scelte costruite sopra
  smettono di essere inquinate.
- **Grafo "chi copre chi"** (`MapDef.positionCovers`, `buildTacticalLinks`): per ogni posizione,
  quali posizioni copre (settore + gittata + linea di tiro). **Dato derivato**: ricalcolato a ogni
  load, mai autorato né salvato → non può diventare stale. Non è l'auto-gen fallita: qui si *deriva
  da dati autorati a mano*, non si inventa dalla geometria.
- **`bestOverwatchFor`**: posizione di tiro che copre il punto verso cui un compagno avanza — il dato
  che abiliterà il bounding overwatch nella fase AI.
- **Costo misurato** (l'ADR lo richiedeva): firebase **638 link su 60 posizioni in 2,4 ms**; outpost
  2 link in 0,01 ms. Trascurabile. Scala O(n²·box): su mappe molto più grandi il rimedio è una griglia
  spaziale, non un cambio di modello.
- `WorldIntel.cpp` aggiunto anche al target **GFEditor** (il loader è condiviso fra i due binari).
- **Verificato**: build 0/0, `--validate` 0/0, `--sim` senza crash.

## 2026-07-20 (27) — Settore di tiro: la copertura diventa posizione d'attacco (ADR-031, metadata M1)

Il cambio che risponde alla richiesta *"cover che non servono solo a nascondersi ma anche ad attaccare
e aggirare, sparando da luoghi più coperti e tattici"*.
- **`fireArcDeg` + `fireRange`** sulla posizione tattica: cosa quella posizione **batte** (arco
  centrato sul fronte + gittata utile). Default ampi (120° / 25 m) → le 60 posizioni già autorate
  restano valide senza ri-autorarle; clamp al load.
- **Nuova query `worldintel::bestFiringPosition`**: fra le posizioni raggiungibili, la migliore che può
  davvero colpire il bersaglio (`canShoot` + dentro gittata + dentro settore), premiando la protezione
  e penalizzando la distanza da percorrere.
- **Due domande diverse, due query** — è il punto concettuale: `bestCoverToward` = *"dove mi riparo
  dalla minaccia"* (difensiva, ciclo peek/hide); `bestFiringPosition` = *"da dove la colpisco restando
  coperto"* (offensiva). Prima esisteva solo la prima, ed è per questo che le coperture erano solo
  nascondigli.
- **AI**: nella scelta dell'approccio (ADR-029) l'opzione "copertura" diventa **posizione di tiro** —
  l'AI ci va **per colpire**, non per sparire.
- **Editor**: slider Ampiezza/Gittata + **settore disegnato** (due raggi gialli) sulla sola posizione
  selezionata — con 60 posizioni disegnarli tutti sarebbe illeggibile, e senza vederlo il settore non
  sarebbe autorabile con cura.
- **Limite dichiarato**: il settore è geometrico, non verifica ostacoli fra posizione e bersaglio →
  lo farà il precalcolo visibilità (M4). Resta il filtro economico di primo livello.
- **Verificato**: build 0/0, `--validate` 0/0 (60 posizioni), `--sim` senza crash.

## 2026-07-20 (26) — Una sola "posizione tattica": unificazione (ADR-030, metadata M2)

Primo incremento del **completamento metadata** (doc 33 §5-bis), su priorità dell'utente: si mette in
pausa l'intelligenza AI e si finisce il percorso metadata. Scelta dell'utente: **unificare prima**,
così il settore di tiro (M1) e la rete tattica (M3) si costruiscono una volta sola.
- **`TacticalPositionDef`** sostituisce `CoverPointDef` + `TacticalPointDef`: posizione, fronte,
  **ruolo** (cover/vantage/defensive/chokepoint/observation), altezza, protezione, canShoot,
  importanza, raggio. `MapDef.tacticalPositions` sostituisce i due vettori.
- **Il ruolo descrive, i campi abilitano**: le query filtrano per CAPACITÀ, non per ruolo — una
  copertura è una posizione con `protection > 0`, quindi una `vantage` che ripara vale anche come
  copertura senza casi speciali. Stessa logica in `NavManager` (marca COVER solo ciò che ripara).
- **Migrazione trasparente**: il loader legge `tactical_positions` **e** le legacy `cover_points` /
  `tactical_points` → le mappe esistenti funzionano senza toccarle. L'editor salva la chiave nuova e
  **cancella le legacy**: aprire+salvare migra il file. I JSON non sono stati riscritti a mano perché
  l'utente li sta autorando in questo momento.
- **Impatto tracciato prima di scrivere codice** (CLAUDE.md §1.4), 8 consumatori: Definitions, loader,
  `worldintel`, `AiSystem`, `NavManager`, `Application` (ordine TakeCover), `MapEditor`, dati.
- **Editor**: una sola lista "posizioni tattiche" con dropdown ruolo; il marker si adatta (lastra alta
  `height` se ripara, pilastro se no; disco del raggio solo per i ruoli d'area); pannello che mostra
  solo i campi sensati per il ruolo.
- **Verificato**: build 0/0, `--validate` 0/0 con **60 posizioni migrate** su firebase, 4 su outpost;
  `--sim` senza crash. Osservazione dalla telemetria: 4 stuck in Patrol nello stesso **varco stretto
  (~1.75 m)** fra "Cassa NO" e "Cover Centro O" — congestione di level design, candidato naturale a
  essere marcato come `chokepoint`.

## 2026-07-20 (25) — Approccio tattico all'ingaggio + personalità individuale (ADR-029)

Primo passo **costruttivo** verso AI più intelligenti: dopo aver tolto ciò che *impediva*
l'indipendenza (giro 24), ora le AI **usano davvero i metadata** per decidere *come* attaccare.
Vale per **entrambe le fazioni** — è lo stesso sistema, non due implementazioni (direttiva utente).
- **`AiComponent.bias`**: valore per-unità [0,1) dall'hash dell'entity id. È la **personalità**: rompe
  le parità, sfasa i tempi, sceglie lato/ampiezza degli aggiramenti e il posto in formazione. Senza,
  unità con lo stesso profilo prendevano **sempre la stessa decisione** → si muovevano come un corpo
  solo anche senza nemici (il difetto che rendeva le AI "meccaniche e finte").
- **Scelta dell'approccio** in `enterHunt`: il mondo offre le opzioni, il profilo le pesa —
  **diretto** (∝ `aggression`), **aggiramento** (∝ `flank_chance`, lato/ampiezza dal bias),
  **copertura che guarda il bersaglio** (`worldintel::bestCoverToward`, ∝ `cover_preference` ×
  protezione), **punto dominante** vicino al bersaglio (`nearestTacticalPoint("vantage")`, ∝
  importanza). Riusa il waypoint di approccio esistente (`flankActive`) → nessuno stato AI nuovo.
  I profili del BalanceEditor ora **contano davvero**: differenziarli cambia il modo di attaccare.
- **La squadra si DISPONE** attorno al leader (anello 3-5.5 m, angolo dal bias) invece di ammassarsi
  o restare immobile — quest'ultimo era un difetto introdotto dal fix del giro 24 (per togliere
  l'oscillazione le facevo stare ferme).
- **Verificato**: build 0/0, `--validate` 0/0, `--sim` 25 s senza crash, unità sparse su **tutti i
  quadranti**, `stuck` 3. Naturalezza e varietà = giudizio in partita.

## 2026-07-20 (24) — Fine del "blocco unico": contatti locali, obiettivi individuali

Feedback utente: *"ogni partita va sempre uguale, tutti si muovono in gruppi e vanno sempre sullo
stesso fronte, nonostante ci siano path per tutta la mappa"* + i cloni che a fine scontro si ammassano
e oscillano su 1-2 m. Causa comune: **le unità condividevano troppo stato**, quindi agivano come un
corpo solo. Tre correzioni.

1. **Shared awareness a livello di ESERCITO → contatti LOCALI.** Esisteva **una sola** `lastKnown` per
   team, propagata a **tutte** le unità: bastava che un droide vedesse un clone perché l'intero
   esercito passasse a Hunt su quel punto → due blocchi che si scontrano sempre sullo stesso fronte,
   partite identiche. Ora ogni avvistamento è un **contatto con la sua posizione** e ogni unità adotta
   solo il più vicino entro `AI_CONTACT_SHARE_RADIUS` (20 m): **chi combatte a ovest non risucchia chi
   presidia a est**. È il cambiamento che genera fronti indipendenti.
2. **Advance = obiettivo INDIVIDUALE.** In avanzata i droidi puntavano tutti l'unico focus del
   comandante → si ammassavano. Ora ogni droide sceglie il **command post catturabile più vicino a
   sé** (`nearestCapturablePost`): l'ordine resta "avanzate" (intento, ADR-024 v2), ma la forza si
   distribuisce su più obiettivi.
3. **I membri in `Follow` non pattugliano più.** Restavano nel raggio del guinzaglio e intanto
   cercavano di percorrere la loro route → il guinzaglio li richiamava: **avanti-indietro di 1-2 m**
   sul posto, tutti ammassati. Ora chi scorta il leader tiene la posizione (al resto pensa il leash).
4. **Punti di ricerca clampati ai confini mappa**: in Search il punto casuale (±12 m) poteva cadere
   oltre un muro perimetrale, agganciarsi al navmesh dove l'unità già era e lasciarla ferma contro il
   muro (osservato a z≈18.9 col muro a z=20).

- **Misurato** (`--sim` 25 s): unità ora **sparse su tutta la mappa** (est x≈17-19, ovest x≈-9, nord
  z≈-16, sud z≈18.9) invece di convergere su un fronte; eventi `stuck` 5 → **3**; 0 errori.
  Gli stuck residui sono reali e circoscritti: due droidi nel varco stretto (~1.7 m) fra "Cassa NO" e
  "Cover Centro O" (congestione crowd/level design), e si sbloccano da soli in 1.2 s.
- **Da valutare in partita**: se la battaglia è ora davvero più dinamica. Il raggio di condivisione
  (20 m) è la leva principale: più basso = più indipendenza/caos, più alto = più coordinamento.

## 2026-07-20 (23) — Perché i path non erano fluidi: quattro cause reali

Indagine sul feedback "non riescono a usare in maniera fluida i path". Non era una causa sola.
Metodo: partire dai **dati** (telemetria `stuck`: stato, posizione, durata) invece che da ipotesi.

1. **Falso "stuck" a velocità di pattuglia.** L'anti-stuck usava una soglia FISSA di 0.05 m/tick, ma
   il profilo AI impone `patrol_speed 2.5` → **0.042 m/tick a 60 Hz**: un droide che marciava
   normalmente veniva marcato bloccato dopo 1.2 s, facendo scattare `advancePatrol` → **saltava al
   segmento di route successivo senza esserci mai arrivato**. Ora la soglia è **proporzionale alla
   velocità** (bloccato = si muove < 1/4 di quanto dovrebbe).
2. **Sosta di 12 s a OGNI waypoint.** `patrolDwell = 12s` (pensata per catturare i post, che
   richiedono presenza >8 s) veniva applicata a **ogni** punto della route → le pattuglie stavano
   ferme quasi sempre. Ora la sosta lunga vale **solo sui command post**
   (`worldintel::nearCommandPost`, nuova query nel layer); sugli altri waypoint si prosegue subito.
3. **Destinazioni scartate in silenzio.** `NavManager::requestMoveTarget` agganciava il target al
   navmesh con le *query extents* del crowd (≈ raggio agente, ~0.8 m): un target dentro un muro o
   poco fuori mesh — **l'ultima posizione nota in Hunt, il punto casuale in Search** — non trovava
   poligoni e la richiesta veniva **scartata senza segnalazione**, lasciando l'agente fermo. Ora
   l'aggancio usa **estensioni crescenti** (2 → 6 → 14 m) e il confronto "stesso target" avviene sul
   punto **già agganciato** (prima un target dentro un muro sembrava sempre diverso → ripianificazione
   a ogni frame, moto a scatti).
4. **Il segnale `stuck` era inaffidabile.** In Alert il log era soppresso ma **il timer continuava ad
   accumulare**: stare fermi in copertura o a distanza d'ingaggio è legittimo, eppure maturava uno
   "stuck" riportato all'uscita dallo stato (gli eventi da ~2.8 s in Hunt). Ora in Alert il timer si
   **azzera**: il segnale significa davvero "non avanzo mentre cerco di spostarmi".

- **Verificato**: build 0/0, `--validate` 0/0, `--sim` 20 s → eventi `stuck` **da 9 a 0**, 22 cambi di
  stato, nessun crash. NB: la sim è deterministica ed entra in combattimento quasi subito, quindi
  **non è un buon strumento per misurare la pattuglia**: i punti 1-2 vanno valutati in partita.

## 2026-07-20 (22) — Il comandante dà INTENTO, non destinazioni + fix conteggio nemici

**Correzione architetturale (utente).** Il Droide Tattico non deve dire ai droidi *dove andare*:
deve **identificare obiettivi e dare ordini** (advance/hold/retreat); poi **i droidi scelgono** quali
percorsi usare, quali coperture, quando ingaggiare. Altrimenti è un **cervello unico che pensa al
posto delle singole AI** — l'opposto di "AI semplici in un mondo intelligente".
- `World::enemyCommand` ora porta uno **`stance`** (`Hold`/`Advance`/`Retreat`) + l'**obiettivo
  identificato**, non un waypoint. Il comandante **analizza la situazione** (euristica v1 sul rapporto
  di forze: molto in inferiorità → Retreat, in inferiorità → Hold, altrimenti → Advance) ed emette
  l'ordine nel feed ("Ordine del Droide Tattico: AVANZATA — obiettivo Alpha").
- Consumo: `Hold` → ognuno presidia/pattuglia (**è qui che le route autorate vivono**); `Advance` →
  la forza di manovra spinge sull'obiettivo, il presidio resta sulle route; `Retreat` → ripiegamento.
  **Percorso, coperture e ingaggio restano decisioni della singola AI.**
- **Fix conteggio nemici**: l'HUD segnava nemici vivi inesistenti (2 con solo comandante e torre in
  campo). Il conteggio sommava qualunque entità con team+vita → ci finivano **torre comunicazioni**
  (bersaglio strategico), **veicoli** e il **Droide Tattico**. Ora conta solo le **truppe**: strutture,
  veicoli e comandante esclusi. (Segnalato dall'utente, ipotesi corretta.)
- **Verificato**: build 0/0, `--validate` 0/0, `--sim` senza crash (25 cambi di stato).

## 2026-07-20 (21) — Rianimazione: da passiva a deliberata + le route vengono davvero percorse

Secondo giro sui due punti del giro (20): il feedback diceva che **non bastava**. In entrambi i casi
il problema non erano i numeri ma una **causa strutturale**.

**Rianimazione — la causa vera.** `reviverNearby` contava un compagno **QUALSIASI** entro 2.5 m: ma
il `Follow` tiene la squadra ammassata, quindi c'era sempre qualcuno vicino → la rianimazione era di
fatto **gratis e automatica**, e alzare i secondi non poteva risolverlo.
- Ora conta **solo chi si dedica davvero al soccorso**: il **giocatore-leader** (sceglie di fermarsi)
  o il compagno **dispacciato con l'ordine `Revive` su quel caduto** (smette di combattere). Un
  compagno che passa sparando non rianima più. Soccorrere **costa un uomo** — è la tensione voluta.
- Numeri più duri: canalizzazione 6s → **10s**; HP al risveglio 30% → **15%**; bleed-out 20s → **15s**
  (finestra più stretta: il soccorso va avviato subito); soglia colpo letale 0.35 → **0.20** degli HP
  max → **la maggior parte dei colpi uccide sul posto**, il "a terra" torna a essere l'eccezione.

**Route — la causa vera.** La direttiva del Droide Tattico (ADR-024) sovrascriveva la pattuglia per
**tutti** i droidi: con un comandante vivo **nessuna route veniva mai percorsa**.
- Ora le unità **con una route restano in pattuglia**; il comandante dirige la **forza di manovra**
  (le unità senza route). Coerente col mondo: un comandante assegna l'obiettivo a una parte delle
  forze e lascia le altre a presidiare — non manda tutti sullo stesso punto.
- Lo spawn divide la forza: **metà su route** autorate (presidio), metà senza (manovra). Senza questa
  divisione o si annullavano le route, o il comandante non dirigeva più nessuno.
- **Verificato**: build 0/0, `--validate` 0/0, `--sim` senza crash (25 cambi di stato). Il
  bilanciamento va valutato **in partita**: è tuning di sensazione, non verificabile headless.

## 2026-07-20 (20) — Pattuglie sulla route + bilanciamento rianimazione (ADR-028)

**Fase 3a (ADR-028): le pattuglie percorrono il tracciato autorato.** Prima ConquestMode appiattiva
tutte le `patrolRoutes` in segmenti e ne dava **uno** per unità (limite 2-waypoint di `AiComponent`):
su firebase 16 segmenti autorati producevano droidi che facevano avanti-indietro su un tratto, e la
sequenza del percorso non veniva mai seguita — dato autorato quasi decorativo.
- `AiComponent.patrolRoute/patrolSeg` (additivi, -1 = legacy). Nuovo helper `advancePatrol(ai, map)`:
  completato un segmento si passa al **successivo** (wrap) ricalcolando A/B dai punti autorati;
  sostituisce i tre `goingToB = !goingToB` (un solo punto di verità).
- Spawn: **una route per unità** (round-robin) con **segmento di partenza sfalsato** (le unità si
  distribuiscono lungo il tracciato). `RespawnEntry` porta route+segmento → **anche i respawn**
  riprendono la loro route. Le direttive del Droide Tattico (`enemyCommand`) mantengono la precedenza.
- Rimandati (doc 33): filtri navmesh per-ruolo (3b), grafo tattico + `purpose` delle route (3c,
  quando esisteranno i consumatori).

**Bilanciamento rianimazione (feedback utente): era troppo efficiente, non moriva mai nessuno.**
- `SQUAD_REVIVE_TIME` 3s → **6s** (rianimare costa tempo ed espone).
- `SQUAD_REVIVE_HP` 0.5 → **0.3** (chi si rialza è fragile, facile rimetterlo a terra).
- **Nuovo**: `SQUAD_DOWN_LETHAL_HIT_FRAC = 0.35` — finire gli HP non significa più *sempre* "a terra":
  un colpo che toglie ≥35% degli HP massimi **uccide sul posto** (niente finestra di rianimazione);
  i colpi leggeri mettono a terra. Effetto: armi pesanti e colpi alla testa (moltiplicatore hitbox)
  uccidono davvero, il fuoco leggero mette fuori combattimento. Resta una funzione **base**: la classe
  "medico" (futura) potrà accorciare il tempo / alzare gli HP di risveglio.
- **Verificato**: build 0/0, `--validate` 0/0, `--sim` senza crash (AI in pattuglia). Smoke manuale:
  osservare le pattuglie percorrere le route e la nuova durezza di down/rianimazione in partita.

## 2026-07-20 (19) — Tactical Points + pulsanti gizmo cliccabili (ADR-027, doc 33)

Fase 2 del piano metadata + fix ergonomia editor.
- **Tactical Points** (ADR-027): `TacticalPointDef {x,y,z,facing,type,importance,radius}` su
  `MapDef.tacticalPoints` (type: vantage/defensive/chokepoint/observation). Loader `tactical_points`;
  `worldintel::nearestTacticalPoint` (seam per i consumatori futuri). **Editor completo**: lista +/-,
  pannello con dropdown tipo + slider (importanza/raggio/fronte/posizione), marker colorati per tipo
  nel viewport (pilastro + naso del fronte + disco del raggio), gizmo sposta+ruota. Authoring manuale
  (no auto-gen). **Consumo AI = Fase 4/5** (dato autorato-ahead, come height/canShoot).
- **Pulsanti gizmo cliccabili** (Sposta/Ruota/Scala) nel viewport dell'editor: la selezione del tool
  non dipende più dalla scorciatoia da tastiera (che richiedeva viewport in hover + mouse libero,
  poco affidabile). Ruota/Scala si disabilitano (grigi) sui target che non li supportano → anche
  diagnostico. Risolve il "non riesco a selezionare ruota/scala" sui metadata (i cap erano corretti;
  il problema era l'accesso al tool). KI #60.
- **Verificato**: build 0/0, `--validate` 0/0 (firebase "0 tactical", additivo), `--sim` senza crash
  (AI viva). Smoke editor manuale: creare/tipizzare un tactical point, usare i pulsanti tool, salvare.

## 2026-07-20 (18) — Cover Intelligence: copertura come dato tattico + auto-gen (ADR-026, doc 33)

Fase 1 del piano metadata. La copertura smette di essere solo posizione/fronte/altezza.
- **`CoverPointDef` += `protection` (0..1) + `canShoot`** (additivi, default = comportamento vecchio;
  protezione clampata al load). Loader `protection`/`can_shoot`.
- **Scelta più intelligente**: `worldintel::nearestCoverToward` → **`bestCoverToward`** (scoring
  protezione − distanza). L'AI ora preferisce coperture *migliori*, non solo vicine. Con protezione
  uniforme degenera nella più vicina → retrocompatibile.
- **Editor**: slider "Protezione" + checkbox "Si può sparare da qui" nel pannello copertura.
- **Auto-gen coperture da geometria: aggiunta e poi RIMOSSA lo stesso giorno** (feedback utente).
  L'euristica "un cover per faccia di box" produceva coperture insensate; le mappe sono fortemente
  handcrafted → basso valore, e una versione buona richiede analisi tattica (LOS/minaccia/spaziatura),
  non euristiche sui box. Funzione + bottone eliminati. Generazione automatica di metadata dalla
  geometria **de-scoped** (doc 33 §6).
- **Rimandato**: idoneità per ruolo + link (Fase 2), consumo pieno `canShoot` e riduzione danno
  dietro copertura (più avanti); pose alle coperture bloccate (animazioni).
- **Verificato**: build 0/0, `--validate` 0/0, `--sim` senza crash (AI viva). Smoke editor manuale:
  generare coperture, regolare protezione, salvare.

## 2026-07-20 (17) — World Intelligence Layer: seam di query + Fase 0 metadata (ADR-025, doc 33)

Prima fase del piano metadata tattici (doc 33), filosofia "AI semplici in un mondo intelligente".
Fondamenta a basso rischio, **senza cambiare il comportamento AI**.
- **Query layer unico** `mini::worldintel` (`game/ai/WorldIntel.hpp/.cpp`): `nearestCoverToward` +
  `dangerAt`. È il seam dove AI/squadre interrogano la conoscenza della mappa (solo dati+query pure,
  decoupling doc 15). `AiSystem` ora lo chiama per la scelta copertura (rimossa la static `pickCover`,
  stessa logica) → base testabile/ottimizzabile per tutte le fasi successive.
- **Doppia verità danger risolta**: `applyDangerRepulsion` gated a **fallback** (solo `!navActive`);
  col crowd il costo DANGER del navmesh (doc 22) aggira già le zone. Una sola verità.
- **Editor: ruota/scala sui marker metadata** (KI #60 + richiesta utente): cover→ruota(`facing`),
  veicolo→ruota(`ry`), danger/post→scala(`radius`). Prima solo-sposta.
- **Doc-accuracy**: 15/18 aggiornati (l'AI consuma i metadata; il navmesh marca DANGER/COVER).
- **Verificato**: build 0/0, `--validate` 0/0, `--sim` senza crash (AI viva, navmesh ok). Smoke
  manuale editor: ruotare un cover, scalare una danger/post col gizmo.

## 2026-07-20 (16) — Droide Tattico: singolo obiettivo vivente nelle retrovie (ADR-024 v1, doc 32)

Su chiarimento dell'utente, il comandante diventa ciò che è davvero: **uno per mappa**, autorità
strategica che **sta nelle retrovie e si difende soltanto**, non una truppa. Risolve i due problemi
del playtest ("ce ne sono molti" + "avanza in prima linea").
- **Singleton per mappa**: nuovo campo `MapDef.commander { unit, x, z }` (`CommanderSpawnDef`) +
  loader (DefinitionRegistry) + gate (ContentValidation: risolve, porta l'ability `command`, e
  **avvisa** se un comandante finisce in `enemy_types`). ConquestMode ne spawna **uno solo** alla
  posizione autorata. Tolto `Tactical Droid` da `firebase.enemy_types`, aggiunto `firebase.commander`
  (retrovie, `z=-18`). **Non rispawna**: nuovo flag `RespawnEntry.respawns=false` → non entra in
  `m_trackedUnits` (come i bersagli strategici), così resta davvero uno per partita.
- **Retrovie / autodifesa**: spawna **stationary** → AiSystem non lo muove mai (ogni ramo di
  movimento è sotto `!ai->stationary`), ma fronteggia e spara a chi vede. Usa il profilo AI autorato
  dall'utente (`Tactical Droid`: aggression 0, cover 1.0). Leva-dati: `sight_range` per limitarlo
  alle minacce vicine.
- **Data-loss check**: il MapEditor salva via `saveJsonRMW` (ADR-010) → il nuovo campo `commander`
  **sopravvive** anche se l'editor non ha ancora UI per piazzarlo (authoring JSON a mano per ora).
- **Verificato**: build 0/0, `--validate` 0/0, `--sim` → **esattamente 1** `class=Tactical Droid`
  (prima erano molti), 0 crash. Smoke manuale: comandante fermo nelle retrovie + convergenza droidi
  + messaggio alla morte.
- Base dichiarata del futuro **sistema di gradi/ufficiali** (il Droide Tattico resta l'autorità
  strategica; gli ufficiali gestiranno le truppe) — [[command-rank-system]].

## 2026-07-20 (15) — Droide Tattico: COMANDANTE nemico, non aura (ADR-024 riscritto, doc 32)

Su chiarimento dell'utente, il Droide Tattico è ridefinito: **non un buff**, ma lo **stratega** dei
droidi — la **controparte** del comando del giocatore. Sostituisce la bozza-aura del giro (14), mai
committata. Base **v0** (semplice, espandibile); gradi/strati/entità-a-sé sono futuro (doc 32).
- **Concetto**: il comandante osserva la situazione (stato partita, metadata) e **dirige i droidi**;
  come io do ordini ai cloni, lui ne dà ai droidi. Nascosto/protetto (obiettivo di design), ucciderlo
  **rompe il coordinamento** — conseguenza come la torre comunicazioni.
- **Meccanica**: ability `type "command"` → `CommanderComponent` (marker). Nuova mailbox
  `World::enemyCommand` (controparte di `squadOrder`): se ≥1 comandante di team 2 è **vivo**, AiSystem
  calcola il **focus** = command post non-separatista più vicino al comandante e lo pubblica. I droidi
  in **pattuglia** convergono sul focus (movimento; il combattimento resta autonomo). Comandante morto
  → direttiva spenta, feed "i droidi perdono coordinamento", ritorno alla pattuglia.
- **Non** piega il `SquadSystem` (solo-giocatore) né inventa un sistema parallelo: riusa mailbox +
  pipeline abilità→componente. Il calcolo è nel precompute di AiSystem; da estrarre in uno
  `StrategicAiSystem` quando cresce (§5.3, doc 32 Out of Scope).
- **Refactor**: `AuraComponent`→`CommanderComponent` (rename pulito di storage/accessor/IMPL);
  `command_aura`→`command` (Definitions, BalanceEditor, ConquestMode); ability `Tactical Aura`
  eliminata, creata `Tactical Command`. La classe `Tactical Droid` resta (corpo B1, tinta dorata,
  hp_mult 1.5), ora con ability `Tactical Command`.
- **Limiti v0 (ADR-024/doc 32)**: combatte ancora col profilo B1 invece di stare nelle retrovie
  (KI #58, dati); un solo tipo di direttiva; entità a sé e gradi = futuro.
- **Verificato**: build 0/0, `--validate` 0/0, `--sim` senza crash (`class=Tactical Droid` →
  `unit=B1 Battle Droid`). Smoke manuale: convergenza droidi sul focus + messaggio alla morte.

## 2026-07-19 (13) — Asse B (feel): flash di danno + cornice a vita bassa

Completa il bundle di feedback di combattimento (dopo l'indicatore direzionale, giro 11):
- **Flash rosso ai bordi** quando il giocatore è colpito (triggato da `addDamageIndicator`, sfuma
  in ~0.3s) → riscontro immediato di "sei stato colpito".
- **Cornice rossa costante a vita bassa** (< 28% HP, intensità crescente al calare degli HP) → stato
  di pericolo leggibile senza guardare la barra. Serve la **vulnerabilità** (GDD 3.1).
- 4 bande piene ai bordi in `Ui2D` (niente gradiente → ADR-003 intatto); intensità = max fra flash e
  vita-bassa. Solo in Playing.
- **Verificato**: build 0/0, `--validate` 0/0. Resa da smoke manuale (farsi colpire / scendere di HP).

## 2026-07-19 (12) — Fix post-ADR-023: sniper spawnabile, tinte per classe, orientamento sandbox

Tre cose emerse provando (feedback utente):
- **Clone Sniper (marksman) non compariva** in sandbox: la classe non aveva `base_entity` → non
  istanziabile. Aggiunto `base_entity: "Clone Trooper"` → ora spawna (arma DC-15X).
- **Classi indistinguibili (stesso colore)**: `ClassDef` guadagna `colorMult` (tinta che MOLTIPLICA il
  colore del corpo, default {1,1,1}); applicata in `classres::effectiveUnit` → vale sia in gioco sia
  in sandbox. Tinte iniziali: Heavy ambré, Sniper verdino, B1 Heavy rossastro. Editabile nel
  ClassEditor (ColorEdit3). `effectiveUnit` ora copia SEMPRE l'entità effettiva (così l'overlay
  abilità+tinta vale anche per un'entità-con-classe, non solo per una classe-come-unità).
- **Manichini sandbox orientati male** (clone e droidi guardavano la stessa direzione): erano statici a
  ry=0. Ora ogni team guarda verso il nemico (facing dai `dirZ`, convenzione `ry=atan2(dx,dz)` gradi
  come AiSystem). In gioco non serviva (l'AI li gira).
- **Verificato**: build 0/0, `--validate` 0/0. Visuali (sniper, tinte, orientamento) da smoke manuale.

## 2026-07-19 (11) — Consolidamento asse B (feel): indicatore di danno direzionale

Primo slice dell'asse **B (feel e feedback del combattimento)** del piano di consolidamento (doc 31),
scelto perché serve il criterio "feel" della milestone e la sensazione di **vulnerabilità** (GDD 3.1):
il gioco dava feedback sui colpi INFLITTI (hitmarker) ma non su quelli **subiti** — non sapevi da dove
ti sparavano.

- **CombatSystem**: quando un proiettile ferisce il GIOCATORE (`eid == world.playerEntity`), registra
  la direzione MONDO della **sorgente** (opposta al moto del proiettile) nella mailbox
  `combatFeedback` (`playerDamaged` + `hitDirX/Z`).
- **HUD**: `addDamageIndicator(worldDir)` accumula indicatori sfumanti (~1.2s, cap 8); il render
  proietta ogni direzione rispetto alla **direzione di vista** (`setViewDir`, ogni frame) → un blocco
  rosso attorno al mirino nella direzione della sorgente (davanti = alto, dietro = basso), che ruota
  con la camera. Tutto 2D in `Ui2D` (ADR-003 intatto).
- **Verificato**: build 0/0 (engine+editor), `--validate` 0/0, `--sim` nessun crash. Il **rendering**
  (farsi sparare e vedere l'indicatore) resta da smoke manuale — e verifica il lato sinistra/destra:
  se risultasse specchiato, è un cambio di segno di `side` (cross product) in Hud.cpp.

## 2026-07-19 (10) — Entità = corpo, Classe = professione istanziabile (ADR-023, con migrazione)

Raffinazione architetturale del class system chiesta dall'utente: un'**entità** è un **corpo** reale
(usata quando il modello è davvero diverso: B1/B2/Droideka); una **classe** è una **professione** sullo
stesso corpo (armi/abilità/gadget + moltiplicatori di stat). Heavy/Sniper/Medic = un corpo + classi,
non entità. Prima era l'entità a referenziare una classe → variante = seconda entità (duplicazione).

- **`ClassDef`** guadagna `baseEntityId` (il corpo) + `hpMult`/`speedMult`/`damageMult` (default 1.0).
- **Risoluzione**: `ConquestMode::effectiveUnit` mappa un id-roster (entità O classe con base_entity)
  sull'**entità effettiva** (corpo + `classId` sovrapposto) → classres/WeaponAttach/stat funzionano
  invariati; `resolveUnitArchetype` applica i moltiplicatori. I **roster** (`ally_types`/`enemy_types`)
  possono ora referenziare **classi** come tipo-unità.
- **Migrazione** (dati): `Heavy Clone Trooper`→classe `Heavy Trooper` (corpo `Clone Trooper`, arma Z-6,
  abilità Shield/Combat Roll, mult 1.0); `B1 Heavy Battle Droid`(entità)→classe omonima (corpo `B1
  Battle Droid`, ai `B1 Heavy Droid`, hp_mult 1.125 = ex hp 90/80); entità ridondanti **eliminate**;
  firebase/outpost roster resi espliciti (base + heavy come classi).
- **Gate (ADR-018)** esteso: un id-roster deve risolvere come entità o classe-con-corpo; `base_entity`
  deve esistere; moltiplicatori > 0. **ClassEditor**: dropdown corpo + slider moltiplicatori (RMW).
- **Verificato**: build 0/0 (engine+editor), `--validate` 0/0, `--sim`+telemetria → il Heavy B1
  risolve `unit=B1 Battle Droid` (corpo) + `class=B1 Heavy Battle Droid` → `ai=B1 Heavy Droid`,
  `weapon=E-5C`. Il rendering/gameplay resta da smoke manuale. La metà giocatore (doc 27) non toccata.
- **Sandbox — FATTO stesso giorno**: i manichini spawnano ora sia le ENTITÀ (corpi) sia le CLASSI
  istanziabili (con base_entity), instradate al team giusto dal corpo; `spawnDummy` risolve via
  `classres::effectiveUnit` (spostata in ClassResolve, condivisa con ConquestMode → una sola regola,
  e via lo spam di `getClass` su cerr). Verificato: `--sandbox` carica i modelli Z-6/E-5C → gli heavy
  (classi) compaiono. `effectiveUnit` sovrappone anche le abilità della classe sul corpo.

## 2026-07-19 (9) — Milestone formalizzata (doc 31) + zoom in mira per-arma

- **Piano di consolidamento formalizzato**: nuovo **31_ConsolidationMilestone.md** (punto da
  raggiungere = *Vertical Slice v1* con 6 criteri di accettazione; assi di consolidamento A–F; unico
  codice nuovo = Droide Tattico; progressione/meta rimandati). Puntatore "DIREZIONE ATTIVA" in cima a
  06_Todo. Deriva da studio completo di Vision/GDD/Bridge/CurrentState.
- **Zoom in mira per-arma** (`WeaponDef.adsFov`): prima l'ADS usava un FOV fisso (35°) per tutte le
  armi; ora è per-arma e autorabile. Catena: `WeaponDef.adsFov` (default 35 → invariato) → loader
  `ads_fov` + gate campi-fantasma → `Weapon.adsFov` via `weaponFromDef` → `PlayerController` usa
  `weapon().adsFov` in mira → WeaponEditor slider "Zoom in mira / FOV" (RMW, ADR-010). Basso = più
  zoom (es. sniper ~15). Build 0/0, `--validate` 0/0. Reso in mira da smoke manuale.

## 2026-07-19 (8) — Quattro piccoli fix (hitbox orfane, back button, preset)

Serie di rifiniture segnalate dall'utente:
- **Warning hitbox falsi**: `data/hitboxes/B1 Heavy Droid.json` e `Heavy Clone Trooper.json` erano
  profili **vuoti e orfani** (`{"zones":[]}`, nessuna unità li referenzia — gli Heavy riusano i
  profili `B1 Battle Droid`/`Clone Trooper` che HANNO le zone). Verificato che nulla li referenzia,
  **eliminati** → `--validate` ora **0 errori, 0 warning** (era 0/2).
- **Pulsante "Indietro" (mouse)**: aggiunto su ogni pagina di PreMatch e Opzioni — controparte di ESC
  (riusa `handleKey(ESCAPE)` in PreMatch / replica Controls→Root, Root→Back in Opzioni: nessuna logica
  duplicata).
- **Preset F5/F6/F7 ora cliccabili**: nel footer delle Regole tre bottoni "Salva/Carica/Gestisci
  preset" che chiamano `handleKey(F5/F6/F7)` (prima solo da tastiera, i tasti più scomodi).
- **Bug nome preset**: nella pagina "Salva preset" i tasti **W/S** navigavano gli slot, quindi
  digitandoli nel nome si spostava anche la selezione. Ora in text-entry navigano **solo le frecce**
  → le lettere scrivono e basta. (I tasti-lettera colpevoli erano W/S, non A/D.)
- **Editor**: rimosso il pulsante rosso "X Esci" della toolbar — era un workaround di inizio progetto
  (finestra tagliata, X nativa irraggiungibile), non più necessario. Restano X nativa + "Chiudi GFEditor".
- **Verificato**: build 0/0 (engine + editor), `--validate` 0/0. Il comportamento dei click/back da
  smoke manuale (GUI).

## 2026-07-19 (7) — Bug dei veicoli: AI non li attraversano più (KI #31) + non si bloccano (KI #29)

Due bug veicoli chiusi, entrambi contenuti (rischio zero per fanteria/proiettili).

- **KI #31 — AI attraversavano gli speeder**: il crowd (ADR-017) non conosce le entità dinamiche.
  **Fix**: nel write-back del `CrowdSystem`, ogni AI viene spinta fuori dall'OBB dei veicoli lungo
  l'asse di minima penetrazione (solo-veicolo; la geometria statica la gestisce il navmesh).
  Deterministico, niente jitter. Ora l'AI scivola lungo lo speeder invece di attraversarlo.
- **KI #29 — veicoli bloccati alle casse laterali**: la collisione trattava lo speeder in movimento
  come **AABB** (inviluppo della sagoma ruotata) → agli angoli si gonfiava e urtava "aria". **Fix**:
  `hasCollision`/`slideMove`/`slideMoveWithStepUp` hanno un parametro `queryYawRad` opzionale
  (**default 0 = AABB, path invariato** per fanteria/proiettili → rischio zero); con yaw ≠ 0 il test
  è **OBB-vs-OBB esatto** (`obbIntersectsRotatedCollider`, SAT 2D a 4 assi). VehicleDrive passa ora
  il box REALE + `yr`. A 0/90° il risultato è identico a prima.
- **Verificato**: build 0/0 (engine + editor), `--validate` 0 errori, `--sim` 5s senza crash. I due
  comportamenti (AI che scivola, veicolo che non si blocca) restano da **smoke manuale** (serve un
  veicolo guidato/AI vicino in partita). KI #31 e #29 → RISOLTI.

## 2026-07-19 (6) — R2 (down-payment): estratto lo state-dump da Application.cpp

Primo passo, a basso rischio, sul debito R2 (Application.cpp era 2132 righe). Estratta la lambda
`buildStateDump` di `run()` — dump JSON completo dello stato (ADR-013, usato su F12/fine-partita/crash)
— in `core/StateDump` (`statedump::build`, funzione pura read-only). In Application resta una lambda
sottile che fissa i parametri correnti, così i call-site restano `buildStateDump("...")`.

- **Estrazione fedele**: codice identico spostato → **comportamento invariato per costruzione** (nessun
  cambio di logica). Il dump è ora riusabile/testabile in isolamento.
- **Verificato**: build 0/0 (engine + editor), `--validate` 0 errori. Aggiunto `src/core/StateDump.cpp`
  a CMakeLists.
- **Resta R2**: le estrazioni più grosse (SandboxSession, VehicleDriver mount/dismount) restano, da
  fare a piccoli passi per tenere basso il rischio — il main loop è tutto lambda `[&]` intrecciate.

## 2026-07-19 (5) — Rifinitura: la mappa di respawn mostra il fronte (tutti i post)

Scelta "rifinitura e robustezza" dell'utente. Passo piccolo, isolato al rendering, a basso rischio:
la mappa di respawn mostrava solo i PROPRI punti di spawn (base + post alleati). Ora mostra **tutti i
command post colorati per proprietario** (alleato blu, nemico rosso, neutrale grigio) come contesto —
scegliendo dove rinascere si vede il fronte, non solo dove si può.

- `CommandPosts::allPosts()` (label/x/z/owner). `HUD::RespawnMap` porta la lista `posts`; il render li
  disegna come cerchietti colorati sotto i marker verdi selezionabili (non cliccabili: solo contesto).
- Nessun impatto altrove: additivo alla mappa di respawn già esistente (doc 30 Phase 1).
- **Verificato**: build 0/0, `--validate` 0 errori. Resa da smoke manuale (serve morire con 2+ punti).
- **Bonus (verifica sul vivo)**: confermato via `--sim`+telemetria che gli alleati usano il profilo AI
  `Clone Trooper` (class `trooper`), non più `B1 Battle Droid` → class system NPC vivo sui dati reali
  (GDD 12.3 soddisfatto). Roadmap N4 corretto (era datato: ClassEditor già esiste, classi assegnate).

## 2026-07-19 (4) — Slow-mo della ruota: ora rallenta anche il giocatore

Lo slow-mo della ruota di comando scalava solo la **simulazione a passo fisso** (AI, proiettili): il
giocatore era aggiornato FUORI da quel ciclo con il dt reale (`elapsed`), quindi si muoveva a velocità
piena mentre tutto il resto rallentava. L'utente vuole che rallenti **tutto, giocatore incluso**.

- **Fix**: introdotto `simElapsed = elapsed * timeScale` (calcolato una volta accanto a `elapsed`) e
  usato per gli update del GIOCATORE — `updateMovement`, `weapon().update` (cadenza/calore),
  `vehicledrive::update`. La simulazione già usava `timeScale` (invariata). UI/camera/selezione della
  ruota restano a `elapsed` (velocità reale). I proiettili del giocatore, una volta creati, sono entità
  della sim → già rallentati da `world.tick`.
- **Verificato**: build 0/0, `--validate` 0 errori. **Da smoke manuale**: aprire la ruota e muoversi —
  il giocatore ora rallenta insieme al mondo.

## 2026-07-19 (3) — Editor: selezione oggetti dalle viewport col mouse (ray-picking)

Seconda parte dello step "mouse ovunque" (la prima erano i menu engine): nell'editor si potevano
selezionare gli oggetti solo dalla lista. Ora si **clicca l'oggetto nel viewport 3D** — decisivo su
mappe grandi, dove cercare per nome nella lista è una perdita di tempo (motivazione dell'utente).

- **`FreeCameraViewport`**: aggiunto ray-picking dei map box. `MapBoxDraw` porta un `pickId` **opaco**
  (il chiamante ci mette il proprio codice di selezione); su click si lancia un raggio dal pixel
  (unproject con VP inversa) e si testa **ray-OBB** contro ogni box (spazio locale, gestisce la
  rotazione Y), prendendo il più vicino. `popClickedMapBox(outId)` restituisce il pickId colpito.
  Il picking marker/bone esistente (a distanza-schermo) resta e ha la precedenza (punti specifici).
- **Priorità col gizmo**: `drawGizmoOverlay` ora gira PRIMA di `handleViewportClick`; se il click
  afferra un asse del gizmo (`m_gizmoActiveAxis >= 0`) la selezione viene saltata → trascinare il
  gizmo non riseleziona un oggetto dietro.
- **`MapEditor`**: ogni `MapBoxDraw` riceve il `pickId` = codice `m_selBox` (geometria = i; spawn
  −2/−3; post −10−i; cover −100−i; danger −200−i; route −300−i; veicoli −400−i; target −500−i).
  Dopo `m_viewport.draw()` fa `popClickedMapBox` → imposta `m_selBox` e rinfresca (come un click nella
  lista): lista e viewport restano in sync, incluso il gizmo.
- **Verificato**: build 0/0 (engine + editor), `--validate` 0 errori. **Da smoke manuale** (GUI):
  in MapEditor cliccare box/post/target/ecc. nel viewport per selezionarli; verificare che il click
  su un asse del gizmo NON cambi la selezione. Gli altri editor con viewport (Entity/Weapon/Vehicle)
  già selezionano marker/bone col click.

## 2026-07-19 (2) — Fix del mouse nei menu: Launcher + slider che diminuiscono

Due difetti segnalati dopo il giro (1):
- **Menu iniziale (Launcher) dimenticato**: aggiunto `LauncherScreen::handleMouse` (click su AVVIA)
  + wiring in Application.
- **Slider/valori solo in aumento**: nelle Regole (e Loadout/Sandbox) il valore, le frecce `<`/`>` e
  la barra stanno TUTTI a destra del centro riga, ma lo split −/+ era al centro riga → cliccando lì
  (dove sono i controlli) si otteneva sempre `+`. **Fix**: split rispetto al VALORE, non alla riga —
  `<`/sinistra-del-valore = −, `>`/destra = +; sulla barra delle Regole le due metà della barra
  diminuiscono/aumentano. Ora `<` (e la metà sinistra della barra) diminuiscono davvero.
- **Verificato**: build 0/0, `--validate` 0 errori. Da smoke manuale: nelle Regole cliccare `<` e la
  metà sinistra della barra per DIMINUIRE, `>` e metà destra per aumentare.

## 2026-07-19 (1) — Mouse in TUTTI i menu dell'engine (prima solo nel menu principale)

Step intermedio richiesto: il mouse funzionava solo nel menu principale; ora seleziona ovunque
nell'engine (nell'editor c'è già ImGui). Prima parte di due (l'altra: selezione oggetti dalle
viewport dell'editor, step successivo).

- **`handleMouse(mx,my,clicked)` aggiunto** a `PreMatchMenu` (Root/Loadout/Regole + pagine preset),
  `OptionsMenu` (categorie + controlli), `SandboxMenu` (armi + simulazione). Overlay **Pausa** e
  **Fine partita** (Win/Lose): i vecchi suggerimenti-tastiera diventano **bottoni cliccabili**
  (`HUD::overlayPick` + `setMousePos` per l'hover).
- **Modello di interazione** (scelto dall'utente): hover evidenzia la riga; sulle righe a valore il
  click regola — **metà sinistra = −, metà destra = +** (come ←/→); sui bottoni/enum il click attiva
  o cicla. Le geometrie degli hit-test **rispecchiano esattamente** i layout dei `render*` (stessi
  startY/rowH/larghezze) per non divergere.
- **Niente duplicazione di logica**: gli esiti dei menu (`applyPreMatchResult`/`applySandboxResult`)
  sono estratti in lambda condivise fra tastiera e mouse — un solo punto per ogni azione.
- **Menu sandbox**: la cattura del mouse viene rilasciata mentre è aperto (per cliccare) e ripresa
  alla chiusura, sincronizzato per coprire tutte le vie di chiusura (`sbMouseFreed`).
- **Verificato**: build 0/0, `--validate` 0 errori. **Comportamento dei click da smoke manuale**
  (le GUI non sono testabili headless): navigare ogni menu solo col mouse, regolare i valori
  (HP/ticket/conteggi) coi lati −/+, e i bottoni di Pausa/Fine partita.

## 2026-07-18 (8) — De-clip dello spawn ora risolve anche in verticale (lastre rialzate)

Playtest della mappa di respawn: schierandosi ad Alpha si nasceva ancora incastrati nella lastra su
cui poggia il post — troppo larga perché la ricerca **solo orizzontale** di `nudgeOutOfColliders`
trovasse un punto libero entro il raggio. Miglioramento **generale del sistema** (come chiesto
dall'utente), non un fix per Alpha/firebase:

- `physics::nudgeOutOfColliders` diventa una ricerca **3D a livelli di quota crescenti**: livello 0 =
  la ricerca orizzontale a terra di prima (muri → ci si sposta di lato verso il terreno libero); se
  TUTTO il livello 0 è bloccato (geometria rialzata più larga del raggio) sale di un livello e
  riprova, posizionandosi **SOPRA** la piattaforma. Prima l'orizzontale, poi la risalita → non si
  "scala un muro" quando basta un passo di lato (nessuna regressione sul caso muro).
- Vale per **tutti** i chiamati (respawn giocatore + spawn/rinforzi AI in ConquestMode/SandboxMode):
  è il sistema condiviso a migliorare, non il singolo call-site.
- **Verificato**: build 0/0, `--validate` 0 errori. Comportamento su lastra rialzata da **smoke
  manuale** (schierarsi ad Alpha e verificare di nascere sopra la lastra, non dentro). Aggiorna KI #57.

## 2026-07-18 (7) — Mappa top-down di selezione respawn (doc 30, Phase 1, stile BF2005)

Prossimo step dopo il controllo integrità (build 0/0, validate 0 errori, nessun residuo d'engine).
Evoluzione dell'overlay-lista del giro 5 nella forma voluta dall'utente: una **mappa dall'alto** con
i punti di respawn **cliccabili sulla mappa** (come Battlefront II 2005), non un elenco (BF2017).

- **Nuovo sistema** (doc 30 scritto prima del codice, CLAUDE.md §5). Tutto **2D nell'HUD** (`Ui2D`):
  nessuna telecamera 3D, **nessuna modifica alla pipeline OpenGL 3.3 Compat** (ADR-003).
- **Render** (`HUD::RespawnMap` + `setRespawnMap`): pannello mappa, pareti dai box `geometry` come
  rettangoli tenui (orientamento), marker dei punti disponibili (`availableSpawns`) alle posizioni
  proiettate, marker selezionato/hover evidenziato, marker "caduto" del luogo di morte, titolo +
  countdown/prompt. Bounds derivati dalla geometria (allargati a marker/morte) — nessun nuovo campo
  dati (come la navmesh, ADR-004).
- **Picking** (`HUD::respawnMapPick`): la proiezione mondo→schermo vive in UN posto (`rmProj`), usata
  sia dal render sia dal picking → non possono divergere. Mentre la mappa è aperta la cattura del
  mouse è rilasciata (cursore visibile); hover evidenzia, click seleziona e schiera.
- **Coerente col respawn del giro 6**: stesso `respawnSel`/`deployPlayerRespawn`/`nudgeOutOfColliders`;
  `A/D`/frecce + Invio restano come fallback tastiera; con un solo punto (nessun post) resta il respawn
  automatico (KI #56). L'overlay-lista provvisorio (`setRespawnSelect`) è stato **rimosso**.
- **Verificato**: build 0/0, `--validate` 0 errori. **Rendering/click da smoke manuale** (serve morire
  in partita con 2+ punti): la mappa mostra i post ai posti giusti, si clicca un punto e ci si schiera.
- Out of Scope (fasi future, doc 30): mappa tattica generale con **pausa**, post nemici/neutrali,
  ordini dalla mappa, texture terreno, zoom/pan.

## 2026-07-18 (6) — Tre fix sul respawn del giocatore (playtest della scelta del punto)

Emersi provando la scelta del punto di respawn del giro (5). Tre bug distinti, tutti sul rientro
del giocatore (KI #55/#56/#57).

- **HP del respawn (KI #55)**: gli HP impostati nelle regole partita valevano solo al primo spawn,
  poi tornavano a 100. `initWorld` risovrascriveva `currentSettings.playerHp` col `PlayerDef.hp`
  DOPO che il mode aveva già creato l'entità con lo slider → due fonti di verità. **Fix**:
  `PlayerDef.hp` **semina** lo slider una volta all'avvio; da lì lo slider è l'autorità unica per
  spawn iniziale E respawn (clobber in `initWorld` rimosso; le altre stat del personaggio restano).
- **Scelta con priorità (KI #56)**: con `respawnDelay` basso (0.5–1 s, voluto per far rientrare
  in fretta le AI) il giocatore veniva rigenerato **prima di poter scegliere**. **Fix**: con 2+ punti
  il timer è solo l'attesa minima e il rientro avviene alla **conferma** del giocatore (click / Invio /
  Spazio) via il nuovo `deployPlayerRespawn`; l'overlay mostra il countdown e poi "CLICK o INVIO per
  schierarti". Con **un solo punto** (nessun post) niente da scegliere → respawn automatico come prima.
  Le AI restano veloci, la scelta del giocatore non è più a tempo quando conta.
- **De-clip dallo spawn (KI #57)**: rientrando da un command post si finiva **dentro** la geometria
  del post, incastrati. **Fix generale**: la posizione di respawn passa da
  `physics::nudgeOutOfColliders` (helper già esistente, 8 direzioni a raggi crescenti) — vale per
  qualsiasi ostacolo/mappa, nessun fix ad hoc.
- **Verificato**: build 0/0, `--validate` 0 errori. Comportamento a terra/rientro da **smoke
  manuale** (serve morire in partita): (1) HP del respawn = quelli impostati; (2) con respawnDelay
  basso si può ancora scegliere e si rientra solo confermando; (3) rientro da un post senza restare
  incastrati nella geometria.

## 2026-07-18 (5) — Economia dei post: da ticket-bleed a *respawn-slow* + base della scelta del respawn

Direttiva dell'utente: invece di far **consumare ticket** ai post ("chi ha più post drena le riserve
avversarie"), ogni command post posseduto deve **rallentare il respawn** della squadra nemica di una
piccola percentuale. Cambia la natura del vantaggio: controllare la mappa non svuota le riserve
nemiche, le fa **rientrare più lentamente** — più leggibile e meno punitivo. In più, primo mattone
verso la **mappa tattica**: scegliere il punto di respawn prima di rientrare.

- **Task A — respawn-slow (Conquista).** `ConquestMode::checkDeaths`: il timer di un'unità in coda di
  respawn ora è `respawnDelay * (1 + POST_RESPAWN_SLOW * postiNemici)` — ogni post avversario aggiunge
  il 15% (`config::POST_RESPAWN_SLOW`). Il **ticket-bleed a tempo è rimosso** da Conquista:
  `updateObjectiveRules` è ora un gancio vuoto. **Assalto/Difesa (ObjectiveModes, ADR-014) NON sono
  toccate**: sovrascrivono `updateObjectiveRules` e usano `m_bleedTimer/m_bleedInterval` come proprio
  timer di vittoria — i due membri restano nella base con un commento che spiega perché.
- **Task B — base della scelta del punto di respawn.** Nuovo contratto `IGameMode::availableSpawns()`
  → `[{label,pos}]`; default = solo spawn base (indice 0 = `getSpawnPos()`). `ConquestMode` lo
  sovrascrive: spawn base + ogni command post **posseduto dagli alleati** (nuovo
  `CommandPosts::ownedByTeam(team)`). Mentre si è a terra, `Application` mostra un **overlay HUD**
  ("SEI A TERRA" + countdown + lista punti) e `A/D`/frecce scorrono la selezione; il rientro
  (`updateRespawn`) usa il punto scelto. Conquistare un post avanza così anche il **punto da cui si
  rientra**, non solo dove arrivano i rinforzi AI (unlock_spawn, giro 4).
- **Perché insieme**: sono lo stesso filo — il post come *vantaggio di ritmo/posizione* invece che
  come *sink di ticket*. La scelta del respawn è deliberatamente minimale (lista + countdown): è la
  **fondazione** su cui poggerà la mappa tattica top-down (visuale dall'alto, selezione sulla mappa),
  non la sua forma finale.
- **Verificato**: build 0/0, `--validate` 0 errori (2 warning pre-esistenti su hitbox senza zone,
  contenuto utente). **NON verificabile headless** (serve missione attiva + cattura post + morte del
  giocatore): da **smoke manuale** — (1) i respawn nemici rallentano man mano che si catturano post;
  (2) da morti, con un post alleato catturato, l'overlay elenca Base + quel post e si può rientrare lì.

## 2026-07-18 (4) — `unlock_spawn` ora funziona davvero (era una conseguenza a metà)

Prossimo step dalla roadmap, scelto trovando un **sistema isolato** (GDD 21.2): la conseguenza
`unlock_spawn` — "conquistare un posto di comando sblocca un nuovo punto di spawn" (visione
dell'utente / GDD 5.4) — **impostava** `battleState.allySpawnPost` ma **nessuno lo leggeva**. Il
valore era morto: catturare un post non cambiava nulla.

- **Fix**: `ConquestMode::spawnUnit` ora, per i RINFORZI alleati (team 1), se `allySpawnPost` è
  impostato spawna al command post nominato invece che allo spawn di mappa (spread 8-vie per non
  impilarli). Vuoto = comportamento invariato (additivo). È il pezzo che rende la cattura di un post
  una **conquista tattica**, non una casella: i rinforzi arrivano al fronte, non dalle retrovie.
- Telemetria `reinforcement at unlocked spawn` (ADR-016) per osservabilità.
- **Verificato**: build 0/0, validate 0 errori. `firebase_alpha` già usa `capture_alpha` →
  `unlock_spawn: Alpha`, quindi è testabile in gioco. **NON verificabile headless** (serve missione
  attiva + cattura + morte + respawn; `--stress` non attiva missioni): loop completo da **smoke
  manuale** — cattura Alpha, fatti uccidere, verifica che i rinforzi arrivino ad Alpha.

Metodo: ennesima applicazione di "nessun sistema isolato" — una conseguenza che scrive un valore che
nessun sistema legge è codice morto travestito da feature. Ora `battleState` ha 4 conseguenze su 4
con un consumatore reale (block_enemy_reinforcements, enemy_accuracy, ally_reinforcements, unlock_spawn).

## 2026-07-18 (3) — Etichette tagliate nell'editor: fix su TUTTI i moduli

Richiesta dell'utente: il fix del testo tagliato (fatto in alcuni moduli) va applicato ovunque.

- **Causa**: i widget ImGui nativi (`DragFloat`/`SliderFloat`/`Combo`/`InputText`/`InputFloat`/
  `InputInt`) disegnano l'etichetta a DESTRA del campo; a piena larghezza (o in un pannello stretto)
  la label esce dal pannello e viene tagliata dal clip rect.
- **Fix**: helper "etichetta a SINISTRA + campo che riempie il resto" in `util/UiWidgets` — già
  esistevano `dragRow`/`sliderRow`; aggiunti `sliderRowLR`, `intRow`, `inputFloatRow`, `inputIntRow`,
  `comboRow`, `textRow`, `colorRow` (tutti condividono lo stesso helper `rowLabel` che stronca l'id
  `##`). L'etichetta è disegnata PER PRIMA → non può mai essere tagliata.
- **Convertiti**: BalanceEditor (~40), MissionEditor (~25), VehicleEditor (5), MapEditor,
  EntityEditor, WeaponEditor, ClassEditor. Restano native solo le **checkbox** (widget minuscolo,
  etichetta adiacente: non si taglia) e il Briefing multiline (etichetta spostata SOPRA il box).
- **Verificato**: build 0/0; editor si avvia e carica tutti i moduli senza crash. Il comportamento
  dei widget è invariato (i valori si modificano in place come prima); cambia solo il layout
  (etichetta a sinistra). **Smoke manuale**: scorrere i pannelli e confermare che nessuna etichetta
  sia più tagliata.

## 2026-07-18 (2) — Fix "cubo volante" dei bersagli + authoring MapEditor + ruota più lenta

Dal playtest dell'utente + prosecuzione dell'authoring.

- **Ruota di comando: rallentamento più forte** — `WHEEL_TIME_SCALE` 0.25 → **0.15** (~1/7).
- **KI #54 (bug playtest) — bersaglio = "cubo volante" con hitbox sfasata.** Il box di fallback
  aveva `meshOffsetY = 2.0` (fluttuava) e l'hitbox sintetico era un box gigante a +4 (non combaciava
  col visibile) → l'utente non riusciva a colpirlo, quindi non si distruggeva e non appariva il
  messaggio. Fix: hitbox sintetico = cubo unitario (offset 0, extents 0.5) che scala con l'entità e
  usa lo STESSO `meshOffsetY` del render → **visibile == colpibile**; grounding corretto (box 2.5 m
  con base al suolo). Il messaggio di distruzione ("BERSAGLIO DISTRUTTO" + telemetria) c'era già:
  non compariva perché il bersaglio non veniva mai davvero colpito.
- **KI #53 RISOLTO — authoring dei bersagli strategici nel MapEditor.** Lista "Bersagli strategici"
  + aggiungi/rimuovi + gizmo Sposta (range selezione -500) + proprietà (label, X/Z, HP) + box
  arancione nel viewport + load/save `strategic_targets[]` (RMW). Mirroring dei command post.
  Ora DestroyTarget è **finito** per la regola "editabile dall'editor" (10_ProjectMemory).
- **Verificato**: build 0/0; editor si avvia e carica firebase col bersaglio (no crash); validate
  0 errori. **Smoke manuale dovuto**: nel MapEditor aggiungere/spostare un bersaglio e salvare;
  in gioco (`firebase_sabotage`) sparare alla torre → ora dovrebbe cadere, comparire il messaggio,
  e i droidi sparare peggio (`enemy_accuracy`).

## 2026-07-18 (1) — Ruota di comando in slow-motion + obiettivo DestroyTarget (runtime)

- **Slow-motion sulla ruota di comando** (richiesta utente, stile Bannerlord): mentre la ruota è
  aperta il tempo di GIOCO rallenta (non pausa) a `WHEEL_TIME_SCALE = 0.25×`. Implementato scalando
  il tempo reale che alimenta l'accumulatore a timestep fisso: fisica/AI restano deterministiche,
  solo meno passi al secondo. Camera e selezione della ruota restano a velocità reale. (Le future
  mappa tattica ecc. metteranno in PAUSA piena; la ruota solo rallenta, per non perdere tempo utile.)
- **DestroyTarget (doc 25) — runtime completo.** Un **bersaglio strategico** è una struttura statica
  distruttibile piazzata sulla mappa; distruggerla completa un obiettivo e ne scatena la conseguenza
  (es. torre comunicazioni → nemici disorganizzati, `enemy_accuracy`). È il "bersaglio strategico da
  distruggere" del GDD / della visione dell'utente.
  - `StrategicTargetDef` + `MapDef.strategicTargets[]` + `ObjectiveDef.targetStructure` (label);
    loader + gate (la label deve esistere ed essere unica nella mappa della missione).
  - Spawn in `ConquestMode`: entità statica team 2 (colpibile da giocatore/alleati, niente AI),
    Health + hitbox **sintetico** `__strategic_target` (box ~3×4×3 — il fallback sferico da 0.7 m
    era troppo piccolo per una struttura).
  - Mailbox `World::strategicTargets` (entità→label): l'`ObjectiveSystem` collega una distruzione
    (`killedThisTick`) alla label senza conoscere il codice di gioco (doc 10). Telemetria: eventi
    `strategic target spawned` / `destroyed`.
  - Esempio autorato: bersaglio "Torre Comunicazioni" su firebase + obiettivo `destroy_comms_tower`
    (conseguenza `enemy_accuracy 0.6`) + missione `firebase_sabotage`.
  - **Verificato**: build 0/0; gate accetta il contenuto (0 errori); il bersaglio **spawna** (telemetria,
    `--stress`). **NON verificabile headless**: la distruzione (richiede fuoco DELIBERATO — il
    giocatore; gli alleati mirano ai nemici, non alla struttura) e quindi il completamento
    dell'obiettivo. Loop distruzione→obiettivo→conseguenza da **smoke manuale** (gioca
    `firebase_sabotage`, distruggi la torre, verifica che i droidi sparino peggio).
  - **Follow-up (KI #53)**: authoring dei bersagli nel **MapEditor** — oggi si aggiungono/spostano
    solo a mano nel JSON. Per la regola "non finito finché non si autora dall'editor" (10_ProjectMemory)
    DestroyTarget è runtime-completo ma **authoring-in-editor pendente**.

## 2026-07-17 (9) — CoveringFire come soppressione + review sistema ordini + fix nome binding mouse

- **CoveringFire ora SOPPRIME** (era = HoldPosition). Un membro in fuoco di copertura NON entra in
  fase evasiva (niente peek/hide): resta esposto e continua a sparare ("stand and deliver"), con
  ~30% di cadenza in più. Distinto da un semplice "tieni la posizione". (`AiSystem`, flag `covering`.)
- **KI #52 (bug playtest) — il binding mouse non mostrava il nome**: `renderControls` usava
  `getScancode`+`SDL_GetScancodeName`, che su un binding mouse/rotella dà UNKNOWN → riga vuota ("—").
  Ora usa `getKeyName` (descrive "Mouse Centrale", "Rotella su", "Mouse 4/5"...). Prompt del rebind
  aggiornato ("tasto / mouse / rotella").
- **Tasti laterali del mouse (X1/X2)**: già supportati — SDL2 li consegna come pulsanti 4/5, che il
  sistema binding gestisce (cattura, query, nome). **Nessuna libreria serve**: SDL copre nativamente
  tutti i pulsanti e la rotella.
- **Review del sistema ordini — bug silenziosi trovati e chiusi**:
  - messaggio "rianimazione non ancora implementata" era **stantio** (Revive ORA è implementato; solo
    Regroup resta non cablato) → corretto;
  - l'annuncio "Squadra (N): ordine" scattava anche per gli ordini **diretti** a un singolo compagno
    (che l'Application già annuncia col toast) → soppresso per `directedMember != 0`.
  - Verificati OK (non bug): applicazione ordini diretti, skip dei caduti, auto-soccorso che non
    ruba membri sotto ordine del giocatore, esclusioni del pre-check di raggiungibilità, leash di
    Revive (1.5 m < 2.5 m di rianimazione → il soccorritore arriva).
- **Verificato**: build 0/0; `--stress 8` senza regressioni (5 a terra / 3 rianimati / 19 ordini, no
  crash). CoveringFire è player-diretto → non esercitabile in `--sim`: **smoke manuale dovuto**.

## 2026-07-17 (8) — Ruota di comando + mirino verde sugli alleati + binding mouse/rotella

Tre richieste dell'utente dopo il playtest.

- **Ruota di comando (doc 26, livello 2)** — `CommandWheel` (tasto **B** di default, rimappabile).
  Tenuto premuto: la camera si CONGELA e il mouse sceglie il settore (**Regroup** basso-sx / **Hold**
  basso-dx / **Advance** in alto); al rilascio l'ordine va a tutta la squadra. Regroup = raduna sul
  leader (MoveTo player); Hold = ognuno tiene la PROPRIA posizione (HoldPosition per-membro);
  Advance = avanza 15 m nella direzione di mira. HUD radiale con settore evidenziato.
- **Mirino VERDE sugli alleati**: la raycast ora distingue nemico (rosso) e compagno (verde) →
  feedback immediato per i comandi Revive/CoveringFire. `HUD::setAimOnAlly`.
- **Sistema di binding esteso (input non-tastiera)** — l'utente voleva poter mettere azioni su
  rotella/pulsanti mouse; ha chiesto il SISTEMA, non le rimappature. Fatto:
  - `InputBinding{type, code}` con `Key / MouseButton / WheelUp / WheelDown`; `m_bindings` passa da
    scancode a binding. Query (`isDown`/`isPressed`) gestiscono tutti i tipi; tracking di
    rotella+pulsanti nel frame (reset in `update()`, riempito da `processEvent()`).
  - Cattura nelle opzioni: mentre si attende un nuovo binding, rotella e pulsanti del mouse valgono
    come input assegnabili (`OptionsMenu::assignAwaited`, `isAwaitingKey`).
  - Persistenza `{type, code}` per nome azione, **retrocompatibile** col formato vecchio (intero
    nudo = tasto). `getKeyName` descrive anche mouse/rotella ("Mouse Centrale", "Rotella su").
  - **Verificato**: build 0/0; gioco avviato con un `keybindings.json` di prova (Cambia arma→rotella,
    Ordine squadra→tasto centrale) caricato senza crash. La rimappatura vera dalla UI è **smoke
    manuale dell'utente** (li imposta lui).

**Smoke manuali dovuti** (tutto input/render-driven): ruota B→muovi mouse→rilascia; mirino verde su
un compagno; opzioni → rimappa un'azione su rotella/tasto mouse → riavvia → resta.

## 2026-07-17 (7) — Rifiniture Phase C dal playtest + due comandi diretti + keybinding persistenti

Feedback dell'utente dopo aver provato Phase C.

- **KI #50 (bug) — i caduti si muovevano** sotto ordine: l'agente crowd conservava il target e il
  CrowdSystem lo muoveva anche se l'AI era saltata. Ora `AiSystem` **ferma attivamente** l'agente
  (`requestMoveVelocity 0`) ogni tick per gli a-terra.
- **KI #51 (bug) — keybinding non persistenti**: `InputManager` non salvava le rimappature. Aggiunti
  `load`/`save` (per nome azione) in `<exe>/user_presets/keybindings.json`, come un preset.
- **Indicatore visivo del caduto**: manca una posa prone → **tint ROSSO** sul clone a terra (dice
  QUALE è a terra; l'HUD dice quanti/quanto). Riutilizzabile per un futuro HUD dei cloni.
- **Due comandi diretti (tasto G, dal contesto del mirino)** — estendono FocusFire/TakeCover:
  - mirando un **compagno a terra** → **Revive**: manda il membro vivo più vicino a soccorrerlo
    (comando esplicito, oltre all'auto-soccorso);
  - mirando un **compagno vivo** → **CoveringFire**: quel compagno tiene la posizione e fa fuoco di
    supporto.
  Serviti da `SquadOrderRequest.directedMember` (ordine a SINGOLO membro, non a tutta la squadra) e
  da un rilevamento mirino separato per gli alleati (`aimAlly`: prima la raycast saltava il team 1 e
  gli hp≤0, quindi i compagni — e i caduti — non erano mirabili).
- **Verificato**: build 0/0; `--stress 8` senza regressioni (Phase C: 6 a terra / 4 rianimati, 40s,
  no crash). I comandi e l'indicatore sono input/render-driven → **smoke manuale dovuto** (vedi
  sotto).

Nota: `SquadOrderRequest` ora distingue ordini di squadra (directedMember=0) e ordini a un singolo
compagno — la base per futuri comandi mirati senza toccare l'infrastruttura.

## 2026-07-17 (6) — Squad Phase C: stato "a terra" + rianimazione (le perdite pesano)

Il prossimo step dai doc (N1/doc 26): *"stato a terra + rianimazione — è ciò che dà peso alle
perdite... candidato numero uno per far passare l'is-it-fun"*. Implementato come increment additivo,
osservabile in `--sim`.

- **Un membro della squadra alleata non muore subito: va A TERRA** con bleed-out. Stato in
  `SquadComponent` (niente componente nuovo). `CombatSystem` intercetta il colpo letale su un
  alleato-squadra (giocatore escluso, nemici invariati) → a terra invece di distrutto. Un colpo su
  un già-a-terra lo finisce.
- **Rianimazione**: per prossimità (compagno/giocatore entro 2.5m per 3s → 50% HP) **e auto-soccorso**
  (il membro libero più vicino viene dispacciato con un ordine `Revive`, ora implementato: prima
  falliva "non ancora implementato"). La squadra si autoprotegge.
- **Bleed-out**: nessun soccorso in 20s → morte, e SOLO ALLORA conta come perdita (`missionStats`) —
  una perdita evitata non pesa sul giudizio (doc 25).
- **AiSystem**: unità a terra inerme. **HUD**: `[A TERRA n — Xs]`. **Costanti** in GameConfig
  (segnaposto).
- **Verificato in `--sim`** (`--stress 8`, telemetria): 9 a terra / 7 rianimati / ordini `Revive`
  auto-emessi (ciclo down→dispaccio→revive completo, es. bot 11 → #15). Bleed-out esercitato con
  costante a 1.5s → 2 morti. Tutti e tre gli esiti **visti scattare**, non dedotti.

Effetto: la squadra diventa una risorsa da proteggere, non comparse — il pilastro tattico del GDD.
Metodo: intercettazione additiva (cambia solo la morte dell'alleato-squadra), stato in un componente
esistente, verifica per esiti osservati in telemetria (non per lettura).

## 2026-07-17 (5) — La posa dell'arma in mano appartiene all'ARMA (KI #49 migrato)

Implementata la migrazione proposta nel giro 4: la posa/scala in mano si sposta dall'entità
all'`WeaponDef`, così è corretta per chiunque impugni l'arma (una classe che cambia arma non
produce più un modello a scala sbagliata).

- **Schema `WeaponDef`**: `hand_scale`/`hand_rot`/`hand_offset` (loader + noteUnknownKeys).
  `handScale<=0` = fallback al `weapon_display` legacy dell'entità (transizione additiva).
- **Risoluzione unica**: `WeaponAttach` (runtime) e `EntityEditor::updateWeaponTransform`
  (anteprima) usano la posa dell'arma se autorata, altrimenti il legacy. La MANO resta del
  personaggio. Stessa formula in entrambi → "editor == gioco".
- **Authoring**: Weapon Editor → "Posa in mano" (checkbox + scala/rot/offset). L'EntityEditor mostra
  la posa in sola lettura quando è dell'arma (niente campo-fantasma editabile, KI #25) e non la
  riscrive più sul weapon_display in quel caso.
- **Migrazione dati** (RMW-chirurgica, sed, solo righe aggiunte): DC-15A 0.4 · E-5 1.2 · Z-6 80 ·
  E-5C 0.0015. Il range nativo 53000× è la dimostrazione che la scala è dell'arma, non dell'entità.
  Verificato: JSON validi, visivamente identico, `--stress 4` risolve 8 unità senza crash, gate 0
  warning di posa.
- **Gate**: da "display.id ≠ arma effettiva" a "arma effettiva senza `hand_scale`" — la condizione
  ora azionabile.

Nota metodo: ennesima applicazione di "una domanda, una implementazione". La scala in mano era la
stessa cosa (dimensione nativa del mesh) espressa su ogni entità; ora è sull'oggetto che la
possiede davvero — l'arma. Stesso spirito di `classres`, `DataPath`, il rilascio-mouse di EditorApp.

## 2026-07-17 (4) — Refresh classi in EntityEditor + integrità scala arma in mano

Due segnalazioni dell'utente.

- **Refresh (bug netto).** "Ricarica lista" in EntityEditor chiamava solo `loadEntries()`, non
  `loadAvailableIds()`: le classi appena create/modificate in Moduli → Classi **non comparivano** nel
  dropdown, e l'anteprima dell'arma usava la registry vecchia. Ora ricarica liste + registry e
  **conserva l'unità selezionata** per id (loadEntries azzera m_sel). Fix diretto.
- **Scala arma in mano (KI #49).** Verifica richiesta dall'utente ("arma enorme nonostante la scala
  sistemata"). La formula di scala è **identica** tra anteprima editor e runtime — l'integrità
  "editor == gioco" regge. Il buco è che `weapon_display` (posa/scala) è sull'ENTITÀ e tarato su
  un'arma fissa, ma il modello impugnato è l'arma EFFETTIVA della classe (KI #43): quando divergono
  (es. display Z-6 scala 80, classe → DC-15A) l'arma esce 200× sbagliata. Aggiunto **warning nel
  gate** su `weapon_display.id ≠ arma effettiva`; sui dati dell'utente 0 mismatch (già riallineati).
  **Aperto (design)**: la posa in mano appartiene all'ARMA, non all'entità → proposta di migrazione
  su `WeaponDef`, da concordare prima di implementare (GDD 21.4).

Nota: grazie allo sblocco delle armi separatiste (giro 3, KI #47) l'utente ha creato le classi
`B1 Battle Droid`/`B1 Heavy Battle Droid` (E-5/E-5C) e `Heavy Trooper` (Z-6). Il gate le valida pulite.

## 2026-07-17 (3) — Tre problemi del viewport/editor segnalati dall'utente

- **KI #46 (HIGH) — mouse catturato col Tab restava bloccato** uscendo da un modulo con viewport.
  `SDL_SetRelativeMouseMode` è globale, ma solo il Tab nel `tick()` del viewport lo spegne — e il
  tick smette di girare quando il modulo non è attivo. `EditorApp` ora rilascia la cattura **al
  cambio modulo** (`m_prevActive`), dove si SA che il modulo cambia. Esc→Home diventa un'uscita
  d'emergenza dal mouse.
- **KI #47 — ClassEditor mostrava solo le armi repubblicane.** Non si potevano armare le classi dei
  nemici (B1). Rimosso il filtro `faction != Separatist`; ora tutte le armi, con la fazione in
  etichetta.
- **KI #48 — viewport rotto fino al riavvio**: `resizeFBO` non ricostruiva un FBO invalidato se la
  dimensione del pannello era stabile. Probabile causa del "problemi poi risolti chiudendo e
  riaprendo". Ora si auto-ripara al frame dopo. Non riproducibile a comando: fix per lettura del
  codice, da confermare sul campo.

**Effetto architetturale (KI #46).** Un'altra istanza dello schema di questa settimana: uno stato
globale (la cattura del mouse SDL) guidato da un toggle **per-modulo** che smette di girare. Come per
`classres` e `DataPath`, l'invariante va imposto in **un punto che vede tutto** — qui EditorApp, che
sa quando il modulo cambia — non sparso nei singoli tick che non sanno di essere stati abbandonati.

## 2026-07-17 (2) — Pulizia editor: e sotto la pulizia c'era un bug vero

Richiesta dell'utente: *"facciamo ordine e pulizia... togliere dall'Entity Editor tutto ciò che non
decide l'entità... rendere tutto più pulito, stabile e ordinato"*. L'audit ha trovato un guasto di
correttezza che la pulizia da sola non avrebbe toccato.

- **KI #43 (HIGH) — con una classe assegnata, l'unità impugnava un'arma e ne sparava un'altra.**
  La regola di ADR-022 viveva **solo** dentro `resolveUnitArchetype`; i bullet stats e il modello in
  mano leggevano ancora `enemy->primaryWeaponId()`. Nuova **`mini::classres`**: unica
  implementazione di "la classe vince", usata da runtime, render **e editor**.
  **Verificato sui dati reali**: `Heavy Clone Trooper` (Z-6, danno 15 → classe `trooper` → DC-15A,
  danno 20) ora emette `weapon: DC-15A, damage: 20.0`; **col bug rimesso apposta**:
  `damage: 15.0` con `weapon: DC-15A` — incoerenti. Il test è stato visto fallire.
- **KI #44 — Entity Editor: via i campi che il gioco ignorava.** Armi (lista bugiarda: conta solo
  `weapons[0]`), Abilità, AI Profile (li decide la classe); Velocità (la sovrascrive sempre il
  profilo AI) e Danno Scale (zero consumatori). Il combo "Arma primaria" del tab Visuale — che
  scriveva `weapons[0]` travestito da posa — è ora in sola lettura, risolto con `classres`.
  Nessun dato toccato: il modulo ha smesso di **rivendicare** campi che non edita, e RMW li preserva.
- **KI #45 — R8 chiuso, e non era teorico**: le 8 copie della risoluzione di `data/` **divergevano
  già** (4 col controllo forte su `data/weapons`, 4 con quello debole). Ora `editor/util/DataPath`,
  una volta sola, col controllo forte.
- **Telemetria**: nuovo evento `unit class resolved` (ADR-016). Serviva perché `std::cout` è
  bufferizzato e un run headless interrotto lo perde: la risoluzione della classe non era
  osservabile, e un valore che nessun log mostra è un valore che nessuno controlla.

**Effetto architetturale.** Tre guasti diversi, una sola forma: *la stessa domanda con più
implementazioni*. L'arma effettiva (runtime/render/editor), la radice di `data/` (8 moduli), il
loadout (Statistiche + Visuale). ADR-018 lo dice per le regole di validazione; vale identico per
ogni regola di risoluzione. Chi aggiunge un consumatore dell'arma di un'unità passa da `classres`.

## 2026-07-17 (1) — Il giocatore non sceglie più una classe; doc 14 riscritto su ADR-022

Chiusura del punto 4 di ADR-022, che era rimasto **in attesa di una decisione dell'utente**.
Decisione: **rimuovere** la riga "Classe" dal PreMatch.

- **Il fatto che ha sciolto il dubbio "rimuovere o rinominare Loadout?"**: le righe *Arma primaria*
  e *Arma secondaria* **erano già** nello stesso menu, e la riga "Classe" le **sovrascriveva in
  silenzio** (`Application.cpp`: `primaryId = cls->primaryWeaponId`). Non era solo un nome
  sbagliato: era la trappola "due posti decidono lo stesso dato, uno vince senza dirlo" — la stessa
  appena corretta nell'EntityEditor. Rinominarla "Loadout" l'avrebbe conservata cambiando etichetta;
  rimuoverla non toglie **nessuna** funzione.
- **Rimossi anche `setClassList`/`getSelectedClassId`/`ClassEntry` e i membri `m_class*`**: senza i
  metodi la regola è **strutturale**, non una convenzione da ricordare (stesso ragionamento della
  rimozione di `consumeTeam1Ticket()`, KI #39).
- **Trappola evitata**: lasciare `m_settings.classId = getSelectedClassId()` con la riga rimossa
  avrebbe fatto restituire "" a un indice ormai fisso a 0, **azzerando `--class`** a ogni passaggio
  dal menu. È **KI #36 in miniatura**. `classId` ora si preserva come `characterId` in
  `startFromPreMatch()`.
- **`--class` resta come override di TEST**, dichiarato tale nel codice e annunciato in telemetria
  (*"classe iniziale (override di test --class)"*). Non è una scelta offerta al giocatore.
- **Doc 14 riscritto su base ADR-022**. Prima: si dichiarava *"not yet implemented"* mentre la metà
  NPC era in produzione, e in **Out of Scope vietava** proprio ciò che ADR-022 ha poi deciso
  (accoppiare la classe al profilo AI). Ora dichiara lo stato **MISTO** — metà NPC = Current
  Implementation, metà giocatore = Planned — e documenta i debiti reali (`role` fantasma di secondo
  tipo, `abilityIds` inerte per il giocatore, KI #32).
- **Sblocca doc 27 (Progression)**, il cui criterio di accettazione #1 è *"14_ClassSystem
  implementato prima di iniziare"*: è la **metà NPC** a soddisfarlo. Finché doc 14 si dichiarava
  non implementato, la progressione era bloccata da una doc che mentiva sul proprio stato.

**Verificato sul binario** (non sul codice): `"(nessuna - loadout manuale)"` — stringa che esisteva
**solo** in `setClassList` — è **sparita** da `GFEngine.exe`; `"override di test --class"` è
presente; e come **controprova** `"(nessuna - partita libera)"` (riga Missione) è ancora lì, quindi
il controllo sa distinguere invece di passare a vuoto. Build 0 errori / 0 warning.

## 2026-07-16 (13) — Controllo integrità: il gate di validazione era cieco su 3 loader

Controllo di routine richiesto dall'utente (integrità, stabilità, coerenza fra sistemi, residui).
Build e `--validate` erano **puliti**, ed erano puliti **per il motivo sbagliato**.

- **KI #40 — `noteUnknownKeys()` mancava in `loadHitboxProfiles`/`loadMaps`/`loadVehicles`**
  (9 loader su 12 lo chiamavano). Il rilevatore di campi fantasma legge `reg.unknownKeys()`: sui
  file non coperti non aveva **nulla da leggere**. Risultato: `data/hitboxes/B1 Heavy Droid.json`
  aveva `"profile_id"` — violazione ADR-001 conclamata — e il gate diceva *0 warning*. Chiuso su
  tutti e tre; liste di chiavi **lette dal loader, non dedotte** (derivandole a grep avevo mancato
  `color` sui veicoli e `enemy_types`/`ally_types` sulle mappe: una chiave mancante dalla lista
  fa dire al gate di cancellare un campo che funziona). Limite dichiarato: sulle mappe solo il
  primo livello, le sotto-strutture restano scoperte.
- **KI #41 — profilo hitbox vuoto**: `zones: []` non crasha, cade sul fallback sferico di
  `testHit()` → niente zone, niente headshot, **in silenzio**. Il gate ora lo dice, e copre anche
  `half_extents <= 0` (Error: zona mai colpibile) e `damage_multiplier <= 0` (Warn).
- **KI #42 — `MapDef.navmeshPath` scritto e mai letto**: BalanceEditor lo salvava, nessun loader
  lo rileggeva, nessun sistema lo consumava. Contrario ad ADR-004 (navmesh generata a runtime da
  Recast). Rimosso da `Definitions.hpp`, `BalanceEditor.cpp` e `data/maps/firebase.json`.
  **L'ha trovato il gate appena riparato**, non un occhio umano.
- **Residuo di build**: i backup `.bak` di `saveJsonRMW` finivano copiati nelle `data/` di output
  (24 per binario). `cmake -E copy_directory` non sa escludere: aggiunto `cmake/strip_backups.cmake`
  a build time. Innocuo oggi (i loader filtrano `.json`), ma il giorno del packaging sarebbero
  stati backup di authoring spediti ai giocatori.

**Effetto architetturale**: ADR-018 dice *un solo gate per runtime, `--validate` ed editor*. Era
vero per le **regole** ma non per la **copertura**: tre loader non alimentavano il gate, e una
regola che non può fallire non è una garanzia, è una decorazione. Ogni nuovo loader deve chiamare
`noteUnknownKeys()` — senza, il gate mente per omissione.

## 2026-07-16 (12) — Audit organizzazione editor + Home aggiornata

## Organizzazione dell'editor e dei concetti (audit + proposta, 2026-07-16)

Segnalazione dell'utente: *"tra i profili AI in Balance Editor, l'Entity Editor e ora le Classi
rischiamo di fare un caos... l'Entity ci serve soprattutto per modelli e hitbox, anche se ora
gestisce anche quale arma viene usata, che per gli NPC abbiamo detto che la decide la classe. In
realtà avevo usato Entity Editor come una sorta di class editor temporaneo."*

### Audit: chi edita cosa (verificato sul codice)
| Dato | Modulo | Note |
| --- | --- | --- |
| Profili AI (`data/ai/`) | BalanceEditor → tab "AI" | i profili in sé |
| `ai_profile` **di un'unità** | EntityEditor → Statistiche | **sovrapposto** |
| `ai_profile` **di una classe** | ClassEditor | **sovrapposto** |
| Armi **dell'unità** | EntityEditor → Statistiche | **sovrapposto** |
| Armi **della classe** | ClassEditor | **sovrapposto** |
| Mesh / attach / hitbox | EntityEditor → Visuale, Hitbox | corretto |
| Abilità (definizioni) | BalanceEditor → tab "Abilita'" | |
| Personaggio (`PlayerDef`) | BalanceEditor → tab "Personaggio" | |
| Mappe (bilanciamento) | BalanceEditor → tab "Mappe" + MapEditor | **sovrapposto** |

### I tre problemi
1. **Arma e profilo AI si assegnano in due posti.** Da ADR-022 vince la **classe**: il campo
   sull'unità viene ignorato. Editarlo senza saperlo è KI #25 daccapo.
   → **Fatto ora**: l'EntityEditor mostra un **selettore di classe** e avvisa in giallo che, con una
   classe assegnata, arma/profilo/abilità **li decide la classe**. Nessun campo rimosso: prima si
   verifica il modello provando, poi si toglie ciò che avanza.
2. **`Clone Trooper` e `Heavy Clone Trooper` sono UNA entità e DUE classi.** L'utente l'ha detto:
   ha usato l'EntityEditor come class editor provvisorio. Nel modello ADR-022 l'entità è il
   **corpo** (mesh, hitbox, fazione), la classe è la **professione** (loadout, comportamento).
   Due unità che condividono il modello e differiscono per arma+profilo **sono** due classi.
   → **Migrazione proposta (NON fatta: è contenuto dell'utente)**:
   `allies/Clone Trooper` resta l'unica entità-corpo; nascono `classes/trooper` (già c'è) e
   `classes/heavy`; `Heavy Clone Trooper` si cancella **dopo** aver verificato che nessuna mappa lo
   referenzi in `ally_types[]`. Da fare col comando Rinomina/gate, non a mano.
   *Nota*: se le due unità hanno **mesh o hitbox diversi** allora sono davvero due entità e la
   migrazione non vale — va guardato prima.
3. **BalanceEditor è un contenitore misto**: profili AI + mappe + personaggio + abilità, quattro
   cose senza parentela. Non è urgente, ma la direzione naturale è che "Balance" resti **solo gli
   slider di bilanciamento** e i profili AI vadano nel futuro **AI Editor** (già previsto come card
   "presto" nella Home).

### Regola per non ricascarci
**Un dato si edita in UN posto solo.** Se due moduli lo mostrano, uno dei due deve dire chi vince —
o non mostrarlo. È lo stesso principio del gate condiviso (ADR-018: una sola fonte per le regole) e
della mailbox (un solo proprietario del dato).

### Fatto in questo giro
- **Home aggiornata**: ci sono ora le card **Missioni e Obiettivi**, **Classi** e **Validazione
  contenuti**, riordinate per *cosa fai* (contenuto di gioco → strumenti) invece che per ordine
  storico. Le descrizioni dicono anche cosa il modulo **non** decide ("Entity: arma/AI le decide la
  Classe"; "AI Editor: i profili si editano in Balance").
- **EntityEditor**: selettore di classe + avviso di precedenza.
- **Fix**: `saveObjective` scriveva **sempre** `actor_team`, anche per `eliminate_target` che non lo
  usa (conta qualsiasi kill del team bersaglio). Era già successo davvero: `thin_the_garrison` si è
  portato dietro un `actor_team` inerte al primo salvataggio dall'editor. Ora ogni tipo scrive solo
  i campi che usa. File ripristinato.


## 2026-07-16 (11) — ADR-022 riscritto sul modello reale + metà NPC delle classi implementata

### La spiegazione dell'utente ha ribaltato ADR-022 — e ha mostrato che avevo letto male il GDD
Modello reale delle classi, in **tre parti**:
1. **NPC**: la classe indica *"abilità, comportamento, loadout e in caso l'aspetto"* → si
   **instanzia** su un'unità (GDD 12.3).
2. **Giocatore**: *"**non ne sceglie una**: le classi sono tipo un albero delle abilità. Ogni classe
   esiste **contemporaneamente** e può essere **livellata** usando certe armi o completando certi
   tipi di obiettivi... il **gameplay decide** quali classi crescono"*.
3. **Specializzazioni** (ARC Trooper, Clone Commando): *"si sbloccano completando obiettivi
   specifici... **non si livellano**"* → terzo asse, non un ramo delle classi.

**GDD 11.3 lo diceva già**: *"la classe **non è una scelta rigida all'inizio**, ma un'identità che
emerge dal comportamento"*. **Non l'avevo letto**: avevo studiato il cap. 12 (classi) e non l'11
(progressione). Errore di metodo → vincolo registrato in ADR-022: *un concetto può essere
specificato in più capitoli; cercarlo in tutto il GDD, non solo nel capitolo omonimo.*

**Conseguenza scomoda**: `MatchSettings.classId` + la riga **"Classe" nel PreMatch** — che ho
costruito ieri — sono **contrari al design**: fanno scegliere al giocatore una classe che gli
assegna il loadout. Per il giocatore quello è un **preset di loadout**, non una classe. Va rimosso
o rinominato: lasciarlo significa che "classe" ha **due significati opposti** nello stesso codice,
la deriva nome↔concetto che il progetto paga da mesi (KI #7/#25/#35). **Decisione all'utente.**

### ADR-022 riscritto (la prima stesura è marcata SUPERATA, non cancellata)
`ClassDef` resta **una** definizione usata in due modi (è il modello dell'utente: *"la stessa classe
esiste sia per i cloni alleati, sia per il personaggio"*). Metà NPC implementabile ora; metà
giocatore (XP/livelli/perk) → Fase 3, doc 27; specializzazioni → terzo tipo, non ora.

### Metà NPC — implementata
- **`ClassDef.aiProfileId`**: è ciò che rende la classe una **professione** e non un pacchetto di
  armi. `EnemyDef.classId` (opzionale): l'unità referenzia una classe invece di ripetere
  loadout+profilo+abilità. Supera l'Out of Scope del doc 14 (*"non accoppiare gli archetipi AI a
  ClassDef senza un ADR separato"*) — ADR-022 **è** quell'ADR.
- **Ogni campo della classe vince solo se valorizzato**: un'unità può referenziare una classe e
  tenersi una particolarità. Nessuna classe → **tutto come prima** (additivo).
- Gate ADR-018 esteso: `classes.ai_profile` e `enemies/allies.class` sono riferimenti incrociati
  come gli altri. Dropdown "Profilo AI" nel ClassEditor.
- Fix collaterale: il messaggio d'errore del profilo AI stampava quello **dell'unità** anche quando
  il profilo risolto veniva dalla classe → indicava il dato sbagliato da correggere.

### Verifiche (sonda deterministica, poi rimossa; dati ripristinati con git)
- **La classe fornisce davvero comportamento e loadout**: `Clone Trooper` ha nei dati
  `weapons:["DC-15A"]` e `ai_profile:"B1 Battle Droid"`, ma referenziando una classe Heavy risolve
  **`arma=Z-6 Rotary Blaster profiloAI=B1 Heavy Droid`**.
- **Additivo**: rimossa la classe → torna a `arma=DC-15A profiloAI=B1 Battle Droid`, identico.
- **Gate**: classe inesistente su un'unità + profilo AI inesistente su una classe → 2 Error, exit 1.
- Build **0 errori, 0 warning**; `--validate` 0/0; `--stress 6` pulito; nessuno scaffold né dato di
  test rimasto (verificato con `git status`).

## 2026-07-16 (10) — Fix dropdown obiettivi + modulo editor "Classi" (authoring completo)

### Bug segnalato dall'utente: "non tutti gli obiettivi appaiono nel menu a tendina"
Due difetti distinti, uno reale e uno di comunicazione:
1. **`static int sel` condiviso fra i due dropdown** (primari/opzionali): uno `static` dentro una
   lambda è condiviso da **tutte** le sue invocazioni, quindi selezionare in un dropdown faceva
   saltare l'altro — ed era condiviso anche fra missioni diverse. **Fix**: una selezione per lista
   (`m_addSelPrimary`/`m_addSelOptional`).
2. **Il filtro non era spiegato**: gli obiettivi già nella missione non ricompaiono (aggiungerli
   due volte, o metterne uno fra primari *e* opzionali, sarebbe un dato incoerente) — ma nulla lo
   diceva, quindi sembrava che il dropdown perdesse voci da solo. **Fix**: conteggio esplicito
   *"N obiettivi già in questa missione (non rielencati): toglili con X per riassegnarli"*.
3. Bonus: il dropdown mostrava l'**id grezzo** mentre la lista sopra mostra il nome — due modi di
   chiamare la stessa cosa nella stessa finestra. Ora mostra `Nome (id)`.

### Modulo "Classi" (Moduli → Classi)
Ultimo pezzo di contenuto che si autorava a mano. Nome, ruolo, arma primaria/secondaria e abilità,
tutti da **dropdown del registry**. Stessa disciplina del modulo Missioni: `saveJsonRMW` (ADR-010),
`id` mai scritto nel JSON (ADR-001), nuova categoria `rename::Category::Class`, creazione col
minimo valido per il gate (l'arma primaria è obbligatoria), ordinamento **congiunto** delle liste
parallele id/label (ordinarne una sola le disallineerebbe: l'arma mostrata non sarebbe quella scelta).
**Onestà sui limiti**, in giallo nel pannello: il `role` è solo un'etichetta che **nessun sistema
consuma** (ADR-022 Proposed), e le abilità sono trasportate ma **senza effetto** (KI #32).

### Il salvataggio è stato verificato SUI DATI REALI — dall'utente, senza saperlo
Il `git status` mostrava due file dati modificati che io non avevo toccato. Non era un incidente:
erano **le modifiche dell'utente** che testava l'editor (`firebase_ridge.time_limit` 300→350,
`reach_east_ridge.target.radius` 4→5). Confronto campo per campo con `git show HEAD:`:
- **nessun campo perso** in nessuno dei due file;
- `reach_east_ridge` ha conservato **`target.y`**, che `saveObjective` **non scrive** → è l'RMW di
  ADR-010 che preserva ciò che il modulo non possiede. È il test vero di quella regola, passato su
  dati reali e su un file che l'utente teneva.
Nota: i file salvati dall'editor escono riformattati (`dump(4)`, chiavi ordinate) — rumore nel
diff, non perdita di dati; è il comportamento di ogni modulo dell'editor da sempre.

### Verifiche
- Build Debug completa (GFEngine + GFEditor): **0 errori, 0 warning**. `--validate` 0/0.
- **Round-trip classi**: i campi scritti coprono tutti quelli presenti in `trooper`/`marksman`.
- **NON verificato**: il modulo Classi a schermo e una rinomina di classe reale.

## 2026-07-16 (9) — Modulo editor "Missioni e obiettivi": l'authoring esce dai JSON a mano

Chiude il debito aperto dalla direttiva utente del 2026-07-16 (*"più cose posso modificare
dall'editor meglio è; quello rimane lo strumento principale che IO posso usare"*): in tre giorni
avevo aggiunto obiettivi, missioni, classi e conseguenze **senza alcun modulo editor**. Il doc 25
stesso prevedeva *"prima lo schema e il runtime, poi l'authoring"* — schema e runtime erano in
force, quindi l'authoring era il passo dovuto.

### Il modulo (Moduli → "Missioni e obiettivi")
Due tab, perché sono due tipi di definizione distinti: **un obiettivo esiste di per sé** e può
essere usato da più missioni.
- **Obiettivi**: nome, tipo, tier, bersaglio, attivazione, limite di tempo, ricompensa, e le
  **conseguenze** (`on_success`/`on_failure`) con "+ conseguenza" / "X".
- **Missioni**: nome, briefing, mappa, modalità, composizione degli obiettivi (primari/opzionali)
  e regole di successo/fallimento.

### Vincoli rispettati (sono la ragione per cui il modulo è fatto così)
- **Dropdown dal registry, mai id a testo libero** (CLAUDE.md): gli obiettivi si **compongono** da
  una lista, la mappa viene dal registry, e **i command post vengono dalla mappa DELLA MISSIONE**
  — `CaptureZone`/`unlock_spawn` referenziano una label, che a mano è un riferimento rotto in
  attesa di accadere.
- **`saveJsonRMW`** (ADR-010) per ogni scrittura: si toccano solo i campi propri.
  **`id` non è mai scritto nel JSON** (ADR-001): è il filename, e un id in-file stantio ha già
  rotto le cross-ref in silenzio (KI #21).
- **Rinomina via comando**, con **sweep delle cross-reference reali** — nuove categorie
  `rename::Category::Objective` (→ `missions.primary/optional_objectives[]`,
  `objectives.activation.objective`, `linked_objectives[]`) e `::Mission`. Senza lo sweep,
  rinominare un prerequisito lascerebbe l'obiettivo dipendente **inattivabile per sempre**.
- **Ogni tipo mostra solo i campi che USA**: un `value` su `block_enemy_reinforcements` non lo
  legge nessuno, e mostrarlo sarebbe una bugia. Idem per il target del bersaglio.
- **Onestà sui limiti**: i tipi dichiarati ma non eseguiti dal runtime (DestroyTarget, EscortEntity,
  SurviveWave, InteractHack) sono selezionabili **con un avviso arancione** — non nascosti, ma
  nemmeno silenziosi.
- **Creazione = minimo VALIDO per il gate** (missione con entrambe le regole, obiettivo con target
  sensato): un contenuto che nasce già rifiutato sarebbe authoring ostile.

### Verifiche
- Build Debug completa (GFEngine + GFEditor): **0 errori, 0 warning**. `--validate` 0/0.
- **Forma dei dati verificata contro i file reali**: i tre punti dello sweep di rename
  (`primary_objectives[]` array, `activation.objective` stringa annidata, `linked_objectives[]`)
  combaciano con `firebase_alpha`/`hold_alpha`, dove `capture_alpha` è referenziato da **due** file.
- **Round-trip del save**: i campi scritti da `saveObjective` coprono tutti quelli presenti nei
  file reali; `target` contiene esattamente `post`/`actor_team` per CaptureZone. Nessun campo
  perso — e l'RMW preserva comunque gli ignoti.
- **Dati reali intatti**: nessun test li ha toccati (verificato con `diff`).
- **NON verificato — serve un giro in GFEditor**: l'interfaccia (layout, dropdown popolati) e un
  salvataggio/rinomina reali. È il pezzo che va provato a mano: il salvataggio è la classe di
  operazione che nel 2026-07-08 ha distrutto dati.

## 2026-07-16 (8) — `consequence`: gli obiettivi smettono di essere caselle da spuntare

### La direttiva che ha sbloccato il lavoro (e corretto un mio errore)
Avevo rifiutato di implementare `consequence` dicendo *"gli esempi non sono una specifica, serve
design prima del codice"*. **Sbagliato**, e l'utente l'ha chiarito:
> *"Andremo molto avanti ad esempi, perché certe cose non potrò sapere quanto vanno bene senza
> averle provate. Dobbiamo costruire i **sistemi**, più possibile modificabili ed espandibili...
> anche se vuol dire impostare valori/obiettivi/**conseguenze temporanei** da rifinire più avanti."*

Correzione di calibro registrata in **10_ProjectMemory** (direttiva permanente): GDD 21.4 vieta di
cablare **regole di design nel codice** (pesi, formule, id, comportamenti), **non** di usare valori
provvisori nei **dati**. Un valore nei dati è un segnaposto che l'utente cambierà; un `if` nel
codice no. Il design che l'utente non può decidere a tavolino è proprio quello **da provare**.

### Il sistema
`ObjectiveDef` guadagna `on_success[]` / `on_failure[]`: liste di `ConsequenceDef {type, value,
target}`. Quando un obiettivo si conclude, `ObjectiveSystem` le applica **scrivendo solo su
`World::battleState`** — da lì le legge il sistema competente. **Nessun `if (objectiveId == ...)`**:
aggiungere un tipo = un enum + un `case` + un lettore nel sistema giusto, senza toccare gli altri.
È il vincolo che ADR-019 impone e che rende il sistema espandibile.

Tipi implementati (dagli esempi dell'utente), tutti agganciati a sistemi **reali**:
| Tipo | Effetto | Chi lo legge |
| --- | --- | --- |
| `block_enemy_reinforcements` | il nemico non rimpiazza più le perdite | `ConquestMode::checkDeaths` |
| `enemy_accuracy` | moltiplica la precisione nemica (<1 = disorganizzati) | `AiSystem` (solo team 2) |
| `ally_reinforcements` | aggiunge riserve alla squadra | `ConquestMode::update` (possiede i ticket) |
| `unlock_spawn` | la squadra rinasce al post catturato | `battleState.allySpawnPost` |

**I valori sono segnaposto da bilanciare provando**: `capture_alpha` → sblocca lo spawn + 2 riserve;
`hold_alpha` → taglia i rinforzi nemici + precisione nemica a 0.6.
`enemy_accuracy` è **moltiplicativo**: due obiettivi che disorganizzano il nemico si sommano invece
di sovrascriversi.

### Gate ADR-018 esteso
Un `type` con refuso resterebbe `None` e non farebbe **nulla** — l'obiettivo sembrerebbe avere un
effetto e invece è una casella. Ora è **Error**. Validati anche: `enemy_accuracy` fuori da (0,1]
(non disorganizza, o migliora il nemico), `unlock_spawn` senza target o verso un post inesistente
nella mappa della missione, `ally_reinforcements` a 0 (Warn).

### Verifiche (sonda deterministica, poi rimossa)
- **Applicazione**: tutte e 4 le conseguenze emesse su JSONL (`consequence applied`).
- **EFFETTO REALE misurato**: con `block_enemy_reinforcements` attivo, 2 nemici uccisi →
  **2× "RINFORZI INTERROTTI"** e **0 rimpiazzi**. La battaglia cambia davvero.
- **Gate**: type inesistente + `enemy_accuracy: 2.5` + `unlock_spawn` verso post "Zulu" → 3 Error,
  exit 1.
- **Due scenari di test inadatti scartati onestamente** prima di concludere: (1) in Conquista gli
  alleati non catturano mai Alpha (8 s continui, seguono il giocatore fermo); (2) con la missione
  che finisce **nell'istante** in cui applica la conseguenza, l'effetto non può manifestarsi; (3) 1
  alleato contro 6 non uccide nessun nemico → serviva una battaglia bilanciata. Nessuno dei tre era
  un bug del codice.
- Build **0 errori, 0 warning**; `--validate` 0/0; `--stress 6` pulito; nessuno scaffold né dato di
  test rimasto.

### Debito esplicito (direttiva utente: l'editor è lo strumento principale)
Obiettivi, missioni, classi **e ora le conseguenze** si autorano **a mano nei JSON**: nessun modulo
editor. Con la direttiva del 2026-07-16 questo è il debito **più importante** aperto — vedi 06_Todo.

## 2026-07-16 (7) — Statistiche di missione + debrief: le fondamenta del "giudizio"

### La domanda di design era bloccante, l'utente l'ha sciolta
Avevo lasciato il sistema di giudizio in attesa perché il GDD 9.6 dice *"il risultato è narrativo,
non un semplice voto"* e non sapevo cosa significasse in pratica. Chiarimento dell'utente:
> *"È narrativo perché c'è un **insieme di fattori e di scelte** che portano a dei risultati,
> valutati insieme a tutte le statistiche. Così l'esperienza risulta vera: piccole scelte tattiche
> influenzano il giudizio finale, proprio perché sono un insieme di cose."*

**"Narrativo" non significa prosa generata**: significa che il giudizio nasce dalla *combinazione*
dei fattori, non da un numero unico. Questo rende il sistema progettabile — e dice anche cosa
**non** fare: nessun voto, nessun peso inventato qui (i pesi diventano esperienza → progressione,
doc 27, e sono design: GDD 21.4).

### Cosa c'è ora
- **`World::missionStats`** (mailbox): `playerKills`, `teamKills`, `alliesLost`, `playerDeaths`,
  `missionTime`, `objectivesDone/Failed`. **Accumulate mentre i fatti accadono** — una kill esiste
  solo nell'istante in cui avviene, e l'entità viene distrutta subito dopo: ricostruirle a
  posteriori è impossibile. Azzerate da `World::initialize()`: sono per-missione, non per-sessione.
- Ognuno registra **ciò che sa lui**: CombatSystem (kill/perdite, con `bullet.fromPlayer` per
  attribuire quelle del giocatore), ObjectiveSystem (tempo + esiti obiettivi, in **tutti e tre** i
  punti di fallimento), Application (morti del giocatore, incluse quelle non da proiettile).
- **Debrief a fine partita**: sostituisce il testo **cablato e ormai falso** delle schermate
  Win/Lose (*"Tutti i nemici eliminati"* / *"Sei stato eliminato"* — si vince anche completando una
  missione e si perde anche fallendo un obiettivo). Ora racconta i fatti veri.
- **Debrief anche su JSONL** (`match end`): il giudizio dev'essere leggibile da un tool/LLM senza
  guardare lo schermo, ed è da lì che la progressione prenderà l'esperienza quando esisterà.
- **Nessun punteggio calcolato**, di proposito: si mostra la scomposizione. È il consumatore che
  mancava — senza, sarebbe stato l'ennesimo sistema isolato (GDD 21.2).

### Un difetto colto scrivendo
`alliesLost` contava anche il **giocatore** (è team 1!), che l'utente distingue esplicitamente
("alleati morti" ≠ "numero di morti"). Escluso via `world.playerEntity`.

### Verifiche (sonda deterministica sul percorso reale, poi rimossa)
- **Statistiche reali, non zeri**: missione da 20 s → `allies_lost: 2, mission_time: 20.03,
  objectives_done: 2, outcome: win`. Missione da 60 s, 1 alleato vs 6 → `allies_lost: 5,
  **player_deaths: 3**, outcome: lose`.
- **Contatore kill verificato**: restava 0 anche dopo 48 s. Non era un bug: **1 alleato contro 6
  non uccideva nessuno**. Con squadre bilanciate → `team_kills: 1`. *Un contatore mai visto
  contare non è verificato* — è la lezione di ADR-018 applicata a sé stessa.
- **NON esercitato**: `player_kills` (nei test headless il giocatore è fermo e non spara mai) e
  l'aspetto grafico del debrief.
- Build **0 errori, 0 warning**; `--validate` 0/0; nessuno scaffold né dato di test rimasto.

### Registrato in doc 25 (intento dell'utente, non implementato)
1. **Gli obiettivi hanno un VANTAGGIO tattico**: *"ogni mappa avrà posti di comando, bersagli
   strategici, zone strategiche — ognuno con un suo vantaggio"* (posto catturato → nuovo punto di
   spawn; torre comunicazioni distrutta → nemici disorganizzati; base d'atterraggio presa → niente
   rinforzi nemici). È il campo **`consequence`** dello schema doc 25 — **mai implementato, e non
   segnalato come mancante**: doc↔codice drift, ammesso e ora documentato. Gli esempi sono
   illustrativi, non una specifica → serve design prima del codice.
2. Vincolo architetturale da rispettare: ogni consequence tocca un sistema diverso → dato
   dichiarativo, **mai** `if (objectiveId == ...)`, o si reintroduce il fork che ADR-019 evita.

## 2026-07-16 (6) — I ticket sono RINFORZI, non le vite del giocatore (KI #39)

### Il contesto che ha rivelato il disallineamento
Chiarimento dell'utente sul design: **i ticket sono la riserva di rinforzi della squadra** — non
potendo avere centinaia di truppe in campo, c'è un cap di AI e il resto entra man mano che quelle
in campo cadono. *"Idealmente il giocatore non dovrebbe consumare ticket alla morte, ma se muore
quando non rimane più nessun alleato vivo e nessun rinforzo allora perde."*
Verificato contro il codice: **il contrario**. Ogni morte del giocatore bruciava un rinforzo della
squadra (3 punti in Application), e a ticket 0 la morte era **sconfitta secca anche con la squadra
intatta**. Il meccanismo dei rinforzi invece esisteva già ed era corretto:
`ConquestMode::checkDeaths` → un'unità cade → consuma un ticket → un rimpiazzo entra dalla riserva;
a 0 ticket, **morte permanente**.

### Fix
- **Regola in UN SOLO POSTO** (`onPlayerDeath`, prima era duplicata in due rami + un terzo nel
  respawn volontario): il giocatore **non consuma rinforzi** morendo; si perde **solo** cadendo
  quando non resta né un alleato vivo né un rinforzo in arrivo.
- Vale anche per il **respawn volontario** (K): è una morte come le altre — niente più "nessun
  ticket → SCONFITTA" immediata, ma nemmeno un suicidio gratis da ultimo superstite.
  Attenzione al dettaglio: `onPlayerDeath` può decidere la sconfitta, quindi il ritorno a
  `Playing` è condizionato — altrimenti la cancellerebbe.
- **`IGameMode::consumeTeam1Ticket()` RIMOSSO** (interfaccia + 2 implementazioni): serviva solo a
  far pagare al giocatore le proprie morti, ed era rimasto **codice morto**. Rimuoverlo rende la
  regola **strutturale** invece che una convenzione da ricordare: non si può più far consumare un
  rinforzo al giocatore per sbaglio, perché il metodo non esiste. Nota nell'header per chi
  fosse tentato di reintrodurlo.

### Verifiche (sonda deterministica sul percorso reale, poi rimossa)
- **Il giocatore muore → i rinforzi NON calano**: `[PBTEST] rinforzi PRIMA: 5` →
  *"Eliminato! Respawn in 4s (alleati vivi: 1, **rinforzi: 5**)"*.
- **Alleati a 0 ma rinforzi in arrivo → respawn**: *"(alleati vivi: 0, rinforzi: 2)"*.
- **Catena completa fino alla sconfitta** (1 ticket, 1 alleato): morte del giocatore → respawn
  (rinforzi invariati) → *"[Respawn] Alleato eliminato. NESSUN ticket — morte permanente"* →
  morte del giocatore → *"[Game] SCONFITTA: squadra annientata e nessun rinforzo"*. **Entrambi i
  rami della regola nuova verificati.**
- Build Debug completa **0 errori, 0 warning**; `--validate` 0/0; `--stress 6` e `--sandbox`
  senza crash.

### Nota di design registrata (non implementata)
Le **statistiche di missione** (kill, morti, alleati persi, obiettivi completati, tempo) servono
al sistema di **giudizio/debrief** post-missione. Il GDD lo specifica al **9.6**: *"La valutazione
finale pesa obiettivi (primari/secondari/falliti), prestazione tattica e costi (perdite, tempo,
risorse). **Il risultato è narrativo, non un semplice voto**"*, e al 5.2 *"il risultato deve
raccontare una storia"*. Pesi e forma del debrief sono **decisioni di design**, che il GDD 21.4
vieta di prendere scrivendo codice → serve un giro di design prima. Vedi 06_Todo.

## 2026-07-16 (5) — `CaptureZone`/`DefendZone`: il framework obiettivi sa finalmente esprimere i command post

### Perché questo e non i Punti Comando
Il candidato naturale era l'**economia tattica** (doc 26 / GDD 5.4), ma il GDD definisce solo la
direzione (*"spendibile per rinforzi, veicoli o supporto orbitale"*) e lascia aperti **sink e
prezzi** — cioè decisioni di design che il GDD 21.4 dice espressamente di non prendere mentre si
scrive codice. Inoltre implementarne solo il *guadagno* darebbe un numero sull'HUD che non si
spende: un altro sistema isolato, l'errore appena corretto.
Il collegamento **già deciso** (doc 25: *"il command post diventa generabile come ObjectiveDef di
tipo CaptureZone/DefendZone. Non riscrivere ADR-009: avvolgerlo"*) era invece più urgente di
quanto sembrasse: **il framework obiettivi non sapeva esprimere la meccanica principale del
gioco.** Una missione non poteva dire "cattura Alpha" — l'obiettivo più ovvio di Galactic Front.

### Come (avvolgere, non riscrivere)
- **Mailbox `World::commandPostStates`**: i post vivono in `CommandPosts` dentro il game mode, che
  `ecs/` non può includere. Application li pubblica **fra `mode->update()` e `world.tick()`**, così
  ObjectiveSystem legge lo stato di *questo* tick e non di quello prima.
- `CaptureZone` legge **solo chi possiede** il post; `DefendZone` richiede di tenerlo per
  `hold_seconds` e **fallisce subito se il post si perde** (un post perso è perso, non un timer che
  si azzera). La logica di cattura resta interamente in ADR-009: zero duplicazione.
- **Riferimento per LABEL** (`target.post`): i command post non hanno un id — la label è il loro
  unico nome autorato. Il **gate ADR-018** la risolve **nella mappa della missione** (è l'unico
  riferimento incrociato che dipende da una definizione scelta altrove) e segnala label ambigue
  (duplicate nella stessa mappa).
- Contenuto nuovo in repo: `capture_alpha`, `hold_alpha`, missione **`firebase_alpha`**
  ("cattura Alpha, poi tienilo 20s") — la prima missione che usa la meccanica vera del gioco.

### Bug corretto durante il lavoro
`evaluate()` ora può concludere da sé con un fallimento proprio del tipo (DefendZone: post
perduto), ma il chiamante proseguiva con il controllo del `timeLimit` → **due eventi
`objective failed`** per lo stesso obiettivo. Aggiunta la guardia `if (r.state != Active) continue;`.
Verificato: **1 solo evento**.

### Verifiche (sonda deterministica sul percorso reale, poi rimossa)
- **Build**: Debug completa, **0 errori, 0 warning**. `--validate` 0/0.
- **Catena completa** (missione in modalità Difesa, dove i post partono agli alleati):
  `capture_alpha` attivato → **completato** → `hold_alpha` si attiva **solo dopo** (dipendenza) →
  tenuto 20 s → **completato** → `mission success` → **VITTORIA**.
- **Fallimento** (modalità Assalto, post in mano nemica): `objective failed` (**uno solo**) →
  `mission failed` → **SCONFITTA**.
- **Gate**: post `Zulu` inesistente nella mappa → *"post 'Zulu' non esiste nella mappa 'firebase'"*
  + exit 1.
- **Diagnosi onesta di un test che sembrava fallire**: in Conquista gli alleati *iniziano* a
  catturare Alpha ma non finiscono (servono 8 s continui; seguono il giocatore fermo e escono dal
  raggio) → nessuna cattura, nessun completamento: **comportamento corretto**, scenario inadatto.
  Il primo grep dava "cattura" per via di *"Mouse catturato"* — falso positivo del test, non del codice.
- **Non-regressione**: senza missione zero eventi Objective, `--stress 6` invariato, nessun crash.

## 2026-07-16 (4) — Missioni e classi selezionabili dal PreMatch: fine dell'ultima isolazione

Stessa lente del giro precedente (GDD 21.2 "evitare i sistemi isolati", confermata dall'utente:
*"un sistema base stabile e ben fatto prima di aggiungere altro"*). Restava una isolazione grossa:
il **sistema di missioni è Core per il GDD 23.1**, ma era raggiungibile solo da `--mission` — cioè
in partita normale HUD ed esiti appena collegati non si vedevano mai. Stesso problema per le classi.

### Cosa c'è ora
- **Riga "Missione"** in cima al PreMatch (solo se esistono missioni autorate) e **riga "Classe"**
  (solo se esistono classi). Indice 0 = *"(nessuna)"* → **partita libera / loadout manuale**: il
  default è il comportamento storico, il sistema resta additivo.
- **La missione impone mappa e modalità, e il menu lo MOSTRA**: scegliendola, le righe Mappa e
  Modalità si aggiornano a vista (`syncRowsToMission`). L'alternativa — lasciare il menu su
  "outpost" e far correggere Application di nascosto — avrebbe fatto leggere al giocatore una
  cosa e giocarne un'altra.
- Riuso esatto del pattern delle mappe (`MapEntry`/`m_mapNamePtrs`/riga enum), non un meccanismo
  nuovo. Gli indici vivono nella UI, **non** in `MatchSettings`: si persiste l'**ID** (lezione
  KI #20 — l'indice cambia significato se si aggiunge o rinomina una definizione).
- `missionId` persistito nei preset; `applyPreset`/`setSettings` ricostruiscono gli indici dagli
  id. Un id che non risolve più (definizione cancellata) torna a "(nessuna)": degradazione onesta.
- `--mission`/`--class` ora **seminano** solo la scelta iniziale; la missione si risolve in
  `initWorld` da `currentSettings.missionId` (prima era congelata al flag CLI all'avvio).

### Due bug della stessa famiglia, trovati e corretti durante il lavoro
1. **Il menu azzerava la missione seminata dal CLI**: `setSettings()` copiava la struct senza
   ricostruire gli indici delle righe enum → `getSelectedMissionId()` tornava vuoto e la missione
   spariva. È di nuovo **KI #36** (un campo posseduto da un componente che ne sovrascrive un altro).
   Fix: `setSettings` risolve id→indice, e Application chiama `setSettings(currentSettings)` dopo
   aver seminato i valori CLI.
2. **La toppa di KI #36 è diventata sbagliata**: `startFromPreMatch` ripristinava `classId` da
   `currentSettings` se il menu ne aveva uno vuoto. Ora che il menu **possiede** classe e missione,
   quel ripristino distruggerebbe una scelta esplicita ("(nessuna)" dopo un `--class`). Rimosso;
   resta solo per `characterId`, che nel menu non c'è. *Una toppa va rimossa quando sparisce il
   buco che copriva.*

### Correzione di un'affermazione sbagliata di ieri (nello stesso giorno)
Avevo scritto — e detto all'utente — che **`--direct-prematch` avvia la partita da solo** dopo
~15-20 s. **Falso**: è **non deterministico** (stesso comando, stessa durata: a volte parte, a
volte no). Era una generalizzazione da poche run fortunate. Il modo corretto di verificare il
percorso PreMatch→partita headless è una **sonda temporanea che chiama `startFromPreMatch()`**,
cioè la stessa funzione del tasto ENTER. 10_ProjectMemory corretto.

### Verifiche (tutte con sonda deterministica, poi rimossa)
- **Build**: Debug completa (GFEngine + GFEditor), **0 errori, 0 warning**. `--validate` 0/0.
- `--mission firebase_ridge` → percorso PreMatch → partita: `mission started`, **2 obiettivi
  attivati**. (Prima della correzione del bug 1: **0** — regressione colta e chiusa.)
- `--class marksman` → `primary: DC-15X`; `--class trooper` → `DC-15A`.
- `--mission firebase_ridge --map outpost` → *"--map 'outpost' ignorato: la missione impone
  'firebase'"*.
- **Non-regressione**: senza missione/classe, zero eventi Objective, `--stress 6` invariato,
  nessun crash.
- **NON verificato**: l'aspetto delle nuove righe nel menu (posizione, leggibilità) e il
  comportamento a vista di Mappa/Modalità che cambiano quando si scorre la missione.

## 2026-07-16 (3) — N2 Phase B: il sistema obiettivi smette di essere un sistema ISOLATO

**Prossimo passo scelto leggendo il GDD**, non i soli ProjectDocs — ed è il primo giro in cui il
GDD (ora in `29_GDD.md`) ha guidato la priorità invece di limitarsi a validarla:
- **GDD 21.1**: *"Prima le fondamenta, poi l'espansione. **Meglio pochi sistemi solidi che molti
  incompleti**"* e *"Profondità tramite sistemi **collegati**, non tramite quantità"*.
- **GDD 21.2**: *"**Evitare i sistemi isolati.** I sistemi principali devono comunicare."*
- **GDD 23.2** (criteri di successo): *"le funzionalità sono **integrate tra loro**"*.
Verdetto sullo stato reale: in tre giorni erano stati aggiunti **tre sistemi a Phase A** (squadra,
obiettivi, classi) e il framework obiettivi era **isolato al 100%** — quindi la scelta giusta non
era un quarto sistema (né ADR-022), ma **collegare**.

### Tre connessioni mancanti, tutte trovate per analisi (nessun test le avrebbe viste)
1. **`ObjectiveSystem::outcome()` non lo chiamava NESSUNO** → codice morto: completare una
   missione non faceva assolutamente nulla. Stesso difetto del ramo FocusFire (2026-07-15).
   **Fix**: Application tiene un puntatore non-proprietario al sistema (i sistemi sopravvivono a
   `World::initialize()`, verificato) e l'esito della missione chiude la partita. Divisione di
   doc 25 rispettata: **il mode ha la precedenza** (se i ticket hanno già deciso, la missione non
   ribalta); la missione decide solo quando il mode è ancora `Ongoing`.
2. **Nessun HUD obiettivi** → il giocatore non poteva *vedere* la missione. Un obiettivo che non
   si vede non esiste. **Fix**: pannello OBIETTIVI (colonna sinistra) letto dallo **stato reale**
   del sistema; primari in evidenza, opzionali defilati; colore = stato (verde fatto / rosso
   fallito). Progresso mostrato **solo dove esiste davvero** (`EliminateTarget` N/M,
   `HoldAreaForDuration` s/s): inventarlo per gli altri tipi sarebbe un numero falso. Gli
   obiettivi `Inactive` non si mostrano — rivelerebbero la struttura della missione in anticipo.
3. **La missione non imponeva la sua mappa** → `--mission firebase_ridge --map outpost` avrebbe
   piazzato obiettivi a coordinate senza senso su un'altra mappa. **Fix**: `MissionDef.mapId`
   vince; un `--map` contraddittorio viene **segnalato**, non risolto in silenzio.

### Bug trovato per analisi: missione congelata al riavvio
I sistemi sopravvivono a `World::initialize()`, e `ObjectiveSystem` ri-bindava solo se il
*puntatore* alla missione cambiava. Al riavvio della stessa missione: nessun rebind → obiettivi
ancora "completati" e `m_outcome != Ongoing` → early-return **per sempre**. Riavviare una missione
completata non la ricominciava. **Fix**: rebind anche quando il tick torna indietro (è il segnale
di restart — `initialize()` azzera `m_tickCount`).

### Correzione di un mio errore di metodo (importante)
`--direct-prematch` **avvia la partita da solo** dopo ~15-20 s. I miei test dei giorni scorsi
usavano `timeout 12-14` e concludevano "non verificabile headless, serve un playtest" — **falso**:
erano solo troppo corti. Conseguenza: **il test della classe chiesto all'utente era inutile**, e
ora è verificato headless (`--class marksman` → `primary: DC-15X`; `--class trooper` → `DC-15A`;
`character equipped` presente). Lezione: prima di dichiarare qualcosa non verificabile, verificare
che il *tentativo* di verifica fosse valido.

### Verifiche
- **Build-verified**: Debug completa (GFEngine + GFEditor), **0 errori, 0 warning**.
- **Esito missione → partita, in ENTRAMBE le direzioni** (test deterministico, partita vera):
  fallimento a tempo → **"SCONFITTA (obiettivo perso)"**; obiettivo primario completato →
  **"VITTORIA"**. Prima: nessuno dei due, `outcome()` era morto.
  *Nota*: il primo test usava `--stress`, che gira in **osservatore** (dove per design la partita
  non finisce mai) → invalido. Stessa trappola del "verificato in sandbox" di ieri.
- **Mappa imposta**: `--mission firebase_ridge --map outpost` → *"--map 'outpost' ignorato: la
  missione impone 'firebase'"*.
- **Non-regressione**: senza missione, **zero** eventi Objective, nessun pannello, `--stress 6`
  invariato, nessun crash; `--validate` 0/0.
- **NON verificato**: l'aspetto grafico del pannello OBIETTIVI (posizione/leggibilità) — build-ok
  ma non guardato a schermo.

## 2026-07-16 (2) — GDD convertito in `29_GDD.md`: sorgente `.docx` + copia operativa `.md`
Su richiesta dell'utente ("come sfruttare al meglio il GDD"): il `.docx` restava leggibile solo
riestraendo XML da uno zip a ogni sessione — costo di tool call ripetuto, e il testo grezzo
appiattiva titoli e le 18 tabelle del documento (es. la matrice classi del cap. 12) in prosa
continua, rendendo fragile citare una sezione con precisione.
- **Convertitore** (Node, `docx2md.js`, non versionato — è uno script una tantum, non parte della
  build): legge `word/document.xml`, mappa `w:pStyle` (Heading1/2/3) su `##`/`###`/`####`,
  ricostruisce le tabelle da `<w:tbl>`/`<w:tr>`/`<w:tc>` in Markdown, preserva grassetto/corsivo
  dai run `w:r`. Un semplice strip dei tag XML (il metodo usato il 2026-07-15 per la prima
  lettura) avrebbe perso questa struttura.
- **Verifica di completezza prima di pubblicare**: conteggio parole del `.md` generato (tag
  rimossi) confrontato col testo grezzo — **11.800 / 11.800**, identico; ultima riga del
  documento presente (*"Fine del documento."*, dopo il Glossario in Appendice E).
- **`Galactic_Front_GDD.docx`** resta la sorgente di autoring (l'utente lo modifica in Word);
  **`ProjectDocs/29_GDD.md`** è la copia operativa da leggere/citare, con nota di provenienza in
  testa che vieta di modificarlo a mano. Rigenerazione documentata in 23_GameDesignBridge
  ("Dove vive il GDD"). Riferimenti in 13_ADR e 14_ClassSystem aggiornati a puntare al `.md`.

## 2026-07-16 — Classe e personaggio non arrivavano MAI in partita (KI #36) + il GDD entra nel repo

### Il bug (segnalato dall'utente: "le classi non funzionano")
`currentSettings = preMatchMenu.getSettings()` all'ENTER del PreMatch **sovrascriveva la struct
intera**, azzerando `classId` e `characterId` — che il PreMatch non possiede (non ha selettori) e
che erano stati risolti all'avvio. Un istante dopo partiva `startGame()` con i campi vuoti.
- È **la stessa modalità di guasto della regola READ-MODIFY-WRITE** (ADR-010): costruire un
  oggetto nuovo e sovrascrivere invece di modificare solo i propri campi. Lì era su file, qui in
  memoria. La disciplina RMW è documentata per i save JSON; **il pattern è più generale**.
- **Conseguenza peggiore, e mia colpa**: azzerava anche `characterId` → **nemmeno le stat del
  personaggio (KI #35) arrivavano in partita**. Avevo verificato in **sandbox**, che non passa da
  quella riga, e generalizzato al percorso reale. Il "feeling identico" confermato dall'utente era
  corretto **per il motivo sbagliato**: in partita il personaggio non veniva applicato affatto e
  il gioco usava i default del codice.
- **Fix**: `startFromPreMatch()`, punto unico. Se il PreMatch ha un valore (es. da un preset, che
  serializza `"class"`) **vince lui**; altrimenti si tiene quello risolto all'avvio — mai il
  contrario, sarebbe la toppa a distruggere una scelta esplicita.
- **Verificato sul percorso REALE** (non più solo sandbox): `--class marksman` → `class equipped`
  con `primary: DC-15X`; `--class trooper` → `DC-15A`; `character equipped` presente in entrambi.
  *Nota di metodo*: la sonda di test iniziale **replicava** la logica del fix invece di eseguirla —
  avrebbe potuto passare con il gioco rotto. Riscritta per esercitare **la stessa funzione** del
  tasto ENTER. Un test che non passa per il codice di produzione non prova niente.

### Il GDD originale è ora nel repo — ed è l'autorità di design
`Galactic_Front_GDD.docx` (53 KB, ~11.800 parole, 7 parti + appendici, indicizzato — dal
2026-07-16 anche come `ProjectDocs/29_GDD.md`, la copia operativa; vedi il changelog del giorno
dopo). Leggibile
estraendone il testo (`.docx` = zip; `word/document.xml`). **Va tenuto**: 23_GameDesignBridge
stabilisce che sull'*intento di design* il GDD vince, ma finora il GDD non era nel repo — la
regola di precedenza puntava a un documento assente. Al primo controllo ha già trovato un errore
di design reale (sotto).

### Conflitto trovato: 14_ClassSystem contraddice il GDD cap. 12
GDD 12, prima riga: *"Rappresentare **professioni militari, non semplici categorie di armi**."*
Doc 14 modella esattamente una categoria di armi (primaria + secondaria + abilità), con `role`
come tag che nessuno consuma, e **vieta** il legame con l'IA — mentre GDD 12.3 dice che le classi
definiscono *"composizioni degli NPC, il loro comportamento IA e loadout"* e che una squadra mista
"deve comportarsi diversamente" (è il pilastro #4, la squadra come risorsa).
Il `ClassDef` implementato copre **1 dei 6 parametri** che il GDD elenca (loadout base; mancano
perk sbloccabili, curva XP, comportamento IA associato, affinità equipaggiamento, requisiti di
sblocco). Non è sbagliato: è un **seme incorniciato male** dal doc 14.
→ **ADR-022 (Proposed)**: riconciliazione. Vedi 13_ADR. Nessun codice scritto in quella direzione:
è una decisione di design, non di implementazione.

## 2026-07-15 — KI #35 risolto: `PlayerDef` da tipo morto a dati vivi

Decisione delegata dall'utente, presa così: **(a) renderlo vivo, non cancellarlo.**

**Perché (a).** Fatto decisivo: i valori autorati **coincidevano già** con quelli che il gioco
usava (`move_speed 5.0` = `PLAYER_SPEED`, `hp 100` = `playerHp` default, moltiplicatori neutri) →
consumarli è a **variazione zero**: cambia qualcosa solo quando l'utente modifica i dati, che è
esattamente lo scopo di quel pannello. Cancellare avrebbe distrutto contenuto che la Fase 3
(personaggi/progressione, doc 27) dovrà comunque ricreare.

**La trappola che ha quasi rovinato il fix.** I dati dicevano `sprint_mult: 1.5`, ma il gioco
girava con `SPRINT_MULT = 1.65f` — costante **hardcoded in `PlayerController.cpp`** (contro
CLAUDE.md: le costanti di gameplay vanno nei dati o in GameConfig). Applicare i dati alla cieca
avrebbe **cambiato il feel dello sprint**, appena validato dall'utente. **La verità è il
comportamento, non il dato**: il dato è stato allineato a 1.65, poi reso autoritativo, e la
costante rimossa. Stessa logica per i default in codice: `PlayerController` e il loader hanno
default **identici alle vecchie costanti** → senza personaggio, comportamento invariato per
costruzione.

**Implementazione.** `MatchSettings.characterId` → risolto in **`initWorld`**, non in
`startGame()`: vale per **partita e sandbox**, perché il giocatore non può comportarsi
diversamente a seconda di come è entrato (era un difetto del mio primo tentativo).
`PlayerController` guadagna `moveSpeed`/`jumpMult`/`sprintMult`/`armorRating`;
`HealthComponent.armor` (generico, 1 = nessuna riduzione, guardia su <= 0) applicato in
`CombatSystem` dopo lo scudo. Gate ADR-018 esteso ai personaggi + campi fantasma sul loro loader.

**Selezione senza indovinare.** Con **un solo** personaggio autorato non c'è nulla da scegliere →
è il giocatore, e il pannello dell'editor diventa vivo **senza UI**. Con più personaggi la scelta
è ambigua e **non si indovina** (sceglierne uno a caso è il fallback hardcoded che ADR-007 ha già
fatto pagare): si logga che serve il selettore nel PreMatch.

**Verificato**: build Debug completa **0 errori / 0 warning**; `character equipped` in sandbox con
hp 100 / move_speed 5.0 / sprint_mult 1.65 / armor 1.0 = **esattamente i valori storici**;
personaggio rotto (hp/move_speed/armor ≤ 0, sprint_mult < 1, campo `id`) → Error + Warn + exit 1;
`--stress 8 --mission firebase_ridge` senza crash con squad/objective/character tutti attivi.

## 2026-07-15 — N4 Class System Phase A (doc 14) + scoperta: `PlayerDef` è un tipo morto

### La scoperta che ha cambiato il disegno
Il doc 14 parte da due premesse **false rispetto al codice live**, verificate prima di costruirci
sopra: (1) *"`EnemyDef`/`PlayerDef` referenziano `weaponIds[]` direttamente"* — `PlayerDef` non ha
`weaponIds`, ha solo stat base; (2) *"`PlayerDef` guadagna un `classId`"* — ma **nessuna riga in
`src/`/`include/` legge `PlayerDef`**: è autorato dal BalanceEditor e mai consumato.
Attaccarci la classe avrebbe prodotto una funzionalità **provatamente senza effetto**.
Il codice vince sulla documentazione (CLAUDE.md §1.2): la classe è andata su **`MatchSettings`**,
che il gioco legge davvero, e il doc 14 è stato corretto. → **KI #35** (nuovo, HIGH).

### ClassDef (esattamente lo scope del doc 14)
- `ClassDef` {id, name, primary_weapon, secondary_weapon, abilities[], role} in
  `Definitions.hpp`; `loadClasses()` + `getClass()`/`classes()` col pattern identico agli altri
  tipi (id = filename stem, ADR-001). `role` resta **tag descrittivo**: nessun sistema AI lo
  consuma, come impone l'Out of Scope del doc.
- **Consumo reale**: `MatchSettings.classId` → risolto in `startGame()`, **nello stesso punto in
  cui l'arma viene già scelta oggi** (Integration del doc 14): la classe riempie primaria,
  secondaria e abilità. Vuota = loadout manuale, comportamento **identico a prima** → additivo,
  non breaking. Persistito nei preset (`"class"`).
- Un `classId` sconosciuto **avverte e ricade sul manuale**, non degrada in silenzio.
- Nuovo flag `--class <id>`: il PreMatch non ha ancora un selettore, e senza un consumatore
  `ClassDef` sarebbe stato dato morto — cioè il difetto appena diagnosticato in KI #35.
- **Gate ADR-018 esteso alle classi**: arma primaria obbligatoria e risolta, secondaria e
  abilità risolte, primaria == secondaria → Warn, near-duplicate sui nomi.
- Esempi in repo: `trooper` (DC-15A + DC-17 + Combat Roll), `marksman` (DC-15X + DC-17 + Shield).

### Verifiche
- **Build-verified**: Debug completa (GFEngine + GFEditor), **0 errori, 0 warning**.
- **Verificato headless**: le 2 classi si caricano; `--validate` 0/0 sui dati reali; classi rotte
  (arma inesistente, abilità fantasma, nessuna primaria) → **Error + exit 1**; `--class marksman`
  risolto ("classe iniziale: marksman"); `--class` inesistente → avviso esplicito;
  **non-regressione**: senza `--class`, `--stress 6` invariato e nessun crash.
- **NON verificato — serve playtest**: che la classe **equipaggi** davvero l'arma in partita. La
  risoluzione vive in `startGame()`, raggiungibile solo confermando il PreMatch con ENTER, e gli
  input sintetici non arrivano alla finestra SDL (vincolo doc 10). Comando:
  `GFEngine.exe --direct-prematch --class marksman` → avviando la partita l'arma primaria deve
  essere la **DC-15X** (con `--class trooper`: **DC-15A**).

### Non fatto (Phase B)
- **Selettore di classe nel PreMatch**: oggi la classe si sceglie solo da `--class` o da un
  preset salvato. È il pezzo che la rende una feature per il giocatore.
- **Modulo editor "Classi"**: il doc 14 lo chiede (dropdown-only). Le classi si autorano ancora
  a mano nei JSON — ma il gate ADR-018 protegge già i riferimenti.
- **`abilityIds` della classe non ha effetto**: le abilità del giocatore non sono applicate da
  nessuno (KI #32). La classe le trasporta correttamente; manca il consumatore.

## 2026-07-15 — N3 Gate di validazione contenuti (ADR-018)

Chiude strutturalmente la classe di bug che il progetto paga da mesi: **dati sbagliati che non
falliscono, ma degradano in silenzio**, col sintomo lontano dalla causa (KI #7 near-duplicate,
#24/#26 id e fallback morti, incidente hitbox 2026-07-09). ADR-010 aveva reso strutturale la
*scrittura* sicura; questo fa lo stesso per la *correttezza*.

### Un solo posto per le regole, tre consumatori
- **`include/mini/core/Result.hpp`**: `Diagnostic` {severity, category, file, message,
  **suggestion**}. Il suggerimento non è decorazione: senza "cosa fare" una diagnostica è solo
  un altro messaggio da ignorare. Distinzione esplicita da assert (= bug di codice).
- **`game/data/ContentValidation.{hpp,cpp}`**: `validateContent(registry, dataRoot)`. Vive
  accanto a `DefinitionRegistry`, non nell'editor → entrambi i binari la linkano senza violare
  ADR-002. Legge solo il registry già caricato (nessun re-parse dei JSON); unica eccezione, i
  gate sugli asset, che devono guardare il disco.
- **Runtime**: gira dopo `loadAll()`; un Error **blocca l'avvio** con diagnostica azionabile —
  niente fallback silenzioso. **Editor**: nuovo pannello *Moduli → Validazione contenuti*
  (tabella gravità/file/problema+correzione, bottone Rivalida) che **linka la stessa funzione**.
  **Headless**: `GFEngine.exe --validate` → stampa + JSONL + **exit code ≠ 0** (niente finestra,
  niente mondo: usabile da CI e da un LLM).

### I gate (ognuno da un problema realmente occorso, non teorici)
Riferimenti risolti (`ai_profile`, `hitbox_profile`, `weapons[]`, `abilities[]`, archetipi di
mappa, veicoli) · asset mesh esistenti su disco ("modello invisibile") · armi con i campi che il
runtime consuma sensati (damage/fire_rate/bullet_speed/effective_range > 0, min_range coerente) ·
unità con hp > 0 e almeno un'arma (Warn) · mappe con geometry non vuota e command post catturabili ·
**near-duplicate sui NOMI VISUALIZZATI** — è così che si manifestò KI #7 ("DC-15A Blaster" /
"DC-15A Blaster Rifle"): gli id sono per forza diversi (= filename), è il nome che tradisce il
duplicato · missioni/obiettivi (ADR-019) · obiettivi orfani (Warn).

### Rimossa una duplicazione appena introdotta
Il gate che avevo scritto dentro `ObjectiveSystem` ieri è stato **estratto** in
`validateMission(mission, registry)`, ora usata da **entrambi**: runtime (missione attiva) e
validateContent (editor/`--validate`). Le regole vivono in un posto solo — se vivessero in due,
divergerebbero, che è precisamente il bug che ADR-018 esiste per togliere. Il debito annotato in
doc 25 è chiuso.

### Verifiche
- **Build-verified:** Debug completa (GFEngine + GFEditor), **0 errori, 0 warning**.
- **Sui dati reali del progetto: 0 errori, 0 warning, exit 0.** Un risultato verde da un
  rilevatore mai messo alla prova non vale niente, quindi il gate è stato verificato **con
  guasti deliberati** su file temporanei (i dati veri non sono mai stati toccati):
  riferimenti rotti (ai_profile/hitbox/arma inesistenti), asset mancante, arma con
  `fire_rate`/`effective_range` = 0, e due near-duplicate stile KI #7 → **6 errori, 3 warning,
  exit code 1**, ognuno con file, causa e azione. Il near-duplicate ha agganciato anche l'arma
  reale "DC-15A" già in repo: l'euristica sul prefisso funziona sul caso vero.
- **Runtime blocca**: con quel contenuto rotto, `GFEngine.exe` rifiuta di avviarsi
  ("Avvio BLOCCATO: contenuto critico invalido") invece di degradare.
- **Non-regressione**: rimossi i guasti, dati reali di nuovo validi; partita e missione partono.
- **NON verificato:** il pannello dell'editor è build-verified ma **non aperto a mano** — serve
  un tuo giro in GFEditor (Moduli → Validazione contenuti).

### Gate "campi fantasma" — aggiunto in seconda battuta (opzione (a), decisa dall'utente)
I loader registrano in `DefinitionRegistry::unknownKeys()` le chiavi che **non leggono**, mentre
il JSON è ancora in mano (dopo il parsing l'informazione non esiste più) → nessun I/O nuovo,
nessun re-parse. Gli elenchi delle chiavi note stanno **accanto al parser**: l'unico posto dove
non possono divergere dal codice che legge davvero. Copre weapons/ai/abilities/enemies/allies/
objectives/missions, primo livello. `id`/`profile_id` hanno un messaggio dedicato (non è un
refuso: è il campo che ADR-001 ignora di proposito, e che causò KI #21).
- **Verificato**: file di prova con `"fire_rat"` (refuso), `campo_obsoleto`, `id` → 3 Warning
  distinti; rimosso il file, di nuovo 0/0.
- **Trovato un caso VERO al primo colpo sui dati reali**: `data/ai/B1 Heavy Droid.json` conteneva
  `profile_id: "B1 Heavy Droid"` — residuo pre-ADR-001. Inerte oggi (il valore coincideva col
  filename e il loader lo ignora già), ma una trappola: al primo rename avrebbe mentito.
  **Rimosso** con edit chirurgico — `git diff` conferma **1 riga, nient'altro**.
  *Nota di metodo:* il primo tentativo (riscrittura via `JSON.stringify`) aveva anche convertito
  `110.0`→`110`, `12.0`→`12`, `4.0`→`4`: numericamente identico, ma è esattamente il danno
  collaterale che la regola READ-MODIFY-WRITE esiste per impedire. Ripristinato e rifatto a mano.
- **Limite documentato** (verificato sul codice): cattura le chiavi che il loader **ignora**, non
  i campi che il loader legge ma nessun sistema consuma (`min_range`, `fov_deg`, `hearing_range`
  — la lista storica di KI #25). Quelli sono un fatto sul *codice*, non sui *dati*: nessun gate
  sul registry può vederli. La formulazione originale del doc 24 confondeva i due casi — corretta.

## 2026-07-15 — N2 Framework obiettivi Phase A (ADR-019)

Primo codice del framework che sblocca la Fase 2: prima l'unico obiettivo era il command post
cablato nei mode, e "distruggi il relè" avrebbe richiesto una **modalità nuova**.

### Definizioni (solo dati, id = filename stem per ADR-001)
- `ObjectiveDef` (`data/objectives/<id>.json`): type, tier, target (zona/team/count/hold),
  activation (immediate | after_objective | after_time), time_limit opzionale, reward,
  linked_objectives. `MissionDef` (`data/missions/<id>.json`): map, mode, briefing,
  primary/optional objectives, **success_rules e failure_rules**.
- Caricati dal `DefinitionRegistry` con lo stesso pattern degli altri tipi; getter
  `getObjective`/`getMission` + accessor `objectives()`/`missions()`.
- Una regola con stringa sconosciuta **non** diventa un default silenzioso: è un dato invalido
  e la missione viene rifiutata (spirito del gate, doc 24).
- Esempio reale in repo: `firebase_ridge` (raggiungi il crinale → **poi** tienilo 8 s; assottiglia
  la guarnigione come opzionale; fallimento a tempo).

### `ObjectiveSystem` (dopo Ai/Crowd)
- Valuta lo stato quando le unità si sono già mosse nel tick. **Inerte senza missione**: i mode
  esistenti (ADR-008/009/014) continuano identici — il framework si affianca, non li riscrive.
- Tipi implementati (valutabili leggendo **solo il World**): `ReachArea`, `EliminateTarget`,
  `HoldAreaForDuration` (presenza **continuativa**: uscire azzera il progresso). Gli altri 6 tipi
  del doc 25 sono dichiarati ma **falliscono con causa esplicita** — stessa disciplina di ADR-020.
- Attivazione dichiarativa → dipendenze fra obiettivi **senza scripting**. `tier` è un campo, non
  tre sistemi paralleli. Esiti di missione dalle regole dichiarate nel MissionDef.
- **Gate**: missione senza success/failure rules valide, con un id di obiettivo inesistente, con
  un obiettivo elencato fra i primari ma di tier diverso, o senza obiettivi → **rifiutata** con
  causa (telemetria ERROR + messaggio a schermo), non avviata a metà.
- Mailbox `World::activeMission` + `World::objectiveDefs` (pattern doc 10): `ecs/` non include
  header di gioco. Nuovo flag `--mission <id>` per verificarlo headless.

### Fix collaterale: la mailbox delle uccisioni non bastava
`World::killedThisTick` portava solo l'`EntityId`, ma l'entità è **già distrutta**: un consumatore
non può più leggerne il team. `EliminateTarget` avrebbe contato **anche i propri morti**, ignorando
`target_team`. Ora la mailbox porta `{entity, team}` — il team viene letto in CombatSystem mentre
la vittima esiste ancora. (`SquadSystem` adeguato.)

### Verifiche
- **Build-verified:** Debug completa (GFEngine + GFEditor), **0 errori, 0 warning**.
- **Verificato headless** (`--stress N --mission <id>`):
  - `firebase_ridge`: 3 obiettivi + missione caricati dai dati; `mission started`; i due
    `immediate` attivati; **`hold_east_ridge` NON attivato** finché il suo prerequisito non è
    completo (dipendenza rispettata); `thin_the_garrison` **completato** contando 5 uccisioni del
    solo team 2 (filtro per team corretto).
  - **Successo di missione** end-to-end: obiettivo primario completato → `mission success`.
  - **Gate**: missione senza `failure_rules` → `mission rejected` con causa; obiettivo elencato
    fra i primari ma `tier != primary` → rifiutata con causa.
  - **Non-regressione**: senza `--mission`, **zero** eventi Objective, squadre e mode intatti,
    nessun crash.
  - Acceptance #4 (`nessun if (missionId == ...)`) verificata per grep oltre che per costruzione.
- **NON verificato:** nessuna UI di selezione missione (oggi solo `--mission`, come da doc 25 che
  mette l'authoring fuori scope) e nessun pannello HUD degli obiettivi — gli esiti passano dal feed.

## 2026-07-15 — N1 Squad & Command Phase B: ordine contestuale + HUD (ADR-020)

Il giocatore ora **comanda** la squadra: fino a ieri `Follow` era automatico e non esisteva modo
di esprimere un'intenzione.

### Comando contestuale (un tasto, nessun menu)
- Nuova `Action::SquadOrder`, default **G**, rimappabile dalle Opzioni ("Ordine squadra"). I
  binding non sono persistiti su file → aggiungere un'azione non ha rischi di dati.
- Il contesto lo decide il **mirino**: nemico inquadrato → `FocusFire`; punto a terra vicino a un
  **cover point reale del MapDef** (≤4 m, doc 15/18) → `TakeCover`; altrimenti → `MoveTo`.
  Riusa l'entità già risolta dal loop del mirino → **ciò che il mirino segna è ciò che la squadra
  riceve**, per costruzione (stessa disciplina di KI #13).
- L'intenzione passa da una **mailbox** `World::squadOrder` consumata da SquadSystem: `ecs/` non
  conosce l'input (ADR-002, doc 10).
- **Raggiungibilità verificata PRIMA di impartire** (`NavManager::findPath`): Detour restituisce un
  path **parziale** se il bersaglio è irraggiungibile — non fallisce — quindi si confronta l'arrivo
  col punto chiesto. Ordine impossibile → rifiutato subito con causa a schermo. È la trappola
  "Collina Centrale" (KI #34) resa strutturalmente impossibile da ordinare.

### Ordini nuovi
- **`TakeCover`**: MoveTo su un cover point reale; stessa condizione di completamento.
- **`FocusFire`**: vincola la **scelta del bersaglio**, non la mira — se l'AI non vede il designato
  resta autonoma (un ordine non deve farla sparare a un muro). Applicato dentro il ramo di sensing
  di `AiSystem` per non violare il time-slicing (ADR-015). **Non** vincola il movimento: l'AI
  continua a manovrare mentre concentra il fuoco.
- `Revive` (Phase C) e `Regroup` (ruota di comando) restano non eseguiti → falliscono **con causa
  esplicita**, mai in silenzio.

### Fix: un successo veniva segnalato come fallimento
`CombatSystem` **distrugge l'entità nello stesso update in cui la uccide**, e gira PRIMA di
SquadSystem: interrogare la salute di un bersaglio ucciso dà "non esiste più". Il ciclo di vita di
FocusFire riportava quindi `order failed: bersaglio non più esistente` **proprio quando la squadra
aveva fatto ciò che il giocatore aveva ordinato** — e il ramo di completamento era codice morto
(misurato: 1 failed / 0 completed).
- **Fix:** nuova mailbox `World::killedThisTick` (scritta da CombatSystem, svuotata da Application
  a fine frame come `combatFeedback`) → SquadSystem distingue "ucciso" da "sparito".
- **Dopo il fix:** 5 FocusFire emessi → **4 completati, 0 falliti**.

### HUD (doc 26: ordine, stato, distanza, esito)
- Pannello **SQUADRA** sotto gli alleati: numero membri, ordine corrente, distanza dal task —
  letto dallo **stato reale dei membri**, mai da ciò che si crede di aver ordinato. Nessuna
  distanza mostrata per FocusFire (sarebbe un numero inventato).
- Completamento e **causa del fallimento** vanno nel feed via `World::pushEvent`, annunciati **una
  volta per ordine** e non una per membro (5 alleati = 5 righe identiche).
- `orderName()` spostato in `SquadComponent.hpp`: telemetria e HUD condividono l'unica tabella.

### Verifiche
- **Build-verified:** Debug completa (GFEngine + GFEditor), **0 errori, 0 warning**.
- **Verificato headless** iniettando l'ordine direttamente nella mailbox (il tasto G non è
  testabile senza finestra — doc 10): `TakeCover` 8 emessi / 3 completati; `FocusFire` 5 emessi /
  4 completati; 0 fallimenti spuri; nessun crash; `--stress 10` pulito.
- **NON verificato — richiede playtest:** il tasto G, la risoluzione del contesto dal mirino
  (nemico vs cover vs terra), il rifiuto per irraggiungibilità e il pannello HUD. Tutta la catena
  a valle della mailbox è invece verificata sopra.

## 2026-07-15 — N1 Squad & Command Phase A + fix del pathfinding di traversata (ADR-020, ADR-017)

### Squad & Command Phase A (ADR-020, doc 26)
Primo codice per il pilastro GDD "la squadra è una risorsa, non decorazione".
- **Nuovi:** `SquadComponent.hpp` (squadId/isLeader/order/state/target/issuedTick/failureReason),
  `SquadSystem.hpp/.cpp`, registrato in `Application` fra Combat e Ai — **l'ordine dei sistemi è
  un vincolo, non uno stile** (se girasse dopo Ai gli alleati sarebbero telecomandati).
- **Mailbox** `World::playerEntity` (pattern 10_ProjectMemory): l'AI legge chi è il leader senza
  che `ecs/` dipenda dal codice di gioco. Riscritta a ogni respawn (il respawn crea un'entità NUOVA).
- Squadra alleata formata e ri-arruolata a runtime; leader = giocatore se è entità valida di team 1,
  altrimenti la prima AI alleata (in `--sim` il player è team 0 e parcheggiato fuori campo).
- Ordine di default `Follow`; ciclo di vita completo con telemetria discreta (`order issued /
  completed / failed`, doc 21). Gli ordini non ancora eseguiti (TakeCover/FocusFire/Revive/Regroup)
  **falliscono con causa esplicita**, non spariscono in silenzio.
- **Modello a guinzaglio** (vedi ADR-020 "Vincoli scoperti implementando"): l'ordine ha precedenza
  solo *fuori* dal raggio di soddisfazione; dentro, l'AI è autonoma. Il movimento è vincolato, mira
  e fuoco no — è ciò che fa funzionare il comando *durante* il firefight.

### Fix: la traversata col crowd non ha mai avuto pathfinding (ADR-017)
Trovato mentre si validava Phase A. `AiSystem` chiamava
`requestMoveTarget(et + moveDX, et + moveDZ)`, ma **`norm2D()` normalizza `moveDX/DZ` in place**:
a Detour arrivava sempre un bersaglio a **1 metro** davanti all'agente, mai la destinazione reale.
Un pathfinder a cui chiedi un punto a 1 m non può aggirare nulla — pianificava dentro l'ostacolo e
ci spingeva contro. Il commento nel codice dichiarava "i rami traversal impostano moveDX/DZ =
destinazione − posizione": **descriveva un'intenzione che il codice non implementava** (Hunt,
Search e Patrol chiamano tutti `norm2D`).
- **Fix:** nuova `moveDist` (distanza reale, default 1.0 = comportamento storico per i rami che non
  la impostano) valorizzata da Hunt/Search/Patrol/ordine; `requestMoveTarget` riceve
  `et + moveDir * moveDist` = la destinazione vera.
- **Fix correlato:** flag `orderTravel` → un ordine che fa percorrere distanza usa
  `requestMoveTarget` **anche in Alert** (il ramo Alert usa `requestMoveVelocity`, che non pianifica).

### Verifiche
- **Build-verified:** build completa Debug (GFEngine + GFEditor), **0 errori, 0 warning**.
- **Verificato headless:** `--stress 10` senza crash/assert, navmesh ok, 10 ordini emessi per 10
  alleati, 0 fallimenti.
- **Criterio di accettazione doc 26** ("una squadra sotto ordini si comporta diversamente da una
  libera") **dimostrato** con un `MoveTo` deterministico su punto raggiungibile: distanza media dal
  bersaglio **8.0 → 2.6 → 1.3 → 1.5 m** e ordini completati **0 → 1915**, contro i ~6-7 m di
  dispersione di una squadra libera.
- **Effetto sullo stuck: modesto e non concludente** — `--stress 10`, 35 → 31 eventi su singolo run,
  plausibilmente entro il rumore. Il valore del fix è la traversata a lungo raggio, non lo stuck.
- **Nota di metodo:** i primi A/B erano invalidi, non il sistema. Il bersaglio di test (0,0) è il
  centro della piattaforma **"Collina Centrale"** (10×10, alta 1 m, collider): con
  `kAgentClimb = STEP_HEIGHT` un gradino di 1 m non è scalabile → **(0,0) è irraggiungibile dal
  pavimento** e nessun `MoveTo` poteva completarsi. Gli agenti si fermavano su un anello a ~7.5 m =
  il perimetro dei cover a ±6. Chi testa il movimento su firebase deve scegliere punti liberi
  (es. **(12,0)**), non il centro mappa.
- **Da smoke-testare manualmente:** in partita vera, che gli alleati seguano il giocatore in modo
  leggibile e continuino a combattere mentre lo seguono (headless il leader è un'AI, non il player).

## 2026-07-15 — Allineamento al GDD: ponte di design + 4 sistemi pianificati (solo docs)
Sessione **di sola documentazione** (zero codice toccato), a partire dal GDD consolidato e dai
master plan engine/gioco.
- **Nuovi doc:** 23_GameDesignBridge (ponte GDD↔engine: pilastri, bestiario tattico, matrice
  ruoli armi, gerarchia GAR, i due stati persistenti), 24_ContentValidation (ADR-018),
  25_ObjectivesAndMissions (ADR-019), 26_SquadAndCommand (ADR-020), 27_Progression,
  28_Persistence (ADR-021).
- **Nuovi ADR:** 018 (gate validazione condiviso runtime/editor), 019 (framework obiettivi
  generico; command post = configurazione), 020 (SquadSystem fra AiSystem e CrowdSystem),
  021 (save di carriera: snapshot di dominio + rename atomico). Numerazione continuata da 017.
- **Drift corretto** (i doc contraddicevano sé stessi e il codice):
  - ADR-010 e ADR-011 erano marcati "Proposed" pur essendo **fatti** → Accepted.
  - 00_Vision dichiarava lo spike split-screen "non eseguito" (è del 07-09, esito (a)), i map
    metadata "non ancora implementati" (fatti il 07-10) e il rename tooling "non implementato"
    (ADR-010, fatto) → corretti.
  - 10_ProjectMemory ripeteva gli stessi tre punti stale → corretti.
  - Titoli di 15/16/17/18/19 dicevano "(Planned Feature)" pur dichiarando "Implementato" nel
    corpo → allineati.
- **Effetto architetturale:** nessuno sul codice. Sul processo: il *perché* di design ora è nel
  repo (23), e i tre sistemi che il GDD chiede e che non esistono (squadra/comando, obiettivi
  generici, classi→progressione) hanno un bersaglio preciso invece di essere impliciti.
- **Nota importante:** i master plan usati come sorgente descrivono uno stato del progetto
  antecedente al 07-09 (davano per mancanti Assault/Defense, HUD command post, spike
  split-screen e AI tattica — tutti **già fatti**). Dove divergevano, ha vinto ProjectDocs +
  codice. Le loro proposte di ADR-010..025 collidevano con la numerazione reale: ignorate.

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
