#include "mini/core/Audio.hpp"

#include <SDL2/SDL.h>
#include <cmath>
#include <cstdlib>
#include <iostream>

namespace mini
{

static constexpr float PI = 3.14159265f;

// ── Costruttore / distruttore ─────────────────────────────────────────────

Audio::Audio()
{
    SDL_AudioSpec want{}, have{};
    want.freq     = RATE;
    want.format   = AUDIO_S16SYS;
    want.channels = 1;
    want.samples  = 512;
    want.callback = nullptr; // usiamo SDL_QueueAudio

    m_device = SDL_OpenAudioDevice(nullptr, 0, &want, &have, 0);
    if (m_device == 0)
    {
        std::cerr << "[Audio] SDL_OpenAudioDevice fallita: " << SDL_GetError() << std::endl;
        return;
    }
    SDL_PauseAudioDevice(m_device, 0); // avvia playback
    std::cout << "[Audio] Aperto — " << have.freq << " Hz, " << (int)have.channels << "ch" << std::endl;
}

Audio::~Audio()
{
    if (m_device) SDL_CloseAudioDevice(m_device);
}

// ── Generatori di forma d'onda ────────────────────────────────────────────

// Chirp: sweep lineare di frequenza da f0 a f1 in 'dur' secondi
Audio::Buf Audio::makeChirp(float f0, float f1, float dur, float vol)
{
    const int n = (int)(RATE * dur);
    Buf buf(n);
    for (int i = 0; i < n; ++i)
    {
        const float t     = (float)i / RATE;
        const float phase = 2.0f * PI * (f0 * t + (f1 - f0) * t * t / (2.0f * dur));
        const float env   = 1.0f - t / dur;
        buf[i] = (Sint16)(32767.0f * vol * env * std::sin(phase));
    }
    return buf;
}

// Sine: onda sinusoidale pura con inviluppo esponenziale
Audio::Buf Audio::makeSine(float freq, float dur, float vol, float decay)
{
    const int n = (int)(RATE * dur);
    Buf buf(n);
    for (int i = 0; i < n; ++i)
    {
        const float t   = (float)i / RATE;
        const float env = std::pow(1.0f - t / dur, decay);
        buf[i] = (Sint16)(32767.0f * vol * env * std::sin(2.0f * PI * freq * t));
    }
    return buf;
}

// Noise: rumore bianco con inviluppo lineare
Audio::Buf Audio::makeNoise(float dur, float vol)
{
    const int n = (int)(RATE * dur);
    Buf buf(n);
    for (int i = 0; i < n; ++i)
    {
        const float t     = (float)i / RATE;
        const float env   = 1.0f - t / dur;
        const float noise = ((float)std::rand() / RAND_MAX) * 2.0f - 1.0f;
        buf[i] = (Sint16)(32767.0f * vol * env * noise);
    }
    return buf;
}

// Mix: somma due buffer alla stessa lunghezza (allunga il piu' corto con zeri)
Audio::Buf Audio::mixBufs(const Buf& a, const Buf& b)
{
    const size_t len = a.size() > b.size() ? a.size() : b.size();
    Buf result(len, 0);
    for (size_t i = 0; i < a.size(); ++i) result[i] += a[i] / 2;
    for (size_t i = 0; i < b.size(); ++i) result[i] += b[i] / 2;
    return result;
}

// Concat: appende b dopo a
Audio::Buf Audio::concat(const Buf& a, const Buf& b)
{
    Buf result = a;
    result.insert(result.end(), b.begin(), b.end());
    return result;
}

// ── Accodamento ───────────────────────────────────────────────────────────

void Audio::enqueue(const Buf& buf)
{
    if (!m_device) return;
    // Limite coda: non accumulare piu' di ~0.3s di audio (evita delay udibile)
    if (SDL_GetQueuedAudioSize(m_device) > (Uint32)(RATE * 2 * 0.3f)) return;
    SDL_QueueAudio(m_device, buf.data(), (Uint32)(buf.size() * sizeof(Sint16)));
}

// ── Suoni di gioco ────────────────────────────────────────────────────────

void Audio::playShoot()
{
    // Chirp laser discendente: 900 → 250 Hz in 0.08s
    enqueue(makeChirp(900.0f, 250.0f, 0.08f, 0.55f));
}

void Audio::playHit()
{
    // Impatto: tono basso 100 Hz + rumore, 0.15s
    const Buf tone  = makeSine (100.0f, 0.15f, 0.5f, 2.0f);
    const Buf noise = makeNoise(0.12f, 0.45f);
    enqueue(mixBufs(tone, noise));
}

void Audio::playVictory()
{
    // Arpeggio ascendente: Do5 → Mi5 → Sol5 → Do6
    Buf jingle;
    jingle = concat(jingle, makeSine(523.25f, 0.12f, 0.6f)); // C5
    jingle = concat(jingle, makeSine(659.25f, 0.12f, 0.6f)); // E5
    jingle = concat(jingle, makeSine(783.99f, 0.12f, 0.6f)); // G5
    jingle = concat(jingle, makeSine(1046.5f, 0.28f, 0.7f)); // C6 (piu' lungo)
    enqueue(jingle);
}

void Audio::playGameOver()
{
    // Accordo discendente lento: Sol3 → Mi3 → Do3
    Buf jingle;
    jingle = concat(jingle, makeSine(196.0f, 0.22f, 0.55f, 1.5f)); // G3
    jingle = concat(jingle, makeSine(164.81f, 0.22f, 0.55f, 1.5f)); // E3
    jingle = concat(jingle, makeSine(130.81f, 0.42f, 0.6f,  1.8f)); // C3
    enqueue(jingle);
}

} // namespace mini