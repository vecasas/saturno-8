#pragma once
// Arpegiador y secuenciador de pasos de una capa. Se sitúa entre el teclado y la capa:
// recibe las teclas pulsadas y genera las notas a tempo. Sin dependencias de JUCE.

#include "../dsp/Layer.h"
#include <array>
#include <atomic>

namespace s8
{
constexpr int kSeqSteps = 16;

enum class PlayerMode { Off, Arp, Seq };
enum class ArpPattern { Arp, Leap, Order, Random };
enum class ArpDir { Up, Down, UpDown, DownUp };
enum class SeqPlay { Loop, PingPong, Random };

// Un paso del secuenciador empaquetado en un entero (se lee y escribe sin bloqueos).
struct Step
{
    int note = 0;            // semitonos sobre la tecla más grave pulsada (-24..+24)
    bool on = true, tie = false, accent = false;

    int pack() const { return (note + 64) | (on ? 1 << 8 : 0) | (tie ? 1 << 9 : 0) | (accent ? 1 << 10 : 0); }
    static Step unpack (int v) { return { (v & 0xff) - 64, (v >> 8 & 1) != 0, (v >> 9 & 1) != 0, (v >> 10 & 1) != 0 }; }
};

struct StepData
{
    std::array<std::atomic<int>, kSeqSteps> packed;
    StepData() { reset(); }
    void reset() { for (auto& p : packed) p.store (Step {}.pack()); }
    Step get (int i) const { return Step::unpack (packed[(size_t) i].load (std::memory_order_relaxed)); }
    void set (int i, Step s) { packed[(size_t) i].store (s.pack(), std::memory_order_relaxed); }
};

struct PlayerParams
{
    PlayerMode mode = PlayerMode::Off;
    ArpPattern pattern = ArpPattern::Arp;
    ArpDir dir = ArpDir::Up;
    int range = 1;           // octavas 1..4
    int division = 3;        // 0 = 1/4, 1 = 1/8, 2 = 1/8T, 3 = 1/16, 4 = 1/16T, 5 = 1/32
    float gate = 0.5f;       // fracción del paso
    float swing = 0.5f;      // 0.5 = recto .. 0.75
    float chance = 1.0f;     // probabilidad de que suene cada paso
    bool hold = false;       // las teclas quedan enganchadas al soltarlas
    int length = 16;         // pasos del secuenciador
    SeqPlay play = SeqPlay::Loop;
};

inline float divisionBeats (int d)
{
    static const float beats[] { 1.0f, 0.5f, 1.0f / 3.0f, 0.25f, 1.0f / 6.0f, 0.125f };
    return beats[std::clamp (d, 0, 5)];
}

class Player
{
public:
    PlayerParams p;
    const StepData* steps = nullptr;

    explicit Player (Layer& l) : layer (l) { order.reserve (128); seq.reserve (2048); }

    bool active() const { return p.mode != PlayerMode::Off; }

    void keyDown (int note, float vel, float gain)
    {
        if (p.hold && ! anyPhysical()) clearKeys();   // nueva frase con enganche: empieza de cero
        physical[(size_t) note] = true;
        if (! keys[(size_t) note]) { keys[(size_t) note] = true; order.push_back (note); dirty = true; }
        lastVel = vel; lastGain = gain;
        if (! running) { running = true; restart = true; }
    }

    void keyUp (int note)
    {
        physical[(size_t) note] = false;
        if (p.hold) return;
        if (keys[(size_t) note]) { keys[(size_t) note] = false; order.erase (std::remove (order.begin(), order.end(), note), order.end()); dirty = true; }
        if (order.empty()) { running = false; stopNote(); }
    }

    // Apaga el reproductor (al cambiar de modo o quitar el enganche sin teclas).
    void stop()
    {
        stopNote();
        clearKeys();
        physical.fill (false);
        running = false;
    }

    // Comprueba el enganche: al quitarlo, se sueltan las teclas que ya no están pulsadas.
    void updateHold()
    {
        if (! p.hold && wasHold)
        {
            for (int n = 0; n < 128; ++n)
                if (keys[(size_t) n] && ! physical[(size_t) n]) keyUp (n);
        }
        wasHold = p.hold;
    }

    // Avanza una muestra. beat = posición en negras; hostSync = posición absoluta del DAW.
    void tick (double beat, bool hostSync)
    {
        if (! running) return;
        if (restart)
        {
            restart = false;
            origin = hostSync ? 0.0 : beat;   // sin DAW, la primera tecla marca el tiempo 0
            lastIndex = hostSync ? stepIndexAt (beat) - 1 : -1;
            counter = 0; seqPos = 0; seqDir = 1;
        }
        const double pos = beat - origin;
        const int64_t idx = stepIndexAt (pos);
        if (idx != lastIndex) { lastIndex = idx; triggerStep(); stepStart = pos; stepLength = stepDuration (idx); }
        if (sounding >= 0 && ! tieNext && pos - stepStart >= stepLength * p.gate) stopNote();
    }

    int soundingNote() const { return sounding; }
    int currentStep() const { return running && p.mode == PlayerMode::Seq ? lastSeqStep : -1; }

private:
    bool anyPhysical() const { for (bool b : physical) if (b) return true; return false; }
    void clearKeys() { keys.fill (false); order.clear(); dirty = true; }

