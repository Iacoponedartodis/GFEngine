# 06 — Todo (reality-based, prioritized)

## ▶ I TRE LAVORI SUGLI STRUMENTI chiesti dall'utente il 2026-08-05

1. ✅ **Validazione navmesh** nell'editor (changelog 161, ADR-054).
2. ✅ **Editor strutture come TAB** del Map Editor (changelog 163, ADR-055, doc 48).
   Resta fuori scope, deliberatamente: il gate `--validate` sui tipi non verificati (serve quando i
   tipi saranno in uso su una mappa vera).
3. ✅ **Liste del Map Editor in categorie espandibili** (changelog 165): box per tipo, posizioni per
   ruolo (ruoli ricavati dai dati), filtro per nome, conteggi e posizioni cieche nell'intestazione.

**I tre lavori sugli strumenti sono chiusi.** Fatto in più, dopo: doc 49 (stabilità), doc 50
(misure), doc 51 (audit), doc 52 (framework), **assemblaggi** (ADR-056), **guida in-editor (F1)**,
e il framework condiviso F1-F4.

## ▶ STATO AL 2026-08-07 — pronti per la mappa

### Aperto, in ordine di rischio
1. **KI #98** — crash entrando in Entity Editor. Non riproducibile; la rete di diagnosi ora
   funziona (fase + simboli, verificata). **Serve un'occorrenza reale**: allegare `crash_report.txt`.
2. **KI #100** — identità posizionale e tetti dei codici. Guardati (avviso a 80% e al tetto), causa
   non rimossa. La riparazione vera (doc 49 R5) è invasiva e si fa solo se il segnale scatta.
3. **KI #97** — recinto Droid CT irraggiungibile su Training Ground: richiede una scelta di design.
4. **Gizmo del Map Editor su `ViewportEditing`** — differito con motivo scritto (doc 52): funziona,
   e nessun collaudo può confermarne la migrazione. Criterio: quando il gizmo sarà verificabile
   senza mouse.
5. **Editor prefab** — condizionato al fatto che gli assemblaggi funzionino nell'uso reale.
   Con ADR-056 (un solo sistema) il prefab è il caso degenere di un assemblaggio: da valutare se
   serva ancora un editor separato o basti estendere quello delle strutture.

### Rumore da tenere d'occhio (non un difetto)
Salvare una mappa **riscrive tutti i numeri** normalizzandoli (`8.0` → `8`): una correzione di una
parola produce un diff di 1159 righe. Non danneggia nulla, ma rende impossibile vedere a colpo
d'occhio cosa è cambiato davvero in una mappa. Da valutare se stabilizzare il formato numerico.

### Verifiche manuali che restano (io non vedo lo schermo)
- Barra tab: aprire/chiudere, il popup salva/scarta, e che tornando su `Mappa` vista e selezione
  siano quelle di prima.
- **Assemblaggi**: crearne uno, salvarlo, piazzarlo in mappa, **salvare la mappa e riaprirla** —
  deve restare intero (era il difetto del changelog 179).
- Ctrl+Z nel tab strutture e in Entity Editor; clic su una parte nella viewport.
- `Vista → Marcatori` e "Solo geometria" con l'overlay navmesh acceso.
- Uscita con modifiche non salvate in più moduli: l'elenco deve nominarli tutti.
## ⚠ PUNTO DELLA SITUAZIONE — 2026-08-02 (aggiornato dopo changelog 116-123)

Gli ultimi giri hanno chiuso molto ma lasciato aperte cose di tre tipi diversi. **Non confonderli**:
il primo tipo costa minuti all'utente, il secondo è lavoro mio, il terzo è una decisione di design.

### A. Verifiche manuali dovute (solo l'utente può farle — io non vedo lo schermo)
Accumulo di changelog 108-123. In ordine di rischio:
1. **Tarare `hand_scale`** di DC-15X, DC-17, T-21 con la nuova anteprima in mano (123). Il gate le segnala.
2. **Rallentatore sulle AI**: restano ~13-17 episodi di stallo per sim; verificare se si notano ancora (121).
3. **Soccorso differito** (122): verificare che *"Zona troppo calda"* compaia quando serve e che la squadra
   non abbandoni sistematicamente i caduti — le soglie sono nel BalanceEditor.
4. **Marksman schierato** dall'editor: si comporta da tiratore? (122)
5. Smoke test arretrati dei moduli editor migrati a `ModuleShell` (109-115) e del sistema prefab (104-107).

### B. Debito tecnico aperto (mio)
- ▶ **OSSERVABILITÀ (ADR-050, doc 42)** — binario nuovo, in corso.
  ✅ profiler + inventario avvio (124) · ✅ flussi separati + storico (125) · ✅ verbosità a runtime
  (126) · ✅ **O1 crowd/nav** (127) · ✅ O2 *parziale*: funnel draw call (125).
  ✅ **O4 combat strutturato** + ✅ **O3 sessione giocata** (128).
  ✅ **O5 ability/veicoli** + ✅ **O7 missioni** (129-130).
  ✅ **O2 completo** + ✅ **O6 asset** (131) — **copertura ADR-050 COMPLETA su tutti i sistemi vivi**.
  Resta solo l'attribuzione per ARMA nel funnel di fuoco (serve un id sul proiettile).
- ✅ **KI #87 — performance: CAUSA TROVATA** (131). Non è l'AI (2,9% del frame): è il **volume di
  vertici** — 1,45 M/frame, di cui due terzi dal solo mesh del B1 (**161.304 vertici**, dieci volte
  il Clone Trooper). Col rendering client-side-array (ADR-003) i vertici risalgono alla GPU a ogni
  draw call, quindi ogni droide in più costa 161k vertici/frame.
  - ▶ **Prossimo lavoro di ottimizzazione**, in ordine di valore/rischio: **(1) decimare il mesh del
    B1** — contenuto, non motore, guadagno massimo e rischio minimo · **(2) frustum culling**, che
    oggi **non esiste** (205 disegnate su 206 esaminate) · (3) LOD per distanza · (4) VBO, **ultima
    risorsa**: ADR-003 è un workaround deliberato per il driver Intel.
- **KI #85** — testi ancora tagliati in alcuni punti dell'editor.
- **Regole editor** (doc 39): R1 *Elimina* manca in 5 moduli su 7, R5 splitter in Class/Mission. Si applicano
  quando si tocca il modulo.
- **ADR-050 non retroattivo**: navigazione, game mode, missioni, ability e veicoli **non hanno
  osservabilità**. Si strumentano quando li si tocca.
- **Sonde temporanee**: nessuna lasciata accesa (verificato 2026-08-02).

### B-bis. RENDERING — nuovo binario, causa misurata (doc 43, KI #87)
Il collo di bottiglia del gioco. **R2 fatto** (138); il resto è scope in doc 43.
- ▶▶ **R1 è LA priorità di rendering, ora dimostrata**: tempo ≈ vertici × ~15-20 ns, e i vertici
  seguono quanti B1 sono vivi. Portare il B1 da 161k a ~16k toglie ~90% del traffico.
- ▶ **R1 LOD degli asset** — *dipende dall'utente (Blender)*: mesh semplificate, con un livello
  "bot/lontano" molto leggero. Guadagno massimo, rischio minimo, non tocca il motore. Il B1 a 161k
  vertici vale da solo due terzi del traffico.
- ✅ **R2 frustum culling** (138): fatto e corretto, ma **non è la leva** — scarta il 18% delle entità
  e solo il 5% dei vertici, tempo invariato. Il costo sta nelle unità, quasi sempre inquadrate.
- ⏸ **R3 soglia di distanza + LOD a scaglioni** (dopo R1: serve avere i livelli).
- ⏸ **R4 simulazione a distanza (AI LOD)** — **non è performance oggi** (la simulazione è il 2,9%
  del frame): è scalabilità futura per mappe grandi. Solo se il profilo lo giustifica.
- ❌ **VBO / riapertura ADR-003**: fuori scope. Si riapre per **compatibilità universale Windows**,
  non per performance, e **sull'hardware nuovo** (il workaround è per il driver di questo PC).

### B-ter. FONDAZIONE DEL MONDO — il prossimo blocco grande (doc 44)
Ordine deciso con l.utente (2026-08-04): **prima la fondazione, poi gli ordini**, perché ruota,
ordini rapidi e mappa tattica si appoggiano tutti a settori/posizioni/obiettivi.
Stato al 2026-08-04: **la pianificazione dei metadata è COMPLETA (doc 46)**. Si passa al piano di
map building, poi l.utente costruisce la mappa, poi si implementa.

- ✅ **W1 = MAP BUILDING — doc 47, COMPLETO**. ✅ G1 metriche (`MapMetrics.hpp`) · ✅ G2 undo/redo ·
  ✅ G4 primitive parametriche (ADR-053 Accepted, `MapStructures.hpp`) · ✅ G5 `type` al runtime.
  ✅ G3 selezione multipla (Ctrl+click; Ctrl+A a interruttore; sposta/ruota/elimina/duplica sul gruppo).
  ✅ G6 serie con offset, filtri di vista, figura di scala, righello a due selezioni.
  ✅ G7 validazione dal vivo nel viewport (stessa analisi del gate) + contatore di salute.
  ✅ G8 collaudo su Training Ground: due correzioni piccole + KI #97 (recinto Droid CT
  irraggiungibile, difetto invisibile ai dati). **Piano doc 47 COMPLETO.**
  ▶ **PROSSIMO: l.utente costruisce la mappa 300 × 200**, poi si passa a doc 46 (metadata).
  Fasi originali G1-G8:
  metriche normative → undo/redo → selezione multipla → **primitive parametriche**
  (scala/rampa/muro/piattaforma-con-accessi, che si espandono in box) → `type` letto dal runtime →
  array/livelli/figura di scala → validazione dal vivo nel viewport → **riparazione di Training
  Ground come collaudo**. Tre scelte all.utente (doc 47 §12): metriche, quante primitive nel primo
  giro, se riparare Training Ground prima.
- ⏸ **W4 mappa grande — 300 × 200 m** (deciso dall.utente 2026-08-04; **verificato per misura**:
  navmesh a tile singola, 1,385 s di build, nessun cambio architetturale). La costruisce l.utente
  con gli strumenti di doc 47, **prima** dei metadata: non per giocarci, ma per avere il banco su
  cui sviluppare la derivazione.
- ⏸ **W2 metadata DERIVATI** — **PIANIFICATO: doc 46** (ricerca in doc 45, decisioni in ADR-051/052).
  Substrato a tre livelli: griglia d.influenza / poligoni navmesh / posizioni tattiche.
  Fasi M1-M7 con criteri di accettazione. Restano tre scelte all.utente (doc 46 §12): dimensione
  della mappa grande, fin dove arrivare (M1-M4 o fino a M7), se servono le posizioni generate.
- ⏸ **W3 generazione automatica** = M7 di doc 46, col gate del 60% contro il fallimento di ADR-026.

### Dopo la fondazione (ordine dell.utente)
- ⏸ **Rework degli ordini rapidi** (l.utente dirà come) · **ordini della ruota** · **mappa tattica**
  con controllo di priorità, obiettivi e settori.
- ⏸ Modelli e animazioni: in **parallelo** lato utente, nessuna dipendenza reciproca.

### C. Decisioni di design aperte (servono all'utente, non implementarle d'istinto)
- ▶ **A5 taratura curve** (doc 40 §6): i pesi sono in `AiUtility.hpp`. **Sbloccata**: KI #86 è chiuso
  (stalli 41 → 3). È il lavoro in corso.
- ⏸ **Soccorso come MANOVRA di squadra — doc 26 Phase D** (visione utente 2026-08-04): non cadere in
  zone aperte e contese; valutare se il recupero è possibile; coordinarsi in più di uno (chi rianima,
  chi copre) o ripulire prima la zona. Col **clone medico**: i normali *trascinano* il ferito dietro
  una copertura + *semi-rianimazione che rallenta* il bleed-out, e chiamano il medico. Il caduto
  diventa bersaglio a **priorità molto bassa**. *I valori di KI #92 sono un tampone fino ad allora.*
- **Ri-valutazione dell'approccio durante Hunt**: oggi si sceglie una volta all'ingresso. Solo se un
  playtest lo mostra.
