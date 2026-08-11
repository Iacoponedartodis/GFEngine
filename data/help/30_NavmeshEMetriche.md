# Navmesh e metriche

## La finestra Problemi

Il pulsante **Problemi (N)** nella barra in alto: e' l'**unico** posto dove si guardano i difetti
di una mappa. Il colore dice il grado (arancione = c'e' qualcosa che non funziona, giallo = solo
roba da valutare, normale = niente), il numero la quantita'.

Dentro ci sono **tutte e tre** le famiglie di controlli, raggruppate per tipo:

- **salute tattica e geometria** — posizioni che non coprono niente, ripiani irraggiungibili,
  fessure, ripiani troppo piccoli;
- **navmesh** — isole, posizioni e command post che il navmesh non raggiunge (dopo aver premuto
  Verifica);
- **dati e riferimenti** — lo stesso controllo di `--validate` (dopo aver premuto Controlla i dati).

I due pulsanti per rifare le verifiche stanno in cima alla finestra.

**Cliccando una voce vieni portato li'**: l'elemento si seleziona e la telecamera lo inquadra. Su
una mappa 300 x 200 "il box 147" non e' un indirizzo — senza questo, un elenco di problemi si
smette di usare al terzo che non riesci a trovare.

I gruppi con problemi si aprono da soli; quelli di soli avvisi restano chiusi. Le famiglie che
nella tua mappa sono intenzionali le chiudi una volta e non disturbano piu'.

### Due difetti che NON si vedono guardando la mappa
Sono i piu' insidiosi: la geometria e' perfetta nei dati e inesistente per l'AI.

- **Ripiani troppo piccoli.** Una superficie sopraelevata perde, prima di diventare navmesh, una
  cella di strapiombo per lato, poi 0,40 m di erosione per lato, poi le regioni sotto 2,56 m2
  vengono scartate. **Sotto 3,00 m di lato non resta niente.** Se e' sotto su entrambi i lati e'
  un problema (non ci cammina nessuno); se lo e' su un lato solo e' un avviso (puo' essere una
  passerella voluta).
