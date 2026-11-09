// Pruebas del sinte completo sin JUCE: capas, división, bender, portamento, deriva y limitador.
#include "../src/dsp/Synth.h"
#include <cstdio>
#include <vector>

static int failures = 0;
#define CHECK(c) do { if (! (c)) { std::printf ("FALLO %s:%d  %s\n", __FILE__, __LINE__, #c); ++failures; } } while (0)

using namespace s8;
static const float sr = 48000;

static std::vector<float> run (Synth& S, int n)
{
    std::vector<float> out ((size_t) n);
    for (int i = 0; i < n; ++i) { S.updatePlay(); float l = 0, r = 0; S.process (l, r); out[(size_t) i] = l + r; }
    return out;
}
static float freqOf (const std::vector<float>& x, size_t from, size_t to)
{
    int first = -1, last = -1, count = 0;
    for (size_t i = from + 1; i < std::min (to, x.size()); ++i)
        if (x[i - 1] < 0 && x[i] >= 0) { if (first < 0) first = (int) i; last = (int) i; ++count; }
    return count > 1 ? (float) (count - 1) * sr / (float) (last - first) : 0.0f;
}
static Synth make()
{
    Synth S; S.setSampleRate (sr);
    for (Layer* L : { &S.upper, &S.lower })
    {
        L->p.cutoff = 1; L->p.keyFollow = 0; L->p.slope24 = false; L->p.vco1Wave = Wave1::Saw;
        L->p.a2 = 0.001f; L->p.r2 = 0.05f;
    }
    S.g.limiter = false;
    return S;
}
static std::vector<int> notes (const Layer& L) { std::vector<int> v; L.soundingNotes (v); std::sort (v.begin(), v.end()); return v; }

int main()
{
    // Entera: solo la superior. Doble: las dos.
    {
        Synth S = make(); S.g.keyMode = KeyMode::Whole;
        S.noteOn (60, 1); run (S, 100);
        CHECK (notes (S.upper).size() == 1 && notes (S.lower).empty());
        S.allNotesOff(); run (S, 9600);
        S.g.keyMode = KeyMode::Dual; S.noteOn (60, 1); run (S, 100);
        CHECK (notes (S.upper).size() == 1 && notes (S.lower).size() == 1);
    }

    // División: graves a la inferior, agudas a la superior; la nota se suelta en su capa.
    {
        Synth S = make(); S.g.keyMode = KeyMode::Split; S.g.splitPoint = 60;
        S.noteOn (48, 1); S.noteOn (59, 1); S.noteOn (60, 1); S.noteOn (72, 1); run (S, 100);
        CHECK ((notes (S.lower) == std::vector<int> { 48, 59 }));
        CHECK ((notes (S.upper) == std::vector<int> { 60, 72 }));
        S.g.keyMode = KeyMode::Whole;   // cambio de modo con teclas pulsadas
        S.noteOff (48); S.noteOff (59); run (S, 100);
        CHECK (notes (S.lower).empty());
    }

    // División con fundido: cerca del punto suenan las dos capas, lejos solo una.
    {
        Synth S = make(); S.g.keyMode = KeyMode::SplitFade; S.g.splitPoint = 60;
        S.noteOn (60, 1); S.noteOn (40, 1); S.noteOn (80, 1); run (S, 100);
        CHECK ((notes (S.lower) == std::vector<int> { 40, 60 }));
        CHECK ((notes (S.upper) == std::vector<int> { 60, 80 }));
    }

    // Balance: a la derecha del todo, la inferior no se oye.
    {
        Synth S = make(); S.g.keyMode = KeyMode::Split; S.g.balance = 1;
        S.noteOn (40, 1);
        float peak = 0; for (float x : run (S, 4800)) peak = std::max (peak, std::abs (x));
        CHECK (peak < 1e-6f);
    }

    // Bender: +1 con rango de 12 st sube una octava.
    {
        Synth S = make(); S.g.bendRange = 12; S.setPitchBend (1);
        S.noteOn (57, 1);
        auto x = run (S, 24000);
        CHECK (std::abs (freqOf (x, 2000, x.size()) - 440.0f) < 2.0f);
    }

    // Portamento: tras saltar una octava, a mitad del tiempo aún no ha llegado; al final sí.
    {
        Synth S = make(); S.g.portaOn = true; S.g.portamento = 0.5f; S.upper.a.mode = Assign::Solo;
        S.noteOn (45, 1); run (S, 4800); S.noteOn (57, 1);
        auto x = run (S, 72000);
        const float early = freqOf (x, 0, 4800), late = freqOf (x, 60000, 72000);
        CHECK (early > 115 && early < 300);
        CHECK (std::abs (late - 220.0f) < 2.0f);
    }

    // Deriva apagada = afinación exacta; encendida en "taller" = se desvía pero poco.
    {
        Synth S = make(); S.noteOn (69, 1);
        auto a = run (S, 48000);
        CHECK (std::abs (freqOf (a, 4800, a.size()) - 440.0f) < 1.0f);
        Synth D = make(); D.g.driftOn = true; D.g.condition = Condition::Service; D.g.driftVco = 1;
        D.noteOn (69, 1);
        auto b = run (D, 96000);
        const float f = freqOf (b, 48000, b.size());
        CHECK (std::abs (f - 440.0f) > 0.05f && std::abs (f - 440.0f) < 40.0f);
    }

    // Pedal por capa: con el pedal solo en la superior, la inferior se suelta.
    {
        Synth S = make(); S.g.keyMode = KeyMode::Dual; S.g.sustainLower = false;
        S.setSustain (true); S.noteOn (60, 1); run (S, 100); S.noteOff (60); run (S, 24000);
        CHECK (S.upper.activeVoices() == 1 && S.lower.activeVoices() == 0);
    }

    // Limitador: 32 voces a tope no pasan del techo.
    {
        Synth S = make(); S.g.limiter = true; S.g.volume = 1; S.g.keyMode = KeyMode::Dual;
        for (Layer* L : { &S.upper, &S.lower }) { L->p.level = 1; L->p.vco2Level = 1; L->p.resonance = 0.9f; }
        for (int n = 36; n < 52; ++n) S.noteOn (n, 1);
        float peak = 0; bool finite = true;
        for (int i = 0; i < 48000; ++i) { S.updatePlay(); float l = 0, r = 0; S.process (l, r); peak = std::max ({ peak, std::abs (l), std::abs (r) }); finite = finite && std::isfinite (l); }
        CHECK (finite && peak <= 0.951f);
    }

    std::printf (failures ? "%d fallos\n" : "OK: pruebas del sinte pasan\n", failures);
    return failures ? 1 : 0;
}
