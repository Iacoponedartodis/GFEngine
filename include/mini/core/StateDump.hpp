#pragma once
// StateDump — dump JSON dello stato completo del gioco (ADR-013 + Phase 4).
// Estratto da Application.cpp (R2): era una lambda dentro run(); qui è una
// funzione pura, read-only, così il main loop resta più corto e il dump è
// riusabile/testabile in isolamento. Usato su F12, fine partita e crash.

#include <nlohmann/json.hpp>

namespace mini
{
class World;
class Camera;
class PlayerController;
class IGameMode;

namespace statedump
{
// `gameState` è già l'int dello stato (evita di dipendere da GameState qui).
// `mode` può essere nullptr (i ticket diventano 0).
[[nodiscard]] nlohmann::json build(const char* reason, int gameState, bool worldReady,
                                   const Camera& cam, const PlayerController& player,
                                   const IGameMode* mode, const World& world);
}
} // namespace mini
