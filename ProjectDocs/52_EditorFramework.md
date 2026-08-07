# 52 — Audit della duplicazione nell'editor e piano di fattorizzazione

> Richiesta dell'utente (2026-08-07): *"ti avevo detto di creare delle strutture condivise tra i
> vari moduli e creare uno scheletro di funzioni che vengono richiamate, non ricostruite ogni volta
> da capo… la viewport del map editor è evidentemente molto più avanti di quella dell'editor
> strutture, o del weapon editor… ho bisogno che fai le tue ricerche, ti studi bene l'editor e
> individui tutti i casi di questo tipo"*.

## 0. Il fatto che spiega tutto

**Lo scheletro condiviso ESISTE già.** ADR-049 (2026-08-02) lo aveva deciso e implementato:
`editor/include/framework/ModuleShell.hpp` e `AssetBrowser.hpp`.

**È adottato da UN modulo su sette**: `VehicleEditor`. Che è anche, di gran lunga, il più piccolo.

| modulo | righe | usa il framework | gizmo | undo | pannelli a mano | popup a mano |
|---|---|---|---|---|---|---|
| MapEditor | **6258** | no | 6 | 25 | 9 | 14 |
| EntityEditor | 1530 | no | 3 | 0 | 6 | 1 |
| BalanceEditor | 944 | no | 0 | 0 | 12 | 0 |
| WeaponEditor | 933 | no | 1 | 0 | 5 | 1 |
| MissionEditor | 755 | no | 0 | 0 | 2 | 0 |
| ClassEditor | 348 | no | 0 | 0 | 2 | 1 |
| **VehicleEditor** | **349** | **sì** | 0 | 0 | **0** | **0** |

La migrazione si è fermata dopo il pilota, e ADR-049 lo prevedeva ("un modulo alla volta") ma
nessuno l'ha ripresa. Il risultato è esattamente quello che l'utente descrive.

## 1. L'errore nell'ADR-049 (da correggere)

ADR-049 dice: *"Il viewport resta `FreeCameraViewport`: già condiviso, non si tocca."*

**È vero per la classe e falso per la capacità.** `FreeCameraViewport` è usato da 4 moduli, ma tutto
ciò che lo rende un EDITOR vive nel chiamante:

| capacità | dove vive | chi ce l'ha |
|---|---|---|
| ray-picking (`popClickedMapBox`) | viewport ✅ | chiunque lo chiami — ma solo MapEditor lo chiamava |
| gizmo (sposta/ruota/scala) | viewport ✅ | il *wiring* è riscritto da ogni modulo |
| tradurre il delta del gizmo in modifiche | **nel modulo** | MapEditor, EntityEditor, ora il tab strutture |
| undo/redo | **solo MapEditor** | 1 modulo su 7 |
| overlay misure, viste ortografiche, "inquadra" | viewport ✅ | tutti (aggiunte ieri) |
| filtri di vista, taglio in quota | **solo MapEditor** | 1 su 7 |
| selezione multipla | **solo MapEditor** | 1 su 7 |

Il difetto segnalato oggi — *"non posso selezionare oggetti dalla viewport dell'editor strutture"* —
è la prova: il ray-picking **c'era già nel viewport condiviso**, mancava solo la riga che lo chiama.
Non era un pezzo da costruire: era un pezzo da **collegare**, e non l'avevo collegato perché ogni
modulo ricostruisce a mano il proprio strato di editing.

## 2. I casi di duplicazione, in ordine di costo

### D1 — Strato di editing del viewport (il più grave)
Selezione (picking + selezione multipla), gizmo, applicazione dei delta, evidenziazione del
selezionato. Riscritto in MapEditor, EntityEditor, tab strutture; assente in WeaponEditor e
VehicleEditor. **È la ragione per cui una viewport è "più avanti" dell'altra.**

### D2 — Undo/redo
Esiste in **un** modulo. Ogni altro modulo perde le modifiche senza rimedio. Oggi Ctrl+Z non
funziona nel tab strutture — dentro lo stesso MapEditor — perché `captureState()` fotografa la
mappa e non i tab.

### D3 — Modifiche non salvate
Anche questa era in un posto solo: chiudendo con un tipo di struttura modificato non chiedeva
nulla. **Un avviso che vale in un posto solo è peggio di nessun avviso**, perché insegna a fidarsi.
(Riparato oggi per il MapEditor; resta da generalizzare agli altri moduli.)

