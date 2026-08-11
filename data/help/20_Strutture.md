# Strutture e assemblaggi

## L'idea: si autora la RICETTA, non i box
Una scala non e' un oggetto: e' una ricetta. Dichiari "da qui, salendo di 3 m, larga 2,4" e i
gradini li genera il motore rispettando lo scalino massimo. Cosi' un'alzata sbagliata diventa
IMPOSSIBILE da disegnare, invece che un errore da scoprire dopo.

I box generati non si salvano mai: si rigenerano a ogni caricamento dalla ricetta.

## Mettere una struttura in mappa
Il pulsante **+ Struttura** apre un menu con tre parti:
- **le primitive nude** (Scala, Rampa, Muro, Muro con apertura, Stanza, Piattaforma,
  Passerella, Linea di coperture): da riparametrizzare ogni volta;
- **Libreria**: i TIPI gia' tarati e collaudati. Quelli in giallo sono marcati
  "non verificata" — si possono usare, ma nessuno ha ancora controllato che l'AI ci cammini;
- in fondo, l'accesso all'editor dei tipi.

## Creare un TIPO
**+ Struttura → "Editor strutture..."** apre un tab accanto a "Mappa",
con una viewport tutta sua che mostra la struttura DA SOLA. Le misure si leggono dalla barra di
scala, dalle coordinate ai bordi e dal righello — in numeri, non per confronto visivo.

