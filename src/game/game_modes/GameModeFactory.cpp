// GameModeFactory.cpp — punto unico di registrazione delle modalità (ADR-008).
// Aggiungere una modalità = aggiungere una classe che implementa IGameMode
// e una riga qui. Application non conosce le classi concrete.

#include "mini/game/game_modes/IGameMode.hpp"
#include "mini/game/game_modes/ConquestMode.hpp"
#include "mini/game/game_modes/SandboxMode.hpp"
#include "mini/game/game_modes/ObjectiveModes.hpp"
#include "mini/core/Telemetry.hpp"

#include <nlohmann/json.hpp>
#include <iostream>

namespace mini
{

std::unique_ptr<IGameMode> createGameMode(const std::string& modeId)
{
    // Telemetria (ADR-016): quale modalità viene creata (ADR-008).
    telemetry::event(telemetry::Level::Info, "GameMode", "mode created",
                     {{"mode_id", modeId}});

    if (modeId == "conquest") return std::make_unique<ConquestMode>();
    if (modeId == "assault")  return std::make_unique<AssaultMode>();
    if (modeId == "defense")  return std::make_unique<DefenseMode>();
    if (modeId == "sandbox")  return std::make_unique<SandboxMode>();

    telemetry::event(telemetry::Level::Warn, "GameMode", "unknown mode, fallback conquest",
                     {{"requested", modeId}});
    std::cerr << "[GameMode] Modalita' sconosciuta '" << modeId
              << "' — fallback su conquest.\n";
    return std::make_unique<ConquestMode>();
}

} // namespace mini
