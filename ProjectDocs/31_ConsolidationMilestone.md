# 31 — Milestone di consolidamento: Vertical Slice v1

**Status: Milestone attiva — deciso 2026-07-19.**
Documento di indirizzo: definisce **il punto da raggiungere** prima di fermarsi ad ampliare in
verticale e passare a lavorare **orizzontalmente** (collegare, migliorare, arricchire, rifinire) per
alzare la barra di qualità di tutto ciò che già esiste. Nasce da uno studio completo di 00_Vision,
29_GDD, 23_GameDesignBridge, 05_CurrentState, 06_Todo e del codice live (2026-07-19).

Autorità: per l'**intento di design** vince 29_GDD; per lo **stato reale** vincono 05_CurrentState +
codice (23_GameDesignBridge, regola di precedenza). Questo doc non li scavalca: li ORDINA verso un
obiettivo.

## Perché ora
Il GDD ha un ordine di costruzione esplicito: verticale minimo (combat) → **livello tattico** (squadra
+ AI + obiettivi) → carriera → meta-gioco (23_GameDesignBridge, "Ordine di costruzione"). Il progetto
ha completato i primi due strati:
- **Fase 1 (core playable)** — completa e rifinita.
- **Livello tattico (25 obiettivi + 26 squadra)** — costruito e verificato (ordini contestuali, ruota,
  down/revive, command post, economia respawn, class system NPC vivo sui dati reali — verificato via
  `--sim` il 07-19).

Siamo sul **confine Fase 1 → Fase 2**. Ma **i sistemi esistono, l'esperienza no ancora**: il GDD chiede
di "vincere decidendo, non mirando" (Pilastro 1) e oggi nessuna partita concreta lo *dimostra* — manca
la varietà del bestiario (tutti B1), manca una missione rappresentativa, l'iterazione "is it fun" della
Fase 1 (05_CurrentState) è aperta. Aggiungere progressione/meta su un tattico non ancora *provato*
comporrebbe la ruvidezza. Quindi: si raggiunge un punto, poi si consolida.

## Il punto da raggiungere — Vertical Slice v1
> **Una missione completa, giocabile dall'inizio alla fine, che DIMOSTRA il core loop del GDD (cap.
> 5.1) end-to-end.**

Criteri di accettazione (quando tutti veri, il punto è raggiunto):
1. **La squadra conta.** Si combatte con una squadra comandabile e la missione è misurabilmente più
   difficile senza usare gli ordini (5° criterio del doc 26, oggi l'unico non verificato perché
   richiede una missione vera).
2. **Il bestiario impone scelte.** I nemici arrivano in **≥3 sapori tattici distinti** che richiedono
   risposte diverse: carne da cannone (B1), pesante da fuoco concentrato (B2-like), **HVT-comandante**
   (Droide Tattico serie T). Non tre sacchi di HP diversi (GDD App. B, Bridge §5).
3. **Gli obiettivi valgono più delle kill.** Almeno un primario + un obiettivo strategico/opzionale;
   l'esito dipende dagli obiettivi, non dal conteggio nemici (Pilastro 3).
4. **Il fronte si muove.** I command post creano pressione (economia respawn già fatta).
5. **Racconto della missione.** Un **briefing** inquadra "qual è il problema" e un **debrief** racconta
   com'è andata (5.1; il debrief esiste, il briefing no).
6. **Feel:** una partita *si sente* come "essere un clone che prende decisioni tattiche".

Questo è il metro del GDD stesso ("deve essere già divertente qui", Fase 1) applicato con rigore, ed è
il confine giusto per consolidare.

## Cosa manca per raggiungerlo (poco, e delimitato)
Il grosso è **collegare/arricchire ciò che c'è**, non nuovi sistemi. I veri buchi *verticali*:
1. ~~**Droide Tattico (serie T)**~~ — ✅ **FATTO v0 (2026-07-20, ADR-024 riscritto, doc 32).** L'UNICO
   nemico che il GDD segnalava come codice nuovo (Bridge §5). Ridefinito su chiarimento utente: **non**
   un'aura/buff, ma il **comandante** dei droidi — la **controparte** del comando del giocatore.
   Ability `command` → `CommanderComponent`; `AiSystem` pubblica un **focus** strategico
   (`World::enemyCommand`, il post non-separatista più vicino) e i droidi vi **convergono**; ucciderlo
   spegne la direttiva (feed + ritorno alla pattuglia) → dà senso allo Sniper e all'assalto all'HVT.
   **Uno per mappa** (campo `MapDef.commander`, non nel roster), spawnato **stationary** nelle retrovie
   (sta fermo, si difende soltanto). **v1 base**: un solo tipo di direttiva; ordini più ricchi, UI di
   piazzamento, gerarchia gradi/ufficiali ed entità-a-sé futuri ([[command-rank-system]]).
2. **Briefing minimo** — schermata/pannello che inquadra il problema prima della missione (oggi c'è il
   pannello obiettivi, non un briefing).
3. **La missione autorata** — usa i sistemi esistenti + il bestiario vario. Contenuto dell'utente;
   l'engine può scaffoldare un esempio.

Tutto il resto degli archetipi (B2 pesante, BX fiancheggiatore) è **dati** autorabili coi campi che
`AiProfileDef` ha già (aggression/retreat/cover_preference/flank) — nessun codice nuovo.

## Poi: consolidamento orizzontale (assi, in ordine di valore)
Raggiunto il punto, si alza la barra di TUTTO ciò che la missione tocca. Assi:
- **A — UX di authoring nell'editor.** Il moltiplicatore di tutto: creare nemici/missioni/mappe deve
  essere veloce e a prova di errore. È ciò che rende "facili ed efficaci le aggiunte seguenti".
- **B — Feel e feedback del combattimento.** Bilanciamento matrice armi (App. A: ruoli con trade-off
  veri), feedback colpo/uccisione, leggibilità danno/salute, audio. (Animazioni **bloccate** finché
  l'utente non le sblocca — [[animations-blocked]].)
- **C — Espressione dell'AI.** Autorare gli archetipi perché *esprimano* i ruoli tattici (dati), tarare.
- **D — Coerenza HUD/UX + qualità del debrief** (racconto, non voto — GDD 9.6).
- **E — Salute del codice.** R2: ridurre Application.cpp (down-payment fatto: `core/StateDump`),
  estrarre SandboxSession/VehicleDriver → le feature future entrano pulite.
- **F — Validazione/telemetria/testing** estesi al nuovo contenuto.

## Out of scope (esplicitamente rimandato)
- **Progressione/carriera (27)**, **persistenza (28)**, **Galactic Conquest/Chronicles** (Fase 3/4/5):
  NON prima del consolidamento. Aggiungerli su un tattico non consolidato è l'errore che questo doc
  evita.
- **Economia tattica spendibile (Punti Comando)**: il GDD la vuole (5.4) ma oggi la pressione-fronte è
  data dall'economia respawn; la valuta spendibile (rinforzi/veicoli/supporto) resta design aperto,
  non necessaria per la Vertical Slice v1.
- **Mappa tattica generale (30 Phase 2)**: quando l'utente la chiede.
