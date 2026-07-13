#include "mini/core/Application.hpp"
#include <cstring>
#include <cstdlib>

int main(int argc, char* argv[])
{
    bool directPreMatch = false;
    bool sandbox        = false;
    bool autoSim        = false;
    std::string mapOverride;
    int  stressAiCount  = 0;   // --stress N: N AI per team nel sim (profiling)
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
        else if (std::strcmp(argv[i], "--stress") == 0 && i + 1 < argc)
        {   // stress test AI: forza sim + N AI per team (per profilare con Tracy)
            sandbox = true; autoSim = true;
            stressAiCount = std::atoi(argv[++i]);
        }
    }

    mini::Application app;
    app.run(directPreMatch, sandbox, autoSim, mapOverride, stressAiCount);
    return 0;
}