- **A6 obiettivi nel decisore**, A7 repertorio azioni, A8-A10 BT/HTN/GOAP: invariati, in coda.
- **B6 pipeline Blender**, B7 chokepoint, B8 stanze, B9 cover distruttibili: binario mondo, fermo da 104.
- **Pose/animazioni**: bloccate su richiesta dell'utente (attende PC + Blender). Il muzzle interim e il
  fix dei muretti bassi restano band-aid da rimuovere quando si sbloccano.

### Cosa NON è aperto (chiuso e misurato, non riaprire senza nuovi dati)
KI #86 cause 1-4 (mira disallineata, hide congelato, ricerca a caso, manovra congelata) · KI #88 (classi nel
roster) · KI #89 ("ignorano i vicini": misurato 0%, è leggibilità) · A1-A5.

## ★ PIANO DIRETTORE AI + MONDO TATTICO (doc 40 + doc 41, dal 2026-07-27)
Due binari **paralleli e indipendenti**: l'AI può migliorare senza attendere la pipeline dati, e viceversa.
Ordine dentro ogni binario, non fra binari. Ogni voce è verificabile (`--sim-ticks`, `--validate`, telemetria).

> **P0 in corso — KI #86 (ingaggio).** Due cause strutturali trovate e corrette (changelog 118): punto di
> mira dell'acquisizione disallineato dal gate di fuoco (**13,2%** delle acquisizioni), e fase di hide che
> si congelava fino a **26,6 s** con il fuoco bloccato. **Causa 3 diagnosticata** (changelog 119): il
> 61-72% di perdita in LOS **non** è geometria muta — il controllo statico `UnmarkedCover` trova 4 soli
> ostacoli non marcati su Training Ground, e i tre grandi bloccanti hanno coperture autorate a 3,3-5,4 m.
> La perdita è in gran parte fisiologica per uno sparatutto a coperture. Il buco sul lato AI **non** era
> l'approccio (`enterHunt` pesa già una posizione di tiro) ma la **RICERCA**: punto uniformemente casuale,
> l'unica decisione che ignorava il mondo tattico. **Corretta** (changelog 120): 76% delle ricerche ora
> chiede al mondo una posizione da cui la zona si vede; acquisizione 28% → 32% del cono.
> **Causa 4 corretta** (changelog 121): la manovra si fermava alla perdita del contatto — non esisteva un
> ramo di movimento per "Alert senza bersaglio". Trovata con la nuova **scatola nera per-agente**
> (`AiTrace.cpp` + `--trace-ai <id>`). Stalli **41 → 13**, tempo perso 122 → 46 s-AI, combattimento
> 233 → **281**. **Residuo**: 13 episodi, 8 senza causa evidente — il tetto attuale, da guardare col
> microscopio quando servirà.
> **Resta da decidere**: se serva una ri-valutazione dell'approccio *durante* Hunt (oggi si sceglie una
> volta sola all'ingresso). Da fare solo se un playtest lo mostra — non d'istinto.

### Binario A — AI (doc 40)
- ✅ **A1 Percezione: FOV + udito** (changelog 100). Misurato: fuori-campo 2000-3100/report, spari uditi
  72-138. **Smoke test utente in sospeso** (aggirare senza essere visti; sparare e farsi sentire).
- ✅ **A2 Confidenza sui contatti** (changelog 111): `c(t)=c₀·e^(−t/τ)`; vista 1.0 / udito 0.4; sotto soglia il
  contatto diventa META DI PERLUSTRAZIONE invece di bersaglio. Misurato: entrambi i rami vivi (fino al 53% di
  investigazioni), `fermi` 0, combat 116→128. **Fase 1 AI COMPLETA** (percezione + memoria).
- ✅ **A3 Soppressione** (changelog 112): near-miss entro 2.2 m → soppressione con decadimento; effetti su
  mira, copertura e blocco delle manovre allo scoperto. Misurato: inchiodati **12-26%** dei tick soppressi
  (preme senza paralizzare), manovre non crollate, `fermi` 0, combat 128→**151**. **Smoke test dovuto.**
- ✅ **A4 Ruoli di combattimento** (changelog 114): ruolo assegnato all'ingaggio per saturazione + affinità di
  profilo (sopprime/aggira/avanza); chi sopprime non manovra; rilascio alla perdita del contatto. Misurato
  ≈55/30/18%, manovre salite, `fermi` 0. **Fase 2 COMPLETA.**
  - ▶ Resta il comando player **"Suppress"** (ora ha una meccanica sotto) e da valutare in playtest se il
    combattimento risulta "manovrato" o "fiacco" (eventi combat 151→128, vedi changelog 114).
