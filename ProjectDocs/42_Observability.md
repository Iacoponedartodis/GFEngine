# 42 — Osservabilità: mappa dei punti ciechi e piano

> Documento operativo di ADR-050. Risponde a una domanda sola: **cosa non riesco a vedere di
> questo progetto, e cosa costruire perché diventi visibile.** Non è una wishlist: la colonna
> "stato" è misurata sul codice, non stimata.

## 0. Perché, in una riga

Io non vedo lo schermo, non sento il "feel", non noto che un fucile è piccolo. **Ciò che non è
strumentato, per me non esiste** — e viene sostituito da ipotesi plausibili e sbagliate. Il conto
già pagato: tre diagnosi consecutive fuorviate su KI #86, un flag di test inerte da settimane
(`--stress`), e una domanda di performance (KI #87) rimasta *non rispondibile* per un mese.

## 1. L'audit — misurato 2026-08-03

Densità di telemetria per file, contata sul codice (`telemetry::event|logTrace`):

| modulo | eventi | livelli ADR-050 coperti |
|---|---|---|
| `AiSystem` + `AiTrace` + `AiCommandLayer` | 10 | **tutti e tre** (sintomo, funnel, singola entità) |
| `ObjectiveSystem` | 7 | parziale (eventi, nessun funnel) |
| `SquadSystem` | 4 | parziale |
| `ConquestMode`, `CombatSystem`, `CommandPosts` | 2-3 | eventi sparsi |
| **`MovementSystem`** | **0** | nessuno |
| **`CrowdSystem`** | **0** | nessuno |
| **`NavManager`** | **0** | nessuno |
| **`Collision` (physics)** | **0** | nessuno |
| **`DefinitionRegistry`** | **0** | nessuno |
| **`PlayerController`** | **0** | nessuno |
| **tutto `render/`** (13 file) | **0** | nessuno |

E il buco trasversale: **zone Tracy in tutto il motore: 4.** Nessuna misura di costo nella
telemetria. Tracy resta utile (ADR-015) ma è una GUI separata: non posso aprirla, non posso
leggerla, non posso confrontarne due esecuzioni in uno script.

## 2. Cosa è stato costruito (changelog 124)

### `Profiler` — dove finisce il tempo
Zone annidate con report periodico nel JSONL: `frame ⊃ render ⊃ attesa_vsync`, `frame ⊃
simulazione ⊃ mondo ⊃ world.tick ⊃ {ai, crowd, combat, squad, movement, objective}` e `gamemode`.
Per zona: **ms/frame, ms di picco, quota del frame, chiamate**. Sempre acceso — un profiler che si
accende "quando serve" non c'è mai quando il problema si presenta.

Due scelte che cambiano le conclusioni:
- **`attesa_vsync` è una zona a sé.** Senza, il primo risultato diceva "render = 97% del frame":
  vero e completamente fuorviante, perché 7,3 ms su 16,7 sono *attesa del vblank*, non lavoro.
  Distinguere lavoro da attesa è metà del valore di un profiler.
- **Il denominatore è esplicito** (`quota` sulla zona radice). Un "12 ms" senza denominatore non
  dice se è tanto o poco — stessa lezione dei funnel dell'AI.

### `inventario avvio` — cosa succede quando si avvia
Evento strutturato con il tempo di caricamento del registry e la *dimensione reale* del contenuto,
mappa per mappa. Misurato: registry in **30 ms**; Training Ground ha 167 box, **169 posizioni
tattiche**, 23 settori, 22 route.

## 3. Cosa hanno trovato subito

**`--stress N` era INERTE.** Impostava i conteggi e il blocco `--sim` li sovrascriveva con quelli
della mappa; siccome `--stress` implica `--sim`, era inerte **da sempre**: 10 o 100 AI richieste
davano identiche 12 unità. L'ho visto perché il costo non cambiava di un microsecondo al variare di
N — impossibile per un test di scalabilità. Ogni "profilazione a scala" fatta prima non misurava
nulla. Corretto: l'override esplicito dell'operatore vince sui conteggi di mappa.

**C'è un transitorio di riscaldamento enorme.** Prima finestra da 300 frame: **87 ms/frame**
(render 80). Converge a ~16,7 ms entro la quinta. Qualunque misura presa sui primi ~1500 frame è
priva di senso — ed è esattamente l'errore che ho fatto al primo tentativo di test di scalabilità,
confrontando run a maturità diversa e concludendo (assurdamente) che il costo *scendeva*
all'aumentare delle AI.

> **Regola d'uso**: per confronti, `--sim-ticks ≥ 4200` e si legge **l'ultima finestra**. Una sola
> finestra, o una run corta, misura il warm-up.

**Scalabilità della simulazione** (ultima finestra, maturità pari):

| AI vive | simulazione | ai | crowd |
|---|---|---|---|
| 9  | 0,73 ms | 0,16 | 0,25 |
| 18 | 0,81 ms | 0,26 | 0,36 |
| 31 | 1,66 ms | 0,39 | **1,05** |
| 36 | 1,51 ms | 0,38 | 0,93 |

**Conclusione provvisoria su KI #87**: a 36 AI l'intera simulazione costa **1,5 ms/frame** su un
frame da 17-30 ms. Il collo di bottiglia **non è la simulazione**, e dentro la simulazione il
`crowd` cresce più in fretta dell'AI (×4,2 di costo per ×3,4 di unità, contro il ×2,4 dell'AI).
*Provvisoria* perché le misure di render in headless sono rumorose (7-20 ms fra run): la prova
decisiva è una sessione giocata vera — e ora basta giocare, il profilo si scrive da solo.

