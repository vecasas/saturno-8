// Pruebas del motor de voz sin JUCE: afinación, estabilidad, filtros y modos de asignación.
#include "../src/dsp/Layer.h"
#include <cstdio>
#include <vector>

static int failures = 0;
#define CHECK(c) do { if (! (c)) { std::printf ("FALLO %s:%d  %s\n", __FILE__, __LINE__, #c); ++failures; } } while (0)

using namespace s8;
static const float sr = 48000;

// Renderiza n muestras (canal izquierdo + derecho).
static std::vector<float> run (Layer& L, int n)
{
    std::vector<float> out ((size_t) n);
    for (int i = 0; i < n; ++i) { float l = 0, r = 0; L.process (l, r); out[(size_t) i] = l + r; }
    return out;
}

// Frecuencia por cruces ascendentes por cero.
static float freqOf (const std::vector<float>& x, int from)
{
    int first = -1, last = -1, count = 0;
    for (size_t i = (size_t) from + 1; i < x.size(); ++i)
        if (x[i - 1] < 0 && x[i] >= 0) { if (first < 0) first = (int) i; last = (int) i; ++count; }
    return count > 1 ? (float) (count - 1) * sr / (float) (last - first) : 0.0f;
}

static Layer makeLayer()
{
    Layer L; L.setSampleRate (sr);
    L.p.cutoff = 1.0f; L.p.keyFollow = 0; L.p.vco1Wave = Wave1::Saw;
    L.p.a2 = 0.001f; L.p.s2 = 1.0f;
    return L;
}

