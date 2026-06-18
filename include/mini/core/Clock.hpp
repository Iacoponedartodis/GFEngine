#pragma once
 
#include <chrono>
 
namespace mini
{
 
// Utility per misurare il tempo reale trascorso tra frame.
// Usa steady_clock: monotonica, non influenzata da cambio fuso orario o NTP.
class Clock
{
public:
    Clock();
 
    // Ritorna i secondi trascorsi dall'ultima chiamata a restart() (o dalla
    // costruzione), poi resetta il timer interno.
    float restart();
 
    // Ritorna i secondi trascorsi senza resettare.
    [[nodiscard]] float getElapsed() const;
 
private:
    using TimePoint = std::chrono::steady_clock::time_point;
    TimePoint m_start;
};
 
} // namespace mini
 