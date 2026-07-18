#pragma once
// DataPath — la radice della cartella `data/`, risolta in UN SOLO posto (06_Todo R8).
//
// Perché esiste. Ogni modulo dell'editor si risolveva la propria radice, e le
// otto copie **erano già divergenti** (audit 2026-07-17): quattro verificavano
// `data/weapons` prima di accettare il percorso, quattro si accontentavano che la
// cartella esistesse. Basta che `exeDir/../../../data` risolva a una directory
// qualunque perché metà dei moduli usi una `data/` e l'altra metà un'altra —
// cioè un editor che salva dove il gioco non legge. Il pannello di validazione
// aveva il problema peggiore: avrebbe validato una `data/` diversa da quella
// caricata, cioè avrebbe mentito.
//
// Nota (10_ProjectMemory): la sorgente VINCE sulla copia accanto all'exe, ed è
// voluto — è ciò che fa arrivare le modifiche dell'editor al repo e le salva dal
// `remove_directory` del post-build. La copia in output è il fallback per un
// eseguibile distribuito.

#include <string>

namespace editor::datapath
{

// Radice di `data/`, SENZA slash finale. Risolta una volta e memorizzata.
const std::string& root();

// Radice con lo slash finale, per i call site che concatenano
// (es. `dir() + "classes/" + id + ".json"`).
const std::string& dir();

} // namespace editor::datapath