### D4 — Layout a tre pannelli
`ModuleShell` lo fa; sei moduli lo rifanno a mano, con 36 `BeginChild` sparsi e i difetti noti
(grip sul bordo finestra, scroll tagliato) che ricompaiono uno alla volta.

### D5 — Popup e finestre modali
14 in MapEditor, sparsi altrove, ciascuno con la sua forma. Nessun contratto comune per
"conferma distruttiva", "salva/scarta/annulla", "errore". Il difetto del modale invisibile
(changelog 164) è nato proprio da qui.

### D6 — Ciclo di vita di una definizione
`AssetBrowser` lo fa (crea/duplica/rinomina/elimina); lo usa un modulo. Gli altri reimplementano
pezzi diversi, e infatti *Elimina* mancava in 5 moduli su 7 (doc 39).

## 3. Piano di fattorizzazione

Vincolo che resta valido da ADR-049: **l'editor è GUI e non è verificabile con `--sim`**, quindi la
migrazione è **un modulo alla volta**, ognuno con la sua prova a mano. Nessun big-bang.

- **F1 — `ViewportEditing`**: componente che incapsula picking, selezione (singola e multipla),
  gizmo e applicazione dei delta, parametrizzato su un'interfaccia minima "che cosa è selezionabile
  e come lo si sposta". Adottato PRIMA dal tab strutture (piccolo, nuovo, senza eredità), poi da
  EntityEditor, infine da MapEditor.
- **F2 — `UndoStack`**: pila di snapshot generica, parametrizzata sul tipo di stato. Il MapEditor ce
  l'ha già in forma matura: si estrae quello, non se ne inventa un altro.
- **F3 — `DirtyGuard`**: chi ha modifiche non salvate, cosa sono, come si salvano. Un'unica finestra
  di uscita che interroga tutti i moduli invece del solo Map Editor.
- **F4 — `Dialogs`**: `confirmDestructive`, `saveDiscardCancel`, `errorBox` — con l'apertura e il
  disegno **nello stesso livello di ID** per costruzione, così il difetto del modale invisibile
  diventa inesprimibile.
- **F5 — migrazione a `ModuleShell`** dei moduli rimasti, uno alla volta, partendo da quelli
  piccoli (ClassEditor, MissionEditor) per validare il componente prima di toccare MapEditor.
- **F6 — `AssetBrowser`** dove manca il ciclo di vita completo.

### F1 generalizzato — e il MapEditor deliberatamente NON migrato (2026-08-07)
`ViewportEditing` ora lavora su una **selezione come insieme**, con `valid(int)` al posto di un
`count` (i codici del Map Editor non sono contigui: sono intervalli negativi per tipo) e un
`anchor(selezione)` per il baricentro del gizmo. Con questo, l'interfaccia **copre anche il Map
Editor**, e il tab strutture ha guadagnato gratis la predisposizione alla selezione multipla.

**Il confine è stato scritto dentro il componente**: instrada EVENTI, non decide SEMANTICA. Le
operazioni ricevono l'intera selezione e il modulo decide cosa significhi — perché le tre politiche
sono davvero diverse e tutte giuste:
- Map Editor: ruotando un gruppo, ogni elemento **orbita attorno al baricentro** (altrimenti un
  edificio gira sul posto pezzo per pezzo);
- tab strutture: ogni parte gira su sé stessa;
- Entity Editor: model space, e selezione per nome.

Portarle nel componente ne avrebbe fatto un contenitore di casi particolari, cioè il framework
rigido contro cui mette in guardia ADR-049.

**Il Map Editor NON è stato migrato, di proposito.** Il suo percorso del gizmo è ~80 righe con casi
particolari maturi (rotazione orbitale di gruppo, punti dei percorsi, scala limitata al singolo), è
**codice che funziona**, e **nessun collaudo automatico può confermarne la migrazione** — a
differenza dell'undo, dove un controllo esisteva ed è per quello che è andato per ultimo e senza
rischi.

