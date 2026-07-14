# 22 — Navigation (Recast / Detour / DetourCrowd)

Navigazione AI: pathfinding reale + crowd-avoidance (ADR-017). Prima l'AI non aveva alcun
pathfinding (`aiMove` la muoveva lungo il vettore verso il target con collision-sliding) → si
bloccava su qualsiasi ostacolo fra sé e il goal, e non c'era avoidance fra decine di bot.
Stato: **in force (Phase A+B+C)**. Solo GFEngine (ADR-002). Libreria: `recastnavigation` v1.6.0
(FetchContent, tag pinnato), moduli `Recast` + `Detour` + `DetourCrowd`.

## File e componenti
- `include/mini/game/nav/NavManager.hpp` + `src/game/nav/NavManager.cpp` — possiede il
  `dtNavMesh` + `dtNavMeshQuery` + `dtCrowd`. Header leggero (tipi Detour forward-declarati).
- `src/ecs/systems/CrowdSystem.cpp` — sistema ECS (dopo AiSystem): registra/reap/ticka/write-back.
- `World::nav` (`NavManager*`, puntatore opaco settato da Application) — come `World::activeMap`.
- `AiComponent::crowdAgentIdx` (int, -1 = non registrato).

## Data flow (per step fisso)
```
mode->update + world.tick:
  MovementSystem → CombatSystem
  AiSystem::update      # SENSING (chi ingaggiare, doc 20) + state machine (dove andare) →
                        #   traversata: nav.requestMoveTarget(idx, dest)   [pathfinding]
                        #   Alert/roll: nav.requestMoveVelocity(idx, vel)  [velocità tattica]
  CrowdSystem::update   # 1) reap agenti morti  2) registra nuovi  3) crowd.update(dt)
                        # 4) write-back: transform = agentPos + (0,AI_HALF_Y,0)
```
Il **sensing ottimizzato** (doc 20) resta ortogonale: decide CHI; il crowd decide COME muoversi.
Player, proiettili e veicoli NON sono agenti crowd — usano il sistema `physics/Collision`.

## Phase A — NavMesh da MapDef.geometry
`NavManager::build(const MapDef&)` al load mappa (`Application::initWorld`), ricostruito a ogni
restart/cambio mappa (contatore `generation()` per il reset del CrowdSystem):
1. **Geometria → triangle soup:** ogni `MapGeometryBox` con `collider=true` → 12 triangoli
   (winding esterno, con rotazione `ry`).
2. **Pipeline Recast solo-mesh:** `rcCreateHeightfield` → `rcRasterizeTriangles` → filtri →
   `rcBuildCompactHeightfield` → `rcErodeWalkableArea` → (marca aree, Phase C) →
   `rcBuildRegions` → `rcBuildContours` → `rcBuildPolyMesh` → `rcBuildPolyMeshDetail` →
   `dtCreateNavMeshData` → `dtNavMesh::init` (single tile) → `dtNavMeshQuery::init`.
3. **`findPath(start,end,out)`** (Detour): usato per validazione e query future.
- **Parametri chiave** (tarati per box ~50×40 m): `cellSize 0.30`, `cellHeight 0.10`,
  `agentHeight 1.8`, `agentRadius = AI_HALF_X (0.4)`, **`agentClimb = STEP_HEIGHT (0.55)`**
  (scavalca scalini bassi), `agentSlope 45°`. Muri/coperture alte → non walkable → Detour ci
  path INTORNO: **è questo che risolve l'AI-stuck alla radice**, non il solo `walkableClimb`.

## Phase B — DetourCrowd muove le AI
- **Crowd** (`dtCrowd`, max 128 agenti) inizializzato dopo il navmesh. `addAgent(pos, radius,
  height, maxSpeed)` con `updateFlags` = ANTICIPATE_TURNS | OBSTACLE_AVOIDANCE | SEPARATION |
  OPTIMIZE_VIS | OPTIMIZE_TOPO; `separationWeight 2`.
- **CrowdSystem** (per tick): (1) reap degli agenti la cui entità non è più valida (mappa
  idx→entità); (2) registra le AI ancora senza indice (`maxSpeed = ai->seekSpeed`); (3)
  `nav.updateCrowd(dt)`; (4) write-back.