    double stepBeats() const { return divisionBeats (p.division); }
    // Índice del paso en una posición, con swing: los pasos pares duran swing·2 y los impares el resto.
    int64_t stepIndexAt (double pos) const
    {
        pos += 1.0e-9;   // evita que el redondeo retrase un paso una muestra
        const double pair = 2.0 * stepBeats();
        const double k = std::floor (pos / pair);
        const double frac = pos - k * pair;
        return (int64_t) k * 2 + (frac >= pair * p.swing ? 1 : 0);
    }
    double stepDuration (int64_t idx) const { return 2.0 * stepBeats() * ((idx & 1) == 0 ? p.swing : 1.0 - p.swing); }

    void stopNote()
    {
        if (sounding >= 0) { layer.noteOff (sounding); sounding = -1; }
        tieNext = false;
    }

    void startNote (int note, float vel)
    {
        note = std::clamp (note, 0, 127);
        if (sounding >= 0) layer.noteOff (sounding);
        layer.noteOn (note, vel, lastGain);
        sounding = note;
    }

    float random01() { return (float) (rnd.next() * 0.5f + 0.5f); }

    void rebuildArp()
    {
        dirty = false;
        seq.clear();
        std::array<int, 128> base {};
        int n = 0;
        if (p.pattern == ArpPattern::Order) for (int k : order) base[(size_t) n++] = k;
        else for (int k = 0; k < 128; ++k) if (keys[(size_t) k]) base[(size_t) n++] = k;
        if (n == 0) return;
        std::array<int, 512> up {};
        int m = 0;
        for (int o = 0; o < std::clamp (p.range, 1, 4); ++o)
            for (int i = 0; i < n; ++i) up[(size_t) m++] = base[(size_t) i] + 12 * o;
        if (p.pattern == ArpPattern::Leap && m > 1)
        {
            // Salto: la nota más grave alterna con cada una de las demás.
            for (int i = 1; i < m; ++i) { seq.push_back (up[0]); seq.push_back (up[(size_t) i]); }
        }
        else for (int i = 0; i < m; ++i) seq.push_back (up[(size_t) i]);

        const int len = (int) seq.size();
        if (p.dir == ArpDir::Down || p.dir == ArpDir::DownUp) std::reverse (seq.begin(), seq.end());
        if ((p.dir == ArpDir::UpDown || p.dir == ArpDir::DownUp) && len > 2)
            for (int i = len - 2; i > 0; --i) seq.push_back (seq[(size_t) i]);
    }

    void triggerStep()
    {
        tieNext = false;
        if (p.mode == PlayerMode::Arp)
        {
            if (dirty || lastDir != p.dir || lastPattern != p.pattern || lastRange != p.range)
            {
                lastDir = p.dir; lastPattern = p.pattern; lastRange = p.range;
                rebuildArp();
            }
            if (seq.empty()) return;
            const int note = p.pattern == ArpPattern::Random ? seq[(size_t) (random01() * (float) seq.size()) % seq.size()]
                                                              : seq[(size_t) (counter++ % (int64_t) seq.size())];
            if (random01() > p.chance) { stopNote(); return; }
            startNote (note, lastVel);
            return;
        }

        // Secuenciador
        const int len = std::clamp (p.length, 1, kSeqSteps);
        int i = 0;
        switch (p.play)
        {
            case SeqPlay::Loop: i = (int) (seqPos++ % len); break;
            case SeqPlay::Random: i = (int) (random01() * (float) len) % len; break;
            case SeqPlay::PingPong:
                if (seqPos >= len) seqPos = len - 1;
                i = (int) seqPos;
                if (len > 1) { if (seqPos + seqDir < 0 || seqPos + seqDir >= len) seqDir = -seqDir; seqPos += seqDir; }
                break;
        }
        lastSeqStep = i;
        const Step s = steps != nullptr ? steps->get (i) : Step {};
        int lowest = -1;
        for (int k = 0; k < 128 && lowest < 0; ++k) if (keys[(size_t) k]) lowest = k;
        if (lowest < 0) return;
        // Ligadura: este paso prolonga la nota anterior sin volver a dispararla.
        const Step next = steps != nullptr ? steps->get ((i + 1) % len) : Step {};
        if (s.tie && sounding >= 0) { tieNext = next.tie && next.on; return; }
        if (! s.on || random01() > p.chance) { stopNote(); return; }
        startNote (lowest + s.note, s.accent ? 1.0f : 0.75f * lastVel + 0.1f);
        tieNext = next.on && next.tie;
    }

    Layer& layer;
    std::array<bool, 128> keys {}, physical {};
    std::vector<int> order, seq;
    bool dirty = true, running = false, restart = false, wasHold = false, tieNext = false;
    ArpDir lastDir = ArpDir::Up; ArpPattern lastPattern = ArpPattern::Arp; int lastRange = 1;
    float lastVel = 1, lastGain = 1;
    double origin = 0, stepStart = 0, stepLength = 0;
    int64_t lastIndex = -1, counter = 0, seqPos = 0, seqDir = 1;
    int sounding = -1, lastSeqStep = -1;
    Noise rnd { 0x1234567u };
};
} // namespace s8