**Criterio per farlo**: quando esisterà un modo di verificare il gizmo senza mouse. Fino ad allora
il guadagno (meno duplicazione in un modulo) non paga il rischio (rompere in silenzio la cosa che
l'utente usa di più). Riscrivere codice funzionante e non verificabile è il modo più elegante di
introdurre una regressione.

### Stato dell'adozione (2026-08-07, terzo giro)
- **F2 `UndoStack` — COMPLETO.** Il Map Editor l'ha adottata per **ultimo**, come previsto: il
  componente era stato estratto da lui, quindi la migrazione non cambia comportamento. Tre moduli
  su una sola implementazione (Map Editor, tab strutture, Entity Editor), dove all'inizio ce n'era
  una sola in un solo modulo. **11 controlli automatici.**
  Verificata dal collaudo che esercita proprio il percorso migrato — l'unica migrazione che un
  controllo poteva confermare da sola, ed è per questo che è andata per ultima.

### Stato dell'adozione (2026-08-07, secondo giro)
- **F3 `DirtyGuard` ✅** — **cinque moduli su sette** tenevano uno stato "modificato" e **uno solo**
  lo dichiarava. Uscendo con modifiche in Entity, Weapon, Vehicle o Balance Editor, sparivano in
  silenzio: il difetto era stato riparato per il solo Map Editor, cioè in un quinto dei casi.
  Ora l'uscita interroga tutti, elenca **cosa** non è salvato, e salva tutto ciò che sa salvarsi.
  Il Balance Editor si dichiara **non salvabile a comando** (scrive per singola definizione):
  l'uscita non offre "Salva ed esci" quando qualcuno non sa salvarsi, invece di mantenere la
  promessa a metà.
- **F4 `Dialogs` ✅** — `confirmDestructive`, `saveDiscardCancel`, `errorBox`. `OpenPopup` e
  `BeginPopupModal` stanno nella **stessa funzione**, quindi nello stesso livello di ID **per
  costruzione**: il modale invisibile del changelog 164 non è più un difetto da evitare, è
  inesprimibile. Adottati dall'uscita di GFEditor e dalla chiusura di un tab struttura.

### Stato dell'adozione (2026-08-07)
- **F2 `UndoStack` ✅ estratto**, con 10 controlli automatici. Adottato dal **tab strutture**
  (Ctrl+Z prima non faceva nulla) e dall'**Entity Editor**, che **non aveva alcun annullamento**:
  si spostava una zona hitbox col gizmo e non si tornava indietro. Costo dell'adozione: una
  dichiarazione, uno `snapshot()`, tre chiamate.
- **F1 `ViewportEditing` ✅ estratto**, adottato dal tab strutture (~50 righe di wiring sostituite
  da una dichiarazione).

### ⚠ Confine trovato: F1 NON calza su Entity Editor
Tentata l'adozione, e **respinta con motivo**. Entity Editor ha un modello d'interazione diverso:
- la selezione è per **nome** (`m_gizmoTarget` è una stringa: `"hit:2"`, `"right_hand"`), non un
  indice;
- le coordinate sono in **model space** e il delta va riportato con `deltaToLocal`;
- il picking è su **joint e marker proiettati**, non sui box del viewport — `popClickedMapBox` lì
  non ha nulla da colpire.

Forzarcelo dentro avrebbe richiesto di inventare una mappatura indice→nome, cioè di reintrodurre
l'**identità posizionale** che KI #100 ci ha già insegnato a evitare. **Il confine di F1 è "selezione
per indice su box disegnati nel viewport"**, e va detto invece di scoprirlo al terzo adottante.
Un'astrazione che copre due casi bene è meglio di una che ne copre tre male.

Una generalizzazione utile l'adozione l'ha comunque prodotta: `rotateY(int, float)` è diventata
`rotate(int, vec3)`, perché il viewport fornisce tre assi e il prossimo adottante non deve dover
modificare il componente per usarli.

### Ordine consigliato
**F1 e F2 per primi** (sono ciò che l'utente sente ogni giorno: selezionare e annullare), **F3/F4**
subito dopo perché sono piccoli e proteggono il lavoro, **F5/F6** come manutenzione continua.

MapEditor va toccato **per ultimo** in ogni fase: è il modulo che funziona meglio e quello il cui
danno costerebbe di più. Si estrae DA lui, non si riscrive lui per primo.

## 4. Metrica di successo
Non "meno righe" come obiettivo estetico, ma: **una capacità aggiunta al componente deve comparire
in tutti i moduli che lo adottano, senza toccarli.** Il controllo pratico: dopo F1, aggiungere una
funzione di selezione deve valere anche per il tab strutture e per l'Entity Editor senza
modificarli.
