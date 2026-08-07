# 51 — Audit dell'editor prima della mappa grande (2026-08-06)

> Richiesta dell'utente: non solo eseguire i suoi esempi, ma *"informandoti, confrontando il nostro
> progetto e analizzandolo capisci se ci sono effettivamente aspetti su cui dovremmo lavorare prima
> di procedere con la mappa"*. Questo documento è quella parte: **cosa ho trovato io**, non cosa mi
> è stato indicato.

## 0. Il rilievo che conta più di tutti

**Il Map Editor non aveva né salvataggio automatico né un avviso all'uscita.** Chiudere la finestra
buttava via le modifiche non salvate **in silenzio** — e non serviva nemmeno un crash: bastava la X.
Nel frattempo **KI #98 (crash di Entity Editor) è ancora aperto**.

Stai per costruire una mappa 300 × 200 in più giorni. Questo era, di gran lunga, il rischio più
grosso dell'editor — più di qualunque lentezza, e nessuno degli esempi che mi hai fatto lo toccava.

**Riparato (changelog 169)**: copia di recupero ogni 2 minuti in `_autosave/` (fuori da `data/maps/`,
o diventerebbe una mappa fantasma nell'elenco) e finestra **Salva ed esci / Esci senza salvare /
Annulla** su ogni via d'uscita. La copia di recupero **non** azzera lo stato "modificato": non è un
salvataggio, e far credere il contrario sarebbe peggio del problema.

Nota a favore di com'era già fatto: `saveJsonRMW` crea un `.bak` a ogni scrittura, quindi un
salvataggio andato male era già recuperabile. Mancava la protezione dal *non salvare affatto*.

## 1. Scalabilità: misurata, non temuta

`updateViewport()` ricalcola l'esposizione tattica a **ogni modifica** (`recomputeDerived = true` di
default), incluso **ogni frame mentre si trascina col gizmo**, e `buildTacticalLinks` è **O(n²)**
sulle posizioni. Misura vera, aggiunta al self-test e ripetibile (`--editor-selftest`):

| posizioni | costo di UNA modifica |
|---|---|
| 169 (Training Ground) | 0,7 ms |
| 500 | 3,5–4,9 ms |
| 1000 | 9,5–14,6 ms |
| 1500 (stima per 300 × 200) | **21–34 ms** |

**Verdetto: non è un blocco.** A 1500 posizioni un trascinamento gira a ~30-45 fps: si sente, non
impedisce. Diventa un problema **oltre**, e la curva è quadratica (3000 posizioni ≈ 4× → ~100 ms).

**Rimedio quando servirà, in ordine di costo**: (a) non ricalcolare *durante* il trascinamento e
farlo una volta al rilascio — toglie l'intero costo per-frame, che è il caso peggiore; (b) griglia
spaziale in `buildTacticalLinks`, che è comunque già in programma (doc 46 M0-bis, "deve smettere di
essere O(n²) prima di M7").

## 2. Interfaccia — i difetti segnalati dall'utente, e cosa c'era sotto

- **Le scritte tagliate nei pannelli stretti.** Riparato alla radice: `editor::ui::sliderRow` — usata
  in **178 punti** — ora si **riorganizza** invece di tagliare. Sotto la larghezza utile mette
  l'etichetta sopra e i controlli sotto, e la larghezza dell'etichetta è quella **vera**
  (`CalcTextSize`) invece di un 58 px fisso che con nomi lunghi tagliava comunque.
  In Dear ImGui non esiste layout a vincoli: il ramo va scritto a mano interrogando
  `GetContentRegionAvail` — quindi la leva giusta è il widget condiviso, non i moduli.
- **Il pannello del Weapon Editor non si allargava.** Causa: `ImGuiChildFlags_ResizeX` mette il grip
  sul bordo **destro**, che per un pannello di destra coincide col bordo della finestra — una volta
  stretto non c'è più niente da afferrare. **La riparazione esisteva già nel Map Editor** ma non era
  stata propagata: ora è un helper condiviso, `editor::ui::panelSplitter`. Verificati tutti gli altri
  usi di `ResizeX`: sono su pannelli di sinistra/centro, dove il grip è interno e funziona.
- **Strumento per l'interfaccia** (menu **Aspetto → Interfaccia**): dimensione del testo e densità,
  con effetto immediato e conservati alla chiusura. Copre *"scritte che non si vedono"* e, in parte,
  *"tasti troppo in mezzo"* (a densità 0,8 entra molto di più senza tagliare nulla).
  **Limite dichiarato**: si conservano **solo** testo e densità, non l'intera `ImGuiStyle`. Scaricare
  quella struct su file **non è affidabile fra versioni** della libreria (ocornut/imgui #8659, #101),
  e un file di preferenze che si rompe a ogni aggiornamento è peggio che non averlo. Colori e
  arrotondamenti si regolano dal vivo con l'editor integrato e, se una combinazione convince, si
  esporta e la si rende predefinita.
- **Aggiunto `imgui_demo.cpp`** al build: non è "la demo", è dove vivono `ShowStyleEditor` e
  `ShowMetricsWindow` — quest'ultima contiene l'**ID Stack Tool**, cioè lo strumento che diagnostica
  esattamente i conflitti di identificatore che ci sono già costati un modale invisibile
  (changelog 164). Menu **Aspetto → Diagnostica ImGui**.

### Cosa NON copre lo strumento aspetto
**Spostare i comandi.** Un editor di stile regola densità, testo e colori; non decide che il pulsante
"Serie" stia altrove. Per quello servirebbe una **barra configurabile** (quali comandi, in che ordine,
in quali gruppi) — è fattibile ed è una feature a sé, da decidere se vale. Segnalarlo è più onesto che
lasciar credere che il pannello aspetto risolva anche quello.

## 3. Cosa resta aperto, in ordine di rischio

| | stato |
|---|---|
| **KI #98** — crash di Entity Editor | ⚠ aperto. Non riproducibile a comando; ora la rete di diagnosi funziona (fase + simboli, verificata con `--crash-test`). **Serve un'occorrenza reale.** |
| **KI #100** — identità posizionale, tetti dei codici | guardato (avviso a 80% e al tetto), causa non rimossa |
| Costo quadratico per modifica | misurato, sopportabile a 1500, rimedio noto |
| **C1 — assemblaggi** (ADR-056) | deciso, non implementato: è il prossimo lavoro |
| Barra comandi configurabile | non decisa |
| Griglia etichettata in ortografica | non decisa (l'inquadratura in metri + righello coprono il caso d'uso) |

## 4. Cosa consiglio prima di iniziare la mappa

1. **Provare l'editor per un'ora** con quello che c'è ora: viste ortografiche, righello, dimensioni,
   liste raggruppate, densità. Sono strumenti: si giudicano usandoli, e io non vedo lo schermo.
2. **Poi C1 (assemblaggi)**, che è il pezzo grosso mancante e ora ha una decisione architetturale
   alle spalle (ADR-056).
3. Il resto (barra configurabile, ottimizzazione del ricalcolo, griglia etichettata) **solo se
   l'uso reale li richiede**. Sono tutti rimedi a problemi che oggi sono misurati come sopportabili,
   e anticiparli significherebbe indovinare invece di osservare.
