# 26 — Squad & Command (Planned Feature)

**Status: Planned Feature — non ancora implementato in codice.**
Descrive un sistema che NON esiste ancora: oggi non c'è alcun concetto di squadra né di ordine.
Non trattare nessun nome come presente finché 05_CurrentState non lo conferma. Vedi ADR-020.

## Overview
Il sistema che rende vero il pilastro #4 del GDD: **la squadra è una risorsa, non decorazione.**
Il giocatore esprime un'**intenzione**; l'AI la esegue con autonomia. Il comando deve funzionare
*durante* gli scontri, non in un menu separato.

## Problem Solved
Questo è **il sistema più importante che manca al gioco**, e l'unico pilastro del GDD senza
alcuna traccia nel codice. Senza di esso Galactic Front resta uno sparatutto competente ma
generico: il GDD dice esplicitamente che la vittoria deve nascere da decisioni tattiche e
gestione della squadra, **non dalla mira**. Oggi nasce solo dalla mira.

Le fondamenta ci sono già tutte — ed è per questo che il momento è adesso:
- L'AI ha profili tattici reali (doc 16: aggression, retreat, cover_preference, flank).
- L'AI si muove con pathfinding + crowd avoidance (doc 22) → **un ordine "vai lì" è eseguibile**.
- L'AI legge i metadata di mappa (doc 18) → **"prendi copertura" ha dati veri sotto**.
- La telemetria osserva gli stati AI (doc 21) → **gli ordini sono verificabili in `--sim`**.

## Goal
Un ordine impartito con **un tasto**, eseguito in modo credibile, verificabile headless.

## Scope

### Modello dati (runtime, non definizioni JSON)
```
Squad                                   Order
- squadId                               - issuer            (player | AI leader)
- leaderEntityId                        - recipient         (squad | membro)
- members[]                             - type              (vedi sotto)
- formation                             - target            (entità | posizione | obiettivo)
- posture                               - priority
- currentObjectiveId                    - issuedTick
- sharedThreatMemory                    - executionState    (pending|active|done|failed)
- commandQueue                          - failureReason     (esplicito, mai silenzioso)
- tacticalResource   (punti comando)
```

### Ordini iniziali (pochi e utili, non un catalogo)
`Follow` · `HoldPosition` · `MoveTo` · `TakeCover` · `FocusFire` · `Revive` · `Regroup`

### Comando a due livelli (GDD)
1. **Ordine contestuale (un tasto):** si punta un elemento e si preme un tasto → l'ordine
   implicito. Copertura → `TakeCover`; nemico → `FocusFire`; alleato a terra → `Revive`;
   posizione → `MoveTo`. La risoluzione del contesto usa il raycast già esistente
   (`physics/HitTest`, condiviso mirino/proiettili) + i `coverPoints` del MapDef (doc 15).
2. **Ruota di comando:** ordini ampi (regroup, hold, advance).

Il punto di design: la tattica sta **dentro il flusso dell'azione**. Un sistema di comando che
richiede di fermarsi ha già fallito.

### Contratto di autonomia (chi decide cosa)
- **Giocatore:** intenzione ("difendi qui", "fuoco su quello").
- **SquadSystem:** assegna il task ai membri.
- **AI individuale (doc 16):** sceglie movimento, copertura, path, micro-combattimento.
- **CrowdSystem (doc 22):** esegue il movimento.
- **HUD:** mostra ordine, stato, distanza, completamento, **e il motivo del fallimento**.

Un ordine non deve mai sparire in silenzio: o si completa, o fallisce **con causa esplicita**
(vincolo coerente con la disciplina telemetria, doc 21).

### Stato "a terra" + rianimazione (prerequisito del pilastro)
Incapacitazione non letale con finestra di rianimazione. È ciò che dà **peso alle perdite** e
rende la squadra una risorsa invece che comparse. Tocca `CombatSystem` (doc 03).

### Economia tattica (Punti Comando)
Risorsa **in-missione** guadagnata **completando obiettivi** (doc 25), spesa per rinforzi/
veicoli/supporto. Deliberatamente **non** guadagnata con le kill: è il pilastro "obiettivi >
uccisioni" reso meccanica.

