#pragma once
// JsonSave — helper di salvataggio JSON centralizzato (ADR-010).
// OGNI salvataggio JSON dell'editor DEVE passare da qui: rende il pattern
// READ-MODIFY-WRITE un vincolo strutturale invece di una disciplina da
// ricordare (incidente reale 2026-07-08: un save path che riscriveva il file
// da zero ha distrutto geometry/command_posts di firebase.json).
//
// Comportamento:
//  1. Legge il file esistente in `j` (json::object() se assente/corrotto).
//  2. Chiama patchFn(j): il modulo modifica SOLO i campi che possiede.
//     Ritorna true se qualcosa è cambiato e va scritto (false = no-op).
//  3. Prima della scrittura crea un backup `.bak` (una rotazione).
//  4. Scrive j con dump(4).

#include <nlohmann/json.hpp>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <string>

namespace editor::jsonsave
{

inline bool saveJsonRMW(const std::string& path,
                        const std::function<bool(nlohmann::json&)>& patchFn,
                        bool makeBackup = true)
{
    namespace fs = std::filesystem;

    nlohmann::json j = nlohmann::json::object();
    {
        std::ifstream fin(path);
        if (fin.is_open())
        {
            try { fin >> j; }
            catch (const std::exception& e)
            {
                std::cerr << "[JsonSave] File corrotto, riparto da vuoto: "
                          << path << " (" << e.what() << ")\n";
                j = nlohmann::json::object();
            }
        }
    }

    if (!patchFn(j))
        return true; // nessuna modifica richiesta: non toccare il file

    std::error_code ec;
    if (makeBackup && fs::exists(path, ec))
        fs::copy_file(path, path + ".bak",
                      fs::copy_options::overwrite_existing, ec); // best-effort

    std::ofstream out(path);
    if (!out.is_open())
    {
        std::cerr << "[JsonSave] ERRORE scrittura: " << path << "\n";
        return false;
    }
    out << j.dump(4) << "\n";
    return true;
}

} // namespace editor::jsonsave
