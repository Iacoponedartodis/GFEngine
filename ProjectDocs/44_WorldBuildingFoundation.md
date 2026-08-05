# 44 — Fondazione del mondo: geometria, metadata derivati, generazione (Planned Feature)

> **Stato: PIANIFICATO — zero righe di codice.** Documento di scope (CLAUDE.md §5).
> L'utente lo ha definito *"una delle cose più importanti dell'intero gioco"*, da fare
> **prendendosela con calma, progettando e pianificando in maniera scrupolosa**. Questo documento
> serve a rendere quella pianificazione discutibile prima che diventi codice.

## Overview

Oggi una mappa è: **box di geometria autorati a mano** + **metadata tattici autorati a mano** (169
posizioni su Training Ground, messe una per una).

> **Correzione ai numeri (2026-08-04)**: avevo scritto *"una mappa di prova da 50×40"*. **È falso**,
> verificato sulla geometria: Training Ground è **71,3 × 92,4 m, quota massima 15,5 m**. Non è una
> mappetta. E il dato che ne esce è più severo, non meno: **169 posizioni su 6.595 m² sono una
> posizione ogni 39 m²**. Il mondo è quasi vuoto di significato — coerente con ciò che vediamo, cioè
> AI che non sanno cosa farci con lo spazio.

Non regge l'obiettivo dichiarato: mappe **grandi, profonde, con più fronti e obiettivi**, e nemmeno
il lavoro di rifinitura dei metadata, perché **rifinire a mano 169 posizioni è già al limite** e su
una mappa più grande diventa impossibile.

Il salto da fare è di natura, non di quantità: **la maggior parte dei metadata deve essere
DERIVATA** dalla geometria, dall'ambiente e dalla posizione — non scritta a mano. All'autore resta
l'intento (*"questo è un obiettivo, questo fronte conta"*), alla macchina il resto.

## Perché adesso, e non dopo gli ordini

L'utente ha proposto quest'ordine e i dati lo confermano: gli **ordini** (ruota, ordini rapidi,
mappa tattica) operano *su* settori, posizioni, obiettivi e priorità. Costruirli sopra metadata
scarni significa progettare l'interfaccia attorno ai limiti attuali e poi rifarla. Prima la
fondazione, poi ciò che ci si appoggia.

## Problem Solved — e sono problemi MISURATI, non ipotesi

