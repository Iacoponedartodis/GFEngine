#pragma once
#include "EditorApp.hpp"

namespace editor
{

// Home screen: griglia di card per ogni modulo + pulsante Avvia GFEngine.
class HomeScreen
{
public:
    // Ritorna il modulo su cui l'utente ha cliccato, oppure Home se nessuno
    ActiveModule draw(bool& wantsLaunchGame);
};

} // namespace editor