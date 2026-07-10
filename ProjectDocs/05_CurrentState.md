# 05 — Current State

## 2026-07-10 (4) — Battaglia AI viva + Sandbox Tools rifiniti
- Fix AiSystem (search hardcoded pre-firebase, Search senza uscita, primo colpo
  soppresso): la battaglia AI-vs-AI ora produce ingaggi/kill continui (verificato con
  `--sim`: flag CLI che avvia direttamente la simulazione osservatore). Heartbeat
  `ai:` e riepilogo spawn `[Conquest]` in telemetria. Sandbox: un manichino per ogni
  definizione; menu con slot arma primaria/secondaria, ticket/respawn, scelta modalità
  della simulazione; log chat scorrevole (PAGSU/PAGGIU, storico 200).

## 2026-07-10 (3) — Sandbox Tools (17_SandboxTools)
- TAB in sandbox apre il menu prova: tutte le armi (scroll+INVIO), parametri partita
  (manichini per team, HP, riavvio), simulazione AI-vs-AI con osservatore neutrale in
  volo libero (WASD+SPAZIO/CTRL). L = log chat eventi in-game (hit/scudi/kill), attiva
  in ogni modalità. `SandboxMode` ora legge i conteggi da MatchSettings.

## 2026-07-10 — Profilo tattico AI completo + ability shield (16_AiBehavior, Todo #3)
- Tutti i campi tattici di `AiProfileDef` ora hanno effetto: aggression (distanza
  d'ingaggio), retreat_hp_threshold (disimpegno con fuoco di copertura), peek/hide da
  cover_preference (in "hide" non spara), flank_chance (approccio laterale in Hunt).
- Ability runtime tipo "shield": `ShieldComponent` assorbe il danno prima degli HP e si
  rigenera (param1/2/3 dell'AbilityDef); assegnata allo spawn da `abilities[]` dell'unità.
  `B1 Heavy Droid` la referenzia; assegnabile dall'EntityEditor (sezione Abilita') e
  bilanciabile dalla tab Abilita' del BalanceEditor. Vale anche sui manichini sandbox.

## 2026-07-09 (12) — Spike split-screen (ADR-011): esito (a)
- Due viewport + seconda Camera sulla stessa scena live: funziona con sole aggiunte minori
  al Renderer (`drawMeshFrom(const Camera&)`, `setViewportRect`). Toggle debug F9 in
  partita. Il soft-gate ADR-011 decade; il lavoro futuro (input/HUD del secondo giocatore)
  è additivo. Fix minore: nel PreMatch la riga "Modalità" non disegna più la barra
  (il nome, es. "Conquista", finiva sotto la barra).

## 2026-07-09 (3) — Hitbox solo in Entity Editor (ADR-012)
- HitboxEditor rimosso; EntityEditor copre tutto (incl. debug_visible, gap colmato).
  BalanceEditor ripulito (via tab Nemici/Alleati vestigiali). Ultimo id hardcoded
  ("grunt" in spawnUnit) rimosso. Profili orfani eliminati. Tab Balance: Armi/AI/Mappe/
  Personaggio.

## 2026-07-09 (2) — AI dal profilo
- L'AI ora usa dal `AiProfileDef`: `jump_enabled` (salto anti-ostacolo quando bloccata a
  terra), `accuracy` (dispersione colpi), `reaction_time` (ritardo primo colpo),
  `seek_speed`. Profilo `grunt` creato (Todo #7). Abilità runtime e ruoli tattici deferiti
  a un documento Planned Feature (Todo #3).

## 2026-07-09 — Messa in regola (ADR-010 Accepted)
- `saveJsonRMW` centralizzato + `.bak`: unico canale di scrittura JSON editor (tutti i
  moduli migrati). Comando **Rinomina** con sweep cross-ref in Weapon/Entity/Hitbox/Map
  editor. Audit dropdown passato. `id`/`profile_id` deprecati in rimozione progressiva.
- Pendente: smoke GUI del rename (KnownIssues #7); poi chiudere #7.

_Last verified: 2026-07-04 (against live code)._

## 2026-07-09 (11) — Tre modalità reali (ADR-014)
- Conquista/Assalto/Difesa selezionabili nel PreMatch (pagina Regole); esito partita
  deciso dal mode via `outcome()`; HUD mostra proprietario+cattura dei command post.
  La promessa Fase 1 "modalità come configurazioni" è implementata.

## Position vs Vision roadmap (00_Vision)
**We are mid/late-Phase 1** ("core playable"). Present: 1 map (firebase, data-driven),
infantry both factions, working weapons, spawns, base AI, **IGameMode + factory (ADR-008)**,
**command post catturabili con ticket bleed (ADR-009, autorabili nel Map Editor)**, Conquest
+ Sandbox, editor suite pro (gizmo 3 modalità, slider, camera Unreal-style). Missing for
Phase 1 exit: Assault/Defense come registrazioni della factory, vehicles, runtime
weapon-in-hand, HUD stato post, split-screen feasibility check, "is it fun" iteration.
Phases 2-5 not started (by design).

## Working
- DefinitionRegistry loads weapons/enemies/allies/ai/hitboxes/maps/abilities/characters
  from `data/`, id = filename stem.
- Two binaries build clean (GFEngine + GFEditor), Debug preset `windows-debug`.
- **Data-driven map:** `MapDef.geometry` authored in MapEditor, read by ConquestMode and
  SandboxMode. `firebase.json` now holds a ~50x40 arena + spawn points.
- **ConquestMode** reads `MapDef.spawnTeam1/2` (player + procedural unit spread) and
  `enemyTypes`/`allyTypes`. Units spawn at ground level.
- **SandboxMode** (`--sandbox`): firebase geometry, player at team1 spawn, respawning
  dummies at team2 spawn (stationary, damageable).
- **GLB pipeline:** node-hierarchy baking (non-skinned) / identity (skinned), multi-primitive
  merge, byteStride-correct accessor reads. `meshOffsetY` applied in render (no floating models).
- **EntityEditor:** mesh browse (+ saved), transform, rig bones visible/clickable, attach
  points (bone-bindable, rendered as boxes + text labels), inline hitbox zones (bone-bindable),
  weapon-in-hand pose persisted as `weapon_display`.
- **HitboxEditor:** 3-column layout, 3D viewport (model+bones+wireframe zones), bone binding,
  auto-snap of bone-bound zones, gizmo.
- **MapEditor & HitboxEditor & EntityEditor:** gizmo a 3 modalità (Sposta/Ruota/Scala,
  scorciatoie 1/2/3, barra [Sposta][Ruota][Scala] per modulo) + selezione visibile attraverso
  i modelli; pannelli proprietà a slider+campo numerico (`UiWidgets::sliderRow`); wireframe
  hitbox rotation-aware anche nell'EntityEditor.
- Weapon GLBs assigned: E5/E-5C -> e-5_blaster_rifle.glb, DC-17 -> dc-17.glb.
- Enemy/ally meshes assigned (B1 droids, Clone Trooper).

## Resolved 2026-07-04
- Hitbox authoring unified on the PROFILE (ADR-006); EntityEditor + HitboxEditor edit the same
  store the runtime reads. B1 inline zones migrated out.
- ConquestMode fallback ids now registry-derived (ADR-007).
- EntityEditor gizmo correct under scale/rotation (toWorld/deltaToLocal).
- Repo hygiene: .gitignore rewritten, build/+imgui.ini+presets.cfg untracked (to commit).

## Resolved 2026-07-04 (later batches)
- GameMode abstraction (ADR-008): `IGameMode` + factory; Application interface-only.
- Editor pro: gizmo 3 modalità, slider ovunque, camera Unreal-style (RMB look/fly, wheel,
  MMB pan, niente volo mentre si digita), WeaponEditor attach point nel viewport.
- Clone Trooper scale: risolto dall'utente via editor (nuovo GLB + mesh_scale 0.011).

## Partial / fragile
- **HitboxEditor/EntityEditor concurrency:** same profile file, last save wins (ADR-006 note).
- **Mode id dal flag CLI:** la scelta modalità dovrebbe in futuro venire da MapDef/PreMatch
  (nota in ADR-008).

## Not implemented
- AI Editor, Asset Manager, UI/Interface Editor modules.
- Rename tooling for all definition types (partial in BalanceEditor history).
- Weapon rendering in the actual runtime (weapon_display is editor-only preview so far).
