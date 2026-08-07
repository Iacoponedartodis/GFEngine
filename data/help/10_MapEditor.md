# Map Editor

## Cos'e' e com'e' fatto
Il Map Editor ha una barra di TAB in alto. Il primo, "Mappa", e' l'editor vero e proprio;
gli altri si aprono quando modifichi un tipo di struttura e si chiudono con la x.

Sotto la barra, da sinistra a destra:
- la LISTA degli elementi, con in cima le dimensioni della mappa e la salute tattica;
- la VIEWPORT 3D;
- le PROPRIETA' dell'elemento selezionato.

I pannelli laterali si allargano trascinando il loro bordo interno.

## Sapere quanto e' grande la mappa
In cima al pannello di sinistra c'e' sempre l'ingombro reale: `71.3 x 92.4 m` e l'escursione
di quota. Tiene conto della ROTAZIONE dei box e comprende anche i box generati dalle primitive.

> Riferimenti utili: Training Ground e' 71 x 92 m. La mappa grande pianificata e' 300 x 200.

Selezionando piu' box, nel pannello destro compare l'INGOMBRO DELLA SELEZIONE
(larghezza x profondita' x altezza), con un avviso se il lato minore scende sotto la
misura del corridoio (2,40 m).

## Le viste e come non perdersi
In cima alla viewport ci sono quattro pulsanti: **Prosp**, **Alto**, **Fronte**, **Lato**.

- **Prosp** e' la vista libera: tasto destro per guardarti attorno, WASD/QE per volare,
  Shift per andare veloce, rotella per la velocita'.
- **Alto / Fronte / Lato** sono viste ORTOGRAFICHE. Servono a MISURARE: in prospettiva una
  lunghezza sullo schermo non corrisponde a una lunghezza nel mondo. Qui si', quindi e' dove
  si lavora quando le misure contano.
  In ortografica il tasto destro SPOSTA (non ruota) e la rotella INGRANDISCE.

**Inquadra** (o il tasto **F**) riporta la camera su tutta la mappa. Usalo ogni volta che ti
perdi: e' il rimedio, non un ripiego.

Tornando in **Prosp** ritrovi la vista libera esattamente come l'avevi lasciata.

## Misurare
- **Misure ON/OFF**: barra di scala in basso a sinistra (dice quanto e' grande cio' che vedi)
  e, nelle viste ortografiche, le coordinate lungo i bordi con le linee di riferimento.
  Le coordinate non compaiono in prospettiva perche' li' la scala cambia con la distanza.
- **Righello**: due clic sul terreno misurano la distanza fra due punti QUALSIASI, anche nel
  vuoto — la larghezza di un varco, la luce di un passaggio. Si aggancia alla griglia. Il
  terzo clic ricomincia.
- Con **esattamente due elementi selezionati**, il pannello destro mostra la loro distanza e,
  se c'e' dislivello, quanti gradini servirebbero.

## Vista: nascondere quello che disturba
Il pulsante **Vista** apre i filtri. Sono solo visivi: non toccano i dati.
- per tipo di box (pavimenti, muri, piattaforme, coperture, decorazioni);
- **taglio in quota**: nasconde tutto sopra una certa altezza, e la geometria a cavallo viene
  sezionata davvero. Serve a lavorare dentro un edificio senza il tetto davanti.
- **Marcatori**: posizioni tattiche, settori e zone di pericolo, percorsi, punti di gioco.
  Il pulsante **Solo geometria** li spegne tutti: e' la vista giusta per leggere il navmesh.

Un asterisco accanto a "Vista" ricorda che qualcosa e' nascosto. Se la mappa sembra vuota,
guarda li'.

## Le liste
Le box sono raggruppate per tipo, le posizioni tattiche per ruolo, ciascuna col suo conteggio.
In cima c'e' un campo di ricerca: scrivendo, le categorie si aprono da sole e i conteggi
diventano quelli filtrati.

Nell'intestazione di un gruppo di posizioni compare anche quante sono "cieche" — cosi' vedi
da fuori dove c'e' un problema senza aprirle tutte.

## Salvataggio e sicurezza
- **Salva** scrive il file della mappa. Ogni scrittura crea un `.bak`.
- Ogni 2 minuti, se ci sono modifiche, viene scritta una COPIA DI RECUPERO in `_autosave/`.
  Non e' un salvataggio: serve solo se qualcosa va storto.
- Chiudendo l'editor con modifiche non salvate ti viene chiesto cosa fare.

## Annulla
**Ctrl+Z** annulla, **Ctrl+Y** (o Ctrl+Shift+Z) ripete. Un intero trascinamento conta come una
sola operazione. **Ctrl+A** seleziona tutto, e premuto di nuovo deseleziona.
