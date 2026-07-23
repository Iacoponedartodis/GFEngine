# 38 — Audit integrazione Metadata ↔ AI (2026-07-22)

> Richiesto dall'utente: prima di rifinire, **capire cosa i metadata autorati producono davvero** nel
> comportamento delle AI, cosa è morto o decorativo, e dove i sistemi non lavorano insieme. Verificato
> contro il codice vivo (`AiSystem.cpp`, `WorldIntel.cpp`, `NavManager.cpp`) e contro i dati autorati
> (firebase, outpost/Training Ground).

## Metodo
Per ogni tipo di metadato: **chi lo consuma** (query/sistema), **quanto** (call site reali, non la
definizione), e se l'effetto è **osservabile**. Poi: gli **attriti** fra sistemi.

---

## A. Cosa È consumato e funziona
| Metadato | Consumatore | Stato |
|---|---|---|
| Posizioni `cover` (protection>0 / canShoot) | `bestCoverToward`, `bestFiringPosition` | ✅ manovre di copertura/tiro |
| Campi `fireArcDeg`/`fireRange`/`canShoot`/`facingDeg`/`protection`/`importance` | `bestFiringPosition`, `hasLineOfFire`, `enterHunt` | ✅ tutti letti |
| Posizioni `vantage` | `nearestPositionByRole("vantage")` | ✅ approccio "punto dominante" |
| Esposizione (derivata) | `bestFlankingPosition` | ✅ scelta dell'aggiramento |
| Settori | comandante (`enemyCommand`) + torre di controllo (`allyIntel`) | ✅ direttive multi-fronte / segnali |
| Command post | cattura, obiettivi, `nearestCapturablePost` | ✅ |
| Strutture/torri | rete comunicazione, ingaggio strutture, intel | ✅ |
| Comandante | `enemyCommand` | ✅ |
| Danger zone | costo navmesh (il pathfinding le aggira) + repulsione di fallback (solo senza navmesh) | ✅ (nessuna doppia verità: la repulsione è solo il fallback) |

---

## B. Morto o DECORATIVO (authoring sprecato)

### B1. 3 ruoli su 5 delle posizioni tattiche NON fanno nulla — **impatto reale alto**
Solo `cover` (via protection/canShoot) e `vantage` (via `nearestPositionByRole`) guidano un
comportamento. **`defensive`, `chokepoint`, `observation` non hanno alcun consumatore per-ruolo**:
contano solo *se* per caso hanno `protection>0`/`canShoot`, e allora vengono trattati come cover
generica. Il ruolo autorato è ignorato.
**Sui dati reali (firebase+outpost): 8 `chokepoint` + 6 `defensive` + 3 `observation` = 17 posizioni
il cui ruolo è decorativo.** L'utente le ha autorate aspettandosi un significato.

### B2. `dangerAt()` — query MORTA (0 chiamate)
Le danger zone sono consumate da navmesh + repulsione; la query `dangerAt` non è chiamata da nessuno.
Codice morto **oppure** l'aggancio mancante per far sì che la scelta tattica (posizione di tiro/cover)
**eviti** le zone pericolose — oggi un'AI può scegliere una posizione di tiro **dentro** una danger
zone (vedi C3).

### B3. `bestOverwatchFor()` (versione senza `Position`) — MORTA
Delegava a `bestFiringPosition`, mai chiamata. Sostituita da `bestOverwatchForPosition` (che usa il
grafo). Da rimuovere.

### B4. Grafo `positionCovers` → overwatch esplicito — CONSUMATO ma MARGINALE
`bestOverwatchForPosition` legge il grafo, ma in `--sim` su firebase scatta **di rado** (misurato:
`overwatch_avviati` ~0-1). Al confine col decorativo: o si spinge (più unità che restano a coprire)
o si accetta come sapore occasionale.

### B5. Campo `height` delle posizioni tattiche — solo visivo (editor)
Non consumato dall'AI; serve solo al marker dell'editor. Accettabile, ma da documentare.

### B6. Filtri navmesh per-ruolo (Fase 3b) — marcati ma NON cablati
Le aree COVER sono marcate nel navmesh ma con costo neutro (1.0). I costi per-ruolo sono "un'estensione
banale" già predisposta ma non attiva. Documentato, non un bug.

