# 34 — Rete di comunicazione (torri di comunicazione) — Planned Feature / v1

> **Stato: scope definito, v1 in implementazione (2026-07-20).** Sistema nuovo, nato da una
> direttiva esplicita dell'utente: *"le torri di comunicazione se distrutte non devono bloccare i
> rinforzi, però senza torre di comunicazioni tutte le informazioni, gli ordini e i rinforzi vengono
> rallentati"*. Vedi ADR-038 per la decisione e le memorie [[command-rank-system]] /
> [[design-coherence-principle]].

## Overview
La **torre di comunicazione** è il nodo che tiene insieme la catena di comando di una fazione.
Finché è in piedi, quella fazione comunica bene: un avvistamento si propaga lontano e subito, gli
ordini dall'alto arrivano rapidi, i rimpiazzi sono organizzati. Quando cade, **niente si spegne** —
tutto **rallenta e si accorcia**. È una struttura per **entrambe le fazioni** (Repubblica e
Separatisti), autorata sulla mappa con lo stesso schema (ADR-036: team, scala, rotazione,
collisione).

## Goal
Rendere la comunicazione una **risorsa tattica che si può attaccare**, e far sentire la differenza
fra un esercito connesso e uno che combatte a pezzi — senza mai togliere al giocatore o all'AI la
possibilità di continuare a combattere.

Il criterio di riuscita è percettivo, non numerico: dopo aver perso la torre, la propria fazione
deve *sembrare* più lenta a reagire — i compagni lontani non accorrono, gli ordini arrivano quando
la situazione è già cambiata, i rimpiazzi tardano.

## Problem Solved
Oggi la distruzione di una struttura è un **interruttore**: la conseguenza `block_enemy_reinforcements`
azzera i rinforzi (dato autorato in `hold_alpha.json`). Un interruttore ha due difetti:
1. **È binario e definitivo** — una volta scattato la partita è decisa, non resa più difficile.
2. **Non è coerente** né con Star Wars né con eserciti reali: perdere le comunicazioni non impedisce
   ai rinforzi di arrivare, li rende **disorganizzati e in ritardo**
   ([[design-coherence-principle]]).

Inoltre la qualità della comunicazione è oggi una **costante globale**: `AI_CONTACT_SHARE_RADIUS` è
lo stesso per tutti, sempre, e un avvistamento si propaga **istantaneamente**. Non esiste alcun modo
per cui una fazione comunichi meglio dell'altra, né per cui la comunicazione peggiori nel tempo.

## Scope (v1)
1. **Ruolo autorato sulla struttura.** `StrategicTargetDef` guadagna `role` (`"generic"` |
   `"comms"`). Solo le strutture con `role: "comms"` alimentano la rete della loro fazione.
2. **Stato di comunicazione per fazione** (`World::comms`, mailbox): dice se la fazione **aveva** una
   torre e se **ne ha ancora una viva**, e da lì quattro moltiplicatori — raggio di condivisione,
   ritardo dell'informazione, periodo di decisione del comando, ritardo dei rimpiazzi.
3. **Regola di non-regressione**: una fazione che **non ha mai avuto** una torre autorata comunica
   **normalmente**. Il degrado si applica solo a chi una torre l'aveva e l'ha persa. Le mappe
   esistenti non cambiano comportamento.
4. **Tre effetti concreti, uno per ciascun tipo di flusso citato dalla direttiva:**
   - **Informazioni** — il raggio entro cui un avvistamento si propaga si **riduce**, e
     l'informazione arriva **in ritardo**: i compagni convergono su dove il nemico *era*, non dove è.
   - **Ordini** — il comandante (Droide Tattico, doc 32) ri-decide la direttiva **più di rado**: i
     droidi eseguono più a lungo un intento ormai vecchio.
   - **Rinforzi** — i rimpiazzi **tardano** (moltiplicatore sul timer di respawn). **Non vengono mai
     bloccati.**
*(Un quinto punto — un consequence type `degrade_comms` in `25_ObjectivesAndMissions` — era previsto
in prima stesura e poi **tolto dalla v1**: vedi Out of Scope.)*

## Out of Scope (v1)
- **Effetto positivo della torre oltre il nominale**: la torre non *migliora* la comunicazione sopra
  la baseline, la sua perdita la peggiora. Un bonus attivo (raggio esteso, ordini più rapidi del
  normale) è una scelta di bilanciamento successiva, da fare quando esisterà il sistema di gradi.
- **Consequence type `degrade_comms`** (un obiettivo che degrada le comunicazioni nemiche senza
  passare da una torre). Era nello scope iniziale, **tolto in fase di implementazione**: lo stato
  della rete si ricalcola dal vivo dalle torri ancora in piedi, quindi una conseguenza che scrivesse
  lo stesso stato creerebbe **due sorgenti di verità** che si sovrascrivono a vicenda. Se servirà,
  va progettato come un modificatore *separato* che si compone con quello delle torri, non come una
  seconda scrittura sullo stesso campo.
- **Riparazione / riconquista della torre.**
- **Torre di controllo** (coordinazione e visione d'insieme per i cloni): è un sistema **diverso**,
  con una responsabilità diversa — vedi [[command-rank-system]]. La torre di comunicazione riguarda
  la **qualità del canale**, la torre di controllo riguarda **cosa passa nel canale**. Non vanno
  fusi.
- **Comunicazione per-unità** (radio individuali, unità isolate che perdono il contatto a
  prescindere dalla torre): la v1 lavora a livello di fazione.
- **Effetti sul giocatore** (HUD degradato, marker dei compagni in ritardo): la v1 tocca solo l'AI e
  i rimpiazzi. È l'estensione più interessante, ma richiede decidere quanta informazione l'HUD deve
  perdere senza diventare frustrante.

## Dependencies
- **ADR-036** — le strutture strategiche sono solide, autorate e per fazione (corpo della torre).
- **ADR-020 / doc 26** — Squad & Command (gli ordini che vengono rallentati).
- **ADR-024 / doc 32** — Comando nemico (il comandante che ri-decide più di rado).
- **doc 25 ObjectivesAndMissions** — i consequence type e la conseguenza `block_enemy_reinforcements`
  che questo sistema **sostituisce** come effetto della torre.
- **`AI_CONTACT_SHARE_RADIUS`** (`core/GameConfig.hpp`) — la baseline che i moltiplicatori scalano.

## Note di design
La direttiva dell'utente contiene un principio che vale oltre questo sistema: **una struttura
distrutta non deve rimuovere una capacità, deve degradarla**. Un interruttore chiude la partita; un
degrado la rende più difficile e lascia spazio a giocarla. Quando in futuro si aggiungeranno altre
strutture (generatori, hangar, centri di controllo), la domanda giusta sarà "cosa **rallenta** o
**riduce** la sua perdita", non "cosa **blocca**".