## Out of Scope
- Comando scalabile oltre la squadra (plotone/compagnia) — è Fase 3, dipende dai gradi (doc 27).
- Formazioni complesse: iniziare con spacing/separation (il crowd Detour lo dà già).
- Comunicazioni vocali/radio: audio è fuori dal focus attuale.
- Ordini per veicoli (Fase B, doc 19).

## Architecture
Nuovo `SquadSystem` in `World::tick`, **fra `AiSystem` e `CrowdSystem`**: l'AI decide il
comportamento, la squadra lo vincola al task, il crowd esegue il movimento. Ordine risultante:
`MovementSystem → CombatSystem → SquadSystem → AiSystem → CrowdSystem`.

`SquadComponent` (squadId, ruolo, ordine corrente) sui membri. L'input del giocatore arriva
dall'Application via il **pattern mailbox** sul World (doc 10), non accoppiando `ecs/` al codice
di gioco.

## Technical Decisions
- **Perché SquadSystem prima di AiSystem:** l'ordine è un *vincolo* sulla decisione, non un
  override del movimento. L'AI resta autonoma dentro il vincolo — è ciò che rende gli alleati
  credibili invece che telecomandati.
- **Perché non scrivere il transform:** vincolo confermato (doc 10/22) — il movimento AI passa
  per `NavManager::requestMove*`; lo scrive il CrowdSystem. Gli ordini **non** sono un'eccezione.
- **Perché pochi ordini:** il GDD chiede "complessità accessibile". Sette ordini che funzionano
  valgono più di venti che nessuno usa in mezzo a un firefight.

## Acceptance
Stato al 2026-07-15 (Phase A+B). "Implementato" ≠ "verificato": marcato solo ciò che è stato
davvero misurato — vedi 07_Changelog per i numeri.
- [x] Un ordine contestuale si impartisce con un tasto senza fermare l'azione. — tasto **G**
      (rimappabile), contesto dal mirino. Catena a valle della mailbox verificata headless;
      **tasto, contesto dal mirino e HUD confermati dal playtest dell'utente il 2026-07-15**.
