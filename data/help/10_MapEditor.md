# Map Editor

## Cos'e' e com'e' fatto
Il Map Editor ha una barra di TAB in alto. Il primo, "Mappa", e' l'editor vero e proprio;
gli altri si aprono quando modifichi un tipo di struttura e si chiudono con la x.

Sotto la barra, da sinistra a destra:
- la LISTA degli elementi, con in cima le dimensioni della mappa e la salute tattica;
- la VIEWPORT 3D;
- le PROPRIETA' dell'elemento selezionato.

I pannelli laterali si allargano trascinando il loro bordo interno.

> **Se stai costruendo, vai prima al capitolo "Costruire velocemente".** Disegna, tira una faccia,
> passo di griglia con Ctrl+rotella, allinea e distribuisci: e' li' che sta la differenza fra sei
> gesti a box e due.

## La barra dei comandi: dove sta cosa

I comandi sono raggruppati in quattro menu. Sono pochi apposta: una barra che cresce a ogni
funzione nuova finisce per tagliare gli ultimi comandi, e un comando tagliato **non esiste** per
chi lo usa.

| Menu | Cosa contiene |
|---|---|
| **Mappa** | Salva (**Ctrl+S**), Rinomina..., Nuova mappa... |
| **Crea** | Box, Struttura parametrica..., Disegna sulla griglia (con altezza e quota) |
| **Modifica** | Duplica, Serie..., Precisione..., Elimina |
| **Vista** | filtri per tipo, taglio in quota, figura di scala, difetti, Solido |

Fuori dai menu restano solo le cose che servono a colpo d'occhio o di continuo: **quale mappa**
stai modificando, l'**asterisco** delle modifiche non salvate, il **passo di aggancio**,
**Annulla/Ripristina**, **Prova da qui**, **Problemi (N)** e la **verifica navmesh**.

**Problemi (N)** e' l'unico posto dove si guardano i difetti della mappa: salute tattica,
navmesh e controllo dei dati insieme, raggruppati per tipo, ognuno cliccabile per andarci.
Vedi il capitolo *Navmesh e metriche*.

### Il pulsante «...»
Se restringi la finestra, i comandi che non entrano **non spariscono**: finiscono dentro un
pulsante `...` in fondo alla barra, e il suo suggerimento ti dice quanti ne contiene. Allarga la
finestra e tornano al loro posto.

### Salvare
**Ctrl+S** salva e basta — non apre nessuna finestra: compare la scritta verde *Salvato* accanto
al nome della mappa e sparisce da sola. Funziona anche dai tab delle strutture, e fa la cosa
giusta per quello che stai guardando: la mappa, il tipo di struttura, o la singola struttura
modificata.

L'**asterisco arancione** accanto al nome della mappa dice che ci sono modifiche non salvate.

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

## Strutture composite in mappa
Una composita (una torre, un bunker) e' **UN elemento**: si sposta, si ruota e si aggiorna
insieme alla sua definizione. Selezionandola, il pannello di destra non mostra alzate e pedate —
non sono sue, sono del tipo — ma quattro cose:

- **nome** e **posa** (X, Y, Z, rotazione);
- **Modifica solo QUESTA...**: apre l'editor strutture su questa sola struttura;
- **Modifica il TIPO (tutte le copie)**: apre la struttura di libreria;
- **Esplodi in parti**: la scioglie negli elementi della mappa, ognuno modificabile da solo.

### Modificare UNA copia sola
Hai quattro Tactic Bunker in mappa e su uno serve una modifica specifica. **Modifica solo
QUESTA...** apre l'editor strutture in un tab intitolato *"Tactic Bunker (solo questa)"*, con una
fascia gialla in cima che ricorda cosa stai toccando. Ci lavori come sempre — aggiungi, togli,
sposta parti — poi **Applica alla struttura**.

Da quel momento quella copia porta un **asterisco**: nell'elenco appare `[+] Tactic Bunker *`, e
nel pannello c'e' scritto "modificata solo qui". Il tipo in libreria e le altre tre copie **non
sono state toccate**: modificando il tipo, quella con l'asterisco non cambia piu'.

- **Ripristina dall'originale** butta via le modifiche e la fa tornare a seguire il tipo.
- **Promuovi a tipo di libreria...** (dentro il tab) trasforma la variante in un tipo vero, se
  scopri che ti serve anche altrove.

> Le modifiche di una copia sola stanno nel **file della mappa**, non in libreria: si salvano
> salvando la mappa.

### Le due strade, da non confondere
| Vuoi... | Comando | Effetto |
|---|---|---|
| sistemare **tutti** i bunker | Modifica il TIPO | cambia ovunque, anche nelle altre mappe |
| sistemare **questo** bunker | Modifica solo QUESTA | cambia solo qui, il tipo resta intatto |
| smontarlo del tutto | Esplodi in parti | non e' piu' un bunker: sono elementi sciolti |

### Quando esplodere, e cosa comporta
Serve per le modifiche ad hoc di UN punto: la barricata storta perche' li' c'e' una roccia.
Senza, l'unica strada era duplicare l'intero tipo in libreria per una modifica di mezzo metro.

Le parti primitive **restano primitive** (conservano ricetta e vincoli: alzate a norma,
larghezze minime), le parti che erano riferimenti restano composite. Esplodere e' un passo, non
una demolizione fino ai box.

> **Attenzione**: da quel momento il legame col tipo e' sciolto. Correggere l'originale non
> cambia piu' quelle parti. **Ctrl+Z** annulla.

### Il ritorno: raggruppare
Seleziona due o piu' elementi (Ctrl+clic, o Ctrl+A) e premi **Raggruppa in una composita...**:
crea un TIPO con quegli elementi — origine al loro baricentro — e li sostituisce con una sola
istanza. E' il modo per prendere qualcosa costruito a mano qui e renderlo riusabile ovunque.

La struttura nasce **non verificata**: aprila nell'editor strutture e premi Verifica prima di
riusarla altrove.

## Salvataggio e sicurezza
- **Salva** scrive il file della mappa. Ogni scrittura crea un `.bak`.
- Ogni 2 minuti, se ci sono modifiche, viene scritta una COPIA DI RECUPERO in `_autosave/`.
  Non e' un salvataggio: serve solo se qualcosa va storto.
- Chiudendo l'editor con modifiche non salvate ti viene chiesto cosa fare.

## Annulla
**Ctrl+Z** annulla, **Ctrl+Y** (o Ctrl+Shift+Z) ripete. Un intero trascinamento conta come una
sola operazione. **Ctrl+A** seleziona tutto, e premuto di nuovo deseleziona.