## 4. Cosa resta cieco — piano, in ordine di valore

| # | Buco | Perché conta | Livelli da costruire |
|---|---|---|---|
| ~~**O1**~~ | ~~Crowd/Nav: 0 eventi~~ **✅ FATTO** (changelog 127) — funnel `Nav/navigazione` + stato agenti (`agenti`/`con_meta`/`in_moto`) | Ha subito smentito un'ipotesi (le mete sono tutte camminabili: 100% al primo aggancio, 0 scartate) e trovato il 60% di query spaziali ridondanti | — |
| ~~**O2**~~ | ~~Render: 0 eventi~~ **✅ FATTO** (125 + 131) — funnel draw call, zone `render.scena` / `render.ui`, e i **vertici spediti per frame** | **Ha chiuso KI #87**: la scena 3D è il **95% del frame**, 1,45 M vertici/frame, e il B1 da 161k vertici vale due terzi del traffico. Il frustum culling **non esiste** (205 disegnate su 206) | — |
| ~~**O3**~~ | ~~Sessione giocata: nessuna traccia~~ **✅ FATTO** (changelog 128) — evento `Player/sessione`: vivo/hp/pos, colpi, ordini, metri percorsi, secondi vivo/morto/in mira, arma, uccisioni e alleati persi | In `--sim` esce tutto a zero: il giocatore è un osservatore. **È la conferma che lo strumento distingue una simulazione da una partita** — la distinzione che mancava a ogni conclusione di bilanciamento | — |
| ~~**O4**~~ | ~~Combat: eventi su stdout~~ **✅ FATTO** (changelog 128) — funnel `Combat/funnel di fuoco` per team, che **chiude esattamente** (a segno + geometria + spenti = sparati) | Ha rivelato un'asimmetria mai vista: cloni 52,7% di accuratezza e 16 uccisioni, droidi 34,8% e 2. E che il **55% dei colpi finisce sulla geometria** | *resta*: attribuzione per ARMA (serve un id sul proiettile) |
| ~~**O5**~~ | ~~Ability e veicoli: 0~~ **✅ FATTO** (changelog 129) — `Combat/ability e veicoli` + `roll_attivati` + `scudo_assorbito` | Sospettavo peso morto (un solo punto di attivazione in tutto il codice): **smentito** — roll parte 48 volte, gli scudi assorbono 339 danni. Il peso morto è il **veicolo**: presente, mai guidato | guardia: `stati_attivabili` a 0 = sistema inerte |
| ~~**O6**~~ | ~~Asset: solo conteggi~~ **✅ FATTO** (changelog 131) — `Engine/inventario asset`: mesh caricate, vertici, memoria, le 8 più pesanti | È il dato che ha dato un **nome** al collo di bottiglia (`star_wars_b1_battle_droid.glb`, 161.304 vertici) invece di lasciarlo un aggregato anonimo | — |
| ~~**O7**~~ | ~~Missioni: nessun funnel~~ **✅ FATTO** (changelog 130) — `Objective/stato missione`, con una misura **specifica per tipo** di obiettivo | Ha trovato **KI #90** al primo utilizzo: `firebase_alpha` non avanza mai in 150 s. E ha insegnato che *anche la soglia d'allarme* deve leggere il dato che mostra, non solo il testo | — |

## 4-bis. Organizzazione di `_telemetry_data/` (2026-08-03, changelog 125)

Un solo `session_latest.jsonl` mescolava tutto: in una sessione giocata di pochi minuti sono
**1943 righe / 658 KB**, in cui il profilo (29 righe) e gli stalli (348) annegano fra 1043 cambi di
stato dell'AI. E si troncava a ogni avvio: "fare un po' di prove diverse" significava conservarne
una sola.

