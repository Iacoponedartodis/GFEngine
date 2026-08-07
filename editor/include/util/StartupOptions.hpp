#pragma once
#include <string>

// Opzioni da riga di comando lette all'avvio dai MODULI, non solo da EditorApp.
// Vivono qui e non in EditorApp.hpp per non far dipendere un modulo dall'app —
// il verso della dipendenza è modulo → utility, mai il contrario.
//
// A cosa servono: esercitare da riga di comando percorsi di UI che altrimenti
// esistono solo dietro un clic. Non è una comodità: un percorso che non si può
// eseguire senza mouse non si può nemmeno verificare, e resta "dichiarato
// funzionante" finché non si rompe in mano all'utente (ADR-050, KI #98).
namespace editor::startup
{
    inline std::string g_structTab;        // --struct-tab <id> (vuoto = tipo nuovo)
    inline bool        g_structTabSet = false;
    inline bool        g_selfTest     = false;   // --editor-selftest, poi esce
    // --entity <id|indice>: seleziona subito un'entità in Entity Editor. Senza,
    // il modulo si apre con `m_sel = -1` e **nessun modello caricato**: tutte le
    // riproduzioni automatiche di KI #98 hanno esercitato una viewport vuota,
    // cioè non il percorso su cui l'utente lavora davvero.
    inline std::string g_entitySelect;
    inline bool        g_entitySelectSet = false;
}
