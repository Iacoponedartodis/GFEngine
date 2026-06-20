#pragma once
#include <SDL2/SDL.h>
#include <vector>

namespace mini
{

// Suoni procedurali generati via PCM, senza file audio esterni.
// Tutti i suoni sono Sint16, 44100 Hz, mono.
class Audio
{
public:
    Audio();
    ~Audio();

    Audio(const Audio&)            = delete;
    Audio& operator=(const Audio&) = delete;

    [[nodiscard]] bool isOpen() const { return m_device != 0; }

    void playShoot();      // giocatore spara   — chirp discendente
    void playHit();        // giocatore colpito  — impatto basso + rumore
    void playVictory();    // vittoria           — arpeggio ascendente
    void playGameOver();   // sconfitta          — accordo discendente

private:
    SDL_AudioDeviceID m_device = 0;

    using Buf = std::vector<Sint16>;
    static constexpr int RATE = 44100;

    // Generatori di forma d'onda
    static Buf makeChirp(float f0, float f1, float dur, float vol);
    static Buf makeSine (float freq, float dur, float vol, float decay = 1.0f);
    static Buf makeNoise(float dur, float vol);
    static Buf mixBufs  (const Buf& a, const Buf& b);
    static Buf concat   (const Buf& a, const Buf& b);

    // Mette in coda l'audio (non-blocking, SDL gestisce il playback)
    void enqueue(const Buf& buf);
};

} // namespace mini