- **Fessure che il navmesh non attraversa.** Due ripiani alla stessa quota separati da meno di
  0,80 m (il diametro dell'agente): l'erosione mangia i due bordi e il passaggio non si forma. Si
  vede un varco e non ci passa nessuno. Nasce accostando due box "a occhio" — a 3 cm di distanza
  sembrano attaccati. Rimedio: **Modifica → Precisione → Appoggia**.

> I gradini di scala non vengono segnalati come ripiani: hanno il loro controllo, che ragiona
> sull'**alzata**. La prima versione non li distingueva e produceva 412 segnalazioni tutte uguali.

## Verifica navmesh: cosa vuol dire "isole"

Il pulsante **Verifica navmesh** costruisce il navmesh VERO (lo stesso codice del gioco) e colora
il viewport: **verde** = ci si arriva dallo spawn alleato, **rosso** = isola. L'esito in dettaglio
sta nella finestra **Problemi**, in tre gruppi diversi — prima era tutto tradotto in "N isole",
anche quando il problema era un altro:

- **Isole** — superficie che il navmesh genera ma da cui **non si arriva allo spawn**. Ogni voce
  dice **quanti m²** e dove. E' la misura che conta: `0,4 m²` e' un angolo, `25 m²` e' una stanza
  tagliata fuori. Sotto 6 m² e' un **avviso**, sopra un **problema**.
- **Posizioni tattiche irraggiungibili** — lavoro sprecato e un buco invisibile nella copertura.
- **Command post irraggiungibili** — il caso peggiore: una missione che li chiede e'
  **incompletabile**.

### Sacche: isole a un passo dal navmesh buono
Il caso piu' frequente in assoluto, e il piu' facile da correggere. Il navmesh **si ritira di
0,40 m per lato** da ogni ostacolo: fra due cubi vicini la striscia di terreno in mezzo resta
scollegata pur essendo a un metro da dove si cammina.

Si riconoscono dalla **distanza dal navmesh buono** (la mostra `--navcheck`): sotto il metro e'
una sacca, e **basta allargare il varco di pochi centimetri** per collegarla. Molti metri invece
significa una zona davvero staccata, che vuole un accesso vero.

Su Training Ground i quattro angoli hanno ~43 m² ciascuno a **1,00 m** dal navmesh buono: 170 m²
di terreno che si recuperano allargando quattro passaggi.

### Le isole piccole agli spigoli: perche' nascono e cosa farci
Il caso tipico: una piattaforma rialzata dove arrivano due scale, e nello **spigolo** dove le due
rampe convergono resta una chiazza di pavimento scollegata.

Non e' un difetto del navmesh ed **e' inutile alzare le soglie per farla sparire**: Recast scarta
gia' da solo le regioni sotto 2,56 m², quindi una chiazza che sopravvive e' pavimento vero, largo
abbastanza da starci in piedi, da cui pero' non si esce. Nasce perche' **l'erosione toglie 0,40 m
per lato** a ogni bordo: due superfici che si toccano solo in un angolo, dopo l'erosione non si
toccano piu'.

Cosa fare, in ordine di preferenza:
1. **Allarga il pianerottolo** dove le scale incontrano la piattaforma, cosi' le due superfici si
   sovrappongono invece di sfiorarsi. E' la correzione vera.
2. **Accosta le superfici** (Modifica → Precisione → Appoggia) se il problema e' una fessura.
3. **Lasciala stare**, se sono pochi metri quadri in un angolo dove non deve andare nessuno: e'
   segnata come avviso, non come problema, proprio perche' questa e' una scelta legittima.

> **"Tutto verde ma dice 10 isole" non era una contraddizione.** Le isole si disegnano in rosso,
> ma chiazze da pochi triangoli in mezzo a migliaia di verdi non si notano. Ora ogni voce dice
> **quanto e' grande** e ti ci porta.

### Controlla i dati
Il pulsante **Controlla i dati...** sotto l'elenco esegue lo **stesso** controllo di `--validate`
e del pannello Validazione, filtrato su questa mappa e sulle strutture: riferimenti rotti, tipi di
struttura spariti, valori che il runtime ignorerebbe in silenzio. Ogni voce porta con se' **cosa
fare**, non solo cosa non va.

Gira **su richiesta** e non da solo: ricarica tutti i contenuti, e non e' un costo da pagare
mentre trascini un box.


## Perche' il navmesh non e' quello che disegni
Fra i box che disegni e le superfici su cui l'AI cammina ci sono quattro filtri, e tutti e
quattro possono far sparire una superficie che sulla carta e' perfetta:
- **erosione**: il bordo di ogni superficie perde 0,40 m per lato (il raggio dell'unita');
- **sfoltimento dei cigli**: i bordi a strapiombo vengono tolti;
- **altezza libera**: sotto 2,10 m di spazio sopra una superficie, quella superficie non e'
  calpestabile;
- **area minima di regione**: una superficie che dopo l'erosione resta sotto ~2,56 m2 viene
  scartata del tutto.

E' per questo che il controllo sui dati puo' dire "0 problemi" mentre un intero recinto e'
irraggiungibile: i dati sono corretti, e' la voxelizzazione a non produrre superficie.

## Verificare il navmesh della mappa
Nel Map Editor, **Verifica navmesh** costruisce il navmesh vero della mappa che stai editando
(box a mano piu' quelli generati dalle primitive), con lo stesso codice del gioco.

- **verde** = ci si arriva dallo spawn alleato;
- **rosso** = isola, cioe' superficie percorribile ma irraggiungibile.

Il pannello elenca poligoni, isole, e i post e le posizioni che il navmesh non raggiunge:
cliccandoli ci si sposta sopra.

Il risultato INVECCHIA da solo: appena tocchi la geometria viene marcato da rifare.

> Consiglio: accendi **Vista → Solo geometria** prima di guardare l'overlay. Con 169 posizioni
> tattiche colorate davanti, il navmesh non si legge.

## Le misure normative
Sono le misure con cui e' costruita la libreria, e i controlli le usano come riferimento:

- unita' di riferimento: 2,10 m di altezza (il "gigante" di taratura: 2,40 x 1,20)
- scalino massimo: 0,55 m — oltre, l'AI non sale
- alzata normativa: 0,20 m · pedata 0,30 m
- larghezza minima di una scala: 1,60 m
- corridoio: 2,40 m
- porta: 1,80 x 2,80 m
- muro: alto 3,20 m, spesso 0,25 m
- copertura bassa 1,00 m · alta 1,70 m
- superficie sopraelevata: almeno 3,00 m di lato (sotto, il navmesh la scarta)

## Cosa fare quando una superficie non e' raggiungibile
1. Guarda il **numero di componenti**: piu' di una significa che qualcosa e' staccato.
2. Guarda **quale box** e' muto: l'editor lo dice per nome e quota.
3. Nell'ordine di frequenza: larghezza sotto il minimo, giunzioni che si sfiorano invece di
   sovrapporsi, altezza libera insufficiente, accesso mancante.
4. Se e' una scala fatta a mano, valuta di rifarla con la primitiva **Scala**: rende
   inesprimibili tutte e tre le cause piu' comuni.

## Unita' di misura e convenzioni della quota
**Tutto l'editor e tutto il motore lavorano in METRI.** Una unita' = 1 metro, ovunque: geometria,
metriche, righello, barra di scala, celle del navmesh (0,20 m), raggio dell'unita' (0,40 m).
Non ci sono conversioni nascoste fra moduli.

Quello che NON e' uguale ovunque e' il significato della **Y**:

- un **BOX** ha la Y al **CENTRO**. Un box alto 2 m messo a `y = 0` va da −1 a +1, quindi meta'
  sta sotto terra. Per appoggiarlo a terra: `y = altezza / 2`.
- una **PRIMITIVA** (muro, scala, rampa, stanza) ha la Y alla **BASE**. Un muro a `y = 0` poggia
  a terra e sale fino alla sua altezza.
- una **PIATTAFORMA** o una **PASSERELLA** hanno la Y al **RIPIANO CALPESTABILE**, cioe' la quota
  su cui si cammina.

E' il motivo per cui accostando un box a un muro "lungo 6" si trovano piccole differenze: la
LUNGHEZZA e' identica (6 = 6), a scostarsi e' la quota. Nell'editor strutture l'etichetta del campo
lo dice — `Y (centro)`, `Y (base)`, `Y (ripiano)` — e sotto e' scritto da dove a dove arriva.

## Le misure "normative" (i campi a zero)
Molti campi valgono **0** per dire *"usa la misura normativa"*. Non e' un campo vuoto: e' una scelta.
Il campo mostra il valore reale (`normativo: 2.80`) cosi' sai da cosa parti prima di toccarlo, e
appena scrivi un numero quello sostituisce la norma.