In alto: il **nome del tipo** (come si chiamera' nella Libreria), la **primitiva** di base,
**Salva** e **Verifica**. Appena il tipo diventa un assemblaggio, il menu della primitiva di
base sparisce: li' ogni parte ha la sua, e quella "del tipo" non governerebbe nulla.

Per modificare un tipo che esiste gia': **+ Struttura → "Modifica un tipo"**.

## Tipo SEMPLICE: una primitiva con dei limiti
Nel pannello di sinistra, sotto le parti, c'e' "Misure e vincoli". Per ogni misura:
- la **spunta** decide se quella misura si potra' toccare quando la struttura e' in mappa;
- **min** e **max** la restringono ulteriormente.

Sotto ogni misura e' scritto il suo PAVIMENTO FISICO ("fisico: min 2.40 m"). Quello non si puo'
allentare: viene dai filtri del navmesh, e sotto quella soglia la struttura non genera
superficie calpestabile. Un minimo autorato piu' basso viene alzato e l'editor te lo dice.

## ASSEMBLAGGI: strutture di piu' parti
In cima al pannello di sinistra c'e' **"Parti dell'assemblaggio"**. Tutto cio' che si puo'
aggiungere sta sotto **un solo tasto: "+ Aggiungi"**, che apre una tendina con tre voci.

### + Aggiungi → Primitiva
Una scala, un muro, una piattaforma... Le misure restano garantite dalla primitiva.

### + Aggiungi → Box libero
Serve per cio' che nessuna primitiva esprime: contrafforti, parapetti storti, feritoie, insegne.
Ricordati di dargli il TIPO giusto (`floor`, `wall`, `platform`, `cover`, `decoration`): lo
leggono il navmesh e la derivazione dei metadata, e un pavimento dichiarato "muro" non genera
superficie.

### + Aggiungi → Composita
Un'altra struttura intera. Vedi "Riusare una composita dentro un'altra", piu' sotto.

Appena aggiungi una parte, il tipo diventa un ASSEMBLAGGIO e le misure si autorano parte per
parte. Le posizioni delle parti sono LOCALI: (0,0,0) e' l'origine dell'assemblaggio, e tutto
ruota insieme quando lo piazzi in mappa.

Nell'elenco delle parti ognuna ha la sua sigla: `[prim]` primitiva, `[box]` box libero,
`[rif]` riferimento a un'altra composita.

## Verificare — la parte che conta
**Verifica** costruisce il navmesh VERO sulla struttura isolata, posata su un piano neutro.
Il pannello di destra risponde a tre domande:
- **superficie persa %**: quanta della superficie che hai dichiarato calpestabile e'
  davvero raggiungibile;
- il **funnel**: box espansi → triangoli → componenti connesse. Piu' di una componente
  significa che la struttura si SPEZZA;
- **quale box** resta irraggiungibile, uno per uno.

Per un ostacolo puro (muro, porta, barricata) il criterio si rovescia: non c'e' niente da
raggiungere, si verifica che non ostruisca il passaggio.

### Perche' serve proprio sugli assemblaggi
Le parti possono essere tutte corrette e l'insieme rotto lo stesso. Un caso reale: ripiano a
3 m con la sua scala, parapetto, e un'insegna decorativa. L'insegna stava sopra l'arrivo della
scala e lasciava 1,20 m di altezza libera invece dei 2,10 necessari: il ripiano risultava
IRRAGGIUNGIBILE (2 componenti, 7,2 di 71,2 m2). Alzando l'insegna: tutto raggiungibile.

Nessun controllo sui dati poteva vederlo — le tre parti erano legali. Solo il navmesh lo dice.

## Cause tipiche quando qualcosa non e' raggiungibile
- **larghezza sotto il minimo**: il navmesh erode i bordi e scarta le regioni troppo piccole;
- **gradini che si sfiorano** invece di sovrapporsi: due superfici che devono restare connesse
  vanno sovrapposte, non accostate;
- **altezza libera insufficiente** sopra una superficie (il caso dell'insegna);
- **accessi mancanti**: una piattaforma senza lati d'accesso dichiarati e' un'isola.

## Comporre le parti nella viewport
Selezionando una parte nell'elenco compare il GIZMO nella viewport della struttura, esattamente
come in mappa. I pulsanti **Sposta / Ruota / Scala** sono in cima al viewport (le scorciatoie
1/2/3 sono state tolte: intercettavano i tasti mentre si lavora).

Su un **box** la scala cambia le sue tre dimensioni. Su una **primitiva** agisce sulle sue
MISURE (larghezza, dislivello, lunghezza...), non su un fattore: scalare una scala del 30%
produrrebbe alzate fuori norma, cioe' proprio l'errore che le primitive rendono impossibile.
Ogni misura resta comunque bloccata al suo pavimento fisico. Le misure che valgono 0 ("normativo")
partono dal loro valore reale: la pedata a 0 vale 0,30 e da li' si allunga.

Le parti nuove nascono **dove stai guardando** nel viewport, come gli oggetti nel Map Editor:
inquadra il punto dove la vuoi e premi il pulsante.

## Salvare e la marcatura "verificata"
**Verifica** salva da sola l'esito sul file e ricarica la Libreria: un tipo appena verificato
smette subito di comparire in giallo. Non serve premere Salva per quello.

**Salva** serve per le modifiche che hai fatto tu (nome, misure, vincoli, parti). Se un
salvataggio fallisce, l'errore compare in rosso accanto al pulsante e il tab resta marcato
come non salvato.

## Selezionare, annullare, categorie
- **Clic sulla parte nella viewport** per selezionarla: si evidenzia, e il gizmo si sposta su
  di lei. Funziona come in mappa, perche' e' lo stesso componente.
- **Ctrl+Z / Ctrl+Y** annullano e ripetono dentro il tab, con il proprio storico separato da
  quello della mappa. Un trascinamento intero conta come una sola operazione.
- **Categoria**: campo accanto al nome. Scrivi quello che vuoi — una categoria nuova compare da
  sola nella Libreria, raggruppata in sottomenu. Le strutture COMPOSITE hanno il segno `[+]`.
- Chiudendo l'editor con un tipo modificato l'avviso dice cosa non e' salvato, e
  "Salva ed esci" salva la mappa **e** i tipi.

## Nascondere il navmesh
Dopo **Verifica** il verde si accende da solo. La spunta **Mostra navmesh** accanto al pulsante lo
spegne senza perdere l'esito, che resta scritto nel pannello di destra. Ogni tab ha il suo: cambiando
struttura non ti ritrovi il navmesh di quella precedente.

## Fare VARIANTI di una struttura complessa
Una volta che hai una composita che funziona (una torre, un bunker), le varianti non si rifanno da
zero: si aprono e si salvano come copia.

1. **+ Struttura → Modifica un tipo → Tower**
2. modifichi quello che vuoi
3. **Salva come copia...** — scegli il nome della variante

Cosa succede esattamente:
- l'originale su disco **non viene toccato**: e' tutto il punto;
- il tab passa a lavorare sulla **copia**, quindi il "Salva" successivo scrive sulla variante e non
  sull'originale (e' la trappola classica del salva-con-nome, qui non c'e');
- la copia nasce **non verificata**, perche' la sua geometria puo' gia' essere diversa da quella che
  era stata verificata: premi **Verifica** quando sei soddisfatto.

Accanto a **Salva** il suggerimento dice sempre quale file stai per sovrascrivere: e' l'unica difesa
contro il "volevo farne una variante e ho salvato sull'originale".

## L'ORIGINE di un assemblaggio (e perche' conta)
Nel viewport della struttura vedi una **croce ciano** a terra: e' l'origine dell'assemblaggio,
il punto (0,0,0).

Non e' un dettaglio estetico. Quando piazzi la struttura in mappa, l'origine e':
- il **perno di rotazione** — la struttura gira attorno a quel punto;
- il posto dove compare il **gizmo**.

Se l'origine cade fuori dalla struttura, in mappa ruoti attorno al vuoto e ti ritrovi le frecce
lontane dall'oggetto. Sopra l'elenco delle parti l'editor te lo dice
(*"origine a 3,2 m dal centro"*) e il pulsante **Centra origine** sposta tutte le parti insieme
finche' l'origine non cade al centro. **Non cambia la forma**: cambia dove sta il perno.

Le parti nuove nascono **dove stai guardando** nel viewport, quindi se costruisci attorno
all'origine resta centrata da sola.

## Riusare una composita dentro un'altra
**+ Aggiungi → Composita** mette dentro questa struttura un'altra struttura intera, come
**RIFERIMENTO**: correggendo la torre originale, cambia anche qui e in ogni altro posto dove
l'hai usata. Non e' una copia — e' la stessa cosa, vista da piu' punti.

**Si possono riferire solo composite gia' VERIFICATE.** Le altre restano visibili nella tendina,
in grigio, col motivo scritto quando ci passi sopra: non verificata, oppure contiene questa
struttura (si annidderebbe in se stessa), oppure troppi livelli. Il limite serve perche' un
riferimento porta dentro geometria che non puoi piu' controllare da qui: almeno deve essere
geometria di cui si sa gia' che il navmesh la attraversa.

Selezionando una parte `[rif]` il pannello mostra a quale struttura rimanda, quante parti ha, e
**Apri la struttura originale** per andarci a lavorare (si apre nel suo tab).

### Isola e modifica: cambiare SOLO questa copia
Seleziona la parte `[rif]` e premi **Isola e modifica**: entri dentro quella copia, la vedi da
sola, e la modifichi pezzo per pezzo — togliendo, aggiungendo o cambiando box e primitive. In
cima compare una fascia gialla con scritto dove sei. Quando hai finito, **Fine — richiudi in un
oggetto solo**: torna a essere un oggetto unico, e nell'elenco porta un asterisco (`[rif]*`) che
dice "questa copia e' diversa dall'originale".

La struttura in libreria **non cambia**, e nemmeno le sue altre copie. Se ci ripensi,
**Annulla le modifiche** la fa tornare un riferimento puro.

### Esplodi: sciogliere il riferimento
**Esplodi** e' un'altra cosa e conviene non confonderli:

- **Isola** tiene il confine e il nome, e cambia cosa c'e' dentro *questa copia*.
- **Esplodi** scioglie il riferimento: le parti si sparpagliano dentro questa struttura, alla
  stessa identica posizione, e da quel momento non sono piu' "una copia di X" — sono parti come
  tutte le altre.

Il primo e' una variante, il secondo una demolizione. **Ctrl+Z** torna indietro da entrambi.

**Duplica** copia la parte selezionata spostandola di un metro.

## La griglia
Non finisce mai e **non si muove**: e' ancorata al mondo, con il passo fisso di 2 m e una linea
piu' chiara ogni cinque (10 m). Gli assi del mondo (rosso = X, blu = Z) restano marcati: sono
l'origine della mappa, ed e' li' che nasce l'origine di quello che costruisci.
