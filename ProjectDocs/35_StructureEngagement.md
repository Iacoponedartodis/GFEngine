# 35 — Interazione AI ↔ strutture (bersagli come dato tattico) — Planned Feature / v1

> **Stato: scope definito, v1 implementata (2026-07-20).** Nasce da KI #70 e dalla direttiva
> dell'utente: *"il modo in cui le AI si relazionano con strutture e bersagli va autorato per bene,
> per adesso magari intanto crea il sistema per farli interagire... e il sistema per autorare
> l'interazione, poi ci penso io ad autorarlo quando ho tempo"*. Con la nota decisiva: **queste
> informazioni devono essere considerate anche dalla torre di controllo e dal Droide Tattico.**
> Vedi ADR-039.

## Overview
Una struttura (torre di comunicazione, generatore, bersaglio di missione) oggi esiste come **corpo
solido colpibile** (ADR-036) e, se è una torre di comunicazione, come **nodo di rete** (doc 34). Ciò
che manca è il terzo lato: **una struttura è anche un fatto tattico** — qualcosa che le AI possono
decidere di attaccare, e che i livelli di comando devono poter *leggere* quando scelgono dove
concentrare la forza.

Questo documento definisce **come si autora quel valore tattico** e **come le AI lo usano**.

## Goal
Che una struttura sia un obiettivo **vivo** nella battaglia e non solo un bersaglio che aspetta il
giocatore: le AI la ingaggiano quando ha senso, e il comando la considera fra le opzioni.

Il criterio di riuscita è che l'autore della mappa possa dire, **come dato**, quanto una struttura
conta e quanto le truppe devono essere disposte a deviare per attaccarla — senza toccare codice.

## Problem Solved
Tre difetti distinti, che si sommavano:
1. **Le strutture erano invisibili alle AI** (KI #70). Non per scelta: `physics::hasLineOfSight`
   scorreva **tutti** i collider senza escludere il bersaglio, quindi il collider aggiunto da
   ADR-036 **bloccava la visuale verso il centro della struttura stessa**. Nessuna AI poteva
   acquisirla. Le uniche torri distrutte in `--sim` cadevano per **fuoco vagante**.
2. **Nessun valore tattico autorabile**: una struttura non aveva modo di dichiarare quanto vale
   distruggerla, né a che distanza vale la pena deviare per farlo.
3. **Nessuna sorgente unica** che i livelli di comando potessero leggere. Il Droide Tattico ragiona
   sui settori (ADR-034); le strutture non entravano nella sua lettura della situazione, e la futura
   torre di controllo dei cloni avrebbe dovuto ricostruirsi le stesse informazioni per conto suo.

## Scope (v1)
1. **`hasLineOfSight` accetta un'entità da ignorare** — il bersaglio non si occlude da sé.
   Prerequisito: senza questo, nulla del resto può funzionare.
2. **Due campi autorati** su `StrategicTargetDef`:
   - **`priority`** (0..1, default 0.5) — quanto la fazione avversaria vuole vederla distrutta.
     È il valore che i livelli di comando confrontano fra loro e con i settori.
   - **`engage_radius`** (m, default 0) — entro quanto un'unità avversaria la ingaggia **di propria
     iniziativa**. **`0` = mai spontaneamente**: la struttura è affare del giocatore e del comando.
     Il default è volutamente `0`: aggiungere questo sistema **non deve cambiare** il comportamento
     delle mappe già autorate. È l'autore a decidere di accenderlo.
3. **`World::strategicTargets` diventa la sorgente unica di intel sulle strutture**: ogni voce porta
   posizione, fazione, ruolo, priorità, raggio di ingaggio e se è ancora viva. La leggono l'AI, il
   comando nemico e — quando esisterà — la torre di controllo dei cloni. **Una sola lista.**
4. **Ingaggio opportunistico** (AiSystem): un'unità che **non ha un bersaglio-unità valido** e ha una
   struttura nemica entro `engage_radius` con linea di tiro, la ingaggia. La precedenza alle unità
   non è negoziabile: una struttura **non spara**, e preferirla a chi ti sta sparando sarebbe
   semplicemente stupido.
5. **Il comando considera le strutture** (doc 32): nella scelta dell'obiettivo, una struttura nemica
   viva entra in concorrenza con i settori, pesata da `priority`. Il comandante indica **dove**
   concentrarsi, non spara e non ordina il singolo colpo — resta valida la correzione dell'utente
   ("non deve diventare un cervello unico al posto delle singole AI").

## Out of Scope (v1)
- **Ordine di squadra "distruggi quella struttura"** dal giocatore (ruota/comando contestuale): il
  sistema di ordini esiste già, ma aggiungere un ordine è una scelta di UX da fare a parte.
- **Restrizioni per classe/ruolo** ("solo i demolitori attaccano le strutture"): richiede il sistema
  di classi in gioco su questa dimensione, e soprattutto una decisione di design. Il campo che la
  renderà possibile (`priority`) esiste già; il filtro no.
- **Strutture che reagiscono** (torrette, contraerea): un'altra cosa, con altro ADR.
- **Danno strutturale differenziato** (armi anti-struttura vs anti-fanteria).
- **Autorazione dei valori sulle mappe reali**: l'utente ha detto esplicitamente che ci pensa lui.
  Qui si consegna lo **strumento**, non il contenuto — su firebase i valori restano ai default
  conservativi.

## Dependencies
- **ADR-036** — la struttura come corpo solido autorato (è il collider di lì ad aver causato KI #70).
- **doc 34 / ADR-038** — rete di comunicazione: la torre di comunicazione è il primo caso d'uso reale
  di una struttura che vale la pena attaccare.
- **ADR-034 / doc 32** — settori e comando nemico: è lì che la priorità delle strutture entra in
  concorrenza con il resto.
- **doc 25** — obiettivi `destroy_target`: continuano a funzionare come prima, per label.

## Note di design
`engage_radius = 0` come default è la scelta centrale di questo documento. Un sistema che si accende
da solo su tutte le mappe esistenti avrebbe cambiato battaglie già bilanciate senza che nessuno
l'avesse chiesto. Così invece lo strumento è pronto e **inerte** finché l'autore non lo usa — che è
esattamente ciò che l'utente ha chiesto ("poi ci penso io ad autorarlo quando ho tempo").
