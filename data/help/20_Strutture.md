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
con una viewport tutta sua che mostra la struttura DA SOLA, con due sagome di riferimento
accanto (un clone da 2,0 m e un "gigante" da 2,40 x 1,20).

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
In cima al pannello di sinistra c'e' **"Parti dell'assemblaggio"** con due pulsanti:

### + Primitiva
Aggiunge una scala, un muro, una piattaforma... Le misure restano garantite dalla primitiva.

### + Box
Aggiunge un box libero. Serve per cio' che nessuna primitiva esprime: contrafforti, parapetti
storti, feritoie, insegne. Ricordati di dargli il TIPO giusto (`floor`, `wall`, `platform`,
`cover`, `decoration`): lo leggono il navmesh e la derivazione dei metadata, e un pavimento
dichiarato "muro" non genera superficie.

Appena aggiungi una parte, il tipo diventa un ASSEMBLAGGIO e le misure si autorano parte per
parte. Le posizioni delle parti sono LOCALI: (0,0,0) e' l'origine dell'assemblaggio, e tutto
ruota insieme quando lo piazzi in mappa.

> Un assemblaggio non puo' contenere altri assemblaggi. Le parti sono primitive o box, punto.

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
Selezionando una parte nell'elenco compare il GIZMO nella viewport della struttura,
esattamente come in mappa:
- **1** Sposta · **2** Ruota · **3** Scala

Su un **box** la scala cambia le sue tre dimensioni. Su una **primitiva** agisce sulle sue
MISURE (larghezza, dislivello, lunghezza...), non su un fattore: scalare una scala del 30%
produrrebbe alzate fuori norma, cioe' proprio l'errore che le primitive rendono impossibile.
Ogni misura resta comunque bloccata al suo pavimento fisico.

Le parti nuove nascono ACCANTO a quelle esistenti, non sovrapposte: si vedono subito e si
trascinano al loro posto.

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
