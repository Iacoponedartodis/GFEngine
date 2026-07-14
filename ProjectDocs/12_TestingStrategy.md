# 12 — Testing Strategy

Nessun test automatico blocca le modifiche; la verifica è build + telemetria headless + smoke
manuali. La telemetria (doc 21) è oggi il canale primario per verificare comportamento senza
giocare a mano.

## Build verification (always)
- `cmake --build build/windows-debug --config Debug` deve essere pulito (**0 errori, 0 warning**
  — il progetto è warning-free: `_CRT_SECURE_NO_WARNINGS`, C4456/C4100 risolti). Killa
  GFEngine/GFEditor prima (lock exe).

## Verifica headless via telemetria (nuovo canale primario)
Molto comportamento (AI, nav, game mode, command post) si verifica SENZA input manuale:
1. Avvia una simulazione headless: `GFEngine.exe --sim` (osservatore) o `--stress N` (N AI/team).
2. Leggi `_telemetry_data/session_latest.jsonl` — una riga JSON per evento (doc 21). Filtra per
   `system`/`msg`, parsa con qualsiasi tool JSON (es. `ConvertFrom-Json`).
3. Esempi: `navmesh built`/`sample path` (nav ok), `state change`/`stuck` (AI), `Ticket bleed`/
   `Capture update` (game mode), `[Combat] Colpito!` su stdout (combattimento attivo).
4. Dump stato completo: F12 in gioco, o automatico a fine-partita/crash → `game_state.json`.
- **Limite noto (10_ProjectMemory):** gli input sintetici NON raggiungono la finestra SDL
  headless → i bug di input INTERATTIVO (es. roll del giocatore, guida) si validano solo con un
  playtest reale + la telemetria del run dell'utente.

## Manual smoke tests (per meaningful change)
1. **Editor lists:** launch GFEditor -> open EntityEditor/WeaponEditor/MapEditor/VehicleEditor
   (HitboxEditor rimosso, ADR-012); verify definition lists load without duplicates.
2. **Registry cross-refs:** an enemy's weapon/ai_profile/hitbox_profile dropdowns resolve to
   existing ids; renamed files don't orphan references.
3. **Runtime match:** GFEngine `--direct-prematch` -> loadout shows the same weapon set as the
   editor; enemies spawn (not floating), map geometry matches MapEditor.
4. **Sandbox:** GFEngine `--sandbox` -> firebase geometry loads, dummies spawn at team2 spawn,
   take damage, die, and respawn; player spawns at team1 spawn.
5. **Viewport:** models render un-corrupted; bones align to mesh; attach points visible as
   boxes+labels through the model; gizmo arrows move the selected object.

## What typically needs manual verification (cannot be traced statically)
- Actual OpenGL rendering / FBO output, gizmo drag feel, AI behavior at runtime, model scale
  in-scene. Flag these explicitly as unverified when not manually run.
