#include "mini/core/Application.hpp"
#include <cstring>

int main(int argc, char* argv[])
{
    bool directPreMatch = false;
    bool sandbox        = false;
    for (int i = 1; i < argc; ++i)
    {
        if (std::strcmp(argv[i], "--direct-prematch") == 0)
            directPreMatch = true;
        else if (std::strcmp(argv[i], "--sandbox") == 0)
            sandbox = true;
    }

    mini::Application app;
    app.run(directPreMatch, sandbox);
    return 0;
}