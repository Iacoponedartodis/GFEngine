# 19 — Veicoli base (Fase A implementata / Fase B pianificata)

Status: **Fase A implementata** (2026-07-10, build-verified; smoke di guida manuale
in carico allo sviluppatore). Fase B (armi di bordo, multi-posto, AI alla guida,
authoring MapEditor) resta pianificata. Template come 14-18.
Requisito Fase 1 (00_Vision: "veicoli base (preferiti)"); Todo #22 chiede
veicoli come entità ECS componibili, non controller bespoke.

## Overview
Primo sistema veicoli: mezzi leggeri guidabili dal giocatore, definiti come
dati (`data/vehicles/<id>.json`), piazzati nelle mappe via `MapDef`, realizzati
come normali entità ECS con un `VehicleComponent`.

## Goal
Salire su un veicolo in partita/sandbox, guidarlo per la mappa, scenderne —
con veicolo danneggiabile e definito interamente nei dati.

## Problem Solved
La Fase 1 richiede i veicoli per essere "già divertente"; ogni altro sistema
(mappe grandi, AI, obiettivi) è pronto a riceverli.

## Scope — Fase A (questa)
1. **VehicleDef** (`data/vehicles/<id>.json`, id = filename stem, ADR-001):
   name, hp, max_speed, accel, turn_rate_deg, mesh/mesh_scale/mesh_rot_y,
   half extents di collisione (box, come tutto il resto per ora — Todo #23).
2. **Registry**: `loadVehicles()` + `getVehicle(id)` + `vehicles()`.
3. **MapDef.vehicleSpawns[]** (`vehicle_spawns`: x, z, ry, vehicle_id):
   parse additivo; i mode spawnano i veicoli allo start (entità con Transform,
   MeshRenderer, Health, VehicleComponent, team 0 finché nessuno guida).
4. **Mount/drive/dismount**: tasto E vicino a un veicolo libero (raggio in
   GameConfig) → il giocatore guida: W/S accelera/frena, A/D sterza (yaw),
   mouse look invariato, camera dal posto di guida; E scende a fianco.
   L'entità player segue il veicolo (resta bersagliabile). Movimento con lo
   stesso slide/step-up della fanteria.
5. Feedback: toast/log chat su salita/discesa; hint nel toast sandbox.

## Out of Scope — Fase B+ (futuro, da documentare quando arriva)
- Armi di bordo e sparo alla guida (il pilota non spara in Fase A).
- Multi-posto (pilota+passeggeri) — lo schema `seats` arriverà con la Fase B.
- AI alla guida; veicoli nemici che pattugliano.
- Authoring `vehicle_spawns` nel MapEditor (Fase B; per ora JSON a mano,
  additivo — il MapEditor non tocca le chiavi che non conosce, RMW).
- Fisica dedicata (sospensioni, derapate): si usa lo slide-move esistente.
- Danno da investimento.
- ~~Respawn dei veicoli distrutti~~ → FATTO 2026-07-10 (25): RespawnTracker,
  15s, attivo in tutti i mode.
- ~~Authoring vehicle_spawns nel MapEditor~~ → FATTO 2026-07-10 (25): sezione
  "[VS]" con combo dal registry, gizmo e RMW.
- ~~Collider del veicolo verso fanteria/altri veicoli~~ → FATTO 2026-07-10 (16):
  i mezzi sono solidi (ColliderComponent + excludeId nel movimento proprio).
  ~~Danno a sagoma piena~~ → FATTO 2026-07-10 (20): il danno vale su tutto l'OBB
  del box veicolo; il pilota è protetto dal mezzo finché guida (R5 chiuso).
  ~~Mesh custom renderizzata~~ → FATTO 2026-07-11: i veicoli usano la mesh del
  VehicleDef (era sempre il box); mesh caricata nel cache come nemici/armi.
  ~~Hitbox di danno separata dalla collisione~~ → FATTO 2026-07-11:
  `hit_offset_y`/`hit_half_*` — lo spazio vuoto sotto un mezzo che fluttua non conta;
  authoring nel Vehicle Editor (wireframe giallo).
  Restano per la Fase B: hitbox a ZONE MULTIPLE del veicolo (punti deboli con
  moltiplicatori) e danno residuo al pilota alla distruzione.

## Dependencies
- DefinitionRegistry (loader additivo), MapDef (campo additivo), ECS World
  (nuovo componente), physics::slideMoveWithStepUp (riusato), GameConfig
  (raggio interazione).
