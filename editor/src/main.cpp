#include "EditorApp.hpp"
#include <iostream>

int main(int /*argc*/, char** /*argv*/)
{
    try
    {
        editor::EditorApp app;
        app.run();
    }
    catch (const std::exception& e)
    {
        std::cerr << "[GFEditor] Errore fatale: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}