---

## C. Attriti fra sistemi (i sistemi non lavorano INSIEME)

### C1. Le route IGNORANO il comandante e la torre — **il gap più grande**
Le unità con una route (`patrolRoute >= 0`, ~**metà** della forza per costruzione) **non rispondono
mai** alle direttive del comandante (`enemyCommand`) né ai segnali della torre (`allyIntel`): quel ramo
richiede `patrolRoute < 0`. Il comando raggiunge solo le unità senza route. Su firebase ci sono molte
route autorate → metà esercito è di fatto **sordo al comando**, su un binario fisso. È il "sistemi che
non funzionano insieme" nella sua forma più netta. Era una scelta deliberata (dare la route a tutti
avrebbe svuotato il comando), ma va ripensata: la route dovrebbe essere un **default sovrascrivibile**
dal comando, non un mondo separato.

### C2. Le route sono RIGIDE (osservazione dell'utente)
`advancePatrol`: `patrolSeg = (patrolSeg+1) % segCount`, sempre "verso B". Solo in avanti, ciclico
(con un salto-teletrasporto al wrap), mai all'indietro; e l'unità non cambia mai route. Non si può
raccogliere una route dal punto più vicino né percorrerla al contrario.

### C3. La scelta tattica non tiene conto delle danger zone
`bestFiringPosition`/`bestCoverToward` scelgono per protezione/gittata/LOS, ma **non** penalizzano le
posizioni dentro una danger zone. Cover e danger sono due sistemi che non si parlano. (Aggancio
naturale per resuscitare `dangerAt`, B2.)

---

## D. Priorità proposte
**Alta (integrazione — la richiesta dell'utente):**
- **P1 — Route ↔ comando (C1)** → **✅ FATTO (ADR-045, 2026-07-22)**: Advance/Retreat sovrascrivono la
  pattuglia per tutti; Hold = pattuglia. Metà forza sbloccata.
- **P2 — Route fluide (C2)** → **✅ FATTO (ADR-045)**: bidirezionali, raccolta dal punto più vicino,
  cambio route dopo Search.

**Media (authoring sprecato):**
- **P3 — Dare senso ai 3 ruoli (B1)** → **✅ FATTO (ADR-046, 2026-07-22)**: `observation` = vista estesa
  locale (`aggroRange ×1.5` entro 10 m); `defensive`/`chokepoint` = posizioni da tenere sotto comando
  `Hold` (nuova `bestHoldPosition`). Tutti e 5 i ruoli ora hanno comportamento. `chokepoint` come
  imbottigliamento attivo e `observation` che alimenta l'intel di squadra restano futuri (doc 33).
- **P3b — Cover evita danger (C3)** → **✅ FATTO (ADR-046)**: `bestCoverToward`/`bestFiringPosition`
  sottraggono `dangerAt` dal punteggio. `dangerAt` non è più morta (chiude B2).

**Bassa (pulizia):**
- **P4** — rimuovere `bestOverwatchFor` morta (B3) → **✅ FATTO (ADR-046)**; `dangerAt` (B2) → risolto in
  P3b. Restano da **documentare** `height` (B5) e i filtri per-ruolo (B6) — solo nota, nessun codice.
- **P5** — decidere sull'overwatch marginale (B4): spingerlo o accettarlo. **Aperto.**

### Nota harness (emersa durante P3)
Verificando `obs_vista_estesa` ho scoperto che `--map <id>` era **ignorato in `--sim`/sandbox**: il sim
girava sempre sulla mappa d'indice 0 (Training Ground, 0 punti observation), non su quella nominata.
Corretto (KI #77). Conseguenza: le misure `--sim` di P1/P2 (ADR-045) giravano su Training Ground, non
su firebase — valide come misura, ma non sulla mappa che il flag indicava.

## E. Nota trasversale
Il quadro è **sano**: la maggior parte dei metadata è consumata e nessun sistema si contraddice. I
problemi non sono bug ma **integrazione incompleta**: pezzi costruiti che non si sono ancora agganciati
fra loro (route↔comando, cover↔danger, ruoli↔comportamento). È esattamente il "far funzionare insieme
i sistemi" che l'utente ha chiesto — e la lista P1-P3 lo realizza senza aggiungere sistemi nuovi.