1. **La geometria non dice la verità sulla percorribilità.** Su Training Ground le scale esistono
   come box impilati, ma con alzate di **0,68-1,21 m** contro uno `STEP_HEIGHT` di **0,55**: il
   navmesh non le collega, le piattaforme sono isole, e un ordine MoveTo risponde correttamente
   *"posizione irraggiungibile"* (KI #95). L'autore le ha disegnate credendole scale.
2. **Nessuno strumento lo diceva.** Il difetto è stato trovato guardando le AI girare in tondo, non
   dal gate — la guardia è arrivata solo il 2026-08-04, dopo mezza giornata di indagine.
3. **I metadata sono tutti autorati a mano**, quindi pochi e statici: `importance`, `protection`,
   `facing`, `arco`, `gittata`. Ciò che il mondo "sa" (chi copre chi, esposizione, copertura
   dall'alto) è già derivato ed è **la parte che funziona meglio** — è la prova che la direzione è
   giusta.
4. **Il map building non scala.** Nessun blocco riusabile oltre i prefab, nessuna scala/rampa come
   primitiva, nessuna verifica di percorribilità mentre si costruisce.

## Scope — quattro blocchi, in quest'ordine

### W1 — La geometria diventa PERCORRIBILE per costruzione
> **PIANIFICATO IN DETTAGLIO: [47_MapBuildingPlan.md](47_MapBuildingPlan.md)** (2026-08-04), con
> ADR-053 (primitive parametriche che si espandono in box). Include anche gli strumenti dell'editor
> senza cui una mappa da 1.520 box non è costruibile (undo, selezione multipla, array, livelli) e la
> tabella di **metriche normative** che oggi non esiste. Fasi G1-G8.

Il problema #1 è che si può disegnare una scala che non è una scala.
- **Primitive di costruzione**: scala e rampa come oggetti autorabili (alzata/pedata/larghezza),
  che *generano* i box rispettando `STEP_HEIGHT` — impossibile sbagliare l'alzata.
- **Verifica in tempo reale nell'editor**: mentre costruisci, i ripiani senza accesso si colorano.
  La guardia esiste già lato dati (`UnreachablePoint`); va portata nel viewport.
- **Verifica di CONNETTIVITÀ vera** (navmesh, non dati): "dallo spawn si raggiunge X?" — il limite
  dichiarato della guardia attuale, che vede "c'è terreno" ma non "è connesso".
- *Decisione aperta*: alzare `STEP_HEIGHT` non è la risposta (0,9 m non è un gradino, è un salto, e
  cambierebbe il movimento ovunque). Semmai serve un **salto/arrampicata** come capacità dichiarata.

### W2 — I metadata si DERIVANO
> **PIANIFICATO IN DETTAGLIO: [46_MetadataPlan.md](46_MetadataPlan.md)** (2026-08-04). Ricerca in
> [45](45_MetadataResearch.md), piano operativo in 46. La decisione centrale — **substrato a tre
> livelli** (griglia d'influenza / poligoni navmesh / posizioni tattiche), con fasi M1-M7 e criteri
> di accettazione verificabili — è lì. Ciò che segue resta come elenco d'intento originario.

Il cuore del blocco. Oggi derivati: `positionCovers`, `positionExposure`, `hasOverheadCover`.
Da derivare (elenco iniziale, da discutere):
- **copertura**: altezza, direzioni protette, se è peek-over o peek-around → dalla forma del box;
- **valore posizionale**: quanto terreno si domina, da quante direzioni si è esposti, quanto si è
  vicini a un collo di bottiglia;
- **chokepoint e stanze**: dalla topologia dello spazio percorribile (B7/B8 di doc 41);
- **vie d'accesso**: per ogni area, da dove ci si arriva — è ciò che permette all'AI di ragionare su
  "aggirare" invece che su "andare verso";
- **quota e dominanza**: chi vede cosa da sopra.
Regola: **derivato = ricalcolato al load, mai salvato** (ADR-033) — non può diventare stantio.
All'autore resta ciò che la macchina non può sapere: **l'intento** (obiettivi, importanza di un
fronte, danger zone narrative).

### W3 — GENERAZIONE automatica delle posizioni tattiche
Con W2 in piedi, le 169 posizioni a mano diventano un **suggerimento automatico** che l'autore
corregge. Non generazione di mappe: **generazione di metadata su geometria autorata**.
Verifica pronta: la Salute Tattica confronta generato vs autorato e dice cosa cambierebbe.

### W4 — La mappa GRANDE — **e va fatta PRIMA di W2/W3** (correzione dell'utente, 2026-08-04)
> Avevo messo la mappa grande per ultima, dopo i metadata. **L'utente ha corretto, e ha ragione**:
> la mappa va costruita **prima**, non per giocarci — sarebbe priva di dati — ma per **avere il
> banco su cui sviluppare la derivazione**. Non si può progettare "quali dati ricavare dalla
> geometria" avendo come unico riferimento una mappa 50×40 con 167 box: si finirebbe per derivare
> ciò che serve a *quella*, e scoprire i casi veri solo dopo.
>
> Ordine corretto: **W1 → W4 (mappa grande, geometria sola) → W2 (derivazione, sviluppata e testata
> sulla mappa grande) → W3 (generazione)**.

Una mappa più grande e complessa di Training Ground, costruita con gli strumenti di W1. Serve a
tre cose: dare casi reali alla derivazione (dislivelli veri, stanze, corridoi, coperture di forme
diverse), mettere sotto sforzo i costi di calcolo, e verificare che la generazione automatica
produca risultati **coerenti e realistici** su uno spazio che nessuno ha annotato a mano.

## Out of Scope

- Ordini rapidi, ruota, mappa tattica: **dopo**, e per scelta dell'utente. Si appoggiano a questo.
- Generazione procedurale di mappe intere.
- Modelli e animazioni: procedono **in parallelo** lato utente, senza dipendenze reciproche.

## Dependencies e rischi

- **Osservabilità**: già pronta (ADR-050). Ogni passo qui va misurato col funnel di navigazione, la
  salute tattica e la scatola nera — sono gli strumenti che hanno trovato tutti i difetti di questa
  fase.
- **ADR-047** (box = verità tattica, Blender = visivo): W1 lo rende *verificabile* invece che
  raccomandato. Il caso delle scale visive senza box è esattamente la sua violazione.
- **Rischio principale — sovra-derivazione**: derivare troppo produce metadata che l'autore non
  capisce e non può correggere. Ogni dato derivato deve essere **ispezionabile** e
  **sovrascrivibile** a mano.
- **Rischio secondario**: costo di calcolo al load. `buildTacticalLinks` è già O(n²) su 169
  posizioni; con generazione automatica il numero cresce. Va misurato (l'inventario di avvio lo
  fa già) prima di allargare.

## Prima cosa da fare quando si parte — **FATTA (2026-08-04)**

Non codice: decidere l'elenco dei dati derivati di W2. **Fatto**: ricerca in doc 45, piano completo
in **doc 46**. Restano tre scelte all'utente (doc 46 §12): quanto grande la mappa grande, fin dove
arrivare in questo giro (M1-M4 o fino a M7), e se le posizioni generate servono davvero.

**Il passo successivo è il piano di MAP BUILDING (doc 47)** — W1 + gli strumenti che servono
all'utente per costruire la mappa grande. Poi l'utente costruisce, e solo dopo si implementa doc 46.
