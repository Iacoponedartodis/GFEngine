#include "mini/core/Application.hpp"
#include <cstring>

int main(int argc, char* argv[])
{
    bool directPreMatch = false;
    for (int i = 1; i < argc; ++i)
    {
        if (std::strcmp(argv[i], "--direct-prematch") == 0)
            directPreMatch = true;
    }

    mini::Application app;
    app.run(directPreMatch);
    return 0;
}