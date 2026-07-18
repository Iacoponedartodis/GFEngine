#include "util/DataPath.hpp"
#include <SDL2/SDL.h>
#include <filesystem>
#include <iostream>

namespace fs = std::filesystem;

namespace editor::datapath
{

static std::string resolve()
{
    char* base = SDL_GetBasePath();
    fs::path exeDir = base ? base : ".";
    SDL_free(base);

    std::error_code ec;
    fs::path sourceData = fs::canonical(exeDir / "../../../data", ec);
    // Il controllo è su `weapons/`, non sulla sola esistenza della cartella:
    // era la variante FORTE fra le otto copie divergenti che questo file
    // sostituisce. "Esiste una cartella chiamata data" non prova che sia LA
    // data del progetto — e accettarla significherebbe salvare nel posto
    // sbagliato in silenzio.
    if (!ec && fs::exists(sourceData / "weapons", ec))
        return sourceData.string();

    // Fallback: la copia accanto all'eseguibile (caso "gioco distribuito").
    const std::string fallback = (exeDir / "data").string();
    std::cerr << "[DataPath] data/ sorgente non trovata: uso " << fallback << "\n";
    return fallback;
}

const std::string& root()
{
    static const std::string r = resolve();
    return r;
}

const std::string& dir()
{
    static const std::string d = root() + "/";
    return d;
}

} // namespace editor::datapath
