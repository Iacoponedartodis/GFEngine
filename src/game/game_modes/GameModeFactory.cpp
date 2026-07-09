// GameModeFactory.cpp — punto unico di registrazione delle modalità (ADR-008).
// Aggiungere una modalità = aggiungere una classe che implementa IGameMode
// e una riga qui. Application non conosce le classi concrete.

#include "mini/game/game_modes/IGameMode.hpp"
#include "mini/game/game_modes/ConquestMode.hpp"
#include "mini/game/game_modes/SandboxMode.hpp"

#include <iostream>

namespace mini
{

std::unique_ptr<IGameMode> createGameMode(const std::string& modeId)
{
    if (modeId == "conquest") return std::make_unique<ConquestMode>();
    if (modeId == "sandbox")  return std::make_unique<SandboxMode>();

    std::cerr << "[GameMode] Modalita' sconosciuta '" << modeId
              << "' — fallback su conquest.\n";
    return std::make_unique<ConquestMode>();
}

} // namespace mini