int main()
{
    // Afinación: La 4 en 8' suena a 440 Hz; 16' una octava abajo; VCO-2 +7 st.
    {
        Layer L = makeLayer(); L.p.slope24 = false;
        L.noteOn (69, 1.0f);
        CHECK (std::abs (freqOf (run (L, 24000), 2000) - 440.0f) < 2.0f);
        L.reset(); L.p.vco1Range = 0; L.noteOn (69, 1.0f);
        CHECK (std::abs (freqOf (run (L, 24000), 2000) - 220.0f) < 1.5f);
        L.reset(); L.p.vco1Range = 1; L.p.vco1Level = 0; L.p.vco2Level = 1; L.p.vco2Wave = Wave2::Sine; L.p.vco2Fine = 7;
        L.noteOn (57, 1.0f);
        CHECK (std::abs (freqOf (run (L, 24000), 2000) - 329.63f) < 2.0f);
    }

    // Estabilidad: todo al máximo, 16 voces, 5 s, sin NaN ni explosión.
    {
        Layer L = makeLayer();
        L.p.crossMod = 1; L.p.sync = true; L.p.vco2Level = 1; L.p.resonance = 1; L.p.vcfEnv = 1;
        L.p.vcfLfo = 1; L.p.vcoLfo = 1; L.p.vcoEnv = 1; L.p.pwmAmount = 1; L.p.vco1Wave = Wave1::Pulse;
        L.p.lfoRate = 30; L.p.lfoWave = LfoWave::Random; L.p.hpf = 0.5f;
        for (int n = 36; n < 52; ++n) L.noteOn (n, 1.0f);
        bool finite = true; float peak = 0;
        for (float x : run (L, (int) sr * 5)) { finite = finite && std::isfinite (x); peak = std::max (peak, std::abs (x)); }
        CHECK (finite);
        CHECK (peak < 16.0f);
        CHECK (L.activeVoices() == 16);
    }

    // El filtro de 24 dB autooscila con la resonancia al máximo y sin señal de entrada.
    {
        Layer L = makeLayer(); L.p.vco1Level = 0; L.p.cutoff = 0.6f; L.p.resonance = 1.0f; L.p.slope24 = true;
        L.noteOn (60, 1.0f);
        auto x = run (L, 48000);
        float peak = 0; for (size_t i = 24000; i < x.size(); ++i) peak = std::max (peak, std::abs (x[i]));
        CHECK (peak > 0.05f);
    }

    // El filtro cerrado atenúa mucho una sierra aguda.
    {
        Layer open = makeLayer(), closed = makeLayer();
        closed.p.cutoff = 0.25f;
        open.noteOn (84, 1.0f); closed.noteOn (84, 1.0f);
        auto a = run (open, 9600), b = run (closed, 9600);
        double ea = 0, eb = 0; for (size_t i = 4800; i < a.size(); ++i) { ea += a[i] * a[i]; eb += b[i] * b[i]; }
        CHECK (eb < ea * 0.01);
    }

    // La envolvente suelta llega al silencio y libera la voz.
    {
        Layer L = makeLayer(); L.p.r2 = 0.2f;
        L.noteOn (60, 1.0f); run (L, 4800); L.noteOff (60); run (L, 48000);
        CHECK (L.activeVoices() == 0);
    }

    // Poli: el número de voces limita la polifonía y se roba la más antigua.
    {
        Layer L = makeLayer(); L.a.mode = Assign::Poly1; L.a.polyVoices = 4;
        for (int n = 60; n < 66; ++n) { L.noteOn (n, 1.0f); run (L, 10); }
        std::vector<int> s; L.soundingNotes (s);
        CHECK (s.size() == 4);
        CHECK (std::find (s.begin(), s.end(), 60) == s.end());
        CHECK (std::find (s.begin(), s.end(), 65) != s.end());
    }

    // Poli 1 rota de voz al repetir nota; poli 2 reutiliza la misma voz.
    {
        Layer L1 = makeLayer(); L1.a.mode = Assign::Poly1;
        L1.noteOn (60, 1); run (L1, 100); L1.noteOff (60); L1.noteOn (60, 1); run (L1, 100);
        CHECK (L1.activeVoices() == 2);
        Layer L2 = makeLayer(); L2.a.mode = Assign::Poly2;
        L2.noteOn (60, 1); run (L2, 100); L2.noteOff (60); L2.noteOn (60, 1); run (L2, 100);
        CHECK (L2.activeVoices() == 1);
    }

    // Solo con prioridad: grave, aguda y última.
    {
        for (auto [prio, expect] : { std::pair { Priority::Low, 60 }, { Priority::High, 67 }, { Priority::Last, 64 } })
        {
            Layer L = makeLayer(); L.a.mode = Assign::Solo; L.a.priority = prio;
            L.noteOn (60, 1); L.noteOn (67, 1); L.noteOn (64, 1); run (L, 100);
            std::vector<int> s; L.soundingNotes (s);
            CHECK (s.size() == 1 && s[0] == expect);
        }
        // Al soltar la nota que suena vuelve a la anterior.
        Layer L = makeLayer(); L.a.mode = Assign::Solo; L.a.priority = Priority::Last;
        L.noteOn (60, 1); L.noteOn (64, 1); L.noteOff (64); run (L, 100);
        std::vector<int> s; L.soundingNotes (s);
        CHECK (s.size() == 1 && s[0] == 60);
    }

    // Unísono: N voces en la misma nota.
    {
        Layer L = makeLayer(); L.a.mode = Assign::Unison; L.a.unisonVoices = 6;
        L.noteOn (48, 1); run (L, 100);
        std::vector<int> s; L.soundingNotes (s);
        CHECK (s.size() == 6);
        for (int n : s) CHECK (n == 48);
    }

    // Memoria de acorde: aprende do-mi-sol-si y lo transpone a la tecla tocada.
    {
        Layer L = makeLayer(); L.a.mode = Assign::Chord;
        for (int n : { 60, 64, 67, 71 }) L.noteOn (n, 1);
        CHECK (L.learnChord());
        L.reset();
        L.noteOn (50, 1); run (L, 100);
        std::vector<int> s; L.soundingNotes (s); std::sort (s.begin(), s.end());
        CHECK ((s == std::vector<int> { 50, 54, 57, 61 }));
        L.noteOff (50); run (L, 100); L.soundingNotes (s);
        CHECK (s.empty());
    }

    // Pedal de sostenido: la nota sigue tras soltarla y se suelta con el pedal.
    {
        Layer L = makeLayer(); L.p.r2 = 0.05f;
        L.setSustain (true); L.noteOn (60, 1); run (L, 100); L.noteOff (60); run (L, 24000);
        CHECK (L.activeVoices() == 1);
        L.setSustain (false); run (L, 24000);
        CHECK (L.activeVoices() == 0);
    }

    std::printf (failures ? "%d fallos\n" : "OK: pruebas del motor de voz pasan\n", failures);
    return failures ? 1 : 0;
}
