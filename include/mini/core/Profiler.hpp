#pragma once
// ── Profiler — DOVE FINISCE IL TEMPO, in numeri leggibili da un agente ───────
//
// PERCHÉ ESISTE (ADR-050). Il motore aveva **4 zone Tracy in tutto** e nessuna
// misura di costo nella telemetria. Conseguenza pratica: KI #87 — *"il numero
// massimo di AI senza lag sta calando"* — era una domanda **non rispondibile**.
// Non "difficile": non rispondibile. Qualunque ottimizzazione sarebbe partita da
// un'ipotesi su quale sistema costi, e ottimizzare a caso è il modo migliore per
// aggiungere complessità senza guadagno.
//
// PERCHÉ NON BASTA TRACY. Tracy resta (ADR-015) ed è migliore per una sessione
// interattiva: timeline, flame graph, un occhio umano che guarda. Ma è un'altra
// applicazione con una GUI: io non posso aprirla, non posso leggerla, e non posso
// confrontarne due esecuzioni in uno script. Questo profiler produce **numeri
// nella telemetria JSONL**, quindi entra nello stesso flusso di tutto il resto:
// `--sim-ticks N` → JSONL → confronto fra due build. I due strumenti non
// competono, rispondono a domande diverse.
//
// COSTO. Una lettura di orologio all'ingresso e una all'uscita di ogni zona. Con
// ~15 zone a 60 Hz sono ~1800 letture/s: irrilevante rispetto a ciò che misura.
// È **sempre acceso** di proposito — un profiler che si accende solo "quando
// serve" non c'è mai quando il problema si presenta, e i numeri storici nel JSONL
// valgono più di una misura fatta dopo il fatto.
//
// NIDIFICAZIONE. Le zone possono contenersi (`frame` ⊃ `world.tick` ⊃ `ai`): la
// somma delle percentuali supera quindi il 100%. È voluto — serve sia il totale
// di un livello sia la sua ripartizione. Il report separa le zone per profondità
// così la gerarchia si legge invece di doverla indovinare.

#include <cstdint>

namespace mini::profiler
{

// Accumula una zona di codice sotto `label`. `label` DEVE essere un letterale (o
// comunque una stringa con vita ≥ programma): si usa il puntatore come chiave,
// niente copie né allocazioni nel percorso caldo.
void addSample(const char* label, std::int64_t micros, int depth);

// RAII: misura dalla costruzione alla distruzione.
class Zone
{
public:
    explicit Zone(const char* label);
    ~Zone();
    Zone(const Zone&) = delete;
    Zone& operator=(const Zone&) = delete;
private:
    const char*  m_label;
    std::int64_t m_start;
    int          m_depth;
};

// Un frame renderizzato è passato: serve come denominatore ("ms per frame").
void endFrame();

// ── TARATURA: la macchina è rallentata, o è rallentato il nostro codice? ────
// Nella prima sessione giocata, render, simulazione e crowd sono rallentati
// TUTTI di ~10x insieme — e in una finestra con **zero AI** il render costava
// comunque 86 ms. Un fattore comune su sottosistemi indipendenti non è "il
// renderer è lento": è il processo che gira più piano (throttling, contesa con
// un altro processo, stato del driver). Ma dai soli tempi non si distingue.
//
// Questa è una quantità FISSA di lavoro puramente CPU, misurata ogni frame. Se
// il suo costo sale insieme al resto → è la macchina. Se resta piatto mentre il
// render esplode → è il nostro codice. Costo: ~2 µs per frame, e senza questa
// riga le misure di performance restano ambigue per sempre.
void calibrate();

// Emette l'evento `profilo` con la ripartizione della finestra e azzera. Da
// chiamare a cadenza fissa; se non ci sono campioni non emette nulla.
void report();

// Quanti frame nella finestra corrente (per chi vuole normalizzare da sé).
int windowFrames();

} // namespace mini::profiler

// Zona con nome esplicito. Macro e non funzione perché deve creare un oggetto
// con un nome unico nello scope del chiamante. La doppia indirezione serve a far
// ESPANDERE `__LINE__` prima di incollarlo: senza, tutte le zone di uno stesso
// file si chiamerebbero `_gf_zone___LINE__` e la seconda non compilerebbe.
#define GF_PROF_CAT2(a, b) a##b
#define GF_PROF_CAT(a, b)  GF_PROF_CAT2(a, b)
#define GF_PROFILE_ZONE(name) \
    ::mini::profiler::Zone GF_PROF_CAT(_gf_zone_, __LINE__)(name)
