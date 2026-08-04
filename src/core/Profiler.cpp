// Profiler.cpp — vedi Profiler.hpp per il perché. Qui solo il come.

#include "mini/core/Profiler.hpp"
#include "mini/core/Telemetry.hpp"

#include <nlohmann/json.hpp>
#include <algorithm>
#include <array>
#include <chrono>
#include <cstring>

namespace mini::profiler
{
namespace
{

// Numero massimo di zone distinte. Fisso e piccolo di proposito: il percorso
// caldo resta senza allocazioni, e se un giorno si sfora è un segnale che le
// zone sono diventate troppo granulari per essere lette (non un bug da alzare).
constexpr int kMaxZones = 48;

struct Acc
{
    const char*  label = nullptr;
    std::int64_t total = 0;   // µs accumulati nella finestra
    std::int64_t max   = 0;   // il picco: è quello che si vede come scatto
    int          calls = 0;
    int          depth = 0;
};

std::array<Acc, kMaxZones> g_acc{};
int g_count  = 0;
int g_frames = 0;
int g_depth  = 0;   // profondità corrente (per la gerarchia nel report)

std::int64_t nowMicros()
{
    using namespace std::chrono;
    return duration_cast<microseconds>(steady_clock::now().time_since_epoch()).count();
}

} // namespace

void addSample(const char* label, std::int64_t micros, int depth)
{
    if (!label) return;
    // Ricerca lineare su ≤48 puntatori: più veloce di una hash map a queste
    // dimensioni, e senza allocazioni.
    for (int i = 0; i < g_count; ++i)
        if (g_acc[i].label == label)
        {
            g_acc[i].total += micros;
            g_acc[i].max = std::max(g_acc[i].max, micros);
            ++g_acc[i].calls;
            return;
        }
    if (g_count >= kMaxZones) return;   // sfondato: si perde la zona, non si crasha
    g_acc[g_count] = {label, micros, micros, 1, depth};
    ++g_count;
}

Zone::Zone(const char* label)
    : m_label(label), m_start(nowMicros()), m_depth(g_depth)
{
    ++g_depth;
}

Zone::~Zone()
{
    --g_depth;
    addSample(m_label, nowMicros() - m_start, m_depth);
}

void endFrame() { ++g_frames; }

void calibrate()
{
    // Lavoro fisso, puramente CPU, senza memoria né sistema: ~2 µs. `volatile`
    // per impedire al compilatore di eliminarlo — un benchmark ottimizzato via
    // misura zero e mentirebbe con la faccia seria.
    Zone z("taratura_cpu");
    volatile double acc = 0.0;
    for (int i = 1; i <= 2000; ++i) acc += 1.0 / (double)i;
    (void)acc;
}

int windowFrames() { return g_frames; }

void report()
{
    if (g_count == 0) return;

    // Ordina per costo totale decrescente: la prima riga è sempre dove guardare.
    std::array<int, kMaxZones> idx{};
    for (int i = 0; i < g_count; ++i) idx[i] = i;
    std::sort(idx.begin(), idx.begin() + g_count,
              [](int a, int b) { return g_acc[a].total > g_acc[b].total; });

    const int frames = std::max(1, g_frames);
    // Denominatore per le percentuali: la zona di profondità 0 più costosa, cioè
    // il frame intero. Senza un denominatore esplicito "12 ms" non dice se è
    // tanto o poco — è la stessa lezione dei funnel dell'AI.
    std::int64_t rootTotal = 0;
    for (int i = 0; i < g_count; ++i)
        if (g_acc[i].depth == 0) rootTotal = std::max(rootTotal, g_acc[i].total);

    nlohmann::json zones = nlohmann::json::array();
    for (int k = 0; k < g_count; ++k)
    {
        const Acc& a = g_acc[idx[k]];
        nlohmann::json z;
        z["zona"]      = a.label;
        z["liv"]       = a.depth;                       // gerarchia: 0 = frame intero
        z["ms_frame"]  = (double)a.total / 1000.0 / frames;
        z["ms_picco"]  = (double)a.max / 1000.0;        // lo scatto percepito
        z["chiamate"]  = a.calls;
        if (rootTotal > 0) z["quota"] = (double)a.total / (double)rootTotal;
        zones.push_back(z);
    }

    telemetry::event(telemetry::Level::Info, "Perf", "profilo",
                     {{"frame_finestra", g_frames}, {"zone", zones}});

    g_count = 0;
    g_frames = 0;
    for (auto& a : g_acc) a = {};
}

} // namespace mini::profiler
