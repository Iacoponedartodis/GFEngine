# 36 — Torre di controllo (coordinazione senza comando) — Planned Feature / v1

> **Stato: scope definito, v1 implementata (2026-07-21).** Direttiva dell'utente: *"la torre di
> controllo si ferma ad un livello più basso per lasciare più indipendenza, al massimo può segnalare
> i vari possibili obiettivi, ma non indirizzare i cloni in un punto specifico o dare direttamente
> ordini"*. Vedi ADR-040 e la memoria [[command-rank-system]].

## Overview
La **torre di controllo** è la struttura che dà ai cloni una **visione d'insieme**: quali settori
sono contesi, dove ci sono obiettivi. Non è il gemello repubblicano del Droide Tattico — è
deliberatamente **un livello sotto**.

La distinzione è l'intero punto del sistema:

| | Droide Tattico (droidi) | Torre di controllo (cloni) |
|---|---|---|
| Cosa pubblica | **UN** intento (`enemyCommand`) | **UNA LISTA** di segnali (`allyIntel`) |
| Effetto | tutti i droidi convergono | ogni clone **sceglie da sé** quale seguire |
| Se cade | i droidi perdono coordinamento | i cloni restano truppe autonome |

## Goal
Che i **cloni risultino più indipendenti dei droidi** — non come differenza estetica ma come
differenza di **struttura di comando**, percepibile guardando una battaglia: i droidi si muovono come
una forza diretta, i cloni come soldati informati che decidono.

## Problem Solved
Dopo ADR-037 (rimozione del Follow fisso) i cloni sono truppe indipendenti, ma **cieche**: senza
ordini si limitano alla pattuglia autorata o al vagare. Il Droide Tattico dà ai droidi una lettura
della situazione; i cloni non avevano **nessun** equivalente. Il rischio era ovvio e andava evitato
esplicitamente: dare anche a loro un comandante avrebbe reso le due fazioni la stessa cosa con
modelli diversi.

## Scope (v1)
1. **`role: "control"`** sulla struttura strategica (whitelist nel loader, combo in editor).
2. **`World::allyIntel`** (mailbox): `active` + una lista di `Signal` (posizione, raggio, peso,
   label). Popolata solo se esiste una torre di controllo **viva** della Repubblica.
3. **Sorgenti dei segnali**: settori non saldamente in mano ai cloni (peso = importanza × pressione,
   premio se in mano nemica) e **strutture nemiche vive** (peso = `priority`, premio alla torre di
   comunicazione). Le strutture arrivano dalla sorgente unica di doc 35.
4. **Scelta individuale**: un clone senza ordini e senza route consulta i segnali e ne prende **uno
   decorrelato dal proprio `bias`** — non il massimo. Poi decide **da sé** il punto dentro l'area
   (il command post catturabile più vicino, o un punto sull'anello). Due cloni con la stessa
   informazione finiscono in posti diversi.
5. **Osservabilità**: `torre_controllo`, `segnali_cloni`, `segnali_seguiti` nella telemetria
   `AI / tactical decisions` — distingue "la torre non c'è", "non ha nulla da segnalare" e "segnala
   ma nessuno la ascolta".

## Out of Scope (v1)
- **Qualunque forma di ordine**: nessun `SquadOrder`, nessuna destinazione imposta, nessun focus
  unico. Se un giorno servisse una torre che comanda, sarebbe un **altro** sistema con un altro ADR —
  non un'estensione di questo. È il vincolo che tiene in piedi la differenza fra le fazioni.
- **Effetti sull'HUD del giocatore** (marker degli obiettivi segnalati): interessante e coerente, ma
  è una scelta di UX da fare a parte.
- **Il giocatore che consulta la torre** o ne subisce la perdita: v1 tocca solo l'AI alleata.
- **Torre di controllo per i droidi**: i droidi hanno il comandante, che fa già di più.
- **Peso della perdita**: oggi se la torre cade i segnali spariscono e basta. Un degrado graduale
  (come doc 34 per le comunicazioni) è possibile ma non necessario in v1.

## Dependencies
- **ADR-037** — lo stato privo di ordini: senza truppe indipendenti non c'è niente da informare.
- **ADR-034 / doc 32** — settori e comando nemico: la controparte da cui questo sistema si distingue.
- **ADR-039 / doc 35** — strutture come intel: `World::strategicTargets` è la sorgente dei segnali
  sulle strutture.
- **ADR-036** — corpo autorabile della struttura (team, scala, rotazione, collisione, navmesh).

## Note di design
La tentazione da evitare è far scegliere a ogni clone il segnale **migliore**: sarebbe più
"intelligente" e ricostruirebbe esattamente un comando unico sotto un altro nome — tutti nello stesso
posto, cioè il difetto che ADR-037 ha appena rimosso. La scelta pesata ma **decorrelata dal bias** è
ciò che rende la torre una fonte di *informazione* invece che di *direzione*.
