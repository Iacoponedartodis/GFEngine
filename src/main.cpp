#include "mini/core/Application.hpp"
#include <cstring>

int main(int argc, char* argv[])
{
    bool directPreMatch = false;
    bool sandbox        = false;
    bool autoSim        = false;
    for (int i = 1; i < argc; ++i)
    {
        if (std::strcmp(argv[i], "--direct-prematch") == 0)
            directPreMatch = true;
        else if (std::strcmp(argv[i], "--sandbox") == 0)
            sandbox = true;
        else if (std::strcmp(argv[i], "--sim") == 0) // sandbox + simulazione AI
        { sandbox = true; autoSim = true; }
    }

    mini::Application app;
    app.run(directPreMatch, sandbox, autoSim);
    return 0;
}