| file | contenuto | `system` instradati |
|---|---|---|
| `session_latest.jsonl` | **tutto**, in ordine cronologico | — |
| `perf.jsonl` | profilo, rendering, memoria, inventario avvio | `Perf`, `Engine` |
| `ai.jsonl` | decisioni tattiche, stalli, scatola nera | `AI` |
| `combat.jsonl` | colpi, danni, squadra, soccorso | `Combat`, `Squad` |
| `world.jsonl` | game mode, obiettivi, command post, navmesh | `GameMode`, `Objective`, `CommandPost`, `Nav`, `Mission` |
| `content.jsonl` | registry, validazione, classi/armi | `Content`, `Registry`, `Map` |
| `storico/<data-ora>/` | la sessione precedente, per intero | — |

Due scelte deliberate:
- **`session_latest` resta e riceve tutto.** I file per dominio servono ad analizzare *dentro* un
  dominio; l'indice cronologico serve a **correlare fra domini** — è così che si è visto che il
  rallentamento seguiva i cambi mappa, cosa invisibile in un file di solo `Perf`.
- **Un `system` non mappato finisce solo in `session`.** Un sistema nuovo senza flusso si *nota*
  (manca dal file del suo dominio) invece di confondersi in mezzo agli altri.

L'archiviazione all'avvio sposta i `.jsonl` (e il `game_state.json` che li accompagna) in
`storico/<YYYYMMDD-HHMMSS>/`: nessuna prova si perde più.

## 4-ter. Quanto costa osservare — MISURATO, non stimato (2026-08-03)

Domanda dell'utente: *"per non distruggere le prestazioni, limitiamo certe cose alla build Debug?"*.
Prima di rispondere ho reso il costo una zona di profilo (`telemetria`), perché "quanto costa
osservare" non può essere l'unica cosa decisa a intuito.

| modalità | `session_latest` | costo telemetria | quota del frame |
|---|---|---|---|
| normale (default) | **43 KB** | **0,0025 ms/frame** | **0,01%** |
| `--telemetry-verbose` | 64 KB | 0,0161 ms/frame | 0,05% |

**Conclusione: la build Debug è la leva sbagliata.** Due motivi:

1. **Non costa.** Un centesimo di percento del frame. Il problema del volume era di
   **leggibilità**, non di prestazioni: un solo evento legacy (`AI/state change`) occupava il **39%
   del file** con 743 righe che non sono mai servite a una diagnosi. Declassato a `Debug`: il file
   passa da **467 KB a 43 KB (−91%)** senza perdere nulla di ciò che si legge.
2. **Debug è il posto sbagliato.** I bug che contano si presentano **giocando**, e si gioca in
   Release. Un'osservabilità che vive in Debug non c'è mai quando il problema si presenta — lo
   stesso motivo per cui il profiler è sempre acceso. E qui la build Debug è di fatto inutilizzabile
   (ASan senza la sua DLL): l'osservabilità sparirebbe del tutto.

**La leva giusta è la verbosità a RUNTIME**, con due livelli:

| livello | cosa contiene | quando |
|---|---|---|
| `Info` (default, anche in Release) | aggregati e guardie: profilo, rendering, funnel d'ingaggio, stalli per causa, decisioni tattiche | **sempre** — è ciò che leggo |
| `Debug` (`--telemetry-verbose`, o implicito con `--trace-ai`) | per-evento e per-entità: cambi di stato, traccia d'agente | quando si indaga |

Un evento sotto soglia esce **prima di serializzare**: costa un confronto. È questo che permette di
aggiungere strumenti verbosi senza pagarli quando sono spenti — quindi possiamo spingerci molto più
in là sulla precisione, come chiede l'utente, senza toccare le prestazioni.

> Compile-time (`#ifdef`) resta riservato a ciò che è caro **nel percorso caldo**: raycast extra,
> dump per-entità a ogni tick. Finora non è servito per nulla.

## 5. Regole d'uso (per me, la prossima volta)

1. **Scarta il warm-up.** Ultima finestra di una run ≥ 4200 tick, mai la prima.
2. **Zone annidate**: le percentuali sommano oltre il 100% per costruzione. Leggere `liv`.
3. **Attesa ≠ lavoro.** Se una zona include un blocco (vsync, I/O, lock), va separata o dichiarata.
4. **Un flag di test va verificato che faccia qualcosa**, con un effetto misurabile. `--stress` è
   stato inerte per settimane perché nessuno ha mai controllato che cambiasse un numero.
5. **Sonde vs guardie**: le guardie (contatori) restano; le sonde (raycast, allocazioni) si tolgono
   appena hanno risposto, lasciando scritto **quale risposta hanno dato**.
