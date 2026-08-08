# Navmesh e metriche

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
