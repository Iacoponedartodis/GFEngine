# 20 — Optimization (profiling, frame pacing, AI scaling)

Riferimento del lavoro di ottimizzazione (ADR-015 + Fasi 3-4). Obiettivo: reggere 40+ AI in
simulazioni dense (mischie ai command post) mantenendo il frame fluido. Stato: **in force**.

## Overview
Tre livelli, tutti verificati con misure reali:
1. **Profiling** — Tracy opt-in per misurare *prima* di ottimizzare.
2. **Frame pacing** — timing a doppia precisione + cap di sicurezza.
3. **Scaling AI** — la parte pesante: la sensing dell'AI era O(N²) e dominava il tick.

Misura di riferimento (build DEBUG, tick = Movement+Combat+Ai): a **100 AI (50v50)** il costo
del tick è passato da **~203 ms a ~36 ms** (≈5.6×). A ~30-40 AI (scala reale del gioco) il gioco
è fluido (prima il limite era ~30-32). In Release i valori assoluti sono ~5-20× più bassi.

## 1. Profiling — Tracy (ADR-015)
- **Opzione CMake `USE_TRACY_PROFILER`** (default **OFF**): OFF → `TracyClient` è uno stub a
  costo nullo e le macro `ZoneScoped`/`FrameMark` sono no-op (build normali IDENTICHE). Linkato
  **solo a GFEngine** (ADR-002). Tag pinnato `v0.11.1`.
- **Strumentazione:** `FrameMark` a fine loop (dopo lo swap); `ZoneScoped` in `World::tick`,
  `AiSystem::update`, `CombatSystem::update`; `ZoneScopedN("render.drawScene")` nel rendering.
  Le zone si annidano da sole (`World::tick` ⊃ Combat + Ai).
- **Come profilare:** configura una build RelWithDebInfo con `-DUSE_TRACY_PROFILER=ON`, avvia
  `GFEngine.exe --stress 50` e collega il Tracy profiler GUI a `127.0.0.1`. NB: con il
  generatore multi-config di VS `CMAKE_BUILD_TYPE` è vuoto → si usa l'opzione esplicita, non un
  default per-configurazione.

## 2. Frame pacing (ADR-015, Fase 2)
Il main loop (`Application::run`) usa un **timestep fisso con accumulatore in DOPPIA precisione**:
- Il `dt` viene da `SDL_GetPerformanceCounter`/`Frequency` (contatore hardware sub-ms), non da
  `std::chrono`/`SDL_GetTicks`. `SIMULATION_STEP = 1.0/60.0` (double). La simulazione riceve
  comunque `fixedDt` (float, 1/60) → gameplay identico a 60 Hz, senza il drift dell'accumulatore
  float su sessioni lunghe. Clamp anti spiral-of-death: 0.25 s.
- **VSync ON di default** (pacing GPU). Se qualcuno la spegne (`WindowConfig.vsync=false`), un
  **frame-cap di sicurezza** (`config::MAX_UNCAPPED_FPS=300`, sleep IBRIDO: `SDL_Delay`
  grossolano + busy-wait finale sub-ms) evita che il loop giri a migliaia di FPS. Con VSync ON è
  inerte (un solo `SDL_GL_GetSwapInterval()==0` per frame).
- **Nota (debito, A10):** timestep MISTO — il `world.tick` gira a fixedDt, ma la camera/fisica
  del player e lo sparo girano a `dt` variabile. Rilevante per determinismo/replay/split-screen.

## 3. Scaling AI (Fasi 3-4) — dove stava il costo
La sensing dell'Aia (chi ingaggiare) era il collo di bottiglia: due passate O(AI × bersagli)
con `hasLineOfSight` (che itera tutti i collider) nel ciclo interno → **O(N²)**, ~99% del tick.
Il movimento/sparo è O(N), trascurabile. Tre tecniche additive:

### 3a. Layout SoA (Fase 3)
La ricerca target faceva `getTransform(tgt)` (lookup `unordered_map` = hash + pointer-chase su
heap sparso) per OGNI coppia. Ora id+posizione dei bersagli sono raccolti UNA volta in array
contigui paralleli (`team{1,2}Tgts` / `team{1,2}Pos`); i loop leggono `pos[i]` contiguo. Il
componente pesante si recupera solo per il bersaglio selezionato.

### 3b. Time-slicing della sensing (Fase 4a)
Ogni AI esegue la sensing pesante (ricerca target + LOS) **1 tick su `config::AI_SENSE_INTERVAL`
(=6, ~10 Hz)**, scaglionata per entità: `(tickCount + entityId) % AI_SENSE_INTERVAL == 0`. Fra
un sensing e l'altro riusa il **bersaglio cachato** (`AiComponent::targetEntity`) per mirare e
muoversi; movimento e sparo restano ogni tick. La morte del target è rilevata ogni frame
(`getTransform`); il LOS è **ri-verificato al momento dello sparo** (solo a cooldown scaduto,
raro → economico) così non spara attraverso i muri col target cachato. **Effetto: ~3.7×.**

### 3c. Cap LOS ai K vicini (Fase 4b)
La griglia spaziale del piano NON serviva qui: `aggroRange` (~20 m) ≈ dimensione mappa (50×40),
un query 3×3 coprirebbe tutto. Il costo è LOS-bound nella mischia. Quindi: si raccolgono i
**K = `config::AI_MAX_LOS_CHECKS` (8) bersagli PIÙ VICINI** (solo distanze) e si verifica il LOS
solo su quei K dal più vicino — il primo visibile è il nearest visibile. La LOS costosa passa da
**O(N²) a O(N·K)**. Comportamento: l'AI ingaggia un nemico vicino visibile invece dello stretto
più vicino globale (differenza impercettibile; shared-awareness + ri-sensing compensano).
**Effetto: +1.5×** sul residuo.

## Stress test tooling
- **`--stress N`** (CLI): forza una simulazione AI-vs-AI con N (clampato a `config::MAX_AI_PER_TEAM
  =50`) AI per team, headless → profiling riproducibile.
- I cap AI (SandboxMenu, PreMatch, ConquestMode) usano tutti `MAX_AI_PER_TEAM`.
- Lo spawn dei bot è a **griglia** (`perRow` + `findFreeSpot`), non a fila singola, così restano
  nei limiti mappa anche a 50/team.

## Costanti (core/GameConfig.hpp)
| Costante | Valore | Ruolo |
|---|---|---|
| `MAX_UNCAPPED_FPS` | 300 | cap di sicurezza se VSync off |
| `AI_SENSE_INTERVAL` | 6 | ogni quanti tick l'AI ri-sensa (più alto = economico ma latente) |
| `AI_MAX_LOS_CHECKS` | 8 | quanti bersagli vicini verificano il LOS per sensing |
| `MAX_AI_PER_TEAM` | 50 | cap AI per team (sim e partita) |

## Gotchas / dove intervenire
- La stuck-telemetry (doc 21) NON logga in stato Alert (strafe sul posto in mischia = falso
  positivo). Resta valida per la traversata.
- Il residuo di costo a scala è ora il lavoro per-frame (movimento/collisione), non la sensing.
- Ulteriore scaling verrebbe da una struttura spaziale per i COLLIDER (abbassare il costo di ogni
  singola `hasLineOfSight`), non da una griglia di entità — non necessario alla scala attuale.
