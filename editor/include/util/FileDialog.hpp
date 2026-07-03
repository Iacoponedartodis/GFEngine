#pragma once
#include <string>

namespace editor
{

// Apre un dialog nativo "Apri file" e ritorna il percorso scelto (vuoto se annullato).
// filter: coppie nome\0pattern\0 terminate da \0 extra. Es:
//   "Modelli 3D\0*.glb;*.gltf;*.obj\0Tutti i file\0*.*\0\0"
// startDir: directory iniziale (può essere relativa o assoluta); se vuota usa il cwd.
std::string openFileDialog(const char* filter, const std::string& startDir = "");

} // namespace editor