- [x] Gli alleati eseguono usando cover reali (doc 15/18) e pathfinding (doc 22). — `TakeCover`
      punta i `coverPoints` del MapDef; movimento via `requestMoveTarget` (destinazione reale,
      KI #33). Misurato: 8 emessi / 3 completati in `--stress`.
- [x] Ogni ordine finisce completato o fallito **con causa**, visibile in `session_latest.jsonl`.
      — eventi `order issued/completed/failed` con `reason`. Revive/Regroup falliscono con causa.
- [x] Verificabile in `--sim`: una squadra sotto ordini si comporta diversamente da una libera.
      — dimostrato con `MoveTo` deterministico: 8.0 → 1.3 m contro ~6-7 m di una squadra libera.
- [ ] Misurabile: la missione è più difficile senza usare gli ordini (la squadra è utile davvero).
      — **non ancora misurabile**: serve il framework obiettivi (N2/doc 25) per avere una
      "missione" da fallire. È il criterio che conta di più: gli altri dicono che il sistema
      funziona, questo direbbe che **serve**.
- [x] **Le perdite hanno peso** (Phase C, 2026-07-17): un membro va a terra invece di morire; la
      squadra lo soccorre o lo perde. Verificato in `--sim` (down→revive→bleed-out, tutti e tre).

### Phase C — FATTA 2026-07-17 (stato "a terra" + rianimazione)
Incapacitazione non letale: un membro della squadra alleata, invece di morire, va **a terra** con
una finestra di bleed-out. Dà peso alle perdite — la squadra è una risorsa, non comparse.
- **Stato** in `SquadComponent` (`downed`/`bleedoutRemaining`/`reviveProgress`), non un componente
  nuovo: solo i membri squadra vanno a terra e ce l'hanno già.
- **CombatSystem**: intercetta il colpo letale su un alleato-squadra (team 1, **non** il giocatore →
  ha il suo respawn, KI #39) → a terra invece che distrutto. Additivo: nemici e giocatore invariati.
  Un colpo su un già-a-terra lo finisce (morte reale).
- **Rianimazione per PROSSIMITÀ**: un compagno vivo (o il giocatore-leader) entro
  `SQUAD_REVIVE_RADIUS` per `SQUAD_REVIVE_TIME` continui → rianima a `SQUAD_REVIVE_HP` degli HP max.
  Interrotto se il soccorritore si allontana.
- **Auto-soccorso**: il membro libero (in Follow/idle, mai su ordine del giocatore) più vicino viene
  dispacciato con un ordine **`Revive`** verso il caduto. È ciò che rende la squadra
  autoprotettiva; l'ordine `Revive` ora è *implementato* (prima falliva "non ancora implementato").
- **Bleed-out**: nessun soccorso in tempo → morte; SOLO ORA conta come perdita (`missionStats`),
  non alla caduta — una perdita evitata non deve pesare sul giudizio (doc 25).
- **AiSystem**: un'unità a terra è inerme (non sente/decide/spara/muove).
- **HUD**: pannello squadra mostra `[A TERRA n — Xs]` col conto alla rovescia più urgente.
- **Costanti** in `core/GameConfig.hpp` (segnaposto da rifinire): bleed-out 20s, raggio 2.5m,
  canalizzazione 3s, HP al risveglio 50%.
- **Verificato in `--sim`** (`--stress 8`, telemetria ADR-016): 9 a terra / 7 rianimati / ordini
  `Revive` auto-emessi (es. bot 11 → rianima #15, ciclo down→dispaccio→revive completo). Path
  bleed-out esercitato abbassando la costante a 1.5s → 2 morti per bleed-out. Tutti e tre gli esiti
  visti scattare.

### Comandi diretti a un compagno (tasto G, 2026-07-17)
Oltre a FocusFire (nemico) / MoveTo·TakeCover (terra), il mirino su un **compagno** dà due comandi:
- **compagno a terra → `Revive`**: manda il membro vivo più vicino a soccorrerlo (esplicito, oltre
  all'auto-soccorso);
- **compagno vivo → `CoveringFire`**: quel compagno SOPPRIME — tiene la posizione e **non entra in
  fase evasiva** (niente peek/hide), resta esposto e continua a sparare con ~30% di cadenza in più
  ("stand and deliver"). È ciò che lo distingue da HoldPosition.
Infrastruttura: `SquadOrderRequest.directedMember` (ordine a UN membro, non a tutta la squadra) +
rilevamento mirino separato per gli alleati (`aimAlly`: la raycast dei nemici salta team 1 e hp≤0,
quindi i compagni e i caduti non erano mirabili).

### Rifinitura Phase C dal playtest (2026-07-17)
- **Bug risolto (KI #50)**: i caduti si muovevano ancora sotto ordine — l'agente crowd conservava
  il target. `AiSystem` ora li **ferma attivamente** (`requestMoveVelocity 0`) ogni tick.
- **Indicatore**: tint ROSSO sul clone a terra (manca una posa prone; riutilizzabile per un HUD
  cloni).

### Ruota di comando (livello 2) — FATTA 2026-07-17
Tasto `CommandWheel` (default **B**, rimappabile) TENUTO → la camera si congela, il mouse sceglie il
settore, al rilascio l'ordine va a tutta la squadra:
- **Regroup**: raduna sul leader (MoveTo sulla posizione del giocatore).
- **Hold**: ognuno tiene la PROPRIA posizione (HoldPosition per-membro, non un punto condiviso).
- **Advance**: avanza ~15 m nella direzione di mira.
HUD radiale a 3 settori col settore puntato evidenziato. Il **mirino diventa verde** su un compagno
(feedback per Revive/CoveringFire).

### Non ancora fatto
- **Punti Comando**: bloccati su N2 (si guadagnano completando obiettivi, non uccidendo).
- **Rifiniture Phase C**: nessuna posa "prone" (l'unità a terra resta in piedi, solo tint+HUD) —
  in attesa di un set base di pose/animazioni (dipende da tooling dell'utente); bilanciamento
  tempi/raggio da provare. `CoveringFire` è ora soppressione vera (giro 9).
- `FocusFire` resta Active finché il designato vive: se la squadra non lo vede mai, l'ordine non
  scade. È coerente col contratto (l'ordine è visibile sull'HUD e il giocatore può sostituirlo),
  ma se in playtest risultasse fastidioso serve un timeout con causa.

## Interconnessioni
Vincola l'AI (doc 16) · usa nav/crowd (doc 22) e i metadata (doc 15/18) · consuma gli obiettivi
(doc 25) e ne produce la valutazione · l'economia tattica lega squadra e obiettivi · il livello
di comando sarà sbloccato dal grado (doc 27) · è il pilastro #4 del GDD (doc 23).