- **AiSystem esecuzione movimento:** `useCrowd = world.nav && crowdReady && crowdAgentIdx>=0`.
  - **Traversata** (Hunt/Search/Patrol): i rami impostano `moveDX/DZ = destinazione − posizione`,
    quindi il target è `pos + moveDX` → `requestMoveTarget` (pathfinding). `requestMoveTarget`
    **salta la ri-pianificazione se il target è ~invariato** (chiamarlo ogni frame rende il moto
    lento/a scatti — bug reale trovato in Phase B).
  - **Alert / roll**: `requestMoveVelocity` con la velocità tattica (strafe/approccio o lo scatto
    del roll) → steering + avoidance del crowd. Scelta "traversata-prima, combattimento come
    velocità" (l'AI decide la velocità, il crowd la esegue con avoidance).
  - Fallback su `aiMove` (physics) se il navmesh manca.
- **Write-back Y (gotcha importante):** `agentPos` sta sulla SUPERFICIE del navmesh (≈ piedi), e
  la voxelizzazione la mette ~`cellHeight` SOPRA il pavimento reale → `agentPos` **sottrae
  quella polarizzazione**; il CrowdSystem poi **somma `AI_HALF_Y`** (il transform.y è il CENTRO
  fisico, come lo spawn `ground + AI_HALF_Y`). Senza questo i modelli avevano i piedi sottoterra
  (regressione risolta 2026-07-14).
- **Disattivati col crowd:** salto anti-ostacolo dell'AI (velY non integrato), stuck-telemetry
  in Alert (falso positivo). La stuck-telemetry di traversata resta come rilevatore di
  fallimento del crowd.

## Phase C — Aree semantiche
In `build()`, dopo l'erosione e prima delle regioni, `rcMarkCylinderArea` marca:
- le `dangerZones` come area **DANGER** (id 1), i `coverPoints` come **COVER** (id 2); ground = 0.
Il filtro del crowd (`getEditableFilter(0)`) assegna costo **DANGER=10** → il pathfinding
**aggira le danger zone**; GROUND/COVER neutri. Le aree extra restano WALKABLE (attraversabili se
non c'è alternativa). I metadata vengono da `MapDef.dangerZones`/`coverPoints` (caricati a
runtime; `MapGeometryBox` non ha `type`, è editor-only). **Costi/filtri per-ruolo** = estensione
banale (filtri aggiuntivi + `queryFilterType` per agente) — struttura pronta, non ancora cablata.

## API (NavManager)
```
NavBuildStats build(const MapDef&);   void clear();   bool ready();   unsigned generation();
bool findPath(start, end, out[]);
bool crowdReady();
int  addAgent(pos, radius, height, maxSpeed);   void removeAgent(idx);
void requestMoveTarget(idx, target);            // pathfinding (salta se target ~uguale)
void requestMoveVelocity(idx, vel);             // steering diretto + avoidance
void updateCrowd(dt);   bool agentPos(idx, out&);   // out = superficie reale (bias tolto)
```
`NavBuildStats`: ok, inputTris, polyCount, vertCount, dangerPolys, coverPolys, bmin/bmax
(loggati via telemetria `Nav`/`navmesh built`).

## Parametri (NavManager.cpp, anon namespace)
`kCellSize 0.30`, `kCellHeight 0.10`, `kAgentHeight 1.8`, `kAgentRadius = AI_HALF_X`,
`kAgentClimb = STEP_HEIGHT`, `kAgentSlope 45`; aree `kAreaGround/Danger/Cover`,
`kCostDanger 10`.

## Verifica (headless, telemetria JSONL)
- `navmesh built` + `sample path` in `session_latest.jsonl`: su firebase 91 poligoni, path
  spawn1→spawn2 che aggira la geometria centrale; 5 poligoni DANGER + 7 COVER taggati.
- `--stress 20`: 40/40 agenti on-mesh, traversano e combattono, reap ok, 0 crash. Stuck da ~80 a
  ~35 e spostato via dal cover z=-6 → obstacle-stuck risolto; il residuo è congestione crowd.

## Aperti / rischi (vedi anche ADR-017, KI #31)
- **AI attraversano i veicoli** (KI #31): il crowd non li conosce (navmesh = solo geometria
  statica). Fix futuro: veicoli come ostacoli dinamici (dtObstacleAvoidance / tile-cache carve)
  o ri-check collisione veicolo nel movimento AI.
- **Feel del combattimento via velocità** (avoidance in più) da validare a mano; se non convince,
  si torna al combattimento manuale puro.
- **Congestione residua** tarabile: `separationWeight`, soglia stuck.
- `applyDangerRepulsion` (repulsione manuale in AiSystem) è ora ridondante col costo DANGER del
  navmesh — innocua, candidata alla rimozione.
