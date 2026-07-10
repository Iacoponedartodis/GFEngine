#include "mini/core/Application.hpp"
#include <cstring>

int main(int argc, char* argv[])
{
    bool directPreMatch = false;
    bool sandbox        = false;
    bool autoSim        = false;
    std::string mapOverride;
    for (int i = 1; i < argc; ++i)
    {
        if (std::strcmp(argv[i], "--direct-prematch") == 0)
            directPreMatch = true;
        else if (std::strcmp(argv[i], "--sandbox") == 0)
            sandbox = true;
        else if (std::strcmp(argv[i], "--sim") == 0) // sandbox + simulazione AI
        { sandbox = true; autoSim = true; }
        else if (std::strcmp(argv[i], "--map") == 0 && i + 1 < argc)
            mapOverride = argv[++i];   // mappa iniziale (test/debug, R3)
    }

    mini::Application app;
    app.run(directPreMatch, sandbox, autoSim, mapOverride);
    return 0;
}