- ✅ **A5 Utility formalizzata** (changelog 116 + 117): **ispettore** nel dump di stato (`facing_deg`,
  `fov_deg`, `target`, `suppression`, `role`, `evading`, `reposition` — "perché questo agente ha scelto
  quella posizione") + gli **8 bilanci di pesi in `include/mini/game/ai/AiUtility.hpp`**, non più numeri
  magici in nove formule su tre file. Refactor a comportamento invariato: `--sim-ticks` **128 = baseline**.
  - ▶ Resta da fare, ora che è possibile in un posto solo: **tarare** le curve verso doc 40 §6. Volutamente
    NON fatto insieme al refactor (altrimenti una differenza non sarebbe attribuibile), e da fare **dopo**
    KI #86 — tarare pesi mentre l'ingaggio ha un bug significa inseguire il sintomo sbagliato.
- ▶ **A6 Obiettivi nel decisore** (doc 40 Cucitura 2): l'utility passa da "quale settore" a "quale
  obiettivo, e quale settore lo serve". `ObjectiveSystem` esiste ma **non alimenta l'AI**. *Dipende da:
  A5 + B3. È l'abilitatore reale delle mappe profonde con approcci alternativi.*
- ⏸ **A7 Repertorio di azioni dichiarato** (`requires`/`provides`) — Cucitura 3. Costa poco, e rende un
  risolutore GOAP-like (o un HTN) un'AGGIUNTA invece che una riscrittura.
- ⏸ **A8 BT per manovre multi-passo** · **A9 HTN** (quando gli obiettivi saranno profondi) ·
  **A10 risolutore stile GOAP** su azione bloccata, profondità ≤3 (trigger: quando ci si ritrova a scrivere
  a mano il 3°/4° ripiego annidato). Regola: **una tecnica per tipo di domanda** (doc 41 §8).

### Binario B — Mondo tattico e pipeline dati (doc 41)
- ✅ **B1 Formato PREFAB + espansione al load** (changelog 101). Verificato: 1 prefab × 2 istanze ruotate
  0°/90° → 6 box + 6 posizioni, `--validate` 0 errori, Training Ground invariata. Parser condivisi
  mappa↔prefab (uno schema solo). Riferimento rotto = messaggio esplicito.
- ✅ **B2 `fromPrefab` sui nodi tattici** (changelog 101) — derivato vs autorato, prerequisito per rigenerare
  senza perdere il lavoro manuale.
  - **Limite noto**: l'editor non conosce ancora i prefab (→ B5); non li mostra ma **non li perde**
    (`saveJsonRMW` preserva le chiavi altrui — verificato).
- ✅ **B3 Copertura dall'ALTO derivata** (changelog 102) — `worldintel::hasOverheadCover`, calcolata in
  `buildTacticalLinks`, mostrata in editor e nel log. Misurato: Training Ground 25/167, firebase 0/60.
  **NON è "indoor"**: è "c'è qualcosa sopra" (un sottopasso conta quanto un bunker). L'interno vero richiede
  il rilevamento della CHIUSURA → B8. *Consumatori presenti dal primo giorno (lezione KI #25b).*
- ✅ **B4 Salute tattica** (changelog 103): `analyzeTacticalHealth` in **ContentValidation** (regole condivise
  editor ↔ `--validate`, mai duplicate) — 5 difetti: non copre nessuno · cieca cross-quota · esposta ≥55% ·
  ridondante <2 m · settore senza posizioni (severità per importanza). Pannello editor cliccabile + gate
  headless. Misurato Training Ground **5 problemi / 53 avvisi**. **Smoke test editor dovuto.**
  - ✅ **Rifinito col playtest (changelog 104)**: bug ridondanza (ignorava il `facing` → segnalava posizioni
    con versi opposti) → avvisi Training Ground **53→19**; avvisi raggruppati per tipo con tendine;
    `NoCoverage` distingue **ISOLATA** (problema) da "avanzata" (avviso). Prefab corretto (era colpa mia).
  - **▶ Difetto REALE residuo da correggere in authoring**: su **firebase**, `vantage 45` e `vantage 46` sono
    **ISOLATE** (non coprono nessuno e nessuno le batte) → nessuna AI le userà mai. Training Ground e Prefab
    Test: **0 problemi**.
- ✅ **B5 Editor: piazzamento prefab** (changelog 105) — lista + combo + "+ Piazza", anteprima nel viewport
  con la stessa trasformazione del motore, gizmo + rotazione, riferimenti rotti visibili, save dei soli
  riferimenti. `loadPrefabs` pubblica → l'editor usa il loader del runtime, nessun secondo parser.
  **Smoke test dovuto** (piazzare, salvare, riaprire, verificare in partita).
  - **Binario B: fondamenta COMPLETE** (B1-B5). Restano gli avanzati: B6 Blender · B7 choke point ·
    B8 stanze/ingressi · B9 copertura distruttibile.
- ⏸ **B6 Pipeline Blender**: convenzioni `UCX_` (collisione) / `TP_<ruolo>` (posizioni) / `ENTRY_`,
  importatore GLB→prefab. **Rischio più alto del piano** (pipeline esterna, qualità asset).
  *Dipende da: B1. Non iniziare prima: importare mesh senza un posto dove mettere il significato
  ripeterebbe il fallimento di ADR-026.*
- ⏸ **B7 Choke point** (analisi del grafo navmesh) · **B8 stanze/ingressi** · **B9 copertura distruttibile**
  (riusa `strategicTargets`: HP + collider già presenti).

### Vincoli di scala da affrontare PRIMA delle mappe profonde
- **`buildTacticalLinks` è O(N²)** — **misurato** 2026-07-27: 60 pos → 0.43 ms; **167 pos → 7.9 ms**.
  Estrapolando: 500 → ~70 ms, 1000 → ~280 ms, 1500 → ~640 ms (inaccettabile al load).
  *(Correzione: una stima precedente diceva ~20 ms a 500 — la realtà è ~3× peggio.)*
  Mitigazione: limitare le coppie alla gittata massima con griglia spaziale (→ ~O(N·k)).
  **Soglia d'intervento abbassata a ~300 posizioni.** Oggi non è un problema.
- **`allyTac`** è O(posizioni × nemici) ogni 0.33 s → filtrare ai soli settori contesi quando servirà.

### Cosa NON fare (deciso, con motivo)
ML/ONNX a runtime (uccide determinismo/debug/autorabilità — ML solo **offline** per tarare i pesi) ·
auto-generazione di posizioni dalla geometria (ADR-026, già fallita) · mesh Blender nella collisione
(ADR-047) · nuove strutture spaziali finché il profiling non le chiede · room clearing prima di B3/B8.

## ▶ USO SENSATO DI COVER/METADATA (playtest 2026-07-23)
- **✅ Fix 1 — cerca copertura all'ingaggio (changelog 67)**: entrando in Alert, valutazione proattiva di
  `bestFiringPosition` (riusa ADR-035, non un sistema nuovo). Misurato: uso posizioni di tiro su, `fermi=0`.
- **✅ Fix 2 — arco morbido + importanza (changelog 68)**: arco = preferenza (non esclusione) + importanza
  nel punteggio → posizioni di tiro trovate 92%, manovre triplicate. LOS-y accurata provata e scartata
  (riduceva l'uso — rivela che molte elevate non hanno tiro pulito a terra; tenuta permissiva).
- **✅ Fix 3 — modello copertura "peek" (changelog 69)**: la LOS di tiro si sporge (1.5m avanti) invece
  di partire dal centro dietro la cover → la propria copertura non blocca più il tiro. `tiro_trovato` 100%.
  Intuizione utente: "una cover blocca parte della visuale, sennò non ripara". Grafo overwatch non toccato.
- **▶ Ancora aperto (playtest dirà)**: le AI restano un po' meccaniche; possibile prossimo giro —
  reachability anche sul target di riposizionamento in combattimento (ora `bestFiringPosition` non la
  controlla); e i **droidi** che restano fermi/scoperti (estendere il "cerca-copertura-all'ingaggio" — è
  già di entrambe le fazioni, ma verificare).

## ▶ PLAYTEST utente 2026-07-23 (3 problemi su Training Ground)
- **#1 Performance** → vedi sezione sotto (✅ per ora).
- **#3 Cloni idle vicino allo spawn / torre di controllo** → **✅ (changelog 63)**: `pickAllySignal` non
  ripiega più su idle quando i segnali sono saturi (rinforza il fronte più vicino → avanzata coerente,
  `segnali_seguiti` da 0 a 600-3457); nuova `bestAdvantageInArea` fa puntare ai cloni le posizioni
  vantaggiose (incl. elevate ad alta importanza) invece del centro. Da confermare in playtest.
- **▶ #2 Ponti/zone rialzate + danger-blocco + trappole (playtest 2026-07-23)** — DIAGNOSI PROFONDA e fix
  (changelog 65): danger costo navmesh 10×→2× (non più muro); `NavManager::isReachable` + i cloni usano
  vantage solo se raggiungibili + commitment su waypoint + recupero stuck (niente più trappole); voxel
  navmesh 0.30→0.20 (passaggi stretti raggiungibili). **RESTA**: (2b) cablare `bestAdvantageInArea` +
  reachability anche nei **droidi** (Advance); e authoring dell'utente — scale con gradini ≤0.55m (sopra,
  non le sale né fisicamente né nel navmesh) e passaggi ≥~0.8m. **Da riconfermare in playtest.**
- **▶ Sanitizer**: `/RTC1` tolto quando ASan ON (fatto). MA la build usa **VS 2022 Community (14.44)** e
  il componente ASan è in un'ALTRA installazione VS → link error thunk lib. Utente: installare "C++
  AddressSanitizer" **nella VS 2022 Community** (VS Installer → Modifica → Componenti individuali). Poi
  `-DGF_ENABLE_ASAN=ON` builda; a runtime DLL `clang_rt.asan_dynamic-x86_64.dll` sul PATH.
- **Cleanup minore**: warning C4189 preesistente in SandboxMenu.cpp (`PH` non usato in handleMouse).
- **✅ #3-bis Cloni indietro / rework torre (changelog 64)**: commitment del clone (`allySig*`, resta sul
  segnale finché non lo raggiunge/sparisce → fine oscillazione) + analisi tattica più ricca del settore
  (minoranza→rinforza, valore poco difeso→sfrutta). Torre = info/analisi, non ordini
  ([[control-tower-informs-not-orders]]). Verificato build; effetto visivo da confermare in playtest.
- **▶ Futuro richiesto dall'utente (2026-07-23)**: (a) ~~usare **ruota ordini + ordini rapidi** anche in
  OSSERVAZIONE della sim sandbox~~ **FATTO 2026-07-23 (changelog 70)**: osservatore-comandante, riuso
  della pipeline ordini (ADR-020) con ancora = punto mirato a terra (`crosshairGround`); build-verified,
  smoke test manuale da fare. (b) mappa tattica + controllo obiettivi/priorità + **chain of command**
  ([[command-rank-system]]) — grande blocco futuro.
- **▶ Rework ordini più tattici (2026-07-23/24, [[orders-design-vision]])**
  - ⛔ **Primo tentativo REVERTATO (changelog 77)**: Hold custom (72), Advance continuo (73/74), reposition-gate
    (76) → scavalcavano l'AI autonoma con una LOS/tattica parallela e la disconnettevano dai bersagli (Advance
    senza bersaglio 81% del tempo). Base tornata stabile. **TENUTI**: LOS occhi (75), ruota sandbox (70).
  - ✅ **Provato**: il tiro dell'AI funziona anche da cover/ponte (29/31). Il "non sparano" era **posizionamento**
    (stavano su posizioni senza bersaglio), non tiro (KI #79).
  - **▶ NUOVO APPROCCIO (concordato)**: gli ordini come **bias leggero sull'AI autonoma** (che già bounda e
    sceglie firing position che affacciano i nemici), NON override. Advance = spinta aggro/fronte in direzione;
    Hold = abbassa aggressività + ancora l'area (leash); Follow = segue il leader; Retreat = ripiega; Regroup =
    zona priorità. Lasciare a `enterHunt`/reposition la scelta delle posizioni. Vedi KI #80b.
  - **▶ Ripresa 2026-07-26 (base ormai solida) — una postura alla volta, bias non override:**
    - ✅ **HOLD (changelog 89)**: l'ordine dà il CENTRO dell'area; ogni membro sceglie da sé la miglior posizione
      (`bestAdvantageInArea`, anti-ammasso via bias) e la presidia (clamp `holdRadius`), combattendo da lì.
      Riusa il presidio droidi. Build-verified; **smoke test visivo da fare** (non testabile in `--sim`: niente player).
    - ✅ **ADVANCE (changelog 90)**: `OrderType::Advance` + helper `advanceWaypoint` (firing position verso il
      nemico nell'area + commitment + sbalzo), riusa la macchina dei droidi senza toccarla. Guinzaglio largo
      all'area (no caccia infinita), persiste come postura. Build-verified; **smoke test visivo da fare**.
    - ✅ **Ruota a 5 settori + FREE dedicato (changelog 90)**: ADVANCE/HOLD/FOLLOW/REGROUP/FREE (prima 4, Liberi
      era solo toggle di Follow). FREE libera sempre dagli ordini (richiesta utente).
    - ⚠️ **89-91 erano APPROSSIMAZIONI** (playtest utente: Retreat andava avanti, Hold si ammassava, ecc.).
    - ✅ **MOTORE UNIFICATO occupancy-aware (changelog 92)**: `worldintel::bestFreePosition` (posizione libera a
      priorità max, tutti i ruoli utili incl. observation, filtro direzione) + `selectOrderWaypoint` (commitment
      + reachability) + occupancy `m_claimedPositions`. Hold difende la miglior posizione libera; Advance salta
      verso il nemico; Retreat salta LONTANO dal nemico (bug "andavano avanti" risolto); Follow cover-to-cover
      attorno al leader; Regroup sul settore conteso a peso max. Build-verified; **smoke test visivo da fare**.
    - ✅ **Fix post-playtest (changelog 93)**: Retreat ora arretra ANCHE in combattimento (attiva `retreating`);
      niente più cloni fermi (fallback del selettore corretto per postura).
    - ✅ **TORRE-HUB (changelog 93, scelta utente "dati + occupancy centralizzata")**: la torre (`updateAllyTactical`,
      0.33 s) pre-calcola per posizione se BATTE un nemico (LOS) + score; `bestOrderPosition` sceglie la posizione
      libera a score max con **occupancy centrale** (`World::allyTac`), per ordini E cloni autonomi → meno calcolo
      per-clone, posizioni che sparano, no ammasso. Ramo autonomo sim-verificato (canFire 43-122/167, no regressione).
      Ordini del player da validare **visivamente**.
    - Se il visivo conferma, il rework ordini è completo per questa fase. Futuro: ordini rapidi per-membro;
      coordinamento più ricco dalla torre (ruoli, bounding coordinato).
- **▶ VERTICALITÀ — è AUTHORING, non codice (KI #83, changelog 96)**: misurato che l'AI ingaggia tutto ciò che
  vede a quota diversa e spara (visibili = acquisiti 1:1, tiro mai bloccato); il limite è che solo l'1-3% dei
  nemici cross-quota è VISIBILE, per la forma della mappa (piattaforme = blocchi pieni 7.8×8.3×3.1 → serve
  ≳12 m orizzontali per vedere a terra; più pareti alte 3 m diffuse).
  - ✅ **Strumento EDITOR "visuale verticale" FATTO (changelog 97)**: per ogni posizione, `X / Y` posizioni a
    quota diversa viste (stessa `hasLineOfFire` e stesse quote occhi/corpo del runtime). Riepilogo
    `Verticale: N/M cieche` sopra la lista, `!` sulle voci difettose, rombo rosso nel viewport sulle cieche.
    Build-verified e **confermato dall'utente 2026-07-27** (il conteggio reagisce allo spostamento delle
    posizioni). [[editor-accessibility-priority]]
  - Leve di authoring immediate (senza attendere lo strumento): posizioni sul BORDO delle piattaforme,
    parapetti ≤1 m invece di pareti 3 m sul perimetro, o sporgenze/balconi.
- **▶ DEBITO TECNICO (audit 2026-07-27, changelog 94)** — sistemati #1/#2/#3/#5/#10; rimandati:
  - **✅ #7 Split del monolite FATTO (changelog 95)**: `AiSystem.cpp` 2578 → **1805** righe; nuovo
    **`AiCommandLayer.cpp`** (843) col livello di comando (settori, torre, quadro tattico, direttive del Droide
    Tattico, selezione posizioni per gli ordini) + seam privato `AiInternal.hpp` (`namespace mini::aicmd`).
    Spostamento verbatim, `--sim` 206 identico prima/dopo, build Debug+Release pulite.
    - **▶ Resta il ciclo per-entità (~1200 righe) dentro `update()`**: spezzarlo richiede di sciogliere decine di
      variabili locali condivise fra le fasi (bersagli SoA, `teamAlive`, `repositioning`, `moveDX/DZ`…). Refactor
      a sé, rischio/beneficio diverso — valutare solo se quel codice tornerà a dare problemi.
  - **#4** la retry di reachability marca `allyTac.claimed` le posizioni irraggiungibili per l'intero tick →
    saltate anche da compagni che le raggiungerebbero (impatto basso: le isole sono irraggiungibili per tutti).
  - **#6** quadro tattico torre stale ≤`TAC_PICTURE_PERIOD` (0.33 s): un clone può puntare una posizione verso
    un nemico appena morto (impatto minimo).
  - **#11** Regroup è no-op se nessun settore è conteso (early game senza contatto) → il membro resta fermo.
  - ✅ **Ordini a MEMBRI SPECIFICI (changelog 98)**: `directedMembers` (lista) + selezione mirando il compagno
    (toggle), rispettata da ordini rapidi E ruota → gruppi diversi con ordini diversi. Auto-pulizia della
    selezione (respawn = entità nuove). HUD `[SEL n]`. **Smoke test manuale da fare.**
    - Conseguenza: CoveringFire non è più sul tasto rapido (si ottiene con selezione + HOLD dalla ruota).
      Se servirà un accesso diretto, riesporlo senza affollare la ruota.
  - **▶ Futuro**: ambito ristretto alla "piccola squadra del player" (oggi la squadra = tutti gli alleati
    team-1). Rimandato per scelta: in 6v6 la squadra è già piccola e la selezione copre il bisogno; da
    riprendere quando le partite saranno più grandi.

## ▶ PERFORMANCE (playtest utente 2026-07-23): lag con ~25 AI su Training Ground
- **✅ Indice spaziale collisioni/LOS (changelog 62)**: `hasLineOfSight`/`hasCollision` da O(tutte le
  entità) a O(celle vicine). Corretto, verificato.
- **Diagnosi**: il costo AI scala col **numero di entità-geometria** (175 box), non con le AI (12→50 AI:
  +10%). ~10 passaggi pre-loop iterano tutte le entità. E le misure erano in **Debug** (non ottimizzata).
- **✅ `snap` solo-team (changelog 62)**: i passaggi AI iterano solo entità con `Team` (no box di
  geometria). Comportamento invariato, verificato. Taglia il costo che scala con la dimensione mappa.
- **✅ Build Release costruita/verificata + confermata dall'utente**: "lag migliorato in maniera
  evidente" giocando la Release. Debug è 10-30× più lenta. **Capitolo perf chiuso per ora.**
- **▶ Futuro (l'utente prevede più AI e mappe più complesse)**: indice spaziale anche per le query
  tattiche/`hasLineOfFire`; profilare crowd/rendering quando le AI crescono oltre.

## ⇒ DIREZIONE ATTIVA (2026-07-19): Milestone "Vertical Slice v1" → poi consolidamento
Deciso dopo studio completo di Vision/GDD/Bridge/CurrentState: **31_ConsolidationMilestone.md**.
Siamo sul confine Fase 1 → Fase 2: i sistemi tattici (25+26) sono costruiti ma l'esperienza non è
ancora *dimostrata*. Il punto da raggiungere è **una missione completa che prova il core loop**
(squadra che conta + bestiario ≥3 ruoli tattici + obiettivi > kill + fronte mobile + briefing/debrief);
poi si **consolida orizzontalmente** (authoring editor, feel combat, espressione AI, HUD/debrief,
salute codice, validazione). Unico codice nuovo per arrivarci: il **Droide Tattico** (comandante)
→ **✅ FATTO v1 2026-07-20 (ADR-024 riscritto, doc 32)**: ability `command` → `CommanderComponent`;
`AiSystem` pubblica un focus strategico (`World::enemyCommand`) e i droidi vi convergono; ucciderlo
rompe il coordinamento. **Uno per mappa** (campo `MapDef.commander`, non nel roster), spawnato
**stationary** nelle retrovie (sta fermo, si difende soltanto). Resta come **dati/futuro** (non codice):
ordini più ricchi, UI di piazzamento MapEditor, gerarchia gradi/ufficiali ([[command-rank-system]]),
entità-a-sé (doc 32 Out of Scope).
Rimandati esplicitamente: progressione (27), persistenza (28), meta (Fase 4/5). Vedi doc 31 per i
criteri di accettazione e gli assi di consolidamento.

**DIREZIONE (2026-07-20): prima la BASE del sistema tattico, poi rendere intelligente il Droide
Tattico.** Direttiva utente: per far decidere bene AI/squadre/comandante servono metadata ricchi.
Piano a fasi in **33_WorldTacticalIntelligence.md** — filosofia "AI semplici in un mondo intelligente"
([[world-tactical-intelligence]]). Ogni fase = un ADR; NON tutto in una volta.
- **✅ Fase 0 FATTA (ADR-025)**: query layer `mini::worldintel` (seam), doppia-verità danger risolta,
  ruota/scala sui metadata (KI #60). Build/validate/sim OK.
- **✅ Fase 1 FATTA (ADR-026)**: Cover Intelligence — `CoverPointDef` += protezione/canShoot,
  `bestCoverToward` (scelta per protezione), editor slider/checkbox. Retrocompatibile.
  (Auto-gen coperture RIMOSSA su feedback: mappe handcrafted → auto-gen de-scoped, doc 33 §6.)
  Build/validate/sim OK.
- **✅ Fase 2 FATTA (ADR-027)**: Tactical Points — `TacticalPointDef` (vantage/defensive/chokepoint/
  observation + importanza/raggio/fronte), loader, `nearestTacticalPoint` (seam), editor completo
  (lista/dropdown/slider/marker/gizmo). Authoring manuale. **Consumo = Fase 4/5**. + pulsanti gizmo
  cliccabili (KI #60). Build/validate/sim OK.
- **✅ Fase 3a FATTA (ADR-028)**: le pattuglie seguono la **route intera** (`patrolRoute/patrolSeg` +
  `advancePatrol`), segmento di partenza sfalsato, respawn conservano la route. Superato il limite
  2-waypoint: le route autorate diventano percorsi veri. Build/validate/sim OK.
- **✅ Route FLUIDE + obbedienti al comando (ADR-045, 2026-07-22)** — audit doc 38 P1+P2: le unità su
  route rispondono ad Advance/Retreat (prima sorde); route bidirezionali, raccolta dal punto più
  vicino, cambio route.
- **✅ Ruoli tattici + cover-evita-danger (ADR-046, 2026-07-22)** — audit doc 38 P3: `observation` →
  vista estesa; `defensive`/`chokepoint` → posizioni da tenere sotto `Hold` (`bestHoldPosition`);
  `bestCoverToward`/`bestFiringPosition` evitano le danger (`dangerAt` non più morta); rimossi
  `bestOverwatchFor` e `pickObjectiveSector`. **Audit doc 38 chiuso** (restano solo P5 overwatch
  marginale + note documentali B5/B6). Bug harness `--map` ignorato in `--sim` corretto (KI #77).
- **✅ Editor: creazione nuove mappe (2026-07-22)** — voce "＋ Nuova mappa…" in coda al dropdown mappe →
  popup nome/conferma; crea `data/maps/<id>.json` con schema minimo valido (floor + spawn) e ci passa.
  Build-verified (carica, valida, gira in sim).
- **▶ EDITOR UX & ACCESSIBILITÀ (doc 39, richiesta utente 2026-07-22)** — [[editor-accessibility-priority]].
  Prerequisito al salto di complessità: rendere il Map Editor uno strumento con cui si costruisce davvero.
  - **✅ F1 igiene UI (changelog 55)**: toolbar sfoltita (via i gizmo duplicati), "Nuova mappa" nel
    dropdown, fix click-through overlay. Da collaudare a mano.
  - **✅ F2 alzare/posizionare (changelog 56)**: campo `y` per le strutture strategiche (torri) —
    efficace perché statiche, verificato; comandante/veicoli restano a terra (gravità → una Y in aria
    cadrebbe) e i settori sono 2D. Creazione oggetti **davanti alla camera** (`groundFocusPoint`) invece
    che al centro, su tutti i "+ Aggiungi". Da collaudare a mano.
  - **✅ F3 superfici (changelog 57)**: facce piene ombreggiate sui box + toggle "Solido" (ADR-003
    rispettato: opaco, stesso pipeline). Aree 2D restano contorni (servirebbe blending). Da collaudare.
  - **✅ F4 metadata senza attrito (changelog 58)**: "Duplica" generalizzato a ogni metadato (copia tutti
    i campi). Default già sensati. Pennello click-per-posare resta futuro.
  - **▶ F5 Training Ground banco di prova** — IL SALTO DI COMPLESSITÀ. Editor pronto (F1-F4). **Misura
    2026-07-22 (6v6, mappa caricata correttamente)**: la mappa **funziona** — comandante attivo a 3
    fronti (Alpha/Bravo/Delta), combattimento, route/osservazione/manovre scattano. Gap reali rimasti:
    (a) ~~0 danger zones~~ → **✅ aggiunte 7** (artiglieria centro + 2 corsie + 4 chokepoint minati) per
    esercitare cover-evita-pericolo; l'utente rivede/aggiusta le pose nell'editor; (b) `overwatch` → **✅
    RISOLTO (changelog 61)**: il segnale d'avanzata viveva 1 tick mentre la manovra dura ~6s → TTL sul
    segnale, ora scatta; (c) `hold` → **✅ RISOLTO (changelog 60)**: comando TIENI sugli obiettivi
    catturati + droidi ancorati a posizione difensiva (opzione A). **F5: obiettivo raggiunto** — tutti i
    sistemi scattano insieme (overwatch 4, hold 543, osservazione 597, route 54, manovre 19, contatti
    295, fermi 0). Resta authoring incrementale + rifinitura del feel. Attenzione harness: id mappa con
    spazi vanno quotati ([[powershell-quote-args-with-spaces]]).
- **DIREZIONE CONFERMATA (utente 2026-07-20)**: continuare a **migliorare i metadata** il più
  possibile, e **poi** rendere più intelligenti sia le **AI normali** sia quella del **Droide
  Tattico** — nel senso di *usare meglio le informazioni che hanno, analizzare meglio la situazione
  e prendere decisioni più coerenti e tattiche*. Vincolo permanente: il comandante dà **intento**
  (obiettivi + advance/hold/retreat), **i droidi decidono il come** ([[droide-tattico-concept]]).
- **▶ REVISIONE Droide Tattico + Torre (2026-07-26, in corso)** — su base ormai solida (importanza reale,
  distribuzione-per-contesa, verticalità, multi-spawn).
  - **✅ Coerenza peso comandante↔torre (changelog 85)**: il comandante ora pesa le direttive come la torre
    (`importanza + pressione×2 + minoranza + opportunità`), non più `importanza×(1+pressione)`. I due lati si
    comportano allo stesso modo. Smoke test visivo Release da fare.
  - **✅ Assegnazione SPAZIALE delle direttive (changelog 86)**: `pickEnemyDirective(bias, x, z)` modula il peso
    per la prossimità (`COMMAND_PROXIMITY_HALFDIST`) → i droidi servono il fronte per dove sono, la distribuzione
    resta (bias + weight). Era la precondizione del ripiego per-fronte.
  - **✅ Ripiego PER-FRONTE (changelog 86)**: il comandante marca un fronte che collassa (cloni ≥ droidi+2, terreno
    nemico) come Retreat; i droidi vicini cadono sul settore controllato più vicino (`retreatPointForTeam2`). A 6v6
    è una **valvola di sicurezza** (il collasso +2 non si verifica in gioco normale — i droidi spesso dominano);
    percorso collaudato forzando la soglia. Smoke test visivo Release consigliato.
  - **✅ Copertura strutturale delle CORSIE (changelog 87)**: primitiva condivisa `worldintel::lateralCoord`
    (proiezione laterale sull'asse d'attacco) + selezione lane-diverse nel comandante (prima una corsia diversa
    ciascuna, poi riempi per peso). Torre non toccata (segnala già tutti i settori). Misurato: top-3 sempre in
    3 corsie distinte. Smoke test visivo Release consigliato.
  - **✅ DRY analisi settori (changelog 88)**: `sectorTacticalWeight(sec, st, myTeam)` unico, chiamato da torre
    (`,1`) e comandante (`,2` + bonus stance). Comportamento identico (verificato per costruzione); i due lati non
    possono più divergere. Revisione droide tattico + torre **COMPLETA** per questa fase.
- **▶ PRIORITÀ RIVISTA (utente 2026-07-20): completare i METADATA, poi l'AI.** Si mette in pausa il
  miglioramento dell'intelligenza AI e si finisce il percorso metadata "con il massimo della cura".
  Piano in **doc 33 §5-bis**. Ordine scelto dall'utente: **unificare prima**, così M1/M3/M4 si
  costruiscono una volta sola. Visibilità: **calcolata al load** (sempre coerente con la geometria).
  - **✅ M2 FATTA (ADR-030)**: `TacticalPositionDef` unica (ruolo + protezione/altezza/canShoot/
    importanza/raggio); query per **capacità** non per ruolo; migrazione legacy trasparente.
  - **✅ M1 FATTA (ADR-031)**: settore di tiro (`fireArcDeg`/`fireRange`) + query
    `bestFiringPosition` → l'AI va su una copertura **per colpire**, non per sparire. Due query
    distinte: riparo vs posizione di tiro. Editor con settore disegnato.
  - **✅ M3+M4 FATTE INSIEME (ADR-032)**: `hasLineOfFire` su MapDef; la posizione di tiro verifica la
    linea di tiro (**cade il limite geometrico di M1**); grafo `positionCovers` "chi copre chi"
    calcolato al load (derivato, mai stale); `bestOverwatchFor`. Costo: **638 link / 60 pos in 2,4 ms**.
  - **✅ Aggiramento FATTO (ADR-033)**: `positionExposure` derivata (invertendo il grafo) +
    `bestFlankingPosition` (attacca da direzione diversa, preferendo il coperto). Le "corsie" sono
    espresse come **destinazione**, non come tracciato da autorare → zero authoring aggiuntivo.
    Editor mostra l'esposizione in sola lettura, con la stessa funzione del runtime.
  - **✅ M5 FATTA (ADR-034)**: `SectorDef` autorato + `sectorStates` runtime (presenze/controllo/
    pressione); il comandante sceglie l'obiettivo fra i **settori**, i droidi il punto dentro la zona.
  - **✅ PERCORSO METADATA COMPLETO.** Restano due follow-up minori: visualizzazione dei **link** in
    editor (verifica; sono derivati, non si autorano) e `purpose` delle route (se servirà).

**▶ FASE AI (in corso)** — far sfruttare davvero i metadata completati.
- **✅ Manovra in combattimento (ADR-035)**: l'AI ingaggiata si riposiziona (aggiramento / posizione
  di tiro) **continuando a sparare**, col pathfinding; bounding overwatch **emergente** dal cap di
  concorrenza. Effetto misurato: cambi di stato 11 → **33**, `stuck` → **1**. Era il gap che rendeva
  inutilizzati i metadata proprio durante lo scontro. Firebase ha ora **7 settori** autorati.
- **✅ Truppe indipendenti per default (ADR-037)** — direttiva utente #1 delle tre. Rimosso il
  `Follow` fisso che `SquadSystem` imponeva a chiunque non avesse ordini: era la causa reale del
  "si muovono tutti insieme / sempre le stesse strade / tutti aggregati", e rendeva i **cloni meno
  indipendenti dei droidi**. Ora il default è **nessun ordine**; `Follow` è il 4° settore della ruota
  di comando e lo stesso settore **revoca** (LIBERI). Misurato: `sq_follow` 9 → **0**, 10/10 senza
  ordini a inizio partita, 3 manovre avviate al picco.
- **▶ DIRETTIVE UTENTE RESTANTI (2026-07-20)** — asimmetria di fazione, vedi [[command-rank-system]]:
  - **✅ #2 Torre di comunicazione — FATTA (ADR-038, doc 34)**: `role: "comms"` + `World::comms` per
    fazione. Direttiva utente: **distruggerla NON blocca i rinforzi**, li rallenta — insieme a
    informazioni (raggio di condivisione dimezzato, avviso in ritardo → si accorre dove il nemico
    **era**) e ordini (il comando ri-decide 2.5× più di rado). Autorata su firebase per entrambe le
    fazioni; quella della Repubblica è in **posizione segnaposto, da riposizionare in editor**.
  - **✅ Interazione AI↔strutture — FATTA (ADR-039, doc 35)**: KI #70 era **tre bug** (LOS che non
    escludeva il bersaglio, mira all'origine a terra, LOS al tiro con entrambi i difetti), non una
    decisione mancante. Ora le AI abbattono le strutture, il comando le considera fra gli obiettivi e
    `World::strategicTargets` è la **sorgente unica di intel** che leggerà anche la torre di controllo.
    **Authoring pronto e inerte** (`priority`, `engage_radius`; 0 = mai di iniziativa): **i valori sulle
    mappe li autora l'utente**.
  - **✅ #3 Torre di controllo — FATTA (ADR-040, doc 36)**: `role: "control"` → `World::allyIntel`,
    una **lista** di segnali (settori contesi + strutture nemiche) da cui **ogni clone sceglie da sé**,
    decorrelato dal `bias`. **Nessun ordine, nessuna destinazione imposta.** Canale separato da
    `enemyCommand` per costruzione. Autorata su firebase in posizione **segnaposto**.
- **▶ FASE AUTHORING (prossima, richiesta utente 2026-07-21)** — audit completo in **doc 37**.
  Ordine consigliato, e il motivo dell'ordine:
  1. **KI #73** (torre di controllo che ammassa) → **✅ RISOLTO 2026-07-21** (saturazione:
     `ALLY_SIGNAL_CAPACITY`, un segnale coperto non attira altri, i restanti pattugliano).
     **B1 (grafo overwatch)** → **✅ CONSUMATO 2026-07-21** (l'utente: "provare a metterlo"):
     `bestOverwatchForPosition` legge `positionCovers`, chi non avanza copre chi avanza. Funziona ma
     scatta di rado su firebase — se lo si vuole vedere di più servono posizioni che si coprano meglio.
  2. ~~**Estendere il gate ADR-018**~~ → **✅ FATTO 2026-07-21**: `role` validato (niente più
     normalizzazione silenziosa), `hp`, `engage_radius` inerte, torre di controllo di team 2, torri
     duplicate, asimmetria fra fazioni. Ha subito intercettato le 3 strutture di firebase a
     `engage_radius: 1`.
  3. **Rendere autorabile** (decisioni di casa prese con l'utente 2026-07-21):
     - `hunt_timeout` → **✅ FATTO**: nei profili AI (carattere).
     - rianimazione: **cap → ✅ FATTO** (2ª caduta uccide). **✅ TUTTI E 6 i parametri squadra +
       i 4 `comms_lost_*` sono ora AUTORABILI (ADR-043, 2026-07-21)**: `data/config/gameplay.json` +
       tab **Gameplay** nel BalanceEditor (slider, salva RMW, ripristina default). Verificato
       end-to-end (cap=0 → zero cadute a terra). NB: i `comms_lost_*` stanno per ora nel tab Gameplay;
       se nascerà l'editor "Strutture & Comando" (ADR-041 §4) migreranno lì.
       Resta il **per-classe** (medico che rianima prima / alza il cap): moltiplicatore sopra il
       globale, col sistema classi.
     - soglie tecniche dei contatti (`MERGE_*`, `TTL`, `FRESH`) → **non esposte**: tarature interne.
  4. **Droide Tattico** (ADR-041): **✅ Fase 1 FATTA 2026-07-21** — spawn dedicato con raggio di
     **leash** autorabile nel MapEditor (marker, pannello, gizmo), comportamento AI (non insegue,
     clamp nel raggio). Chiude anche la UI di piazzamento del comandante.
     **✅ Stance v2 FATTA 2026-07-21 (ADR-042)**: `enemyCommand` = lista di direttive, 3 fronti con
     stance per-settore, droidi distribuiti, ripiegamento globale — fine del "sempre avanzata".
     **✅ Fase 2 FATTA 2026-07-22 (ADR-044)**: migrato fuori da `class` → `CommanderDef`.
     **✅ Editor "Comando" FATTO 2026-07-22 (changelog 50)**: tab BalanceEditor che autora i
     CommanderDef + i `COMMS_LOST_*`. **Rework del Droide Tattico sostanzialmente COMPLETO.**
     **Restano** (non bloccanti): ruolo comando implicito nel tipo (raffinamento); entità-a-sé con
     corpo proprio (dipende dal tooling mesh); e il **grado intermedio** ([[command-rank-system]]) che
     interpreterà le direttive per il micro del gruppo — sistema a sé, futuro.
  5. ~~**`Hunt` che scade**~~ → **✅ FATTO 2026-07-21** (KI #68 chiuso): 20 s → Search → Patrol.
- **▶ Altri candidati** (da scegliere dopo il playtest): (a) il **comandante** che usa
  `sectorStates` per una stance per-settore invece che globale; (b) **coppie di overwatch esplicite**
  dal grafo `positionCovers` (chi copre chi) invece che emergenti; (c) rifinitura del *feel*
  (quanto spesso manovrano, distanze, timer) in base a come si vede in partita.
Poi la mappa più grande, e con essa il sistema di **geometrie oltre i box** (registrato sopra).
  - Solo DOPO: la fase AI che sfrutta tutto questo (bounding overwatch, aggiramenti, pathfinding
    semplificato dai metadata), e poi la mappa più grande.
  - Solo DOPO: sistemi AI che li sfruttano, e poi una mappa più grande.
- **REGISTRATO, NON ORA — geometrie oltre i box**: mappe complesse da Blender lette e trattate
  correttamente (impatta collisioni, navmesh, hitbox). Sistema a sé, con proprio ADR, **dopo** i
  metadata. L'attuale limite a cubi/parallelepipedi è riconosciuto come vincolante.
- **Fase 3b** (filtri navmesh per-ruolo, già pronti in doc 22): resta pianificata, dopo i metadata.
- **Rimandati di proposito (3c)**: grafo tattico fra Tactical Points + `purpose` delle route → quando
  esisteranno i consumatori (Fase 4/5), altrimenti dato decorativo.
- Poi: **settori (4 — rende intelligente il comandante)**, Squad layer entrambi i team (5),
  predisposizione simulazione (6).

## PROSSIMO SALTO — derivato da GDD + master plan (2026-07-15)

Contesto: la **Fase 1 è essenzialmente completa** (05_CurrentState) e sopra ci sono già sistemi
di respiro Fase 2/3 (nav ADR-017, telemetria ADR-016, ottimizzazione ADR-015). Il confronto con
il GDD (nuovo doc 23) dice che il gap non è più tecnico: **è di design**. Ordine proposto —
discutibile, ma questa è la logica.

**N1. Squad & Command (doc 26, ADR-020) — il salto più grande. → Phase A+B FATTE 2026-07-15.**
Stato: `SquadComponent` + `SquadSystem` (fra Combat e Ai) in force; squadra alleata, default
`Follow`, ciclo di vita con telemetria, ordini non implementati che falliscono con causa. **Phase B**:
comando contestuale a un tasto (G) risolto dal mirino → `FocusFire`/`TakeCover`/`MoveTo`, mailbox
`World::squadOrder`, raggiungibilità verificata prima di impartire, HUD pannello SQUADRA + esiti nel
feed. 4 criteri di accettazione del doc 26 su 5 soddisfatti (vedi 07_Changelog per i numeri).
**~~Phase C~~ → FATTA 2026-07-17**: stato "a terra" + rianimazione (per prossimità + auto-soccorso
con ordine `Revive`) + bleed-out; intercettazione additiva in `CombatSystem`, HUD, costanti in
GameConfig. Verificata in `--sim`: down/revive/bleed-out tutti e tre visti scattare. Le perdite ora
pesano (doc 26). **~~ruota di comando~~ → FATTA 2026-07-17** (tasto B tenuto → mouse sceglie
Regroup/Hold/Advance; HUD radiale; mirino verde sugli alleati). Aggiunti anche i comandi diretti
Revive/CoveringFire (giro 7) e il **sistema di binding esteso** (rotella/pulsanti mouse assegnabili
dalle opzioni, giro 8). `CoveringFire` rifinito in **soppressione** (giro 9). **Restano rifiniture
Phase C**: posa prone (in attesa di pose/animazioni — tooling dell'utente), bilanciamento tempi/raggio.
L'economia tattica (Punti Comando) resta bloccata su N2 —
come il 5° criterio di accettazione ("la missione è più difficile senza ordini"), che richiede una
missione vera: **è l'argomento più forte per fare N2 adesso**.
→ **Playtest confermato dall'utente 2026-07-15** (tasto G, contesto dal mirino, pannello HUD:
"funziona perfettamente"). N1 è chiuso per quanto costruibile senza obiettivi.

È **l'unico pilastro del GDD senza una riga di codice**, ed è quello che decide se Galactic Front
è "uno sparatutto competente" o "il gioco del GDD": la vittoria deve nascere da decisioni
tattiche e gestione della squadra, non dalla mira. Oggi nasce solo dalla mira.
Perché **adesso** e non prima: tutte le fondamenta sono appena state completate —
AI con profili tattici (16), pathfinding+crowd (22: un ordine "vai lì" è finalmente eseguibile),
consumo dei metadata di mappa (18: "prendi copertura" ha dati veri sotto), telemetria
osservabile (21: gli ordini si verificano in `--sim` senza giocare a mano). Prima di ADR-017
questo sistema non era costruibile bene; ora sì.
Nota: include lo **stato "a terra" + rianimazione** — è ciò che dà peso alle perdite.
→ Lega con l'iterazione **"is it fun"** ancora aperta della Fase 1: è il candidato numero uno a
farla passare.

**N2. Framework obiettivi (doc 25, ADR-019) → Phase A + B FATTE (07-15 / 07-16).**
**Phase B (07-16) — collegamento, scelto leggendo il GDD 21.2 "evitare i sistemi isolati"**: il
framework era completo ma per il giocatore **non esisteva** (KI #37). Ora: esito missione → esito
partita (con precedenza al mode); **HUD OBIETTIVI**; la missione impone la sua mappa; rebind al
riavvio (KI #38). Verificato in partita vera: fallimento → SCONFITTA, successo → VITTORIA.
**Selezione dal PreMatch FATTA (07-16)**: riga Missione nel menu; la missione impone
mappa/modalita' e il menu le mostra. (La riga **Classe**, aggiunta lo stesso giorno, e' stata
**rimossa il 07-17**: contraddiceva GDD 11.3 — vedi N4.)
**CaptureZone/DefendZone FATTE (07-16)**: ADR-009 **avvolto** via mailbox `commandPostStates`
(zero duplicazione della logica di cattura); riferimento per label validato dal gate nella mappa
della missione. Missione esempio: `firebase_alpha`. Ora il framework sa esprimere la meccanica
principale del gioco.
**Restano**: `DestroyTarget`/`EscortEntity`/`SurviveWave`/`InteractHack` (nessuno urgente: nascono
quando serve una missione che li usa) e i **Punti Comando**.
→ **Punti Comando — serve una decisione di design prima del codice.** Il GDD 5.4 dà la direzione
("guadagnati completando obiettivi, NON con le kill; spesi per rinforzi/veicoli/supporto orbitale")
ma lascia aperti **sink e prezzi**, e il GDD 21.4 vieta di deciderli scrivendo codice.
**Chiarimento dell'utente (07-16): i ticket SONO già il sistema di rinforzi** (cap di AI in campo +
riserva che entra man mano) → "comprare rinforzi" coi Punti Comando sarebbe un **doppione**. Se il
sistema si farà, il sink dovrà essere altro (veicoli? supporto orbitale?). Implementare solo il
guadagno = numero sull'HUD che non si spende (sistema isolato, GDD 21.2).

**Statistiche di missione + debrief — FATTE 2026-07-16** (la domanda di design l'ha sciolta
l'utente: *"narrativo"* = il giudizio nasce dall'**insieme dei fattori**, non da un voto unico).
`World::missionStats` accumulato da chi conosce il fatto; debrief sulle schermate Win/Lose +
evento JSONL `match end`. **Nessun punteggio calcolato di proposito**: i pesi → esperienza sono
progressione (doc 27) e restano design.
**`consequence` degli obiettivi — FATTO 2026-07-16.** 4 tipi agganciati a sistemi reali
(`block_enemy_reinforcements`, `enemy_accuracy`, `ally_reinforcements`, `unlock_spawn`) via
`World::battleState`; valori segnaposto da bilanciare provando; gate ADR-018 esteso.
Verificato con **effetto reale** (2× "RINFORZI INTERROTTI", 0 rimpiazzi).
**`unlock_spawn` completato 2026-07-18**: era una conseguenza a metà (scriveva `allySpawnPost` che
nessuno leggeva). Ora i rinforzi alleati spawnano al post conquistato (`ConquestMode::spawnUnit`).
Tutte e 4 le conseguenze hanno un consumatore reale. Loop completo da smoke manuale (firebase_alpha).

**Economia dei post: da ticket-bleed a *respawn-slow* — FATTO 2026-07-18** (direttiva utente). Il
vecchio "chi ha più post drena i ticket avversari" è **rimosso** da Conquista: ora ogni post posseduto
**rallenta il respawn** della squadra nemica (`config::POST_RESPAWN_SLOW = 0.15` additivo per post, in
`ConquestMode::checkDeaths`). Il post è un vantaggio di **ritmo/posizione**, non un sink di ticket.
Assalto/Difesa (ObjectiveModes) NON toccate: mantengono `m_bleedTimer` come timer di vittoria.
Questo **scioglie il nodo "sink dei Punti Comando"** solo in parte: il vantaggio-post ora c'è ma è
strutturale (respawn + unlock_spawn), non una valuta spendibile — la valuta resta design aperto.

**Mappa top-down di selezione respawn — FATTO 2026-07-18** (doc 30, Phase 1, stile Battlefront II 2005).
`IGameMode::availableSpawns()` (default = spawn base; `ConquestMode` = base + post alleati via
`CommandPosts::ownedByTeam`); **mappa dall'alto** in `Ui2D` (pareti dai box `geometry`, marker dei
punti proiettati, marker "caduto") con selezione **col mouse SULLA mappa** (hover + click) + fallback
`A/D`/Invio; conferma esplicita per schierarsi — **non** auto-respawn con 2+ punti (KI #56); spawn
de-clippato con `nudgeOutOfColliders` (KI #57); HP dal setting partita (KI #55). Tutto 2D → ADR-003
intatto. Build-verified; rendering/click da **smoke manuale**.
→ **Fasi future (doc 30 Out of Scope)**: **mappa tattica generale** (tasto dedicato, mette in PAUSA —
sistema *distinto* ma con base condivisa, chiarito dall'utente 07-18), post nemici/neutrali sulla
mappa, **ordini dalla mappa** (muovi la squadra cliccando), texture terreno, zoom/pan. Da fare quando
l'utente lo chiede.

**Mouse in tutti i menu dell'engine — FATTO 2026-07-19** (prima solo nel menu principale). `handleMouse`
su PreMatch/Opzioni/Sandbox + bottoni cliccabili negli overlay Pausa/Fine partita (`HUD::overlayPick`).
Hover evidenzia; sulle righe a valore il click regola (metà sx = −, dx = +); esiti condivisi con la
tastiera via lambda. Build-verified; click da smoke manuale.
**Selezione oggetti dalle VIEWPORT dell'editor — FATTO 2026-07-19** (picking 3D via ray). In MapEditor
si clicca un oggetto nel viewport per selezionarlo (ray-OBB sui map box, `MapBoxDraw::pickId` opaco +
`FreeCameraViewport::popClickedMapBox`); lista ↔ viewport in sync, gizmo compreso; click sul gizmo non
riseleziona. Marker/bone (altri editor) già cliccabili da prima. Build-verified; GUI da smoke manuale.
→ Estensioni possibili: box-select multiplo, hover-highlight nel viewport, picking anche dove un editor
mostra sia box sia marker (oggi in MapEditor ci sono solo box → nessun conflitto).

## Editor: modulo "Missioni e obiettivi" FATTO 2026-07-16
Moduli → "Missioni e obiettivi": due tab (obiettivi / missioni), dropdown-only dal registry
(obiettivi composti da lista, mappa dal registry, **command post dalla mappa della missione**),
conseguenze editabili, `saveJsonRMW` (ADR-010), id mai scritto nel JSON (ADR-001), Rinomina con
sweep delle cross-ref (nuove categorie `Objective`/`Mission`). Tipi non ancora eseguiti dal
runtime: selezionabili ma **con avviso**.
→ **Smoke dovuto**: aprire GFEditor → Moduli → "Missioni e obiettivi", provare un salvataggio e
una rinomina. Il salvataggio è la classe di operazione che nel 2026-07-08 ha distrutto dati:
build-verified non basta.
**Modulo "Classi" FATTO 2026-07-16**: nome, ruolo, armi e abilita' da dropdown del registry;
stessa disciplina (RMW, id = filename, Rinomina con categoria `Class`, minimo valido per il gate).
Avvisi espliciti: `role` non consumato da nessuno (ADR-022) e abilita' senza effetto (KI #32).
**L'authoring dei contenuti e' ora completo**: nessun tipo di definizione richiede piu' di
scrivere JSON a mano.
→ **Smoke dovuto**: Moduli → Classi (salvataggio e rinomina).

## Debito editor — CHIUSO 2026-07-16 (storico)
Era: *"obiettivi, missioni, classi e conseguenze si autorano a mano nei JSON, nessun modulo
editor"*, aperto dalla direttiva utente del 07-16 (l'editor è lo strumento principale) e dal doc 25
("prima lo schema e il runtime, **poi l'authoring**"). Chiuso dai due moduli sopra.
**Regola che ne deriva, da rispettare d'ora in poi**: un tipo di definizione nuovo non è finito
finché non si autora dall'editor. Il test è *"l'utente può modificarlo senza di me?"*
(10_ProjectMemory).

**Altro su questo filone:**
1. **~~`DestroyTarget`~~ → FATTO 2026-07-18 (runtime + editor).** Bersaglio strategico = struttura
   statica distruttibile su mappa (`StrategicTargetDef`/`MapDef.strategicTargets[]`); distruggerla
   completa l'obiettivo e scatena la conseguenza. Esempio: torre comunicazioni su firebase → missione
   `firebase_sabotage` (conseguenza `enemy_accuracy`). Authoring nel MapEditor completo (KI #53).
   Fix del "cubo volante"/hitbox sfasata (KI #54). Da rifinire quando ci saranno mesh reali per le
   strutture (ora box di fallback); smoke manuale del loop distruzione→obiettivo→conseguenza.
2. **Pesi/esperienza** del giudizio → doc 27 (Fase 3), dopo aver provato.

**Intento di design confermato dall'utente (2026-07-18)**: in una missione **convivranno più
obiettivi**, e distruggere un bersaglio spesso NON completerà la missione — le torri di
comunicazione e simili saranno per lo più bersagli da **distruggere o difendere per ottenere
vantaggi in partita** (conseguenze), non condizioni di vittoria. Già supportato: una missione ha
primari + opzionali con regole di vittoria, e le conseguenze danno vantaggi. Quando si autora, un
DestroyTarget "per vantaggio" va messo come **opzionale** (o con sola conseguenza, senza renderlo
condizione di successo). L'esempio `firebase_sabotage` lo usa come primario solo perché è una demo
a obiettivo singolo.

**Storico — candidato ormai chiuso:**
L'utente lo indica come necessario: *"dopo ogni missione si ottengono punti grazie a un sistema di
giudizio basato su parametri e statistiche — obiettivi completati, alleati morti, kill, morti"*.
Il GDD lo specifica al **9.6**: *"la valutazione finale pesa obiettivi (primari/secondari/falliti),
prestazione tattica (gestione squadra, uso risorse, efficacia decisioni) e costi (perdite, tempo,
risorse). **Il risultato è narrativo, non un semplice voto**"* (vedi anche 5.2, "debrief: successo
militare + prestazione personale + costi; deve raccontare una storia").
**Già disponibile**: obiettivi con tier e stato (ADR-019), tempo di missione, morti/kill via
`killedThisTick` + telemetria, perdite alleate via `checkDeaths`.
**Serve una decisione di design PRIMA del codice** (GDD 21.4): quali parametri pesano e quanto, e
soprattutto cosa significa "narrativo invece che un voto". Raccogliere statistiche senza il
consumatore = ennesimo sistema isolato (GDD 21.2).

**Nota di metodo (07-16):** `--direct-prematch` **NON** avvia la partita in modo affidabile (a
volte sì, a volte no, stesso comando): non è un meccanismo di verifica. Per testare headless il
percorso PreMatch→partita serve una **sonda temporanea che chiami `startFromPreMatch()`** — la
stessa funzione del tasto ENTER, mai una copia della sua logica. `--stress`/`--sim` girano in
osservatore e **non finiscono mai**: inutili per testare gli esiti. Vedi 10_ProjectMemory.

**Storico Phase A (2026-07-15):**
Stato: `ObjectiveDef`/`MissionDef` nel registry, `ObjectiveSystem` dopo Ai/Crowd, 3 tipi
implementati, attivazione/dipendenze/tier, regole di missione dichiarate, gate che rifiuta con
causa, flag `--mission`. Inerte senza missione → mode intatti. Tutti e 5 i criteri di accettazione
del doc 25 soddisfatti (07_Changelog per i numeri). **Restano**: CaptureZone/DefendZone
(avvolgimento ADR-009, serve la mailbox degli stati dei post), gli altri 4 tipi, HUD obiettivi,
selezione missione, Punti Comando.

**N3. Gate di validazione contenuti (doc 24, ADR-018) — FATTO 2026-07-15.**
In force: `core/Result.hpp` + `game/data/ContentValidation`, tre consumatori dello **stesso**
codice (runtime blocca su Error, pannello editor *Moduli → Validazione contenuti*, `--validate`
con exit code ≠ 0 + JSONL). Verificato con guasti deliberati (6 errori/3 warning/exit 1) e sui
dati reali (0/0). Chiusa anche la duplicazione del gate missioni introdotta da N2:
`validateMission` è ora condivisa fra runtime ed editor.
Tutti i criteri del doc 24 soddisfatti, **campi fantasma incluso** (opzione (a): i loader
registrano le chiavi che non leggono → `unknownKeys()`, zero I/O nuovo). Ha trovato subito un
caso vero: `profile_id` residuo in `data/ai/B1 Heavy Droid.json`, rimosso.
Limite documentato: cattura le chiavi IGNORATE dal loader, non i campi letti-ma-non-consumati
(la lista storica di KI #25) — quelli sono codice, non dati, e restano annotati a mano.
→ **Smoke dovuto**: aprire GFEditor → Moduli → Validazione contenuti.

**N4. Class System → ADR-022 RISCRITTO e meta' NPC FATTA (2026-07-16).**
Modello reale (spiegazione utente + GDD 11.3, capitolo che non avevo letto): le classi sono **due
sistemi in una definizione**. (1) **NPC**: la classe da' abilita', **comportamento**, loadout e
aspetto → si instanzia. (2) **Giocatore**: **non ne sceglie una** — tutte esistono insieme e si
**livellano** giocando ("il gameplay decide quali classi crescono"), sbloccando perk. (3)
**Specializzazioni** (ARC, Commando): sbloccate da obiettivi, **non si livellano** → terzo asse.
**FATTO**: `ClassDef.aiProfileId` + `EnemyDef.classId` → l'unita' referenzia una classe (additivo);
gate + dropdown nell'editor. Verificato: la classe fornisce arma E profilo AI all'alleato.
**~~RESTA — decisione utente~~ → DECISO E FATTO 2026-07-17: riga "Classe" RIMOSSA dal PreMatch**
(scelta dell'utente). Il fatto decisivo: le righe *Arma primaria/secondaria* erano **gia'** nello
stesso menu e la riga "Classe" le **sovrascriveva in silenzio** → non era solo un nome sbagliato,
era la trappola "due posti, uno vince senza dirlo". Rimuoverla non toglie nessuna funzione.
Rimossi anche `setClassList`/`getSelectedClassId`/`ClassEntry`: la regola e' **strutturale**.
`--class` resta come **override di test**, dichiarato tale.
**~~Il doc 14 va riscritto~~ → FATTO 2026-07-17**: stato **MISTO** (meta' NPC = Current
Implementation, meta' giocatore = Planned). **Questo sblocca doc 27**, il cui criterio #1 e'
*"14 implementato prima di iniziare"*.
**RESTA — Fase 3 (doc 27), ORA SBLOCCATA**: XP/livelli/perk per classe; le fondamenta ci sono gia'
(`missionStats` conta kill/obiettivi, gli obiettivi hanno tier/type). Serve prima il sistema perk.
**RESTA**: `SpecializationDef` (terzo tipo) — non progettarlo ora (dipende dai perk).
**~~RESTA — contenuto (utente)~~ → FATTO/VERIFICATO 2026-07-19.** L'utente ha assegnato le classi:
`data/allies/Clone Trooper.json` ha ora `class: trooper` (+ `ai_profile: Clone Trooper`). Verificato
**sul vivo** (`--sim` + telemetria `unit class resolved`): l'alleato risolve `ai=Clone Trooper`,
`class=trooper`, `weapon=DC-15A` — **non** più `B1 Battle Droid`. I nemici risolvono `B1 Battle
Droid`/`B1 Heavy Droid`. Quindi il criterio "una squadra multi-classe si comporta diversamente"
(GDD 12.3) È soddisfatto: alleati e nemici usano profili AI distinti. La metà NPC del class system è
ora **viva sui dati reali**, non solo costruita.

**Dettaglio di quanto già in force (Phase A):**
`ClassDef` nel registry (id = filename stem) + `MatchSettings.classId` risolto in `startGame()`,
additivo (vuoto = loadout manuale invariato) + persistenza nei preset + flag `--class` + gate
ADR-018 sulle classi. Esempi: `trooper`, `marksman`.
**Il doc 14 partiva da due premesse false** (PlayerDef con `weaponIds`; `PlayerDef.classId`):
`PlayerDef` non e' letto da nessuno → **KI #35**. La classe e' andata su `MatchSettings`; doc corretto.
**Restano**: selettore classe nel PreMatch (oggi solo `--class`/preset) e modulo editor "Classi".
`abilityIds` e' trasportato ma senza effetto finche' esiste KI #32.
→ ~~Playtest dovuto~~ **FATTO dall'utente 2026-07-16: non funzionava.** Era un **bug reale**
(KI #36), non il design: all'ENTER il PreMatch azzerava `classId` e `characterId` sovrascrivendo
la struct intera — stessa classe di guasto della regola RMW (ADR-010), in memoria invece che su
file. Azzerava anche il personaggio, quindi **nemmeno KI #35 funzionava in partita** (verificato
solo in sandbox e generalizzato: errore di metodo). Corretto e verificato sul percorso reale:
marksman → DC-15X, trooper → DC-15A.

**KI #35 — `PlayerDef` morto → RISOLTO 2026-07-15** (decisione delegata: opzione (a), renderlo
vivo). Le stat del personaggio ora atterrano sul giocatore in `initWorld` (partita **e** sandbox);
default in codice = costanti storiche → variazione zero. Con un solo personaggio autorato viene
scelto da solo, senza UI. Rimossa la costante hardcoded `SPRINT_MULT` (viveva in
`PlayerController.cpp` contro CLAUDE.md); il dato è stato allineato al comportamento reale (1.65),
non viceversa. Gate ADR-018 esteso a personaggi e classi.

**Restano da fare (in ordine di valore):**
1. ~~Selettore PreMatch per classe~~ **FATTO 07-16** (riga "Classe" nel menu, insieme a
   "Missione"). Resta il **selettore personaggio**: serve solo quando esisterà un secondo
   personaggio autorato — con uno solo la scelta è automatica e il pannello dell'editor è già vivo.
2. **~~Modulo editor "Classi"~~ → GIÀ ESISTE** (`editor/.../ClassEditor`): questa voce era datata.
   Il modulo c'è (vedi anche KI #47, sistemato il 07-17). Eventuali rifiniture dropdown-only del
   ClassEditor restano possibili ma non urgenti.
3. **KI #32 — abilità/gadget lato giocatore**: `ClassDef.abilityIds` è trasportato correttamente
   ma **nessuno lo consuma**. Volutamente NON affrontato ora: l'utente ha chiesto di non
   complicare abilità/gadget ("ci lavoreremo meglio in futuro"), ed è un sistema, non una patch.

**N5. Progressione (27) e Persistenza (28) — Fase 3, dopo N4.** Non prima.

**~~R8 (nuovo, 2026-07-15)~~ → CHIUSO 2026-07-17** (`editor/util/DataPath`, KI #45).
Il rischio era scritto al condizionale (*"se una copia divergesse"*): **erano gia' divergenti** —
4 copie su 8 verificavano `data/weapons`, le altre 4 accettavano qualunque cartella di nome `data`
(EntityEditor, MapEditor, VehicleEditor, WeaponEditor). Ora una sola risoluzione, col controllo
forte. Lezione: un debito descritto come ipotetico va **misurato** prima di classificarlo tale.

**Debiti che restano validi in parallelo:** ~~KI #31 (AI attraversano i veicoli)~~ **RISOLTO 07-19**
(push-out OBB nel CrowdSystem); ~~KI #29 (veicoli bloccati alle casse)~~ **RISOLTO 07-19** (collisione
OBB-vs-OBB col param `queryYawRad`); KI #32 (abilità/gadget player-side); **R2** — Application.cpp era
2132 righe: **down-payment 07-19** (estratto `core/StateDump`), restano SandboxSession/VehicleDriver;
R7 (igiene `.bak`).
~~KI #7 (bonifica manuale near-duplicate)~~ → non è più un lavoro manuale: dal 2026-07-15 il gate
ADR-018 li rileva da solo, e sui dati attuali non ce ne sono.


## Done 2026-07-11 → 07-14 (ottimizzazione + telemetria + navigazione)
- **Ottimizzazione loop/AI (ADR-015, doc 20):** frame pacing doppia precisione + cap sicurezza;
  Tracy opt-in; ricerca target SoA; time-slicing sensing; cap LOS ai K vicini. Stress `--stress N`.
  Risultato: ~40 AI fluidi in sim (prima ~30).
- **Telemetria LLM-observable (ADR-016, doc 21):** sink JSONL `session_latest.jsonl` + hook
  GameMode/CommandPost/AI + dump stato completo su F12/fine-partita/crash.
- **Navigazione Recast/Detour (ADR-017, doc 22):** navmesh da MapDef.geometry, DetourCrowd muove
  le AI (pathfinding → AI-stuck su ostacoli RISOLTO; crowd-avoidance), aree danger/cover.
- **Fix:** spawn giocatore posato a terra (KI #28), glitch mouse primo-frame, piedi sottoterra AI
  (regressione crowd). Aperti nuovi: KI #31 (AI attraversano veicoli), KI #32 (abilità player).
- **Nuovi candidati a valore:** R2 (Application.cpp ~1250 righe → estrarre VehicleDriver/Sandbox),
  KI #31 (AI-veicolo), sistema abilità/gadget player-side (KI #32).

## Robustezza (audit codice completo 2026-07-10 — ordine = gravità)

A1. ~~FATTO 2026-07-10~~ **Preset partita: sopravvivenza alle build + formato robusto** (KI #19+#20).
    Spostati in `<exe>/user_presets/match` (fuori dalla `data/` che CMake azzera),
    serializzati con nlohmann (escaping ok), persistono `map_id` (non più l'indice
    fragile) e l'intero loadout. Migrazione legacy best-effort. Smoke: salvare un
    preset → rebuild → deve sopravvivere; caricare un preset → mappa e loadout giusti.
A2. ~~FATTO 2026-07-10~~ **Fallback `id`/`profile_id` in-file rimosso da TUTTI i loader**
    (KI #21): id = solo filename stem (ADR-001). Verificato zero mismatch nei dati
    prima del cambio; smoke `--sim` con registry completo.
A3. ~~FATTO 2026-07-10~~ **Heat reset allo switch arma** (KI #22): stato riscritto in
    `weapons[activeWeapon]` prima del cambio. Consolidare `weapon`/`weapons[2]` → A10.
A4. ~~FATTO 2026-07-10~~ **Spawn spec ConquestMode unificato**: `UnitTemplate` eliminato,
    `RespawnEntry` (con default sensati) è l'unico tipo per spawn/tracking/respawn —
    copia integrale, zero liste di campi da allineare. Le lambda `mkUnit*` restano
    (candidate a prendere direttamente una RespawnEntry — pulizia futura, bassa urgenza).
A5. ~~FATTO 2026-07-10~~ **Collider ruotati** (KI #23): SAT 2D esatto nel movimento +
    LOS in spazio locale del box; coerente col test OBB dei proiettili. Smoke manuale
    alla prima mappa con muri diagonali.
A6. ~~FATTO 2026-07-10~~ **Dipendenze CMake pinnate** (KI #27): stb 31c1ad3, imgui 6029ee3
    (i commit già in uso in _deps).
A7. ~~FATTO 2026-07-10 (completo)~~ **Dedup loader e resolve**: `parseUnitDef` condiviso
    tra loadEnemies/loadAllies; `resolveUnitArchetype` unico per nemici e alleati in
    ConquestMode (gli alleati ora prendono le stats proiettile dall'arma reale, non
    più 8/20/5 hardcoded).
A8. ~~FATTO 2026-07-10 (in parte)~~ **Igiene dati/fallback** (KI #24+#26): id hardcoded
    rimossi da SandboxMode; preset armi morti rimossi da Weapon.hpp (resta solo il
    fallback di ultima istanza documentato); `data/definitions/*` e cartelle vuote
    eliminati. RESTA: geometria fallback firebase (decisione ADR-004) e
    `data/versions.json` (verificare se serve, poi rimuovere).
A9. ~~FATTO 2026-07-10~~ **Campi fantasma marcati "(non attivo)"** negli editor
    (KI #25): min_range, mesh proiettile, fov_deg, hearing_range, reposition_chance,
    damage_scale (+ nota su move_speed vinto dal profilo AI). Quando un campo viene
    consumato dal runtime, togliere il suffisso nello stesso change set.
A10. **Vincoli architetturali da tracciare (non da fixare ora):** team 1/2 hardcoded
    trasversale (nessun supporto 3+ fazioni/FFA); timestep misto (world a fixedDt,
    player/sparo a dt variabile — rilevante per determinismo/replay/split-screen);
    ~~nessuna broad-phase spaziale (collision/LOS O(N²) — muro alla scala fase 2/3)~~
    → MITIGATO 2026-07-14: sensing AI ora SoA + time-slicing + cap LOS ai K vicini (doc 20),
    e il movimento AI usa il navmesh Detour invece di `hasCollision` (doc 22). Resta O(N²) solo
    il broad-phase collisione di player/proiettili (pochi collider, non un muro);
    `MatchSettings.hpp` include ancora SDL in header condiviso.
    ~~Doppia rappresentazione weapon/weapons[2]~~ → RISOLTA 2026-07-10 (28):
    accessor `weapon()` su `weapons[activeWeapon]`, nessuna copia da sincronizzare.

## Rifinitura (diagnosi 2026-07-10 — candidati per la fase di polish)

R1. ~~FATTO 2026-07-10 (18)~~ **Spread/gittata delle armi MAI applicati al giocatore.** `WeaponDef` ha 5 campi
    spread (base/ADS/move/sprint/jump) + effective_range/min_range, autorabili nel
    BalanceEditor con tanto di anteprima — ma PlayerController non li consuma (il
    player spara sempre perfetto, a gittata infinita fino a bullet_lifetime) e le AI
    usano solo `accuracy` del profilo. O si consumano (feel!) o si dichiara il gap.
R2. ~~PARZIALE 2026-07-10 (18): guida estratta in VehicleDrive.hpp (1120→1057);
    prossimi candidati: sim/sandbox session, viewmodel~~ **Application.cpp a 1120 righe** e in crescita: main loop + menu routing + guida
    veicoli + viewmodel + aim check + sim sandbox. Refactor candidato: estrarre
    VehicleDriver e SandboxSession in file propri (nessun cambio di comportamento).
R3. ~~FATTO 2026-07-10 (21): MatchSettings.mapId + selettore nel PreMatch + seconda
    mappa "Outpost" + flag --map~~ **Mappa "firebase" hardcoded nei mode.**
R4. ~~PARZIALE 2026-07-10 (19)~~ **Editor: parser divergenti degli stessi JSON.**
    VehicleEditor ora usa il DefinitionRegistry (duplicato eliminato). Analisi:
    Entity/Map editor restano LEGITTIMAMENTE su parser propri — leggono campi
    editor-only (label/type dei box mappa, buffer ImGui, zone in editing) che il
    runtime non deve caricare. Se lo schema runtime cambia, aggiornare comunque
    entrambi (regola in CLAUDE.md §1.4). Chiuso salvo nuovi duplicati.
R5. ~~FATTO 2026-07-10 (20): pilota saltato come bersaglio, il mezzo assorbe;
    danno a sagoma OBB piena~~ **Pilota colpibile "attraverso" il veicolo**: l'entità player segue la camera
    dentro il mezzo e il test entità avviene PRIMA del blocco collider → i colpi al
    centro del veicolo uccidono il pilota invece di danneggiare il mezzo. Da
    ridefinire con le hitbox veicolo (19 Fase B: il pilota dovrebbe prendere danno
    via veicolo, non direttamente).
R6. ~~FATTO 2026-07-10 (18): VehicleSpawn.hpp condiviso~~ **Spawn veicoli duplicato** in Conquest/Sandbox (2 copie identiche ~25 righe):
    estrarre helper condiviso (es. in MapQuery o un VehicleSpawn.hpp).
R7. **Igiene data/**: `.bak` accumulati (uno per arma) — sono il paracadute ADR-010,
    ma prima o poi serve una pulizia/rotazione; near-duplicate armi (KI #7) ancora
    da bonificare A MANO con il rename tool.

## Done 2026-07-04
- ~~Unify hitbox authoring~~ → ADR-006: profile = single source of truth; EntityEditor
  reads/writes the profile; inline zones deprecated + B1 data migrated.
- ~~ConquestMode dead fallback ids~~ → ADR-007: registry-derived fallback, empty-safe.
- ~~EntityEditor gizmo under scale/rotation~~ → toWorld()/deltaToLocal() at all call sites.
- ~~.gitignore hygiene~~ → .gitignore rewritten (was corrupted), build/ + imgui.ini +
  presets.cfg untracked.

## Done 2026-07-04 (later batches)
- ~~GameMode interface + factory~~ → ADR-008 (`IGameMode` + `createGameMode`); smoke test
  runtime `--sandbox` passato. KnownIssues #8 chiuso.
- ~~Editor professionalization~~ → gizmo 3 modalità, slider ovunque, camera Unreal-style,
  WeaponEditor attach point nel viewport.

## Done 2026-07-04 (5)
- ~~Objective / Command Post riusabile~~ → ADR-009: `CommandPosts` + MapDef + Map Editor
  authoring + ticket bleed in Conquista + test in sandbox. KnownIssues #9 chiuso.

## Done 2026-07-09 — ADR-010 implementato (messa in regola)
- ~~[P0] Rename tooling~~ → `DefinitionRename` (mappa cross-ref esplicita) + UI "Rinomina"
  in Weapon/Entity/Hitbox/Map editor. **Smoke GUI manuale pendente** (KnownIssues #7).
- ~~[P0] Dropdown-only audit~~ → PASSATO: gli unici InputText residui sono creazione nuovi
  id, nomi/etichette/descrizioni e path mesh — nessuna assegnazione di id via testo libero.
- ~~#8 Save-safety helper~~ → `saveJsonRMW` (util/JsonSave.hpp, con .bak) + TUTTI i save
  path migrati. KnownIssues #15 chiuso. In più: `id`/`profile_id` deprecati rimossi dai
  JSON a ogni save (ADR-001).

## P0 — Blocking (superato, riferimento storico)

1. **[P0] Rename tooling for all definition types (was #16, promoted from Low/future).**
   User-confirmed symptom (2026-07-09): renaming weapon names/ids without the tool produced
   duplicate-looking weapon entries in the loadout menu. Root cause: id=filename (ADR-001) +
   no rename command → manual "rename by creating a new file" leaves the old file as an
   orphaned near-duplicate. Scope:
   - Add a "Rinomina" action in EntityEditor, WeaponEditor, HitboxEditor, MapEditor (AI
     profile editor when it exists).
   - Implementation must: rename the physical file (`std::filesystem::rename`), update the
     in-memory id, sweep and rewrite every cross-reference in `data/` that pointed to the old
     id (`weaponIds[]`, `aiProfileId`, `hitboxProfileId`, `enemyTypes[]`, `allyTypes[]`,
     `MapDef` references), then reload the registry.
   - Must use the RMW save discipline (04_CodingStandards) for every file it touches.
   - See 13_ADR, ADR-010 (Proposed) for the full decision record.
   - Blocks: clean data authoring for the rest of Phase 1 content (more weapons/enemies).

2. **[P0] Dropdown-only enforcement audit.** Full audit of every editor module for any
   remaining free-text id input (`ImGui::InputText` on an id/reference field). Convert to
   registry-backed combo per 04_CodingStandards. This is a prerequisite for Todo #1 above:
   the rename tool is only fully effective if there is no remaining path to type a stale id
   by hand.

## High priority

0. ~~Sandbox: selettore armi~~ → FATTO 2026-07-09 (10): tasti 1-9 equipaggiano qualunque
   arma del registry (ordinate per nome, incluse separatiste), toast col nome + hint
   all'avvio, log in telemetria.

3. ~~AI: abilità + ruoli tattici~~ → FATTO 2026-07-10 (core, doc 16_AiBehavior);
   prima abilità ATTIVA (Combat Roll) 2026-07-10 (23); ~~command~~ (comandante) → FATTO v0
   2026-07-20 (ADR-024 riscritto, Droide Tattico). Restano jetpack/missile sullo stesso binario:
   aggression→distanza d'ingaggio, retreat_hp_threshold→disimpegno, peek/hide da
   cover_preference, flank_chance in Hunt; ability "shield" runtime (ShieldComponent,
   assorbimento + regen in CombatSystem). Resta (16_AiBehavior Out of Scope): abilità
   attive (roll/jetpack/...), vera copertura geometrica (dipende da 15_MapMetadata).
4. ~~Assault/Defense mode~~ → FATTO 2026-07-09 (11), ADR-014. ~~modeIndex nei preset~~
   → FATTO 2026-07-10 (13).
5. ~~Runtime weapon-in-hand~~ → FATTO 2026-07-04 (8): WeaponAttach.hpp + attachMesh nel
   renderer. Resta: assegnare mesh alle armi Republic (DC-15A ecc.) nel Weapon Editor.
6. ~~HUD: stato command post~~ → FATTO 2026-07-09 (11): barra in alto con proprietario,
   lettera e progresso di cattura per ogni post.
7. ~~Dato mancante: profilo AI "grunt"~~ → FATTO 2026-07-09 (2): creato `data/ai/grunt.json`
   (infantry), log pulito, 3 profili caricati.
8. **Save-safety helper centralizzato.** Introdurre un helper condiviso
   (`saveJsonRMW(path, patchFn)`) usato da tutti i moduli editor per ogni salvataggio JSON,
   così la regola RMW (04_CodingStandards) diventa un vincolo strutturale e non solo una
   disciplina documentata. Motivato dall'incidente reale del 2026-07-08 (clobber di
   firebase.json). Vedi ADR-010 candidate scope in 13_ADR.

## Medium

9. **Clone Trooper scale.** Normalize oversized FBX-cm GLB (author `mesh_scale` per entity or
   pre-scale asset). (KnownIssues #5)
10. ~~Weapon attach-point grip/right_hand~~ → codice GIÀ implementato (WeaponAttach usa
    gripAttach, right_hand prioritario); resta SOLO autorare i punti nei GLB delle armi
    dal Weapon Editor (attività dati). Chiuso lato codice 2026-07-10 (14).
11. ~~Runtime weapon rendering~~ → FATTO: unità AI (weapon_display, 2026-07-04) e ora
    anche viewmodel prima persona del giocatore (2026-07-10 (14)).
12. **Commit the hygiene change** (untracked build/ + new .gitignore) — staged, needs a
    commit.
13. **Introduce explicit Objective and Command Post concepts.** Required by Vision Phase 1/2
    to express Conquest/Assault/Defense as data configurations instead of bespoke mode logic.
    (KnownIssues #9)
14. **Introduce explicit Class concept** (weapon + equipment + role composition) separate
    from a single weapon definition, ahead of Phase 3 progression work. (KnownIssues #10)
15. ~~Extend MapDef with AI-relevant metadata~~ → FATTO 2026-07-10 (6): schema + loader +
    authoring MapEditor (15_MapMetadata ora Implementato). Resta il CONSUMER runtime
    (AI tattica, parte del futuro lavoro fase 2 — da documentare a parte).
    (KnownIssues #11 chiuso lato dati)
16. ~~Verify split-screen/multi-viewport feasibility~~ → FATTO 2026-07-09 (12): spike
    ADR-011 eseguito, esito (a) — fattibile con modifiche minori. Toggle F9 in partita.
    KnownIssues #12 chiuso; il soft-gate ADR-011 decade.

## Low / future

17. AI Editor module (dropdown-driven, rename-capable — inherits Todo #1 tooling once built).
18. Asset Manager module.
19. UI/Interface Editor (centralize menu text/layout/palette/fonts — currently scattered).
20. Data hygiene pass to remove existing near-duplicate weapon JSONs (manual cleanup once
    rename tooling — Todo #1 — exists, to avoid re-creating the same problem while cleaning).
21. Define AI hierarchy extension points (squad/strategic tiers) even before implementing
    them, so individual-agent AI doesn't need a rewrite when Phase 2/5 AI is added.
22. Vehicle system as ECS-composable entities (multi-seat), not bespoke controllers.
    → **Fase A FATTA 2026-07-10 (8)** (doc 19_Vehicles): VehicleDef + spawn da MapDef +
    guida player come entità ECS. Resta Fase B: armi di bordo, multi-posto, AI alla
    guida, authoring vehicle_spawns nel MapEditor.
23. **Sistema shape/collision oltre i box** (richiesta utente 2026-07-10): geometria
    mappa, hitbox e collisioni sono limitate a parallelepipedi. Servirà un sistema più
    ricco (forme composte/mesh semplificate) per ambiente ed entità — grande, da
    progettare con un ADR quando diventa bloccante.
24. **Meccaniche FPS alle coperture** (richiesta utente 2026-07-10): crouch dietro
    copertura, mira da copertura, peek-over/around con pose — dipende da un sistema di
    pose/animazioni; il dato `coverPoints[].height` è già pronto a guidarle.